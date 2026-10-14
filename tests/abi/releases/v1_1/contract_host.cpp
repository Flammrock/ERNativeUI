#include "connection_control.h"

#include <Windows.h>

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace {

std::atomic_uint32_t g_not_ready_responses{};
std::atomic<ERUI_Result> g_terminal_result{ERUI_OK};
std::atomic<ERUI_Result> g_language_result{ERUI_OK};
std::atomic_uint32_t g_get_api_calls{};
std::atomic_uint32_t g_language_calls{};
std::atomic_uint32_t g_register_calls{};
std::atomic_uint32_t g_commit_calls{};
std::atomic_uint32_t g_abort_calls{};
std::atomic_uint64_t g_next_handle{UINT64_C(100)};

constexpr ERUI_Capabilities kCapabilities =
    ERUI_CAP_TOGGLE | ERUI_CAP_SLIDER | ERUI_CAP_BUTTON |
    ERUI_CAP_SUBMENU | ERUI_CAP_PAGINATION | ERUI_CAP_HOST_OWNED_VALUES |
    ERUI_CAP_PAGE_PRESENTATION | ERUI_CAP_ALERT | ERUI_CAP_INLINE_CHOICE |
    ERUI_CAP_POPUP_CHOICE | ERUI_CAP_GAME_LANGUAGE | ERUI_CAP_TEXT_INPUT |
    ERUI_CAP_COLOR_PICKER | ERUI_CAP_BUILTIN_PAGES |
    ERUI_CAP_INPUT_BINDINGS | ERUI_CAP_STORAGE;

struct ActionState {
    ERUI_ProviderHandle provider{};
    std::string id{};
    ERUI_ActionInputs defaults{};
    ERUI_ActionInputs current{};
};

struct ProviderState {
    ERUI_AssignmentsChangedCallback assignments_changed{};
    void* assignments_user_data{};
    ERUI_StorageHandle storage{ERUI_INVALID_STORAGE};
    bool storage_loaded{};
    bool storage_dirty{};
    std::uint64_t storage_revision{};
    std::uint64_t storage_saved_revision{};
    std::unordered_map<std::string, std::string> strings{};
    std::unordered_map<std::string, ERUI_ActionInputs> action_inputs{};
};

std::mutex g_state_mutex{};
std::unordered_map<ERUI_ProviderHandle, ProviderState> g_providers{};
std::unordered_map<ERUI_InputActionHandle, ActionState> g_actions{};

bool valid_slot(std::uint32_t state, std::uint32_t input,
    std::uint32_t count) noexcept {
    if (state == ERUI_INPUT_SLOT_ABSENT || state == ERUI_INPUT_SLOT_UNBOUND) {
        return input == 0;
    }
    return state == ERUI_INPUT_SLOT_BOUND && input > 0 && input < count;
}

bool valid_inputs(const ERUI_ActionInputs& value) noexcept {
    return value.size >= sizeof(value) && value.flags == 0 &&
        value.reserved[0] == 0 && value.reserved[1] == 0 &&
        valid_slot(value.controller.state, value.controller.input,
            ERUI_CONTROLLER_BUTTON_COUNT) &&
        valid_slot(value.keyboard.state, value.keyboard.input,
            ERUI_KEYBOARD_KEY_COUNT) &&
        valid_slot(value.mouse.state, value.mouse.input,
            ERUI_MOUSE_BUTTON_COUNT);
}

bool valid_machine_identifier(ERUI_StringView value) noexcept {
    if (!value.data || value.length == 0 ||
        value.length > ERUI_STORAGE_MAX_IDENTIFIER_BYTES ||
        value.reserved != 0) return false;
    for (std::uint32_t index = 0; index < value.length; ++index) {
        const unsigned char byte =
            static_cast<unsigned char>(value.data[index]);
        if (!((byte >= 'a' && byte <= 'z') ||
              (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') ||
              byte == '.' || byte == '_' || byte == '-')) {
            return false;
        }
    }
    return true;
}

template <typename Slot>
bool overlay_slot(const Slot& current, const Slot& patch, Slot& output) {
    output = current;
    if (patch.state == ERUI_INPUT_SLOT_ABSENT) return true;
    if (current.state == ERUI_INPUT_SLOT_ABSENT) return false;
    output = patch;
    return true;
}

bool overlay_inputs(const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& patch, ERUI_ActionInputs& output) {
    if (!valid_inputs(current) || !valid_inputs(patch)) return false;
    output = current;
    return overlay_slot(current.controller, patch.controller,
               output.controller) &&
        overlay_slot(current.keyboard, patch.keyboard, output.keyboard) &&
        overlay_slot(current.mouse, patch.mouse, output.mouse);
}

std::string copy_view(ERUI_StringView value) {
    return {value.data ? value.data : "", value.length};
}

bool valid_key(const ERUI_StorageKey* key) noexcept {
    return key && key->size >= sizeof(*key) && key->flags == 0 &&
        key->reserved[0] == 0 && key->reserved[1] == 0 &&
        key->section.data && key->section.length != 0 &&
        key->key.data && key->key.length != 0;
}

std::string storage_key(const ERUI_StorageKey& key) {
    std::string result = copy_view(key.section);
    result.push_back('\0');
    result += copy_view(key.key);
    return result;
}

ERUI_Result ERUI_CALL register_provider(
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) {
    g_register_calls.fetch_add(1, std::memory_order_relaxed);
    if (!description || !out_provider || !out_root_page ||
        description->size < sizeof(ERUI_ProviderDesc) ||
        description->api_version != ERUI_API_VERSION_1_1 ||
        !valid_machine_identifier(description->provider_id)) {
        return ERUI_INVALID_ARGUMENT;
    }
    *out_provider = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    *out_root_page = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard lock(g_state_mutex);
        g_providers.try_emplace(*out_provider);
    }
    return ERUI_OK;
}

ERUI_Result ERUI_CALL add_button(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_ButtonDesc*,
    ERUI_RowHandle* out_row) {
    if (!out_row) return ERUI_INVALID_ARGUMENT;
    *out_row = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    return ERUI_OK;
}

ERUI_Result ERUI_CALL add_toggle(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_ToggleDesc*,
    ERUI_RowHandle* out_row) {
    return add_button(0, 0, nullptr, out_row);
}

ERUI_Result ERUI_CALL add_slider(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_SliderDesc*,
    ERUI_RowHandle* out_row) {
    return add_button(0, 0, nullptr, out_row);
}

ERUI_Result ERUI_CALL add_choice(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_ChoiceDesc*,
    ERUI_RowHandle* out_row) {
    return add_button(0, 0, nullptr, out_row);
}

ERUI_Result ERUI_CALL add_submenu(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_SubmenuDesc*,
    ERUI_PageHandle* out_child,
    ERUI_RowHandle* out_row) {
    if (!out_child || !out_row) return ERUI_INVALID_ARGUMENT;
    *out_child = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    *out_row = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    return ERUI_OK;
}

ERUI_Result ERUI_CALL set_page_presentation(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_PagePresentationDesc*) {
    return ERUI_OK;
}

ERUI_Result ERUI_CALL commit_provider(ERUI_ProviderHandle provider) {
    g_commit_calls.fetch_add(1, std::memory_order_relaxed);
    return provider == ERUI_INVALID_PROVIDER ? ERUI_INVALID_HANDLE : ERUI_OK;
}

ERUI_Result ERUI_CALL abort_provider(ERUI_ProviderHandle provider) {
    g_abort_calls.fetch_add(1, std::memory_order_relaxed);
    return provider == ERUI_INVALID_PROVIDER ? ERUI_INVALID_HANDLE : ERUI_OK;
}

ERUI_Result ERUI_CALL set_row_value(
    ERUI_ProviderHandle,
    ERUI_RowHandle,
    uint8_t) {
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_row_value(
    ERUI_ProviderHandle,
    ERUI_RowHandle,
    uint8_t* out_value) {
    if (!out_value) return ERUI_INVALID_ARGUMENT;
    *out_value = 0;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL enqueue_alert(
    ERUI_ProviderHandle,
    const ERUI_AlertDesc*) {
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_game_language(
    ERUI_GameLanguageInfo* out_language) {
    g_language_calls.fetch_add(1, std::memory_order_relaxed);
    const ERUI_Result result =
        g_language_result.load(std::memory_order_relaxed);
    if (result != ERUI_OK) return result;
    if (!out_language || out_language->size < sizeof(*out_language)) {
        return ERUI_INVALID_ARGUMENT;
    }
    static constexpr char kIdentifier[] = "english";
    ERUI_GameLanguageInfo language{};
    language.size = sizeof(language);
    language.known_language = ERUI_GAME_LANGUAGE_ENGLISH;
    language.identifier.data = kIdentifier;
    language.identifier.length = sizeof(kIdentifier) - 1u;
    *out_language = language;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL add_text_input(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_TextInputDesc*,
    ERUI_RowHandle* out_row) {
    return add_button(0, 0, nullptr, out_row);
}

ERUI_Result ERUI_CALL set_text_input_value(
    ERUI_ProviderHandle,
    ERUI_RowHandle,
    const ERUI_Utf16View*) {
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_text_input_value(
    ERUI_ProviderHandle,
    ERUI_RowHandle,
    uint16_t*,
    uint32_t,
    uint32_t* out_length) {
    if (!out_length) return ERUI_INVALID_ARGUMENT;
    *out_length = 0;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL add_color_picker(
    ERUI_ProviderHandle,
    ERUI_PageHandle,
    const ERUI_ColorPickerDesc*,
    ERUI_RowHandle* out_row) {
    return add_button(0, 0, nullptr, out_row);
}

ERUI_Result ERUI_CALL set_color_picker_value(
    ERUI_ProviderHandle,
    ERUI_RowHandle,
    const ERUI_Color*) {
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_color_picker_value(
    ERUI_ProviderHandle,
    ERUI_RowHandle,
    ERUI_Color* out_value) {
    if (!out_value) return ERUI_INVALID_ARGUMENT;
    *out_value = {};
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_builtin_page(
    ERUI_ProviderHandle provider,
    ERUI_BuiltinPage builtin_page,
    ERUI_PageHandle* out_page) {
    if (!out_page) return ERUI_INVALID_ARGUMENT;
    *out_page = ERUI_INVALID_PAGE;
    if (provider == ERUI_INVALID_PROVIDER) return ERUI_INVALID_HANDLE;
    if (builtin_page >= ERUI_BUILTIN_PAGE_COUNT) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (builtin_page == ERUI_BUILTIN_PAGE_GAME_OPTIONS) {
        *out_page = provider + 1u;
    } else {
        *out_page = (provider << 8u) |
            (static_cast<ERUI_PageHandle>(builtin_page) + 1u);
    }
    return ERUI_OK;
}

ERUI_Result ERUI_CALL add_input_section(
    ERUI_ProviderHandle provider,
    const ERUI_InputSectionDesc* description,
    ERUI_InputSectionHandle* out_section) {
    if (provider == ERUI_INVALID_PROVIDER || !description ||
        description->size < sizeof(*description) ||
        description->flags != 0 || description->reserved[0] != 0 ||
        description->reserved[1] != 0 || !out_section) {
        return ERUI_INVALID_ARGUMENT;
    }
    *out_section = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    return ERUI_OK;
}

ERUI_Result ERUI_CALL add_input_action(
    ERUI_ProviderHandle provider,
    ERUI_InputSectionHandle section,
    const ERUI_InputActionDesc* description,
    ERUI_InputActionHandle* out_action) {
    if (provider == ERUI_INVALID_PROVIDER ||
        section == ERUI_INVALID_INPUT_SECTION || !description ||
        description->size < sizeof(*description) ||
        description->flags != 0 || description->reserved[0] != 0 ||
        description->reserved[1] != 0 ||
        !valid_machine_identifier(description->action_id) ||
        !description->activated_callback) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (!valid_inputs(description->default_inputs)) return ERUI_INVALID_ARGUMENT;
    const ERUI_InputActionHandle action =
        g_next_handle.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard lock(g_state_mutex);
        if (g_providers.find(provider) == g_providers.end()) {
            return ERUI_INVALID_HANDLE;
        }
        g_actions.emplace(action, ActionState{
            provider,
            copy_view(description->action_id),
            description->default_inputs,
            description->default_inputs,
        });
    }
    if (out_action) *out_action = action;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL set_assignments_changed_handler(
    ERUI_ProviderHandle provider,
    const ERUI_AssignmentsChangedHandlerDesc* description) {
    if (provider == ERUI_INVALID_PROVIDER || !description ||
        description->size < sizeof(*description) || description->flags != 0 ||
        description->reserved[0] != 0 || description->reserved[1] != 0 ||
        (!description->callback && description->user_data)) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end()) return ERUI_INVALID_HANDLE;
    found->second.assignments_changed = description->callback;
    found->second.assignments_user_data = description->user_data;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL set_action_inputs(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    const ERUI_ActionInputs* inputs) {
    if (provider == ERUI_INVALID_PROVIDER ||
        action == ERUI_INVALID_INPUT_ACTION || !inputs ||
        !valid_inputs(*inputs)) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_actions.find(action);
    if (found == g_actions.end() || found->second.provider != provider) {
        return ERUI_INVALID_HANDLE;
    }
    ERUI_ActionInputs next{};
    if (!overlay_inputs(found->second.current, *inputs, next)) {
        return ERUI_INVALID_ARGUMENT;
    }
    found->second.current = next;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_action_inputs(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    ERUI_ActionInputs* out_inputs) {
    if (provider == ERUI_INVALID_PROVIDER ||
        action == ERUI_INVALID_INPUT_ACTION || !out_inputs) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_actions.find(action);
    if (found == g_actions.end() || found->second.provider != provider) {
        return ERUI_INVALID_HANDLE;
    }
    *out_inputs = found->second.current;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL get_action_default_inputs(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    ERUI_ActionInputs* out_inputs) {
    if (provider == ERUI_INVALID_PROVIDER ||
        action == ERUI_INVALID_INPUT_ACTION || !out_inputs) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_actions.find(action);
    if (found == g_actions.end() || found->second.provider != provider) {
        return ERUI_INVALID_HANDLE;
    }
    *out_inputs = found->second.defaults;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL reset_action_inputs(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    ERUI_InputDevices devices) {
    if (provider == ERUI_INVALID_PROVIDER ||
        action == ERUI_INVALID_INPUT_ACTION ||
        devices == ERUI_INPUT_DEVICE_NONE ||
        (devices & ~ERUI_INPUT_DEVICE_ALL) != 0) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_actions.find(action);
    if (found == g_actions.end() || found->second.provider != provider) {
        return ERUI_INVALID_HANDLE;
    }
    if ((devices & ERUI_INPUT_DEVICE_CONTROLLER) != 0) {
        found->second.current.controller = found->second.defaults.controller;
    }
    if ((devices & ERUI_INPUT_DEVICE_KEYBOARD) != 0) {
        found->second.current.keyboard = found->second.defaults.keyboard;
    }
    if ((devices & ERUI_INPUT_DEVICE_MOUSE) != 0) {
        found->second.current.mouse = found->second.defaults.mouse;
    }
    return ERUI_OK;
}

ERUI_Result ERUI_CALL open_storage(
    ERUI_ProviderHandle provider,
    const ERUI_StorageDesc* description,
    ERUI_StorageHandle* out_storage) {
    if (provider == ERUI_INVALID_PROVIDER || !description || !out_storage ||
        description->size < sizeof(*description) || description->flags != 0 ||
        description->reserved0 != 0 || description->reserved[0] != 0 ||
        description->reserved[1] != 0) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end()) return ERUI_INVALID_HANDLE;
    if (found->second.storage == ERUI_INVALID_STORAGE) {
        found->second.storage =
            g_next_handle.fetch_add(1, std::memory_order_relaxed);
    }
    *out_storage = found->second.storage;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_ok(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage) {
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || storage == ERUI_INVALID_STORAGE ||
        found->second.storage != storage) return ERUI_INVALID_HANDLE;
    found->second.storage_loaded = true;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_save(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage) {
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || found->second.storage != storage) {
        return ERUI_INVALID_HANDLE;
    }
    if (!found->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    found->second.storage_saved_revision = found->second.storage_revision;
    found->second.storage_dirty = false;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_get_utf8(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    char* output,
    std::uint32_t capacity,
    std::uint32_t* out_length) {
    if (!valid_key(key) || !out_length || (!output && capacity != 0)) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found_provider = g_providers.find(provider);
    if (found_provider == g_providers.end() ||
        found_provider->second.storage != storage) return ERUI_INVALID_HANDLE;
    if (!found_provider->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    const auto found = found_provider->second.strings.find(storage_key(*key));
    if (found == found_provider->second.strings.end()) return ERUI_NOT_FOUND;
    *out_length = static_cast<std::uint32_t>(found->second.size());
    if (!output && capacity == 0) return ERUI_OK;
    if (capacity < found->second.size()) return ERUI_BUFFER_TOO_SMALL;
    if (!found->second.empty()) {
        std::memcpy(output, found->second.data(), found->second.size());
    }
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_set_utf8(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    const ERUI_StringView* value) {
    if (!valid_key(key) || !value ||
        (value->length != 0 && !value->data)) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || found->second.storage != storage) {
        return ERUI_INVALID_HANDLE;
    }
    if (!found->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    found->second.strings[storage_key(*key)] = copy_view(*value);
    found->second.action_inputs.erase(storage_key(*key));
    ++found->second.storage_revision;
    found->second.storage_dirty = true;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_erase(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key) {
    if (!valid_key(key)) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || found->second.storage != storage) {
        return ERUI_INVALID_HANDLE;
    }
    if (!found->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    const std::string composite = storage_key(*key);
    const std::size_t erased = found->second.strings.erase(composite) +
        found->second.action_inputs.erase(composite);
    if (erased == 0) return ERUI_OK;
    ++found->second.storage_revision;
    found->second.storage_dirty = true;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_get_action_inputs(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    ERUI_ActionInputs* out_inputs) {
    if (!valid_key(key) || !out_inputs) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(g_state_mutex);
    const auto found_provider = g_providers.find(provider);
    if (found_provider == g_providers.end() ||
        found_provider->second.storage != storage) return ERUI_INVALID_HANDLE;
    if (!found_provider->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    const auto found = found_provider->second.action_inputs.find(
        storage_key(*key));
    if (found == found_provider->second.action_inputs.end()) {
        return ERUI_NOT_FOUND;
    }
    *out_inputs = found->second;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_set_action_inputs(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    const ERUI_ActionInputs* inputs) {
    if (!valid_key(key) || !inputs || !valid_inputs(*inputs)) {
        return ERUI_INVALID_ARGUMENT;
    }
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || found->second.storage != storage) {
        return ERUI_INVALID_HANDLE;
    }
    if (!found->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    found->second.action_inputs[storage_key(*key)] = *inputs;
    found->second.strings.erase(storage_key(*key));
    ++found->second.storage_revision;
    found->second.storage_dirty = true;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_apply_assignment_changes(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StringView* section,
    const ERUI_AssignmentChange* changes,
    std::uint32_t count) {
    if (!section || !section->data || section->length == 0 ||
        (count != 0 && !changes)) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || found->second.storage != storage) {
        return ERUI_INVALID_HANDLE;
    }
    if (!found->second.storage_loaded) return ERUI_STORAGE_NOT_LOADED;
    for (std::uint32_t index = 0; index < count; ++index) {
        if (changes[index].size < sizeof(changes[index]) ||
            !valid_inputs(changes[index].current) ||
            !changes[index].action_id.data ||
            changes[index].action_id.length == 0) return ERUI_INVALID_ARGUMENT;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        ERUI_StorageKey key{};
        key.size = sizeof(key);
        key.section = *section;
        key.key = changes[index].action_id;
        found->second.action_inputs[storage_key(key)] = changes[index].current;
        ++found->second.storage_revision;
    }
    found->second.storage_dirty = count != 0 || found->second.storage_dirty;
    return ERUI_OK;
}

ERUI_Result ERUI_CALL storage_get_info(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    ERUI_StorageInfo* out_info) {
    if (!out_info) return ERUI_INVALID_ARGUMENT;
    std::lock_guard lock(g_state_mutex);
    const auto found = g_providers.find(provider);
    if (found == g_providers.end() || found->second.storage != storage) {
        return ERUI_INVALID_HANDLE;
    }
    ERUI_StorageInfo result{};
    result.size = sizeof(result);
    result.loaded = found->second.storage_loaded ? 1u : 0u;
    result.dirty = found->second.storage_dirty ? 1u : 0u;
    result.current_revision = found->second.storage_revision;
    result.last_saved_revision = found->second.storage_saved_revision;
    *out_info = result;
    return ERUI_OK;
}

ERUI_Api make_api() noexcept {
    ERUI_Api api{};
    api.size = ERUI_API_V1_1_SIZE;
    api.api_version = ERUI_API_VERSION_1_1;
    api.capabilities = kCapabilities;
    api.register_provider = &register_provider;
    api.add_button = &add_button;
    api.add_toggle = &add_toggle;
    api.add_slider = &add_slider;
    api.add_inline_choice = &add_choice;
    api.add_popup_choice = &add_choice;
    api.add_submenu = &add_submenu;
    api.set_page_presentation = &set_page_presentation;
    api.commit_provider = &commit_provider;
    api.abort_provider = &abort_provider;
    api.set_row_value = &set_row_value;
    api.get_row_value = &get_row_value;
    api.enqueue_alert = &enqueue_alert;
    api.get_game_language = &get_game_language;
    api.add_text_input = &add_text_input;
    api.set_text_input_value = &set_text_input_value;
    api.get_text_input_value = &get_text_input_value;
    api.add_color_picker = &add_color_picker;
    api.set_color_picker_value = &set_color_picker_value;
    api.get_color_picker_value = &get_color_picker_value;
    api.get_builtin_page = &get_builtin_page;
    api.add_input_section = &add_input_section;
    api.add_input_action = &add_input_action;
    api.set_assignments_changed_handler = &set_assignments_changed_handler;
    api.set_action_inputs = &set_action_inputs;
    api.get_action_inputs = &get_action_inputs;
    api.get_action_default_inputs = &get_action_default_inputs;
    api.reset_action_inputs = &reset_action_inputs;
    api.open_storage = &open_storage;
    api.storage_load = &storage_ok;
    api.storage_save = &storage_save;
    api.storage_get_utf8 = &storage_get_utf8;
    api.storage_set_utf8 = &storage_set_utf8;
    api.storage_erase = &storage_erase;
    api.storage_get_action_inputs = &storage_get_action_inputs;
    api.storage_set_action_inputs = &storage_set_action_inputs;
    api.storage_apply_assignment_changes =
        &storage_apply_assignment_changes;
    api.storage_get_info = &storage_get_info;
    return api;
}

} // namespace

extern "C" __declspec(dllexport) ERUI_Result ERUI_CALL
ERUI_TestConfigureConnection(
    const ERUI_TestConnectionConfig* configuration) {
    if (!configuration || configuration->size != sizeof(*configuration) ||
        configuration->version != ERUI_TEST_CONNECTION_CONTROL_VERSION ||
        configuration->reserved[0] != 0 ||
        configuration->reserved[1] != 0 ||
        configuration->reserved[2] != 0) {
        return ERUI_INVALID_ARGUMENT;
    }
    g_not_ready_responses.store(
        configuration->not_ready_responses, std::memory_order_relaxed);
    g_terminal_result.store(
        configuration->terminal_result, std::memory_order_relaxed);
    g_language_result.store(
        configuration->language_result, std::memory_order_relaxed);
    g_get_api_calls.store(0, std::memory_order_relaxed);
    g_language_calls.store(0, std::memory_order_relaxed);
    g_register_calls.store(0, std::memory_order_relaxed);
    g_commit_calls.store(0, std::memory_order_relaxed);
    g_abort_calls.store(0, std::memory_order_relaxed);
    {
        std::lock_guard lock(g_state_mutex);
        g_providers.clear();
        g_actions.clear();
    }
    return ERUI_OK;
}

extern "C" __declspec(dllexport) ERUI_Result ERUI_CALL
ERUI_TestGetConnectionSnapshot(
    ERUI_TestConnectionSnapshot* out_snapshot) {
    if (!out_snapshot || out_snapshot->size < sizeof(*out_snapshot)) {
        return ERUI_INVALID_ARGUMENT;
    }
    ERUI_TestConnectionSnapshot snapshot{};
    snapshot.size = sizeof(snapshot);
    snapshot.version = ERUI_TEST_CONNECTION_CONTROL_VERSION;
    snapshot.get_api_calls = g_get_api_calls.load(std::memory_order_relaxed);
    snapshot.language_calls = g_language_calls.load(std::memory_order_relaxed);
    snapshot.register_calls = g_register_calls.load(std::memory_order_relaxed);
    snapshot.commit_calls = g_commit_calls.load(std::memory_order_relaxed);
    snapshot.abort_calls = g_abort_calls.load(std::memory_order_relaxed);
    *out_snapshot = snapshot;
    return ERUI_OK;
}

extern "C" __declspec(dllexport) ERUI_Result ERUI_CALL
ERUI_TestEmitAssignmentChange(
    ERUI_InputActionHandle action,
    const ERUI_ActionInputs* current,
    ERUI_AssignmentChangeReason reason) {
    if (!current || !valid_inputs(*current) ||
        reason < ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT ||
        reason > ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS) {
        return ERUI_INVALID_ARGUMENT;
    }

    ERUI_AssignmentsChangedCallback callback{};
    void* user_data{};
    ERUI_ProviderHandle provider{};
    ERUI_ActionInputs previous{};
    std::string action_id{};
    {
        std::lock_guard lock(g_state_mutex);
        const auto found_action = g_actions.find(action);
        if (found_action == g_actions.end()) return ERUI_INVALID_HANDLE;
        const auto found_provider = g_providers.find(
            found_action->second.provider);
        if (found_provider == g_providers.end()) return ERUI_INVALID_HANDLE;
        previous = found_action->second.current;
        if (!overlay_inputs(previous, *current, found_action->second.current)) {
            return ERUI_INVALID_ARGUMENT;
        }
        provider = found_action->second.provider;
        action_id = found_action->second.id;
        callback = found_provider->second.assignments_changed;
        user_data = found_provider->second.assignments_user_data;
    }
    if (!callback) return ERUI_NOT_FOUND;

    ERUI_InputDevices changed = ERUI_INPUT_DEVICE_NONE;
    if (std::memcmp(&previous.controller, &current->controller,
            sizeof(previous.controller)) != 0 &&
        current->controller.state != ERUI_INPUT_SLOT_ABSENT) {
        changed |= ERUI_INPUT_DEVICE_CONTROLLER;
    }
    if (std::memcmp(&previous.keyboard, &current->keyboard,
            sizeof(previous.keyboard)) != 0 &&
        current->keyboard.state != ERUI_INPUT_SLOT_ABSENT) {
        changed |= ERUI_INPUT_DEVICE_KEYBOARD;
    }
    if (std::memcmp(&previous.mouse, &current->mouse,
            sizeof(previous.mouse)) != 0 &&
        current->mouse.state != ERUI_INPUT_SLOT_ABSENT) {
        changed |= ERUI_INPUT_DEVICE_MOUSE;
    }
    if (changed == ERUI_INPUT_DEVICE_NONE) return ERUI_INVALID_ARGUMENT;

    ERUI_ActionInputs complete{};
    {
        std::lock_guard lock(g_state_mutex);
        complete = g_actions.at(action).current;
    }
    ERUI_AssignmentChange change{};
    change.size = sizeof(change);
    change.action = action;
    change.action_id = {action_id.data(),
        static_cast<std::uint32_t>(action_id.size()), 0};
    change.previous = previous;
    change.current = complete;
    change.reason = reason;
    change.changed_devices = changed;
    ERUI_AssignmentsChangedContext context{};
    context.size = sizeof(context);
    context.provider = provider;
    context.changes = &change;
    context.change_count = 1;
    callback(user_data, &context);
    return ERUI_OK;
}

extern "C" ERUI_EXPORT ERUI_Result ERUI_CALL ERUI_GetApi(
    uint32_t requested_version,
    ERUI_Api* out_api) {
    if (!out_api) return ERUI_INVALID_ARGUMENT;
    const uint32_t call =
        g_get_api_calls.fetch_add(1, std::memory_order_relaxed) + 1u;
    if (call <= g_not_ready_responses.load(std::memory_order_relaxed)) {
        return ERUI_HOST_NOT_READY;
    }
    const ERUI_Result terminal =
        g_terminal_result.load(std::memory_order_relaxed);
    if (terminal != ERUI_OK) return terminal;
    if (requested_version != ERUI_API_VERSION_1_1) {
        return ERUI_UNSUPPORTED_VERSION;
    }
    if (out_api->size < ERUI_API_V1_1_SIZE) return ERUI_INVALID_ARGUMENT;
    const ERUI_Api api = make_api();
    std::memcpy(out_api, &api, ERUI_API_V1_1_SIZE);
    return ERUI_OK;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
