#include <ernativeui/ERNativeUI.hpp>

#include "localization.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace {

HMODULE g_module{};
std::atomic<std::uint8_t> g_enabled{1};
std::atomic<std::uint8_t> g_intensity{50};
std::atomic<std::uint8_t> g_label_help_variant{0};
std::atomic<std::uint8_t> g_inline_preset{1};
std::atomic<std::uint8_t> g_popup_preset{1};
std::atomic<std::uint8_t> g_popup_binary{0};
std::atomic<std::uint8_t> g_popup_extended{3};
std::array<std::uint32_t, 32> g_action_numbers{};
erui::Registration g_registration{};
erui::ToggleRow g_enabled_row{};
erui::SliderRow g_intensity_row{};
erui::ButtonRow g_root_label_help_button{};

struct LabelHelpRows {
    erui::ButtonRow button{};
    erui::ToggleRow toggle{};
    erui::SliderRow slider{};
    erui::NumericSliderRow numeric_slider{};
    erui::CustomSliderRow custom_slider{};
    erui::InlineChoiceRow inline_choice{};
    erui::PopupChoiceRow popup_choice{};
    erui::TextInputRow text_input{};
    erui::ColorPickerRow color_picker{};
};

LabelHelpRows g_label_help_rows{};
std::atomic<std::uint8_t> g_label_help_toggle{1};
std::atomic<std::uint8_t> g_label_help_slider{5};
std::atomic<std::uint8_t> g_label_help_inline_choice{0};
std::atomic<std::uint8_t> g_label_help_popup_choice{1};
std::atomic<std::uint8_t> g_availability_controller{1};
std::atomic<std::uint8_t> g_availability_appearance{0};
std::atomic<std::uint8_t> g_availability_target_value{1};
std::atomic<std::uint8_t> g_initially_disabled_controller{0};
std::atomic<std::uint8_t> g_initially_disabled_value{1};
std::atomic<std::uint8_t> g_self_disabling_value{0};
erui::ToggleRow g_availability_target{};
erui::ToggleRow g_initially_disabled_target{};
erui::ToggleRow g_self_disabling_target{};

struct AvailabilityMatrixRows {
    erui::SliderRow intensity{};
    erui::ButtonRow button{};
    erui::ToggleRow toggle{};
    erui::SliderRow slider{};
    erui::NumericSliderRow numeric_slider{};
    erui::CustomSliderRow custom_slider{};
    erui::InlineChoiceRow inline_choice{};
    erui::PopupChoiceRow popup_choice{};
    erui::TextInputRow text_input{};
    erui::ColorPickerRow color_picker{};
};

AvailabilityMatrixRows g_availability_matrix_rows{};
std::atomic<std::uint8_t> g_availability_matrix_disabled{0};
std::atomic<std::uint8_t> g_availability_matrix_intensity{50};
std::atomic<std::uint8_t> g_availability_matrix_toggle{1};
std::atomic<std::uint8_t> g_availability_matrix_slider{5};
std::atomic<std::uint8_t> g_availability_matrix_inline_choice{0};
std::atomic<std::uint8_t> g_availability_matrix_popup_choice{1};
constexpr std::array<std::wstring_view, 3> kLabelHelpChoiceValues{
    L"VALUE ZERO", L"VALUE ONE", L"VALUE TWO"};
constexpr std::array<std::wstring_view, 3> kAvailabilityAppearanceValues{
    L"Follow", L"Normal", L"Disabled"};

struct ShowcaseActionState {
    explicit ShowcaseActionState(const wchar_t* name) noexcept
        : debug_name(name) {}

    const wchar_t* debug_name{};
    std::atomic_uint32_t activation_count{};
};

// Binding callback state is client-owned and must retain a stable address for
// the process lifetime. Static storage makes that contract explicit here.
ShowcaseActionState g_native_dialog_action{L"show-native-ok-dialog"};
ShowcaseActionState g_toggle_enabled_action{L"toggle-showcase-enabled"};
ShowcaseActionState g_controller_dialog_action{L"controller-native-dialog"};

struct ColorPickerState {
    std::mutex mutex{};
    erui::Color value{};
};

ColorPickerState g_root_color{{}, {171u, 125u, 99u}};
ColorPickerState g_subpage_color{{}, {90u, 174u, 63u}};
ColorPickerState g_label_help_color{{}, {107u, 100u, 80u}};
ColorPickerState g_availability_matrix_color{{}, {107u, 100u, 80u}};

struct TextInputState {
    std::mutex mutex{};
    std::wstring value{};
};

TextInputState g_player_note{};
TextInputState g_extended_note{};
TextInputState g_label_help_text{};
TextInputState g_availability_matrix_text{};

constexpr std::size_t kVisibilityFillerCount = 8;

struct VisibilitySurface {
    explicit VisibilitySurface(const wchar_t* name) noexcept
        : debug_name(name) {}

    const wchar_t* debug_name{};
    std::atomic<std::uint8_t> layout{0};
    std::atomic<std::uint8_t> toggle{1};
    std::atomic<std::uint8_t> slider{5};
    std::atomic<std::uint8_t> inline_choice{1};
    std::atomic<std::uint8_t> popup_choice{1};
    TextInputState text_input{};
    ColorPickerState color_picker{};
    erui::ButtonRow sentinel{};
    erui::ButtonRow button{};
    erui::ToggleRow toggle_row{};
    erui::SliderRow slider_row{};
    erui::NumericSliderRow numeric_slider{};
    erui::CustomSliderRow custom_slider{};
    erui::InlineChoiceRow inline_choice_row{};
    erui::PopupChoiceRow popup_choice_row{};
    erui::TextInputRow text_input_row{};
    erui::ColorPickerRow color_picker_row{};
    std::array<erui::ButtonRow, kVisibilityFillerCount> fillers{};
};

VisibilitySurface g_root_visibility{L"Game Options"};
VisibilitySurface g_tab_visibility{L"Reactive Layout tab"};
VisibilitySurface g_subpage_visibility{L"Reactive Layout subpage"};

struct PopupVariant {
    std::wstring label{};
    std::wstring help{};
    std::wstring message{};
    erui::AlertOptions options{};
};

constexpr erui::AlertOptions popup_options(
    erui::AlertPlacement placement,
    erui::AlertButtons buttons) noexcept {
    erui::AlertOptions options{};
    options.buttons = buttons;
    options.placement = placement;
    return options;
}

std::array<PopupVariant, 14> g_popup_variants{};
const showcase::Text* g_text{};
const showcase::TextInputText* g_text_input{};
const showcase::ColorPickerText* g_color_picker_text{};
const showcase::SliderText* g_slider_text{};

void initialize_popup_variants(const showcase::Text& text) {
    constexpr std::array<erui::AlertButtons, 7> buttons{
        erui::AlertButtons::dismiss_only, erui::AlertButtons::ok,
        erui::AlertButtons::cancel, erui::AlertButtons::yes,
        erui::AlertButtons::no, erui::AlertButtons::ok_cancel,
        erui::AlertButtons::yes_no};
    constexpr std::array<const wchar_t*, 7> names{
        nullptr, L"OK", L"CANCEL", L"YES", L"NO",
        L"OK + CANCEL", L"YES + NO"};
    for (std::size_t placement = 0; placement < 2; ++placement) {
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            PopupVariant& variant = g_popup_variants[placement * buttons.size() + index];
            variant.label = placement == 0 ? text.bottom : text.center;
            variant.label += L": ";
            variant.label += index == 0 ? text.dismiss_only : names[index];
            variant.help = text.popup_help;
            variant.message = text.popup_message;
            variant.options = popup_options(
                placement == 0 ? erui::AlertPlacement::bottom
                               : erui::AlertPlacement::center,
                buttons[index]);
        }
    }
}

