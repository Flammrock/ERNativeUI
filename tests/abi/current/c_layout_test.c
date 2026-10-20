#include <ernativeui/erui.h>

#include <stddef.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ERUI_StringView) == 16u, "ERUI_StringView ABI drift");
_Static_assert(sizeof(ERUI_Utf16View) == 16u, "ERUI_Utf16View ABI drift");
_Static_assert(sizeof(ERUI_ProviderDesc) == 56u, "v1.0 provider prefix drift");
_Static_assert(sizeof(ERUI_TabIconDesc) == 32u,
    "tab-icon descriptor drift");
_Static_assert(offsetof(ERUI_TabIconDesc, pixels) == 16u,
    "tab-icon pixel-pointer offset drift");
_Static_assert(sizeof(ERUI_TabDesc) == 64u, "tab descriptor drift");
_Static_assert(offsetof(ERUI_TabDesc, tab_id) == 8u,
    "tab ID offset drift");
_Static_assert(offsetof(ERUI_TabDesc, title) == 24u,
    "tab title offset drift");
_Static_assert(offsetof(ERUI_TabDesc, preferred_order) == 40u,
    "tab preferred-order offset drift");
_Static_assert(offsetof(ERUI_TabDesc, reserved0) == 44u,
    "tab reserved0 offset drift");
_Static_assert(offsetof(ERUI_TabDesc, icon) == 48u,
    "tab icon offset drift");
_Static_assert(offsetof(ERUI_TabDesc, reserved) == 56u,
    "tab reserved offset drift");
_Static_assert(ERUI_CUSTOM_TAB_MAX_COUNT == 1024u,
    "custom-tab count limit drift");
_Static_assert(ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS == 95u,
    "custom-tab title limit drift");
_Static_assert(ERUI_TAB_LIMIT_REACHED == 22u,
    "custom-tab limit result drift");
_Static_assert(ERUI_TAB_ICON_FORMAT_RGBA8 == 1u,
    "RGBA8 tab-icon format drift");
_Static_assert(sizeof(ERUI_ButtonDesc) == 64u, "v1.0 button prefix drift");
_Static_assert(sizeof(ERUI_ToggleDesc) == 64u, "v1.0 toggle prefix drift");
_Static_assert(sizeof(ERUI_SliderDesc) == 80u, "v1.0 slider prefix drift");
_Static_assert(sizeof(ERUI_ChoiceDesc) == 72u, "v1.0 choice prefix drift");
_Static_assert(sizeof(ERUI_SubmenuDesc) == 80u, "v1.0 submenu prefix drift");
_Static_assert(sizeof(ERUI_PageTitleFormatContext) == 56u,
    "v1.0 page-title context drift");
_Static_assert(sizeof(ERUI_PagePresentationDesc) == 64u,
    "v1.0 page-presentation prefix drift");
_Static_assert(sizeof(ERUI_AlertDesc) == 48u, "v1.0 alert prefix drift");
_Static_assert(ERUI_API_V1_0_SIZE == 128u, "v1.0 table prefix drift");
_Static_assert(ERUI_STORAGE_MAX_IDENTIFIER_BYTES == 255u,
    "storage identifier limit drift");
_Static_assert(ERUI_STORAGE_MAX_SECTION_BYTES == 255u,
    "storage section limit drift");
_Static_assert(ERUI_STORAGE_MAX_KEY_BYTES == 255u,
    "storage key limit drift");
_Static_assert(ERUI_STORAGE_MAX_VALUE_BYTES == 65536u,
    "storage value limit drift");
_Static_assert(ERUI_STORAGE_MAX_FILE_BYTES == 1048576u,
    "storage file limit drift");
_Static_assert(ERUI_STORAGE_MAX_DECODED_BYTES == 262144u,
    "storage decoded-data limit drift");
_Static_assert(ERUI_STORAGE_MAX_SECTIONS == 1024u,
    "storage section-count limit drift");
_Static_assert(ERUI_STORAGE_MAX_ENTRIES == 4096u,
    "storage entry-count limit drift");
_Static_assert(ERUI_STORAGE_MAX_ASSIGNMENT_CHANGES == 4096u,
    "storage assignment-batch limit drift");
