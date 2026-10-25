#include <ernativeui/erui.h>

#include <stddef.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ERUI_StringView) == 16u, "ERUI_StringView ABI drift");
_Static_assert(sizeof(ERUI_Utf16View) == 16u, "ERUI_Utf16View ABI drift");
_Static_assert(sizeof(ERUI_GameLanguageInfo) == 32u,
    "ERUI_GameLanguageInfo ABI drift");
_Static_assert(sizeof(ERUI_ProviderDesc) == 56u, "ERUI_ProviderDesc ABI drift");
_Static_assert(sizeof(ERUI_TabIconDesc) == 32u,
    "ERUI_TabIconDesc ABI drift");
_Static_assert(sizeof(ERUI_TabDesc) == 64u, "ERUI_TabDesc ABI drift");
_Static_assert(sizeof(ERUI_ButtonDesc) == 64u, "ERUI_ButtonDesc ABI drift");
_Static_assert(sizeof(ERUI_ToggleDesc) == 64u, "ERUI_ToggleDesc ABI drift");
_Static_assert(sizeof(ERUI_SliderDesc) == 80u, "ERUI_SliderDesc ABI drift");
_Static_assert(sizeof(ERUI_ChoiceDesc) == 72u, "ERUI_ChoiceDesc ABI drift");
_Static_assert(sizeof(ERUI_TextInputChangeContext) == 48u,
    "ERUI_TextInputChangeContext ABI drift");
_Static_assert(sizeof(ERUI_TextInputDesc) == 96u,
    "ERUI_TextInputDesc ABI drift");
_Static_assert(sizeof(ERUI_Color) == 4u,
    "ERUI_Color ABI drift");
_Static_assert(sizeof(ERUI_ColorPickerChangeContext) == 32u,
    "ERUI_ColorPickerChangeContext ABI drift");
_Static_assert(sizeof(ERUI_ColorPickerDesc) == 64u,
    "ERUI_ColorPickerDesc ABI drift");
_Static_assert(sizeof(ERUI_DecimalValue) == 16u,
    "ERUI_DecimalValue ABI drift");
_Static_assert(sizeof(ERUI_NumericSliderChangeContext) == 56u,
    "ERUI_NumericSliderChangeContext ABI drift");
_Static_assert(sizeof(ERUI_CustomSliderChangeContext) == 40u,
    "ERUI_CustomSliderChangeContext ABI drift");
_Static_assert(sizeof(ERUI_NumericSliderDesc) == 136u,
    "ERUI_NumericSliderDesc ABI drift");
_Static_assert(sizeof(ERUI_CustomSliderDesc) == 80u,
    "ERUI_CustomSliderDesc ABI drift");
_Static_assert(sizeof(ERUI_ActionInputs) == 40u,
    "ERUI_ActionInputs ABI drift");
_Static_assert(sizeof(ERUI_InputActionActivatedContext) == 32u,
    "ERUI_InputActionActivatedContext ABI drift");
_Static_assert(sizeof(ERUI_InputSectionDesc) == 32u,
    "ERUI_InputSectionDesc ABI drift");
_Static_assert(sizeof(ERUI_InputActionDesc) == 104u,
    "ERUI_InputActionDesc ABI drift");
_Static_assert(sizeof(ERUI_AssignmentChange) == 128u,
    "ERUI_AssignmentChange ABI drift");
_Static_assert(sizeof(ERUI_AssignmentsChangedContext) == 32u,
    "ERUI_AssignmentsChangedContext ABI drift");
_Static_assert(sizeof(ERUI_StorageDesc) == 40u,
    "ERUI_StorageDesc ABI drift");
_Static_assert(sizeof(ERUI_StorageKey) == 48u,
    "ERUI_StorageKey ABI drift");
_Static_assert(sizeof(ERUI_StorageInfo) == 40u,
    "ERUI_StorageInfo ABI drift");
