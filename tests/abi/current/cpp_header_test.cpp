#include <ernativeui/ERNativeUI.hpp>

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace {

#if defined(ERUI_API_VERSION_1_1)
void button_action() noexcept {}
void text_changed(const erui::TextInputChange&) noexcept {}
void stateful_text_changed(
    unsigned& calls,
    const erui::TextInputChange&) noexcept {
    ++calls;
}
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

template <typename T, typename = void>
struct has_edit : std::false_type {};

template <typename T>
struct has_edit<T, std::void_t<decltype(std::declval<const T&>().edit())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_caption : std::false_type {};

template <typename T>
struct has_caption<T, std::void_t<decltype(
    std::declval<T&>().caption(std::declval<std::wstring_view>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_caption_template_bypass : std::false_type {};

template <typename T>
struct has_caption_template_bypass<T, std::void_t<decltype(
    std::declval<T&>().template caption<erui::ButtonRowEdit>(
        std::declval<std::wstring_view>()))>> : std::true_type {};

#if defined(ERUI_API_VERSION_1_4)
template <typename T, typename = void>
struct has_interactive : std::false_type {};

template <typename T>
struct has_interactive<T, std::void_t<decltype(
    std::declval<T&>().interactive(true))>> : std::true_type {};

template <typename T, typename = void>
struct has_availability_appearance : std::false_type {};

template <typename T>
struct has_availability_appearance<T, std::void_t<decltype(
    std::declval<T&>().availability_appearance(
        erui::AvailabilityAppearance::follow_interactivity))>>
    : std::true_type {};
#endif

template <typename T, typename = void>
struct has_set_value : std::false_type {};

template <typename T>
struct has_set_value<T, std::void_t<decltype(&T::set_value)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_get_value : std::false_type {};

template <typename T>
struct has_get_value<T, std::void_t<decltype(&T::get_value)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_set_index : std::false_type {};

template <typename T>
struct has_set_index<T, std::void_t<decltype(&T::set_index)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_get_index : std::false_type {};

template <typename T>
struct has_get_index<T, std::void_t<decltype(&T::get_index)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_set_text : std::false_type {};

template <typename T>
struct has_set_text<T, std::void_t<decltype(&T::set_text)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_get_text : std::false_type {};

template <typename T>
struct has_get_text<T, std::void_t<decltype(&T::get_text)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_set_color : std::false_type {};

template <typename T>
struct has_set_color<T, std::void_t<decltype(&T::set_color)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_get_color : std::false_type {};

template <typename T>
struct has_get_color<T, std::void_t<decltype(&T::get_color)>>
    : std::true_type {};

template <typename... Rows>
struct no_cross_row_conversions : std::true_type {};

template <typename Head, typename... Tail>
struct no_cross_row_conversions<Head, Tail...>
    : std::bool_constant<
          ((!std::is_convertible_v<Head, Tail> &&
               !std::is_convertible_v<Tail, Head> &&
               !std::is_constructible_v<Head, Tail> &&
               !std::is_constructible_v<Tail, Head> &&
               !std::is_assignable_v<Head&, Tail> &&
               !std::is_assignable_v<Tail&, Head>) && ...) &&
          no_cross_row_conversions<Tail...>::value> {};

template <typename T>
constexpr bool typed_row_value_contract =
    std::is_base_of_v<erui::RowRef, T> &&
    std::is_final_v<T> &&
    !std::is_polymorphic_v<T> &&
    std::is_nothrow_default_constructible_v<T> &&
    std::is_nothrow_copy_constructible_v<T> &&
    std::is_nothrow_move_constructible_v<T> &&
    std::is_nothrow_copy_assignable_v<T> &&
    std::is_nothrow_move_assignable_v<T> &&
    std::is_convertible_v<T, erui::RowHandle> &&
    !std::is_constructible_v<T, erui::RowHandle> &&
    sizeof(T) == sizeof(erui::RowRef);

template <typename T>
constexpr bool row_edit_value_contract =
    !std::is_default_constructible_v<T> &&
    !std::is_copy_constructible_v<T> &&
    !std::is_copy_assignable_v<T> &&
    std::is_nothrow_move_constructible_v<T> &&
    std::is_nothrow_move_assignable_v<T> &&
    std::is_same_v<decltype(std::declval<T&>().label(
        std::declval<std::wstring_view>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().label(
        std::declval<std::wstring_view>())), T&&> &&
    std::is_same_v<decltype(std::declval<T&>().help(
        std::declval<std::wstring_view>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().help(
        std::declval<std::wstring_view>())), T&&> &&
    std::is_same_v<decltype(std::declval<T&>().label(
        std::declval<const erui::StyledText&>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().label(
        std::declval<erui::StyledText&&>())), T&&> &&
    std::is_same_v<decltype(std::declval<T&>().help(
        std::declval<const erui::StyledText&>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().help(
        std::declval<erui::StyledText&&>())), T&&> &&
    std::is_same_v<decltype(std::declval<T&>().commit()), ERUI_Result> &&
    noexcept(std::declval<T&>().label(std::declval<std::wstring_view>())) &&
    noexcept(std::declval<T&>().help(std::declval<std::wstring_view>())) &&
    noexcept(std::declval<T&>().label(
        std::declval<const erui::StyledText&>())) &&
    noexcept(std::declval<T&>().help(
        std::declval<erui::StyledText&&>())) &&
    noexcept(std::declval<T&>().commit());

template <typename T>
constexpr bool caption_edit_value_contract =
    std::is_same_v<decltype(std::declval<T&>().caption(
        std::declval<std::wstring_view>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().caption(
        std::declval<std::wstring_view>())), T&&> &&
    std::is_same_v<decltype(std::declval<T&>().caption(
        std::declval<const erui::StyledText&>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().caption(
        std::declval<const erui::StyledText&>())), T&&> &&
    std::is_same_v<decltype(std::declval<T&>().caption(
        std::declval<erui::StyledText&&>())), T&> &&
    std::is_same_v<decltype(std::declval<T&&>().caption(
        std::declval<erui::StyledText&&>())), T&&> &&
    noexcept(std::declval<T&>().caption(
        std::declval<std::wstring_view>())) &&
    noexcept(std::declval<T&&>().caption(
        std::declval<std::wstring_view>())) &&
    noexcept(std::declval<T&>().caption(
        std::declval<const erui::StyledText&>())) &&
    noexcept(std::declval<T&&>().caption(
        std::declval<const erui::StyledText&>())) &&
    noexcept(std::declval<T&>().caption(
        std::declval<erui::StyledText&&>())) &&
    noexcept(std::declval<T&&>().caption(
        std::declval<erui::StyledText&&>()));

using ButtonReturn = decltype(std::declval<erui::Page&>().add_button(
    std::declval<std::wstring_view>(),
    std::declval<std::wstring_view>(),
    std::declval<erui::ButtonCallback>()));
using StaticButtonReturn = decltype(
    std::declval<erui::Page&>().add_button<&button_action>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>()));
using ToggleReturn = decltype(std::declval<erui::Page&>().add_toggle(
    std::declval<std::wstring_view>(),
    std::declval<std::wstring_view>(),
    std::declval<std::uint8_t>()));
using SliderReturn = decltype(std::declval<erui::Page&>().add_slider(
    std::declval<std::wstring_view>(),
    std::declval<std::wstring_view>(),
    std::declval<const erui::SliderOptions&>()));
using NumericSliderReturn = decltype(
    std::declval<erui::Page&>().add_slider(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::NumericSliderOptions&>()));
using StaticNumericSliderReturn = decltype(
    std::declval<erui::Page&>().add_slider<&numeric_slider_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::NumericSliderOptions&>()));
using StatefulNumericSliderReturn = decltype(
    std::declval<erui::Page&>().add_slider<
        &stateful_numeric_slider_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::NumericSliderOptions&>(),
        std::declval<unsigned&>()));
using CustomSliderReturn = decltype(
    std::declval<erui::Page&>().add_custom_slider(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::CustomSliderOptions&>()));
using StaticCustomSliderReturn = decltype(
    std::declval<erui::Page&>().add_custom_slider<&custom_slider_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::CustomSliderOptions&>()));
using StatefulCustomSliderReturn = decltype(
    std::declval<erui::Page&>().add_custom_slider<
        &stateful_custom_slider_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::CustomSliderOptions&>(),
        std::declval<unsigned&>()));
using InlineChoiceReturn = decltype(
    std::declval<erui::Page&>().add_inline_choice(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::ChoiceOptions&>()));
using PopupChoiceReturn = decltype(
    std::declval<erui::Page&>().add_popup_choice(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::ChoiceOptions&>()));
using TextInputReturn = decltype(
    std::declval<erui::Page&>().add_text_input(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::TextInputOptions&>()));
using StaticTextInputReturn = decltype(
    std::declval<erui::Page&>().add_text_input<&text_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::TextInputOptions&>()));
using StatefulTextInputReturn = decltype(
    std::declval<erui::Page&>().add_text_input<&stateful_text_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::TextInputOptions&>(),
        std::declval<unsigned&>()));
using ColorPickerReturn = decltype(
    std::declval<erui::Page&>().add_color_picker(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::ColorPickerOptions&>()));
using StaticColorPickerReturn = decltype(
    std::declval<erui::Page&>().add_color_picker<&color_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::ColorPickerOptions&>()));
using StatefulColorPickerReturn = decltype(
    std::declval<erui::Page&>().add_color_picker<&stateful_color_changed>(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>(),
        std::declval<const erui::ColorPickerOptions&>(),
        std::declval<unsigned&>()));

static_assert(std::is_same_v<ButtonReturn, erui::ButtonRow>);
static_assert(std::is_same_v<StaticButtonReturn, erui::ButtonRow>);
static_assert(std::is_same_v<ToggleReturn, erui::ToggleRow>);
static_assert(std::is_same_v<SliderReturn, erui::SliderRow>);
static_assert(std::is_same_v<NumericSliderReturn, erui::NumericSliderRow>);
static_assert(std::is_same_v<
    StaticNumericSliderReturn, erui::NumericSliderRow>);
static_assert(std::is_same_v<
    StatefulNumericSliderReturn, erui::NumericSliderRow>);
static_assert(std::is_same_v<CustomSliderReturn, erui::CustomSliderRow>);
static_assert(std::is_same_v<
    StaticCustomSliderReturn, erui::CustomSliderRow>);
static_assert(std::is_same_v<
    StatefulCustomSliderReturn, erui::CustomSliderRow>);
static_assert(std::is_same_v<InlineChoiceReturn, erui::InlineChoiceRow>);
static_assert(std::is_same_v<PopupChoiceReturn, erui::PopupChoiceRow>);
static_assert(std::is_same_v<TextInputReturn, erui::TextInputRow>);
static_assert(std::is_same_v<StaticTextInputReturn, erui::TextInputRow>);
static_assert(std::is_same_v<StatefulTextInputReturn, erui::TextInputRow>);
static_assert(std::is_same_v<ColorPickerReturn, erui::ColorPickerRow>);
static_assert(std::is_same_v<StaticColorPickerReturn, erui::ColorPickerRow>);
static_assert(std::is_same_v<
    StatefulColorPickerReturn, erui::ColorPickerRow>);
static_assert(std::is_same_v<
    decltype(std::declval<erui::Page&>().add_submenu(
        std::declval<std::wstring_view>(),
        std::declval<std::wstring_view>())),
    erui::Page>);

static_assert(!std::is_polymorphic_v<erui::RowRef>);
static_assert(!std::has_virtual_destructor_v<erui::RowRef>);
static_assert(sizeof(erui::RowRef) <= sizeof(void*) * 3u);
static_assert(typed_row_value_contract<erui::ButtonRow>);
static_assert(typed_row_value_contract<erui::ToggleRow>);
static_assert(typed_row_value_contract<erui::SliderRow>);
static_assert(typed_row_value_contract<erui::NumericSliderRow>);
static_assert(typed_row_value_contract<erui::CustomSliderRow>);
static_assert(typed_row_value_contract<erui::InlineChoiceRow>);
static_assert(typed_row_value_contract<erui::PopupChoiceRow>);
static_assert(typed_row_value_contract<erui::TextInputRow>);
static_assert(typed_row_value_contract<erui::ColorPickerRow>);
static_assert(no_cross_row_conversions<
    erui::ButtonRow,
    erui::ToggleRow,
    erui::SliderRow,
    erui::NumericSliderRow,
    erui::CustomSliderRow,
    erui::InlineChoiceRow,
    erui::PopupChoiceRow,
    erui::TextInputRow,
    erui::ColorPickerRow>::value);

static_assert(has_edit<erui::ButtonRow>::value);
static_assert(has_edit<erui::ToggleRow>::value);
static_assert(has_edit<erui::SliderRow>::value);
static_assert(has_edit<erui::NumericSliderRow>::value);
static_assert(has_edit<erui::CustomSliderRow>::value);
static_assert(has_edit<erui::InlineChoiceRow>::value);
static_assert(has_edit<erui::PopupChoiceRow>::value);
static_assert(has_edit<erui::TextInputRow>::value);
static_assert(has_edit<erui::ColorPickerRow>::value);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::ButtonRow&>().edit()),
    erui::ButtonRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::ToggleRow&>().edit()),
    erui::ToggleRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::SliderRow&>().edit()),
    erui::SliderRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::NumericSliderRow&>().edit()),
    erui::NumericSliderRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::CustomSliderRow&>().edit()),
    erui::CustomSliderRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::InlineChoiceRow&>().edit()),
    erui::InlineChoiceRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::PopupChoiceRow&>().edit()),
    erui::PopupChoiceRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::TextInputRow&>().edit()),
    erui::TextInputRowEdit>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::ColorPickerRow&>().edit()),
    erui::ColorPickerRowEdit>);