_Static_assert(ERUI_ACTION_INPUTS_TEXT_MAX_BYTES == 67u,
    "ActionInputs text-codec bound drift");
_Static_assert(offsetof(ERUI_Api, register_provider) == 16u,
    "v1.0 first function moved");
_Static_assert(offsetof(ERUI_Api, get_game_language) == 120u,
    "v1.0 final function moved");

#if defined(ERUI_API_VERSION_1_1)
#if defined(ERUI_API_VERSION_1_3)
_Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_3,
    "unexpected current API version");
#elif defined(ERUI_API_VERSION_1_2)
_Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_2,
    "unexpected current API version");
#else
_Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_1,
    "unexpected current API version");
#endif
_Static_assert(ERUI_BUFFER_TOO_SMALL == 14u, "result values were renumbered");
_Static_assert(ERUI_CAP_TEXT_INPUT == (UINT64_C(1) << 11),
    "TextInput capability bit drift");
_Static_assert(ERUI_CAP_COLOR_PICKER == (UINT64_C(1) << 12),
    "ColorPicker capability bit drift");
_Static_assert(ERUI_CAP_BUILTIN_PAGES == (UINT64_C(1) << 13),
    "built-in pages capability bit drift");
_Static_assert(ERUI_CAP_INPUT_BINDINGS == (UINT64_C(1) << 14),
    "input bindings capability bit drift");
_Static_assert(ERUI_DUPLICATE_ACTION_ID == 15u,
    "input action result value drift");
_Static_assert(ERUI_NOT_FOUND == 16u && ERUI_STORAGE_FORMAT_ERROR == 20u,
    "storage result values drift");
_Static_assert(ERUI_CAP_STORAGE == (UINT64_C(1) << 15),
    "storage capability bit drift");
_Static_assert(sizeof(ERUI_InputSectionHandle) == 8u,
    "input-section handle ABI drift");
_Static_assert(sizeof(ERUI_InputActionHandle) == 8u,
    "input-action handle ABI drift");
_Static_assert(sizeof(ERUI_StorageHandle) == 8u,
    "storage handle ABI drift");
_Static_assert(ERUI_INPUT_DEVICE_CONTROLLER == (1u << 0),
    "controller input-device bit drift");
_Static_assert(ERUI_INPUT_DEVICE_KEYBOARD == (1u << 1),
    "keyboard input-device bit drift");
_Static_assert(ERUI_INPUT_DEVICE_MOUSE == (1u << 2),
    "mouse input-device bit drift");
_Static_assert(ERUI_INPUT_DEVICE_ALL == 7u,
    "all input-device bits drift");
_Static_assert(ERUI_CONTROLLER_BUTTON_DPAD_UP == 1u &&
    ERUI_CONTROLLER_BUTTON_RIGHT_STICK == 14u &&
    ERUI_CONTROLLER_BUTTON_COUNT == 15u,
    "controller semantic table drift");
_Static_assert(ERUI_KEYBOARD_KEY_DIGIT_1 == 1u &&
    ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE == 72u &&
    ERUI_KEYBOARD_KEY_COUNT == 73u,
    "keyboard semantic table drift");
_Static_assert(ERUI_MOUSE_BUTTON_LEFT == 1u &&
    ERUI_MOUSE_BUTTON_WHEEL_DOWN == 7u &&
    ERUI_MOUSE_BUTTON_COUNT == 8u,
    "mouse semantic table drift");
_Static_assert(sizeof(ERUI_ControllerInput) == 8u,
    "controller input slot drift");
_Static_assert(sizeof(ERUI_KeyboardInput) == 8u,
    "keyboard input slot drift");
_Static_assert(sizeof(ERUI_MouseInput) == 8u,
    "mouse input slot drift");
_Static_assert(sizeof(ERUI_ActionInputs) == 40u,
    "action-input value drift");
_Static_assert(offsetof(ERUI_ActionInputs, controller) == 8u,
    "action-input controller offset drift");
_Static_assert(offsetof(ERUI_ActionInputs, keyboard) == 16u,
    "action-input keyboard offset drift");
_Static_assert(offsetof(ERUI_ActionInputs, mouse) == 24u,
    "action-input mouse offset drift");