_Static_assert(sizeof(ERUI_SubmenuDesc) == 80u, "ERUI_SubmenuDesc ABI drift");
_Static_assert(sizeof(ERUI_PageTitleFormatContext) == 56u,
    "ERUI_PageTitleFormatContext ABI drift");
_Static_assert(sizeof(ERUI_PagePresentationDesc) == 64u,
    "ERUI_PagePresentationDesc ABI drift");
_Static_assert(sizeof(ERUI_AlertDesc) == 48u, "ERUI_AlertDesc ABI drift");
_Static_assert(sizeof(ERUI_Api) == 392u, "ERUI_Api ABI drift");
_Static_assert(_Alignof(ERUI_Api) == 8u, "ERUI_Api alignment drift");
_Static_assert(offsetof(ERUI_ProviderDesc, owner_module) == 16u,
    "ERUI_ProviderDesc offset drift");
_Static_assert(offsetof(ERUI_Api, capabilities) == 8u,
    "ERUI_Api capabilities offset drift");
_Static_assert(offsetof(ERUI_Api, register_provider) == 16u,
    "ERUI_Api first function offset drift");
_Static_assert(offsetof(ERUI_PageTitleFormatContext, base_title) == 24u,
    "ERUI_PageTitleFormatContext base-title offset drift");
_Static_assert(offsetof(ERUI_PagePresentationDesc, formatter) == 40u,
    "ERUI_PagePresentationDesc formatter offset drift");
_Static_assert(offsetof(ERUI_Api, add_inline_choice) == 48u,
    "ERUI_Api inline-choice function offset drift");
_Static_assert(offsetof(ERUI_Api, add_popup_choice) == 56u,
    "ERUI_Api popup-choice function offset drift");
_Static_assert(offsetof(ERUI_Api, set_page_presentation) == 72u,
    "ERUI_Api page-presentation function offset drift");
_Static_assert(offsetof(ERUI_Api, get_row_value) == 104u,
    "ERUI_Api get-row-value offset drift");
_Static_assert(offsetof(ERUI_AlertDesc, callback) == 24u,
    "ERUI_AlertDesc callback offset drift");
_Static_assert(offsetof(ERUI_AlertDesc, buttons) == 40u,
    "ERUI_AlertDesc buttons offset drift");
_Static_assert(offsetof(ERUI_AlertDesc, placement) == 44u,
    "ERUI_AlertDesc placement offset drift");
_Static_assert(offsetof(ERUI_Api, enqueue_alert) == 112u,
    "ERUI_Api alert function offset drift");
_Static_assert(offsetof(ERUI_Api, get_game_language) == 120u,
    "ERUI_Api language function offset drift");
_Static_assert(ERUI_API_V1_0_SIZE == 128u, "ERUI API 1.0 prefix drift");
_Static_assert(offsetof(ERUI_Api, add_color_picker) == 152u,
    "ERUI_Api ColorPicker add offset drift");
_Static_assert(offsetof(ERUI_Api, set_color_picker_value) == 160u,
    "ERUI_Api ColorPicker setter offset drift");
_Static_assert(offsetof(ERUI_Api, get_color_picker_value) == 168u,
    "ERUI_Api ColorPicker getter offset drift");
_Static_assert(offsetof(ERUI_Api, get_builtin_page) == 176u,
    "ERUI_Api built-in page accessor offset drift");
_Static_assert(offsetof(ERUI_Api, add_input_section) == 184u,
    "ERUI_Api input-section offset drift");
_Static_assert(offsetof(ERUI_Api, add_input_action) == 192u,
    "ERUI_Api input-action offset drift");
_Static_assert(offsetof(ERUI_Api, reset_action_inputs) == 232u,
    "ERUI_Api input-action block drift");
_Static_assert(offsetof(ERUI_Api, open_storage) == 240u,
    "ERUI_Api storage block drift");
_Static_assert(offsetof(ERUI_Api, storage_get_info) == 312u,
    "ERUI_Api final storage offset drift");
