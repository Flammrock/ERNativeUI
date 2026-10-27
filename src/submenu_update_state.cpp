#include "submenu_update_state.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace erui::detail {
namespace {

struct PageIdentity {
    ERUI_ProviderHandle provider{};
    ERUI_PageHandle page{};

    friend bool operator==(const PageIdentity&, const PageIdentity&) = default;
};

struct PageIdentityHash {
    std::size_t operator()(const PageIdentity& value) const noexcept {
        const std::size_t first = std::hash<std::uint64_t>{}(value.provider);
        const std::size_t second = std::hash<std::uint64_t>{}(value.page);
        return first ^ (second + static_cast<std::size_t>(0x9E3779B9u) +
            (first << 6u) + (first >> 2u));
    }
};

using SharedTitle = std::shared_ptr<const std::wstring>;

struct PendingTitle {
    SharedTitle title{};
    std::uint64_t revision{};
};

struct Entry {
    PageIdentity identity{};
    SharedTitle desired_title{};
    SharedTitle applied_title{};
    std::uint64_t desired_revision{};
    std::uint64_t applied_revision{};
    bool dirty{};
};

std::mutex g_mutex{};
std::vector<Entry> g_entries{};
std::unordered_map<PageIdentity, std::size_t, PageIdentityHash> g_slots{};
std::unordered_map<PageIdentity, PendingTitle, PageIdentityHash>
    g_preconfiguration_updates{};
std::uint64_t g_revision{};
bool g_configured{};
bool g_pending{};

std::uint64_t next_revision(std::uint64_t current) noexcept {
    std::uint64_t next = current + 1u;
    return next == 0 ? 1u : next;
}

std::size_t decimal_digits(std::size_t value) noexcept {
    std::size_t digits = 1;
    while (value >= 10) {
        value /= 10;
        ++digits;
    }
    return digits;
}

bool title_fits(
    std::wstring_view title,
    std::size_t maximum_slice_count) noexcept {
    if (title.empty() || maximum_slice_count == 0) return false;
    std::size_t required = title.size();
    if (maximum_slice_count > 1) {
        // Space, '(', '/', ')' plus the two decimal numbers.
        const std::size_t digits = decimal_digits(maximum_slice_count);
        if (required > (std::numeric_limits<std::size_t>::max)() -
                (4u + digits * 2u)) {
            return false;
        }
        required += 4u + digits * 2u;
    }
    return required < ERUI_PAGE_TITLE_BUFFER_CAPACITY;
}

void append_decimal(
    std::size_t value,
    wchar_t* output,
    std::size_t& offset) noexcept {
    wchar_t reversed[32]{};
    std::size_t count{};
    do {
        reversed[count++] = static_cast<wchar_t>(L'0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < 32u);
    while (count != 0) output[offset++] = reversed[--count];
}

} // namespace

struct PreparedSubmenuTitleUpdates::State {
    bool configured{};
    std::vector<Entry> entries{};
    std::unordered_map<PageIdentity, PendingTitle, PageIdentityHash>
        preconfiguration_updates{};
    std::uint64_t revision{};
    bool pending{};
};

PreparedSubmenuTitleUpdates::PreparedSubmenuTitleUpdates() noexcept = default;
PreparedSubmenuTitleUpdates::PreparedSubmenuTitleUpdates(
    PreparedSubmenuTitleUpdates&&) noexcept = default;
PreparedSubmenuTitleUpdates& PreparedSubmenuTitleUpdates::operator=(
    PreparedSubmenuTitleUpdates&&) noexcept = default;
PreparedSubmenuTitleUpdates::~PreparedSubmenuTitleUpdates() = default;

PreparedSubmenuTitleUpdates::operator bool() const noexcept {
    return state_ != nullptr;
}

ERUI_Result prepare_submenu_title_updates(
    std::vector<SubmenuTitleUpdate> updates,
    PreparedSubmenuTitleUpdates& output) noexcept {
    output = {};
    if (updates.empty()) return ERUI_INVALID_ARGUMENT;
    try {
        std::unordered_set<PageIdentity, PageIdentityHash> unique{};
        unique.reserve(updates.size());
        std::vector<std::pair<PageIdentity, SharedTitle>> prepared{};
        prepared.reserve(updates.size());
        for (SubmenuTitleUpdate& update : updates) {
            const PageIdentity identity{update.provider, update.page};
            if (identity.provider == ERUI_INVALID_PROVIDER ||
                identity.page == ERUI_INVALID_PAGE ||
                !title_fits(update.title, update.maximum_slice_count) ||
                !unique.insert(identity).second) {
                return ERUI_INVALID_ARGUMENT;
            }
            prepared.emplace_back(
                identity,
                std::make_shared<const std::wstring>(
                    std::move(update.title)));
        }

        std::lock_guard lock(g_mutex);
        auto state = std::make_unique<PreparedSubmenuTitleUpdates::State>();
        state->configured = g_configured;
        state->revision = next_revision(g_revision);
        state->pending = true;
        if (!g_configured) {
            state->preconfiguration_updates = g_preconfiguration_updates;
            for (auto& [identity, title] : prepared) {
                state->preconfiguration_updates[identity] = {
                    .title = std::move(title),
                    .revision = state->revision,
                };
            }
        } else {
            state->entries = g_entries;
            for (auto& [identity, title] : prepared) {
                const auto found = g_slots.find(identity);
                if (found == g_slots.end() ||
                    found->second >= state->entries.size()) {
                    return ERUI_INVALID_HANDLE;
                }
                Entry& entry = state->entries[found->second];
                entry.desired_title = std::move(title);
                entry.desired_revision = state->revision;
                entry.dirty = true;
            }
        }
        output.state_ = std::move(state);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

void commit_prepared_submenu_title_updates(
    PreparedSubmenuTitleUpdates&& prepared) noexcept {
    if (!prepared.state_) return;
    std::lock_guard lock(g_mutex);
    if (prepared.state_->configured) {
        g_entries.swap(prepared.state_->entries);
    } else {
        g_preconfiguration_updates.swap(
            prepared.state_->preconfiguration_updates);
    }
    g_revision = prepared.state_->revision;
    g_pending = prepared.state_->pending;
    prepared.state_.reset();
}

bool configure_submenu_update_state(CompiledMenu& menu) noexcept {
    try {
        std::lock_guard lock(g_mutex);
        if (g_configured || !g_entries.empty() || !g_slots.empty()) {
            return false;
        }
        std::vector<Entry> entries{};
        std::unordered_map<PageIdentity, std::size_t, PageIdentityHash> slots{};
        std::size_t count{};
        for (const CompiledPage& page : menu.pages) {
            if (page.provider != ERUI_INVALID_PROVIDER &&
                page.page != ERUI_INVALID_PAGE) {
                ++count;
            }
        }
        entries.reserve(count);
        slots.reserve(count);
        for (CompiledPage& page : menu.pages) {
            if (page.provider == ERUI_INVALID_PROVIDER ||
                page.page == ERUI_INVALID_PAGE) {
                page.submenu_update_slot = invalid_compiled_index;
                continue;
            }
            const PageIdentity identity{page.provider, page.page};
            Entry entry{.identity = identity};
            if (const auto pending =
                    g_preconfiguration_updates.find(identity);
                pending != g_preconfiguration_updates.end()) {
                entry.desired_title = pending->second.title;
                entry.desired_revision = pending->second.revision;
                entry.dirty = true;
                g_pending = true;
            }
            const std::size_t slot = entries.size();
            if (!slots.emplace(identity, slot).second) return false;
            page.submenu_update_slot = slot;
            entries.push_back(std::move(entry));
        }
        for (const auto& pending : g_preconfiguration_updates) {
            if (slots.find(pending.first) == slots.end()) return false;
        }
        g_entries = std::move(entries);
        g_slots = std::move(slots);
        g_preconfiguration_updates.clear();
        g_configured = true;
        return true;
    } catch (...) {
        return false;
    }
}

void synchronize_pending_submenu_updates() noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_configured || !g_pending) return;
    bool still_pending{};
    for (Entry& entry : g_entries) {
        if (!entry.dirty) continue;
        if (!entry.desired_title || entry.desired_title->empty()) {
            still_pending = true;
            continue;
        }
        entry.applied_title = entry.desired_title;
        entry.applied_revision = entry.desired_revision;
        entry.dirty = false;
    }
    g_pending = still_pending;
}

bool format_submenu_page_title(
    std::size_t submenu_update_slot,
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
    if (!g_configured || submenu_update_slot >= g_entries.size()) {
        // Logical page indices and presentation slots intentionally are not
        // interchangeable. Resolve through the compiled page before calling.
        return false;
    }
    const Entry& entry = g_entries[submenu_update_slot];
    if (entry.applied_revision == 0 || !entry.applied_title) return false;
    const std::wstring& title = *entry.applied_title;
    std::size_t required = title.size();
    if (slice_count > 1) {
        required += 4u + decimal_digits(slice_index + 1u) +
            decimal_digits(slice_count);
    }
    if (required >= output_capacity) return false;
    std::copy(title.begin(), title.end(), output);
    std::size_t offset = title.size();
    if (slice_count > 1) {
        output[offset++] = L' ';
        output[offset++] = L'(';
        append_decimal(slice_index + 1u, output, offset);
        output[offset++] = L'/';
        append_decimal(slice_count, output, offset);
        output[offset++] = L')';
    }
    output[offset] = L'\0';
    out_revision = entry.applied_revision;
    return true;
}

bool submenu_updates_pending() noexcept {
    std::lock_guard lock(g_mutex);
    return g_pending;
}

void reset_submenu_update_state() noexcept {
    std::lock_guard lock(g_mutex);
    g_entries.clear();
    g_slots.clear();
    g_preconfiguration_updates.clear();
    g_revision = 0;
    g_configured = false;
    g_pending = false;
}

} // namespace erui::detail
