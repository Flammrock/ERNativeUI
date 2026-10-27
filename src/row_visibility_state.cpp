#include "row_visibility_state.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace erui::detail {
namespace {

struct RowIdentity {
    ERUI_ProviderHandle provider{};
    ERUI_RowHandle row{};

    friend bool operator==(const RowIdentity&, const RowIdentity&) = default;
};

struct RowIdentityHash {
    std::size_t operator()(const RowIdentity& value) const noexcept {
        const std::size_t first = std::hash<std::uint64_t>{}(value.provider);
        const std::size_t second = std::hash<std::uint64_t>{}(value.row);
        return first ^ (second + static_cast<std::size_t>(0x9E3779B9u) +
            (first << 6u) + (first >> 2u));
    }
};

struct Entry {
    RowIdentity identity{};
    std::size_t page_index{invalid_compiled_index};
    bool desired_visible{true};
    bool applied_visible{true};
    bool dirty{};
};

struct PageState {
    // One slot per immutable CompiledPage::rows element. An invalid slot is an
    // internal Core row and therefore remains unconditionally visible.
    std::vector<std::size_t> source_slots{};
    std::vector<std::size_t> applied_visible_source_rows{};
    std::uint64_t desired_revision{1};
    std::uint64_t applied_revision{1};
};

std::mutex g_mutex{};
std::vector<Entry> g_entries{};
std::vector<PageState> g_pages{};
std::unordered_map<RowIdentity, std::size_t, RowIdentityHash> g_slots{};
std::unordered_map<RowIdentity, std::pair<bool, std::uint64_t>, RowIdentityHash>
    g_preconfiguration_updates{};
std::uint64_t g_revision{};
bool g_configured{};
bool g_pending{};

[[nodiscard]] std::uint64_t next_revision(std::uint64_t value) noexcept {
    ++value;
    return value == 0 ? 1 : value;
}

[[nodiscard]] bool valid_update(const RowVisibilityUpdate& update) noexcept {
    return update.provider != ERUI_INVALID_PROVIDER &&
        update.row != ERUI_INVALID_ROW;
}

void rebuild_page_projection(PageState& page) {
    page.applied_visible_source_rows.clear();
    for (std::size_t source = 0; source < page.source_slots.size(); ++source) {
        const std::size_t slot = page.source_slots[source];
        if (slot == invalid_compiled_index ||
            (slot < g_entries.size() && g_entries[slot].applied_visible)) {
            page.applied_visible_source_rows.push_back(source);
        }
    }
}

} // namespace

struct PreparedRowVisibilityUpdates::Impl {
    explicit Impl(std::unique_lock<std::mutex> acquired) noexcept
        : lock(std::move(acquired)) {}

    std::unique_lock<std::mutex> lock{};
    bool configured{};
    std::uint64_t revision{};
    std::vector<std::pair<std::size_t, bool>> updates{};
    std::unordered_map<
        RowIdentity,
        std::pair<bool, std::uint64_t>,
        RowIdentityHash> staged_preconfiguration{};
};

PreparedRowVisibilityUpdates::PreparedRowVisibilityUpdates() noexcept = default;
PreparedRowVisibilityUpdates::~PreparedRowVisibilityUpdates() = default;
PreparedRowVisibilityUpdates::PreparedRowVisibilityUpdates(
    PreparedRowVisibilityUpdates&&) noexcept = default;
PreparedRowVisibilityUpdates& PreparedRowVisibilityUpdates::operator=(
    PreparedRowVisibilityUpdates&&) noexcept = default;

PreparedRowVisibilityUpdates::operator bool() const noexcept {
    return impl_ != nullptr;
}

