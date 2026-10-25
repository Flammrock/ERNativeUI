#include <ernativeui/ERNativeUI.hpp>

#include "test_assertions.hpp"
#include <array>
#include <iterator>
#include <string>
#include <type_traits>

namespace {
void action() noexcept {}
void ERUI_CALL changed(void*, std::uint8_t) noexcept {}
struct AlertObservation {
    ERUI_Result result{ERUI_INTERNAL_ERROR};
    erui::AlertResponse response{erui::AlertResponse::none};
    bool called{};
};
void ERUI_CALL alert_completed(
    void* context,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    auto* const observation = static_cast<AlertObservation*>(context);
    observation->result = result;
    observation->response = response;
    observation->called = true;
}
ERUI_Result ERUI_CALL format_title(
    const erui::PageTitleFormatContext*,
    std::uint16_t* output,
    std::uint32_t capacity,
    std::uint32_t* length) noexcept {
    return erui::write_page_title(L"Static title", output, capacity, length);
}
void input_action_activated(const erui::ActionActivation&) noexcept {}
void stateful_input_action_activated(
    unsigned& calls,
    const erui::ActionActivation&) noexcept {
    ++calls;
}
void assignments_changed(const erui::AssignmentsChangedEvent& event) noexcept {
    for (const erui::AssignmentChange change : event.changes()) {
        (void)change.action_id();
        (void)change.action();
        (void)change.previous();
        (void)change.current();
        (void)change.reason();
        (void)change.controller_changed();
    }
}
void stateful_assignments_changed(
    unsigned& calls,
    const erui::AssignmentsChangedEvent&) noexcept { ++calls; }
void numeric_slider_changed(
    const erui::NumericSliderChange&) noexcept {}
void stateful_numeric_slider_changed(
    unsigned& calls,
    const erui::NumericSliderChange&) noexcept { ++calls; }
void custom_slider_changed(
    const erui::CustomSliderChange&) noexcept {}
void stateful_custom_slider_changed(
    unsigned& calls,
    const erui::CustomSliderChange&) noexcept { ++calls; }
}

