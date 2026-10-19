#include "gfx/settings_gfx_runtime.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace erui::gfx {
namespace {

constexpr std::string_view kGameOptionsPath =
    "data0:/menu/win/02_040_optionsetting.gfx";
constexpr std::string_view kAdvancedSettingsPath =
    "data0:/menu/win/02_042_pc_graphicsetting.gfx";

[[nodiscard]] std::size_t target_index(
    SettingsGfxRuntimeTarget target) noexcept {
    switch (target) {
    case SettingsGfxRuntimeTarget::game_options: return 0;
    case SettingsGfxRuntimeTarget::advanced_settings: return 1;
    case SettingsGfxRuntimeTarget::none: break;
    }
    return 2;
}

[[nodiscard]] SettingsGfxHost expected_host(
    SettingsGfxRuntimeTarget target) noexcept {
    switch (target) {
    case SettingsGfxRuntimeTarget::game_options:
        return SettingsGfxHost::game_options;
    case SettingsGfxRuntimeTarget::advanced_settings:
        return SettingsGfxHost::advanced_settings;
    case SettingsGfxRuntimeTarget::none:
        return SettingsGfxHost::unknown;
    }
    return SettingsGfxHost::unknown;
}

} // namespace

struct SettingsGfxRuntimeCache::Storage {
    struct Entry {
        std::mutex mutex{};
        bool populated{};
        SettingsGfxRuntimeOutcome outcome{SettingsGfxRuntimeOutcome::ignored};
        SettingsGfxInspection before{};
        SettingsGfxInspection after{};
        SettingsGfxRuntimeError error{};
        std::vector<std::uint8_t> source{};
        std::vector<std::uint8_t> replacement{};
    };

    explicit Storage(std::uint16_t requested_tab_slots) noexcept
        : minimum_tab_slots(requested_tab_slots) {}

    std::uint16_t minimum_tab_slots{8};
    std::array<Entry, 2> entries{};
};

SettingsGfxRuntimeTarget classify_settings_gfx_runtime_path(
    std::string_view path) noexcept {
    if (path == kGameOptionsPath) {
        return SettingsGfxRuntimeTarget::game_options;
    }
    if (path == kAdvancedSettingsPath) {
        return SettingsGfxRuntimeTarget::advanced_settings;
    }
    return SettingsGfxRuntimeTarget::none;
}

SettingsGfxRequirements settings_gfx_runtime_requirements(
    SettingsGfxRuntimeTarget target,
    std::uint16_t minimum_tab_slots) noexcept {
    SettingsGfxRequirements requirements{};
    if (target == SettingsGfxRuntimeTarget::game_options ||
        target == SettingsGfxRuntimeTarget::advanced_settings) {
        requirements.text_input =
            SettingsGfxPresentation::ensure_character_creation;
        requirements.color_picker =
            SettingsGfxPresentation::ensure_character_creation;
    }
    if (target == SettingsGfxRuntimeTarget::game_options) {
        requirements.minimum_tab_slots = minimum_tab_slots;
        requirements.minimum_game_options_rows = std::uint16_t{13};
        requirements.minimum_camera_options_rows = std::uint16_t{13};
        requirements.minimum_display_rows = std::uint16_t{13};
        requirements.minimum_sound_rows = std::uint16_t{13};
        // NetworkSetting reserves its lower region for the game's conditional
        // UgcRestricted warning. Ten rows fill the non-overlapping authored
        // region; larger provider menus continue through host pagination.
        requirements.minimum_network_rows = std::uint16_t{10};
        requirements.minimum_keyboard_mouse_rows = std::uint16_t{13};
        requirements.minimum_graphics_rows = std::uint16_t{13};
        requirements.ensure_button_label = true;
    }
    return requirements;
}

SettingsGfxRuntimeCache::SettingsGfxRuntimeCache(
    std::uint16_t minimum_tab_slots) noexcept
    : storage_(new (std::nothrow) Storage{minimum_tab_slots}) {}

SettingsGfxRuntimeCache::~SettingsGfxRuntimeCache() {
    delete storage_;
}

bool SettingsGfxRuntimeCache::ready() const noexcept {
    return storage_ != nullptr;
}

