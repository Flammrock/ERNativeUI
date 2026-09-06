#include <ernativeui/ERNativeUI.hpp>

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <type_traits>

namespace {

#if defined(ERUI_API_VERSION_1_1)
void text_changed(const erui::TextInputChange&) noexcept {}
void color_changed(const erui::ColorPickerChange&) noexcept {}
void stateful_color_changed(
    unsigned& calls,
    const erui::ColorPickerChange&) noexcept {
    ++calls;
}
void input_action_activated(const erui::ActionActivation&) noexcept {}
void stateful_input_action_activated(
    unsigned& calls,
    const erui::ActionActivation&) noexcept {
    ++calls;
}

static_assert(sizeof(ERUI_TextInputDesc) == 96);
static_assert(offsetof(ERUI_TextInputDesc, maximum_length) == 88);
static_assert(std::is_trivially_copyable<ERUI_TextInputDesc>::value);
static_assert(std::is_same<
    decltype(erui::TextInputOptions{}.maximum_length),
    std::uint32_t>::value);
static_assert(noexcept(text_changed(std::declval<const erui::TextInputChange&>())));
static_assert(static_cast<ERUI_Capabilities>(erui::Capability::text_input) ==
    ERUI_CAP_TEXT_INPUT);
static_assert(sizeof(ERUI_Color) == 4);
static_assert(sizeof(ERUI_ColorPickerChangeContext) == 32);
static_assert(sizeof(ERUI_ColorPickerDesc) == 64);
static_assert(offsetof(ERUI_ColorPickerDesc, initial_value) == 56);
static_assert(std::is_trivially_copyable<ERUI_Color>::value);
static_assert(std::is_trivially_copyable<ERUI_ColorPickerDesc>::value);
static_assert(std::is_same<
    decltype(erui::Color{}.red),
    std::uint8_t>::value);
static_assert(noexcept(color_changed(
    std::declval<const erui::ColorPickerChange&>())));
static_assert(static_cast<ERUI_Capabilities>(erui::Capability::color_picker) ==
    ERUI_CAP_COLOR_PICKER);
static_assert(static_cast<ERUI_Capabilities>(erui::Capability::builtin_pages) ==
    ERUI_CAP_BUILTIN_PAGES);
static_assert(sizeof(ERUI_ActionInputs) == 40);
static_assert(sizeof(ERUI_InputActionActivatedContext) == 32);
static_assert(sizeof(ERUI_InputSectionDesc) == 32);
static_assert(sizeof(ERUI_InputActionDesc) == 104);
static_assert(std::is_trivially_copyable<ERUI_ActionInputs>::value);
static_assert(std::is_trivially_copyable<ERUI_InputActionDesc>::value);
static_assert(noexcept(input_action_activated(
    std::declval<const erui::ActionActivation&>())));
static_assert(static_cast<ERUI_Capabilities>(
    erui::Capability::input_bindings) == ERUI_CAP_INPUT_BINDINGS);
static_assert(static_cast<ERUI_InputDevices>(
    erui::InputDevice::controller) == ERUI_INPUT_DEVICE_CONTROLLER);
static_assert(static_cast<ERUI_InputDevices>(
    erui::InputDevice::keyboard) == ERUI_INPUT_DEVICE_KEYBOARD);
static_assert(static_cast<ERUI_InputDevices>(
    erui::InputDevice::mouse) == ERUI_INPUT_DEVICE_MOUSE);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::game_options) ==
    ERUI_BUILTIN_PAGE_GAME_OPTIONS);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::camera_options) ==
    ERUI_BUILTIN_PAGE_CAMERA_OPTIONS);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::display) ==
    ERUI_BUILTIN_PAGE_DISPLAY);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::sound) ==
    ERUI_BUILTIN_PAGE_SOUND);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::network) ==
    ERUI_BUILTIN_PAGE_NETWORK);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::keyboard_mouse) ==
    ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE);
static_assert(static_cast<ERUI_BuiltinPage>(erui::BuiltinPage::graphics) ==
    ERUI_BUILTIN_PAGE_GRAPHICS);