_Static_assert(ERUI_BUILTIN_PAGE_GAME_OPTIONS == 0u,
    "Game Options built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_CAMERA_OPTIONS == 1u,
    "Camera Options built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_DISPLAY == 2u,
    "Display built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_SOUND == 3u,
    "Sound built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_NETWORK == 4u,
    "Network built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE == 5u,
    "Keyboard/Mouse built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_GRAPHICS == 6u,
    "Graphics built-in page value drift");
_Static_assert(ERUI_BUILTIN_PAGE_COUNT == 7u,
    "built-in page count drift");
_Static_assert(sizeof(ERUI_TextInputChangeContext) == 48u,
    "TextInput callback context ABI drift");
_Static_assert(offsetof(ERUI_TextInputChangeContext, value) == 24u,
    "TextInput callback value offset drift");
_Static_assert(sizeof(ERUI_TextInputDesc) == 96u,
    "TextInput descriptor ABI drift");
_Static_assert(offsetof(ERUI_TextInputDesc, changed_callback) == 72u,
    "TextInput callback offset drift");
_Static_assert(offsetof(ERUI_TextInputDesc, maximum_length) == 88u,
    "TextInput maximum offset drift");
_Static_assert(offsetof(ERUI_TextInputDesc, reserved) == 92u,
    "TextInput reserved offset drift");
_Static_assert(offsetof(ERUI_Api, add_text_input) == 128u,
    "TextInput add function is not append-only");
_Static_assert(offsetof(ERUI_Api, set_text_input_value) == 136u,
    "TextInput setter offset drift");
_Static_assert(offsetof(ERUI_Api, get_text_input_value) == 144u,
    "TextInput getter offset drift");
_Static_assert(sizeof(ERUI_Color) == 4u,
    "Color channel structure ABI drift");
_Static_assert(offsetof(ERUI_Color, reserved) == 3u,
    "Color reserved-channel offset drift");
_Static_assert(sizeof(ERUI_ColorPickerChangeContext) == 32u,
    "ColorPicker callback context ABI drift");
_Static_assert(offsetof(ERUI_ColorPickerChangeContext, value) == 24u,
    "ColorPicker callback value offset drift");
_Static_assert(offsetof(ERUI_ColorPickerChangeContext, reserved) == 28u,
    "ColorPicker callback reserved offset drift");
_Static_assert(sizeof(ERUI_ColorPickerDesc) == 64u,
    "ColorPicker descriptor ABI drift");
_Static_assert(offsetof(ERUI_ColorPickerDesc, changed_callback) == 40u,
    "ColorPicker callback offset drift");
_Static_assert(offsetof(ERUI_ColorPickerDesc, initial_value) == 56u,
    "ColorPicker initial-value offset drift");
_Static_assert(offsetof(ERUI_ColorPickerDesc, enabled) == 60u,
    "ColorPicker enabled offset drift");
_Static_assert(offsetof(ERUI_Api, add_color_picker) == 152u,
    "ColorPicker add function is not append-only");
_Static_assert(offsetof(ERUI_Api, set_color_picker_value) == 160u,
    "ColorPicker setter offset drift");
_Static_assert(offsetof(ERUI_Api, get_color_picker_value) == 168u,
    "ColorPicker getter offset drift");
_Static_assert(offsetof(ERUI_Api, get_builtin_page) == 176u,
    "built-in page accessor is not append-only");
_Static_assert(sizeof(ERUI_InputActionActivatedContext) == 32u,
    "action callback context ABI drift");
_Static_assert(offsetof(ERUI_InputActionActivatedContext, action) == 16u,
    "action callback handle offset drift");
_Static_assert(sizeof(ERUI_AssignmentChange) == 128u,
    "assignment-change ABI drift");
_Static_assert(offsetof(ERUI_AssignmentChange, action) == 8u,
    "assignment action offset drift");
_Static_assert(offsetof(ERUI_AssignmentChange, action_id) == 16u,
    "assignment action-ID offset drift");
_Static_assert(offsetof(ERUI_AssignmentChange, previous) == 32u,
    "assignment previous snapshot offset drift");
_Static_assert(offsetof(ERUI_AssignmentChange, current) == 72u,
    "assignment current snapshot offset drift");
_Static_assert(offsetof(ERUI_AssignmentChange, reason) == 112u,
    "assignment reason offset drift");
