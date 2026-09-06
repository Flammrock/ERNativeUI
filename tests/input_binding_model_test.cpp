#include "input_binding_model.hpp"
#include "test_assertions.hpp"

#include <array>
#include <cstdint>

namespace {

using erui::detail::NativeInputToken;

ERUI_ActionInputs empty_inputs() noexcept {
    ERUI_ActionInputs value{};
    value.size = sizeof(value);
    value.controller = {
        ERUI_INPUT_SLOT_ABSENT, ERUI_CONTROLLER_BUTTON_INVALID};
    value.keyboard = {
        ERUI_INPUT_SLOT_ABSENT, ERUI_KEYBOARD_KEY_INVALID};
    value.mouse = {ERUI_INPUT_SLOT_ABSENT, ERUI_MOUSE_BUTTON_INVALID};
    return value;
}

bool same_inputs(
    const ERUI_ActionInputs& left,
    const ERUI_ActionInputs& right) noexcept {
    return left.size == right.size && left.flags == right.flags &&
        left.controller.state == right.controller.state &&
        left.controller.input == right.controller.input &&
        left.keyboard.state == right.keyboard.state &&
        left.keyboard.input == right.keyboard.input &&
        left.mouse.state == right.mouse.state &&
        left.mouse.input == right.mouse.input &&
        left.reserved[0] == right.reserved[0] &&
        left.reserved[1] == right.reserved[1];
}

void check_validation() {
    using namespace erui::detail;

    ERUI_ActionInputs value = empty_inputs();
    ERUI_TEST_CHECK(valid_action_inputs(value));
    ERUI_TEST_CHECK(!valid_action_default_inputs(value));

    // This value is embedded by value in other 1.1 structures, so its layout
    // is exact rather than an extensible descriptor prefix.
    value.size = sizeof(value) + 16;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.size = sizeof(value) - 1;
    ERUI_TEST_CHECK(!valid_action_inputs(value));

    value = empty_inputs();
    value.flags = 1;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value = empty_inputs();
    value.reserved[0] = 1;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value = empty_inputs();
    value.reserved[1] = 1;
    ERUI_TEST_CHECK(!valid_action_inputs(value));

    value = empty_inputs();
    value.controller = {
        ERUI_INPUT_SLOT_UNBOUND, ERUI_CONTROLLER_BUTTON_INVALID};
    value.keyboard = {
        ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE};
    value.mouse = {ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_WHEEL_DOWN};
    ERUI_TEST_CHECK(valid_action_inputs(value));
    ERUI_TEST_CHECK(valid_action_default_inputs(value));

    value.controller.state = 99;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value = empty_inputs();
    value.controller.input = ERUI_CONTROLLER_BUTTON_FACE_SOUTH;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.controller = {
        ERUI_INPUT_SLOT_UNBOUND, ERUI_CONTROLLER_BUTTON_FACE_SOUTH};
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.controller = {
        ERUI_INPUT_SLOT_BOUND, ERUI_CONTROLLER_BUTTON_INVALID};
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.controller.input = ERUI_CONTROLLER_BUTTON_COUNT;
    ERUI_TEST_CHECK(!valid_action_inputs(value));

    value = empty_inputs();
    value.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.keyboard = {ERUI_INPUT_SLOT_UNBOUND, ERUI_KEYBOARD_KEY_Q};
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_INVALID};
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.keyboard.input = ERUI_KEYBOARD_KEY_COUNT;
    ERUI_TEST_CHECK(!valid_action_inputs(value));

    value = empty_inputs();
    value.mouse.input = ERUI_MOUSE_BUTTON_LEFT;
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.mouse = {ERUI_INPUT_SLOT_UNBOUND, ERUI_MOUSE_BUTTON_LEFT};
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.mouse = {ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_INVALID};
    ERUI_TEST_CHECK(!valid_action_inputs(value));
    value.mouse.input = ERUI_MOUSE_BUTTON_COUNT;
    ERUI_TEST_CHECK(!valid_action_inputs(value));