static_assert(std::is_same<
    decltype(std::declval<erui::Menu&>().page(erui::BuiltinPage::sound)),
    erui::Page>::value);
static_assert(std::is_same<
    decltype(std::declval<erui::Menu&>().input_bindings()),
    erui::InputBindings>::value);
static_assert(!std::is_default_constructible<erui::Connection>::value);
static_assert(std::is_same<
    decltype(erui::connect(std::chrono::milliseconds{0})),
    erui::ConnectionResult>::value);
#endif

} // namespace

int main() {
    static_assert(ERUI_API_V1_0_SIZE == 128u);
    static_assert(std::is_trivially_copyable<ERUI_Api>::value);
#if defined(ERUI_API_VERSION_1_1)
    erui::TextInputOptions options{};
    if (options.maximum_length != ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH) return 1;
    erui::ColorPickerOptions color_options{};
    if (!color_options.enabled) return 2;
    if (color_options.initial_value.red != 0 ||
        color_options.initial_value.green != 0 ||
        color_options.initial_value.blue != 0) return 3;
    erui::Page page{};
    unsigned calls{};
    if (page.add_color_picker<&color_changed>(
            L"Color", L"Static callback", color_options) !=
        ERUI_INVALID_ROW) return 4;
    if (page.add_color_picker<&stateful_color_changed>(
            L"Color", L"Stateful callback", color_options, calls) !=
        ERUI_INVALID_ROW) return 5;
    erui::InputSection section{};
    if (section.valid()) return 6;
    if (section.add_action<&input_action_activated>(
            "static-action", L"Static action")) return 7;
    const erui::ActionInputs defaults = erui::inputs::keyboard_mouse(
        erui::KeyboardKey::key_q, erui::MouseButton::button4);
    if (section.add_action<&stateful_input_action_activated>(
            "stateful-action", L"Stateful action", defaults, calls)) return 8;
    ERUI_ColorPickerChangeContext context{};
    context.size = sizeof(context);
    context.provider = 17;
    context.row = 23;
    context.value = ERUI_Color{1u, 2u, 3u, 0u};
    erui::ColorPickerChange change{};
    if (!erui::detail::make_color_picker_change(&context, change)) return 9;
    if (change.provider != 17 || change.row != 23 ||
        change.value.red != 1 || change.value.green != 2 ||
        change.value.blue != 3) return 10;
    context.value.reserved = 1;
    if (erui::detail::make_color_picker_change(&context, change)) return 11;
    context.value.reserved = 0;
    context.size = sizeof(context) - 1u;
    if (erui::detail::make_color_picker_change(&context, change)) return 12;

    ERUI_InputActionActivatedContext binding_context{};
    binding_context.size = sizeof(binding_context);
    binding_context.provider = 29;
    binding_context.action = 31;
    binding_context.devices = ERUI_INPUT_DEVICE_CONTROLLER |
        ERUI_INPUT_DEVICE_MOUSE;
    erui::ActionActivation activation{};
    if (!erui::detail::make_action_activation(
            &binding_context, activation)) return 13;
    if (activation.provider != 29 || activation.action != 31 ||
        !activation.includes(erui::InputDevice::controller) ||
        activation.includes(erui::InputDevice::keyboard) ||
        !activation.includes(erui::InputDevice::mouse)) return 14;
    binding_context.flags = 1;
    if (erui::detail::make_action_activation(
            &binding_context, activation)) return 15;
    binding_context.flags = 0;
    binding_context.devices = ERUI_INPUT_DEVICE_ALL | (1u << 7);
    if (erui::detail::make_action_activation(
            &binding_context, activation)) return 16;
    binding_context.devices = ERUI_INPUT_DEVICE_NONE;
    if (erui::detail::make_action_activation(
            &binding_context, activation)) return 17;
    binding_context.devices = ERUI_INPUT_DEVICE_KEYBOARD;
    binding_context.size = sizeof(binding_context) - 1u;
    if (erui::detail::make_action_activation(
            &binding_context, activation)) return 18;
    if (ERUI_API_V1_1_SIZE != 320u) return 19;
#endif
    return 0;
}