static_assert(row_edit_value_contract<erui::ButtonRowEdit>);
static_assert(row_edit_value_contract<erui::ToggleRowEdit>);
static_assert(row_edit_value_contract<erui::SliderRowEdit>);
static_assert(row_edit_value_contract<erui::NumericSliderRowEdit>);
static_assert(row_edit_value_contract<erui::CustomSliderRowEdit>);
static_assert(row_edit_value_contract<erui::InlineChoiceRowEdit>);
static_assert(row_edit_value_contract<erui::PopupChoiceRowEdit>);
static_assert(row_edit_value_contract<erui::TextInputRowEdit>);
static_assert(row_edit_value_contract<erui::ColorPickerRowEdit>);
static_assert(has_caption<erui::ButtonRowEdit>::value);
static_assert(!has_caption<erui::ToggleRowEdit>::value);
static_assert(!has_caption<erui::SliderRowEdit>::value);
static_assert(!has_caption<erui::NumericSliderRowEdit>::value);
static_assert(!has_caption<erui::CustomSliderRowEdit>::value);
static_assert(!has_caption<erui::InlineChoiceRowEdit>::value);
static_assert(has_caption<erui::PopupChoiceRowEdit>::value);
static_assert(!has_caption<erui::TextInputRowEdit>::value);
static_assert(!has_caption<erui::ColorPickerRowEdit>::value);
static_assert(caption_edit_value_contract<erui::ButtonRowEdit>);
static_assert(caption_edit_value_contract<erui::PopupChoiceRowEdit>);
static_assert(!has_caption_template_bypass<erui::ButtonRowEdit>::value);
static_assert(!has_caption_template_bypass<erui::ToggleRowEdit>::value);
static_assert(!has_caption_template_bypass<erui::SliderRowEdit>::value);
static_assert(!has_caption_template_bypass<
    erui::NumericSliderRowEdit>::value);