int main() {
    static_assert(__cplusplus >= 201703L);
    static_assert(sizeof(ERUI_StringView) == 16);
    static_assert(sizeof(ERUI_Utf16View) == 16);
    static_assert(sizeof(ERUI_GameLanguageInfo) == 32);
    static_assert(sizeof(ERUI_ProviderDesc) == 56);
    static_assert(sizeof(ERUI_TabIconDesc) == 32);
    static_assert(sizeof(ERUI_TabDesc) == 64);
    static_assert(sizeof(ERUI_TitleScreenTextDesc) == 40);
    static_assert(sizeof(ERUI_ButtonDesc) == 64);
    static_assert(sizeof(ERUI_ToggleDesc) == 64);
    static_assert(sizeof(ERUI_SliderDesc) == 80);
    static_assert(sizeof(ERUI_ChoiceDesc) == 72);
    static_assert(sizeof(ERUI_TextInputChangeContext) == 48);
    static_assert(sizeof(ERUI_TextInputDesc) == 96);
    static_assert(sizeof(ERUI_Color) == 4);
    static_assert(sizeof(ERUI_ColorPickerChangeContext) == 32);
    static_assert(sizeof(ERUI_ColorPickerDesc) == 64);
    static_assert(sizeof(ERUI_DecimalValue) == 16);
    static_assert(sizeof(ERUI_NumericSliderChangeContext) == 56);
    static_assert(sizeof(ERUI_CustomSliderChangeContext) == 40);
    static_assert(sizeof(ERUI_NumericSliderDesc) == 136);
    static_assert(sizeof(ERUI_CustomSliderDesc) == 80);
    static_assert(sizeof(ERUI_ActionInputs) == 40);
    static_assert(sizeof(ERUI_InputActionActivatedContext) == 32);
    static_assert(sizeof(ERUI_InputSectionDesc) == 32);
    static_assert(sizeof(ERUI_InputActionDesc) == 104);
    static_assert(sizeof(ERUI_AssignmentChange) == 128);
    static_assert(sizeof(ERUI_AssignmentsChangedContext) == 32);
    static_assert(sizeof(ERUI_AssignmentsChangedHandlerDesc) == 32);
    static_assert(sizeof(ERUI_StorageDesc) == 40);
    static_assert(sizeof(ERUI_StorageKey) == 48);
    static_assert(sizeof(ERUI_StorageInfo) == 40);
    static_assert(sizeof(ERUI_SubmenuDesc) == 80);
    static_assert(sizeof(ERUI_PageTitleFormatContext) == 56);
    static_assert(sizeof(ERUI_PagePresentationDesc) == 64);
    static_assert(sizeof(ERUI_AlertDesc) == 48);
    static_assert(sizeof(ERUI_Api) == 392);
    static_assert(ERUI_API_V1_0_SIZE == 128);
    static_assert(ERUI_API_V1_1_SIZE == 320);
    static_assert(ERUI_API_V1_2_SIZE == 368);
    static_assert(ERUI_API_V1_3_SIZE == 384);
    static_assert(ERUI_API_V1_4_SIZE == 392);
    static_assert(offsetof(ERUI_Api, add_tab) == 368);
    static_assert(offsetof(ERUI_Api, add_title_screen_text) == 376);
    static_assert(offsetof(ERUI_Api, commit_updates) == 384);
    static_assert(offsetof(ERUI_TabIconDesc, pixels) == 16);
    static_assert(offsetof(ERUI_TabDesc, preferred_order) == 40);
    static_assert(offsetof(ERUI_TabDesc, icon) == 48);
    static_assert(offsetof(ERUI_TitleScreenTextDesc, text) == 8);
    static_assert(offsetof(ERUI_TitleScreenTextDesc, color) == 24);
    static_assert(offsetof(ERUI_TitleScreenTextDesc, corner) == 28);
    static_assert(offsetof(ERUI_TitleScreenTextDesc, reserved) == 32);
    static_assert(ERUI_CUSTOM_TAB_MAX_COUNT == 1024);
    static_assert(ERUI_TAB_LIMIT_REACHED == 22);
    static_assert(ERUI_TITLE_SCREEN_TEXT_MAX_LINES == 16);
    static_assert(ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED == 23);
    static_assert(ERUI_CAP_TITLE_SCREEN_TEXT == (UINT64_C(1) << 19));
    static_assert(ERUI_CAP_UPDATES == (UINT64_C(1) << 20));
    static_assert(ERUI_CAP_ROW_AVAILABILITY == (UINT64_C(1) << 21));
    static_assert(ERUI_ROW_UPDATE_FIELD_INTERACTIVE == (1u << 3));
    static_assert(ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE == (1u << 4));
    static_assert(ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY == 0u);
    static_assert(ERUI_AVAILABILITY_APPEARANCE_NORMAL == 1u);
    static_assert(ERUI_AVAILABILITY_APPEARANCE_DISABLED == 2u);
    static_assert(ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR == (1u << 0));
    static_assert(ERUI_TITLE_SCREEN_CORNER_TOP_LEFT == 1u);
    static_assert(ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT == 2u);
    static_assert(ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT == 3u);
    static_assert(ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT == 4u);
    static_assert(ERUI_TAB_ICON_FORMAT_RGBA8 == 1);
    static_assert(ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS == 255);
    static_assert(static_cast<ERUI_TabIconFormat>(
        erui::TabIconFormat::rgba8) == ERUI_TAB_ICON_FORMAT_RGBA8);
    static_assert(static_cast<ERUI_TitleScreenCorner>(
        erui::TitleScreenCorner::top_left) ==
        ERUI_TITLE_SCREEN_CORNER_TOP_LEFT);
    static_assert(static_cast<ERUI_TitleScreenCorner>(
        erui::TitleScreenCorner::bottom_right) ==
        ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT);
    static_assert(std::is_trivially_copyable<ERUI_Api>::value);
    static_assert(std::is_pointer<erui::ButtonCallback>::value);
    static_assert(std::is_pointer<erui::AlertCallback>::value);
    static_assert(std::is_pointer<erui::InputActionActivatedCallback>::value);
    static_assert(sizeof(erui::AlertOptions) == sizeof(std::uint32_t) * 2);
    static_assert(std::is_same<
        std::underlying_type<erui::AlertResponse>::type,
        std::uint32_t>::value);
    static_assert(static_cast<std::uint32_t>(erui::AlertButtons::dismiss_only) ==
        ERUI_ALERT_BUTTONS_DISMISS_ONLY);
    static_assert(static_cast<std::uint32_t>(erui::AlertPlacement::center) ==
        ERUI_ALERT_PLACEMENT_CENTER);

    const std::wstring exact_title_line(
        ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS, L'X');
    ERUI_TEST_CHECK(erui::detail::valid_title_screen_text(exact_title_line));
    ERUI_TEST_CHECK(erui::detail::valid_title_screen_corner(
        erui::TitleScreenCorner::top_left));
    ERUI_TEST_CHECK(erui::detail::valid_title_screen_corner(
        erui::TitleScreenCorner::top_right));
    ERUI_TEST_CHECK(erui::detail::valid_title_screen_corner(
        erui::TitleScreenCorner::bottom_left));
    ERUI_TEST_CHECK(erui::detail::valid_title_screen_corner(
        erui::TitleScreenCorner::bottom_right));
    ERUI_TEST_CHECK(!erui::detail::valid_title_screen_corner(
        static_cast<erui::TitleScreenCorner>(0u)));
    ERUI_TEST_CHECK(!erui::detail::valid_title_screen_corner(
        static_cast<erui::TitleScreenCorner>(5u)));
    ERUI_TEST_CHECK(!erui::detail::valid_title_screen_text(
        exact_title_line + L"X"));
    for (const wchar_t separator : {
             L'\r', L'\n', static_cast<wchar_t>(0x0085u),
             static_cast<wchar_t>(0x2028u),
             static_cast<wchar_t>(0x2029u)}) {
        const std::array<wchar_t, 3> separated{L'A', separator, L'B'};
        ERUI_TEST_CHECK(!erui::detail::valid_title_screen_text(
            std::wstring_view(separated.data(), separated.size())));
    }
    static_assert(static_cast<std::uint32_t>(erui::AlertResponse::dismissed) ==
        ERUI_ALERT_RESPONSE_DISMISSED);
    static_assert(!std::is_default_constructible<erui::Connection>::value);
    static_assert(static_cast<ERUI_Capabilities>(
        erui::Capability::input_bindings) == ERUI_CAP_INPUT_BINDINGS);
    static_assert(static_cast<ERUI_Capabilities>(
        erui::Capability::storage) == ERUI_CAP_STORAGE);
    static_assert(static_cast<ERUI_Capabilities>(
        erui::Capability::numeric_slider) == ERUI_CAP_NUMERIC_SLIDER);
    static_assert(static_cast<ERUI_Capabilities>(
        erui::Capability::custom_slider) == ERUI_CAP_CUSTOM_SLIDER);
    static_assert(static_cast<ERUI_ControllerButton>(
        erui::ControllerButton::right_trigger) ==
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER);
    static_assert(static_cast<ERUI_KeyboardKey>(
        erui::KeyboardKey::key_q) == ERUI_KEYBOARD_KEY_Q);
    static_assert(static_cast<ERUI_MouseButton>(
        erui::MouseButton::button4) == ERUI_MOUSE_BUTTON_4);
    static_assert(std::is_same<
        decltype(erui::connect()),
        erui::ConnectionResult>::value);

    erui::ProviderOptions provider{};
    provider.provider_id = "compile-test";
    provider.display_name = L"Compile Test";
    ERUI_TEST_CHECK(erui::detail::valid_machine_identifier(
        provider.provider_id));
    ERUI_TEST_CHECK(!erui::detail::valid_machine_identifier("invalid/id"));
    erui::SliderOptions slider{};
    slider.initial_value = 50;
    erui::NumericSliderOptions numeric_slider{};
    numeric_slider.minimum = -1.0;
    numeric_slider.maximum = 1.0;
    numeric_slider.step = 0.1;
    numeric_slider.initial_value = -0.5;
    numeric_slider.prefix = L"x";
    erui::CustomSliderOptions custom_slider{};
    custom_slider.value_count = 3;
    custom_slider.initial_index = 1;
    custom_slider.format_value = [](std::uint32_t index, std::uint32_t) {
        return std::to_wstring(index);
    };
    erui::ColorPickerOptions color_picker{};
    color_picker.initial_value = erui::Color{10, 20, 30};
    ERUI_TEST_CHECK(slider.maximum == 100);
    ERUI_TEST_CHECK(color_picker.enabled);
    ERUI_TEST_CHECK(color_picker.initial_value.green == 20);
    erui::Page page{};
    const std::wstring_view choices[] = {L"One", L"Two"};
    erui::ChoiceOptions choice{};
    choice.values = choices;
    choice.count = std::size(choices);
    choice.initial_index = 1;
    ERUI_TEST_CHECK(page.add_inline_choice(
        L"Inline", L"", choice, &changed) == ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(page.add_popup_choice(
        L"Popup", L"", choice, &changed) == ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(page.add_color_picker(
        L"Color", L"", color_picker) == ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(page.add_slider<&numeric_slider_changed>(
        L"Numeric", L"", numeric_slider) == ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(page.add_custom_slider<&custom_slider_changed>(
        L"Custom", L"", custom_slider) == ERUI_INVALID_ROW);
    erui::ActionInputs defaults = erui::inputs::all(
        erui::KeyboardKey::key_q,
        erui::MouseButton::button4,
        erui::ControllerButton::right_trigger);
    ERUI_TEST_CHECK(defaults.controller_input() ==
        erui::ControllerButton::right_trigger);
    ERUI_TEST_CHECK(defaults.keyboard_input() == erui::KeyboardKey::key_q);
    ERUI_TEST_CHECK(defaults.mouse_input() == erui::MouseButton::button4);
    defaults = erui::ActionInputs{}
        .keyboard(erui::KeyboardKey::key_z)
        .mouse()
        .controller(erui::ControllerButton::face_south);
    ERUI_TEST_CHECK(defaults.mouse_supported());
    ERUI_TEST_CHECK(!defaults.mouse_input());

    erui::InputSection input_section{};
    unsigned binding_calls{};
    ERUI_TEST_CHECK(page.add_slider<&stateful_numeric_slider_changed>(
        L"Numeric state", L"", numeric_slider, binding_calls) ==
        ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(page.add_custom_slider<&stateful_custom_slider_changed>(
        L"Custom state", L"", custom_slider, binding_calls) ==
        ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(!input_section.valid());
    ERUI_TEST_CHECK(!input_section.add_action<&input_action_activated>(
        "compile-test", L"Compile test"));
    ERUI_TEST_CHECK(!input_section.add_action<&stateful_input_action_activated>(
        "stateful-compile-test", L"Stateful compile test", defaults,
        binding_calls));
    erui::InputAction input_action{};
    ERUI_TEST_CHECK(input_action.bind(defaults) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(input_action.devices().keyboard().bind(
        erui::KeyboardKey::key_q) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(input_action.devices().mouse().unbind() ==
        ERUI_INVALID_HANDLE);
    erui::InputBindings input_bindings{};
    ERUI_TEST_CHECK(!input_bindings.valid());
    ERUI_TEST_CHECK(input_bindings.on_assignments_changed<
        &assignments_changed>() == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(input_bindings.on_assignments_changed<
        &stateful_assignments_changed>(binding_calls) == ERUI_INVALID_HANDLE);
    page.set_presentation<&format_title>(L"Outer", L"Base");
    erui::PagePresentation presentation{};
    presentation.menu_title = L"Outer";
    presentation.page_title = L"Base";
    ERUI_TEST_CHECK(presentation.formatter == nullptr);
    erui::Registration registration{};
    erui::Color color{};
    ERUI_TEST_CHECK(registration.set_color(ERUI_INVALID_ROW, color) ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.get_color(ERUI_INVALID_ROW, color) ==
        ERUI_INVALID_HANDLE);
    double numeric_value{};
    std::uint32_t custom_index{};
    ERUI_TEST_CHECK(registration.set_numeric_slider_value(
        ERUI_INVALID_ROW, 0.6) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.get_numeric_slider_value(
        ERUI_INVALID_ROW, numeric_value) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.set_custom_slider_index(
        ERUI_INVALID_ROW, 1) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.get_custom_slider_index(
        ERUI_INVALID_ROW, custom_index) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.alert(L"Compile test") == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.alert(L"Compile test", &alert_completed) ==
        ERUI_INVALID_HANDLE);
    erui::AlertOptions alert_options{};
    ERUI_TEST_CHECK(alert_options.buttons == erui::AlertButtons::ok);
    ERUI_TEST_CHECK(alert_options.placement == erui::AlertPlacement::bottom);
    alert_options.buttons = erui::AlertButtons::yes_no;
    alert_options.placement = erui::AlertPlacement::center;
    ERUI_TEST_CHECK(registration.alert(
        L"Compile test", alert_options, &alert_completed) == ERUI_INVALID_HANDLE);

    AlertObservation observation{};
    auto* callback_state = new erui::detail::AlertCallbackState{
        &alert_completed, &observation};
    erui::detail::alert_callback_thunk(
        callback_state, ERUI_OK, ERUI_ALERT_RESPONSE_SECONDARY);
    ERUI_TEST_CHECK(observation.called);
    ERUI_TEST_CHECK(observation.result == ERUI_OK);
    ERUI_TEST_CHECK(observation.response == erui::AlertResponse::secondary);

    ERUI_InputActionActivatedContext binding_context{};
    binding_context.size = sizeof(binding_context);
    binding_context.provider = 41;
    binding_context.action = 43;
    binding_context.devices = ERUI_INPUT_DEVICE_KEYBOARD |
        ERUI_INPUT_DEVICE_MOUSE;
    erui::ActionActivation activation{};
    ERUI_TEST_CHECK(erui::detail::make_action_activation(
        &binding_context, activation));
    ERUI_TEST_CHECK(activation.provider == 41);
    ERUI_TEST_CHECK(activation.action == 43);
    ERUI_TEST_CHECK(!activation.includes(erui::InputDevice::controller));
    ERUI_TEST_CHECK(activation.includes(erui::InputDevice::keyboard));
    ERUI_TEST_CHECK(activation.includes(erui::InputDevice::mouse));

    erui::Storage storage{};
    ERUI_TEST_CHECK(storage.load() == ERUI_INVALID_HANDLE);
    auto storage_section = storage.section("bindings");
    ERUI_TEST_CHECK(!storage_section.valid());
    ERUI_TEST_CHECK(input_bindings.on_assignments_changed<
        &erui::persist_assignment_changes>(storage_section) ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(input_bindings.on_assignments_changed<
        &erui::persist_assignment_changes>(
            storage.section("bindings")) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(input_bindings.persist_assignments_to(storage_section) ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(storage_section.set("quick-action", defaults) ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(!storage_section.get<erui::ActionInputs>(
        "quick-action").found());
    ERUI_TEST_CHECK(!storage_section.get<std::string>("name").found());
    ERUI_TEST_CHECK(!storage_section.get<std::int64_t>("count").found());
    ERUI_TEST_CHECK(!storage_section.get<double>("scale").found());
    ERUI_TEST_CHECK(storage_section.set("name", "value") ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(storage_section.set("count", std::int64_t{42}) ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(storage_section.set("scale", 1.5) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(erui::StorageOptions::beside_module(L"config.ini").location ==
        erui::StorageLocation::owner_module_directory);

    erui::detail::PreparedNumericSlider prepared{};
    ERUI_TEST_CHECK(erui::detail::prepare_numeric_slider(
        numeric_slider, prepared));
    ERUI_TEST_CHECK(prepared.minimum == -10);
    ERUI_TEST_CHECK(prepared.maximum == 10);
    ERUI_TEST_CHECK(prepared.step == 1);
    ERUI_TEST_CHECK(prepared.initial_value == -5);
    ERUI_TEST_CHECK(prepared.decimal_places == 1);
    ERUI_TEST_CHECK(prepared.value_count == 21);
    ERUI_DecimalValue decimal{};
    ERUI_TEST_CHECK(erui::detail::infer_decimal_value(12.75, decimal));
    ERUI_TEST_CHECK(decimal.scaled_value == 1275);
    ERUI_TEST_CHECK(decimal.decimal_places == 2);
    (void)&action;
    (void)&changed;
    return 0;
}
