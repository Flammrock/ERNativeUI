#include <ernativeui/ERNativeUI.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            std::cerr << "check failed at line " << __LINE__ << ": "         \
                      << #expression << '\n';                                  \
            return false;                                                      \
        }                                                                      \
    } while (false)

static_assert(std::is_same_v<decltype(erui::format_action_inputs(
    std::declval<const erui::ActionInputs&>(),
    std::declval<std::string&>())), ERUI_Result>);
static_assert(std::is_same_v<decltype(erui::format_action_inputs(
    std::declval<const erui::ActionInputs&>())),
    std::optional<std::string>>);
static_assert(std::is_same_v<decltype(erui::parse_action_inputs(
    std::declval<std::string_view>(),
    std::declval<erui::ActionInputs&>())), ERUI_Result>);
static_assert(std::is_same_v<decltype(erui::parse_action_inputs(
    std::declval<std::string_view>())),
    std::optional<erui::ActionInputs>>);
static_assert(noexcept(erui::format_action_inputs(
    std::declval<const erui::ActionInputs&>(),
    std::declval<std::string&>())));
static_assert(noexcept(erui::format_action_inputs(
    std::declval<const erui::ActionInputs&>())));
static_assert(noexcept(erui::parse_action_inputs(
    std::declval<std::string_view>(),
    std::declval<erui::ActionInputs&>())));
static_assert(noexcept(erui::parse_action_inputs(
    std::declval<std::string_view>())));

bool test_explicit_overloads() {
    const erui::ActionInputs inputs = erui::ActionInputs{}
        .controller(erui::ControllerButton::right_trigger)
        .keyboard(erui::KeyboardKey::key_q)
        .mouse(erui::MouseButton::button4);
    constexpr std::string_view expected =
        "controller:right-trigger,keyboard:key-q,mouse:button4";

    std::string encoded = "leave me unchanged on failure";
    CHECK(erui::format_action_inputs(inputs, encoded) == ERUI_OK);
    CHECK(encoded == expected);

    erui::ActionInputs parsed = erui::inputs::controller(
        erui::ControllerButton::face_north);
    CHECK(erui::parse_action_inputs(
        "mouse:button4,controller:right-trigger,keyboard:key-q", parsed) ==
        ERUI_OK);
    CHECK(parsed == inputs);

    std::string canonical;
    CHECK(erui::format_action_inputs(parsed, canonical) == ERUI_OK);
    CHECK(canonical == expected);
    return true;
}

bool test_optional_overloads() {
    const erui::ActionInputs sparse = erui::ActionInputs{}
        .keyboard(erui::KeyboardKey::numpad_multiply)
        .mouse();
    const auto encoded = erui::format_action_inputs(sparse);
    CHECK(encoded.has_value());
    CHECK(*encoded == "keyboard:numpad-multiply,mouse:unbound");

    const auto parsed = erui::parse_action_inputs(*encoded);
    CHECK(parsed.has_value());
    CHECK(*parsed == sparse);

    const auto empty_text = erui::format_action_inputs(erui::ActionInputs{});
    CHECK(empty_text.has_value());
    CHECK(empty_text->empty());
    const auto empty_inputs = erui::parse_action_inputs(std::string_view{});
    CHECK(empty_inputs.has_value());
    CHECK(empty_inputs->empty());
    CHECK(empty_inputs->controller_state() == erui::InputSlotState::absent);
    CHECK(empty_inputs->keyboard_state() == erui::InputSlotState::absent);
    CHECK(empty_inputs->mouse_state() == erui::InputSlotState::absent);
    return true;
}

bool test_failure_preserves_explicit_outputs() {
    const erui::ActionInputs invalid = erui::ActionInputs{}.keyboard(
        static_cast<erui::KeyboardKey>(ERUI_KEYBOARD_KEY_COUNT));
    std::string encoded = "sentinel";
    CHECK(erui::format_action_inputs(invalid, encoded) ==
        ERUI_INVALID_ARGUMENT);
    CHECK(encoded == "sentinel");
    CHECK(!erui::format_action_inputs(invalid).has_value());

    const erui::ActionInputs sentinel = erui::inputs::all(
        erui::KeyboardKey::key_z,
        erui::MouseButton::button5,
        erui::ControllerButton::left_trigger);
    erui::ActionInputs parsed = sentinel;
    CHECK(erui::parse_action_inputs(
        "keyboard:key-q,keyboard:key-w", parsed) ==
        ERUI_STORAGE_FORMAT_ERROR);
    CHECK(parsed == sentinel);
    CHECK(!erui::parse_action_inputs(
        "keyboard:key-q,keyboard:key-w").has_value());

    constexpr char embedded_nul[] = "keyboard:key-q\0mouse:left";
    const std::string_view counted(embedded_nul, sizeof(embedded_nul) - 1u);
    CHECK(erui::parse_action_inputs(counted, parsed) ==
        ERUI_STORAGE_FORMAT_ERROR);
    CHECK(parsed == sentinel);
    CHECK(!erui::parse_action_inputs(counted).has_value());
    return true;
}

bool test_fluent_state_semantics() {
    const erui::ActionInputs inputs = erui::ActionInputs{}
        .controller()
        .mouse(erui::MouseButton::wheel_down);
    CHECK(inputs.controller_supported());
    CHECK(!inputs.keyboard_supported());
    CHECK(inputs.mouse_supported());
    CHECK(inputs.controller_state() == erui::InputSlotState::unbound);
    CHECK(inputs.keyboard_state() == erui::InputSlotState::absent);
    CHECK(inputs.mouse_state() == erui::InputSlotState::bound);
    CHECK(!inputs.controller_input().has_value());
    CHECK(!inputs.keyboard_input().has_value());
    CHECK(inputs.mouse_input() == erui::MouseButton::wheel_down);

    const auto text = erui::format_action_inputs(inputs);
    CHECK(text && *text == "controller:unbound,mouse:wheel-down");
    const auto parsed = erui::parse_action_inputs(*text);
    CHECK(parsed && *parsed == inputs);
    return true;
}

} // namespace

int main() {
    // These operations intentionally run without connect() and without a host
    // DLL, proving that the public codec is genuinely header-only.
    if (!test_explicit_overloads()) return 1;
    if (!test_optional_overloads()) return 2;
    if (!test_failure_preserves_explicit_outputs()) return 3;
    if (!test_fluent_state_semantics()) return 4;
    return 0;
}