SettingsGfxRuntimeResult SettingsGfxRuntimeCache::prepare(
    SettingsGfxRuntimeTarget target,
    std::span<const std::uint8_t> source) noexcept {
    SettingsGfxRuntimeResult result{};
    result.target = target;
    const std::size_t index = target_index(target);
    if (index >= 2U) {
        result.outcome = SettingsGfxRuntimeOutcome::failed;
        result.error.code = SettingsGfxRuntimeErrorCode::invalid_target;
        return result;
    }
    if (storage_ == nullptr) {
        result.outcome = SettingsGfxRuntimeOutcome::failed;
        result.error.code = SettingsGfxRuntimeErrorCode::allocation_failed;
        return result;
    }
    if (source.size() > kSettingsGfxRuntimeMaximumBytes) {
        result.outcome = SettingsGfxRuntimeOutcome::failed;
        result.error.code = SettingsGfxRuntimeErrorCode::input_too_large;
        return result;
    }

    try {
        Storage::Entry& entry = storage_->entries[index];
        std::lock_guard lock(entry.mutex);
        if (entry.populated) {
            const bool same_source = entry.source.size() == source.size() &&
                std::equal(entry.source.begin(), entry.source.end(),
                    source.begin(), source.end());
            // A repository implementation may return the same MemoryFile on
            // another OpenFile call after its view has already been replaced.
            // Recognize our exact published output as an idempotent cache hit
            // instead of misreporting it as a foreign source change.
            const bool same_published_replacement =
                entry.outcome == SettingsGfxRuntimeOutcome::transformed &&
                entry.replacement.size() == source.size() &&
                std::equal(entry.replacement.begin(),
                    entry.replacement.end(), source.begin(), source.end());
            if (!same_source && !same_published_replacement) {
                result.outcome = SettingsGfxRuntimeOutcome::failed;
                result.error.code = SettingsGfxRuntimeErrorCode::source_changed;
                return result;
            }
            result.outcome = entry.outcome;
            result.before = entry.before;
            result.after = entry.after;
            result.error = entry.error;
            result.cache_hit = true;
            if (entry.outcome == SettingsGfxRuntimeOutcome::transformed) {
                result.replacement_bytes = entry.replacement.data();
                result.replacement_size = entry.replacement.size();
            }
            return result;
        }

        std::vector<std::uint8_t> owned_source(
            source.begin(), source.end());
        GfxDocumentLimits limits{};
        limits.maximum_physical_bytes = kSettingsGfxRuntimeMaximumBytes;
        limits.maximum_declared_bytes = kSettingsGfxRuntimeMaximumBytes;
        limits.maximum_tag_body_bytes = kSettingsGfxRuntimeMaximumBytes;
        GfxDocumentParseResult parsed =
            GfxDocument::parse(owned_source, limits);
        if (!parsed.success()) {
            entry.populated = true;
            entry.outcome = SettingsGfxRuntimeOutcome::failed;
            entry.error.code = SettingsGfxRuntimeErrorCode::parse_failed;
            entry.error.document_error = parsed.error;
            entry.source = std::move(owned_source);
        } else {
            const SettingsGfxRequirements requirements =
                settings_gfx_runtime_requirements(
                    target, storage_->minimum_tab_slots);
            SettingsGfxTransformResult transformed =
                target == SettingsGfxRuntimeTarget::game_options
                ? transform_settings_gfx(
                    std::move(parsed.document), requirements)
                : transform_settings_widgets_gfx(
                    std::move(parsed.document), requirements);
            entry.before = transformed.before;
            entry.after = transformed.after;

            if (!transformed.success()) {
                entry.populated = true;
                entry.outcome = SettingsGfxRuntimeOutcome::failed;
                entry.error.code =
                    SettingsGfxRuntimeErrorCode::transform_failed;
                entry.error.settings_error = transformed.error;
                entry.source = std::move(owned_source);
            } else if (transformed.after.host != expected_host(target)) {
                entry.populated = true;
                entry.outcome = SettingsGfxRuntimeOutcome::failed;
                entry.error.code =
                    SettingsGfxRuntimeErrorCode::unexpected_host;
                entry.source = std::move(owned_source);
            } else if (!transformed.changed()) {
                entry.populated = true;
                entry.outcome = SettingsGfxRuntimeOutcome::unchanged;
                entry.source = std::move(owned_source);
            } else {
                GfxDocumentBytesResult serialized =
                    transformed.document.serialize_and_validate();
                if (!serialized.success()) {
                    entry.populated = true;
                    entry.outcome = SettingsGfxRuntimeOutcome::failed;
                    entry.error.code =
                        SettingsGfxRuntimeErrorCode::serialization_failed;
                    entry.error.document_error = serialized.error;
                    entry.source = std::move(owned_source);
                } else if (serialized.bytes.size() >
                               kSettingsGfxRuntimeMaximumBytes ||
                           serialized.bytes.size() > static_cast<std::size_t>(
                               (std::numeric_limits<std::int32_t>::max)())) {
                    entry.populated = true;
                    entry.outcome = SettingsGfxRuntimeOutcome::failed;
                    entry.error.code =
                        SettingsGfxRuntimeErrorCode::output_too_large;
                    entry.source = std::move(owned_source);
                } else {
                    entry.source = std::move(owned_source);
                    entry.replacement = std::move(serialized.bytes);
                    entry.outcome = SettingsGfxRuntimeOutcome::transformed;
                    entry.populated = true;
                }
            }
        }

        result.outcome = entry.outcome;
        result.before = entry.before;
        result.after = entry.after;
        result.error = entry.error;
        if (entry.outcome == SettingsGfxRuntimeOutcome::transformed) {
            result.replacement_bytes = entry.replacement.data();
            result.replacement_size = entry.replacement.size();
        }
        return result;
    } catch (const std::bad_alloc&) {
        result.outcome = SettingsGfxRuntimeOutcome::failed;
        result.error.code = SettingsGfxRuntimeErrorCode::allocation_failed;
        return result;
    } catch (const std::length_error&) {
        result.outcome = SettingsGfxRuntimeOutcome::failed;
        result.error.code = SettingsGfxRuntimeErrorCode::output_too_large;
        return result;
    } catch (...) {
        result.outcome = SettingsGfxRuntimeOutcome::failed;
        result.error.code = SettingsGfxRuntimeErrorCode::transform_failed;
        return result;
    }
}