static_assert(!has_caption_template_bypass<
    erui::CustomSliderRowEdit>::value);
static_assert(!has_caption_template_bypass<
    erui::InlineChoiceRowEdit>::value);
static_assert(!has_caption_template_bypass<
    erui::PopupChoiceRowEdit>::value);
static_assert(!has_caption_template_bypass<erui::TextInputRowEdit>::value);
static_assert(!has_caption_template_bypass<
    erui::ColorPickerRowEdit>::value);

static_assert(!has_set_value<erui::ButtonRow>::value);
static_assert(!has_get_value<erui::ButtonRow>::value);
static_assert(!has_set_index<erui::ButtonRow>::value);
static_assert(!has_get_index<erui::ButtonRow>::value);
static_assert(!has_set_text<erui::ButtonRow>::value);
static_assert(!has_get_text<erui::ButtonRow>::value);
static_assert(!has_set_color<erui::ButtonRow>::value);
static_assert(!has_get_color<erui::ButtonRow>::value);
static_assert(has_set_value<erui::ToggleRow>::value);
static_assert(has_get_value<erui::ToggleRow>::value);
static_assert(!has_get_index<erui::ToggleRow>::value);
static_assert(!has_get_text<erui::ToggleRow>::value);
static_assert(!has_get_color<erui::ToggleRow>::value);
static_assert(has_set_value<erui::SliderRow>::value);
static_assert(has_get_value<erui::SliderRow>::value);
static_assert(!has_set_index<erui::SliderRow>::value);
static_assert(!has_get_index<erui::SliderRow>::value);
static_assert(!has_set_text<erui::SliderRow>::value);
static_assert(!has_get_text<erui::SliderRow>::value);
static_assert(!has_set_color<erui::SliderRow>::value);
static_assert(!has_get_color<erui::SliderRow>::value);
static_assert(has_set_value<erui::NumericSliderRow>::value);
static_assert(has_get_value<erui::NumericSliderRow>::value);
static_assert(!has_set_index<erui::NumericSliderRow>::value);
static_assert(!has_get_index<erui::NumericSliderRow>::value);
static_assert(!has_set_text<erui::NumericSliderRow>::value);
static_assert(!has_get_text<erui::NumericSliderRow>::value);
static_assert(!has_set_color<erui::NumericSliderRow>::value);
static_assert(!has_get_color<erui::NumericSliderRow>::value);
static_assert(!has_set_index<erui::ToggleRow>::value);
static_assert(!has_set_text<erui::ToggleRow>::value);
static_assert(!has_set_color<erui::ToggleRow>::value);
static_assert(has_set_index<erui::CustomSliderRow>::value);
static_assert(has_get_index<erui::CustomSliderRow>::value);
static_assert(!has_set_text<erui::CustomSliderRow>::value);
static_assert(!has_get_text<erui::CustomSliderRow>::value);
static_assert(!has_set_color<erui::CustomSliderRow>::value);
static_assert(!has_get_color<erui::CustomSliderRow>::value);
static_assert(has_set_index<erui::InlineChoiceRow>::value);
static_assert(has_get_index<erui::InlineChoiceRow>::value);
static_assert(!has_set_text<erui::InlineChoiceRow>::value);
static_assert(!has_get_text<erui::InlineChoiceRow>::value);
static_assert(!has_set_color<erui::InlineChoiceRow>::value);
static_assert(!has_get_color<erui::InlineChoiceRow>::value);
static_assert(has_set_index<erui::PopupChoiceRow>::value);
static_assert(has_get_index<erui::PopupChoiceRow>::value);
static_assert(!has_set_text<erui::PopupChoiceRow>::value);
static_assert(!has_get_text<erui::PopupChoiceRow>::value);
static_assert(!has_set_color<erui::PopupChoiceRow>::value);
static_assert(!has_get_color<erui::PopupChoiceRow>::value);
static_assert(has_set_text<erui::TextInputRow>::value);
static_assert(has_get_text<erui::TextInputRow>::value);
static_assert(!has_set_index<erui::TextInputRow>::value);
static_assert(!has_get_index<erui::TextInputRow>::value);
static_assert(!has_set_color<erui::TextInputRow>::value);
static_assert(!has_get_color<erui::TextInputRow>::value);
static_assert(has_set_color<erui::ColorPickerRow>::value);
static_assert(has_get_color<erui::ColorPickerRow>::value);
static_assert(!has_set_index<erui::ColorPickerRow>::value);
static_assert(!has_get_index<erui::ColorPickerRow>::value);
static_assert(!has_set_text<erui::ColorPickerRow>::value);
static_assert(!has_get_text<erui::ColorPickerRow>::value);
static_assert(!has_set_value<erui::CustomSliderRow>::value);
static_assert(!has_get_value<erui::CustomSliderRow>::value);
static_assert(!has_set_value<erui::InlineChoiceRow>::value);
static_assert(!has_get_value<erui::InlineChoiceRow>::value);
static_assert(!has_set_value<erui::PopupChoiceRow>::value);
static_assert(!has_get_value<erui::PopupChoiceRow>::value);
static_assert(!has_set_value<erui::TextInputRow>::value);
static_assert(!has_get_value<erui::TextInputRow>::value);
static_assert(!has_set_value<erui::ColorPickerRow>::value);
static_assert(!has_get_value<erui::ColorPickerRow>::value);