_Static_assert(ERUI_API_V1_1_SIZE == 320u, "ERUI API 1.1 prefix drift");
_Static_assert(offsetof(ERUI_Api, add_numeric_slider) == 320u,
    "ERUI_Api numeric-slider block drift");
_Static_assert(offsetof(ERUI_Api, add_custom_slider) == 344u,
    "ERUI_Api custom-slider block drift");
_Static_assert(offsetof(ERUI_Api, get_custom_slider_index) == 360u,
    "ERUI_Api final custom-slider offset drift");
_Static_assert(ERUI_API_V1_2_SIZE == 368u, "ERUI API 1.2 size drift");
_Static_assert(offsetof(ERUI_Api, add_tab) == 368u,
    "ERUI_Api tab block drift");
_Static_assert(offsetof(ERUI_Api, add_title_screen_text) == 376u,
    "ERUI_Api title-screen text block drift");
_Static_assert(ERUI_API_V1_3_SIZE == 384u, "ERUI API 1.3 size drift");
_Static_assert(offsetof(ERUI_Api, commit_updates) == 384u,
    "ERUI_Api update block drift");
_Static_assert(ERUI_API_V1_4_SIZE == 392u, "ERUI API 1.4 size drift");
_Static_assert(ERUI_API_CURRENT_SIZE == ERUI_API_V1_4_SIZE,
    "ERUI current API does not select 1.4");
_Static_assert(sizeof(ERUI_TextStyleSpan) == 32u,
    "ERUI text-style span ABI drift");
_Static_assert(ERUI_STYLED_TEXT_MAX_CODE_UNITS == 4096u,
    "ERUI styled-text length limit drift");
_Static_assert(ERUI_STYLED_TEXT_MAX_SPANS == 256u,
    "ERUI styled-text span limit drift");
_Static_assert(sizeof(ERUI_StyledTextView) == 48u,
    "ERUI styled-text view ABI drift");
_Static_assert(ERUI_ROW_UPDATE_FIELD_CAPTION == (1u << 2),
    "ERUI row-caption field bit drift");
_Static_assert(ERUI_ROW_UPDATE_FIELD_INTERACTIVE == (1u << 3),
    "ERUI row-interactive field bit drift");
_Static_assert(ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE == (1u << 4),
    "ERUI row-appearance field bit drift");
_Static_assert(ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY == 0u,
    "ERUI follow-interactivity appearance drift");
_Static_assert(ERUI_AVAILABILITY_APPEARANCE_NORMAL == 1u,
    "ERUI normal appearance drift");
_Static_assert(ERUI_AVAILABILITY_APPEARANCE_DISABLED == 2u,
    "ERUI disabled appearance drift");
_Static_assert(sizeof(ERUI_RowUpdateDesc) == 176u,
    "ERUI row-update descriptor ABI drift");
_Static_assert(offsetof(ERUI_RowUpdateDesc, interactive) == 20u,
    "ERUI row-update interactive offset drift");
_Static_assert(offsetof(ERUI_RowUpdateDesc, availability_appearance) == 168u,
    "ERUI row-update appearance offset drift");
_Static_assert(offsetof(ERUI_RowUpdateDesc, reserved) == 172u,
    "ERUI row-update reserved offset drift");
_Static_assert(sizeof(ERUI_UpdateDesc) == 24u,
    "ERUI update descriptor ABI drift");
_Static_assert(sizeof(ERUI_UpdateBatchDesc) == 32u,
    "ERUI update-batch descriptor ABI drift");
_Static_assert(sizeof(ERUI_TitleScreenTextDesc) == 40u,
    "ERUI title-screen text descriptor ABI drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, flags) == 4u,
    "ERUI title-screen text flags offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, text) == 8u,
    "ERUI title-screen text view offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, color) == 24u,
    "ERUI title-screen text color offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, corner) == 28u,
    "ERUI title-screen text corner offset drift");
_Static_assert(offsetof(ERUI_TitleScreenTextDesc, reserved) == 32u,
    "ERUI title-screen text reserved offset drift");
