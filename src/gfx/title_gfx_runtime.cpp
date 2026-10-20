#include "gfx/title_gfx_runtime.hpp"

#include "sha256.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

namespace erui::gfx {
namespace {

constexpr std::string_view kTitleGfxPath =
    "data0:/menu/05_000_title.gfx";

constexpr std::size_t kEldenRing271TitleGfxBytes = 12176U;
constexpr erui::detail::Sha256Digest kEldenRing271TitleGfxSha256{
    0x8aU, 0x0aU, 0xcdU, 0x5bU, 0x73U, 0xc0U, 0x4fU, 0x1aU,
    0x88U, 0x61U, 0xe7U, 0xd7U, 0xccU, 0xb2U, 0x1fU, 0xbbU,
    0x1aU, 0x62U, 0x1bU, 0x69U, 0x9dU, 0x17U, 0x37U, 0x19U,
    0x0bU, 0xdaU, 0xecU, 0x9eU, 0xf8U, 0xf5U, 0xdcU, 0xc2U,
};

[[nodiscard]] std::uint64_t byte_fingerprint(
    std::span<const std::uint8_t> bytes) noexcept {
    constexpr std::uint64_t offset_basis = UINT64_C(14695981039346656037);
    constexpr std::uint64_t prime = UINT64_C(1099511628211);
    std::uint64_t value = offset_basis;
    for (const std::uint8_t byte : bytes) {
        value ^= byte;
        value *= prime;
    }
    return value;
}

[[nodiscard]] bool exact_bytes(
    std::span<const std::uint8_t> left,
    std::uint64_t left_fingerprint,
    const std::vector<std::uint8_t>& right,
    std::uint64_t right_fingerprint) noexcept {
    return left_fingerprint == right_fingerprint &&
        left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin());
}

} // namespace

struct TitleGfxRuntimeCache::Storage {
    struct CachedResult {
        TitleGfxRuntimeOutcome outcome{TitleGfxRuntimeOutcome::failed};
        TitleGfxRuntimeSourceProfile source_profile{
            TitleGfxRuntimeSourceProfile::unrecognized};
        TitleGfxInspection before{};
        TitleGfxInspection after{};
        TitleGfxRuntimeError error{};
        bool dlc_row_reuse_permitted{};
        std::vector<std::uint8_t> source{};
        std::vector<std::uint8_t> replacement{};
        std::uint64_t source_fingerprint{};
        std::uint64_t replacement_fingerprint{};
    };

    std::mutex mutex{};
    std::condition_variable published{};
    std::atomic<TitleGfxRuntimePlanState> state{
        TitleGfxRuntimePlanState::pending};
    std::vector<TitleGfxRuntimeLine> lines{};
    std::vector<TitleGfxTextOccupancy> external_occupancy{};
    TitleGfxDlcState dlc_state{TitleGfxDlcState::unknown};
    std::size_t provider_count{};
    std::size_t custom_color_count{};
    std::optional<CachedResult> cached{};
};

bool is_title_gfx_runtime_path(std::string_view path) noexcept {
    return path == kTitleGfxPath;
}

TitleGfxRuntimeSourceProfile classify_title_gfx_runtime_source(
    std::span<const std::uint8_t> source) noexcept {
    if (source.size() != kEldenRing271TitleGfxBytes) {
        return TitleGfxRuntimeSourceProfile::unrecognized;
    }
    return erui::detail::sha256(source) == kEldenRing271TitleGfxSha256
        ? TitleGfxRuntimeSourceProfile::elden_ring_2_7_1_app_1_17_1
        : TitleGfxRuntimeSourceProfile::unrecognized;
}

TitleGfxRuntimeCache::TitleGfxRuntimeCache() noexcept
    : storage_(new (std::nothrow) Storage{}) {}

TitleGfxRuntimeCache::~TitleGfxRuntimeCache() {
    delete storage_;
}

bool TitleGfxRuntimeCache::ready() const noexcept {
    return storage_ != nullptr;
}

TitleGfxRuntimePlanState TitleGfxRuntimeCache::plan_state() const noexcept {
    return storage_
        ? storage_->state.load(std::memory_order_acquire)
        : TitleGfxRuntimePlanState::pending;
}

