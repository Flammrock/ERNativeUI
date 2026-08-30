#include <ernativeui/erui.h>

#include <stddef.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ERUI_StringView) == 16u, "ERUI_StringView ABI drift");
_Static_assert(sizeof(ERUI_Utf16View) == 16u, "ERUI_Utf16View ABI drift");
_Static_assert(sizeof(ERUI_GameLanguageInfo) == 32u,
    "ERUI_GameLanguageInfo ABI drift");
_Static_assert(sizeof(ERUI_ProviderDesc) == 56u, "ERUI_ProviderDesc ABI drift");
_Static_assert(sizeof(ERUI_ButtonDesc) == 64u, "ERUI_ButtonDesc ABI drift");
_Static_assert(sizeof(ERUI_ToggleDesc) == 64u, "ERUI_ToggleDesc ABI drift");
_Static_assert(sizeof(ERUI_SliderDesc) == 80u, "ERUI_SliderDesc ABI drift");
_Static_assert(sizeof(ERUI_ChoiceDesc) == 72u, "ERUI_ChoiceDesc ABI drift");
_Static_assert(sizeof(ERUI_SubmenuDesc) == 80u, "ERUI_SubmenuDesc ABI drift");
_Static_assert(sizeof(ERUI_PageTitleFormatContext) == 56u,
    "ERUI_PageTitleFormatContext ABI drift");
_Static_assert(sizeof(ERUI_PagePresentationDesc) == 64u,
    "ERUI_PagePresentationDesc ABI drift");
_Static_assert(sizeof(ERUI_AlertDesc) == 48u, "ERUI_AlertDesc ABI drift");
_Static_assert(sizeof(ERUI_Api) == 128u, "ERUI_Api ABI drift");
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
#endif

static void ERUI_CALL button_callback(void* user_data) {
    (void)user_data;
}

static void ERUI_CALL value_callback(void* user_data, uint8_t value) {
    (void)user_data;
    (void)value;
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
    ERUI_PagePresentationDesc presentation = {0};
    ERUI_AlertDesc alert = {0};
    ERUI_Api api = {0};
    ERUI_GameLanguageInfo language = {0};
    provider.size = (uint32_t)sizeof(provider);
    provider.api_version = ERUI_API_VERSION_CURRENT;
    button.size = (uint32_t)sizeof(button);
    button.callback = &button_callback;
    toggle.size = (uint32_t)sizeof(toggle);
    toggle.changed_callback = &value_callback;
    presentation.size = (uint32_t)sizeof(presentation);
    presentation.formatter = &page_title_formatter;
    alert.size = (uint32_t)sizeof(alert);
    alert.callback = &alert_callback;
    alert.buttons = ERUI_ALERT_BUTTONS_YES_NO;
    alert.placement = ERUI_ALERT_PLACEMENT_CENTER;
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
        toggle.changed_callback == 0 || presentation.formatter == 0 ||
        alert.callback == 0 || ERUI_CAP_ALERT == 0u ||
        ERUI_CAP_INLINE_CHOICE == 0u || ERUI_CAP_POPUP_CHOICE == 0u ||
        alert.buttons != ERUI_ALERT_BUTTONS_YES_NO ||
        alert.placement != ERUI_ALERT_PLACEMENT_CENTER ||
        ERUI_ALERT_RESPONSE_DISMISSED != 3u ||
        api.size == 0u || language.size == 0u ||
        ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA != 15u ||
        ERUI_CAP_GAME_LANGUAGE == 0u ||
        ERUI_PAGE_TITLE_BUFFER_CAPACITY < 1u;
}