_Static_assert(offsetof(ERUI_TabIconDesc, pixels) == 16u,
    "ERUI tab-icon pixels offset drift");
_Static_assert(offsetof(ERUI_TabDesc, tab_id) == 8u,
    "ERUI tab ID offset drift");
_Static_assert(offsetof(ERUI_TabDesc, title) == 24u,
    "ERUI tab title offset drift");
_Static_assert(offsetof(ERUI_TabDesc, preferred_order) == 40u,
    "ERUI tab order offset drift");
_Static_assert(offsetof(ERUI_TabDesc, reserved0) == 44u,
    "ERUI tab reserved0 offset drift");
_Static_assert(offsetof(ERUI_TabDesc, icon) == 48u,
    "ERUI tab icon offset drift");
_Static_assert(offsetof(ERUI_TabDesc, reserved) == 56u,
    "ERUI tab reserved offset drift");
_Static_assert(ERUI_CAP_CUSTOM_TABS == (UINT64_C(1) << 18),
    "ERUI custom-tabs capability drift");
_Static_assert(ERUI_DUPLICATE_TAB_ID == 21u,
    "ERUI duplicate-tab result drift");
_Static_assert(ERUI_TAB_LIMIT_REACHED == 22u,
    "ERUI tab-limit result drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED == 23u,
    "ERUI title-screen text limit result drift");
_Static_assert(ERUI_CAP_TITLE_SCREEN_TEXT == (UINT64_C(1) << 19),
    "ERUI title-screen text capability drift");
_Static_assert(ERUI_CAP_UPDATES == (UINT64_C(1) << 20),
    "ERUI updates capability drift");
_Static_assert(ERUI_CAP_ROW_AVAILABILITY == (UINT64_C(1) << 21),
    "ERUI row-availability capability drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR == (1u << 0),
    "ERUI title-screen custom-color flag drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_TOP_LEFT == 1u,
    "ERUI top-left title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT == 2u,
    "ERUI top-right title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT == 3u,
    "ERUI bottom-left title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT == 4u,
    "ERUI bottom-right title-screen corner drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS == 255u,
    "ERUI title-screen text length limit drift");
_Static_assert(ERUI_TITLE_SCREEN_TEXT_MAX_LINES == 16u,
    "ERUI title-screen text line limit drift");
_Static_assert(ERUI_CUSTOM_TAB_MAX_COUNT == 1024u,
    "ERUI custom-tab count limit drift");
_Static_assert(ERUI_TAB_ICON_FORMAT_RGBA8 == 1u,
    "ERUI RGBA8 tab-icon format drift");
_Static_assert(ERUI_CAP_INPUT_BINDINGS == (UINT64_C(1) << 14),
    "ERUI input bindings capability drift");
_Static_assert(ERUI_DUPLICATE_ACTION_ID == 15u,
    "ERUI duplicate action result drift");
_Static_assert(ERUI_INPUT_DEVICE_ALL == 7u,
    "ERUI input device mask drift");
_Static_assert(ERUI_BUILTIN_PAGE_GAME_OPTIONS == 0u,
    "ERUI built-in Game Options value drift");
_Static_assert(ERUI_BUILTIN_PAGE_CAMERA_OPTIONS == 1u,
    "ERUI built-in Camera Options value drift");
_Static_assert(ERUI_BUILTIN_PAGE_DISPLAY == 2u,
    "ERUI built-in Display value drift");
_Static_assert(ERUI_BUILTIN_PAGE_SOUND == 3u,
    "ERUI built-in Sound value drift");
_Static_assert(ERUI_BUILTIN_PAGE_NETWORK == 4u,
    "ERUI built-in Network value drift");
_Static_assert(ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE == 5u,
    "ERUI built-in Keyboard/Mouse value drift");
_Static_assert(ERUI_BUILTIN_PAGE_GRAPHICS == 6u,
    "ERUI built-in Graphics value drift");
_Static_assert(ERUI_BUILTIN_PAGE_COUNT == 7u,
    "ERUI built-in page count drift");
