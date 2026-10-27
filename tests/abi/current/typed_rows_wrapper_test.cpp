#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

struct CallbackCounts {
    unsigned button{};
    unsigned byte_value{};
    unsigned numeric{};
    unsigned custom{};
    unsigned text{};
    unsigned color{};
};

struct Rows {
    erui::ButtonRow button{};
    erui::ToggleRow toggle{};
    erui::SliderRow slider{};
    erui::NumericSliderRow numeric{};
    erui::CustomSliderRow custom{};
    erui::InlineChoiceRow inline_choice{};
    erui::PopupChoiceRow popup_choice{};
    erui::TextInputRow text{};
    erui::ColorPickerRow color{};
    erui::Submenu submenu{};
    erui::ToggleRow submenu_toggle{};
};

void ERUI_CALL button_pressed(void* user_data) noexcept {
    ++*static_cast<unsigned*>(user_data);
}

void ERUI_CALL byte_value_changed(
    void* user_data,
    std::uint8_t) noexcept {
    ++*static_cast<unsigned*>(user_data);
}

void ERUI_CALL numeric_value_changed(
    void* user_data,
    const ERUI_NumericSliderChangeContext*) noexcept {
    ++*static_cast<unsigned*>(user_data);
}

void ERUI_CALL custom_value_changed(
    void* user_data,
    const ERUI_CustomSliderChangeContext*) noexcept {
    ++*static_cast<unsigned*>(user_data);
}

void ERUI_CALL text_changed(
    void* user_data,
    const ERUI_TextInputChangeContext*) noexcept {
    ++*static_cast<unsigned*>(user_data);
}

void ERUI_CALL color_changed(
    void* user_data,
    const ERUI_ColorPickerChangeContext*) noexcept {
    ++*static_cast<unsigned*>(user_data);
}

bool close_to(double left, double right) noexcept {
    return std::fabs(left - right) < 0.0000001;
}

bool same_color(erui::Color left, erui::Color right) noexcept {
    return left.red == right.red && left.green == right.green &&
        left.blue == right.blue;
}