    ERUI_TEST_CHECK(!valid_controller_button(ERUI_CONTROLLER_BUTTON_INVALID));
    ERUI_TEST_CHECK(valid_controller_button(
        ERUI_CONTROLLER_BUTTON_RIGHT_STICK));
    ERUI_TEST_CHECK(!valid_controller_button(ERUI_CONTROLLER_BUTTON_COUNT));
    ERUI_TEST_CHECK(!valid_keyboard_key(ERUI_KEYBOARD_KEY_INVALID));
    ERUI_TEST_CHECK(valid_keyboard_key(ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE));
    ERUI_TEST_CHECK(!valid_keyboard_key(ERUI_KEYBOARD_KEY_COUNT));
    ERUI_TEST_CHECK(!valid_mouse_button(ERUI_MOUSE_BUTTON_INVALID));
    ERUI_TEST_CHECK(valid_mouse_button(ERUI_MOUSE_BUTTON_WHEEL_DOWN));
    ERUI_TEST_CHECK(!valid_mouse_button(ERUI_MOUSE_BUTTON_COUNT));
}

void check_masks_and_mutations() {
    using namespace erui::detail;

    ERUI_ActionInputs defaults = empty_inputs();
    defaults.controller = {
        ERUI_INPUT_SLOT_BOUND, ERUI_CONTROLLER_BUTTON_FACE_SOUTH};
    defaults.keyboard = {
        ERUI_INPUT_SLOT_UNBOUND, ERUI_KEYBOARD_KEY_INVALID};
    defaults.mouse = {ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_4};

    ERUI_TEST_CHECK(
        supported_input_devices(defaults) == ERUI_INPUT_DEVICE_ALL);
    ERUI_TEST_CHECK(bound_input_devices(defaults) ==
        (ERUI_INPUT_DEVICE_CONTROLLER | ERUI_INPUT_DEVICE_MOUSE));
    ERUI_TEST_CHECK(default_input_devices(defaults, defaults) ==
        ERUI_INPUT_DEVICE_ALL);
    ERUI_TEST_CHECK(same_supported_input_devices(defaults, defaults));

    ERUI_ActionInputs current = defaults;
    current.controller.input = ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER;
    current.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_Q};
    current.mouse = {ERUI_INPUT_SLOT_UNBOUND, ERUI_MOUSE_BUTTON_INVALID};
    ERUI_TEST_CHECK(changed_input_devices(defaults, current) ==
        ERUI_INPUT_DEVICE_ALL);
    ERUI_TEST_CHECK(default_input_devices(current, defaults) ==
        ERUI_INPUT_DEVICE_NONE);

    ERUI_ActionInputs patch = empty_inputs();
    patch.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_NUMPAD_ADD};
    ERUI_ActionInputs overlaid{};
    ERUI_TEST_CHECK(overlay_action_inputs(current, patch, overlaid));
    ERUI_TEST_CHECK(overlaid.size == sizeof(ERUI_ActionInputs));
    ERUI_TEST_CHECK(overlaid.controller.input ==
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER);
    ERUI_TEST_CHECK(overlaid.keyboard.input == ERUI_KEYBOARD_KEY_NUMPAD_ADD);
    ERUI_TEST_CHECK(overlaid.mouse.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(changed_input_devices(current, overlaid) ==
        ERUI_INPUT_DEVICE_KEYBOARD);

    ERUI_ActionInputs controller_only = empty_inputs();
    controller_only.controller = {
        ERUI_INPUT_SLOT_BOUND, ERUI_CONTROLLER_BUTTON_DPAD_UP};
    patch = empty_inputs();
    patch.keyboard = {ERUI_INPUT_SLOT_BOUND, ERUI_KEYBOARD_KEY_A};
    ERUI_ActionInputs sentinel = defaults;
    sentinel.size = 0x12345678u;
    const ERUI_ActionInputs original_sentinel = sentinel;
    ERUI_TEST_CHECK(!overlay_action_inputs(controller_only, patch, sentinel));
    ERUI_TEST_CHECK(same_inputs(sentinel, original_sentinel));

    patch = empty_inputs();
    patch.keyboard = {99, ERUI_KEYBOARD_KEY_INVALID};
    ERUI_TEST_CHECK(!overlay_action_inputs(current, patch, sentinel));
    ERUI_TEST_CHECK(same_inputs(sentinel, original_sentinel));

    ERUI_ActionInputs unbound{};
    ERUI_TEST_CHECK(unbind_supported_action_inputs(current, unbound));
    ERUI_TEST_CHECK(
        supported_input_devices(unbound) == ERUI_INPUT_DEVICE_ALL);
    ERUI_TEST_CHECK(
        bound_input_devices(unbound) == ERUI_INPUT_DEVICE_NONE);
    ERUI_TEST_CHECK(unbound.controller.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(unbound.keyboard.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(unbound.mouse.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(
        unbound.controller.input == ERUI_CONTROLLER_BUTTON_INVALID);
    ERUI_TEST_CHECK(unbound.keyboard.input == ERUI_KEYBOARD_KEY_INVALID);
    ERUI_TEST_CHECK(unbound.mouse.input == ERUI_MOUSE_BUTTON_INVALID);

    ERUI_ActionInputs absent_preserved{};
    ERUI_TEST_CHECK(
        unbind_supported_action_inputs(controller_only, absent_preserved));
    ERUI_TEST_CHECK(
        absent_preserved.controller.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(
        absent_preserved.keyboard.state == ERUI_INPUT_SLOT_ABSENT);
    ERUI_TEST_CHECK(absent_preserved.mouse.state == ERUI_INPUT_SLOT_ABSENT);

    ERUI_ActionInputs invalid_current = current;
    invalid_current.reserved[0] = 1;
    sentinel = original_sentinel;
    ERUI_TEST_CHECK(
        !unbind_supported_action_inputs(invalid_current, sentinel));
    ERUI_TEST_CHECK(same_inputs(sentinel, original_sentinel));

    ERUI_ActionInputs reset{};
    ERUI_TEST_CHECK(reset_action_inputs_to_defaults(
        current, defaults, ERUI_INPUT_DEVICE_KEYBOARD, reset));
    ERUI_TEST_CHECK(reset.controller.input ==
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER);
    ERUI_TEST_CHECK(reset.keyboard.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(reset.mouse.state == ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(default_input_devices(reset, defaults) ==
        ERUI_INPUT_DEVICE_KEYBOARD);

    ERUI_TEST_CHECK(reset_action_inputs_to_defaults(
        current, defaults, ERUI_INPUT_DEVICE_NONE, reset));
    ERUI_TEST_CHECK(same_inputs(reset, current));

    ERUI_TEST_CHECK(reset_action_inputs_to_defaults(
        current, defaults, ERUI_INPUT_DEVICE_ALL, reset));
    ERUI_TEST_CHECK(same_inputs(reset, defaults));

    ERUI_ActionInputs mismatched_defaults = defaults;
    mismatched_defaults.mouse = {
        ERUI_INPUT_SLOT_ABSENT, ERUI_MOUSE_BUTTON_INVALID};
    sentinel = original_sentinel;
    ERUI_TEST_CHECK(!reset_action_inputs_to_defaults(
        current, mismatched_defaults, ERUI_INPUT_DEVICE_ALL, sentinel));
    ERUI_TEST_CHECK(same_inputs(sentinel, original_sentinel));
    ERUI_TEST_CHECK(!reset_action_inputs_to_defaults(
        current, defaults, 1u << 10, sentinel));
    ERUI_TEST_CHECK(same_inputs(sentinel, original_sentinel));

    ERUI_ActionInputs invalid = current;
    invalid.flags = 1;
    ERUI_TEST_CHECK(
        supported_input_devices(invalid) == ERUI_INPUT_DEVICE_NONE);
    ERUI_TEST_CHECK(bound_input_devices(invalid) == ERUI_INPUT_DEVICE_NONE);
    ERUI_TEST_CHECK(
        changed_input_devices(invalid, defaults) == ERUI_INPUT_DEVICE_NONE);
    ERUI_TEST_CHECK(
        default_input_devices(invalid, defaults) == ERUI_INPUT_DEVICE_NONE);
}

struct ControllerMapping {
    ERUI_ControllerButton semantic;
    std::uint32_t native;
};

constexpr std::array<ControllerMapping, 14> kControllerMappings{{
    {ERUI_CONTROLLER_BUTTON_DPAD_UP, 0x07D0u},
    {ERUI_CONTROLLER_BUTTON_DPAD_DOWN, 0x07D1u},
    {ERUI_CONTROLLER_BUTTON_DPAD_LEFT, 0x07D2u},
    {ERUI_CONTROLLER_BUTTON_DPAD_RIGHT, 0x07D3u},
    {ERUI_CONTROLLER_BUTTON_FACE_SOUTH, 0x07D4u},
    {ERUI_CONTROLLER_BUTTON_FACE_EAST, 0x07D5u},
    {ERUI_CONTROLLER_BUTTON_FACE_WEST, 0x07D6u},
    {ERUI_CONTROLLER_BUTTON_FACE_NORTH, 0x07D7u},
    {ERUI_CONTROLLER_BUTTON_LEFT_SHOULDER, 0x07D8u},
    {ERUI_CONTROLLER_BUTTON_RIGHT_SHOULDER, 0x07D9u},
    {ERUI_CONTROLLER_BUTTON_LEFT_TRIGGER, 0x0BB8u},
    {ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER, 0x0BB9u},
    {ERUI_CONTROLLER_BUTTON_LEFT_STICK, 0x07DAu},
    {ERUI_CONTROLLER_BUTTON_RIGHT_STICK, 0x07DBu},
}};

struct KeyboardMapping {
    ERUI_KeyboardKey semantic;
    std::uint32_t native;
};

constexpr std::array<KeyboardMapping, 72> kKeyboardMappings{{
    {ERUI_KEYBOARD_KEY_DIGIT_1, 0x47u},
    {ERUI_KEYBOARD_KEY_DIGIT_2, 0x48u},
    {ERUI_KEYBOARD_KEY_DIGIT_3, 0x49u},
    {ERUI_KEYBOARD_KEY_DIGIT_4, 0x4Au},
    {ERUI_KEYBOARD_KEY_DIGIT_5, 0x4Bu},
    {ERUI_KEYBOARD_KEY_DIGIT_6, 0x4Cu},
    {ERUI_KEYBOARD_KEY_DIGIT_7, 0x4Du},
    {ERUI_KEYBOARD_KEY_DIGIT_8, 0x4Eu},
    {ERUI_KEYBOARD_KEY_DIGIT_9, 0x4Fu},
    {ERUI_KEYBOARD_KEY_DIGIT_0, 0x50u},
    {ERUI_KEYBOARD_KEY_BACKSPACE, 0x53u},
    {ERUI_KEYBOARD_KEY_TAB, 0x54u},
    {ERUI_KEYBOARD_KEY_Q, 0x55u},
    {ERUI_KEYBOARD_KEY_W, 0x56u},
    {ERUI_KEYBOARD_KEY_E, 0x57u},
    {ERUI_KEYBOARD_KEY_R, 0x58u},
    {ERUI_KEYBOARD_KEY_T, 0x59u},
    {ERUI_KEYBOARD_KEY_Y, 0x5Au},
    {ERUI_KEYBOARD_KEY_U, 0x5Bu},
    {ERUI_KEYBOARD_KEY_I, 0x5Cu},
    {ERUI_KEYBOARD_KEY_O, 0x5Du},
    {ERUI_KEYBOARD_KEY_P, 0x5Eu},
    {ERUI_KEYBOARD_KEY_ENTER, 0x61u},
    {ERUI_KEYBOARD_KEY_LEFT_CONTROL, 0x62u},
    {ERUI_KEYBOARD_KEY_A, 0x63u},
    {ERUI_KEYBOARD_KEY_S, 0x64u},
    {ERUI_KEYBOARD_KEY_D, 0x65u},
    {ERUI_KEYBOARD_KEY_F, 0x66u},
    {ERUI_KEYBOARD_KEY_G, 0x67u},
    {ERUI_KEYBOARD_KEY_H, 0x68u},
    {ERUI_KEYBOARD_KEY_J, 0x69u},
    {ERUI_KEYBOARD_KEY_K, 0x6Au},
    {ERUI_KEYBOARD_KEY_L, 0x6Bu},
    {ERUI_KEYBOARD_KEY_LEFT_SHIFT, 0x6Fu},
    {ERUI_KEYBOARD_KEY_Z, 0x71u},
    {ERUI_KEYBOARD_KEY_X, 0x72u},
    {ERUI_KEYBOARD_KEY_C, 0x73u},
    {ERUI_KEYBOARD_KEY_V, 0x74u},
    {ERUI_KEYBOARD_KEY_B, 0x75u},
    {ERUI_KEYBOARD_KEY_N, 0x76u},
    {ERUI_KEYBOARD_KEY_M, 0x77u},
    {ERUI_KEYBOARD_KEY_RIGHT_SHIFT, 0x7Bu},
    {ERUI_KEYBOARD_KEY_LEFT_ALT, 0x7Du},
    {ERUI_KEYBOARD_KEY_SPACE, 0x7Eu},
    {ERUI_KEYBOARD_KEY_NUMPAD_7, 0x8Cu},
    {ERUI_KEYBOARD_KEY_NUMPAD_8, 0x8Du},
    {ERUI_KEYBOARD_KEY_NUMPAD_9, 0x8Eu},
    {ERUI_KEYBOARD_KEY_NUMPAD_4, 0x90u},
    {ERUI_KEYBOARD_KEY_NUMPAD_5, 0x91u},
    {ERUI_KEYBOARD_KEY_NUMPAD_6, 0x92u},
    {ERUI_KEYBOARD_KEY_NUMPAD_1, 0x94u},
    {ERUI_KEYBOARD_KEY_NUMPAD_2, 0x95u},
    {ERUI_KEYBOARD_KEY_NUMPAD_3, 0x96u},
    {ERUI_KEYBOARD_KEY_NUMPAD_0, 0x97u},
    {ERUI_KEYBOARD_KEY_NUMPAD_ENTER, 0xAFu},
    {ERUI_KEYBOARD_KEY_RIGHT_CONTROL, 0xB0u},
    {ERUI_KEYBOARD_KEY_RIGHT_ALT, 0xBBu},
    {ERUI_KEYBOARD_KEY_HOME, 0xBDu},
    {ERUI_KEYBOARD_KEY_ARROW_UP, 0xBEu},
    {ERUI_KEYBOARD_KEY_PAGE_UP, 0xBFu},
    {ERUI_KEYBOARD_KEY_ARROW_LEFT, 0xC0u},
    {ERUI_KEYBOARD_KEY_ARROW_RIGHT, 0xC1u},
    {ERUI_KEYBOARD_KEY_END, 0xC2u},
    {ERUI_KEYBOARD_KEY_ARROW_DOWN, 0xC3u},
    {ERUI_KEYBOARD_KEY_PAGE_DOWN, 0xC4u},
    {ERUI_KEYBOARD_KEY_INSERT, 0xC5u},
    {ERUI_KEYBOARD_KEY_DELETE, 0xC6u},
    {ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY, 0x7Cu},
    {ERUI_KEYBOARD_KEY_NUMPAD_SUBTRACT, 0x8Fu},
    {ERUI_KEYBOARD_KEY_NUMPAD_ADD, 0x93u},
    {ERUI_KEYBOARD_KEY_NUMPAD_DECIMAL, 0x98u},
    {ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE, 0xB9u},
}};

struct MouseMapping {
    ERUI_MouseButton semantic;
    std::uint32_t native;
};

constexpr std::array<MouseMapping, 7> kMouseMappings{{
    {ERUI_MOUSE_BUTTON_LEFT, 0x02u},
    {ERUI_MOUSE_BUTTON_RIGHT, 0x01u},
    {ERUI_MOUSE_BUTTON_MIDDLE, 0x08u},
    {ERUI_MOUSE_BUTTON_4, 0x03u},
    {ERUI_MOUSE_BUTTON_5, 0x04u},
    {ERUI_MOUSE_BUTTON_WHEEL_UP, 0x09u},
    {ERUI_MOUSE_BUTTON_WHEEL_DOWN, 0x0Au},
}};

template <typename Mapping, std::size_t Size>
void check_unique_native_codes(const std::array<Mapping, Size>& mappings) {
    for (std::size_t left = 0; left < mappings.size(); ++left) {
        for (std::size_t right = left + 1; right < mappings.size(); ++right) {
            ERUI_TEST_CHECK(mappings[left].native != mappings[right].native);
            ERUI_TEST_CHECK(
                mappings[left].semantic != mappings[right].semantic);
        }
    }
}

void check_controller_mapping() {
    using namespace erui::detail;
    static_assert(
        kControllerMappings.size() + 1 == ERUI_CONTROLLER_BUTTON_COUNT);
    check_unique_native_codes(kControllerMappings);

    for (const auto& expected : kControllerMappings) {
        NativeInputToken token{0xFFFFFFFFu, 99u, 99u};
        ERUI_TEST_CHECK(controller_to_native_token(expected.semantic, token));
        ERUI_TEST_CHECK(token.code == expected.native);
        ERUI_TEST_CHECK(token.kind == native_controller_input_kind);
        ERUI_TEST_CHECK(token.auxiliary == 0);
        ERUI_ControllerButton decoded = ERUI_CONTROLLER_BUTTON_INVALID;
        ERUI_TEST_CHECK(controller_from_native_token(token, decoded));
        ERUI_TEST_CHECK(decoded == expected.semantic);
    }

    NativeInputToken output{1, 1, 1};
    ERUI_TEST_CHECK(!controller_to_native_token(
        ERUI_CONTROLLER_BUTTON_INVALID, output));
    ERUI_TEST_CHECK(output.code == 0 && output.kind == 0 &&
        output.auxiliary == 0);
    ERUI_TEST_CHECK(!controller_to_native_token(
        ERUI_CONTROLLER_BUTTON_COUNT, output));

    ERUI_ControllerButton decoded = ERUI_CONTROLLER_BUTTON_FACE_SOUTH;
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x07D0u, native_keyboard_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(decoded == ERUI_CONTROLLER_BUTTON_INVALID);
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x07D0u, native_controller_input_kind, 1}, decoded));
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x07DCu, native_controller_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x07DDu, native_controller_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x0BBAu, native_controller_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x0BC5u, native_controller_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x0B54u, native_controller_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!controller_from_native_token(
        {0x0B55u, native_controller_input_kind, 0}, decoded));
}

void check_keyboard_mapping() {
    using namespace erui::detail;
    static_assert(kKeyboardMappings.size() + 1 == ERUI_KEYBOARD_KEY_COUNT);
    check_unique_native_codes(kKeyboardMappings);

    for (const auto& expected : kKeyboardMappings) {
        NativeInputToken token{0xFFFFFFFFu, 99u, 99u};
        ERUI_TEST_CHECK(keyboard_to_native_token(expected.semantic, token));
        ERUI_TEST_CHECK(token.code == expected.native);
        ERUI_TEST_CHECK(token.kind == native_keyboard_input_kind);
        ERUI_TEST_CHECK(token.auxiliary == 0);
        ERUI_KeyboardKey decoded = ERUI_KEYBOARD_KEY_INVALID;
        ERUI_TEST_CHECK(keyboard_from_native_token(token, decoded));
        ERUI_TEST_CHECK(decoded == expected.semantic);
    }

    NativeInputToken output{1, 1, 1};
    ERUI_TEST_CHECK(
        !keyboard_to_native_token(ERUI_KEYBOARD_KEY_INVALID, output));
    ERUI_TEST_CHECK(output.code == 0 && output.kind == 0 &&
        output.auxiliary == 0);
    ERUI_TEST_CHECK(
        !keyboard_to_native_token(ERUI_KEYBOARD_KEY_COUNT, output));

    ERUI_KeyboardKey decoded = ERUI_KEYBOARD_KEY_A;
    ERUI_TEST_CHECK(!keyboard_from_native_token(
        {0x47u, native_controller_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(decoded == ERUI_KEYBOARD_KEY_INVALID);
    for (std::uint32_t auxiliary = 1; auxiliary <= 3; ++auxiliary) {
        ERUI_TEST_CHECK(!keyboard_from_native_token(
            {0x55u, native_keyboard_input_kind, auxiliary}, decoded));
    }

    // Escape is native Cancel, and function keys lack native assignment
    // capture/presentation. None belongs to the public 1.1 mapping.
    ERUI_TEST_CHECK(!keyboard_from_native_token(
        {0x46u, native_keyboard_input_kind, 0}, decoded));
    for (std::uint32_t code = 0x80u; code <= 0x89u; ++code) {
        ERUI_TEST_CHECK(!keyboard_from_native_token(
            {code, native_keyboard_input_kind, 0}, decoded));
    }
    ERUI_TEST_CHECK(!keyboard_from_native_token(
        {0x9Au, native_keyboard_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!keyboard_from_native_token(
        {0x9Bu, native_keyboard_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!keyboard_from_native_token(
        {0x51u, native_keyboard_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(!keyboard_from_native_token(
        {0xFFFFFFFFu, native_keyboard_input_kind, 0}, decoded));

    // These five are not native capture candidates, but are deliberately
    // supported runtime defaults and have valid native labels.
    constexpr std::array<ERUI_KeyboardKey, 5> kRuntimeOnlyOperators{
        ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY,
        ERUI_KEYBOARD_KEY_NUMPAD_SUBTRACT,
        ERUI_KEYBOARD_KEY_NUMPAD_ADD,
        ERUI_KEYBOARD_KEY_NUMPAD_DECIMAL,
        ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE,
    };
    for (ERUI_KeyboardKey key : kRuntimeOnlyOperators) {
        NativeInputToken token{};
        ERUI_TEST_CHECK(keyboard_to_native_token(key, token));
        ERUI_TEST_CHECK(keyboard_from_native_token(token, decoded));
        ERUI_TEST_CHECK(decoded == key);
    }
}

void check_mouse_mapping() {
    using namespace erui::detail;
    static_assert(kMouseMappings.size() + 1 == ERUI_MOUSE_BUTTON_COUNT);
    check_unique_native_codes(kMouseMappings);

    for (const auto& expected : kMouseMappings) {
        NativeInputToken token{0xFFFFFFFFu, 99u, 99u};
        ERUI_TEST_CHECK(mouse_to_native_token(expected.semantic, token));
        ERUI_TEST_CHECK(token.code == expected.native);
        ERUI_TEST_CHECK(token.kind == native_mouse_input_kind);
        ERUI_TEST_CHECK(token.auxiliary == 0);
        ERUI_MouseButton decoded = ERUI_MOUSE_BUTTON_INVALID;
        ERUI_TEST_CHECK(mouse_from_native_token(token, decoded));
        ERUI_TEST_CHECK(decoded == expected.semantic);
    }

    NativeInputToken output{1, 1, 1};
    ERUI_TEST_CHECK(!mouse_to_native_token(ERUI_MOUSE_BUTTON_INVALID, output));
    ERUI_TEST_CHECK(output.code == 0 && output.kind == 0 &&
        output.auxiliary == 0);
    ERUI_TEST_CHECK(!mouse_to_native_token(ERUI_MOUSE_BUTTON_COUNT, output));

    ERUI_MouseButton decoded = ERUI_MOUSE_BUTTON_LEFT;
    ERUI_TEST_CHECK(!mouse_from_native_token(
        {0x02u, native_keyboard_input_kind, 0}, decoded));
    ERUI_TEST_CHECK(decoded == ERUI_MOUSE_BUTTON_INVALID);
    ERUI_TEST_CHECK(!mouse_from_native_token(
        {0x02u, native_mouse_input_kind, 1}, decoded));
    for (std::uint32_t untested = 0x05u; untested <= 0x07u; ++untested) {
        ERUI_TEST_CHECK(!mouse_from_native_token(
            {untested, native_mouse_input_kind, 0}, decoded));
    }
    ERUI_TEST_CHECK(!mouse_from_native_token(
        {0, native_mouse_input_kind, 0}, decoded));
}

} // namespace

int main() {
    check_validation();
    check_masks_and_mutations();
    check_controller_mapping();
    check_keyboard_mapping();
    check_mouse_mapping();
    return 0;
}