TitleGfxRuntimePlanState TitleGfxRuntimeCache::wait_for_plan(
    std::uint32_t maximum_wait_ms) noexcept {
    if (!storage_) return TitleGfxRuntimePlanState::pending;

    TitleGfxRuntimePlanState state =
        storage_->state.load(std::memory_order_acquire);
    if (state != TitleGfxRuntimePlanState::pending || maximum_wait_ms == 0U) {
        return state;
    }

    try {
        std::unique_lock lock(storage_->mutex);
        (void)storage_->published.wait_for(
            lock,
            std::chrono::milliseconds{maximum_wait_ms},
            [this] {
                return storage_->state.load(std::memory_order_relaxed) !=
                    TitleGfxRuntimePlanState::pending;
            });
        return storage_->state.load(std::memory_order_acquire);
    } catch (...) {
        // This method is used on Scaleform's synchronous file-opening path.
        // Any platform wait failure must preserve the original movie rather
        // than escape across the native detour boundary. Re-read once so a
        // publication that won the race with the wait failure is not lost.
        return storage_->state.load(std::memory_order_acquire);
    }
}

TitleGfxRuntimePlanSummary TitleGfxRuntimeCache::plan_summary() const noexcept {
    TitleGfxRuntimePlanSummary summary{};
    if (!storage_) return summary;
    summary.state = storage_->state.load(std::memory_order_acquire);
    if (summary.state == TitleGfxRuntimePlanState::pending) return summary;

    // Publication initializes these values before the release-store of
    // state. They never change afterward, so this acquire-load makes these
    // lock-free diagnostic reads race-free.
    summary.dlc_state = storage_->dlc_state;
    summary.line_count = storage_->lines.size();
    summary.provider_count = storage_->provider_count;
    summary.custom_color_count = storage_->custom_color_count;
    summary.external_occupancy_count =
        storage_->external_occupancy.size();
    return summary;
}

std::string_view TitleGfxRuntimeCache::provider_id_for_line(
    std::size_t line_index) const noexcept {
    if (!storage_ ||
        storage_->state.load(std::memory_order_acquire) !=
            TitleGfxRuntimePlanState::ready_lines ||
        line_index >= storage_->lines.size()) {
        return {};
    }
    return storage_->lines[line_index].provider_id;
}

TitleGfxRuntimePublishStatus TitleGfxRuntimeCache::publish(
    std::vector<TitleGfxRuntimeLine> lines,
    TitleGfxDlcState dlc_state,
    std::vector<TitleGfxTextOccupancy> external_occupancy) noexcept {
    if (!storage_) {
        return TitleGfxRuntimePublishStatus::allocation_failed;
    }
    if (lines.size() > kTitleGfxRuntimeMaximumLines) {
        return TitleGfxRuntimePublishStatus::too_many_lines;
    }
    if (external_occupancy.size() >
        kTitleGfxRuntimeMaximumExternalOccupancy) {
        return TitleGfxRuntimePublishStatus::too_many_lines;
    }

    try {
        std::lock_guard lock(storage_->mutex);
        if (storage_->state.load(std::memory_order_relaxed) !=
                TitleGfxRuntimePlanState::pending) {
            return TitleGfxRuntimePublishStatus::already_published;
        }

        std::size_t provider_count{};
        std::size_t custom_color_count{};
        for (std::size_t index = 0; index < lines.size(); ++index) {
            if (lines[index].color) ++custom_color_count;
            bool seen{};
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (lines[previous].provider_id == lines[index].provider_id) {
                    seen = true;
                    break;
                }
            }
            if (!seen) ++provider_count;
        }

        storage_->lines.swap(lines);
        storage_->external_occupancy.swap(external_occupancy);
        storage_->dlc_state = dlc_state;
        storage_->provider_count = provider_count;
        storage_->custom_color_count = custom_color_count;
        const TitleGfxRuntimePlanState published_state =
            storage_->lines.empty()
            ? TitleGfxRuntimePlanState::ready_empty
            : TitleGfxRuntimePlanState::ready_lines;
        storage_->state.store(published_state, std::memory_order_release);
        storage_->published.notify_all();
        return TitleGfxRuntimePublishStatus::published;
    } catch (const std::bad_alloc&) {
        return TitleGfxRuntimePublishStatus::allocation_failed;
    } catch (...) {
        return TitleGfxRuntimePublishStatus::allocation_failed;
    }
}