#endif

static void ERUI_CALL button_callback(void* user_data) {
    (void)user_data;
}

static void ERUI_CALL value_callback(void* user_data, uint8_t value) {
    (void)user_data;
    (void)value;
}

static void ERUI_CALL color_callback(
    void* user_data,
    const ERUI_ColorPickerChangeContext* context) {
    (void)user_data;
    (void)context;
}

static void ERUI_CALL numeric_slider_callback(
    void* user_data,
    const ERUI_NumericSliderChangeContext* context) {
    (void)user_data;
    (void)context;
}

static void ERUI_CALL custom_slider_callback(
    void* user_data,
    const ERUI_CustomSliderChangeContext* context) {
    (void)user_data;
    (void)context;
}

static void ERUI_CALL binding_callback(
    void* user_data,
    const ERUI_InputActionActivatedContext* context) {
    (void)user_data;
    (void)context;
}

static ERUI_Result ERUI_CALL page_title_formatter(
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    uint16_t* output,
    uint32_t output_capacity,
    uint32_t* out_length) {
    (void)user_data;
    (void)context;
    if (!output || output_capacity == 0u || !out_length) {
        return ERUI_INVALID_ARGUMENT;
    }
    output[0] = (uint16_t)'X';
    *out_length = 1u;
    return ERUI_OK;
}

static void ERUI_CALL alert_callback(
    void* user_data,
    ERUI_Result completion_result,
    ERUI_AlertResponse response) {
    (void)user_data;
    (void)completion_result;
    (void)response;
}