static_assert(std::is_same_v<
    decltype(&erui::ToggleRow::set_value),
    ERUI_Result (erui::ToggleRow::*)(bool) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::ToggleRow::get_value),
    ERUI_Result (erui::ToggleRow::*)(bool&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::SliderRow::set_value),
    ERUI_Result (erui::SliderRow::*)(std::uint8_t) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::SliderRow::get_value),
    ERUI_Result (erui::SliderRow::*)(std::uint8_t&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::NumericSliderRow::set_value),
    ERUI_Result (erui::NumericSliderRow::*)(double) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::NumericSliderRow::get_value),
    ERUI_Result (erui::NumericSliderRow::*)(double&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::CustomSliderRow::set_index),
    ERUI_Result (erui::CustomSliderRow::*)(std::uint32_t) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::CustomSliderRow::get_index),
    ERUI_Result (erui::CustomSliderRow::*)(std::uint32_t&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::InlineChoiceRow::set_index),
    ERUI_Result (erui::InlineChoiceRow::*)(std::uint8_t) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::InlineChoiceRow::get_index),
    ERUI_Result (erui::InlineChoiceRow::*)(std::uint8_t&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::PopupChoiceRow::set_index),
    ERUI_Result (erui::PopupChoiceRow::*)(std::uint8_t) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::PopupChoiceRow::get_index),
    ERUI_Result (erui::PopupChoiceRow::*)(std::uint8_t&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::TextInputRow::set_text),
    ERUI_Result (erui::TextInputRow::*)(std::wstring_view) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::TextInputRow::get_text),
    ERUI_Result (erui::TextInputRow::*)(std::wstring&) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::ColorPickerRow::set_color),
    ERUI_Result (erui::ColorPickerRow::*)(erui::Color) const noexcept>);
