#include <ernativeui/erui.h>

#include "compat_control.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kMaximumTextUnits = 4096;
constexpr std::size_t kMaximumIdentifierBytes = 255;

enum class RowKind : std::uint8_t {
    button,
    toggle,
    slider,
    inline_choice,
    popup_choice,
    submenu,
};

struct Row {
    ERUI_RowHandle handle{};
    RowKind kind{RowKind::button};
    std::u16string label{};
    std::u16string help{};
    ERUI_ButtonCallback button_callback{};
    ERUI_ValueChangedCallback value_callback{};
    void* user_data{};
    std::uint8_t value{};
    std::int32_t minimum{};
    std::int32_t maximum{255};
    std::int32_t step{1};
    std::vector<std::u16string> choices{};
    ERUI_PageHandle child_page{};
};

struct Page {
    ERUI_PageHandle handle{};
    std::u16string title{};
    ERUI_PageTitleFormatter formatter{};
    void* formatter_user_data{};
};

struct PendingAlert {
    bool active{};
    std::u16string message{};
    ERUI_AlertCallback callback{};
    void* user_data{};
};

struct State {
    std::mutex mutex{};
    std::uint64_t next_handle{1};
    bool provider_registered{};
    bool provider_committed{};
    ERUI_ProviderHandle provider{};
    ERUI_PageHandle root{};
    std::string provider_id{};
    std::u16string display_name{};
    std::unordered_map<ERUI_PageHandle, Page> pages{};
    std::unordered_map<ERUI_RowHandle, Row> rows{};
    PendingAlert alert{};
};

State g_state;

#define FIELD_END(type, field) \
    (offsetof(type, field) + sizeof(((type*)0)->field))

bool field_available(std::uint32_t size, std::size_t end) noexcept {
    return static_cast<std::size_t>(size) >= end;
}

bool valid_utf16_units(const std::u16string& value) noexcept {
    for (std::size_t index = 0; index < value.size(); ++index) {
        const std::uint16_t unit = static_cast<std::uint16_t>(value[index]);
        if (unit == 0) return false;
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (++index >= value.size()) return false;
            const std::uint16_t low =
                static_cast<std::uint16_t>(value[index]);
            if (low < 0xDC00u || low > 0xDFFFu) return false;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return false;
        }
    }
    return true;
}

std::u16string copy_utf16(ERUI_Utf16View view, bool allow_empty = true) {
    if (view.reserved != 0 || view.length > kMaximumTextUnits ||
        (view.length != 0 && !view.data) ||
        (!allow_empty && view.length == 0)) {
        throw std::invalid_argument("invalid UTF-16 view");
    }
    std::u16string result;
    if (view.length != 0) {
        const auto* first = reinterpret_cast<const char16_t*>(view.data);
        result.assign(first, first + view.length);
    }
    if (!valid_utf16_units(result)) {
        throw std::invalid_argument("malformed UTF-16");
    }
    return result;
}

std::string copy_identifier(ERUI_StringView view) {
    if (view.reserved != 0 || !view.data || view.length == 0 ||
        view.length > kMaximumIdentifierBytes ||
        std::memchr(view.data, '\0', view.length)) {
        throw std::invalid_argument("invalid identifier");
    }
    return {view.data, view.data + view.length};
}

void reset_locked(State& state) {
    state.next_handle = 1;
    state.provider_registered = false;
    state.provider_committed = false;
    state.provider = ERUI_INVALID_PROVIDER;
    state.root = ERUI_INVALID_PAGE;
    state.provider_id.clear();
    state.display_name.clear();
    state.pages.clear();
    state.rows.clear();
    state.alert = {};
}

ERUI_Result validate_draft_locked(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page) noexcept {
    if (!g_state.provider_registered || provider != g_state.provider ||
        g_state.pages.find(page) == g_state.pages.end()) {
        return ERUI_INVALID_HANDLE;
    }
    if (g_state.provider_committed) return ERUI_ALREADY_COMMITTED;
    return ERUI_OK;
}