void debug(const wchar_t* message) noexcept {
    OutputDebugStringW(L"[TarnishedUIShowcase] ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

void record_binding_activation(
    ShowcaseActionState& state,
    const erui::ActionActivation& activation) noexcept {
    const std::uint32_t count =
        state.activation_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    wchar_t message[192]{};
    _snwprintf_s(
        message,
        std::size(message),
        _TRUNCATE,
        L"Input binding %ls activated (%u).",
        state.debug_name,
        static_cast<unsigned>(count));
    debug(message);

    // The mask may contain more than one device when alternatives become
    // active in the same sampled frame.
    if (activation.includes(erui::InputDevice::controller)) {
        debug(L"  device: controller");
    }
    if (activation.includes(erui::InputDevice::keyboard)) {
        debug(L"  device: keyboard");
    }
    if (activation.includes(erui::InputDevice::mouse)) {
        debug(L"  device: mouse");
    }
}

void ERUI_CALL value_changed(void* context, std::uint8_t value) noexcept {
    static_cast<std::atomic<std::uint8_t>*>(context)->store(
        value, std::memory_order_release);
}

constexpr erui::Color intensity_color(std::uint8_t value) noexcept {
    // Keep both endpoints readable against Elden Ring's dark settings movie:
    // 0 is warm red, 100 is bright green, and intermediate values are a
    // linear RGB interpolation between them.
    const auto progress = static_cast<std::uint32_t>(value);
    const auto red = 255u - (191u * progress / 100u);
    const auto green = 64u + (191u * progress / 100u);
    return {
        static_cast<std::uint8_t>(red),
        static_cast<std::uint8_t>(green),
        64u};
}

ERUI_Result commit_intensity_presentation(std::uint8_t value) noexcept {
    wchar_t percentage[16]{};
    _snwprintf_s(
        percentage,
        std::size(percentage),
        _TRUNCATE,
        L"%u%%",
        static_cast<unsigned>(value));

    const erui::TextStyle percentage_style =
        erui::TextStyle{}.color(intensity_color(value));
    erui::StyledText label{};
    label.append(g_text ? g_text->intensity : L"Intensity")
        .append(L" (")
        .append(percentage, percentage_style)
        .append(L")");

    erui::StyledText help{};
    help.append(L"API 1.4 updated this label and help text at runtime. Current intensity: ")
        .append(percentage, percentage_style)
        .append(L".");

    return g_intensity_row.edit()
        .label(std::move(label))
        .help(std::move(help))
        .commit();
}

ERUI_Result commit_availability_matrix_intensity_presentation(
    std::uint8_t value) noexcept {
    if (!g_availability_matrix_rows.intensity.valid()) return ERUI_OK;

    wchar_t percentage[16]{};
    _snwprintf_s(
        percentage,
        std::size(percentage),
        _TRUNCATE,
        L"%u%%",
        static_cast<unsigned>(value));

    const erui::TextStyle percentage_style =
        erui::TextStyle{}.color(intensity_color(value));
    erui::StyledText label{};
    label.append(g_text ? g_text->intensity : L"Intensity")
        .append(L" (")
        .append(percentage, percentage_style)
        .append(L")");

    erui::StyledText help{};
    help.append(L"This styled availability target must restore its original ")
        .append(percentage, percentage_style)
        .append(L" color after being disabled and re-enabled.");

    return g_availability_matrix_rows.intensity.edit()
        .label(std::move(label))
        .help(std::move(help))
        .commit();
}

erui::StyledText make_variant_label(
    std::wstring_view row_type,
    std::uint8_t variant) noexcept {
    const bool alternate = (variant & 1u) != 0u;
    const std::wstring_view marker = alternate ? L"[B]" : L"[A]";
    const erui::TextStyle label_style = erui::TextStyle{}
        .color(alternate
            ? erui::Color{96u, 220u, 128u}
            : erui::Color{218u, 185u, 98u})
        .underline();
    erui::StyledText label{};
    label.append(row_type)
        .append(L" ROW ")
        .append(marker, label_style);
    return label;
}

erui::StyledText make_variant_help(
    std::wstring_view row_type,
    std::uint8_t variant) noexcept {
    const bool alternate = (variant & 1u) != 0u;
    const std::wstring_view marker = alternate ? L"[B]" : L"[A]";
    const erui::TextStyle help_style = erui::TextStyle{}
        .color(alternate
            ? erui::Color{96u, 180u, 255u}
            : erui::Color{255u, 96u, 96u})
        .underline();

    erui::StyledText help{};
    help.append(L"Variant ")
        .append(marker, help_style)
        .append(L" - ")
        .append(row_type)
        .append(L" help text.");
    return help;
}

erui::StyledText make_variant_caption(
    std::wstring_view prefix,
    std::uint8_t variant) noexcept {
    const bool alternate = (variant & 1u) != 0u;
    const std::wstring_view marker = alternate ? L"[B]" : L"[A]";
    const erui::TextStyle caption_style = erui::TextStyle{}
        .color(alternate
            ? erui::Color{96u, 220u, 128u}
            : erui::Color{218u, 185u, 98u})
        .underline();

    erui::StyledText caption{};
    caption.append(prefix)
        .append(L" ")
        .append(marker, caption_style);
    return caption;
}

template <typename Row>
ERUI_Result commit_label_help_row(
    const Row& row,
    std::wstring_view row_type,
    std::uint8_t variant) noexcept {
    if (!row.valid()) return ERUI_INVALID_HANDLE;

    return row.edit()
        .label(make_variant_label(row_type, variant))
        .help(make_variant_help(row_type, variant))
        .commit();
}

ERUI_Result commit_root_button_presentation(
    std::uint8_t variant) noexcept {
    if (!g_root_label_help_button.valid()) return ERUI_INVALID_HANDLE;
    return g_root_label_help_button.edit()
        .label(make_variant_label(L"ROOT BUTTON", variant))
        // This deliberately exercises the literal-text caption overload.
        .caption((variant & 1u) != 0u
            ? L"ROOT CAPTION B"
            : L"ROOT CAPTION A")
        .help(make_variant_help(L"ROOT BUTTON", variant))
        .commit();
}

ERUI_Result commit_button_presentation(std::uint8_t variant) noexcept {
    if (!g_label_help_rows.button.valid()) return ERUI_INVALID_HANDLE;
    return g_label_help_rows.button.edit()
        .label(make_variant_label(L"BUTTON", variant))
        .caption(make_variant_caption(L"BUTTON CAPTION", variant))
        .help(make_variant_help(L"BUTTON", variant))
        .commit();
}

ERUI_Result commit_popup_choice_presentation(
    std::uint8_t variant,
    std::uint8_t selected_index) noexcept {
    if (!g_label_help_rows.popup_choice.valid()) {
        return ERUI_INVALID_HANDLE;
    }
    if (selected_index >= kLabelHelpChoiceValues.size()) {
        return ERUI_INVALID_ARGUMENT;
    }
    return g_label_help_rows.popup_choice.edit()
        .label(make_variant_label(L"POPUP CHOICE", variant))
        // The caption is presentation only. The native selected index and
        // the three entries shown by the popup remain independent.
        .caption(make_variant_caption(
            kLabelHelpChoiceValues[selected_index], variant))
        .help(make_variant_help(L"POPUP CHOICE", variant))
        .commit();
}

ERUI_Result commit_label_help_showcase(std::uint8_t variant) noexcept {
    ERUI_Result first_failure = ERUI_OK;
    const auto record = [&](ERUI_Result result, const wchar_t* row_type) {
        if (result == ERUI_OK) return;
        if (first_failure == ERUI_OK) first_failure = result;

        wchar_t message[160]{};
        _snwprintf_s(
            message,
            std::size(message),
            _TRUNCATE,
            L"Presentation update failed for %ls (result=%u).",
            row_type,
            static_cast<unsigned>(result));
        debug(message);
    };
    const auto apply = [&](const auto& row, const wchar_t* row_type) {
        record(commit_label_help_row(row, row_type, variant), row_type);
    };

    record(commit_root_button_presentation(variant), L"ROOT BUTTON");
    record(commit_button_presentation(variant), L"BUTTON");
    apply(g_label_help_rows.toggle, L"TOGGLE");
    apply(g_label_help_rows.slider, L"BYTE SLIDER");
    apply(g_label_help_rows.numeric_slider, L"NUMERIC SLIDER");
    apply(g_label_help_rows.custom_slider, L"CUSTOM SLIDER");
    apply(g_label_help_rows.inline_choice, L"INLINE CHOICE");
    record(commit_popup_choice_presentation(
        variant,
        g_label_help_popup_choice.load(std::memory_order_acquire)),
        L"POPUP CHOICE");
    apply(g_label_help_rows.text_input, L"TEXT INPUT");
    apply(g_label_help_rows.color_picker, L"COLOR PICKER");
    return first_failure;
}

void toggle_label_help_showcase() noexcept {
    const std::uint8_t next = static_cast<std::uint8_t>(
        (g_label_help_variant.fetch_xor(1u, std::memory_order_acq_rel) ^ 1u) &
        1u);
    if (commit_label_help_showcase(next) != ERUI_OK) {
        debug(L"One or more presentation showcase edits were rejected.");
    }
}

void ERUI_CALL label_help_popup_choice_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (!g_registration.supports(erui::Capability::updates)) return;

    const ERUI_Result result = commit_popup_choice_presentation(
        g_label_help_variant.load(std::memory_order_acquire), value);
    if (result != ERUI_OK) {
        debug(L"Could not update the Popup Choice selected-value caption.");
    }
}

void ERUI_CALL intensity_changed(void* context, std::uint8_t value) noexcept {
    value_changed(context, value);
    if (!g_registration.supports(erui::Capability::updates)) return;

    if (commit_intensity_presentation(value) != ERUI_OK) {
        debug(L"Could not commit the API 1.4 intensity row edit.");
    }
}

void ERUI_CALL availability_matrix_intensity_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (!g_registration.supports(erui::Capability::updates)) return;

    if (commit_availability_matrix_intensity_presentation(value) != ERUI_OK) {
        debug(L"Could not update the availability-matrix intensity row.");
    }
}

