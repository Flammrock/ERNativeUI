#include "host_registry.hpp"

#include "native_dialog.hpp"
#include "steam_language.hpp"

#include <algorithm>
#include <cstring>

namespace {

ERUI_Result callable_state() noexcept {
    switch (erui::host::api_state()) {
    case erui::host::ApiState::initializing:
        return ERUI_HOST_NOT_READY;
    case erui::host::ApiState::failed:
        return ERUI_HOST_FAILED;
    case erui::host::ApiState::accepting:
    case erui::host::ApiState::runtime_ready:
        return ERUI_OK;
    }
    return ERUI_INTERNAL_ERROR;
}

ERUI_Result register_provider_for_version(
    std::uint32_t negotiated_api_version,
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().register_provider(
        negotiated_api_version, description, out_provider, out_root_page);
}

ERUI_Result ERUI_CALL register_provider_v1_0_entry(
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) {
    return register_provider_for_version(
        ERUI_API_VERSION_1_0, description, out_provider, out_root_page);
}

ERUI_Result ERUI_CALL register_provider_v1_1_entry(
    const ERUI_ProviderDesc* description,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root_page) {
    return register_provider_for_version(
        ERUI_API_VERSION_1_1, description, out_provider, out_root_page);
}

ERUI_Result ERUI_CALL add_button_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ButtonDesc* description,
    ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_button(provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_toggle_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ToggleDesc* description,
    ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_toggle(provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_slider_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_SliderDesc* description,
    ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_slider(provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_inline_choice_entry(
    ERUI_ProviderHandle provider, ERUI_PageHandle page,
    const ERUI_ChoiceDesc* description, ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_inline_choice(
        provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_popup_choice_entry(
    ERUI_ProviderHandle provider, ERUI_PageHandle page,
    const ERUI_ChoiceDesc* description, ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_popup_choice(
        provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_text_input_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_TextInputDesc* description,
    ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_text_input(
        provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_color_picker_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_ColorPickerDesc* description,
    ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_color_picker(
        provider, page, description, out_row);
}

ERUI_Result ERUI_CALL add_submenu_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_SubmenuDesc* description,
    ERUI_PageHandle* out_child,
    ERUI_RowHandle* out_row) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().add_submenu(
        provider, page, description, out_child, out_row);
}

ERUI_Result ERUI_CALL set_page_presentation_entry(
    ERUI_ProviderHandle provider,
    ERUI_PageHandle page,
    const ERUI_PagePresentationDesc* description) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().set_page_presentation(
        provider, page, description);
}

ERUI_Result ERUI_CALL commit_provider_entry(ERUI_ProviderHandle provider) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    const ERUI_Result committed = erui::host::registry().commit_provider(provider);
    if (committed != ERUI_OK) return committed;

    // A successful public commit means the native host became usable, not
    // merely that a declaration entered the startup registry.
    while (erui::host::api_state() == erui::host::ApiState::accepting) {
        Sleep(10);
    }
    return erui::host::api_state() == erui::host::ApiState::runtime_ready
        ? static_cast<ERUI_Result>(ERUI_OK)
        : static_cast<ERUI_Result>(ERUI_HOST_FAILED);
}

ERUI_Result ERUI_CALL abort_provider_entry(ERUI_ProviderHandle provider) {
    const erui::host::ApiState state = erui::host::api_state();
    if (state == erui::host::ApiState::initializing) return ERUI_HOST_NOT_READY;
    return erui::host::registry().abort_provider(provider);
}

ERUI_Result ERUI_CALL set_row_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row,
    std::uint8_t value) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().set_row_value(provider, row, value);
}

ERUI_Result ERUI_CALL get_row_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row,
    std::uint8_t* value) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().get_row_value(provider, row, value);
}

ERUI_Result ERUI_CALL set_text_input_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row,
    const ERUI_Utf16View* value) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().set_text_input_value(
        provider, row, value);
}

ERUI_Result ERUI_CALL get_text_input_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().get_text_input_value(
        provider, row, output, output_capacity, out_length);
}

ERUI_Result ERUI_CALL set_color_picker_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row,
    const ERUI_Color* value) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().set_color_picker_value(
        provider, row, value);
}

ERUI_Result ERUI_CALL get_color_picker_value_entry(
    ERUI_ProviderHandle provider,
    ERUI_RowHandle row,
    ERUI_Color* out_value) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().get_color_picker_value(
        provider, row, out_value);
}