bool accepts_raw_row(erui::RowHandle row) noexcept {
    return row != ERUI_INVALID_ROW;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    auto connected = erui::connect();
    CHECK(connected.success());
    CHECK(connected.value().api_version() == ERUI_API_VERSION_1_4);
    CHECK(connected.value().supports(erui::Capability::row_availability));

    Rows rows{};
    CallbackCounts callbacks{};
    erui::Registration registration{};
    bool pending_rows_were_inactive{};
    ERUI_Result pending_set_result{ERUI_INTERNAL_ERROR};
    ERUI_Result pending_get_result{ERUI_INTERNAL_ERROR};
    ERUI_Result pending_edit_result{ERUI_INTERNAL_ERROR};
    ERUI_Result pending_submenu_edit_result{ERUI_INTERNAL_ERROR};
    bool pending_get_output{true};
    bool pending_submenu_builder_valid{};
    bool converted_submenu_page_valid{};

    {
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.current-typed-rows-cpp";
        provider.display_name = L"Current typed rows C++ wrapper";
        provider.owner_module = GetModuleHandleW(nullptr);

        const auto result = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                erui::Page root = menu.root();

                rows.button = root.add_button(
                    L"Button",
                    L"Typed button row",
                    &button_pressed,
                    &callbacks.button);
                rows.toggle = root.add_toggle(
                    L"Toggle",
                    L"Typed toggle row",
                    1u,
                    &byte_value_changed,
                    &callbacks.byte_value);

                erui::SliderOptions slider{};
                slider.minimum = 0;
                slider.maximum = 100;
                slider.step = 5;
                slider.initial_value = 45u;
                rows.slider = root.add_slider(
                    L"Slider",
                    L"Typed byte slider row",
                    slider,
                    &byte_value_changed,
                    &callbacks.byte_value);

                erui::NumericSliderOptions numeric{};
                numeric.minimum = -1.0;
                numeric.maximum = 1.0;
                numeric.step = 0.1;
                numeric.initial_value = -0.5;
                numeric.prefix = L"x";
                rows.numeric = root.add_slider(
                    L"Numeric",
                    L"Typed numeric slider row",
                    numeric,
                    &numeric_value_changed,
                    &callbacks.numeric);

                erui::CustomSliderOptions custom{};
                custom.value_count = 3;
                custom.initial_index = 1;
                custom.format_value = [](
                    std::uint32_t index,
                    std::uint32_t) {
                    return std::wstring{L"Value "} +
                        std::to_wstring(index);
                };
                rows.custom = root.add_custom_slider(
                    L"Custom",
                    L"Typed custom slider row",
                    custom,
                    &custom_value_changed,
                    &callbacks.custom);

                constexpr std::array<std::wstring_view, 3> choices{
                    L"One", L"Two", L"Three"};
                erui::ChoiceOptions choice{};
                choice.values = choices.data();
                choice.count = choices.size();
                choice.initial_index = 1u;
                rows.inline_choice = root.add_inline_choice(
                    L"Inline",
                    L"Typed inline choice row",
                    choice,
                    &byte_value_changed,
                    &callbacks.byte_value);
                rows.popup_choice = root.add_popup_choice(
                    L"Popup",
                    L"Typed popup choice row",
                    choice,
                    &byte_value_changed,
                    &callbacks.byte_value);

                erui::TextInputOptions text{};
                text.initial_value = L"Before";
                text.placeholder = L"Type here";
                text.maximum_length = 16u;
                rows.text = root.add_text_input(
                    L"Text",
                    L"Typed text input row",
                    text,
                    &text_changed,
                    &callbacks.text);

                erui::ColorPickerOptions color{};
                color.initial_value = erui::Color{10u, 20u, 30u};
                rows.color = root.add_color_picker(
                    L"Color",
                    L"Typed color picker row",
                    color,
                    &color_changed,
                    &callbacks.color);

                rows.submenu = root.add_submenu(
                    L"Advanced",
                    L"Open advanced settings",
                    L"Advanced Settings",
                    L"Advanced page help");
                pending_submenu_builder_valid = rows.submenu.valid();
                erui::Page submenu_page =
                    static_cast<erui::Page>(rows.submenu);
                converted_submenu_page_valid = submenu_page.valid();
                rows.submenu_toggle = submenu_page.add_toggle(
                    L"Nested toggle",
                    L"Registered through the explicit Page conversion",
                    1u,
                    &byte_value_changed,
                    &callbacks.byte_value);

                pending_rows_were_inactive =
                    !rows.button.valid() && !rows.toggle.valid() &&
                    !rows.slider.valid() && !rows.numeric.valid() &&
                    !rows.custom.valid() && !rows.inline_choice.valid() &&
                    !rows.popup_choice.valid() && !rows.text.valid() &&
                    !rows.color.valid();
                pending_set_result = rows.toggle.set_value(false);
                pending_get_result =
                    rows.toggle.get_value(pending_get_output);
                pending_edit_result = rows.toggle.edit()
                    .label(L"Not published")
                    .commit();
                pending_submenu_edit_result = rows.submenu.edit()
                    .page_title(L"Not published")
                    .commit();
            });

        CHECK(result.success());
        registration = result.value();
    }

    // The row values own stable API/provider context. They remain usable after
    // the builder's Menu/Page objects and RegistrationResult are destroyed.
    CHECK(pending_rows_were_inactive);
    CHECK(pending_set_result == ERUI_INVALID_HANDLE);
    CHECK(pending_get_result == ERUI_INVALID_HANDLE);
    CHECK(pending_edit_result == ERUI_INVALID_HANDLE);
    CHECK(pending_submenu_builder_valid);
    CHECK(converted_submenu_page_valid);
    CHECK(pending_submenu_edit_result == ERUI_INVALID_HANDLE);
    CHECK(pending_get_output);
    CHECK(registration.valid());
    CHECK(rows.button.valid());
    CHECK(rows.toggle.valid());
    CHECK(rows.slider.valid());
    CHECK(rows.numeric.valid());
    CHECK(rows.custom.valid());
    CHECK(rows.inline_choice.valid());
    CHECK(rows.popup_choice.valid());
    CHECK(rows.text.valid());
    CHECK(rows.color.valid());
    CHECK(rows.submenu.valid());
    CHECK(rows.submenu.opener_handle() != ERUI_INVALID_ROW);
    CHECK(rows.submenu.page_handle() != ERUI_INVALID_PAGE);
    CHECK(rows.submenu_toggle.valid());

    erui::StyledText submenu_label{};
    submenu_label
        .append(L"Advanced ")
        .append(
            L"updated",
            erui::TextStyle{}
                .color(erui::Color{96u, 220u, 128u})
                .underline());
    erui::StyledText submenu_help{};
    submenu_help.append(L"Updated submenu help");
    erui::StyledText submenu_caption{};
    submenu_caption
        .append(L"Open ")
        .append(
            L"now",
            erui::TextStyle{}.color(erui::Color{220u, 180u, 96u}));
    CHECK(rows.submenu.edit()
        .label(std::move(submenu_label))
        .help(std::move(submenu_help))
        .caption(std::move(submenu_caption))
        .page_title(L"Updated Advanced Settings")
        .page_help(L"Updated advanced page help")
        .commit() == ERUI_OK);
    auto empty_submenu_edit = rows.submenu.edit();
    CHECK(empty_submenu_edit.commit() == ERUI_INVALID_ARGUMENT);
    CHECK(empty_submenu_edit.commit() == ERUI_ALREADY_COMMITTED);

    erui::StyledText styled_label{};
    styled_label
        .append(L"Button ")
        .append(
            L"updated",
            erui::TextStyle{}
                .color(erui::Color{107u, 100u, 80u})
                .underline());
    erui::StyledText styled_help{};
    styled_help
        .append(L"Styled ")
        .append(L"button help", erui::TextStyle{}.bold().italic());
    CHECK(rows.button.edit()
        .label(styled_label)
        .caption(L"Button caption")
        .help(std::move(styled_help))
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.toggle.edit()
        .help(L"Updated toggle help")
        .commit() == ERUI_OK);
    auto availability_edit = rows.toggle.edit();
    CHECK(&availability_edit.interactive(false) == &availability_edit);
    CHECK(&availability_edit.availability_appearance(
        erui::AvailabilityAppearance::disabled) == &availability_edit);
    CHECK(availability_edit.commit() == ERUI_OK);
    CHECK(rows.toggle.edit()
        .interactive(true)
        .availability_appearance(
            erui::AvailabilityAppearance::follow_interactivity)
        .commit() == ERUI_OK);
    CHECK(rows.toggle.edit()
        .availability_appearance(
            static_cast<erui::AvailabilityAppearance>(99u))
        .commit() == ERUI_INVALID_ARGUMENT);
    CHECK(rows.slider.edit()
        .label(L"Slider updated")
        .help(L"Updated slider help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.numeric.edit()
        .label(L"Numeric updated")
        .help(L"Updated numeric help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.custom.edit()
        .label(L"Custom updated")
        .help(L"Updated custom help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.inline_choice.edit()
        .label(L"Inline updated")
        .help(L"Updated inline help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    erui::StyledText popup_caption{};
    popup_caption
        .append(L"Selected: ")
        .append(
            L"Two",
            erui::TextStyle{}
                .color(erui::Color{96u, 220u, 128u})
                .underline());
    CHECK(rows.popup_choice.edit()
        .label(L"Popup updated")
        .caption(std::move(popup_caption))
        .help(L"Updated popup help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.text.edit()
        .label(L"Text updated")
        .help(L"Updated text help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.color.edit()
        .label(L"Color updated")
        .help(L"Updated color help")
        .interactive(false)
        .availability_appearance(erui::AvailabilityAppearance::disabled)
        .commit() == ERUI_OK);
    CHECK(rows.toggle.edit().help(L"").commit() == ERUI_OK);

    auto empty_edit = rows.toggle.edit();
    CHECK(empty_edit.commit() == ERUI_INVALID_ARGUMENT);
    CHECK(empty_edit.commit() == ERUI_ALREADY_COMMITTED);

    CHECK(rows.button.edit().caption(L"").commit() ==
        ERUI_INVALID_ARGUMENT);
    erui::StyledText empty_caption{};
    CHECK(rows.popup_choice.edit()
        .caption(std::move(empty_caption))
        .commit() == ERUI_INVALID_ARGUMENT);

    erui::StyledText overlong(std::wstring(
        ERUI_STYLED_TEXT_MAX_CODE_UNITS + 1u, L'x'));
    CHECK(rows.toggle.edit().label(std::move(overlong)).commit() ==
        ERUI_INVALID_ARGUMENT);
    CHECK(rows.button.edit()
        .caption(std::wstring(
            ERUI_STYLED_TEXT_MAX_CODE_UNITS + 1u, L'x'))
        .commit() == ERUI_INVALID_ARGUMENT);

    erui::StyledText too_many_spans{};
    for (std::uint32_t index = 0;
         index <= ERUI_STYLED_TEXT_MAX_SPANS;
         ++index) {
        too_many_spans.append(
            L"x",
            (index & 1u) != 0
                ? erui::TextStyle{}.underline()
                : erui::TextStyle{}.italic());
    }
    CHECK(rows.toggle.edit()
        .help(std::move(too_many_spans))
        .commit() == ERUI_INVALID_ARGUMENT);

    CHECK(rows.button != ERUI_INVALID_ROW);
    CHECK(ERUI_INVALID_ROW != rows.toggle);
    CHECK(rows.toggle.handle() != ERUI_INVALID_ROW);
    CHECK(accepts_raw_row(rows.toggle));
    const erui::RowHandle legacy_toggle = rows.toggle;
    CHECK(legacy_toggle == rows.toggle.handle());
    const erui::ToggleRow copied_toggle = rows.toggle;
    CHECK(copied_toggle == rows.toggle);
    CHECK(rows.toggle != rows.slider);

    bool toggle_value{};
    CHECK(rows.toggle.get_value(toggle_value) == ERUI_OK);
    CHECK(toggle_value);
    CHECK(rows.toggle.set_value(false) == ERUI_OK);
    CHECK(rows.toggle.get_value(toggle_value) == ERUI_OK);
    CHECK(!toggle_value);

    std::uint8_t slider_value{};
    CHECK(rows.slider.get_value(slider_value) == ERUI_OK);
    CHECK(slider_value == 45u);
    CHECK(rows.slider.set_value(55u) == ERUI_OK);
    CHECK(rows.slider.get_value(slider_value) == ERUI_OK);
    CHECK(slider_value == 55u);

    double numeric_value{};
    CHECK(rows.numeric.get_value(numeric_value) == ERUI_OK);
    CHECK(close_to(numeric_value, -0.5));
    CHECK(rows.numeric.set_value(0.3) == ERUI_OK);
    CHECK(rows.numeric.get_value(numeric_value) == ERUI_OK);
    CHECK(close_to(numeric_value, 0.3));

    std::uint32_t custom_index{};
    CHECK(rows.custom.get_index(custom_index) == ERUI_OK);
    CHECK(custom_index == 1u);
    CHECK(rows.custom.set_index(2u) == ERUI_OK);
    CHECK(rows.custom.get_index(custom_index) == ERUI_OK);
    CHECK(custom_index == 2u);

    std::uint8_t choice_index{};
    CHECK(rows.inline_choice.get_index(choice_index) == ERUI_OK);
    CHECK(choice_index == 1u);
    CHECK(rows.inline_choice.set_index(2u) == ERUI_OK);
    CHECK(rows.inline_choice.get_index(choice_index) == ERUI_OK);
    CHECK(choice_index == 2u);
    CHECK(rows.popup_choice.get_index(choice_index) == ERUI_OK);
    CHECK(choice_index == 1u);
    CHECK(rows.popup_choice.set_index(0u) == ERUI_OK);
    CHECK(rows.popup_choice.get_index(choice_index) == ERUI_OK);
    CHECK(choice_index == 0u);

    std::wstring text_value{};
    CHECK(rows.text.get_text(text_value) == ERUI_OK);
    CHECK(text_value == L"Before");
    CHECK(rows.text.set_text(L"After") == ERUI_OK);
    CHECK(rows.text.get_text(text_value) == ERUI_OK);
    CHECK(text_value == L"After");

    erui::Color color_value{};
    CHECK(rows.color.get_color(color_value) == ERUI_OK);
    CHECK(same_color(color_value, erui::Color{10u, 20u, 30u}));
    const erui::Color next_color{40u, 50u, 60u};
    CHECK(rows.color.set_color(next_color) == ERUI_OK);
    CHECK(rows.color.get_color(color_value) == ERUI_OK);
    CHECK(same_color(color_value, next_color));

    // Existing Registration methods accept typed rows through the deliberate
    // one-way implicit conversion to the raw provider-local RowHandle.
    CHECK(registration.set_value(rows.toggle, 1u) == ERUI_OK);
    CHECK(registration.set_value(rows.slider, 65u) == ERUI_OK);
    CHECK(registration.set_value(rows.inline_choice, 0u) == ERUI_OK);
    CHECK(registration.set_value(rows.popup_choice, 2u) == ERUI_OK);
    CHECK(registration.set_numeric_slider_value(rows.numeric, -0.2) ==
        ERUI_OK);
    CHECK(registration.set_custom_slider_index(rows.custom, 0u) == ERUI_OK);
    CHECK(registration.set_text(rows.text, L"Legacy") == ERUI_OK);
    const erui::Color legacy_color{70u, 80u, 90u};
    CHECK(registration.set_color(rows.color, legacy_color) == ERUI_OK);

    CHECK(rows.toggle.get_value(toggle_value) == ERUI_OK);
    CHECK(toggle_value);
    CHECK(rows.slider.get_value(slider_value) == ERUI_OK);
    CHECK(slider_value == 65u);
    CHECK(rows.inline_choice.get_index(choice_index) == ERUI_OK);
    CHECK(choice_index == 0u);
    CHECK(rows.popup_choice.get_index(choice_index) == ERUI_OK);
    CHECK(choice_index == 2u);
    CHECK(rows.numeric.get_value(numeric_value) == ERUI_OK);
    CHECK(close_to(numeric_value, -0.2));
    CHECK(rows.custom.get_index(custom_index) == ERUI_OK);
    CHECK(custom_index == 0u);
    CHECK(rows.text.get_text(text_value) == ERUI_OK);
    CHECK(text_value == L"Legacy");
    CHECK(rows.color.get_color(color_value) == ERUI_OK);
    CHECK(same_color(color_value, legacy_color));

    // Programmatic writes publish values but never synthesize player-change
    // callbacks. Typed forwarding must preserve that API 1.3 contract.
    CHECK(callbacks.button == 0u);
    CHECK(callbacks.byte_value == 0u);
    CHECK(callbacks.numeric == 0u);
    CHECK(callbacks.custom == 0u);
    CHECK(callbacks.text == 0u);
    CHECK(callbacks.color == 0u);

    erui::ToggleRow aborted_row{};
    erui::Submenu aborted_submenu{};
    {
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.current-typed-rows-aborted-cpp";
        provider.display_name = L"Aborted typed row context";
        provider.owner_module = GetModuleHandleW(nullptr);

        const auto failed = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                aborted_row = menu.root().add_toggle(
                    L"Captured before failure",
                    L"This row must be invalidated by abort",
                    1u,
                    &byte_value_changed,
                    &callbacks.byte_value);
                aborted_submenu = menu.root().add_submenu(
                    L"Captured submenu",
                    L"This submenu must be invalidated by abort",
                    L"Aborted child");
                aborted_submenu.add_toggle(
                    L"Child sentinel", L"Never published", 1u);

                erui::TextInputOptions invalid{};
                invalid.maximum_length = ERUI_TEXT_INPUT_MAX_LENGTH + 1u;
                (void)menu.root().add_text_input(
                    L"Invalid", L"Force draft failure", invalid);
            });

        CHECK(!failed.success());
        CHECK(failed.error().native_result() == ERUI_INVALID_ARGUMENT);
    }

    CHECK(aborted_row.handle() != ERUI_INVALID_ROW);
    CHECK(!aborted_row.valid());
    CHECK(aborted_row.set_value(false) == ERUI_INVALID_HANDLE);
    CHECK(aborted_row.edit().label(L"Rejected").commit() ==
        ERUI_INVALID_HANDLE);
    bool unchanged{true};
    CHECK(aborted_row.get_value(unchanged) == ERUI_INVALID_HANDLE);
    CHECK(unchanged);
    CHECK(aborted_submenu.opener_handle() != ERUI_INVALID_ROW);
    CHECK(aborted_submenu.page_handle() != ERUI_INVALID_PAGE);
    CHECK(!aborted_submenu.valid());
    CHECK(aborted_submenu.edit().page_title(L"Rejected").commit() ==
        ERUI_INVALID_HANDLE);

    erui::ToggleRow empty{};
    CHECK(!empty.valid());
    CHECK(empty == ERUI_INVALID_ROW);
    CHECK(ERUI_INVALID_ROW == empty);
    CHECK(empty.set_value(true) == ERUI_INVALID_HANDLE);

    erui::Submenu empty_submenu{};
    CHECK(!empty_submenu.valid());
    CHECK(empty_submenu.opener_handle() == ERUI_INVALID_ROW);
    CHECK(empty_submenu.page_handle() == ERUI_INVALID_PAGE);
    CHECK(empty_submenu.edit().label(L"Rejected").commit() ==
        ERUI_INVALID_HANDLE);

    erui_unload_test_host(&host);
    return 0;
}