ERUI_Result prepare_row_visibility_updates(
    std::vector<RowVisibilityUpdate> updates,
    PreparedRowVisibilityUpdates& output) noexcept {
    if (output || updates.empty()) return ERUI_INVALID_ARGUMENT;
    for (const RowVisibilityUpdate& update : updates) {
        if (!valid_update(update)) return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::unordered_set<RowIdentity, RowIdentityHash> unique{};
        unique.reserve(updates.size());
        for (const RowVisibilityUpdate& update : updates) {
            if (!unique.emplace(RowIdentity{update.provider, update.row}).second) {
                return ERUI_INVALID_ARGUMENT;
            }
        }

        auto prepared = std::make_unique<PreparedRowVisibilityUpdates::Impl>(
            std::unique_lock<std::mutex>(g_mutex));
        prepared->configured = g_configured;
        prepared->revision = next_revision(g_revision);
        if (!g_configured) {
            prepared->staged_preconfiguration = g_preconfiguration_updates;
            for (const RowVisibilityUpdate& update : updates) {
                prepared->staged_preconfiguration[
                    RowIdentity{update.provider, update.row}] =
                    {update.visible, prepared->revision};
            }
        } else {
            prepared->updates.reserve(updates.size());
            for (const RowVisibilityUpdate& update : updates) {
                const auto found = g_slots.find(
                    RowIdentity{update.provider, update.row});
                if (found == g_slots.end() ||
                    found->second >= g_entries.size()) {
                    return ERUI_INVALID_HANDLE;
                }
                if (g_entries[found->second].desired_visible ==
                    update.visible) {
                    continue;
                }
                prepared->updates.emplace_back(
                    found->second, update.visible);
            }
        }
        output.impl_ = std::move(prepared);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

void commit_prepared_row_visibility_updates(
    PreparedRowVisibilityUpdates&& prepared) noexcept {
    if (!prepared.impl_) return;
    PreparedRowVisibilityUpdates::Impl& state = *prepared.impl_;
    if (!state.configured) {
        g_preconfiguration_updates =
            std::move(state.staged_preconfiguration);
    } else {
        if (state.updates.empty()) {
            prepared.impl_.reset();
            return;
        }
        for (const auto& [slot, visible] : state.updates) {
            Entry& entry = g_entries[slot];
            entry.desired_visible = visible;
            entry.dirty = true;
            g_pages[entry.page_index].desired_revision = state.revision;
        }
    }
    g_revision = state.revision;
    g_pending = true;
    prepared.impl_.reset();
}

ERUI_Result submit_row_visibility_updates(
    std::vector<RowVisibilityUpdate> updates) noexcept {
    PreparedRowVisibilityUpdates prepared{};
    const ERUI_Result result = prepare_row_visibility_updates(
        std::move(updates), prepared);
    if (result != ERUI_OK) return result;
    commit_prepared_row_visibility_updates(std::move(prepared));
    return ERUI_OK;
}

bool configure_row_visibility_state(const CompiledMenu& menu) noexcept {
    try {
        std::lock_guard lock(g_mutex);
        if (g_configured || !g_entries.empty() || !g_pages.empty() ||
            !g_slots.empty()) {
            return false;
        }

        std::vector<Entry> entries{};
        std::vector<PageState> pages(menu.pages.size());
        std::unordered_map<RowIdentity, std::size_t, RowIdentityHash> slots{};
        std::size_t public_row_count{};
        for (const CompiledPage& page : menu.pages) {
            for (const CompiledRow& row : page.rows) {
                if (row.visibility_editable &&
                    row.provider != ERUI_INVALID_PROVIDER &&
                    row.row != ERUI_INVALID_ROW) {
                    ++public_row_count;
                }
            }
        }
        entries.reserve(public_row_count);
        slots.reserve(public_row_count);

        const std::uint64_t base_revision = g_revision == 0 ? 1 : g_revision;
        for (std::size_t page_index = 0;
             page_index < menu.pages.size(); ++page_index) {
            const CompiledPage& source_page = menu.pages[page_index];
            PageState& page = pages[page_index];
            page.source_slots.assign(
                source_page.rows.size(), invalid_compiled_index);
            page.applied_visible_source_rows.reserve(source_page.rows.size());
            page.desired_revision = base_revision;
            page.applied_revision = base_revision;

            for (std::size_t source_index = 0;
                 source_index < source_page.rows.size(); ++source_index) {
                const CompiledRow& row = source_page.rows[source_index];
                if (!row.visibility_editable ||
                    row.provider == ERUI_INVALID_PROVIDER ||
                    row.row == ERUI_INVALID_ROW) {
                    continue;
                }
                const RowIdentity identity{row.provider, row.row};
                Entry entry{
                    .identity = identity,
                    .page_index = page_index,
                };
                if (const auto pending =
                        g_preconfiguration_updates.find(identity);
                    pending != g_preconfiguration_updates.end()) {
                    entry.desired_visible = pending->second.first;
                    entry.applied_visible = pending->second.first;
                    page.desired_revision = std::max(
                        page.desired_revision, pending->second.second);
                    page.applied_revision = page.desired_revision;
                }
                const std::size_t slot = entries.size();
                if (!slots.emplace(identity, slot).second) return false;
                page.source_slots[source_index] = slot;
                entries.push_back(entry);
            }
        }

        for (const auto& pending : g_preconfiguration_updates) {
            if (slots.find(pending.first) == slots.end()) return false;
        }

        g_entries = std::move(entries);
        g_pages = std::move(pages);
        g_slots = std::move(slots);
        for (PageState& page : g_pages) rebuild_page_projection(page);
        g_preconfiguration_updates.clear();
        g_revision = base_revision;
        g_configured = true;
        g_pending = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool synchronize_pending_row_visibility() noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_configured || !g_pending) return false;

    for (Entry& entry : g_entries) {
        if (!entry.dirty) continue;
        entry.applied_visible = entry.desired_visible;
        entry.dirty = false;
    }
    for (PageState& page : g_pages) {
        if (page.applied_revision == page.desired_revision) continue;
        rebuild_page_projection(page);
        page.applied_revision = page.desired_revision;
    }
    g_pending = false;
    return true;
}

bool row_visibility_updates_pending() noexcept {
    std::lock_guard lock(g_mutex);
    return g_pending;
}

std::size_t editable_row_visibility_count() noexcept {
    std::lock_guard lock(g_mutex);
    return g_entries.size();
}

RowVisibilityProjection row_visibility_projection(
    std::size_t logical_page_index) noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_configured || logical_page_index >= g_pages.size()) return {};
    const PageState& page = g_pages[logical_page_index];
    return {
        .logical_page_index = logical_page_index,
        .source_row_count = page.source_slots.size(),
        .visible_row_count = page.applied_visible_source_rows.size(),
        .revision = page.applied_revision,
    };
}