ERUI_Result ERUI_CALL get_builtin_page_entry(
    ERUI_ProviderHandle provider,
    ERUI_BuiltinPage builtin_page,
    ERUI_PageHandle* out_page) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::registry().get_builtin_page(
        provider, builtin_page, out_page);
}

ERUI_Result ERUI_CALL add_input_section_entry(
    ERUI_ProviderHandle provider,
    const ERUI_InputSectionDesc* description,
    ERUI_InputSectionHandle* out_section) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().add_input_section(
        provider, description, out_section);
}

ERUI_Result ERUI_CALL add_input_action_entry(
    ERUI_ProviderHandle provider,
    ERUI_InputSectionHandle section,
    const ERUI_InputActionDesc* description,
    ERUI_InputActionHandle* out_action) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().add_input_action(
        provider, section, description, out_action);
}

ERUI_Result ERUI_CALL set_assignments_changed_handler_entry(
    ERUI_ProviderHandle provider,
    const ERUI_AssignmentsChangedHandlerDesc* description) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().set_assignments_changed_handler(
        provider, description);
}

ERUI_Result ERUI_CALL set_action_inputs_entry(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    const ERUI_ActionInputs* inputs) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().set_action_inputs(
        provider, action, inputs);
}

ERUI_Result ERUI_CALL get_action_inputs_entry(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    ERUI_ActionInputs* out_inputs) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().get_action_inputs(
        provider, action, out_inputs);
}

ERUI_Result ERUI_CALL get_action_default_inputs_entry(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    ERUI_ActionInputs* out_inputs) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().get_action_default_inputs(
        provider, action, out_inputs);
}

ERUI_Result ERUI_CALL reset_action_inputs_entry(
    ERUI_ProviderHandle provider,
    ERUI_InputActionHandle action,
    ERUI_InputDevices devices) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().reset_action_inputs(
        provider, action, devices);
}

ERUI_Result ERUI_CALL open_storage_entry(
    ERUI_ProviderHandle provider,
    const ERUI_StorageDesc* description,
    ERUI_StorageHandle* out_storage) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().open_storage(
        provider, description, out_storage);
}

ERUI_Result ERUI_CALL storage_load_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_load(provider, storage);
}

ERUI_Result ERUI_CALL storage_save_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_save(provider, storage);
}

ERUI_Result ERUI_CALL storage_get_utf8_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    char* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_get_utf8(
        provider, storage, key, output, output_capacity, out_length);
}

ERUI_Result ERUI_CALL storage_set_utf8_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    const ERUI_StringView* value) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_set_utf8(
        provider, storage, key, value);
}

ERUI_Result ERUI_CALL storage_erase_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_erase(provider, storage, key);
}

ERUI_Result ERUI_CALL storage_get_action_inputs_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    ERUI_ActionInputs* out_inputs) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_get_action_inputs(
        provider, storage, key, out_inputs);
}

ERUI_Result ERUI_CALL storage_set_action_inputs_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StorageKey* key,
    const ERUI_ActionInputs* inputs) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_set_action_inputs(
        provider, storage, key, inputs);
}

ERUI_Result ERUI_CALL storage_apply_assignment_changes_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    const ERUI_StringView* section,
    const ERUI_AssignmentChange* changes,
    std::uint32_t change_count) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_apply_assignment_changes(
        provider, storage, section, changes, change_count);
}

ERUI_Result ERUI_CALL storage_get_info_entry(
    ERUI_ProviderHandle provider,
    ERUI_StorageHandle storage,
    ERUI_StorageInfo* out_info) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) {
        return state;
    }
    return erui::host::registry().storage_get_info(
        provider, storage, out_info);
}

ERUI_Result ERUI_CALL enqueue_alert_entry(
    ERUI_ProviderHandle provider,
    const ERUI_AlertDesc* description) {
    switch (erui::host::api_state()) {
    case erui::host::ApiState::initializing:
    case erui::host::ApiState::accepting:
        return ERUI_HOST_NOT_READY;
    case erui::host::ApiState::failed:
        return ERUI_HOST_FAILED;
    case erui::host::ApiState::runtime_ready:
        break;
    }
    if (!erui::native::native_dialog_available()) {
        return ERUI_NOT_SUPPORTED;
    }
    return erui::host::registry().enqueue_alert(provider, description);
}

ERUI_Result ERUI_CALL get_game_language_entry(
    ERUI_GameLanguageInfo* out_language) {
    if (const ERUI_Result state = callable_state(); state != ERUI_OK) return state;
    return erui::host::get_cached_steam_language(out_language);
}

} // namespace