TitleGfxRuntimeResult TitleGfxRuntimeCache::prepare(
    std::span<const std::uint8_t> source) noexcept {
    TitleGfxRuntimeResult result{};
    if (!storage_) {
        result.outcome = TitleGfxRuntimeOutcome::failed;
        result.error.code = TitleGfxRuntimeErrorCode::allocation_failed;
        return result;
    }

    TitleGfxRuntimePlanState state =
        storage_->state.load(std::memory_order_acquire);
    if (state == TitleGfxRuntimePlanState::pending) {
        result.outcome = TitleGfxRuntimeOutcome::pending;
        return result;
    }
    if (state == TitleGfxRuntimePlanState::ready_empty) {
        result.outcome = TitleGfxRuntimeOutcome::empty;
        return result;
    }
    if (source.size() > kTitleGfxRuntimeMaximumBytes) {
        result.outcome = TitleGfxRuntimeOutcome::failed;
        result.error.code = TitleGfxRuntimeErrorCode::input_too_large;
        return result;
    }

    try {
        std::lock_guard lock(storage_->mutex);
        state = storage_->state.load(std::memory_order_relaxed);
        if (state == TitleGfxRuntimePlanState::pending) {
            result.outcome = TitleGfxRuntimeOutcome::pending;
            return result;
        }
        if (state == TitleGfxRuntimePlanState::ready_empty) {
            result.outcome = TitleGfxRuntimeOutcome::empty;
            return result;
        }

        const std::uint64_t fingerprint = byte_fingerprint(source);
        if (storage_->cached) {
            const Storage::CachedResult& cached = *storage_->cached;
            const bool same_source = exact_bytes(
                source,
                fingerprint,
                cached.source,
                cached.source_fingerprint);
            const bool same_replacement =
                cached.outcome == TitleGfxRuntimeOutcome::transformed &&
                exact_bytes(
                    source,
                    fingerprint,
                    cached.replacement,
                    cached.replacement_fingerprint);
            if (!same_source && !same_replacement) {
                result.outcome = TitleGfxRuntimeOutcome::failed;
                result.error.code = TitleGfxRuntimeErrorCode::source_changed;
                return result;
            }

            result.outcome = cached.outcome;
            result.source_profile = cached.source_profile;
            result.before = cached.before;
            result.after = cached.after;
            result.error = cached.error;
            result.dlc_row_reuse_permitted =
                cached.dlc_row_reuse_permitted;
            result.cache_hit = true;
            if (cached.outcome == TitleGfxRuntimeOutcome::transformed) {
                result.replacement_bytes = cached.replacement.data();
                result.replacement_size = cached.replacement.size();
            }
            return result;
        }

        Storage::CachedResult prepared{};
        prepared.source.assign(source.begin(), source.end());
        prepared.source_fingerprint = fingerprint;
        prepared.source_profile = classify_title_gfx_runtime_source(
            prepared.source);
        prepared.dlc_row_reuse_permitted =
            storage_->dlc_state == TitleGfxDlcState::absent &&
            prepared.source_profile == TitleGfxRuntimeSourceProfile::
                elden_ring_2_7_1_app_1_17_1;

        GfxDocumentLimits limits{};
        limits.maximum_physical_bytes = kTitleGfxRuntimeMaximumBytes;
        limits.maximum_declared_bytes = kTitleGfxRuntimeMaximumBytes;
        limits.maximum_tag_body_bytes = kTitleGfxRuntimeMaximumBytes;
        GfxDocumentParseResult parsed = GfxDocument::parse(
            prepared.source, limits);
        if (!parsed.success()) {
            prepared.outcome = TitleGfxRuntimeOutcome::failed;
            prepared.error.code = TitleGfxRuntimeErrorCode::parse_failed;
            prepared.error.document_error = parsed.error;
        } else {
            std::vector<TitleGfxTextLine> line_views{};
            line_views.reserve(storage_->lines.size());
            for (const TitleGfxRuntimeLine& line : storage_->lines) {
                line_views.push_back({line.text, line.color, line.corner});
            }

            TitleGfxTransformResult transformed = transform_title_gfx(
                std::move(parsed.document),
                line_views,
                {
                    .dlc_state = storage_->dlc_state,
                    .dlc_field_proof = prepared.dlc_row_reuse_permitted
                        ? TitleGfxDlcFieldProof::
                            canonical_shadow_of_the_erdtree
                        : TitleGfxDlcFieldProof::unproven,
                    .external_text_occupancy =
                        storage_->external_occupancy,
                });
            prepared.before = transformed.before;
            prepared.after = transformed.after;
            if (!transformed.success()) {
                prepared.outcome = TitleGfxRuntimeOutcome::failed;
                prepared.error.code =
                    transformed.error.document_error.code ==
                            GfxDocumentErrorCode::output_too_large
                    ? TitleGfxRuntimeErrorCode::output_too_large
                    : TitleGfxRuntimeErrorCode::transform_failed;
                prepared.error.title_error = transformed.error;
                prepared.error.document_error =
                    transformed.error.document_error;
            } else if (!transformed.changed()) {
                prepared.outcome = TitleGfxRuntimeOutcome::unchanged;
            } else {
                GfxDocumentBytesResult serialized =
                    transformed.document.serialize_and_validate();
                if (!serialized.success()) {
                    prepared.outcome = TitleGfxRuntimeOutcome::failed;
                    prepared.error.code =
                        TitleGfxRuntimeErrorCode::serialization_failed;
                    prepared.error.document_error = serialized.error;
                } else if (serialized.bytes.size() >
                               kTitleGfxRuntimeMaximumBytes ||
                           serialized.bytes.size() >
                               static_cast<std::size_t>(
                                   (std::numeric_limits<std::int32_t>::max)())) {
                    prepared.outcome = TitleGfxRuntimeOutcome::failed;
                    prepared.error.code =
                        TitleGfxRuntimeErrorCode::output_too_large;
                } else {
                    prepared.replacement = std::move(serialized.bytes);
                    prepared.replacement_fingerprint = byte_fingerprint(
                        prepared.replacement);
                    prepared.outcome = TitleGfxRuntimeOutcome::transformed;
                }
            }
        }

        storage_->cached.emplace(std::move(prepared));
        const Storage::CachedResult& cached = *storage_->cached;
        result.outcome = cached.outcome;
        result.source_profile = cached.source_profile;
        result.before = cached.before;
        result.after = cached.after;
        result.error = cached.error;
        result.dlc_row_reuse_permitted =
            cached.dlc_row_reuse_permitted;
        if (cached.outcome == TitleGfxRuntimeOutcome::transformed) {
            result.replacement_bytes = cached.replacement.data();
            result.replacement_size = cached.replacement.size();
        }
        return result;
    } catch (const std::bad_alloc&) {
        result.outcome = TitleGfxRuntimeOutcome::failed;
        result.error.code = TitleGfxRuntimeErrorCode::allocation_failed;
        return result;
    } catch (const std::length_error&) {
        result.outcome = TitleGfxRuntimeOutcome::failed;
        result.error.code = TitleGfxRuntimeErrorCode::output_too_large;
        return result;
    } catch (...) {
        result.outcome = TitleGfxRuntimeOutcome::failed;
        result.error.code = TitleGfxRuntimeErrorCode::transform_failed;
        return result;
    }
}