erui::AvailabilityAppearance availability_appearance(
    std::uint8_t value) noexcept {
    switch (value) {
    case 1: return erui::AvailabilityAppearance::normal;
    case 2: return erui::AvailabilityAppearance::disabled;
    default: return erui::AvailabilityAppearance::follow_interactivity;
    }
}

ERUI_Result commit_availability_target() noexcept {
    return g_availability_target.edit()
        .interactive(
            g_availability_controller.load(std::memory_order_acquire) != 0)
        .availability_appearance(availability_appearance(
            g_availability_appearance.load(std::memory_order_acquire)))
        .commit();
}

void ERUI_CALL availability_interaction_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (commit_availability_target() != ERUI_OK) {
        debug(L"Could not update the availability target interaction.");
    }
}

void ERUI_CALL availability_appearance_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (commit_availability_target() != ERUI_OK) {
        debug(L"Could not update the availability target appearance.");
    }
}

void ERUI_CALL initially_disabled_interaction_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    const ERUI_Result result = g_initially_disabled_target.edit()
        .interactive(value != 0)
        .availability_appearance(
            erui::AvailabilityAppearance::follow_interactivity)
        .commit();
    if (result != ERUI_OK) {
        debug(L"Could not update the initially disabled Toggle.");
    }
}

void ERUI_CALL self_disabling_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (g_self_disabling_target.edit().interactive(false).commit() !=
        ERUI_OK) {
        debug(L"Could not disable the currently focused Toggle.");
    }
}

void reenable_self_disabling_target() noexcept {
    if (g_self_disabling_target.edit().interactive(true).commit() !=
        ERUI_OK) {
        debug(L"Could not re-enable the self-disabling Toggle.");
    }
}

template <typename Row>
ERUI_Result commit_matrix_availability(
    const Row& row,
    bool interactive) noexcept {
    if (!row.valid()) return ERUI_OK;
    return row.edit()
        .interactive(interactive)
        .availability_appearance(
            erui::AvailabilityAppearance::follow_interactivity)
        .commit();
}

ERUI_Result commit_availability_matrix(bool interactive) noexcept {
    ERUI_Result first_failure = ERUI_OK;
    const auto apply = [&](const auto& row) {
        const ERUI_Result result =
            commit_matrix_availability(row, interactive);
        if (first_failure == ERUI_OK && result != ERUI_OK) {
            first_failure = result;
        }
    };

    apply(g_availability_matrix_rows.intensity);
    apply(g_availability_matrix_rows.button);
    apply(g_availability_matrix_rows.toggle);
    apply(g_availability_matrix_rows.slider);
    apply(g_availability_matrix_rows.numeric_slider);
    apply(g_availability_matrix_rows.custom_slider);
    apply(g_availability_matrix_rows.inline_choice);
    apply(g_availability_matrix_rows.popup_choice);
    apply(g_availability_matrix_rows.text_input);
    apply(g_availability_matrix_rows.color_picker);
    return first_failure;
}

ERUI_Result commit_availability_matrix_popup_caption(
    std::uint8_t selected_index) noexcept {
    if (!g_availability_matrix_rows.popup_choice.valid()) {
        return ERUI_OK;
    }
    if (selected_index >= kLabelHelpChoiceValues.size()) {
        return ERUI_INVALID_ARGUMENT;
    }
    return g_availability_matrix_rows.popup_choice.edit()
        .label(L"Popup Choice target")
        .caption(kLabelHelpChoiceValues[selected_index])
        .commit();
}

void ERUI_CALL availability_matrix_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (commit_availability_matrix(value == 0u) != ERUI_OK) {
        debug(L"Could not update one or more availability-matrix rows.");
    }
}

void ERUI_CALL availability_matrix_popup_changed(
    void* context,
    std::uint8_t value) noexcept {
    value_changed(context, value);
    if (commit_availability_matrix_popup_caption(value) != ERUI_OK) {
        debug(L"Could not update the availability Popup Choice caption.");
    }
}

void availability_matrix_button_pressed() noexcept {
    debug(L"Availability-matrix Button activated.");
}

void numeric_slider_changed(
    const erui::NumericSliderChange& change) noexcept {
    wchar_t message[192]{};
    _snwprintf_s(
        message,
        std::size(message),
        _TRUNCATE,
        L"Numeric slider changed: value=%g scaled=%lld index=%u/%u.",
        change.value,
        static_cast<long long>(change.scaled_value),
        static_cast<unsigned>(change.index),
        static_cast<unsigned>(change.value_count));
    debug(message);
}

void custom_slider_changed(
    const erui::CustomSliderChange& change) noexcept {
    wchar_t message[128]{};
    _snwprintf_s(
        message,
        std::size(message),
        _TRUNCATE,
        L"Custom slider changed: index=%u/%u.",
        static_cast<unsigned>(change.index),
        static_cast<unsigned>(change.value_count));
    debug(message);
}

void text_input_changed(
    TextInputState& state,
    const erui::TextInputChange& change) noexcept {
    // TextInputChange::value is borrowed for this callback only. A real mod
    // should copy it into its own configuration before returning, as here.
    try {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.value.assign(change.value);
    } catch (...) {
        debug(L"Could not copy a confirmed TextInput value.");
    }
}

void ERUI_CALL action_pressed(void* context) noexcept {
    const auto number = context ? *static_cast<std::uint32_t*>(context) : 0u;
    wchar_t text[96]{};
    wsprintfW(text, L"Large Settings action %u was pressed.", number);
    debug(text);
}

const wchar_t* response_name(erui::AlertResponse response) noexcept {
    switch (response) {
    case erui::AlertResponse::none: return L"none";
    case erui::AlertResponse::primary: return L"primary";
    case erui::AlertResponse::secondary: return L"secondary";
    case erui::AlertResponse::dismissed: return L"dismissed";
    default: return L"unknown";
    }
}