static_assert(std::is_same_v<
    decltype(&erui::ColorPickerRow::get_color),
    ERUI_Result (erui::ColorPickerRow::*)(erui::Color&) const noexcept>);

static_assert(std::is_same_v<
    decltype(std::declval<const erui::ToggleRow&>() ==
        std::declval<erui::RowHandle>()),
    bool>);
static_assert(std::is_same_v<
    decltype(std::declval<erui::RowHandle>() ==
        std::declval<const erui::ToggleRow&>()),
    bool>);
static_assert(std::is_same_v<
    decltype(std::declval<const erui::ToggleRow&>() !=
        std::declval<erui::RowHandle>()),
    bool>);
static_assert(std::is_same_v<
    decltype(std::declval<erui::RowHandle>() !=
        std::declval<const erui::ToggleRow&>()),
    bool>);

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
#if defined(ERUI_API_VERSION_1_4)
static_assert(sizeof(ERUI_TextStyleSpan) == 32);
static_assert(sizeof(ERUI_StyledTextView) == 48);
static_assert(sizeof(ERUI_RowUpdateDesc) == 176);
static_assert(sizeof(ERUI_UpdateDesc) == 24);
static_assert(sizeof(ERUI_UpdateBatchDesc) == 32);
static_assert(std::is_trivially_copyable_v<ERUI_TextStyleSpan>);
static_assert(std::is_trivially_copyable_v<ERUI_StyledTextView>);
static_assert(std::is_trivially_copyable_v<ERUI_RowUpdateDesc>);
static_assert(std::is_trivially_copyable_v<ERUI_UpdateDesc>);
static_assert(std::is_trivially_copyable_v<ERUI_UpdateBatchDesc>);
static_assert(static_cast<ERUI_Capabilities>(erui::Capability::updates) ==
    ERUI_CAP_UPDATES);
