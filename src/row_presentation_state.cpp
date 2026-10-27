#include "row_presentation_state.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
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

struct PendingSnapshot {
    std::wstring label{};
    std::wstring help{};
    std::optional<std::wstring> caption{};
    std::uint64_t revision{};
    std::uint64_t help_revision{};
    bool interactive{true};
    ERUI_AvailabilityAppearance availability_appearance{
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY};
    std::uint64_t availability_revision{};
};

struct Entry {
    RowIdentity identity{};
    std::wstring desired_label{};
    std::wstring desired_help{};
    std::optional<std::wstring> desired_caption{};
    std::unique_ptr<wchar_t[]> label_buffer{};
    std::unique_ptr<wchar_t[]> help_buffer{};
    std::unique_ptr<wchar_t[]> caption_buffer{};
    std::uint64_t desired_revision{};
    std::uint64_t applied_revision{};
    std::uint64_t desired_help_revision{};
    std::uint64_t applied_help_revision{};
    bool desired_interactive{true};
    bool applied_interactive{true};
    ERUI_AvailabilityAppearance desired_availability_appearance{
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY};
    ERUI_AvailabilityAppearance applied_availability_appearance{
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY};
    std::uint64_t desired_availability_revision{1};
    std::uint64_t applied_availability_revision{1};
    bool applied_caption_override{};
    bool dirty{};
};

std::mutex g_mutex{};
std::vector<std::unique_ptr<Entry>> g_entries{};
std::unordered_map<RowIdentity, std::size_t, RowIdentityHash> g_slots{};
std::unordered_map<RowIdentity, PendingSnapshot, RowIdentityHash>
    g_preconfiguration_updates{};
std::uint64_t g_revision{};
std::uint64_t g_help_revision{};
std::uint64_t g_availability_revision{1};
bool g_configured{};
bool g_pending{};

bool valid_snapshot(const RowPresentationUpdate& update) noexcept {
    const bool valid_appearance =
        update.availability_appearance ==
            ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY ||
        update.availability_appearance ==
            ERUI_AVAILABILITY_APPEARANCE_NORMAL ||
        update.availability_appearance ==
            ERUI_AVAILABILITY_APPEARANCE_DISABLED;
    return update.provider != ERUI_INVALID_PROVIDER &&
        update.row != ERUI_INVALID_ROW && !update.label.empty() &&
        update.label.size() <= row_presentation_max_text_units &&
        update.help.size() <= row_presentation_max_text_units &&
        (!update.caption.has_value() ||
            (!update.caption->empty() &&
             update.caption->size() <= row_presentation_max_text_units)) &&
        valid_appearance;
}

void copy_to_buffer(
    const std::wstring& source,
    wchar_t* destination) noexcept {
    std::copy(source.begin(), source.end(), destination);
    destination[source.size()] = L'\0';
}

[[nodiscard]] bool buffer_equals(
    const std::wstring& source,
    const wchar_t* destination) noexcept {
    return destination &&
        std::equal(source.begin(), source.end(), destination) &&
        destination[source.size()] == L'\0';
}

[[nodiscard]] std::uint64_t next_revision(
    std::uint64_t current) noexcept {
    std::uint64_t next = current + 1u;
    if (next == 0) next = 1;
    return next;
}

} // namespace

