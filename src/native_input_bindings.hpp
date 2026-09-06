#pragma once

#include <ernativeui/erui.h>

namespace erui::detail {
struct CompiledMenu;
}

namespace erui::native {

// Copies the compiled binding catalog into process-stable native storage and
// allocates every buffer used by the hooks.
// This must complete before install_native_input_bindings().
[[nodiscard]] bool prepare_native_input_bindings(
    const erui::detail::CompiledMenu& menu) noexcept;

// Installs the native list/capture/input hooks. The list builder is enabled
// last, after every mutation and lifetime boundary is protected.
[[nodiscard]] bool install_native_input_bindings() noexcept;

// Atomically overlays one sparse semantic assignment patch on the latest
// native state. ABSENT slots remain unchanged. The backing native value is
// changed by the input-manager hook, so this remains safe while Elden Ring's
// binding screen retains pointers to it. Programmatic changes never invoke
// the player's assignment-changed callback. On success, complete_inputs is
// the resulting full snapshot.
[[nodiscard]] ERUI_Result stage_native_input_action_inputs(
    ERUI_InputActionHandle action,
    const ERUI_ActionInputs& patch,
    ERUI_ActionInputs& complete_inputs) noexcept;

// Atomically restores selected supported devices from the action's declared
// defaults. On success, complete_inputs is the resulting full snapshot.
[[nodiscard]] ERUI_Result reset_native_input_action_inputs(
    ERUI_InputActionHandle action,
    ERUI_InputDevices devices,
    ERUI_ActionInputs& complete_inputs) noexcept;

// Reads the latest complete desired assignment, including player changes
// that may not have reached the host-worker callback queue yet.
[[nodiscard]] ERUI_Result get_native_input_action_inputs(
    ERUI_InputActionHandle action,
    ERUI_ActionInputs& complete_inputs) noexcept;

// Host-worker boundary. Drains bounded pressed-edge and player-assignment
// events and invokes client callbacks. No callback or disk I/O runs from a
// native game hook.
void dispatch_native_input_binding_events() noexcept;

// Stops publication. Once a row has reached a live KeyConfigDialog, its
// backing definitions and values are intentionally retained until process
// exit because the native cells keep raw pointers to them.
void remove_native_input_bindings() noexcept;

} // namespace erui::native
