#include "tab_update_state.hpp"

#include <algorithm>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace erui::detail {
namespace {

struct TabIdentity final {
    ERUI_ProviderHandle provider{};
    ERUI_PageHandle page{};

    friend bool operator==(const TabIdentity&, const TabIdentity&) = default;
};

struct TabIdentityHash final {
    std::size_t operator()(const TabIdentity& value) const noexcept {
        const std::size_t first = std::hash<std::uint64_t>{}(value.provider);
        const std::size_t second = std::hash<std::uint64_t>{}(value.page);
        return first ^ (second + static_cast<std::size_t>(0x9E3779B9u) +
            (first << 6u) + (first >> 2u));
    }
};

struct PendingUpdate final {
    ERUI_TabUpdateFields fields{};
    std::wstring title{};
    TabIconSpec icon{};
    bool visible{true};
    std::int32_t preferred_order{};
    std::uint64_t revision{};
};

using PendingMap =
    std::unordered_map<TabIdentity, PendingUpdate, TabIdentityHash>;

std::mutex g_mutex{};
std::shared_ptr<const TabRuntimeGeneration> g_desired{};
std::shared_ptr<const TabRuntimeGeneration> g_applied{};
std::unordered_map<TabIdentity, std::size_t, TabIdentityHash> g_slots{};
PendingMap g_preconfiguration_updates{};
std::uint64_t g_revision{};
bool g_configured{};
bool g_pending{};

[[nodiscard]] std::size_t decimal_digits(std::size_t value) noexcept {
    std::size_t digits{1};
    while (value >= 10U) {
        value /= 10U;
        ++digits;
    }
    return digits;
}

void append_decimal(
    std::size_t value,
    wchar_t* output,
    std::size_t& offset) noexcept {
    wchar_t reversed[32]{};
    std::size_t count{};
    do {
        reversed[count++] = static_cast<wchar_t>(L'0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < std::size(reversed));
    while (count != 0U) output[offset++] = reversed[--count];
}

[[nodiscard]] std::uint64_t next_revision(std::uint64_t value) noexcept {
    ++value;
    return value == 0 ? 1 : value;
}

[[nodiscard]] bool valid_identity(const TabIdentity& identity) noexcept {
    return identity.provider != ERUI_INVALID_PROVIDER &&
        identity.page != ERUI_INVALID_PAGE;
}

[[nodiscard]] bool same_icon(
    const TabIconSpec& left,
    const TabIconSpec& right) noexcept {
    return left.source == right.source &&
        left.rgba8_pixels == right.rgba8_pixels;
}

void apply_update(
    TabRuntimeEntry& entry,
    TabRuntimeUpdate&& update,
    std::uint64_t revision) {
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_TITLE) != 0) {
        entry.title = std::move(update.title);
        entry.title_overridden = true;
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_ICON) != 0) {
        if (!same_icon(entry.icon, update.icon)) {
            entry.icon = std::move(update.icon);
            entry.icon_revision = revision;
        }
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_VISIBLE) != 0) {
        entry.visible = update.visible;
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER) != 0) {
        entry.preferred_order = update.preferred_order;
    }
    entry.effective_order = entry.player_order.value_or(
        entry.preferred_order);
}

void merge_pending(
    PendingUpdate& destination,
    TabRuntimeUpdate&& update,
    std::uint64_t revision) {
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_TITLE) != 0) {
        destination.title = std::move(update.title);
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_ICON) != 0) {
        destination.icon = std::move(update.icon);
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_VISIBLE) != 0) {
        destination.visible = update.visible;
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER) != 0) {
        destination.preferred_order = update.preferred_order;
    }
    destination.fields |= update.fields;
    destination.revision = revision;
}

void apply_pending(
    TabRuntimeEntry& entry,
    PendingUpdate&& update) {
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_TITLE) != 0) {
        entry.title = std::move(update.title);
        entry.title_overridden = true;
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_ICON) != 0) {
        if (!same_icon(entry.icon, update.icon)) {
            entry.icon = std::move(update.icon);
            entry.icon_revision = update.revision;
        }
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_VISIBLE) != 0) {
        entry.visible = update.visible;
    }
    if ((update.fields & ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER) != 0) {
        entry.preferred_order = update.preferred_order;
    }
    entry.effective_order = entry.player_order.value_or(
        entry.preferred_order);
}