SettingsGfxRuntimeResult SettingsGfxRuntimeCache::prepare_path(
    std::string_view path,
    std::span<const std::uint8_t> source) noexcept {
    const SettingsGfxRuntimeTarget target =
        classify_settings_gfx_runtime_path(path);
    if (target == SettingsGfxRuntimeTarget::none) return {};
    return prepare(target, source);
}

const char* settings_gfx_runtime_target_name(
    SettingsGfxRuntimeTarget target) noexcept {
    switch (target) {
    case SettingsGfxRuntimeTarget::none: return "none";
    case SettingsGfxRuntimeTarget::game_options: return "game_options";
    case SettingsGfxRuntimeTarget::advanced_settings:
        return "advanced_settings";
    }
    return "unknown";
}

const char* settings_gfx_runtime_outcome_name(
    SettingsGfxRuntimeOutcome outcome) noexcept {
    switch (outcome) {
    case SettingsGfxRuntimeOutcome::ignored: return "ignored";
    case SettingsGfxRuntimeOutcome::failed: return "failed";
    case SettingsGfxRuntimeOutcome::unchanged: return "unchanged";
    case SettingsGfxRuntimeOutcome::transformed: return "transformed";
    }
    return "unknown";
}

const char* settings_gfx_runtime_error_name(
    SettingsGfxRuntimeErrorCode error) noexcept {
    switch (error) {
    case SettingsGfxRuntimeErrorCode::none: return "none";
    case SettingsGfxRuntimeErrorCode::invalid_target: return "invalid_target";
    case SettingsGfxRuntimeErrorCode::source_changed: return "source_changed";
    case SettingsGfxRuntimeErrorCode::input_too_large:
        return "input_too_large";
    case SettingsGfxRuntimeErrorCode::parse_failed: return "parse_failed";
    case SettingsGfxRuntimeErrorCode::unexpected_host:
        return "unexpected_host";
    case SettingsGfxRuntimeErrorCode::transform_failed:
        return "transform_failed";
    case SettingsGfxRuntimeErrorCode::serialization_failed:
        return "serialization_failed";
    case SettingsGfxRuntimeErrorCode::output_too_large:
        return "output_too_large";
    case SettingsGfxRuntimeErrorCode::allocation_failed:
        return "allocation_failed";
    }
    return "unknown";
}

} // namespace erui::gfx
