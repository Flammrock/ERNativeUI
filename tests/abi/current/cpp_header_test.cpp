#include <ernativeui/ERNativeUI.hpp>

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
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
#if defined(ERUI_API_VERSION_1_2)
void numeric_slider_changed(const erui::NumericSliderChange&) noexcept {}
void stateful_numeric_slider_changed(
    unsigned& calls,
    const erui::NumericSliderChange&) noexcept {
    ++calls;
}
void custom_slider_changed(const erui::CustomSliderChange&) noexcept {}
void stateful_custom_slider_changed(
    unsigned& calls,
    const erui::CustomSliderChange&) noexcept {
    ++calls;
}
#endif

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
#if defined(ERUI_API_VERSION_1_2)
static_assert(sizeof(ERUI_DecimalValue) == 16);
static_assert(sizeof(ERUI_NumericSliderChangeContext) == 56);
static_assert(sizeof(ERUI_CustomSliderChangeContext) == 40);
static_assert(sizeof(ERUI_NumericSliderDesc) == 136);
static_assert(sizeof(ERUI_CustomSliderDesc) == 80);
static_assert(std::is_trivially_copyable<ERUI_DecimalValue>::value);
static_assert(std::is_trivially_copyable<ERUI_NumericSliderDesc>::value);
static_assert(std::is_trivially_copyable<ERUI_CustomSliderDesc>::value);
static_assert(static_cast<ERUI_Capabilities>(
    erui::Capability::numeric_slider) == ERUI_CAP_NUMERIC_SLIDER);
static_assert(static_cast<ERUI_Capabilities>(
    erui::Capability::custom_slider) == ERUI_CAP_CUSTOM_SLIDER);
static_assert(noexcept(numeric_slider_changed(
    std::declval<const erui::NumericSliderChange&>())));
static_assert(noexcept(custom_slider_changed(
    std::declval<const erui::CustomSliderChange&>())));
#endif
#if defined(ERUI_API_VERSION_1_3)
template <typename T, typename = void>
struct has_legacy_title_screen_text_overload : std::false_type {};

template <typename T>
struct has_legacy_title_screen_text_overload<T, std::void_t<decltype(
    std::declval<T&>().add_title_screen_text(
        std::declval<const erui::TitleScreenTextOptions&>()))>>
    : std::true_type {};

static_assert(sizeof(ERUI_TabIconDesc) == 32);
static_assert(sizeof(ERUI_TabDesc) == 64);
static_assert(sizeof(ERUI_TitleScreenTextDesc) == 40);
static_assert(std::is_trivially_copyable<ERUI_TabIconDesc>::value);
static_assert(std::is_trivially_copyable<ERUI_TabDesc>::value);
static_assert(std::is_trivially_copyable<ERUI_TitleScreenTextDesc>::value);
static_assert(static_cast<ERUI_Capabilities>(
    erui::Capability::custom_tabs) == ERUI_CAP_CUSTOM_TABS);
static_assert(static_cast<ERUI_Capabilities>(
    erui::Capability::title_screen_text) == ERUI_CAP_TITLE_SCREEN_TEXT);
static_assert(static_cast<ERUI_TabIconFormat>(
    erui::TabIconFormat::rgba8) == ERUI_TAB_ICON_FORMAT_RGBA8);
static_assert(static_cast<ERUI_TitleScreenCorner>(
    erui::TitleScreenCorner::top_left) ==
    ERUI_TITLE_SCREEN_CORNER_TOP_LEFT);
static_assert(static_cast<ERUI_TitleScreenCorner>(
    erui::TitleScreenCorner::top_right) ==
    ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT);
static_assert(static_cast<ERUI_TitleScreenCorner>(
    erui::TitleScreenCorner::bottom_left) ==
    ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT);
static_assert(static_cast<ERUI_TitleScreenCorner>(
    erui::TitleScreenCorner::bottom_right) ==
    ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT);
static_assert(std::is_same<
    decltype(std::declval<erui::Menu&>().add_tab(
        std::declval<const erui::TabOptions&>())),
    erui::Page>::value);
static_assert(std::is_same<
    decltype(std::declval<erui::Menu&>().add_title_screen_text(
        std::declval<erui::TitleScreenCorner>(),
        std::declval<const erui::TitleScreenTextOptions&>())),
    void>::value);
