#pragma once

#include "module.hpp"

#include <ernativeui/erui.h>

#include <string>

namespace erui::native {

// Resolves and installs the game-thread transport used by native alerts.
// Failure is isolated from the menu runtime; native_dialog_available() then
// remains false and enqueue attempts return ERUI_NOT_SUPPORTED. ERUI_CAP_ALERT
// describes the implemented API surface, not address resolution for this build.
[[nodiscard]] bool install_native_dialog_transport(
    const ModuleView& game) noexcept;
void remove_native_dialog_transport() noexcept;

[[nodiscard]] bool native_dialog_available() noexcept;

// True while ERNativeUI's blocking popup owns a live native task. Menu-row
// thunks use this to reject controller events that the underlying GFX page
// incorrectly receives alongside the dialog.
[[nodiscard]] bool native_dialog_owns_menu_input() noexcept;

// True only while CSPopupMenu is dispatching input to ERNativeUI's owned
// dialog. Native action 3 in this scope is the dialog's secondary action, not
// the underlying page's Back command.
[[nodiscard]] bool native_dialog_dispatching_popup_input() noexcept;

// Thread-safe. The message and callback metadata are copied/retained before
// this function returns. Completion callbacks are dispatched separately by
// dispatch_native_dialog_callbacks(), never from the native hook.
[[nodiscard]] ERUI_Result enqueue_native_alert(
    std::wstring message,
    ERUI_AlertButtons buttons,
    ERUI_AlertPlacement placement,
    ERUI_AlertCallback callback,
    void* user_data) noexcept;

// Called by the text-resolver hook. Returned storage remains valid for at
// least the complete native-dialog lifetime plus a retirement grace period.
[[nodiscard]] const wchar_t* lookup_native_dialog_text(
    std::uint32_t message_id) noexcept;

// Called by the host worker. Invokes completed callbacks without holding
// dialog-service locks.
void dispatch_native_dialog_callbacks() noexcept;

} // namespace erui::native
