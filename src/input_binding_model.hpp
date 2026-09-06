#pragma once

#include <ernativeui/erui.h>

#include <cstdint>

namespace erui::detail {

// Exact-build native token used only at the adapter boundary. Public enum
// values are stable ERNativeUI identifiers and deliberately do not equal
// Elden Ring's private token codes.
struct NativeInputToken {
    std::uint32_t code{};
    std::uint32_t kind{};
    std::uint32_t auxiliary{};
};

inline constexpr std::uint32_t native_controller_input_kind = 0;
inline constexpr std::uint32_t native_keyboard_input_kind = 1;
inline constexpr std::uint32_t native_mouse_input_kind = 2;

[[nodiscard]] bool valid_controller_button(
    ERUI_ControllerButton value) noexcept;
[[nodiscard]] bool valid_keyboard_key(ERUI_KeyboardKey value) noexcept;
[[nodiscard]] bool valid_mouse_button(ERUI_MouseButton value) noexcept;

// Validates the complete C value prefix and its canonical tri-state form.
// ABSENT and UNBOUND slots must carry INVALID; BOUND slots must carry one of
// the explicitly supported semantic enum values.
[[nodiscard]] bool valid_action_inputs(
    const ERUI_ActionInputs& value) noexcept;

// Descriptor defaults and complete live snapshots must support at least one
// device. Sparse mutation/persistence values may validly contain only ABSENT.
[[nodiscard]] bool valid_action_default_inputs(
    const ERUI_ActionInputs& value) noexcept;

[[nodiscard]] ERUI_InputDevices supported_input_devices(
    const ERUI_ActionInputs& value) noexcept;
[[nodiscard]] ERUI_InputDevices bound_input_devices(
    const ERUI_ActionInputs& value) noexcept;
[[nodiscard]] ERUI_InputDevices changed_input_devices(
    const ERUI_ActionInputs& left,
    const ERUI_ActionInputs& right) noexcept;

// Returns the supported slots whose current assignment exactly equals the
// declared default. Both values must describe the same supported-device set.
[[nodiscard]] ERUI_InputDevices default_input_devices(
    const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& defaults) noexcept;

[[nodiscard]] bool same_supported_input_devices(
    const ERUI_ActionInputs& left,
    const ERUI_ActionInputs& right) noexcept;

// Applies a sparse patch. ABSENT means leave unchanged. A patch may not add a
// device that the current action declared unsupported. Output is unchanged
// when validation fails.
[[nodiscard]] bool overlay_action_inputs(
    const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& patch,
    ERUI_ActionInputs& output) noexcept;

// Converts every supported slot to explicit UNBOUND. Unsupported slots stay
// ABSENT. Output is unchanged when validation fails.
[[nodiscard]] bool unbind_supported_action_inputs(
    const ERUI_ActionInputs& current,
    ERUI_ActionInputs& output) noexcept;

// Restores selected supported slots from defaults. Unsupported device bits in
// a valid mask are harmless, which lets ERUI_INPUT_DEVICE_ALL implement the
// aggregate reset. Unknown mask bits and support mismatches are rejected.
[[nodiscard]] bool reset_action_inputs_to_defaults(
    const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& defaults,
    ERUI_InputDevices devices,
    ERUI_ActionInputs& output) noexcept;

// Stable semantic enum <-> tested Elden Ring 2.7.0.0 token translation.
// Decoding rejects unknown codes, the wrong device kind, and all nonzero
// auxiliary values. API 1.1 does not expose modifier chords.
[[nodiscard]] bool controller_to_native_token(
    ERUI_ControllerButton value,
    NativeInputToken& output) noexcept;
[[nodiscard]] bool keyboard_to_native_token(
    ERUI_KeyboardKey value,
    NativeInputToken& output) noexcept;
[[nodiscard]] bool mouse_to_native_token(
    ERUI_MouseButton value,
    NativeInputToken& output) noexcept;

[[nodiscard]] bool controller_from_native_token(
    const NativeInputToken& value,
    ERUI_ControllerButton& output) noexcept;
[[nodiscard]] bool keyboard_from_native_token(
    const NativeInputToken& value,
    ERUI_KeyboardKey& output) noexcept;
[[nodiscard]] bool mouse_from_native_token(
    const NativeInputToken& value,
    ERUI_MouseButton& output) noexcept;

} // namespace erui::detail