static_assert(static_cast<ERUI_Capabilities>(
    erui::Capability::row_availability) == ERUI_CAP_ROW_AVAILABILITY);
static_assert(static_cast<ERUI_AvailabilityAppearance>(
    erui::AvailabilityAppearance::follow_interactivity) ==
    ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY);
static_assert(static_cast<ERUI_AvailabilityAppearance>(
    erui::AvailabilityAppearance::normal) ==
    ERUI_AVAILABILITY_APPEARANCE_NORMAL);
static_assert(static_cast<ERUI_AvailabilityAppearance>(
    erui::AvailabilityAppearance::disabled) ==
    ERUI_AVAILABILITY_APPEARANCE_DISABLED);
static_assert(has_interactive<erui::ToggleRowEdit>::value);
static_assert(has_availability_appearance<erui::ToggleRowEdit>::value);
static_assert(has_interactive<erui::ButtonRowEdit>::value);
static_assert(has_interactive<erui::SliderRowEdit>::value);
static_assert(has_interactive<erui::NumericSliderRowEdit>::value);
static_assert(has_interactive<erui::CustomSliderRowEdit>::value);
static_assert(has_interactive<erui::InlineChoiceRowEdit>::value);
static_assert(has_interactive<erui::PopupChoiceRowEdit>::value);
static_assert(has_interactive<erui::TextInputRowEdit>::value);
static_assert(has_interactive<erui::ColorPickerRowEdit>::value);
static_assert(has_availability_appearance<erui::ButtonRowEdit>::value);
static_assert(has_availability_appearance<erui::SliderRowEdit>::value);
static_assert(has_availability_appearance<
    erui::NumericSliderRowEdit>::value);
