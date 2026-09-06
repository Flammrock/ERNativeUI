#include "input_binding_model.hpp"

#include <array>
#include <cstddef>

namespace erui::detail {
namespace {

constexpr std::array<std::uint32_t, 14> kControllerCodes{
    0x07D0u, // D-pad Up
    0x07D1u, // D-pad Down
    0x07D2u, // D-pad Left
    0x07D3u, // D-pad Right
    0x07D4u, // Face South
    0x07D5u, // Face East
    0x07D6u, // Face West
    0x07D7u, // Face North
    0x07D8u, // Left shoulder
    0x07D9u, // Right shoulder
    0x0BB8u, // Left trigger
    0x0BB9u, // Right trigger
    0x07DAu, // Left-stick press
    0x07DBu, // Right-stick press
};

// These are physical US/QWERTY positions. Elden Ring performs the active
// keyboard-layout remap when it formats a binding for the native screen.
constexpr std::array<std::uint32_t, 72> kKeyboardCodes{
    0x47u, 0x48u, 0x49u, 0x4Au, 0x4Bu, 0x4Cu, 0x4Du, 0x4Eu,
    0x4Fu, 0x50u, // Digit 1 .. Digit 0
    0x53u, 0x54u, // Backspace, Tab
    0x55u, 0x56u, 0x57u, 0x58u, 0x59u, 0x5Au, 0x5Bu, 0x5Cu,
    0x5Du, 0x5Eu, // Q .. P
    0x61u, 0x62u, // Enter, Left Control
    0x63u, 0x64u, 0x65u, 0x66u, 0x67u, 0x68u, 0x69u, 0x6Au,
    0x6Bu, // A .. L
    0x6Fu, // Left Shift
    0x71u, 0x72u, 0x73u, 0x74u, 0x75u, 0x76u, 0x77u, // Z .. M
    0x7Bu, 0x7Du, 0x7Eu, // Right Shift, Left Alt, Space
    0x8Cu, 0x8Du, 0x8Eu, // Numpad 7 .. 9
    0x90u, 0x91u, 0x92u, // Numpad 4 .. 6
    0x94u, 0x95u, 0x96u, // Numpad 1 .. 3
    0x97u, // Numpad 0
    0xAFu, 0xB0u, 0xBBu, // Numpad Enter, Right Control, Right Alt
    0xBDu, 0xBEu, 0xBFu, 0xC0u, 0xC1u, 0xC2u, 0xC3u, 0xC4u,
    0xC5u, 0xC6u, // Home .. Delete
    0x7Cu, 0x8Fu, 0x93u, 0x98u, 0xB9u, // Numpad operators
};

constexpr std::array<std::uint32_t, 7> kMouseCodes{
    0x02u, // Left
    0x01u, // Right
    0x08u, // Middle
    0x03u, // Button 4
    0x04u, // Button 5
    0x09u, // Wheel Up
    0x0Au, // Wheel Down
};

static_assert(
    kControllerCodes.size() + 1 == ERUI_CONTROLLER_BUTTON_COUNT);
static_assert(kKeyboardCodes.size() + 1 == ERUI_KEYBOARD_KEY_COUNT);
static_assert(kMouseCodes.size() + 1 == ERUI_MOUSE_BUTTON_COUNT);

template <typename Slot>
bool same_slot(const Slot& left, const Slot& right) noexcept {
    return left.state == right.state && left.input == right.input;
}

template <typename Slot>
void unbind_slot(Slot& slot) noexcept {
    if (slot.state != ERUI_INPUT_SLOT_ABSENT) {
        slot.state = ERUI_INPUT_SLOT_UNBOUND;
        slot.input = 0;
    }
}

template <typename PublicValue, std::size_t Size>
bool encode_token(
    PublicValue value,
    const std::array<std::uint32_t, Size>& codes,
    std::uint32_t kind,
    NativeInputToken& output) noexcept {
    output = {};
    const std::uint32_t index = static_cast<std::uint32_t>(value);
    if (index == 0 || index > codes.size()) return false;
    output = {codes[index - 1], kind, 0};
    return true;
}

template <typename PublicValue, std::size_t Size>
bool decode_token(
    const NativeInputToken& value,
    const std::array<std::uint32_t, Size>& codes,
    std::uint32_t kind,
    PublicValue& output) noexcept {
    output = 0;
    if (value.kind != kind || value.auxiliary != 0) return false;
    for (std::size_t index = 0; index < codes.size(); ++index) {
        if (codes[index] == value.code) {
            output = static_cast<PublicValue>(index + 1);
            return true;
        }
    }
    return false;
}

void normalize_metadata(ERUI_ActionInputs& value) noexcept {
    value.size = sizeof(ERUI_ActionInputs);
    value.flags = 0;
    value.reserved[0] = 0;
    value.reserved[1] = 0;
}

} // namespace

bool valid_controller_button(ERUI_ControllerButton value) noexcept {
    return value > ERUI_CONTROLLER_BUTTON_INVALID &&
        value < ERUI_CONTROLLER_BUTTON_COUNT;
}

bool valid_keyboard_key(ERUI_KeyboardKey value) noexcept {
    return value > ERUI_KEYBOARD_KEY_INVALID &&
        value < ERUI_KEYBOARD_KEY_COUNT;
}

bool valid_mouse_button(ERUI_MouseButton value) noexcept {
    return value > ERUI_MOUSE_BUTTON_INVALID &&
        value < ERUI_MOUSE_BUTTON_COUNT;
}

bool valid_action_inputs(const ERUI_ActionInputs& value) noexcept {
    return ERUI_DetailValidActionInputs(&value) != 0;
}

bool valid_action_default_inputs(const ERUI_ActionInputs& value) noexcept {
    return valid_action_inputs(value) &&
        (value.controller.state != ERUI_INPUT_SLOT_ABSENT ||
            value.keyboard.state != ERUI_INPUT_SLOT_ABSENT ||
            value.mouse.state != ERUI_INPUT_SLOT_ABSENT);
}

ERUI_InputDevices supported_input_devices(
    const ERUI_ActionInputs& value) noexcept {
    if (!valid_action_inputs(value)) return ERUI_INPUT_DEVICE_NONE;
    ERUI_InputDevices result = ERUI_INPUT_DEVICE_NONE;
    if (value.controller.state != ERUI_INPUT_SLOT_ABSENT) {
        result |= ERUI_INPUT_DEVICE_CONTROLLER;
    }
    if (value.keyboard.state != ERUI_INPUT_SLOT_ABSENT) {
        result |= ERUI_INPUT_DEVICE_KEYBOARD;
    }
    if (value.mouse.state != ERUI_INPUT_SLOT_ABSENT) {
        result |= ERUI_INPUT_DEVICE_MOUSE;
    }
    return result;
}

ERUI_InputDevices bound_input_devices(
    const ERUI_ActionInputs& value) noexcept {
    if (!valid_action_inputs(value)) return ERUI_INPUT_DEVICE_NONE;
    ERUI_InputDevices result = ERUI_INPUT_DEVICE_NONE;
    if (value.controller.state == ERUI_INPUT_SLOT_BOUND) {
        result |= ERUI_INPUT_DEVICE_CONTROLLER;
    }
    if (value.keyboard.state == ERUI_INPUT_SLOT_BOUND) {
        result |= ERUI_INPUT_DEVICE_KEYBOARD;
    }
    if (value.mouse.state == ERUI_INPUT_SLOT_BOUND) {
        result |= ERUI_INPUT_DEVICE_MOUSE;
    }
    return result;
}

ERUI_InputDevices changed_input_devices(
    const ERUI_ActionInputs& left,
    const ERUI_ActionInputs& right) noexcept {
    if (!valid_action_inputs(left) || !valid_action_inputs(right)) {
        return ERUI_INPUT_DEVICE_NONE;
    }
    ERUI_InputDevices result = ERUI_INPUT_DEVICE_NONE;
    if (!same_slot(left.controller, right.controller)) {
        result |= ERUI_INPUT_DEVICE_CONTROLLER;
    }
    if (!same_slot(left.keyboard, right.keyboard)) {
        result |= ERUI_INPUT_DEVICE_KEYBOARD;
    }
    if (!same_slot(left.mouse, right.mouse)) {
        result |= ERUI_INPUT_DEVICE_MOUSE;
    }
    return result;
}

ERUI_InputDevices default_input_devices(
    const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& defaults) noexcept {
    if (!same_supported_input_devices(current, defaults)) {
        return ERUI_INPUT_DEVICE_NONE;
    }
    ERUI_InputDevices result = ERUI_INPUT_DEVICE_NONE;
    if (current.controller.state != ERUI_INPUT_SLOT_ABSENT &&
        same_slot(current.controller, defaults.controller)) {
        result |= ERUI_INPUT_DEVICE_CONTROLLER;
    }
    if (current.keyboard.state != ERUI_INPUT_SLOT_ABSENT &&
        same_slot(current.keyboard, defaults.keyboard)) {
        result |= ERUI_INPUT_DEVICE_KEYBOARD;
    }
    if (current.mouse.state != ERUI_INPUT_SLOT_ABSENT &&
        same_slot(current.mouse, defaults.mouse)) {
        result |= ERUI_INPUT_DEVICE_MOUSE;
    }
    return result;
}

bool same_supported_input_devices(
    const ERUI_ActionInputs& left,
    const ERUI_ActionInputs& right) noexcept {
    return valid_action_inputs(left) && valid_action_inputs(right) &&
        supported_input_devices(left) == supported_input_devices(right);
}

bool overlay_action_inputs(
    const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& patch,
    ERUI_ActionInputs& output) noexcept {
    if (!valid_action_inputs(current) || !valid_action_inputs(patch)) {
        return false;
    }
    const ERUI_InputDevices current_devices =
        supported_input_devices(current);
    const ERUI_InputDevices patch_devices = supported_input_devices(patch);
    if ((patch_devices & ~current_devices) != 0) return false;

    ERUI_ActionInputs result = current;
    normalize_metadata(result);
    if (patch.controller.state != ERUI_INPUT_SLOT_ABSENT) {
        result.controller = patch.controller;
    }
    if (patch.keyboard.state != ERUI_INPUT_SLOT_ABSENT) {
        result.keyboard = patch.keyboard;
    }
    if (patch.mouse.state != ERUI_INPUT_SLOT_ABSENT) {
        result.mouse = patch.mouse;
    }
    output = result;
    return true;
}

bool unbind_supported_action_inputs(
    const ERUI_ActionInputs& current,
    ERUI_ActionInputs& output) noexcept {
    if (!valid_action_inputs(current)) return false;
    ERUI_ActionInputs result = current;
    normalize_metadata(result);
    unbind_slot(result.controller);
    unbind_slot(result.keyboard);
    unbind_slot(result.mouse);
    output = result;
    return true;
}

bool reset_action_inputs_to_defaults(
    const ERUI_ActionInputs& current,
    const ERUI_ActionInputs& defaults,
    ERUI_InputDevices devices,
    ERUI_ActionInputs& output) noexcept {
    if ((devices & ~static_cast<ERUI_InputDevices>(
            ERUI_INPUT_DEVICE_ALL)) != 0 ||
        !same_supported_input_devices(current, defaults)) {
        return false;
    }
    ERUI_ActionInputs result = current;
    normalize_metadata(result);
    if ((devices & ERUI_INPUT_DEVICE_CONTROLLER) != 0 &&
        result.controller.state != ERUI_INPUT_SLOT_ABSENT) {
        result.controller = defaults.controller;
    }
    if ((devices & ERUI_INPUT_DEVICE_KEYBOARD) != 0 &&
        result.keyboard.state != ERUI_INPUT_SLOT_ABSENT) {
        result.keyboard = defaults.keyboard;
    }
    if ((devices & ERUI_INPUT_DEVICE_MOUSE) != 0 &&
        result.mouse.state != ERUI_INPUT_SLOT_ABSENT) {
        result.mouse = defaults.mouse;
    }
    output = result;
    return true;
}

bool controller_to_native_token(
    ERUI_ControllerButton value,
    NativeInputToken& output) noexcept {
    return encode_token(
        value, kControllerCodes, native_controller_input_kind, output);
}

bool keyboard_to_native_token(
    ERUI_KeyboardKey value,
    NativeInputToken& output) noexcept {
    return encode_token(
        value, kKeyboardCodes, native_keyboard_input_kind, output);
}

bool mouse_to_native_token(
    ERUI_MouseButton value,
    NativeInputToken& output) noexcept {
    return encode_token(value, kMouseCodes, native_mouse_input_kind, output);
}

bool controller_from_native_token(
    const NativeInputToken& value,
    ERUI_ControllerButton& output) noexcept {
    return decode_token(
        value, kControllerCodes, native_controller_input_kind, output);
}

bool keyboard_from_native_token(
    const NativeInputToken& value,
    ERUI_KeyboardKey& output) noexcept {
    return decode_token(
        value, kKeyboardCodes, native_keyboard_input_kind, output);
}

bool mouse_from_native_token(
    const NativeInputToken& value,
    ERUI_MouseButton& output) noexcept {
    return decode_token(value, kMouseCodes, native_mouse_input_kind, output);
}

} // namespace erui::detail