_Static_assert(offsetof(ERUI_AssignmentChange, changed_devices) == 116u,
    "assignment changed-device offset drift");
_Static_assert(offsetof(ERUI_AssignmentChange, reserved) == 120u,
    "assignment reserved offset drift");
_Static_assert(sizeof(ERUI_AssignmentsChangedContext) == 32u,
    "assignments event ABI drift");
_Static_assert(offsetof(ERUI_AssignmentsChangedContext, provider) == 8u,
    "assignments event provider offset drift");
_Static_assert(offsetof(ERUI_AssignmentsChangedContext, changes) == 16u,
    "assignments event change pointer offset drift");
_Static_assert(offsetof(ERUI_AssignmentsChangedContext, change_count) == 24u,
    "assignments event count offset drift");
_Static_assert(sizeof(ERUI_AssignmentsChangedHandlerDesc) == 32u,
    "assignments handler descriptor drift");
_Static_assert(offsetof(ERUI_AssignmentsChangedHandlerDesc, callback) == 8u,
    "assignments callback offset drift");
_Static_assert(offsetof(ERUI_AssignmentsChangedHandlerDesc, user_data) == 16u,
    "assignments callback state offset drift");
_Static_assert(sizeof(ERUI_InputSectionDesc) == 32u,
    "input-section descriptor ABI drift");
_Static_assert(offsetof(ERUI_InputSectionDesc, label) == 8u,
    "input-section label offset drift");
_Static_assert(offsetof(ERUI_InputSectionDesc, reserved) == 24u,
    "input-section reserved offset drift");
_Static_assert(sizeof(ERUI_InputActionDesc) == 104u,
    "input-action descriptor ABI drift");
_Static_assert(offsetof(ERUI_InputActionDesc, action_id) == 8u,
    "input-action ID offset drift");
_Static_assert(offsetof(ERUI_InputActionDesc, label) == 24u,
    "input-action label offset drift");
_Static_assert(offsetof(ERUI_InputActionDesc, default_inputs) == 40u,
    "input-action defaults offset drift");
_Static_assert(offsetof(ERUI_InputActionDesc, activated_callback) == 80u,
    "input-action callback offset drift");
_Static_assert(offsetof(ERUI_InputActionDesc, user_data) == 88u,
    "input-action callback state offset drift");
_Static_assert(offsetof(ERUI_InputActionDesc, reserved) == 96u,
    "input-action reserved offset drift");
_Static_assert(sizeof(ERUI_StorageDesc) == 40u,
    "storage descriptor ABI drift");
_Static_assert(offsetof(ERUI_StorageDesc, location) == 8u,
    "storage location offset drift");
_Static_assert(offsetof(ERUI_StorageDesc, path) == 16u,
    "storage path offset drift");
_Static_assert(offsetof(ERUI_StorageDesc, reserved) == 32u,
    "storage descriptor reserved offset drift");
_Static_assert(sizeof(ERUI_StorageKey) == 48u,
    "storage key ABI drift");
_Static_assert(offsetof(ERUI_StorageKey, section) == 8u,
    "storage section offset drift");
_Static_assert(offsetof(ERUI_StorageKey, key) == 24u,
    "storage key-name offset drift");
_Static_assert(offsetof(ERUI_StorageKey, reserved) == 40u,
    "storage key reserved offset drift");
_Static_assert(sizeof(ERUI_StorageInfo) == 40u,
    "storage info ABI drift");
_Static_assert(offsetof(ERUI_StorageInfo, loaded) == 8u,
    "storage loaded offset drift");
_Static_assert(offsetof(ERUI_StorageInfo, current_revision) == 16u,
    "storage current-revision offset drift");
_Static_assert(offsetof(ERUI_StorageInfo, last_saved_revision) == 24u,
    "storage saved-revision offset drift");
_Static_assert(offsetof(ERUI_StorageInfo, reserved) == 32u,
    "storage info reserved offset drift");
_Static_assert(offsetof(ERUI_Api, add_input_section) == 184u,
    "input-section function is not append-only");
_Static_assert(offsetof(ERUI_Api, add_input_action) == 192u,
    "input-action function offset drift");
_Static_assert(offsetof(ERUI_Api, set_assignments_changed_handler) == 200u,
    "assignment-handler function offset drift");