static_assert(has_availability_appearance<
    erui::CustomSliderRowEdit>::value);
static_assert(has_availability_appearance<
    erui::InlineChoiceRowEdit>::value);
static_assert(has_availability_appearance<
    erui::PopupChoiceRowEdit>::value);
static_assert(has_availability_appearance<
    erui::TextInputRowEdit>::value);
static_assert(has_availability_appearance<
    erui::ColorPickerRowEdit>::value);
static_assert(std::is_same_v<decltype(
    std::declval<erui::ToggleRowEdit&>().interactive(true)),
    erui::ToggleRowEdit&>);
static_assert(std::is_same_v<decltype(
    std::declval<erui::ToggleRowEdit&&>().interactive(false)),
    erui::ToggleRowEdit&&>);
static_assert(std::is_same_v<decltype(
    std::declval<erui::ButtonRowEdit&>().interactive(false)),
    erui::ButtonRowEdit&>);
static_assert(std::is_same_v<decltype(
    std::declval<erui::ColorPickerRowEdit&&>().availability_appearance(
        erui::AvailabilityAppearance::normal)),
    erui::ColorPickerRowEdit&&>);
static_assert(std::is_same_v<decltype(
    std::declval<erui::ToggleRowEdit&>().availability_appearance(
        erui::AvailabilityAppearance::disabled)),
    erui::ToggleRowEdit&>);
static_assert(std::is_same_v<decltype(
    std::declval<erui::ToggleRowEdit&&>().availability_appearance(
        erui::AvailabilityAppearance::normal)),
    erui::ToggleRowEdit&&>);
static_assert(std::is_same_v<decltype(std::declval<erui::TextStyle&>().color(
    std::declval<erui::Color>())), erui::TextStyle&>);
static_assert(std::is_same_v<decltype(erui::TextStyle{}.bold()),
    erui::TextStyle&&>);
static_assert(std::is_same_v<decltype(erui::TextStyle{}.italic()),
    erui::TextStyle&&>);
static_assert(std::is_same_v<decltype(erui::TextStyle{}.underline()),
    erui::TextStyle&&>);
static_assert(std::is_same_v<decltype(std::declval<erui::StyledText&>().append(
    std::declval<std::wstring_view>())), erui::StyledText&>);
static_assert(std::is_same_v<decltype(erui::StyledText{}.append(
    std::declval<std::wstring_view>(),
    std::declval<erui::TextStyle>())), erui::StyledText&&>);
static_assert(ERUI_API_V1_4_SIZE == 392u);
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
#if defined(ERUI_API_VERSION_1_4)
    erui::StyledText styled{};
    styled.append(L"Plain ")
        .append(
            L"gold",
            erui::TextStyle{}
                .color(erui::Color{107u, 100u, 80u})
                .underline())
        .append(L" and ")
        .append(L"bold italic", erui::TextStyle{}.bold().italic());
    if (styled.text() != L"Plain gold and bold italic") return 36;
    if (styled.span_count() != 2u) return 37;
    const erui::TextStyle underline = erui::TextStyle{}.underline();
    erui::StyledText merged{};
    merged.append(L"adjacent", underline).append(L" spans", underline);
    if (merged.text() != L"adjacent spans" || merged.span_count() != 1u) {
        return 38;
    }
    if (ERUI_API_V1_4_SIZE != 392u) return 39;
#endif
#endif
#endif
#endif
    return 0;
}
