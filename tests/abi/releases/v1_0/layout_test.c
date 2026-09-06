#include <ernativeui/erui.h>

#include <stddef.h>
#include <stdint.h>

_Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_0,
    "this target is not using the frozen v1.0 header");
_Static_assert(sizeof(ERUI_StringView) == 16u, "frozen string-view drift");
_Static_assert(sizeof(ERUI_Utf16View) == 16u, "frozen UTF-16-view drift");
_Static_assert(sizeof(ERUI_GameLanguageInfo) == 32u,
    "frozen language-info drift");
_Static_assert(sizeof(ERUI_ProviderDesc) == 56u, "frozen provider drift");
_Static_assert(sizeof(ERUI_ButtonDesc) == 64u, "frozen button drift");
_Static_assert(sizeof(ERUI_ToggleDesc) == 64u, "frozen toggle drift");
_Static_assert(sizeof(ERUI_SliderDesc) == 80u, "frozen slider drift");
_Static_assert(sizeof(ERUI_ChoiceDesc) == 72u, "frozen choice drift");
_Static_assert(sizeof(ERUI_SubmenuDesc) == 80u, "frozen submenu drift");
_Static_assert(sizeof(ERUI_PageTitleFormatContext) == 56u,
    "frozen title context drift");
_Static_assert(sizeof(ERUI_PagePresentationDesc) == 64u,
    "frozen presentation drift");
_Static_assert(sizeof(ERUI_AlertDesc) == 48u, "frozen alert drift");
_Static_assert(sizeof(ERUI_Api) == 128u, "frozen table drift");
_Static_assert(_Alignof(ERUI_Api) == 8u, "frozen table alignment drift");
_Static_assert(ERUI_API_V1_0_SIZE == 128u, "frozen prefix drift");
_Static_assert(offsetof(ERUI_Api, capabilities) == 8u,
    "frozen capabilities offset drift");
_Static_assert(offsetof(ERUI_Api, register_provider) == 16u,
    "frozen first function offset drift");
_Static_assert(offsetof(ERUI_Api, get_game_language) == 120u,
    "frozen final function offset drift");

int main(void) {
    return 0;
}