ERUI_Result insert_row_locked(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    Row row,
    ERUI_RowHandle* out_row) {
    const ERUI_Result valid = validate_draft_locked(provider, page);
    if (valid != ERUI_OK) return valid;
    row.handle = g_state.next_handle++;
    const ERUI_RowHandle handle = row.handle;
    if (!g_state.rows.emplace(handle, std::move(row)).second) {
        return ERUI_INTERNAL_ERROR;
    }
    if (out_row) *out_row = handle;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL register_provider_entry(
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) {
    if (!out_provider || !out_root_page) return ERUI_INVALID_ARGUMENT;
    *out_provider = ERUI_INVALID_PROVIDER;
    *out_root_page = ERUI_INVALID_PAGE;
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_ProviderDesc, display_name)) ||
        description->api_version != ERUI_API_VERSION_1_0 ||
        description->flags != 0 || !description->owner_module) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::string id = copy_identifier(description->provider_id);
        std::u16string name = copy_utf16(description->display_name, false);
        std::lock_guard lock(g_state.mutex);
        if (g_state.provider_registered) return ERUI_DUPLICATE_PROVIDER_ID;
        g_state.provider = g_state.next_handle++;
        g_state.root = g_state.next_handle++;
        g_state.provider_registered = true;
        g_state.provider_id = std::move(id);
        g_state.display_name = std::move(name);
        Page root{};
        root.handle = g_state.root;
        root.title = g_state.display_name;
        g_state.pages.emplace(root.handle, std::move(root));
        *out_provider = g_state.provider;
        *out_root_page = g_state.root;
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL add_button_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ButtonDesc* description,
    ERUI_RowHandle* out_row) {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_ButtonDesc, reserved)) ||
        description->flags != 0 || description->reserved != 0 ||
        !description->callback || description->enabled > 1u) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        Row row{};
        row.kind = RowKind::button;
        row.label = copy_utf16(description->label, false);
        row.help = copy_utf16(description->help);
        row.button_callback = description->callback;
        row.user_data = description->user_data;
        std::lock_guard lock(g_state.mutex);
        return insert_row_locked(provider, page, std::move(row), out_row);
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL add_toggle_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ToggleDesc* description,
    ERUI_RowHandle* out_row) {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_ToggleDesc, enabled)) ||
        description->flags != 0 || description->enabled > 1u ||
        description->reserved8[0] || description->reserved8[1] ||
        description->reserved8[2]) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        Row row{};
        row.kind = RowKind::toggle;
        row.label = copy_utf16(description->label, false);
        row.help = copy_utf16(description->help);
        row.value_callback = description->changed_callback;
        row.user_data = description->user_data;
        row.value = description->initial_value ? 1u : 0u;
        std::lock_guard lock(g_state.mutex);
        return insert_row_locked(provider, page, std::move(row), out_row);
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL add_slider_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_SliderDesc* description,
    ERUI_RowHandle* out_row) {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_SliderDesc, enabled)) ||
        description->flags != 0 || description->enabled > 1u ||
        description->reserved8[0] || description->reserved8[1] ||
        description->reserved8[2] || description->minimum < 0 ||
        description->maximum > 255 ||
        description->minimum > description->maximum ||
        description->step <= 0 ||
        description->initial_value < description->minimum ||
        description->initial_value > description->maximum) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        Row row{};
        row.kind = RowKind::slider;
        row.label = copy_utf16(description->label, false);
        row.help = copy_utf16(description->help);
        row.value_callback = description->changed_callback;
        row.user_data = description->user_data;
        row.value = description->initial_value;
        row.minimum = description->minimum;
        row.maximum = description->maximum;
        row.step = description->step;
        std::lock_guard lock(g_state.mutex);
        return insert_row_locked(provider, page, std::move(row), out_row);
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result add_choice(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ChoiceDesc* description,
    ERUI_RowHandle* out_row,
    RowKind kind) {
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_ChoiceDesc, reserved8)) ||
        description->flags != 0 || description->reserved8[0] ||
        description->reserved8[1] || description->reserved8[2] ||
        !description->options || description->option_count == 0 ||
        description->option_count > 32 ||
        description->initial_index >= description->option_count) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        Row row{};
        row.kind = kind;
        row.label = copy_utf16(description->label, false);
        row.help = copy_utf16(description->help);
        row.value_callback = description->changed_callback;
        row.user_data = description->user_data;
        row.value = description->initial_index;
        row.choices.reserve(description->option_count);
        for (std::uint32_t index = 0;
             index < description->option_count;
             ++index) {
            row.choices.push_back(copy_utf16(
                description->options[index], false));
        }
        std::lock_guard lock(g_state.mutex);
        return insert_row_locked(provider, page, std::move(row), out_row);
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL add_inline_choice_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ChoiceDesc* description,
    ERUI_RowHandle* out_row) {
    return add_choice(
        provider, page, description, out_row, RowKind::inline_choice);
}