void rebuild_visible_order(TabRuntimeGeneration& generation) {
    generation.visible_order.clear();
    generation.visible_order.reserve(generation.entries.size());
    for (const TabRuntimeEntry& entry : generation.entries) {
        if (entry.visible) {
            generation.visible_order.push_back(entry.stable_index);
        }
    }
    std::sort(
        generation.visible_order.begin(),
        generation.visible_order.end(),
        [&generation](std::size_t left_index, std::size_t right_index) {
            const TabRuntimeEntry& left = generation.entries[left_index];
            const TabRuntimeEntry& right = generation.entries[right_index];
            if (left.effective_order != right.effective_order) {
                return left.effective_order < right.effective_order;
            }
            if (left.provider_id != right.provider_id) {
                return left.provider_id < right.provider_id;
            }
            if (left.tab_id != right.tab_id) {
                return left.tab_id < right.tab_id;
            }
            return left.stable_index < right.stable_index;
        });
}

} // namespace

struct PreparedTabRuntimeUpdates::Impl final {
    explicit Impl(std::unique_lock<std::mutex> acquired) noexcept
        : lock(std::move(acquired)) {}

    std::unique_lock<std::mutex> lock{};
    bool configured{};
    std::uint64_t revision{};
    std::shared_ptr<const TabRuntimeGeneration> generation{};
    PendingMap preconfiguration_updates{};
};

PreparedTabRuntimeUpdates::PreparedTabRuntimeUpdates() noexcept = default;
PreparedTabRuntimeUpdates::~PreparedTabRuntimeUpdates() = default;
PreparedTabRuntimeUpdates::PreparedTabRuntimeUpdates(
    PreparedTabRuntimeUpdates&&) noexcept = default;
PreparedTabRuntimeUpdates& PreparedTabRuntimeUpdates::operator=(
    PreparedTabRuntimeUpdates&&) noexcept = default;

PreparedTabRuntimeUpdates::operator bool() const noexcept {
    return impl_ != nullptr;
}