const char* title_gfx_runtime_plan_state_name(
    TitleGfxRuntimePlanState state) noexcept {
    switch (state) {
    case TitleGfxRuntimePlanState::pending: return "pending";
    case TitleGfxRuntimePlanState::ready_empty: return "ready_empty";
    case TitleGfxRuntimePlanState::ready_lines: return "ready_lines";
    }
    return "unknown";
}

const char* title_gfx_runtime_publish_status_name(
    TitleGfxRuntimePublishStatus status) noexcept {
    switch (status) {
    case TitleGfxRuntimePublishStatus::published: return "published";
    case TitleGfxRuntimePublishStatus::already_published:
        return "already_published";
    case TitleGfxRuntimePublishStatus::too_many_lines:
        return "too_many_lines";
    case TitleGfxRuntimePublishStatus::allocation_failed:
        return "allocation_failed";
    }
    return "unknown";
}

const char* title_gfx_runtime_outcome_name(
    TitleGfxRuntimeOutcome outcome) noexcept {
    switch (outcome) {
    case TitleGfxRuntimeOutcome::pending: return "pending";
    case TitleGfxRuntimeOutcome::empty: return "empty";
    case TitleGfxRuntimeOutcome::failed: return "failed";
    case TitleGfxRuntimeOutcome::unchanged: return "unchanged";
    case TitleGfxRuntimeOutcome::transformed: return "transformed";
    }
    return "unknown";
}

const char* title_gfx_runtime_source_profile_name(
    TitleGfxRuntimeSourceProfile profile) noexcept {
    switch (profile) {
    case TitleGfxRuntimeSourceProfile::unrecognized:
        return "unrecognized";
    case TitleGfxRuntimeSourceProfile::elden_ring_2_7_1_app_1_17_1:
        return "elden_ring_2_7_1_app_1_17_1";
    }
    return "unknown";
}

const char* title_gfx_runtime_error_name(
    TitleGfxRuntimeErrorCode error) noexcept {
    switch (error) {
    case TitleGfxRuntimeErrorCode::none: return "none";
    case TitleGfxRuntimeErrorCode::source_changed: return "source_changed";
    case TitleGfxRuntimeErrorCode::input_too_large: return "input_too_large";
    case TitleGfxRuntimeErrorCode::parse_failed: return "parse_failed";
    case TitleGfxRuntimeErrorCode::transform_failed: return "transform_failed";
    case TitleGfxRuntimeErrorCode::serialization_failed:
        return "serialization_failed";
    case TitleGfxRuntimeErrorCode::output_too_large:
        return "output_too_large";
    case TitleGfxRuntimeErrorCode::allocation_failed:
        return "allocation_failed";
    }
    return "unknown";
}

} // namespace erui::gfx