_Static_assert(offsetof(ERUI_Api, set_action_inputs) == 208u,
    "action-input setter offset drift");
_Static_assert(offsetof(ERUI_Api, get_action_inputs) == 216u,
    "action-input getter offset drift");
_Static_assert(offsetof(ERUI_Api, get_action_default_inputs) == 224u,
    "action-default getter offset drift");
_Static_assert(offsetof(ERUI_Api, reset_action_inputs) == 232u,
    "action reset offset drift");
_Static_assert(offsetof(ERUI_Api, open_storage) == 240u,
    "storage open offset drift");
_Static_assert(offsetof(ERUI_Api, storage_load) == 248u,
    "storage load offset drift");
_Static_assert(offsetof(ERUI_Api, storage_save) == 256u,
    "storage save offset drift");
_Static_assert(offsetof(ERUI_Api, storage_get_utf8) == 264u,
    "storage string getter offset drift");
_Static_assert(offsetof(ERUI_Api, storage_set_utf8) == 272u,
    "storage string setter offset drift");
_Static_assert(offsetof(ERUI_Api, storage_erase) == 280u,
    "storage erase offset drift");
_Static_assert(offsetof(ERUI_Api, storage_get_action_inputs) == 288u,
    "storage action-input getter offset drift");
_Static_assert(offsetof(ERUI_Api, storage_set_action_inputs) == 296u,
    "storage action-input setter offset drift");
_Static_assert(offsetof(ERUI_Api, storage_apply_assignment_changes) == 304u,
    "storage assignment apply offset drift");
_Static_assert(offsetof(ERUI_Api, storage_get_info) == 312u,
    "storage info offset drift");
_Static_assert(ERUI_API_V1_1_SIZE >= 152u,
    "v1.1 prefix does not contain the TextInput block");
_Static_assert(ERUI_API_V1_1_SIZE == 320u,
    "v1.1 prefix does not contain the complete input/storage blocks");
_Static_assert(sizeof(ERUI_Api) >= ERUI_API_V1_1_SIZE,
    "current API structure is smaller than its selected prefix");
#if defined(ERUI_API_VERSION_1_2)
_Static_assert(ERUI_CAP_NUMERIC_SLIDER == (UINT64_C(1) << 16),
    "numeric-slider capability bit drift");
_Static_assert(ERUI_CAP_CUSTOM_SLIDER == (UINT64_C(1) << 17),
    "custom-slider capability bit drift");
_Static_assert(ERUI_SLIDER_MIN_VALUE_COUNT == 2u &&
    ERUI_SLIDER_MAX_VALUE_COUNT == 256u,
    "logical slider count limits drift");
_Static_assert(ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES == 9u,
    "numeric-slider precision limit drift");
_Static_assert(ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH == 64u,
    "custom-slider text limit drift");
_Static_assert(sizeof(ERUI_DecimalValue) == 16u,
    "decimal value ABI drift");
_Static_assert(offsetof(ERUI_DecimalValue, scaled_value) == 0u,
    "decimal scaled-value offset drift");
_Static_assert(offsetof(ERUI_DecimalValue, decimal_places) == 8u,
    "decimal precision offset drift");
_Static_assert(offsetof(ERUI_DecimalValue, reserved) == 12u,
    "decimal reserved offset drift");
_Static_assert(sizeof(ERUI_NumericSliderChangeContext) == 56u,
    "numeric-slider callback context ABI drift");
_Static_assert(offsetof(ERUI_NumericSliderChangeContext, value) == 24u,
    "numeric-slider callback value offset drift");
_Static_assert(offsetof(ERUI_NumericSliderChangeContext, index) == 40u,
    "numeric-slider callback index offset drift");
_Static_assert(offsetof(ERUI_NumericSliderChangeContext, value_count) == 44u,
    "numeric-slider callback count offset drift");
_Static_assert(sizeof(ERUI_CustomSliderChangeContext) == 40u,
    "custom-slider callback context ABI drift");
_Static_assert(offsetof(ERUI_CustomSliderChangeContext, index) == 24u,
    "custom-slider callback index offset drift");
_Static_assert(offsetof(ERUI_CustomSliderChangeContext, value_count) == 28u,
    "custom-slider callback count offset drift");