const wchar_t* result_name(ERUI_Result result) noexcept {
    switch (result) {
    case ERUI_OK: return L"success";
    case ERUI_INVALID_ARGUMENT: return L"invalid argument";
    case ERUI_HOST_NOT_READY: return L"host not ready";
    case ERUI_HOST_FAILED: return L"host failed";
    case ERUI_UNSUPPORTED_VERSION: return L"unsupported API version";
    case ERUI_DUPLICATE_PROVIDER_ID: return L"duplicate provider ID";
    case ERUI_INVALID_HANDLE: return L"invalid handle";
    case ERUI_ALREADY_COMMITTED: return L"provider already committed";
    case ERUI_REGISTRATION_CLOSED: return L"registration closed";
    case ERUI_OUT_OF_MEMORY: return L"out of memory";
    case ERUI_CALLBACK_REJECTED: return L"callback rejected";
    case ERUI_INTERNAL_ERROR: return L"internal host error";
    case ERUI_QUEUE_FULL: return L"alert queue full";
    case ERUI_NOT_SUPPORTED: return L"not supported";
    default: return L"unknown result";
    }
}

void color_picker_changed(
    ColorPickerState& state,
    const erui::ColorPickerChange& change) noexcept {
    try {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.value = change.value;
    } catch (...) {
        debug(L"Could not persist a confirmed ColorPicker value.");
        return;
    }

    wchar_t message[128]{};
    _snwprintf_s(
        message,
        std::size(message),
        _TRUNCATE,
        L"ColorPicker confirmed RGB(%u, %u, %u).",
        static_cast<unsigned>(change.value.red),
        static_cast<unsigned>(change.value.green),
        static_cast<unsigned>(change.value.blue));
    debug(message);
}

const wchar_t* selected_action_name(
    erui::AlertButtons buttons,
    erui::AlertResponse response) noexcept {
    if (response == erui::AlertResponse::dismissed) return L"dismissed";
    if (response == erui::AlertResponse::none) return L"none";
    if (response == erui::AlertResponse::secondary) {
        switch (buttons) {
        case erui::AlertButtons::ok_cancel: return L"CANCEL";
        case erui::AlertButtons::yes_no: return L"NO";
        default: return L"unexpected secondary action";
        }
    }
    if (response != erui::AlertResponse::primary) return L"unknown";
    switch (buttons) {
    case erui::AlertButtons::ok:
    case erui::AlertButtons::ok_cancel:
        return L"OK";
    case erui::AlertButtons::cancel:
        return L"CANCEL";
    case erui::AlertButtons::yes:
    case erui::AlertButtons::yes_no:
        return L"YES";
    case erui::AlertButtons::no:
        return L"NO";
    case erui::AlertButtons::dismiss_only:
        return L"unexpected primary action";
    default:
        return L"unknown";
    }
}

void log_alert_completion(
    const wchar_t* label,
    erui::AlertButtons buttons,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    try {
        std::wstring text = L"Alert \"";
        text += label ? label : L"<unknown>";
        text += L"\" completed: result=";
        text += result_name(result);
        text += L", response=";
        text += response_name(response);
        text += L", action=";
        text += selected_action_name(buttons, response);
        text += L".";
        debug(text.c_str());
    } catch (...) {
        debug(L"An alert completed, but its detailed log could not be allocated.");
    }
}

void log_alert_rejection(
    const wchar_t* label,
    ERUI_Result result) noexcept {
    try {
        std::wstring text = L"Alert \"";
        text += label ? label : L"<unknown>";
        text += L"\" was rejected: result=";
        text += result_name(result);
        text += L".";
        debug(text.c_str());
    } catch (...) {
        debug(L"An alert was rejected, but its detailed log could not be allocated.");
    }
}

void ERUI_CALL alert_completed(
    void*,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    log_alert_completion(
        L"Normal OK Dialog", erui::AlertButtons::ok, result, response);
}

void show_native_alert() noexcept {
    const ERUI_Result result = g_registration.alert(
        g_text ? g_text->native_message : L"ERNativeUI",
        &alert_completed);
    if (result != ERUI_OK) {
        log_alert_rejection(L"Normal OK Dialog", result);
    }
}

void show_native_alert_from_binding(
    ShowcaseActionState& state,
    const erui::ActionActivation& activation) noexcept {
    record_binding_activation(state, activation);
    show_native_alert();
}

void ERUI_CALL popup_variant_completed(
    void* context,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    const auto* const variant = static_cast<const PopupVariant*>(context);
    log_alert_completion(
        variant ? variant->label.c_str() : L"Popup Variant",
        variant ? variant->options.buttons : erui::AlertButtons::ok,
        result,
        response);
}

void ERUI_CALL show_popup_variant(void* context) noexcept {
    auto* const variant = static_cast<PopupVariant*>(context);
    if (!variant) {
        debug(L"ERNativeUI received an empty popup-variant context.");
        return;
    }
    const ERUI_Result result = g_registration.alert(
        variant->message,
        variant->options,
        &popup_variant_completed,
        variant);
    if (result != ERUI_OK) {
        log_alert_rejection(variant->label.c_str(), result);
    }
}

void toggle_from_code() noexcept {
    const bool next = !g_enabled.load(std::memory_order_acquire);
    if (g_enabled_row.set_value(next) == ERUI_OK) {
        g_enabled.store(next ? 1u : 0u, std::memory_order_release);
        debug(L"Enabled was changed through ToggleRow::set_value().");
    }
}

void toggle_from_binding(
    ShowcaseActionState& state,
    const erui::ActionActivation& activation) noexcept {
    record_binding_activation(state, activation);
    toggle_from_code();
}

void persist_binding_changes(
    erui::StorageSection& store,
    const erui::AssignmentsChangedEvent& event) noexcept {
    if (store.apply(event) == ERUI_OK) {
        (void)store.config().save();
    }
}

void restore_binding(
    const erui::StorageSection& store,
    std::string_view action_id,
    const erui::InputAction& action) noexcept {
    const auto saved = store.get<erui::ActionInputs>(action_id);
    if (saved.found()) {
        (void)action.bind(saved.value());
    }
}

void visibility_target_pressed() noexcept {
    debug(L"A Reactive Layout target was activated.");
}

template <typename Row>
void commit_row_visibility(
    const Row& row,
    bool visible,
    ERUI_Result& first_failure) noexcept {
    if (!row.valid()) return;
    const ERUI_Result result = row.edit().visible(visible).commit();
    if (first_failure == ERUI_OK && result != ERUI_OK) {
        first_failure = result;
    }
}

ERUI_Result commit_visibility_surface(
    VisibilitySurface& surface,
    std::uint8_t layout) noexcept {
    if (layout > 2u) return ERUI_INVALID_ARGUMENT;

    const bool full = layout == 0u;
    const bool minimal = layout == 2u;
    ERUI_Result first_failure = ERUI_OK;

    // The controller and sentinel deliberately remain outside this list. They
    // guarantee an escape/focus target even when Minimal hides every target.
    commit_row_visibility(surface.button, full, first_failure);
    commit_row_visibility(surface.toggle_row, !minimal, first_failure);
    commit_row_visibility(surface.slider_row, !minimal, first_failure);
    commit_row_visibility(surface.numeric_slider, !minimal, first_failure);
    commit_row_visibility(surface.custom_slider, full, first_failure);
    commit_row_visibility(surface.inline_choice_row, !minimal, first_failure);
    commit_row_visibility(surface.popup_choice_row, full, first_failure);
    commit_row_visibility(surface.text_input_row, full, first_failure);
    commit_row_visibility(surface.color_picker_row, full, first_failure);
    for (std::size_t index = 0; index < surface.fillers.size(); ++index) {
        commit_row_visibility(
            surface.fillers[index],
            full || (!minimal && index < 3u),
            first_failure);
    }
    return first_failure;
}

void ERUI_CALL visibility_layout_changed(
    void* context,
    std::uint8_t value) noexcept {
    auto* const surface = static_cast<VisibilitySurface*>(context);
    if (!surface) return;
    surface->layout.store(value, std::memory_order_release);
    const ERUI_Result result = commit_visibility_surface(*surface, value);
    if (result != ERUI_OK) {
        wchar_t message[192]{};
        _snwprintf_s(
            message,
            std::size(message),
            _TRUNCATE,
            L"%ls visibility update failed: %ls.",
            surface->debug_name,
            result_name(result));
        debug(message);
    }
}