ERUI_Result ERUI_CALL add_popup_choice_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ChoiceDesc* description,
    ERUI_RowHandle* out_row) {
    return add_choice(
        provider, page, description, out_row, RowKind::popup_choice);
}

ERUI_Result ERUI_CALL add_submenu_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle parent_page,
    const ERUI_SubmenuDesc* description,
    ERUI_PageHandle* out_child_page,
    ERUI_RowHandle* out_row) {
    if (!out_child_page) return ERUI_INVALID_ARGUMENT;
    *out_child_page = ERUI_INVALID_PAGE;
    if (out_row) *out_row = ERUI_INVALID_ROW;
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_SubmenuDesc, reserved)) ||
        description->flags != 0 || description->reserved != 0 ||
        description->enabled > 1u) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        Row row{};
        row.kind = RowKind::submenu;
        row.label = copy_utf16(description->label, false);
        row.help = copy_utf16(description->help);
        std::u16string title = copy_utf16(description->page_title);
        (void)copy_utf16(description->page_help);
        if (title.empty()) title = row.label;

        std::lock_guard lock(g_state.mutex);
        const ERUI_Result valid =
            validate_draft_locked(provider, parent_page);
        if (valid != ERUI_OK) return valid;
        Page child{};
        child.handle = g_state.next_handle++;
        child.title = std::move(title);
        const ERUI_PageHandle child_handle = child.handle;
        g_state.pages.emplace(child_handle, std::move(child));
        row.child_page = child_handle;
        const ERUI_Result inserted = insert_row_locked(
            provider, parent_page, std::move(row), out_row);
        if (inserted != ERUI_OK) {
            g_state.pages.erase(child_handle);
            return inserted;
        }
        *out_child_page = child_handle;
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL set_page_presentation_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_PagePresentationDesc* description) {
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_PagePresentationDesc, reserved)) ||
        description->flags != 0 || description->reserved[0] ||
        description->reserved[1] ||
        (!description->formatter && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        (void)copy_utf16(description->menu_title);
        std::u16string title = copy_utf16(description->page_title);
        std::lock_guard lock(g_state.mutex);
        const ERUI_Result valid = validate_draft_locked(provider, page);
        if (valid != ERUI_OK) return valid;
        Page& target = g_state.pages.at(page);
        if (!title.empty()) target.title = std::move(title);
        target.formatter = description->formatter;
        target.formatter_user_data = description->user_data;
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL commit_provider_entry(ERUI_ProviderHandle provider) {
    std::lock_guard lock(g_state.mutex);
    if (!g_state.provider_registered || provider != g_state.provider) {
        return ERUI_INVALID_HANDLE;
    }
    if (g_state.provider_committed) return ERUI_ALREADY_COMMITTED;
    g_state.provider_committed = true;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL abort_provider_entry(ERUI_ProviderHandle provider) {
    std::lock_guard lock(g_state.mutex);
    if (!g_state.provider_registered || provider != g_state.provider) {
        return ERUI_INVALID_HANDLE;
    }
    if (g_state.provider_committed) return ERUI_ALREADY_COMMITTED;
    reset_locked(g_state);
    return ERUI_OK;
}

ERUI_Result ERUI_CALL set_row_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row_handle,
    std::uint8_t value) {
    std::lock_guard lock(g_state.mutex);
    if (!g_state.provider_committed || provider != g_state.provider) {
        return ERUI_INVALID_HANDLE;
    }
    const auto found = g_state.rows.find(row_handle);
    if (found == g_state.rows.end()) return ERUI_INVALID_HANDLE;
    Row& row = found->second;
    switch (row.kind) {
    case RowKind::toggle:
        row.value = value ? 1u : 0u;
        break;
    case RowKind::slider: {
        const int clamped = std::clamp<int>(
            value, row.minimum, row.maximum);
        const int offset = clamped - row.minimum;
        row.value = static_cast<std::uint8_t>(
            row.minimum + (offset / row.step) * row.step);
        break;
    }
    case RowKind::inline_choice:
    case RowKind::popup_choice:
        if (value >= row.choices.size()) return ERUI_INVALID_ARGUMENT;
        row.value = value;
        break;
    default:
        return ERUI_INVALID_ARGUMENT;
    }
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_row_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row_handle,
    std::uint8_t* out_value) {
    if (!out_value) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(g_state.mutex);
    if (!g_state.provider_committed || provider != g_state.provider) {
        return ERUI_INVALID_HANDLE;
    }
    const auto found = g_state.rows.find(row_handle);
    if (found == g_state.rows.end()) return ERUI_INVALID_HANDLE;
    switch (found->second.kind) {
    case RowKind::toggle:
    case RowKind::slider:
    case RowKind::inline_choice:
    case RowKind::popup_choice:
        *out_value = found->second.value;
        return ERUI_OK;
    default:
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL enqueue_alert_entry(
    ERUI_ProviderHandle provider,
    const ERUI_AlertDesc* description) {
    if (!description ||
        !field_available(description->size,
            FIELD_END(ERUI_AlertDesc, placement)) ||
        description->flags != 0 ||
        description->buttons > ERUI_ALERT_BUTTONS_DISMISS_ONLY ||
        description->placement > ERUI_ALERT_PLACEMENT_CENTER ||
        (!description->callback && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }
    try {
        std::u16string message = copy_utf16(description->message, false);
        std::lock_guard lock(g_state.mutex);
        if (!g_state.provider_committed || provider != g_state.provider) {
            return ERUI_INVALID_HANDLE;
        }
        if (g_state.alert.active) return ERUI_QUEUE_FULL;
        g_state.alert.active = true;
        g_state.alert.message = std::move(message);
        g_state.alert.callback = description->callback;
        g_state.alert.user_data = description->user_data;
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INVALID_ARGUMENT;
    }
}

ERUI_Result ERUI_CALL get_game_language_entry(
    ERUI_GameLanguageInfo* out_language) {
    static constexpr char identifier[] = "english";
    if (!out_language ||
        !field_available(out_language->size,
            FIELD_END(ERUI_GameLanguageInfo, reserved))) {
        return ERUI_INVALID_ARGUMENT;
    }
    ERUI_GameLanguageInfo result{};
    result.size = sizeof(result);
    result.known_language = ERUI_GAME_LANGUAGE_ENGLISH;
    result.identifier.data = identifier;
    result.identifier.length = 7;
    *out_language = result;
    return ERUI_OK;
}

void ERUI_COMPAT_CALL control_reset() {
    std::lock_guard lock(g_state.mutex);
    reset_locked(g_state);
}

ERUI_CompatResult ERUI_COMPAT_CALL control_trigger_button(
    ERUI_CompatRowHandle row_handle) {
    ERUI_ButtonCallback callback{};
    void* user_data{};
    {
        std::lock_guard lock(g_state.mutex);
        const auto found = g_state.rows.find(row_handle);
        if (found == g_state.rows.end()) return ERUI_INVALID_HANDLE;
        if (found->second.kind != RowKind::button) {
            return ERUI_INVALID_ARGUMENT;
        }
        callback = found->second.button_callback;
        user_data = found->second.user_data;
    }
    if (!callback) return ERUI_INVALID_ARGUMENT;
    callback(user_data);
    return ERUI_OK;
}

ERUI_CompatResult ERUI_COMPAT_CALL control_trigger_value(
    ERUI_CompatRowHandle row_handle,
    std::uint8_t value) {
    ERUI_ValueChangedCallback callback{};
    void* user_data{};
    {
        std::lock_guard lock(g_state.mutex);
        const auto found = g_state.rows.find(row_handle);
        if (found == g_state.rows.end()) return ERUI_INVALID_HANDLE;
        Row& row = found->second;
        if (row.kind != RowKind::toggle && row.kind != RowKind::slider &&
            row.kind != RowKind::inline_choice &&
            row.kind != RowKind::popup_choice) {
            return ERUI_INVALID_ARGUMENT;
        }
        row.value = value;
        callback = row.value_callback;
        user_data = row.user_data;
    }
    if (callback) callback(user_data, value);
    return ERUI_OK;
}

ERUI_CompatResult ERUI_COMPAT_CALL control_format_page_title(
    ERUI_CompatPageHandle page_handle,
    std::uint32_t page_number,
    std::uint32_t page_count,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    ERUI_PageTitleFormatter formatter{};
    void* user_data{};
    ERUI_ProviderHandle provider{};
    std::u16string title;
    {
        std::lock_guard lock(g_state.mutex);
        const auto found = g_state.pages.find(page_handle);
        if (found == g_state.pages.end()) return ERUI_INVALID_HANDLE;
        formatter = found->second.formatter;
        user_data = found->second.formatter_user_data;
        provider = g_state.provider;
        title = found->second.title;
    }
    if (!formatter || !out_length || !output || output_capacity == 0 ||
        page_number == 0 || page_count == 0 || page_number > page_count) {
        return ERUI_INVALID_ARGUMENT;
    }
    ERUI_PageTitleFormatContext context{};
    context.size = sizeof(context);
    context.provider = provider;
    context.page = page_handle;
    context.base_title.data =
        reinterpret_cast<const std::uint16_t*>(title.data());
    context.base_title.length = static_cast<std::uint32_t>(title.size());
    context.page_number = page_number;
    context.page_count = page_count;
    return formatter(
        user_data, &context, output, output_capacity, out_length);
}

ERUI_CompatResult ERUI_COMPAT_CALL control_complete_alert(
    std::uint32_t completion_result,
    std::uint32_t response) {
    ERUI_AlertCallback callback{};
    void* user_data{};
    {
        std::lock_guard lock(g_state.mutex);
        if (!g_state.alert.active) return ERUI_INVALID_HANDLE;
        callback = g_state.alert.callback;
        user_data = g_state.alert.user_data;
        g_state.alert = {};
    }
    if (callback) callback(user_data, completion_result, response);
    return ERUI_OK;
}

ERUI_CompatResult copy_control_text(
    const std::u16string& value,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    if (!out_length || !output || output_capacity < value.size()) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (!value.empty()) {
        std::memcpy(output, value.data(), value.size() * sizeof(char16_t));
    }
    *out_length = static_cast<std::uint32_t>(value.size());
    return ERUI_OK;
}

ERUI_CompatResult ERUI_COMPAT_CALL control_get_row_label(
    ERUI_CompatRowHandle row_handle,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    std::lock_guard lock(g_state.mutex);
    const auto found = g_state.rows.find(row_handle);
    if (found == g_state.rows.end()) return ERUI_INVALID_HANDLE;
    return copy_control_text(
        found->second.label, output, output_capacity, out_length);
}

ERUI_CompatResult ERUI_COMPAT_CALL control_get_choice_text(
    ERUI_CompatRowHandle row_handle,
    std::uint32_t option_index,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    std::lock_guard lock(g_state.mutex);
    const auto found = g_state.rows.find(row_handle);
    if (found == g_state.rows.end()) return ERUI_INVALID_HANDLE;
    if (option_index >= found->second.choices.size()) {
        return ERUI_INVALID_ARGUMENT;
    }
    return copy_control_text(
        found->second.choices[option_index],
        output, output_capacity, out_length);
}

ERUI_Api make_v1_0_table() noexcept {
    ERUI_Api table{};
    table.size = ERUI_API_V1_0_SIZE;
    table.api_version = ERUI_API_VERSION_1_0;
    table.capabilities = ERUI_CAP_TOGGLE | ERUI_CAP_SLIDER |
        ERUI_CAP_BUTTON | ERUI_CAP_SUBMENU | ERUI_CAP_PAGINATION |
        ERUI_CAP_HOST_OWNED_VALUES | ERUI_CAP_PAGE_PRESENTATION |
        ERUI_CAP_ALERT | ERUI_CAP_INLINE_CHOICE |
        ERUI_CAP_POPUP_CHOICE | ERUI_CAP_GAME_LANGUAGE;
    table.register_provider = &register_provider_entry;
    table.add_button = &add_button_entry;
    table.add_toggle = &add_toggle_entry;
    table.add_slider = &add_slider_entry;
    table.add_inline_choice = &add_inline_choice_entry;
    table.add_popup_choice = &add_popup_choice_entry;
    table.add_submenu = &add_submenu_entry;
    table.set_page_presentation = &set_page_presentation_entry;
    table.commit_provider = &commit_provider_entry;
    table.abort_provider = &abort_provider_entry;
    table.set_row_value = &set_row_value_entry;
    table.get_row_value = &get_row_value_entry;
    table.enqueue_alert = &enqueue_alert_entry;
    table.get_game_language = &get_game_language_entry;
    return table;
}

} // namespace

extern "C" ERUI_EXPORT ERUI_Result ERUI_CALL ERUI_GetApi(
    std::uint32_t requested_version,
    ERUI_Api* out_api) {
    if (!out_api) return ERUI_INVALID_ARGUMENT;
    if (requested_version != ERUI_API_VERSION_1_0) {
        return ERUI_UNSUPPORTED_VERSION;
    }
    if (out_api->size < ERUI_API_V1_0_SIZE) {
        return ERUI_INVALID_ARGUMENT;
    }
    const ERUI_Api table = make_v1_0_table();
    std::memcpy(out_api, &table, ERUI_API_V1_0_SIZE);
    return ERUI_OK;
}

extern "C" ERUI_COMPAT_EXPORT ERUI_CompatResult ERUI_COMPAT_CALL
ERUI_CompatGetControl(
    std::uint32_t requested_version,
    ERUI_CompatControl* out_control) {
    if (!out_control) return ERUI_INVALID_ARGUMENT;
    if (requested_version != ERUI_COMPAT_CONTROL_VERSION) {
        return ERUI_UNSUPPORTED_VERSION;
    }
    if (out_control->size < ERUI_COMPAT_CONTROL_V1_SIZE) {
        return ERUI_INVALID_ARGUMENT;
    }
    ERUI_CompatControl control{};
    control.size = ERUI_COMPAT_CONTROL_V1_SIZE;
    control.version = ERUI_COMPAT_CONTROL_VERSION;
    control.reset = &control_reset;
    control.trigger_button = &control_trigger_button;
    control.trigger_value = &control_trigger_value;
    control.format_page_title = &control_format_page_title;
    control.complete_alert = &control_complete_alert;
    control.get_row_label = &control_get_row_label;
    control.get_choice_text = &control_get_choice_text;
    std::memcpy(out_control, &control, ERUI_COMPAT_CONTROL_V1_SIZE);
    return ERUI_OK;
}
