#include <ernativeui/erui.h>

#include <stddef.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ERUI_StringView) == 16u, "ERUI_StringView ABI drift");
_Static_assert(sizeof(ERUI_Utf16View) == 16u, "ERUI_Utf16View ABI drift");
_Static_assert(sizeof(ERUI_ProviderDesc) == 56u, "v1.0 provider prefix drift");
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
_Static_assert(offsetof(ERUI_Api, register_provider) == 16u,
    "v1.0 first function moved");
_Static_assert(offsetof(ERUI_Api, get_game_language) == 120u,
    "v1.0 final function moved");

#if defined(ERUI_API_VERSION_1_1)
_Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_1,
    "unexpected current API version");
_Static_assert(ERUI_BUFFER_TOO_SMALL == 14u, "result values were renumbered");
_Static_assert(ERUI_CAP_TEXT_INPUT == (UINT64_C(1) << 11),
    "TextInput capability bit drift");
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
_Static_assert(ERUI_API_V1_1_SIZE >= 152u,
    "v1.1 prefix does not contain the TextInput block");
_Static_assert(sizeof(ERUI_Api) >= ERUI_API_V1_1_SIZE,
    "current API structure is smaller than its selected prefix");
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