static_assert(!has_legacy_title_screen_text_overload<erui::Menu>::value);
#endif
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
#if defined(ERUI_API_VERSION_1_2)
    erui::NumericSliderOptions numeric_options{};
    numeric_options.minimum = -1.0;
    numeric_options.maximum = 1.0;
    numeric_options.step = 0.1;
    numeric_options.initial_value = -0.5;
    numeric_options.prefix = L"x";
    if (page.add_slider<&numeric_slider_changed>(
            L"Numeric", L"Static callback", numeric_options) !=
        ERUI_INVALID_ROW) return 20;
    if (page.add_slider<&stateful_numeric_slider_changed>(
            L"Numeric", L"Stateful callback", numeric_options, calls) !=
        ERUI_INVALID_ROW) return 21;

    erui::CustomSliderOptions custom_options{};
    custom_options.value_count = 3;
    custom_options.initial_index = 1;
    custom_options.format_value = [](
        std::uint32_t index, std::uint32_t count) {
        return std::to_wstring(index) + L"/" + std::to_wstring(count);
    };
    if (page.add_custom_slider<&custom_slider_changed>(
            L"Custom", L"Static callback", custom_options) !=
        ERUI_INVALID_ROW) return 22;
    if (page.add_custom_slider<&stateful_custom_slider_changed>(
            L"Custom", L"Stateful callback", custom_options, calls) !=
        ERUI_INVALID_ROW) return 23;

    erui::detail::PreparedNumericSlider prepared{};
    if (!erui::detail::prepare_numeric_slider(
            numeric_options, prepared)) return 24;
    if (prepared.minimum != -10 || prepared.maximum != 10 ||
        prepared.step != 1 || prepared.initial_value != -5 ||
        prepared.decimal_places != 1 || prepared.value_count != 21) return 25;

    std::int64_t scaled_boundary{};
    if (!erui::detail::scale_decimal_value(
            -9223372036854775808.0, 0, scaled_boundary) ||
        scaled_boundary != (std::numeric_limits<std::int64_t>::min)()) {
        return 26;
    }
    if (erui::detail::scale_decimal_value(
            9223372036854775808.0, 0, scaled_boundary)) return 27;
    if (erui::detail::scale_decimal_value(
            0.1000000001, 1, scaled_boundary)) return 28;

    ERUI_NumericSliderChangeContext numeric_context{};
    numeric_context.size = sizeof(numeric_context);
    numeric_context.provider = 31;
    numeric_context.row = 41;
    numeric_context.value = {-6, 1, 0};
    numeric_context.index = 2;
    numeric_context.value_count = 11;
    erui::NumericSliderChange numeric_change{};
    if (!erui::detail::make_numeric_slider_change(
            &numeric_context, numeric_change)) return 29;
    if (numeric_change.provider != 31 || numeric_change.row != 41 ||
        numeric_change.value != -0.6 || numeric_change.scaled_value != -6 ||
        numeric_change.decimal_places != 1 || numeric_change.index != 2 ||
        numeric_change.value_count != 11) return 30;

    ERUI_CustomSliderChangeContext custom_context{};
    custom_context.size = sizeof(custom_context);
    custom_context.provider = 31;
    custom_context.row = 43;
    custom_context.index = 6;
    custom_context.value_count = 7;
    erui::CustomSliderChange custom_change{};
    if (!erui::detail::make_custom_slider_change(
            &custom_context, custom_change)) return 31;
    if (custom_change.provider != 31 || custom_change.row != 43 ||
        custom_change.index != 6 || custom_change.value_count != 7) return 32;
    if (ERUI_API_V1_2_SIZE != 368u) return 33;
#if defined(ERUI_API_VERSION_1_3)
    std::uint8_t tab_pixels[ERUI_TAB_ICON_RGBA8_BYTE_COUNT]{};
    for (std::size_t offset = 3; offset < sizeof(tab_pixels); offset += 4) {
        tab_pixels[offset] = 255u;
    }
    erui::TabOptions tab_options{};
    tab_options.tab_id = "settings";
    tab_options.title = L"Settings";
    tab_options.preferred_order = 42;
    tab_options.icon = {tab_pixels, sizeof(tab_pixels)};
    tab_options.icon.format = erui::TabIconFormat::rgba8;
    erui::TitleScreenTextOptions title_text{};
    title_text.text = L"Example Mod 1.0";
    title_text.color = erui::Color{216u, 167u, 103u};
    if (page.valid()) return 34;
    if (ERUI_API_V1_3_SIZE != 384u) return 35;
#endif
#endif
#endif
    return 0;
}