bool map_projected_row_to_source(
    std::size_t logical_page_index,
    std::uint64_t revision,
    std::size_t projected_row_index,
    std::size_t& source_row_index) noexcept {
    source_row_index = invalid_compiled_index;
    std::lock_guard lock(g_mutex);
    if (!g_configured || logical_page_index >= g_pages.size()) return false;
    const PageState& page = g_pages[logical_page_index];
    if (revision == 0 || revision != page.applied_revision ||
        projected_row_index >= page.applied_visible_source_rows.size()) {
        return false;
    }
    source_row_index = page.applied_visible_source_rows[projected_row_index];
    return true;
}

bool map_source_row_to_projected(
    std::size_t logical_page_index,
    std::uint64_t revision,
    std::size_t source_row_index,
    std::size_t& projected_row_index) noexcept {
    projected_row_index = invalid_compiled_index;
    std::lock_guard lock(g_mutex);
    if (!g_configured || logical_page_index >= g_pages.size()) return false;
    const PageState& page = g_pages[logical_page_index];
    if (revision == 0 || revision != page.applied_revision ||
        source_row_index >= page.source_slots.size()) {
        return false;
    }
    const auto found = std::find(
        page.applied_visible_source_rows.begin(),
        page.applied_visible_source_rows.end(),
        source_row_index);
    if (found == page.applied_visible_source_rows.end()) return false;
    projected_row_index = static_cast<std::size_t>(std::distance(
        page.applied_visible_source_rows.begin(), found));
    return true;
}

bool resolve_visible_paginated_slice(
    std::size_t logical_page_index,
    std::size_t first_page_capacity,
    std::size_t continuation_capacity,
    std::size_t slice_index,
    PageSlice& output,
    std::uint64_t& projection_revision) noexcept {
    output = {};
    projection_revision = 0;
    const RowVisibilityProjection projection =
        row_visibility_projection(logical_page_index);
    if (!projection || !resolve_paginated_slice(
            logical_page_index,
            projection.visible_row_count,
            first_page_capacity,
            continuation_capacity,
            slice_index,
            output)) {
        return false;
    }
    projection_revision = projection.revision;
    return true;
}

void reset_row_visibility_state() noexcept {
    std::lock_guard lock(g_mutex);
    g_entries.clear();
    g_pages.clear();
    g_slots.clear();
    g_preconfiguration_updates.clear();
    g_revision = 0;
    g_configured = false;
    g_pending = false;
}

} // namespace erui::detail