void build_visibility_surface(
    erui::Page page,
    VisibilitySurface& surface) noexcept {
    erui::SliderOptions layout{};
    layout.minimum = 0;
    layout.maximum = 2;
    layout.step = 1;
    layout.initial_value = surface.layout.load(std::memory_order_acquire);
    page.add_slider(
        L"Layout configuration",
        L"0 = Full, 1 = Compact, 2 = Minimal. The callback rebuilds this page immediately.",
        layout,
        &visibility_layout_changed,
        &surface);

    surface.sentinel = page.add_button<&visibility_target_pressed>(
        L"Layout sentinel (always visible)",
        L"This row remains present so every layout has a stable focus and Back path.");
    surface.button = page.add_button<&visibility_target_pressed>(
        L"Button target",
        L"Visible only in the Full layout.");
    surface.toggle_row = page.add_toggle(
        L"Toggle target",
        L"Visible in Full and Compact layouts.",
        surface.toggle.load(std::memory_order_acquire),
        &value_changed,
        &surface.toggle);

    erui::SliderOptions byte_slider{};
    byte_slider.minimum = 0;
    byte_slider.maximum = 10;
    byte_slider.step = 1;
    byte_slider.initial_value =
        surface.slider.load(std::memory_order_acquire);
    surface.slider_row = page.add_slider(
        L"Byte Slider target",
        L"Visible in Full and Compact layouts.",
        byte_slider,
        &value_changed,
        &surface.slider);

    erui::NumericSliderOptions numeric{};
    numeric.minimum = -1.0;
    numeric.maximum = 1.0;
    numeric.step = 0.1;
    numeric.initial_value = 0.0;
    numeric.decimal_places = 1;
    surface.numeric_slider = page.add_slider<&numeric_slider_changed>(
        L"Numeric Slider target",
        L"Visible in Full and Compact layouts.",
        numeric);

    static constexpr std::array<std::wstring_view, 3> custom_values{
        L"Low", L"Medium", L"High"};
    erui::CustomSliderOptions custom{};
    custom.value_count = static_cast<std::uint32_t>(custom_values.size());
    custom.initial_index = 1;
    custom.format_value = [](std::uint32_t index, std::uint32_t) {
        return index < custom_values.size()
            ? std::wstring(custom_values[index])
            : std::wstring{};
    };
    surface.custom_slider = page.add_custom_slider<&custom_slider_changed>(
        L"Custom Slider target",
        L"Visible only in the Full layout.",
        custom);

    static constexpr std::array<std::wstring_view, 3> choices{
        L"First", L"Second", L"Third"};
    erui::ChoiceOptions choice{};
    choice.values = choices.data();
    choice.count = static_cast<std::uint32_t>(choices.size());
    choice.initial_index =
        surface.inline_choice.load(std::memory_order_acquire);
    surface.inline_choice_row = page.add_inline_choice(
        L"Inline Choice target",
        L"Visible in Full and Compact layouts.",
        choice,
        &value_changed,
        &surface.inline_choice);
    choice.initial_index =
        surface.popup_choice.load(std::memory_order_acquire);
    surface.popup_choice_row = page.add_popup_choice(
        L"Popup Choice target",
        L"Visible only in the Full layout.",
        choice,
        &value_changed,
        &surface.popup_choice);

    erui::TextInputOptions text_input{};
    {
        std::lock_guard<std::mutex> lock(surface.text_input.mutex);
        text_input.initial_value = surface.text_input.value;
    }
    text_input.placeholder = L"Reactive text";
    surface.text_input_row = page.add_text_input<&text_input_changed>(
        L"TextInput target",
        L"Visible only in the Full layout.",
        text_input,
        surface.text_input);

    erui::ColorPickerOptions color{};
    {
        std::lock_guard<std::mutex> lock(surface.color_picker.mutex);
        color.initial_value = surface.color_picker.value;
    }
    surface.color_picker_row = page.add_color_picker<&color_picker_changed>(
        L"ColorPicker target",
        L"Visible only in the Full layout.",
        color,
        surface.color_picker);

    for (std::size_t index = 0; index < surface.fillers.size(); ++index) {
        const std::wstring label = L"Pagination target " +
            std::to_wstring(index + 1u);
        surface.fillers[index] = page.add_button<&visibility_target_pressed>(
            label,
            L"These fillers make Full span two slices; Compact keeps only the first three.");
    }
}

