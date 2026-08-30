#pragma once

#include <ernativeui/erui.h>

#include <cstdint>

namespace erui::native {

// Elden Ring's generic-dialog builders encode placement and button count in
// one numeric selector. Button captions are separate GR_Dialogues text IDs.
// Keep this translation isolated so the public API never exposes native IDs.
struct NativeDialogPresentation {
    std::uint32_t builder_kind{};
    std::uint32_t primary_label{};
    std::uint32_t secondary_label{};
    std::uint32_t button_count{};
};

[[nodiscard]] constexpr bool valid_alert_buttons(
    ERUI_AlertButtons buttons) noexcept {
    switch (buttons) {
    case ERUI_ALERT_BUTTONS_OK:
    case ERUI_ALERT_BUTTONS_CANCEL:
    case ERUI_ALERT_BUTTONS_YES:
    case ERUI_ALERT_BUTTONS_NO:
    case ERUI_ALERT_BUTTONS_OK_CANCEL:
    case ERUI_ALERT_BUTTONS_YES_NO:
    case ERUI_ALERT_BUTTONS_DISMISS_ONLY:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool valid_alert_placement(
    ERUI_AlertPlacement placement) noexcept {
    return placement == ERUI_ALERT_PLACEMENT_BOTTOM ||
        placement == ERUI_ALERT_PLACEMENT_CENTER;
}

[[nodiscard]] constexpr bool resolve_native_dialog_presentation(
    ERUI_AlertButtons buttons,
    ERUI_AlertPlacement placement,
    NativeDialogPresentation& output) noexcept {
    if (!valid_alert_buttons(buttons) ||
        !valid_alert_placement(placement)) {
        return false;
    }

    switch (buttons) {
    case ERUI_ALERT_BUTTONS_OK:
        output = {0, 1, 0, 1};
        break;
    case ERUI_ALERT_BUTTONS_CANCEL:
        output = {0, 2, 0, 1};
        break;
    case ERUI_ALERT_BUTTONS_YES:
        output = {0, 3, 0, 1};
        break;
    case ERUI_ALERT_BUTTONS_NO:
        output = {0, 4, 0, 1};
        break;
    case ERUI_ALERT_BUTTONS_OK_CANCEL:
        output = {0, 1, 2, 2};
        break;
    case ERUI_ALERT_BUTTONS_YES_NO:
        output = {0, 3, 4, 2};
        break;
    case ERUI_ALERT_BUTTONS_DISMISS_ONLY:
        output = {0, 0, 0, 0};
        break;
    default:
        return false;
    }

    if (placement == ERUI_ALERT_PLACEMENT_CENTER) {
        constexpr std::uint32_t builders[] = {6, 1, 2};
        output.builder_kind = builders[output.button_count];
    } else {
        constexpr std::uint32_t builders[] = {9, 7, 8};
        output.builder_kind = builders[output.button_count];
    }
    return true;
}

// Native completion kind 2 is the left action and kind 3 is the right action.
// Single-action and dismiss-only layouts are normalized by their public
// semantics, independently of the native action code used to close them.
[[nodiscard]] constexpr ERUI_AlertResponse normalize_native_alert_response(
    std::uint32_t button_count,
    std::uint32_t native_kind) noexcept {
    if (native_kind != 2 && native_kind != 3) {
        return ERUI_ALERT_RESPONSE_NONE;
    }
    if (button_count == 0) return ERUI_ALERT_RESPONSE_DISMISSED;
    if (button_count == 1) return ERUI_ALERT_RESPONSE_PRIMARY;
    if (button_count == 2) {
        if (native_kind == 2) return ERUI_ALERT_RESPONSE_PRIMARY;
        return ERUI_ALERT_RESPONSE_SECONDARY;
    }
    return ERUI_ALERT_RESPONSE_NONE;
}

} // namespace erui::native