ERUI_Result prepare_tab_runtime_updates(
    std::vector<TabRuntimeUpdate> updates,
    PreparedTabRuntimeUpdates& output) noexcept {
    if (output || updates.empty()) return ERUI_INVALID_ARGUMENT;
    try {
        std::unordered_set<TabIdentity, TabIdentityHash> unique{};
        unique.reserve(updates.size());
        for (const TabRuntimeUpdate& update : updates) {
            const TabIdentity identity{update.provider, update.page};
            if (!valid_identity(identity) || update.fields == 0 ||
                !unique.insert(identity).second) {
                return ERUI_INVALID_ARGUMENT;
            }
        }

        auto prepared = std::make_unique<PreparedTabRuntimeUpdates::Impl>(
            std::unique_lock<std::mutex>(g_mutex));
        prepared->configured = g_configured;
        prepared->revision = next_revision(g_revision);
        if (!g_configured) {
            prepared->preconfiguration_updates =
                g_preconfiguration_updates;
            for (TabRuntimeUpdate& update : updates) {
                PendingUpdate& pending = prepared->preconfiguration_updates[
                    TabIdentity{update.provider, update.page}];
                merge_pending(
                    pending, std::move(update), prepared->revision);
            }
        } else {
            if (!g_desired) return ERUI_INTERNAL_ERROR;
            auto generation =
                std::make_shared<TabRuntimeGeneration>(*g_desired);
            generation->revision = prepared->revision;
            for (TabRuntimeUpdate& update : updates) {
                const auto found = g_slots.find(
                    TabIdentity{update.provider, update.page});
                if (found == g_slots.end() ||
                    found->second >= generation->entries.size()) {
                    return ERUI_INVALID_HANDLE;
                }
                apply_update(
                    generation->entries[found->second],
                    std::move(update),
                    prepared->revision);
            }
            rebuild_visible_order(*generation);
            prepared->generation = std::move(generation);
        }
        output.impl_ = std::move(prepared);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

void commit_prepared_tab_runtime_updates(
    PreparedTabRuntimeUpdates&& prepared) noexcept {
    if (!prepared.impl_) return;
    PreparedTabRuntimeUpdates::Impl& state = *prepared.impl_;
    if (state.configured) {
        if (!state.generation) {
            prepared.impl_.reset();
            return;
        }
        g_desired = std::move(state.generation);
    } else {
        g_preconfiguration_updates =
            std::move(state.preconfiguration_updates);
    }
    g_revision = state.revision;
    g_pending = state.configured;
    prepared.impl_.reset();
}

bool configure_tab_update_state(
    const CompiledMenu& menu,
    const NativeTabCatalogue& catalogue) noexcept {
    try {
        std::lock_guard lock(g_mutex);
        if (g_configured || g_desired || g_applied || !g_slots.empty()) {
            return false;
        }
        const auto custom = catalogue.custom_entries();
        auto generation = std::make_shared<TabRuntimeGeneration>();
        generation->entries.reserve(custom.size());
        generation->visible_order.reserve(custom.size());
        std::unordered_map<TabIdentity, std::size_t, TabIdentityHash> slots{};
        slots.reserve(custom.size());
        const std::uint64_t base_revision = g_revision == 0 ? 1 : g_revision;
        generation->revision = base_revision;

        for (std::size_t index = 0; index < custom.size(); ++index) {
            const NativeTabCatalogueEntry& source = custom[index];
            if (!source.is_custom() || source.page_index >= menu.pages.size()) {
                return false;
            }
            const CompiledPage& page = menu.pages[source.page_index];
            const wchar_t* const title = menu.texts.lookup(
                static_cast<int>(source.title_id));
            if (page.provider == ERUI_INVALID_PROVIDER ||
                page.page == ERUI_INVALID_PAGE || !title || title[0] == L'\0') {
                return false;
            }
            const TabIdentity identity{page.provider, page.page};
            TabRuntimeEntry entry{
                .provider = page.provider,
                .page = page.page,
                .stable_index = index,
                .page_index = source.page_index,
                .provider_id = source.provider_id,
                .tab_id = source.tab_id,
                .title = title,
                .title_overridden = false,
                .icon = source.icon,
                .visible = true,
                .preferred_order = source.preferred_order,
                .player_order = source.player_order,
                .effective_order = source.effective_order,
                .icon_revision = 1,
            };
            if (auto pending = g_preconfiguration_updates.find(identity);
                pending != g_preconfiguration_updates.end()) {
                apply_pending(entry, std::move(pending->second));
                g_preconfiguration_updates.erase(pending);
            }
            const std::size_t slot = generation->entries.size();
            if (!slots.emplace(identity, slot).second) return false;
            generation->entries.push_back(std::move(entry));
        }
        if (!g_preconfiguration_updates.empty()) return false;
        rebuild_visible_order(*generation);
        g_slots = std::move(slots);
        g_desired = generation;
        g_applied = std::move(generation);
        g_revision = base_revision;
        g_configured = true;
        g_pending = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool tab_updates_pending() noexcept {
    std::lock_guard lock(g_mutex);
    return g_pending;
}

std::shared_ptr<const TabRuntimeGeneration>
acquire_applied_tab_generation() noexcept {
    std::lock_guard lock(g_mutex);
    return g_applied;
}

std::shared_ptr<const TabRuntimeGeneration>
acquire_desired_tab_generation() noexcept {
    std::lock_guard lock(g_mutex);
    return g_desired;
}

bool publish_applied_tab_generation(
    const std::shared_ptr<const TabRuntimeGeneration>& generation) noexcept {
    if (!generation) return false;
    std::lock_guard lock(g_mutex);
    if (!g_configured || !g_desired || !g_applied ||
        generation->entries.size() != g_desired->entries.size() ||
        generation->revision < g_applied->revision ||
        generation->revision > g_desired->revision) {
        return false;
    }
    g_applied = generation;
    const bool latest = g_desired.get() == generation.get() &&
        g_desired->revision == generation->revision;
    g_pending = !latest;
    return latest;
}

bool format_applied_tab_page_title(
    std::size_t logical_page_index,
    std::size_t slice_index,
    std::size_t slice_count,
    wchar_t* output,
    std::size_t output_capacity,
    std::uint64_t& out_revision) noexcept {
    out_revision = 0;
    if (!output || output_capacity == 0 || slice_count == 0 ||
        slice_index >= slice_count) {
        return false;
    }
    std::lock_guard lock(g_mutex);
    if (!g_configured || !g_applied) return false;
    const auto found = std::find_if(
        g_applied->entries.begin(),
        g_applied->entries.end(),
        [logical_page_index](const TabRuntimeEntry& entry) noexcept {
            return entry.page_index == logical_page_index;
        });
    if (found == g_applied->entries.end() ||
        !found->title_overridden || found->title.empty()) {
        return false;
    }
    const std::wstring& title = found->title;
    std::size_t required = title.size();
    if (slice_count > 1U) {
        required += 4U + decimal_digits(slice_index + 1U) +
            decimal_digits(slice_count);
    }
    if (required >= output_capacity) return false;
    std::copy(title.begin(), title.end(), output);
    std::size_t offset = title.size();
    if (slice_count > 1U) {
        output[offset++] = L' ';
        output[offset++] = L'(';
        append_decimal(slice_index + 1U, output, offset);
        output[offset++] = L'/';
        append_decimal(slice_count, output, offset);
        output[offset++] = L')';
    }
    output[offset] = L'\0';
    out_revision = g_applied->revision;
    return true;
}

void reset_tab_update_state() noexcept {
    std::lock_guard lock(g_mutex);
    g_desired.reset();
    g_applied.reset();
    g_slots.clear();
    g_preconfiguration_updates.clear();
    g_revision = 0;
    g_configured = false;
    g_pending = false;
}

} // namespace erui::detail