ERUI_Result submit_row_presentation_updates(
    std::vector<RowPresentationUpdate> updates) noexcept {
    if (updates.empty()) return ERUI_INVALID_ARGUMENT;
    for (const RowPresentationUpdate& update : updates) {
        if (!valid_snapshot(update)) return ERUI_INVALID_ARGUMENT;
    }

    try {
        struct PreparedBuffers {
            std::unique_ptr<wchar_t[]> label{};
            std::unique_ptr<wchar_t[]> help{};
            std::unique_ptr<wchar_t[]> caption{};
        };
        std::lock_guard lock(g_mutex);
        const std::uint64_t revision = next_revision(g_revision);
        const std::uint64_t availability_revision =
            next_revision(g_availability_revision);

        if (!g_configured) {
            // The registry has already validated duplicate targets. Reserve
            // all nodes first so map growth cannot partially publish.
            auto staged = g_preconfiguration_updates;
            std::vector<bool> help_changed{};
            help_changed.reserve(updates.size());
            bool batch_help_changed = false;
            for (const RowPresentationUpdate& update : updates) {
                const auto existing = staged.find(
                    RowIdentity{update.provider, update.row});
                const bool changed = existing == staged.end() ||
                    existing->second.help != update.help;
                help_changed.push_back(changed);
                batch_help_changed = batch_help_changed || changed;
            }
            const std::uint64_t help_revision = batch_help_changed
                ? next_revision(g_help_revision)
                : g_help_revision;
            std::size_t update_index = 0;
            for (RowPresentationUpdate& update : updates) {
                const RowIdentity identity{update.provider, update.row};
                const auto existing = staged.find(identity);
                const std::uint64_t row_help_revision =
                    help_changed[update_index]
                    ? help_revision
                    : (existing != staged.end()
                        ? existing->second.help_revision
                        : std::uint64_t{0});
                staged[identity] = {
                    .label = std::move(update.label),
                    .help = std::move(update.help),
                    .caption = std::move(update.caption),
                    .revision = revision,
                    .help_revision = row_help_revision,
                    .interactive = update.interactive,
                    .availability_appearance =
                        update.availability_appearance,
                    .availability_revision = availability_revision,
                };
                ++update_index;
            }
            g_preconfiguration_updates = std::move(staged);
            g_revision = revision;
            if (batch_help_changed) g_help_revision = help_revision;
            g_availability_revision = availability_revision;
            g_pending = true;
            return ERUI_OK;
        }

        std::vector<std::size_t> slots{};
        slots.reserve(updates.size());
        for (const RowPresentationUpdate& update : updates) {
            const auto found = g_slots.find(
                RowIdentity{update.provider, update.row});
            if (found == g_slots.end() || found->second >= g_entries.size()) {
                return ERUI_INVALID_HANDLE;
            }
            slots.push_back(found->second);
        }

        std::vector<bool> help_changed{};
        help_changed.reserve(updates.size());
        bool batch_help_changed = false;
        for (std::size_t index = 0; index < updates.size(); ++index) {
            const bool changed =
                g_entries[slots[index]]->desired_help != updates[index].help;
            help_changed.push_back(changed);
            batch_help_changed = batch_help_changed || changed;
        }
        const std::uint64_t help_revision = batch_help_changed
            ? next_revision(g_help_revision)
            : g_help_revision;

        // Allocate only buffers that do not exist yet. Every allocation is
        // completed before publication, preserving all-or-nothing OOM
        // behavior without churning worst-case encoded-text buffers on every
        // edit.
        // This work never occurs at the UI frame boundary.
        std::vector<PreparedBuffers> prepared(updates.size());
        for (std::size_t index = 0; index < updates.size(); ++index) {
            const Entry& entry = *g_entries[slots[index]];
            if (!entry.label_buffer) {
                prepared[index].label = std::make_unique<wchar_t[]>(
                    row_presentation_max_text_units + 1u);
            }
            if (!entry.help_buffer) {
                prepared[index].help = std::make_unique<wchar_t[]>(
                    row_presentation_max_text_units + 1u);
            }
            if (updates[index].caption.has_value() &&
                !entry.caption_buffer) {
                prepared[index].caption = std::make_unique<wchar_t[]>(
                    row_presentation_max_text_units + 1u);
            }
        }

        // All validation and allocations succeeded. Moving strings and
        // unique_ptrs is noexcept, so publication below cannot be partial.
        for (std::size_t index = 0; index < updates.size(); ++index) {
            Entry& entry = *g_entries[slots[index]];
            if (!entry.label_buffer) {
                entry.label_buffer = std::move(prepared[index].label);
            }
            if (!entry.help_buffer) {
                entry.help_buffer = std::move(prepared[index].help);
            }
            if (updates[index].caption.has_value() &&
                !entry.caption_buffer) {
                entry.caption_buffer = std::move(prepared[index].caption);
            }
            entry.desired_label = std::move(updates[index].label);
            entry.desired_help = std::move(updates[index].help);
            entry.desired_caption = std::move(updates[index].caption);
            entry.desired_revision = revision;
            if (help_changed[index]) {
                entry.desired_help_revision = help_revision;
            }
            entry.desired_interactive = updates[index].interactive;
            entry.desired_availability_appearance =
                updates[index].availability_appearance;
            entry.desired_availability_revision = availability_revision;
            entry.dirty = true;
        }
        g_revision = revision;
        if (batch_help_changed) g_help_revision = help_revision;
        g_availability_revision = availability_revision;
        g_pending = true;
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

bool configure_row_presentation_state(CompiledMenu& menu) noexcept {
    try {
        std::lock_guard lock(g_mutex);
        if (g_configured || !g_entries.empty() || !g_slots.empty()) {
            return false;
        }

        std::vector<std::unique_ptr<Entry>> entries{};
        std::unordered_map<RowIdentity, std::size_t, RowIdentityHash> slots{};
        std::size_t count = 0;
        for (const CompiledPage& page : menu.pages) {
            for (const CompiledRow& row : page.rows) {
                if (row.provider != ERUI_INVALID_PROVIDER &&
                    row.row != ERUI_INVALID_ROW) {
                    ++count;
                }
            }
        }
        entries.reserve(count);
        slots.reserve(count);

        for (CompiledPage& page : menu.pages) {
            for (CompiledRow& row : page.rows) {
                if (row.provider == ERUI_INVALID_PROVIDER ||
                    row.row == ERUI_INVALID_ROW) {
                    row.presentation_slot = invalid_compiled_index;
                    continue;
                }
                const RowIdentity identity{row.provider, row.row};
                auto entry = std::make_unique<Entry>();
                entry->identity = identity;
                const wchar_t* const label = menu.texts.lookup(
                    static_cast<int>(row.label_id));
                const wchar_t* const help = menu.texts.lookup(
                    static_cast<int>(row.help_id));
                entry->desired_label = label ? label : L"";
                entry->desired_help = help ? help : L"";
                entry->desired_interactive = row.enabled;
                entry->applied_interactive = row.enabled;

                if (const auto pending =
                        g_preconfiguration_updates.find(identity);
                    pending != g_preconfiguration_updates.end()) {
                    const bool help_differs_from_registration =
                        entry->desired_help != pending->second.help;
                    entry->desired_label = pending->second.label;
                    entry->desired_help = pending->second.help;
                    entry->desired_caption = pending->second.caption;
                    entry->desired_revision = pending->second.revision;
                    entry->desired_help_revision =
                        help_differs_from_registration
                        ? pending->second.help_revision
                        : std::uint64_t{0};
                    entry->desired_interactive =
                        pending->second.interactive;
                    entry->desired_availability_appearance =
                        pending->second.availability_appearance;
                    entry->desired_availability_revision =
                        pending->second.availability_revision;
                    entry->label_buffer = std::make_unique<wchar_t[]>(
                        row_presentation_max_text_units + 1u);
                    entry->help_buffer = std::make_unique<wchar_t[]>(
                        row_presentation_max_text_units + 1u);
                    if (entry->desired_caption.has_value()) {
                        entry->caption_buffer = std::make_unique<wchar_t[]>(
                            row_presentation_max_text_units + 1u);
                    }
                    entry->dirty = true;
                    g_pending = true;
                }

                const std::size_t slot = entries.size();
                if (!slots.emplace(identity, slot).second) return false;
                row.presentation_slot = slot;
                entries.push_back(std::move(entry));
            }
        }

        // A pre-runtime update naming a row that never reached the compiled
        // menu is a violated registry/compiler invariant. Fail closed.
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

void synchronize_pending_row_presentations() noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_configured || !g_pending) return;
    bool still_pending = false;
    for (const std::unique_ptr<Entry>& entry_pointer : g_entries) {
        Entry& entry = *entry_pointer;
        if (!entry.dirty) continue;
        if (!entry.label_buffer || !entry.help_buffer ||
            entry.desired_label.empty() ||
            entry.desired_label.size() > row_presentation_max_text_units ||
            entry.desired_help.size() > row_presentation_max_text_units ||
            (entry.desired_caption.has_value() &&
                (!entry.caption_buffer || entry.desired_caption->empty() ||
                 entry.desired_caption->size() >
                     row_presentation_max_text_units))) {
            still_pending = true;
            continue;
        }
        // A caller may queue several complete snapshots before this UI
        // boundary. If the final help payload equals the already-published
        // buffer, collapse the intermediate edits without creating a native
        // help-refresh generation.
        if (entry.applied_revision != 0 &&
            buffer_equals(entry.desired_help, entry.help_buffer.get())) {
            entry.desired_help_revision = entry.applied_help_revision;
        }
        copy_to_buffer(entry.desired_label, entry.label_buffer.get());
        copy_to_buffer(entry.desired_help, entry.help_buffer.get());
        if (entry.desired_caption.has_value()) {
            copy_to_buffer(
                *entry.desired_caption, entry.caption_buffer.get());
        }
        entry.applied_caption_override =
            entry.desired_caption.has_value();
        entry.applied_revision = entry.desired_revision;
        entry.applied_help_revision = entry.desired_help_revision;
        entry.applied_interactive = entry.desired_interactive;
        entry.applied_availability_appearance =
            entry.desired_availability_appearance;
        entry.applied_availability_revision =
            entry.desired_availability_revision;
        entry.dirty = false;
    }
    g_pending = still_pending;
}

RowPresentationView row_presentation_view(std::size_t slot) noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_configured || slot >= g_entries.size()) return {};
    const Entry& entry = *g_entries[slot];
    if (entry.applied_revision == 0 || !entry.label_buffer ||
        !entry.help_buffer) {
        return {};
    }
    return {
        .label = entry.label_buffer.get(),
        .help = entry.help_buffer.get(),
        .caption = entry.applied_caption_override
            ? entry.caption_buffer.get()
            : nullptr,
        .revision = entry.applied_revision,
        .help_revision = entry.applied_help_revision,
    };
}

RowAvailabilityView row_availability_view(std::size_t slot) noexcept {
    std::lock_guard lock(g_mutex);
    if (!g_configured || slot >= g_entries.size()) return {};
    const Entry& entry = *g_entries[slot];
    return {
        .interactive = entry.applied_interactive,
        .appearance = entry.applied_availability_appearance,
        .revision = entry.applied_availability_revision,
    };
}

bool row_presentation_updates_pending() noexcept {
    std::lock_guard lock(g_mutex);
    return g_pending;
}

std::size_t editable_row_presentation_count() noexcept {
    std::lock_guard lock(g_mutex);
    return g_entries.size();
}

void reset_row_presentation_state() noexcept {
    std::lock_guard lock(g_mutex);
    g_entries.clear();
    g_slots.clear();
    g_preconfiguration_updates.clear();
    g_revision = 0;
    g_help_revision = 0;
    g_availability_revision = 1;
    g_configured = false;
    g_pending = false;
}

} // namespace erui::detail