DWORD WINAPI initialize(void*) noexcept {
    const auto connection = erui::connect();
    if (!connection) {
        debug(connection.error().message().c_str());
        return 1;
    }

    const erui::LanguageInfo language =
        connection.value().game_language();

    g_text = &showcase::text(language);
    g_text_input = &showcase::text_input_text(language);
    g_color_picker_text = &showcase::color_picker_text(language);
    g_slider_text = &showcase::slider_text(language);
    initialize_popup_variants(*g_text);
    g_player_note.value = L"Tarnished";
    g_label_help_text.value = L"Editable";
    g_root_visibility.text_input.value = L"Game Options";
    g_tab_visibility.text_input.value = L"Custom tab";
    g_subpage_visibility.text_input.value = L"Subpage";
    g_root_visibility.color_picker.value = {218u, 185u, 98u};
    g_tab_visibility.color_picker.value = {96u, 180u, 255u};
    g_subpage_visibility.color_picker.value = {96u, 220u, 128u};

    erui::ProviderOptions options{};
    options.provider_id = "tarnished-showcase";
    options.display_name = g_text->name;
    options.owner_module = g_module;
    options.root_priority = 100;

    auto result = connection.value().register_menu(
        options, [](erui::Menu& menu) {
        auto root = menu.root();

        // A built-in destination is still an ordinary provider-owned Page:
        // only its native top-level heading belongs to Elden Ring. Register
        // the same safe action on every supported settings tab so this one
        // example exercises all seven production materializers through the
        // public API (Game Options is the same destination as menu.root()).
        if (menu.supports(erui::Capability::builtin_pages)) {
            constexpr std::array destinations{
                erui::BuiltinPage::game_options,
                erui::BuiltinPage::camera_options,
                erui::BuiltinPage::display,
                erui::BuiltinPage::sound,
                erui::BuiltinPage::network,
                erui::BuiltinPage::keyboard_mouse,
                erui::BuiltinPage::graphics,
            };
            for (const erui::BuiltinPage destination : destinations) {
                menu.page(destination).add_button<&show_native_alert>(
                    g_text->native_dialog,
                    g_text->row_help);
            }
        }

        // Device support is declared per action. A section appears in Button
        // Settings, Keyboard/Mouse Settings, or both according to the union
        // of devices supported by its actions. The three sections below make
        // those placement rules visible without research-only probe rows.
        if (menu.supports(erui::Capability::input_bindings)) {
            auto bindings = menu.input_bindings();

            auto all_devices = bindings.add_section(g_text->name);
            auto dialog_action =
                all_devices.add_action<&show_native_alert_from_binding>(
                    "show-native-ok-dialog",
                    g_text->native_dialog,
                    erui::inputs::all(
                        erui::KeyboardKey::key_q,
                        erui::MouseButton::button4,
                        erui::ControllerButton::right_trigger),
                    g_native_dialog_action);

            auto keyboard_mouse =
                bindings.add_section(L"ERNativeUI - Keyboard / Mouse");
            auto toggle_action =
                keyboard_mouse.add_action<&toggle_from_binding>(
                    "toggle-showcase-enabled",
                    g_text->toggle_code,
                    erui::inputs::keyboard_mouse(
                        erui::KeyboardKey::key_t,
                        erui::MouseButton::wheel_up),
                    g_toggle_enabled_action);

            auto controller =
                bindings.add_section(L"ERNativeUI - Controller");
            auto controller_action =
                controller.add_action<&show_native_alert_from_binding>(
                    "controller-native-dialog",
                    g_text->native_dialog,
                    erui::inputs::controller(
                        erui::ControllerButton::dpad_left),
                    g_controller_dialog_action);

            // Storage is opt-in: load the in-memory document, overlay any
            // saved assignments, then explicitly apply and save player events.
            // Without this block, the actions remain valid but do not persist.
            if (menu.supports(erui::Capability::storage)) {
                auto config = menu.storage();
                if (config.load() == ERUI_OK) {
                    auto store = config.section("bindings");
                    restore_binding(
                        store, "show-native-ok-dialog", dialog_action);
                    restore_binding(
                        store, "toggle-showcase-enabled", toggle_action);
                    restore_binding(
                        store, "controller-native-dialog", controller_action);
                    (void)bindings.on_assignments_changed<
                        &persist_binding_changes>(store);
                } else {
                    debug(L"Could not load showcase binding storage.");
                }
            }
        }

        g_enabled_row = root.add_toggle(
            g_text->enabled,
            g_text->row_help,
            g_enabled.load(std::memory_order_acquire),
            &value_changed,
            &g_enabled);

        erui::SliderOptions slider{};
        slider.minimum = 0;
        slider.maximum = 100;
        slider.step = 5;
        slider.initial_value = g_intensity.load(std::memory_order_acquire);
        g_intensity_row = root.add_slider(
            g_text->intensity,
            g_text->row_help,
            slider,
            &intensity_changed,
            &g_intensity);

        if (menu.supports(erui::Capability::row_visibility)) {
            // The same no-refresh visibility matrix runs on all three native
            // page families. Full deliberately paginates; Compact and Minimal
            // force an immediate topology rebuild from the slider callback.
            build_visibility_surface(root, g_root_visibility);

            auto visibility_subpage = root.add_submenu(
                L"Reactive Layout subpage",
                L"Exercises live row visibility on an ordinary logical subpage.",
                L"Reactive Layout subpage",
                L"Change Layout configuration; no separate refresh action is required.");
            build_visibility_surface(
                visibility_subpage, g_subpage_visibility);

            if (menu.supports(erui::Capability::custom_tabs)) {
                erui::TabOptions reactive_tab{};
                reactive_tab.tab_id = "reactive-layout";
                reactive_tab.title = L"Reactive Layout";
                reactive_tab.preferred_order = 900;
                auto tab = menu.add_tab(reactive_tab);
                build_visibility_surface(tab, g_tab_visibility);
            }
        }

        if (menu.supports(erui::Capability::updates)) {
            // Keep one action-row test directly on Game Options. Its movie
            // uses a different left-label binding from logical subpages.
            g_root_label_help_button = root.add_button<
                &toggle_label_help_showcase>(
                    L"ROOT CAPTION REGISTERED",
                    L"Tests API 1.4 presentation on the Game Options action widget.");

            auto label_help = root.add_submenu(
                L"Label & Help Updates",
                L"Validates runtime presentation on every editable row type.",
                L"Label & Help Updates",
                L"Press the first row, then inspect every label and focused help message.");

            // The first row doubles as the ButtonRow test and the A/B trigger,
            // keeping this complete matrix to nine physical rows.
            g_label_help_rows.button = label_help.add_button<
                &toggle_label_help_showcase>(
                    L"BUTTON CAPTION REGISTERED",
                    L"Button registered help.");

            g_label_help_rows.toggle = label_help.add_toggle(
                L"Toggle registered label",
                L"Toggle registered help.",
                g_label_help_toggle.load(std::memory_order_acquire),
                &value_changed,
                &g_label_help_toggle);

            erui::SliderOptions label_help_slider{};
            label_help_slider.minimum = 0;
            label_help_slider.maximum = 10;
            label_help_slider.step = 1;
            label_help_slider.initial_value =
                g_label_help_slider.load(std::memory_order_acquire);
            g_label_help_rows.slider = label_help.add_slider(
                L"Byte Slider registered label",
                L"Byte Slider registered help.",
                label_help_slider,
                &value_changed,
                &g_label_help_slider);

            if (menu.supports(erui::Capability::numeric_slider)) {
                erui::NumericSliderOptions numeric{};
                numeric.minimum = -1.0;
                numeric.maximum = 1.0;
                numeric.step = 0.1;
                numeric.initial_value = 0.0;
                numeric.decimal_places = 1;
                g_label_help_rows.numeric_slider =
                    label_help.add_slider<&numeric_slider_changed>(
                        L"Numeric Slider registered label",
                        L"Numeric Slider registered help.",
                        numeric);
            }

            if (menu.supports(erui::Capability::custom_slider)) {
                erui::CustomSliderOptions custom{};
                custom.value_count = 3;
                custom.initial_index = 1;
                custom.format_value =
                    [](std::uint32_t index, std::uint32_t) -> std::wstring {
                        constexpr std::array<std::wstring_view, 3> values{
                            L"Low", L"Medium", L"High"};
                        return index < values.size()
                            ? std::wstring(values[index])
                            : std::wstring{};
                    };
                g_label_help_rows.custom_slider =
                    label_help.add_custom_slider<&custom_slider_changed>(
                        L"Custom Slider registered label",
                        L"Custom Slider registered help.",
                        custom);
            }

            erui::ChoiceOptions inline_choice{};
            inline_choice.values = kLabelHelpChoiceValues.data();
            inline_choice.count = kLabelHelpChoiceValues.size();
            inline_choice.initial_index =
                g_label_help_inline_choice.load(std::memory_order_acquire);
            g_label_help_rows.inline_choice = label_help.add_inline_choice(
                L"Inline Choice registered label",
                L"Inline Choice registered help.",
                inline_choice,
                &value_changed,
                &g_label_help_inline_choice);

            erui::ChoiceOptions popup_choice = inline_choice;
            popup_choice.initial_index =
                g_label_help_popup_choice.load(std::memory_order_acquire);
            g_label_help_rows.popup_choice = label_help.add_popup_choice(
                L"Popup Choice registered label",
                L"Popup Choice registered help.",
                popup_choice,
                &label_help_popup_choice_changed,
                &g_label_help_popup_choice);

            if (menu.supports(erui::Capability::text_input)) {
                erui::TextInputOptions text_input{};
                text_input.initial_value = L"Editable";
                text_input.placeholder = L"Enter text";
                g_label_help_rows.text_input =
                    label_help.add_text_input<&text_input_changed>(
                        L"Text Input registered label",
                        L"Text Input registered help.",
                        text_input,
                        g_label_help_text);
            }

            if (menu.supports(erui::Capability::color_picker)) {
                erui::ColorPickerOptions color_picker{};
                color_picker.initial_value = {107u, 100u, 80u};
                g_label_help_rows.color_picker =
                    label_help.add_color_picker<&color_picker_changed>(
                        L"Color Picker registered label",
                        L"Color Picker registered help.",
                        color_picker,
                        g_label_help_color);
            }
        }

        if (menu.supports(erui::Capability::row_availability)) {
            auto availability_matrix = root.add_submenu(
                L"Row Interaction Matrix",
                L"Validates runtime interaction and disabled presentation on every supported typed row.",
                L"Row Interaction Matrix",
                L"Disable all targets, then verify every target is skipped and visibly dimmed.");

            availability_matrix.add_toggle(
                L"Disable all targets",
                L"On makes every target below non-interactive and selects its disabled presentation.",
                g_availability_matrix_disabled.load(
                    std::memory_order_acquire),
                &availability_matrix_changed,
                &g_availability_matrix_disabled);

            erui::SliderOptions matrix_intensity{};
            matrix_intensity.minimum = 0;
            matrix_intensity.maximum = 100;
            matrix_intensity.step = 5;
            matrix_intensity.initial_value =
                g_availability_matrix_intensity.load(
                    std::memory_order_acquire);
            g_availability_matrix_rows.intensity =
                availability_matrix.add_slider(
                    L"Intensity",
                    L"A styled red-to-green label verifies exact appearance restoration.",
                    matrix_intensity,
                    &availability_matrix_intensity_changed,
                    &g_availability_matrix_intensity);

            g_availability_matrix_rows.button =
                availability_matrix.add_button<
                    &availability_matrix_button_pressed>(
                        L"Button target",
                        L"This Button participates in the availability matrix.");

            g_availability_matrix_rows.toggle =
                availability_matrix.add_toggle(
                    L"Toggle target",
                    L"This Toggle participates in the availability matrix.",
                    g_availability_matrix_toggle.load(
                        std::memory_order_acquire),
                    &value_changed,
                    &g_availability_matrix_toggle);

            erui::SliderOptions matrix_slider{};
            matrix_slider.minimum = 0;
            matrix_slider.maximum = 10;
            matrix_slider.step = 1;
            matrix_slider.initial_value =
                g_availability_matrix_slider.load(std::memory_order_acquire);
            g_availability_matrix_rows.slider =
                availability_matrix.add_slider(
                    L"Byte Slider target",
                    L"This byte Slider participates in the availability matrix.",
                    matrix_slider,
                    &value_changed,
                    &g_availability_matrix_slider);

            if (menu.supports(erui::Capability::numeric_slider)) {
                erui::NumericSliderOptions matrix_numeric{};
                matrix_numeric.minimum = -1.0;
                matrix_numeric.maximum = 1.0;
                matrix_numeric.step = 0.1;
                matrix_numeric.initial_value = 0.0;
                matrix_numeric.decimal_places = 1;
                g_availability_matrix_rows.numeric_slider =
                    availability_matrix.add_slider<&numeric_slider_changed>(
                        L"Numeric Slider target",
                        L"This Numeric Slider participates in the availability matrix.",
                        matrix_numeric);
            }

            if (menu.supports(erui::Capability::custom_slider)) {
                erui::CustomSliderOptions matrix_custom{};
                matrix_custom.value_count = 3;
                matrix_custom.initial_index = 1;
                matrix_custom.format_value =
                    [](std::uint32_t index, std::uint32_t) -> std::wstring {
                        constexpr std::array<std::wstring_view, 3> values{
                            L"Low", L"Medium", L"High"};
                        return index < values.size()
                            ? std::wstring(values[index])
                            : std::wstring{};
                    };
                g_availability_matrix_rows.custom_slider =
                    availability_matrix.add_custom_slider<
                        &custom_slider_changed>(
                            L"Custom Slider target",
                            L"This Custom Slider participates in the availability matrix.",
                            matrix_custom);
            }

            erui::ChoiceOptions matrix_choice{};
            matrix_choice.values = kLabelHelpChoiceValues.data();
            matrix_choice.count = kLabelHelpChoiceValues.size();
            matrix_choice.initial_index =
                g_availability_matrix_inline_choice.load(
                    std::memory_order_acquire);
            g_availability_matrix_rows.inline_choice =
                availability_matrix.add_inline_choice(
                    L"Inline Choice target",
                    L"This Inline Choice participates in the availability matrix.",
                    matrix_choice,
                    &value_changed,
                    &g_availability_matrix_inline_choice);

            matrix_choice.initial_index =
                g_availability_matrix_popup_choice.load(
                    std::memory_order_acquire);
            g_availability_matrix_rows.popup_choice =
                availability_matrix.add_popup_choice(
                    L"Popup Choice target",
                    L"This Popup Choice participates in the availability matrix.",
                    matrix_choice,
                    &availability_matrix_popup_changed,
                    &g_availability_matrix_popup_choice);

            if (menu.supports(erui::Capability::text_input)) {
                erui::TextInputOptions matrix_text{};
                matrix_text.initial_value = L"Editable";
                matrix_text.placeholder = L"Enter text";
                g_availability_matrix_rows.text_input =
                    availability_matrix.add_text_input<&text_input_changed>(
                        L"Text Input target",
                        L"This TextInput participates in the availability matrix.",
                        matrix_text,
                        g_availability_matrix_text);
            }

            if (menu.supports(erui::Capability::color_picker)) {
                erui::ColorPickerOptions matrix_color{};
                matrix_color.initial_value = {107u, 100u, 80u};
                g_availability_matrix_rows.color_picker =
                    availability_matrix.add_color_picker<
                        &color_picker_changed>(
                            L"Color Picker target",
                            L"This ColorPicker participates in the availability matrix.",
                            matrix_color,
                            g_availability_matrix_color);
            }

            auto availability = root.add_submenu(
                L"Runtime Availability",
                L"Validates focus eligibility and native disabled presentation.",
                L"Runtime Availability",
                L"Interaction and appearance are independent API 1.4 properties.");

            availability.add_toggle(
                L"Target interaction",
                L"Off makes the ordinary target below impossible to focus or activate.",
                g_availability_controller.load(std::memory_order_acquire),
                &availability_interaction_changed,
                &g_availability_controller);

            erui::ChoiceOptions appearance{};
            appearance.values = kAvailabilityAppearanceValues.data();
            appearance.count = kAvailabilityAppearanceValues.size();
            appearance.initial_index =
                g_availability_appearance.load(std::memory_order_acquire);
            availability.add_inline_choice(
                L"Target appearance",
                L"Follow tracks interaction; Normal and Disabled force only the visual state.",
                appearance,
                &availability_appearance_changed,
                &g_availability_appearance);

            g_availability_target = availability.add_toggle(
                L"Availability target",
                L"Its focus eligibility and native appearance are controlled above.",
                g_availability_target_value.load(std::memory_order_acquire),
                &value_changed,
                &g_availability_target_value);

            availability.add_toggle(
                L"Enable construction-disabled target",
                L"Tests synchronization from the authored gray twin back to the normal twin.",
                g_initially_disabled_controller.load(
                    std::memory_order_acquire),
                &initially_disabled_interaction_changed,
                &g_initially_disabled_controller);

            g_initially_disabled_target = availability.add_toggle(
                L"Construction-disabled target",
                L"This row starts disabled and becomes selectable when enabled above.",
                g_initially_disabled_value.load(std::memory_order_acquire),
                &value_changed,
                &g_initially_disabled_value,
                false);

            g_self_disabling_target = availability.add_toggle(
                L"Self-disable on change",
                L"Changing this row commits interactive(false) from its own callback.",
                g_self_disabling_value.load(std::memory_order_acquire),
                &self_disabling_changed,
                &g_self_disabling_value);

            availability.add_button<&reenable_self_disabling_target>(
                L"Re-enable self-disabling row",
                L"Restores the row above after its callback moved focus away.");
        }

        if (menu.supports(erui::Capability::numeric_slider) ||
            menu.supports(erui::Capability::custom_slider)) {
            erui::Page advanced_sliders = root.add_submenu(
                g_slider_text->showcase,
                g_slider_text->showcase_help,
                g_slider_text->showcase,
                g_slider_text->showcase_help);

            if (menu.supports(erui::Capability::numeric_slider)) {
                erui::NumericSliderOptions multiplier{};
                multiplier.minimum = 0.1;
                multiplier.maximum = 2.0;
                multiplier.step = 0.1;
                multiplier.initial_value = 0.6;
                multiplier.decimal_places = 1;
                multiplier.prefix = L"x";
                advanced_sliders.add_slider<&numeric_slider_changed>(
                    g_slider_text->numeric_multiplier,
                    g_slider_text->multiplier_help,
                    multiplier);

                erui::NumericSliderOptions signed_value{};
                signed_value.minimum = -1.0;
                signed_value.maximum = 1.0;
                signed_value.step = 0.1;
                signed_value.initial_value = -0.5;
                signed_value.decimal_places = 1;
                advanced_sliders.add_slider<&numeric_slider_changed>(
                    g_slider_text->numeric_signed,
                    g_slider_text->signed_help,
                    signed_value);

                erui::NumericSliderOptions seconds{};
                seconds.minimum = 0.1;
                seconds.maximum = 10.0;
                seconds.step = 0.1;
                seconds.initial_value = 3.0;
                seconds.decimal_places = 1;
                seconds.suffix = L"s";
                advanced_sliders.add_slider<&numeric_slider_changed>(
                    g_slider_text->numeric_seconds,
                    g_slider_text->seconds_help,
                    seconds);
            }

            if (menu.supports(erui::Capability::custom_slider)) {
                static constexpr std::array<std::wstring_view, 7> values{
                    L"-7", L"3", L"7.8", L"1s", L"hello", L"18", L"x1.3"};

                erui::CustomSliderOptions impossible_values{};
                impossible_values.value_count =
                    static_cast<std::uint32_t>(values.size());
                impossible_values.initial_index = 4;
                impossible_values.format_value =
                    [](std::uint32_t index, std::uint32_t) -> std::wstring {
                        return index < values.size()
                            ? std::wstring(values[index])
                            : std::wstring{};
                    };
                advanced_sliders.add_custom_slider<&custom_slider_changed>(
                    g_slider_text->custom_mapping,
                    g_slider_text->custom_help,
                    impossible_values);
            }
        }

        auto choice_rows = root.add_submenu(
            g_text->choices,
            g_text->choice_help,
            g_text->choices,
            g_text->choice_help);

        const std::wstring_view preset_values[] = {
            g_text->minimal, g_text->balanced, g_text->detailed, g_text->maximum
        };
        erui::ChoiceOptions preset{};
        preset.values = preset_values;
        preset.count = std::size(preset_values);
        preset.initial_index =
            g_inline_preset.load(std::memory_order_acquire);
        choice_rows.add_inline_choice(
            g_text->inline_preset,
            g_text->choice_help,
            preset,
            &value_changed,
            &g_inline_preset);

        preset.initial_index =
            g_popup_preset.load(std::memory_order_acquire);
        choice_rows.add_popup_choice(
            g_text->popup_preset,
            g_text->choice_help,
            preset,
            &value_changed,
            &g_popup_preset);

        const std::wstring_view popup_two_values[] = {
            g_text->disabled, g_text->enabled_value
        };
        erui::ChoiceOptions popup_two{};
        popup_two.values = popup_two_values;
        popup_two.count = std::size(popup_two_values);
        popup_two.initial_index =
            g_popup_binary.load(std::memory_order_acquire);
        choice_rows.add_popup_choice(
            g_text->binary_mode,
            g_text->choice_help,
            popup_two,
            &value_changed,
            &g_popup_binary);

        std::array<std::wstring, 8> popup_eight_storage{};
        std::array<std::wstring_view, 8> popup_eight_values{};
        for (std::size_t index = 0; index < popup_eight_storage.size(); ++index) {
            popup_eight_storage[index] = g_text->preset;
            popup_eight_storage[index] += L" ";
            popup_eight_storage[index] += std::to_wstring(index + 1);
            popup_eight_values[index] = popup_eight_storage[index];
        }
        erui::ChoiceOptions popup_eight{};
        popup_eight.values = popup_eight_values.data();
        popup_eight.count = std::size(popup_eight_values);
        popup_eight.initial_index =
            g_popup_extended.load(std::memory_order_acquire);
        choice_rows.add_popup_choice(
            g_text->extended_preset,
            g_text->choice_help,
            popup_eight,
            &value_changed,
            &g_popup_extended);

        root.add_button<&toggle_from_code>(
            g_text->toggle_code,
            g_text->row_help);

        // TextInput was introduced in API 1.1 and remains part of the current
        // API 1.4 table. The nearby capability check documents its dependency.
        if (menu.supports(erui::Capability::text_input)) {
            auto text_inputs = root.add_submenu(
                g_text_input->showcase,
                g_text->row_help,
                g_text_input->showcase,
                g_text->row_help);

            erui::TextInputOptions normal{};
            normal.initial_value = g_player_note.value;
            normal.placeholder = g_text_input->note_placeholder;
            text_inputs.add_text_input<&text_input_changed>(
                g_text_input->note,
                g_text->row_help,
                normal,
                g_player_note);

            erui::TextInputOptions extended{};
            extended.initial_value = g_extended_note.value;
            extended.placeholder = g_text_input->extended_placeholder;
            extended.maximum_length = 35;
            text_inputs.add_text_input<&text_input_changed>(
                g_text_input->extended_note,
                g_text->row_help,
                extended,
                g_extended_note);
        }

        // ColorPicker is also additive in API 1.1. These two rows deliberately
        // own independent values so the showcase exercises both root-page and
        // subpage presentation without sharing client state.
        if (menu.supports(erui::Capability::color_picker)) {
            erui::ColorPickerOptions root_color{};
            {
                std::lock_guard<std::mutex> lock(g_root_color.mutex);
                root_color.initial_value = g_root_color.value;
            }
            root.add_color_picker<&color_picker_changed>(
                g_color_picker_text->root_accent,
                g_text->row_help,
                root_color,
                g_root_color);

            auto color_picker = root.add_submenu(
                g_color_picker_text->showcase,
                g_text->row_help,
                g_color_picker_text->showcase,
                g_text->row_help);
            erui::ColorPickerOptions subpage_color{};
            {
                std::lock_guard<std::mutex> lock(g_subpage_color.mutex);
                subpage_color.initial_value = g_subpage_color.value;
            }
            color_picker.add_color_picker<&color_picker_changed>(
                g_color_picker_text->subpage_accent,
                g_text->row_help,
                subpage_color,
                g_subpage_color);
        }

        auto large = root.add_submenu(
            g_text->large,
            g_text->large_help,
            g_text->large,
            g_text->large_help);

        // The built-in formatter produces the language-neutral "Title (n/t)"
        // form. Providers can still supply a custom formatter when needed.
        large.set_presentation(g_text->name, g_text->large);

        large.add_button<&show_native_alert>(
            g_text->native_dialog,
            g_text->row_help);

        for (std::uint32_t index = 0; index < g_action_numbers.size(); ++index) {
            g_action_numbers[index] = index + 1;
            const std::wstring label = std::wstring(g_text->action) + L" " +
                std::to_wstring(index + 1);
            large.add_button(
                label,
                g_text->row_help,
                &action_pressed,
                &g_action_numbers[index]);
        }

        auto popup_variants = root.add_submenu(
            g_text->popups,
            g_text->popup_help,
            g_text->popups,
            g_text->popup_help);
        popup_variants.set_presentation(
            g_text->name,
            g_text->popups);
        for (PopupVariant& variant : g_popup_variants) {
            popup_variants.add_button(
                variant.label,
                variant.help,
                &show_popup_variant,
                &variant);
        }

    });

    if (!result) {
        debug(result.error().message().c_str());
        return 1;
    }
    g_registration = result.value();

    if (g_registration.supports(erui::Capability::updates)) {
        if (commit_intensity_presentation(
                g_intensity.load(std::memory_order_acquire)) != ERUI_OK) {
            debug(L"Could not apply the initial API 1.4 intensity presentation.");
        }
        if (commit_label_help_showcase(
                g_label_help_variant.load(std::memory_order_acquire)) !=
            ERUI_OK) {
            debug(L"Could not apply one or more initial row presentations.");
        }
        if (g_registration.supports(erui::Capability::row_availability)) {
            if (commit_availability_matrix_intensity_presentation(
                    g_availability_matrix_intensity.load(
                        std::memory_order_acquire)) != ERUI_OK) {
                debug(L"Could not apply the initial availability-matrix intensity presentation.");
            }
            if (commit_availability_matrix_popup_caption(
                    g_availability_matrix_popup_choice.load(
                        std::memory_order_acquire)) != ERUI_OK) {
                debug(L"Could not apply the initial availability Popup Choice caption.");
            }
            if (commit_availability_matrix(
                    g_availability_matrix_disabled.load(
                        std::memory_order_acquire) == 0u) != ERUI_OK) {
                debug(L"Could not apply the initial availability matrix.");
            }
        }
    }
    debug(L"Menu registered successfully.");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, &initialize, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