_Static_assert(sizeof(ERUI_NumericSliderDesc) == 136u,
    "numeric-slider descriptor ABI drift");
_Static_assert(offsetof(ERUI_NumericSliderDesc, minimum_scaled_value) == 56u,
    "numeric-slider minimum offset drift");
_Static_assert(offsetof(ERUI_NumericSliderDesc, decimal_places) == 88u,
    "numeric-slider precision offset drift");
_Static_assert(offsetof(ERUI_NumericSliderDesc, prefix) == 96u,
    "numeric-slider prefix offset drift");
_Static_assert(offsetof(ERUI_NumericSliderDesc, suffix) == 112u,
    "numeric-slider suffix offset drift");
_Static_assert(offsetof(ERUI_NumericSliderDesc, reserved) == 128u,
    "numeric-slider reserved offset drift");
_Static_assert(sizeof(ERUI_CustomSliderDesc) == 80u,
    "custom-slider descriptor ABI drift");
_Static_assert(offsetof(ERUI_CustomSliderDesc, values) == 56u,
    "custom-slider values offset drift");
_Static_assert(offsetof(ERUI_CustomSliderDesc, value_count) == 64u,
    "custom-slider count offset drift");
_Static_assert(offsetof(ERUI_CustomSliderDesc, initial_index) == 68u,
    "custom-slider initial-index offset drift");
_Static_assert(offsetof(ERUI_Api, add_numeric_slider) == 320u,
    "numeric-slider block is not append-only");
_Static_assert(offsetof(ERUI_Api, set_numeric_slider_value) == 328u,
    "numeric-slider setter offset drift");
_Static_assert(offsetof(ERUI_Api, get_numeric_slider_value) == 336u,
    "numeric-slider getter offset drift");
_Static_assert(offsetof(ERUI_Api, add_custom_slider) == 344u,
    "custom-slider block offset drift");
_Static_assert(offsetof(ERUI_Api, set_custom_slider_index) == 352u,
    "custom-slider setter offset drift");
_Static_assert(offsetof(ERUI_Api, get_custom_slider_index) == 360u,
    "custom-slider getter offset drift");
_Static_assert(ERUI_API_V1_2_SIZE == 368u,
    "API 1.2 prefix size drift");
_Static_assert(offsetof(ERUI_Api, add_tab) == 368u,
    "custom-tab block is not append-only");
_Static_assert(offsetof(ERUI_Api, add_title_screen_text) == 376u,
    "title-screen text block is not append-only");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED == 23u,
    "title-screen text limit result drift");
_Static_assert(ERUI_CAP_TITLE_SCREEN_TEXT == (UINT64_C(1) << 19),
    "title-screen text capability bit drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR == (1u << 0),
    "title-screen text custom-color flag drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS == 255u,
    "title-screen text length limit drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_MAX_LINES == 16u,
    "title-screen text line limit drift");
_Static_assert(sizeof(ERUI_TitleScreenTextDesc) == 40u,
    "title-screen text descriptor size drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, flags) == 4u,
    "title-screen text flags offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, text) == 8u,
    "title-screen text view offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, color) == 24u,
    "title-screen text color offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, corner) == 28u,
    "title-screen text corner offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, reserved) == 32u,
    "title-screen text reserved offset drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_TOP_LEFT == 1u,
    "top-left title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT == 2u,
    "top-right title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT == 3u,
    "bottom-left title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT == 4u,
    "bottom-right title-screen corner drift");
_Static_assert(ERUI_API_V1_3_SIZE == 384u,
    "API 1.3 prefix size drift");
_Static_assert(ERUI_API_CURRENT_SIZE == ERUI_API_V1_3_SIZE,
    "current API size does not select API 1.3");
_Static_assert(sizeof(ERUI_Api) == ERUI_API_V1_3_SIZE,
    "current API structure has an unexpected tail");
#endif
#else
_Static_assert(sizeof(ERUI_Api) == 128u,
    "the pre-v1.1 current table unexpectedly changed");
#endif
#endif

int main(void) {
    ERUI_Api api = {0};
    api.size = (uint32_t)sizeof(api);
    return api.size < ERUI_API_V1_0_SIZE;
}