extern "C" ERUI_EXPORT ERUI_Result ERUI_CALL ERUI_GetApi(
    std::uint32_t requested_version,
    ERUI_Api* out_api) {
    if (!out_api) return ERUI_INVALID_ARGUMENT;

    std::uint32_t selected_size{};
    switch (requested_version) {
    case ERUI_API_VERSION_1_0:
        selected_size = ERUI_API_V1_0_SIZE;
        break;
    case ERUI_API_VERSION_1_1:
        selected_size = ERUI_API_V1_1_SIZE;
        break;
    default:
        return ERUI_UNSUPPORTED_VERSION;
    }
    if (out_api->size < selected_size) return ERUI_INVALID_ARGUMENT;

    switch (erui::host::api_state()) {
    case erui::host::ApiState::initializing:
        return ERUI_HOST_NOT_READY;
    case erui::host::ApiState::failed:
        return ERUI_HOST_FAILED;
    case erui::host::ApiState::accepting:
    case erui::host::ApiState::runtime_ready:
        break;
    }

    // API 1.0 was released before explicit connection readiness existed and
    // remains available as soon as provider registration opens. API 1.1 is
    // the first contract whose successful negotiation guarantees that the
    // host has settled Steam language discovery.
    if (requested_version == ERUI_API_VERSION_1_1) {
        switch (erui::host::steam_language_readiness()) {
        case erui::host::SteamLanguageReadiness::pending:
            return ERUI_HOST_NOT_READY;
        case erui::host::SteamLanguageReadiness::unavailable:
        case erui::host::SteamLanguageReadiness::ready:
            break;
        }
    }

    ERUI_Api table{};
    table.size = selected_size;
    table.api_version = requested_version;
    table.capabilities = ERUI_CAP_TOGGLE | ERUI_CAP_SLIDER | ERUI_CAP_BUTTON |
        ERUI_CAP_SUBMENU | ERUI_CAP_PAGINATION | ERUI_CAP_HOST_OWNED_VALUES |
        ERUI_CAP_PAGE_PRESENTATION | ERUI_CAP_ALERT |
        ERUI_CAP_INLINE_CHOICE | ERUI_CAP_POPUP_CHOICE |
        ERUI_CAP_GAME_LANGUAGE;
    table.register_provider = requested_version == ERUI_API_VERSION_1_0
        ? &register_provider_v1_0_entry
        : &register_provider_v1_1_entry;
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
    if (requested_version == ERUI_API_VERSION_1_1) {
        table.capabilities |= ERUI_CAP_TEXT_INPUT | ERUI_CAP_COLOR_PICKER |
            ERUI_CAP_BUILTIN_PAGES | ERUI_CAP_INPUT_BINDINGS |
            ERUI_CAP_STORAGE;
        table.add_text_input = &add_text_input_entry;
        table.set_text_input_value = &set_text_input_value_entry;
        table.get_text_input_value = &get_text_input_value_entry;
        table.add_color_picker = &add_color_picker_entry;
        table.set_color_picker_value = &set_color_picker_value_entry;
        table.get_color_picker_value = &get_color_picker_value_entry;
        table.get_builtin_page = &get_builtin_page_entry;
        table.add_input_section = &add_input_section_entry;
        table.add_input_action = &add_input_action_entry;
        table.set_assignments_changed_handler =
            &set_assignments_changed_handler_entry;
        table.set_action_inputs = &set_action_inputs_entry;
        table.get_action_inputs = &get_action_inputs_entry;
        table.get_action_default_inputs = &get_action_default_inputs_entry;
        table.reset_action_inputs = &reset_action_inputs_entry;
        table.open_storage = &open_storage_entry;
        table.storage_load = &storage_load_entry;
        table.storage_save = &storage_save_entry;
        table.storage_get_utf8 = &storage_get_utf8_entry;
        table.storage_set_utf8 = &storage_set_utf8_entry;
        table.storage_erase = &storage_erase_entry;
        table.storage_get_action_inputs =
            &storage_get_action_inputs_entry;
        table.storage_set_action_inputs =
            &storage_set_action_inputs_entry;
        table.storage_apply_assignment_changes =
            &storage_apply_assignment_changes_entry;
        table.storage_get_info = &storage_get_info_entry;
    }

    std::memcpy(out_api, &table, selected_size);
    return ERUI_OK;
}