int main(void) {
    ERUI_ProviderDesc provider = {0};
    ERUI_ButtonDesc button = {0};
    ERUI_ToggleDesc toggle = {0};
    ERUI_ColorPickerDesc color_picker = {0};
    ERUI_NumericSliderDesc numeric_slider = {0};
    ERUI_CustomSliderDesc custom_slider = {0};
    ERUI_DecimalValue decimal_value = {0};
    ERUI_InputSectionDesc binding_section = {0};
    ERUI_InputActionDesc binding = {0};
    ERUI_PagePresentationDesc presentation = {0};
    ERUI_AlertDesc alert = {0};
    ERUI_TextStyleSpan text_style = {0};
    ERUI_RowUpdateDesc row_update = {0};
    ERUI_UpdateDesc update = {0};
    ERUI_UpdateBatchDesc update_batch = {0};
    ERUI_Api api = {0};
    ERUI_GameLanguageInfo language = {0};
    provider.size = (uint32_t)sizeof(provider);
    provider.api_version = ERUI_API_VERSION_CURRENT;
    button.size = (uint32_t)sizeof(button);
    button.callback = &button_callback;
    toggle.size = (uint32_t)sizeof(toggle);
    toggle.changed_callback = &value_callback;
    color_picker.size = (uint32_t)sizeof(color_picker);
    color_picker.changed_callback = &color_callback;
    color_picker.initial_value.red = 1u;
    color_picker.initial_value.green = 2u;
    color_picker.initial_value.blue = 3u;
    numeric_slider.size = (uint32_t)sizeof(numeric_slider);
    numeric_slider.changed_callback = &numeric_slider_callback;
    numeric_slider.minimum_scaled_value = -10;
    numeric_slider.maximum_scaled_value = 10;
    numeric_slider.step_scaled_value = 1;
    numeric_slider.initial_scaled_value = -5;
    numeric_slider.decimal_places = 1u;
    custom_slider.size = (uint32_t)sizeof(custom_slider);
    custom_slider.changed_callback = &custom_slider_callback;
    custom_slider.value_count = 2u;
    decimal_value.scaled_value = 1275;
    decimal_value.decimal_places = 2u;
    binding_section.size = (uint32_t)sizeof(binding_section);
    binding.size = (uint32_t)sizeof(binding);
    binding.default_inputs.size =
        (uint32_t)sizeof(binding.default_inputs);
    binding.default_inputs.keyboard.state = ERUI_INPUT_SLOT_UNBOUND;
    binding.activated_callback = &binding_callback;
    presentation.size = (uint32_t)sizeof(presentation);
    presentation.formatter = &page_title_formatter;
    alert.size = (uint32_t)sizeof(alert);
    alert.callback = &alert_callback;
    alert.buttons = ERUI_ALERT_BUTTONS_YES_NO;
    alert.placement = ERUI_ALERT_PLACEMENT_CENTER;
    text_style.size = (uint32_t)sizeof(text_style);
    text_style.flags = ERUI_TEXT_STYLE_CUSTOM_COLOR |
        ERUI_TEXT_STYLE_UNDERLINE;
    text_style.length = 1u;
    text_style.color.red = 107u;
    text_style.color.green = 100u;
    text_style.color.blue = 80u;
    row_update.size = (uint32_t)sizeof(row_update);
    row_update.fields = ERUI_ROW_UPDATE_FIELD_LABEL |
        ERUI_ROW_UPDATE_FIELD_HELP |
        ERUI_ROW_UPDATE_FIELD_INTERACTIVE |
        ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
    row_update.interactive = 1u;
    row_update.availability_appearance =
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY;
    row_update.label.size = (uint32_t)sizeof(row_update.label);
    row_update.label.spans = &text_style;
    row_update.label.span_count = 1u;
    row_update.label.span_stride = (uint32_t)sizeof(text_style);
    row_update.help.size = (uint32_t)sizeof(row_update.help);
    row_update.help.span_stride = (uint32_t)sizeof(text_style);
    update.size = (uint32_t)sizeof(update);
    update.kind = ERUI_UPDATE_KIND_ROW;
    update.payload = &row_update;
    update_batch.size = (uint32_t)sizeof(update_batch);
    update_batch.updates = &update;
    update_batch.update_count = 1u;
    update_batch.update_stride = (uint32_t)sizeof(update);
    api.size = (uint32_t)sizeof(api);
    language.size = (uint32_t)sizeof(language);

    if (sizeof(void*) == 8u) {
        if (sizeof(ERUI_StringView) != 16u || sizeof(ERUI_Utf16View) != 16u) {
            return 1;
        }
        if (offsetof(ERUI_Api, capabilities) != 8u) {
            return 2;
        }
    }
    return provider.size == 0u || button.callback == 0 ||
        toggle.changed_callback == 0 || color_picker.changed_callback == 0 ||
        numeric_slider.changed_callback == 0 ||
        custom_slider.changed_callback == 0 ||
        decimal_value.scaled_value != 1275 ||
        binding_section.size == 0u || binding.activated_callback == 0 ||
        color_picker.initial_value.reserved != 0u ||
        presentation.formatter == 0 ||
        alert.callback == 0 || ERUI_CAP_ALERT == 0u ||
        ERUI_CAP_INLINE_CHOICE == 0u || ERUI_CAP_POPUP_CHOICE == 0u ||
        alert.buttons != ERUI_ALERT_BUTTONS_YES_NO ||
        alert.placement != ERUI_ALERT_PLACEMENT_CENTER ||
        ERUI_ALERT_RESPONSE_DISMISSED != 3u ||
        api.size == 0u || language.size == 0u ||
        ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA != 15u ||
        ERUI_CAP_GAME_LANGUAGE == 0u || ERUI_CAP_COLOR_PICKER == 0u ||
        ERUI_CAP_BUILTIN_PAGES == 0u || ERUI_CAP_INPUT_BINDINGS == 0u ||
        ERUI_CAP_STORAGE == 0u || ERUI_CAP_NUMERIC_SLIDER == 0u ||
        ERUI_CAP_CUSTOM_SLIDER == 0u || ERUI_CAP_UPDATES == 0u ||
        update_batch.updates != &update ||
        update.payload != &row_update ||
        ERUI_PAGE_TITLE_BUFFER_CAPACITY < 1u;
}
