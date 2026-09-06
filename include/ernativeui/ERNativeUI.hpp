#pragma once

#include <ernativeui/erui.h>

#if !defined(_WIN32)
#  error ERNativeUI clients currently require Windows.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  define ERUI_DETAIL_UNDEF_WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#  define ERUI_DETAIL_UNDEF_NOMINMAX
#endif
#include <Windows.h>
#ifdef ERUI_DETAIL_UNDEF_NOMINMAX
#  undef NOMINMAX
#  undef ERUI_DETAIL_UNDEF_NOMINMAX
#endif
#ifdef ERUI_DETAIL_UNDEF_WIN32_LEAN_AND_MEAN
#  undef WIN32_LEAN_AND_MEAN
#  undef ERUI_DETAIL_UNDEF_WIN32_LEAN_AND_MEAN
#endif

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <thread>
#include <type_traits>
#include <utility>

namespace erui {

using RowHandle = ERUI_RowHandle;
using InputSectionHandle = ERUI_InputSectionHandle;
using InputActionHandle = ERUI_InputActionHandle;
using StorageHandle = ERUI_StorageHandle;
using ButtonCallback = void (ERUI_CALL*)(void*) noexcept;
using ValueChangedCallback = void (ERUI_CALL*)(void*, std::uint8_t) noexcept;
using TextInputChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_TextInputChangeContext*) noexcept;
using ColorPickerChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_ColorPickerChangeContext*) noexcept;
using InputActionActivatedCallback = void (ERUI_CALL*)(
    void*, const ERUI_InputActionActivatedContext*) noexcept;
using AssignmentsChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_AssignmentsChangedContext*) noexcept;

enum class Capability : ERUI_Capabilities {
    toggle = ERUI_CAP_TOGGLE,
    slider = ERUI_CAP_SLIDER,
    button = ERUI_CAP_BUTTON,
    submenu = ERUI_CAP_SUBMENU,
    pagination = ERUI_CAP_PAGINATION,
    host_owned_values = ERUI_CAP_HOST_OWNED_VALUES,
    page_presentation = ERUI_CAP_PAGE_PRESENTATION,
    alert = ERUI_CAP_ALERT,
    inline_choice = ERUI_CAP_INLINE_CHOICE,
    popup_choice = ERUI_CAP_POPUP_CHOICE,
    game_language = ERUI_CAP_GAME_LANGUAGE,
    text_input = ERUI_CAP_TEXT_INPUT,
    color_picker = ERUI_CAP_COLOR_PICKER,
    builtin_pages = ERUI_CAP_BUILTIN_PAGES,
    input_bindings = ERUI_CAP_INPUT_BINDINGS,
    storage = ERUI_CAP_STORAGE,
};

enum class InputDevice : ERUI_InputDevices {
    controller = ERUI_INPUT_DEVICE_CONTROLLER,
    keyboard = ERUI_INPUT_DEVICE_KEYBOARD,
    mouse = ERUI_INPUT_DEVICE_MOUSE,
};

using InputDeviceMask = ERUI_InputDevices;

enum class ControllerButton : ERUI_ControllerButton {
    dpad_up = ERUI_CONTROLLER_BUTTON_DPAD_UP,
    dpad_down = ERUI_CONTROLLER_BUTTON_DPAD_DOWN,
    dpad_left = ERUI_CONTROLLER_BUTTON_DPAD_LEFT,
    dpad_right = ERUI_CONTROLLER_BUTTON_DPAD_RIGHT,
    face_south = ERUI_CONTROLLER_BUTTON_FACE_SOUTH,
    face_east = ERUI_CONTROLLER_BUTTON_FACE_EAST,
    face_west = ERUI_CONTROLLER_BUTTON_FACE_WEST,
    face_north = ERUI_CONTROLLER_BUTTON_FACE_NORTH,
    left_shoulder = ERUI_CONTROLLER_BUTTON_LEFT_SHOULDER,
    right_shoulder = ERUI_CONTROLLER_BUTTON_RIGHT_SHOULDER,
    left_trigger = ERUI_CONTROLLER_BUTTON_LEFT_TRIGGER,
    right_trigger = ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER,
    left_stick = ERUI_CONTROLLER_BUTTON_LEFT_STICK,
    right_stick = ERUI_CONTROLLER_BUTTON_RIGHT_STICK,
};

enum class KeyboardKey : ERUI_KeyboardKey {
    digit_1 = ERUI_KEYBOARD_KEY_DIGIT_1,
    digit_2 = ERUI_KEYBOARD_KEY_DIGIT_2,
    digit_3 = ERUI_KEYBOARD_KEY_DIGIT_3,
    digit_4 = ERUI_KEYBOARD_KEY_DIGIT_4,
    digit_5 = ERUI_KEYBOARD_KEY_DIGIT_5,
    digit_6 = ERUI_KEYBOARD_KEY_DIGIT_6,
    digit_7 = ERUI_KEYBOARD_KEY_DIGIT_7,
    digit_8 = ERUI_KEYBOARD_KEY_DIGIT_8,
    digit_9 = ERUI_KEYBOARD_KEY_DIGIT_9,
    digit_0 = ERUI_KEYBOARD_KEY_DIGIT_0,
    backspace = ERUI_KEYBOARD_KEY_BACKSPACE,
    tab = ERUI_KEYBOARD_KEY_TAB,
    key_q = ERUI_KEYBOARD_KEY_Q,
    key_w = ERUI_KEYBOARD_KEY_W,
    key_e = ERUI_KEYBOARD_KEY_E,
    key_r = ERUI_KEYBOARD_KEY_R,
    key_t = ERUI_KEYBOARD_KEY_T,
    key_y = ERUI_KEYBOARD_KEY_Y,
    key_u = ERUI_KEYBOARD_KEY_U,
    key_i = ERUI_KEYBOARD_KEY_I,
    key_o = ERUI_KEYBOARD_KEY_O,
    key_p = ERUI_KEYBOARD_KEY_P,
    enter = ERUI_KEYBOARD_KEY_ENTER,
    left_control = ERUI_KEYBOARD_KEY_LEFT_CONTROL,
    key_a = ERUI_KEYBOARD_KEY_A,
    key_s = ERUI_KEYBOARD_KEY_S,
    key_d = ERUI_KEYBOARD_KEY_D,
    key_f = ERUI_KEYBOARD_KEY_F,
    key_g = ERUI_KEYBOARD_KEY_G,
    key_h = ERUI_KEYBOARD_KEY_H,
    key_j = ERUI_KEYBOARD_KEY_J,
    key_k = ERUI_KEYBOARD_KEY_K,
    key_l = ERUI_KEYBOARD_KEY_L,
    left_shift = ERUI_KEYBOARD_KEY_LEFT_SHIFT,
    key_z = ERUI_KEYBOARD_KEY_Z,
    key_x = ERUI_KEYBOARD_KEY_X,
    key_c = ERUI_KEYBOARD_KEY_C,
    key_v = ERUI_KEYBOARD_KEY_V,
    key_b = ERUI_KEYBOARD_KEY_B,
    key_n = ERUI_KEYBOARD_KEY_N,
    key_m = ERUI_KEYBOARD_KEY_M,
    right_shift = ERUI_KEYBOARD_KEY_RIGHT_SHIFT,
    left_alt = ERUI_KEYBOARD_KEY_LEFT_ALT,
    space = ERUI_KEYBOARD_KEY_SPACE,
    numpad_7 = ERUI_KEYBOARD_KEY_NUMPAD_7,
    numpad_8 = ERUI_KEYBOARD_KEY_NUMPAD_8,
    numpad_9 = ERUI_KEYBOARD_KEY_NUMPAD_9,
    numpad_4 = ERUI_KEYBOARD_KEY_NUMPAD_4,
    numpad_5 = ERUI_KEYBOARD_KEY_NUMPAD_5,
    numpad_6 = ERUI_KEYBOARD_KEY_NUMPAD_6,
    numpad_1 = ERUI_KEYBOARD_KEY_NUMPAD_1,
    numpad_2 = ERUI_KEYBOARD_KEY_NUMPAD_2,
    numpad_3 = ERUI_KEYBOARD_KEY_NUMPAD_3,
    numpad_0 = ERUI_KEYBOARD_KEY_NUMPAD_0,
    numpad_enter = ERUI_KEYBOARD_KEY_NUMPAD_ENTER,
    right_control = ERUI_KEYBOARD_KEY_RIGHT_CONTROL,
    right_alt = ERUI_KEYBOARD_KEY_RIGHT_ALT,
    home = ERUI_KEYBOARD_KEY_HOME,
    arrow_up = ERUI_KEYBOARD_KEY_ARROW_UP,
    page_up = ERUI_KEYBOARD_KEY_PAGE_UP,
    arrow_left = ERUI_KEYBOARD_KEY_ARROW_LEFT,
    arrow_right = ERUI_KEYBOARD_KEY_ARROW_RIGHT,
    end = ERUI_KEYBOARD_KEY_END,
    arrow_down = ERUI_KEYBOARD_KEY_ARROW_DOWN,
    page_down = ERUI_KEYBOARD_KEY_PAGE_DOWN,
    insert = ERUI_KEYBOARD_KEY_INSERT,
    delete_key = ERUI_KEYBOARD_KEY_DELETE,
    numpad_multiply = ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY,
    numpad_subtract = ERUI_KEYBOARD_KEY_NUMPAD_SUBTRACT,
    numpad_add = ERUI_KEYBOARD_KEY_NUMPAD_ADD,
    numpad_decimal = ERUI_KEYBOARD_KEY_NUMPAD_DECIMAL,
    numpad_divide = ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE,
};

enum class MouseButton : ERUI_MouseButton {
    left = ERUI_MOUSE_BUTTON_LEFT,
    right = ERUI_MOUSE_BUTTON_RIGHT,
    middle = ERUI_MOUSE_BUTTON_MIDDLE,
    button4 = ERUI_MOUSE_BUTTON_4,
    button5 = ERUI_MOUSE_BUTTON_5,
    wheel_up = ERUI_MOUSE_BUTTON_WHEEL_UP,
    wheel_down = ERUI_MOUSE_BUTTON_WHEEL_DOWN,
};

enum class InputSlotState : ERUI_InputSlotState {
    absent = ERUI_INPUT_SLOT_ABSENT,
    unbound = ERUI_INPUT_SLOT_UNBOUND,
    bound = ERUI_INPUT_SLOT_BOUND,
};

namespace detail {
struct ActionInputsAccess;
template <auto> struct StaticAssignmentsChangedState;
template <auto, typename> struct StatefulAssignmentsChangedState;
template <auto, typename> struct OwnedAssignmentsChangedState;
}

class ActionInputs {
public:
    ActionInputs() noexcept { native_.size = sizeof(native_); }

    static ActionInputs all() noexcept {
        return ActionInputs{}.controller().keyboard().mouse();
    }

    ActionInputs& controller() noexcept {
        native_.controller = {ERUI_INPUT_SLOT_UNBOUND,
            ERUI_CONTROLLER_BUTTON_INVALID};
        return *this;
    }
    ActionInputs& controller(ControllerButton input) noexcept {
        native_.controller = {ERUI_INPUT_SLOT_BOUND,
            static_cast<ERUI_ControllerButton>(input)};
        return *this;
    }
    ActionInputs& keyboard() noexcept {
        native_.keyboard = {ERUI_INPUT_SLOT_UNBOUND,
            ERUI_KEYBOARD_KEY_INVALID};
        return *this;
    }
    ActionInputs& keyboard(KeyboardKey input) noexcept {
        native_.keyboard = {ERUI_INPUT_SLOT_BOUND,
            static_cast<ERUI_KeyboardKey>(input)};
        return *this;
    }
    ActionInputs& mouse() noexcept {
        native_.mouse = {ERUI_INPUT_SLOT_UNBOUND,
            ERUI_MOUSE_BUTTON_INVALID};
        return *this;
    }
    ActionInputs& mouse(MouseButton input) noexcept {
        native_.mouse = {ERUI_INPUT_SLOT_BOUND,
            static_cast<ERUI_MouseButton>(input)};
        return *this;
    }

    [[nodiscard]] bool controller_supported() const noexcept {
        return native_.controller.state != ERUI_INPUT_SLOT_ABSENT;
    }
    [[nodiscard]] bool keyboard_supported() const noexcept {
        return native_.keyboard.state != ERUI_INPUT_SLOT_ABSENT;
    }
    [[nodiscard]] bool mouse_supported() const noexcept {
        return native_.mouse.state != ERUI_INPUT_SLOT_ABSENT;
    }
    [[nodiscard]] bool empty() const noexcept {
        return !controller_supported() && !keyboard_supported() &&
            !mouse_supported();
    }
    [[nodiscard]] InputSlotState controller_state() const noexcept {
        return static_cast<InputSlotState>(native_.controller.state);
    }
    [[nodiscard]] InputSlotState keyboard_state() const noexcept {
        return static_cast<InputSlotState>(native_.keyboard.state);
    }
    [[nodiscard]] InputSlotState mouse_state() const noexcept {
        return static_cast<InputSlotState>(native_.mouse.state);
    }
    [[nodiscard]] std::optional<ControllerButton> controller_input() const noexcept {
        return native_.controller.state == ERUI_INPUT_SLOT_BOUND
            ? std::optional<ControllerButton>{
                static_cast<ControllerButton>(native_.controller.input)}
            : std::nullopt;
    }
    [[nodiscard]] std::optional<KeyboardKey> keyboard_input() const noexcept {
        return native_.keyboard.state == ERUI_INPUT_SLOT_BOUND
            ? std::optional<KeyboardKey>{
                static_cast<KeyboardKey>(native_.keyboard.input)}
            : std::nullopt;
    }
    [[nodiscard]] std::optional<MouseButton> mouse_input() const noexcept {
        return native_.mouse.state == ERUI_INPUT_SLOT_BOUND
            ? std::optional<MouseButton>{
                static_cast<MouseButton>(native_.mouse.input)}
            : std::nullopt;
    }

    friend bool operator==(const ActionInputs& left,
        const ActionInputs& right) noexcept {
        return left.native_.controller.state == right.native_.controller.state &&
            left.native_.controller.input == right.native_.controller.input &&
            left.native_.keyboard.state == right.native_.keyboard.state &&
            left.native_.keyboard.input == right.native_.keyboard.input &&
            left.native_.mouse.state == right.native_.mouse.state &&
            left.native_.mouse.input == right.native_.mouse.input;
    }
    friend bool operator!=(const ActionInputs& left,
        const ActionInputs& right) noexcept { return !(left == right); }

private:
    friend class InputAction;
    friend class InputSection;
    friend class AssignmentChange;
    friend class StorageSection;
    friend struct detail::ActionInputsAccess;

    explicit ActionInputs(const ERUI_ActionInputs& native) noexcept
        : native_(native) { native_.size = sizeof(native_); }
    ERUI_ActionInputs native_{};
};

namespace inputs {

inline ActionInputs all() noexcept { return ActionInputs::all(); }
inline ActionInputs all(KeyboardKey keyboard, MouseButton mouse,
    ControllerButton controller) noexcept {
    return ActionInputs{}.keyboard(keyboard).mouse(mouse).controller(controller);
}
inline ActionInputs controller() noexcept {
    return ActionInputs{}.controller();
}
inline ActionInputs controller(ControllerButton input) noexcept {
    return ActionInputs{}.controller(input);
}
inline ActionInputs keyboard_mouse() noexcept {
    return ActionInputs{}.keyboard().mouse();
}
inline ActionInputs keyboard_mouse(KeyboardKey keyboard,
    MouseButton mouse) noexcept {
    return ActionInputs{}.keyboard(keyboard).mouse(mouse);
}
inline ActionInputs keyboard() noexcept { return ActionInputs{}.keyboard(); }
inline ActionInputs keyboard(KeyboardKey input) noexcept {
    return ActionInputs{}.keyboard(input);
}
inline ActionInputs mouse() noexcept { return ActionInputs{}.mouse(); }
inline ActionInputs mouse(MouseButton input) noexcept {
    return ActionInputs{}.mouse(input);
}

} // namespace inputs

struct ActionActivation {
    ERUI_ProviderHandle provider{};
    InputActionHandle action{};
    InputDeviceMask devices{ERUI_INPUT_DEVICE_NONE};

    [[nodiscard]] bool includes(InputDevice device) const noexcept {
        const auto bit = static_cast<ERUI_InputDevices>(device);
        return (devices & bit) == bit;
    }
};

enum class AssignmentChangeReason : ERUI_AssignmentChangeReason {
    unknown = ERUI_ASSIGNMENT_CHANGE_UNKNOWN,
    player_assignment = ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT,
    player_clear = ERUI_ASSIGNMENT_CHANGE_PLAYER_CLEAR,
    reset_to_defaults = ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS,
};

enum class StorageLocation : ERUI_StorageLocation {
    provider_default = ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT,
    owner_module_directory = ERUI_STORAGE_LOCATION_OWNER_MODULE_DIRECTORY,
    explicit_absolute = ERUI_STORAGE_LOCATION_EXPLICIT_ABSOLUTE,
};

struct StorageOptions {
    StorageLocation location{StorageLocation::provider_default};
    std::wstring_view path{};

    static StorageOptions provider_default() noexcept { return {}; }
    static StorageOptions beside_module(std::wstring_view relative_path) noexcept {
        return {StorageLocation::owner_module_directory, relative_path};
    }
    static StorageOptions at(std::wstring_view absolute_path) noexcept {
        return {StorageLocation::explicit_absolute, absolute_path};
    }
};

struct StorageInfo {
    bool loaded{};
    bool dirty{};
    std::uint64_t current_revision{};
    std::uint64_t last_saved_revision{};
};

enum class BuiltinPage : ERUI_BuiltinPage {
    game_options = ERUI_BUILTIN_PAGE_GAME_OPTIONS,
    camera_options = ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
    display = ERUI_BUILTIN_PAGE_DISPLAY,
    sound = ERUI_BUILTIN_PAGE_SOUND,
    network = ERUI_BUILTIN_PAGE_NETWORK,
    keyboard_mouse = ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE,
    graphics = ERUI_BUILTIN_PAGE_GRAPHICS,
};

enum class GameLanguage : std::uint32_t {
    unknown = ERUI_GAME_LANGUAGE_UNKNOWN,
    english = ERUI_GAME_LANGUAGE_ENGLISH,
    german = ERUI_GAME_LANGUAGE_GERMAN,
    french = ERUI_GAME_LANGUAGE_FRENCH,
    italian = ERUI_GAME_LANGUAGE_ITALIAN,
    korean = ERUI_GAME_LANGUAGE_KOREAN,
    spanish = ERUI_GAME_LANGUAGE_SPANISH,
    chinese_simplified = ERUI_GAME_LANGUAGE_CHINESE_SIMPLIFIED,
    chinese_traditional = ERUI_GAME_LANGUAGE_CHINESE_TRADITIONAL,
    russian = ERUI_GAME_LANGUAGE_RUSSIAN,
    thai = ERUI_GAME_LANGUAGE_THAI,
    japanese = ERUI_GAME_LANGUAGE_JAPANESE,
    polish = ERUI_GAME_LANGUAGE_POLISH,
    arabic = ERUI_GAME_LANGUAGE_ARABIC,
    portuguese_brazil = ERUI_GAME_LANGUAGE_PORTUGUESE_BRAZIL,
    spanish_latin_america = ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA,
};

struct LanguageInfo {
    ERUI_Result result{ERUI_NOT_SUPPORTED};
    GameLanguage known{GameLanguage::unknown};
    std::string identifier{};

    [[nodiscard]] bool available() const noexcept { return result == ERUI_OK; }
};

enum class AlertButtons : std::uint32_t {
    ok = ERUI_ALERT_BUTTONS_OK,
    cancel = ERUI_ALERT_BUTTONS_CANCEL,
    yes = ERUI_ALERT_BUTTONS_YES,
    no = ERUI_ALERT_BUTTONS_NO,
    ok_cancel = ERUI_ALERT_BUTTONS_OK_CANCEL,
    yes_no = ERUI_ALERT_BUTTONS_YES_NO,
    dismiss_only = ERUI_ALERT_BUTTONS_DISMISS_ONLY,
};

enum class AlertPlacement : std::uint32_t {
    bottom = ERUI_ALERT_PLACEMENT_BOTTOM,
    center = ERUI_ALERT_PLACEMENT_CENTER,
};

enum class AlertResponse : std::uint32_t {
    none = ERUI_ALERT_RESPONSE_NONE,
    primary = ERUI_ALERT_RESPONSE_PRIMARY,
    secondary = ERUI_ALERT_RESPONSE_SECONDARY,
    dismissed = ERUI_ALERT_RESPONSE_DISMISSED,
};

using AlertCallback = void (ERUI_CALL*)(
    void*, ERUI_Result, AlertResponse) noexcept;
using PageTitleFormatContext = ERUI_PageTitleFormatContext;
using PageTitleFormatter = ERUI_Result (ERUI_CALL*)(
    void*,
    const PageTitleFormatContext*,
    std::uint16_t*,
    std::uint32_t,
    std::uint32_t*) noexcept;
using StaticPageTitleFormatter = ERUI_Result (ERUI_CALL*)(
    const PageTitleFormatContext*,
    std::uint16_t*,
    std::uint32_t,
    std::uint32_t*) noexcept;

enum class ErrorCode {
    none,
    host_not_loaded,
    export_not_found,
    host_not_ready,
    host_failed,
    incompatible_api,
    registration_failed,
    builder_exception,
    operation_failed,
};

class Error {
public:
    Error() = default;
    Error(ErrorCode code, ERUI_Result native_result) noexcept
        : code_(code), native_result_(native_result) {}
    Error(ErrorCode code, ERUI_Result native_result, std::wstring message)
        : code_(code), native_result_(native_result), message_(std::move(message)) {}

    ErrorCode code() const noexcept { return code_; }
    ERUI_Result native_result() const noexcept { return native_result_; }
    const std::wstring& message() const noexcept { return message_; }

private:
    ErrorCode code_{ErrorCode::none};
    ERUI_Result native_result_{ERUI_OK};
    std::wstring message_{};
};

static_assert(std::is_nothrow_move_assignable_v<Error>,
    "Error fallback assignment must remain allocation-free");

struct ProviderOptions {
    // Stable, case-sensitive ASCII ID containing 1..255 letters, digits,
    // '.', '_', or '-'. Keep it unlocalized and distinct among loaded mods.
    // It is copied during registration.
    std::string provider_id{};
    std::wstring display_name{};
    void* owner_module{};
    std::int32_t root_priority{100};
};

struct SliderOptions {
    std::int32_t minimum{0};
    std::int32_t maximum{100};
    std::int32_t step{1};
    // Off-step values are rounded down to a step measured from minimum before
    // the menu is published.
    std::uint8_t initial_value{0};
    bool enabled{true};
};

struct ChoiceOptions {
    const std::wstring_view* values{};
    std::size_t count{};
    std::uint8_t initial_index{0};
};

struct TextInputOptions {
    std::wstring_view initial_value{};
    std::wstring_view placeholder{};
    // Counts UTF-16 code units, exactly like std::wstring_view::size() on
    // Windows. This is not a byte or user-perceived-character count.
    std::uint32_t maximum_length{ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH};
};

struct TextInputChange {
    ERUI_ProviderHandle provider{};
    RowHandle row{};
    // Borrowed from the callback context and valid only for that invocation.
    std::wstring_view value{};
};

struct Color {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

struct ColorPickerOptions {
    Color initial_value{};
    bool enabled{true};
};

struct ColorPickerChange {
    ERUI_ProviderHandle provider{};
    RowHandle row{};
    Color value{};
};

struct AlertOptions {
    AlertButtons buttons{AlertButtons::ok};
    AlertPlacement placement{AlertPlacement::bottom};
};

struct PagePresentation {
    std::wstring_view menu_title{};
    std::wstring_view page_title{};
    PageTitleFormatter formatter{};
    void* user_data{};
};

inline ERUI_Result write_page_title(
    std::wstring_view title,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) noexcept {
    if (!out_length) return ERUI_INVALID_ARGUMENT;
    *out_length = 0;
    if (title.empty() || !output ||
        title.size() > static_cast<std::size_t>(output_capacity) ||
        title.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return ERUI_INVALID_ARGUMENT;
    }
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
        "ERNativeUI requires Windows UTF-16 wchar_t");
    std::memcpy(output, title.data(), title.size() * sizeof(std::uint16_t));
    *out_length = static_cast<std::uint32_t>(title.size());
    return ERUI_OK;
}

namespace detail {

struct ActionInputsAccess {
    static const ERUI_ActionInputs& native(const ActionInputs& value) noexcept {
        return value.native_;
    }
    static ActionInputs from_native(const ERUI_ActionInputs& value) noexcept {
        return ActionInputs(value);
    }
};

} // namespace detail

/*
 * Convert ActionInputs to and from ERNativeUI's stable, human-readable text
 * representation. These helpers are entirely client-side and do not require
 * a Connection or a loaded ERNativeUI.dll.
 *
 * The explicit overloads retain the precise ERUI_Result and leave output
 * unchanged on failure. The one-argument overloads are convenient when a
 * caller only needs success-or-failure.
 */
[[nodiscard]] inline ERUI_Result format_action_inputs(
    const ActionInputs& inputs,
    std::string& output) noexcept {
    const ERUI_ActionInputs& native = detail::ActionInputsAccess::native(inputs);
    std::uint32_t required = 0;
    ERUI_Result result = ERUI_FormatActionInputs(
        &native, nullptr, 0, &required);
    if (result != ERUI_OK) return result;

    try {
        std::string formatted(required, '\0');
        if (required != 0) {
            std::uint32_t written = 0;
            result = ERUI_FormatActionInputs(
                &native, formatted.data(), required, &written);
            if (result != ERUI_OK || written != required) {
                return result != ERUI_OK
                    ? result
                    : static_cast<ERUI_Result>(ERUI_INTERNAL_ERROR);
            }
        }
        output.swap(formatted);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

[[nodiscard]] inline std::optional<std::string> format_action_inputs(
    const ActionInputs& inputs) noexcept {
    std::string output{};
    if (format_action_inputs(inputs, output) != ERUI_OK) return std::nullopt;
    return std::optional<std::string>{std::move(output)};
}

[[nodiscard]] inline ERUI_Result parse_action_inputs(
    std::string_view text,
    ActionInputs& output) noexcept {
    if (text.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return ERUI_INVALID_ARGUMENT;
    }
    const ERUI_StringView view{
        text.data(), static_cast<std::uint32_t>(text.size()), 0};
    ERUI_ActionInputs parsed{};
    const ERUI_Result result = ERUI_ParseActionInputs(&view, &parsed);
    if (result != ERUI_OK) return result;
    output = detail::ActionInputsAccess::from_native(parsed);
    return ERUI_OK;
}

[[nodiscard]] inline std::optional<ActionInputs> parse_action_inputs(
    std::string_view text) noexcept {
    ActionInputs output{};
    if (parse_action_inputs(text, output) != ERUI_OK) return std::nullopt;
    return std::optional<ActionInputs>{std::move(output)};
}

namespace detail {

inline bool valid_action_inputs(const ERUI_ActionInputs& value) noexcept {
    return ERUI_DetailValidActionInputs(&value) != 0;
}

// The raw C callback carries ERUI_AlertResponse as uint32_t. Adapt it inside
// the client module instead of invoking a typed C++ callback through a
// reinterpreted function pointer. The same client-side thunk that allocates
// this state also destroys it after the asynchronous completion.
struct AlertCallbackState {
    AlertCallback callback{};
    void* user_data{};
};

inline void ERUI_CALL alert_callback_thunk(
    void* opaque_state,
    ERUI_Result result,
    ERUI_AlertResponse response) noexcept {
    std::unique_ptr<AlertCallbackState> state(
        static_cast<AlertCallbackState*>(opaque_state));
    if (!state || !state->callback) return;
    state->callback(
        state->user_data,
        result,
        static_cast<AlertResponse>(response));
}

inline bool make_text_input_change(
    const ERUI_TextInputChangeContext* context,
    TextInputChange& output) noexcept {
    if (!context || context->size < sizeof(ERUI_TextInputChangeContext) ||
        context->value.reserved != 0 ||
        (context->value.length != 0 && !context->value.data)) {
        return false;
    }
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
        "ERNativeUI requires Windows UTF-16 wchar_t");
    const wchar_t* data = context->value.data
        ? reinterpret_cast<const wchar_t*>(context->value.data)
        : L"";
    output.provider = context->provider;
    output.row = context->row;
    output.value = std::wstring_view(data, context->value.length);
    return true;
}

inline ERUI_Color native_color(Color value) noexcept {
    ERUI_Color native{};
    native.red = value.red;
    native.green = value.green;
    native.blue = value.blue;
    return native;
}

inline Color color_from_native(const ERUI_Color& value) noexcept {
    return Color{value.red, value.green, value.blue};
}

inline bool make_color_picker_change(
    const ERUI_ColorPickerChangeContext* context,
    ColorPickerChange& output) noexcept {
    if (!context ||
        context->size < sizeof(ERUI_ColorPickerChangeContext) ||
        context->value.reserved != 0) {
        return false;
    }
    output.provider = context->provider;
    output.row = context->row;
    output.value = color_from_native(context->value);
    return true;
}

inline bool make_action_activation(
    const ERUI_InputActionActivatedContext* context,
    ActionActivation& output) noexcept {
    if (!context ||
        context->size < sizeof(ERUI_InputActionActivatedContext) ||
        context->flags != 0 || context->reserved != 0 ||
        context->provider == ERUI_INVALID_PROVIDER ||
        context->action == ERUI_INVALID_INPUT_ACTION ||
        context->devices == ERUI_INPUT_DEVICE_NONE ||
        (context->devices & ~static_cast<ERUI_InputDevices>(
            ERUI_INPUT_DEVICE_ALL)) != 0) {
        return false;
    }
    output.provider = context->provider;
    output.action = context->action;
    output.devices = context->devices;
    return true;
}

inline bool view_size_fits(std::size_t size) noexcept {
    return size <= (std::numeric_limits<std::uint32_t>::max)();
}

inline bool valid_machine_identifier(std::string_view value) noexcept {
    if (value.empty() ||
        value.size() > ERUI_STORAGE_MAX_IDENTIFIER_BYTES) return false;
    for (const unsigned char byte : value) {
        if (!((byte >= 'a' && byte <= 'z') ||
              (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') ||
              byte == '.' || byte == '_' || byte == '-')) {
            return false;
        }
    }
    return true;
}

inline bool valid_storage_identifier(std::string_view value) noexcept {
    if (value.empty() ||
        value.size() > ERUI_STORAGE_MAX_IDENTIFIER_BYTES ||
        value.front() == ' ' || value.back() == ' ' ||
        value == "." || value == "..") {
        return false;
    }
    for (const unsigned char byte : value) {
        if (!((byte >= 'a' && byte <= 'z') ||
              (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') ||
              byte == '.' || byte == '_' || byte == '-' || byte == ' ')) {
            return false;
        }
    }
    return true;
}

inline ERUI_StringView string_view(std::string_view value) noexcept {
    ERUI_StringView result{};
    result.data = value.data();
    result.length = static_cast<std::uint32_t>(value.size());
    return result;
}

inline ERUI_Utf16View utf16_view(std::wstring_view value) noexcept {
    static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
        "ERNativeUI requires Windows UTF-16 wchar_t");
    ERUI_Utf16View result{};
    result.data = reinterpret_cast<const std::uint16_t*>(value.data());
    result.length = static_cast<std::uint32_t>(value.size());
    return result;
}

inline const wchar_t* result_name(ERUI_Result result) noexcept {
    switch (result) {
    case ERUI_OK: return L"success";
    case ERUI_INVALID_ARGUMENT: return L"invalid argument";
    case ERUI_HOST_NOT_READY: return L"host not ready";
    case ERUI_HOST_FAILED: return L"host failed";
    case ERUI_UNSUPPORTED_VERSION: return L"unsupported API version";
    case ERUI_DUPLICATE_PROVIDER_ID: return L"duplicate provider ID";
    case ERUI_INVALID_HANDLE: return L"invalid handle";
    case ERUI_ALREADY_COMMITTED: return L"provider already committed";
    case ERUI_REGISTRATION_CLOSED: return L"startup registration is closed";
    case ERUI_OUT_OF_MEMORY: return L"out of memory";
    case ERUI_CALLBACK_REJECTED: return L"provider callback/module validation failed";
    case ERUI_INTERNAL_ERROR: return L"internal host error";
    case ERUI_QUEUE_FULL: return L"alert queue is full";
    case ERUI_NOT_SUPPORTED: return L"feature is unavailable for this game build";
    case ERUI_BUFFER_TOO_SMALL: return L"output buffer is too small";
    case ERUI_DUPLICATE_ACTION_ID: return L"duplicate provider-local action ID";
    case ERUI_NOT_FOUND: return L"value was not found";
    case ERUI_STORAGE_NOT_LOADED: return L"storage has not been loaded";
    case ERUI_STORAGE_ALREADY_LOADED: return L"storage is already loaded";
    case ERUI_STORAGE_IO_ERROR: return L"storage I/O failed";
    case ERUI_STORAGE_FORMAT_ERROR: return L"storage data is malformed";
    default: return L"unknown host result";
    }
}

struct ApiConnectionResult {
    ERUI_Api api{};
    Error error{};
    bool connected{};
};

struct RetainedCallbackState {
    virtual ~RetainedCallbackState() = default;
};

inline LanguageInfo read_game_language(const ERUI_Api& api) {
    LanguageInfo language{};
    ERUI_GameLanguageInfo native_language{};
    native_language.size = sizeof(native_language);
    language.result = api.get_game_language
        ? api.get_game_language(&native_language)
        : static_cast<ERUI_Result>(ERUI_NOT_SUPPORTED);
    if (language.result != ERUI_OK) return language;
    language.known = static_cast<GameLanguage>(native_language.known_language);
    if (native_language.identifier.length != 0 &&
        !native_language.identifier.data) {
        language.result = ERUI_INTERNAL_ERROR;
        return language;
    }
    language.identifier.assign(
        native_language.identifier.data ? native_language.identifier.data : "",
        native_language.identifier.length);
    return language;
}

inline bool common_api_complete(const ERUI_Api& api) noexcept {
    return api.register_provider && api.add_button && api.add_toggle &&
        api.add_slider && api.add_inline_choice && api.add_popup_choice &&
        api.add_submenu && api.set_page_presentation &&
        api.commit_provider && api.abort_provider && api.set_row_value &&
        api.get_row_value && api.enqueue_alert && api.get_game_language &&
        (api.capabilities & ERUI_CAP_PAGE_PRESENTATION) != 0 &&
        (api.capabilities & ERUI_CAP_ALERT) != 0 &&
        (api.capabilities & ERUI_CAP_INLINE_CHOICE) != 0 &&
        (api.capabilities & ERUI_CAP_POPUP_CHOICE) != 0 &&
        (api.capabilities & ERUI_CAP_GAME_LANGUAGE) != 0;
}

inline bool api_complete_for_version(
    const ERUI_Api& api,
    std::uint32_t version) noexcept {
    if (!common_api_complete(api) || api.api_version != version) return false;
    if (version == ERUI_API_VERSION_1_0) {
        return api.size == ERUI_API_V1_0_SIZE &&
            (api.capabilities &
                (ERUI_CAP_TEXT_INPUT | ERUI_CAP_COLOR_PICKER |
                    ERUI_CAP_BUILTIN_PAGES |
                    ERUI_CAP_INPUT_BINDINGS | ERUI_CAP_STORAGE)) == 0;
    }
    if (version == ERUI_API_VERSION_1_1) {
        return api.size == ERUI_API_V1_1_SIZE && api.add_text_input &&
            api.set_text_input_value && api.get_text_input_value &&
            api.add_color_picker && api.set_color_picker_value &&
            api.get_color_picker_value && api.get_builtin_page &&
            api.add_input_section && api.add_input_action &&
            api.set_assignments_changed_handler && api.set_action_inputs &&
            api.get_action_inputs && api.get_action_default_inputs &&
            api.reset_action_inputs && api.open_storage &&
            api.storage_load && api.storage_save && api.storage_get_utf8 &&
            api.storage_set_utf8 && api.storage_erase &&
            api.storage_get_action_inputs && api.storage_set_action_inputs &&
            api.storage_apply_assignment_changes && api.storage_get_info &&
            (api.capabilities & ERUI_CAP_TEXT_INPUT) != 0 &&
            (api.capabilities & ERUI_CAP_COLOR_PICKER) != 0 &&
            (api.capabilities & ERUI_CAP_BUILTIN_PAGES) != 0 &&
            (api.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0 &&
            (api.capabilities & ERUI_CAP_STORAGE) != 0;
    }
    return false;
}

inline ERUI_Result request_api(
    ERUI_GetApiFn get_api,
    std::uint32_t version,
    std::uint32_t table_size,
    ERUI_Api& api) noexcept {
    api = {};
    api.size = table_size;
    return get_api(version, &api);
}

inline ApiConnectionResult connect_api(std::chrono::milliseconds timeout) {
    ApiConnectionResult result{};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool saw_host = false;
    do {
        HMODULE host = GetModuleHandleW(L"ERNativeUI.dll");
        if (host) {
            saw_host = true;
            const FARPROC symbol = GetProcAddress(host, "ERUI_GetApi");
            if (!symbol) {
                result.error = Error(ErrorCode::export_not_found, ERUI_INTERNAL_ERROR,
                    L"ERNativeUI.dll does not export ERUI_GetApi.");
                return result;
            }
            ERUI_GetApiFn get_api{};
            static_assert(sizeof(get_api) == sizeof(symbol),
                "Windows function pointers must use one representation");
            std::memcpy(&get_api, &symbol, sizeof(get_api));
            ERUI_Result status = request_api(
                get_api,
                ERUI_API_VERSION_1_1,
                ERUI_API_V1_1_SIZE,
                result.api);
            if (status == ERUI_OK) {
                if (!api_complete_for_version(
                        result.api, ERUI_API_VERSION_1_1)) {
                    result.error = Error(ErrorCode::incompatible_api,
                        ERUI_UNSUPPORTED_VERSION,
                        L"ERNativeUI returned an incomplete or incompatible API 1.1 table.");
                    return result;
                }
                result.connected = true;
                return result;
            }
            if (status == ERUI_HOST_FAILED) {
                result.error = Error(ErrorCode::host_failed, status,
                    L"ERNativeUI host initialization failed; inspect ERNativeUI.log.");
                return result;
            }
            if (status != ERUI_HOST_NOT_READY) {
                result.error = Error(ErrorCode::incompatible_api, status,
                    status == ERUI_UNSUPPORTED_VERSION
                        ? L"ERNativeUI API 1.1 is required; update ERNativeUI.dll."
                        : std::wstring(L"ERNativeUI API 1.1 negotiation failed: ") +
                            result_name(status));
                return result;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } while (true);

    result.error = saw_host
        ? Error(ErrorCode::host_not_ready, ERUI_HOST_NOT_READY,
            L"ERNativeUI.dll did not become ready before the connection timeout.")
        : Error(ErrorCode::host_not_loaded, ERUI_HOST_NOT_READY,
            L"ERNativeUI.dll was not loaded; this mod's configuration menu is disabled.");
    return result;
}

class Draft {
public:
    Draft(ERUI_Api api, ERUI_ProviderHandle provider, ERUI_PageHandle root)
        : api_(api), provider_(provider), root_(root) {}
    ~Draft() = default;
    Draft(const Draft&) = delete;
    Draft& operator=(const Draft&) = delete;

    [[nodiscard]] bool open() const noexcept {
        return open_.load(std::memory_order_acquire);
    }

    void close() noexcept {
        open_.store(false, std::memory_order_release);
    }

    void mark_committed() noexcept {
        // Provider callbacks are process-lifetime by contract. A committed
        // provider's module is pinned by the host, so transfer these small
        // client-side thunk states to the same lifetime. Failed drafts still
        // destroy their states normally.
        for (auto& state : callback_states_) state.release();
        callback_states_.clear();
        close();
    }

    bool retain_callback_state(
        std::unique_ptr<RetainedCallbackState> state) noexcept {
        try {
            callback_states_.push_back(std::move(state));
            return true;
        } catch (...) {
            return false;
        }
    }

    void discard_callback_state(
        const RetainedCallbackState* state) noexcept {
        for (auto iterator = callback_states_.begin();
             iterator != callback_states_.end(); ++iterator) {
            if (iterator->get() == state) {
                callback_states_.erase(iterator);
                return;
            }
        }
    }

    void record(ERUI_Result result, const wchar_t* operation) noexcept {
        if (result == ERUI_OK || failed_ || !open()) return;
        failed_ = true;
        try {
            error_ = Error(ErrorCode::operation_failed, result,
                std::wstring(operation) + L" failed: " + result_name(result));
        } catch (...) {
            // This path may itself be handling allocation failure. Keep the
            // fallback message empty so the noexcept contract remains real.
            error_ = Error(ErrorCode::operation_failed, result);
        }
    }

    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    ERUI_PageHandle root_{};
    bool failed_{};
    Error error_{};
    std::vector<std::unique_ptr<RetainedCallbackState>> callback_states_{};

private:
    std::atomic_bool open_{true};
};

class DraftGuard {
public:
    explicit DraftGuard(std::shared_ptr<Draft> draft) noexcept
        : draft_(std::move(draft)) {}
    ~DraftGuard() { if (draft_) draft_->close(); }
    DraftGuard(const DraftGuard&) = delete;
    DraftGuard& operator=(const DraftGuard&) = delete;
private:
    std::shared_ptr<Draft> draft_{};
};

class ProviderAbortGuard {
public:
    ProviderAbortGuard(ERUI_Api api, ERUI_ProviderHandle provider) noexcept
        : api_(api), provider_(provider) {}
    ~ProviderAbortGuard() {
        if (active_ && provider_ != ERUI_INVALID_PROVIDER) {
            api_.abort_provider(provider_);
        }
    }
    ProviderAbortGuard(const ProviderAbortGuard&) = delete;
    ProviderAbortGuard& operator=(const ProviderAbortGuard&) = delete;
    void release() noexcept { active_ = false; }
private:
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    bool active_{true};
};

} // namespace detail

class RegistrationResult;
class ConnectionResult;

[[nodiscard]] ConnectionResult connect(
    std::chrono::milliseconds timeout) noexcept;
[[nodiscard]] ConnectionResult connect() noexcept;

class Connection {
public:
    [[nodiscard]] bool valid() const noexcept {
        return api_.api_version == ERUI_API_VERSION_1_1 &&
            api_.size == ERUI_API_V1_1_SIZE;
    }
    explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] bool supports(Capability capability) const noexcept {
        const auto bit = static_cast<ERUI_Capabilities>(capability);
        return valid() && (api_.capabilities & bit) == bit;
    }
    [[nodiscard]] std::uint32_t api_version() const noexcept {
        return valid() ? api_.api_version : 0;
    }
    [[nodiscard]] LanguageInfo game_language() const noexcept {
        LanguageInfo unavailable{};
        if (!valid()) {
            unavailable.result = ERUI_INVALID_HANDLE;
            return unavailable;
        }
        try {
            return detail::read_game_language(api_);
        } catch (...) {
            unavailable.result = ERUI_OUT_OF_MEMORY;
            return unavailable;
        }
    }

    template <typename Builder>
    RegistrationResult register_menu(
        const ProviderOptions& options,
        Builder&& builder) const noexcept;

private:
    friend class ConnectionResult;
    friend ConnectionResult connect(std::chrono::milliseconds) noexcept;

    Connection() = default;
    explicit Connection(ERUI_Api api) noexcept : api_(api) {}
    ERUI_Api api_{};
};

class ConnectionResult {
public:
    explicit operator bool() const noexcept { return success_; }
    [[nodiscard]] bool success() const noexcept { return success_; }
    [[nodiscard]] const Error& error() const noexcept { return error_; }
    Connection& value() noexcept { return connection_; }
    const Connection& value() const noexcept { return connection_; }

private:
    friend ConnectionResult connect(std::chrono::milliseconds) noexcept;
    bool success_{};
    Error error_{};
    Connection connection_{};
};

inline ConnectionResult connect(std::chrono::milliseconds timeout) noexcept {
    ConnectionResult output{};
    try {
        const auto connection = detail::connect_api(timeout);
        if (!connection.connected) {
            output.error_ = connection.error;
            return output;
        }
        output.connection_ = Connection(connection.api);
        output.success_ = true;
        return output;
    } catch (...) {
        output.error_ = Error(ErrorCode::operation_failed,
            ERUI_OUT_OF_MEMORY);
        return output;
    }
}

inline ConnectionResult connect() noexcept {
    return connect(std::chrono::milliseconds{30000});
}

class Page {
public:
    Page() = default;

    RowHandle add_button(
        std::wstring_view label,
        std::wstring_view help,
        ButtonCallback callback,
        void* user_data = nullptr,
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return ERUI_INVALID_ROW;
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_button text");
            return ERUI_INVALID_ROW;
        }
        ERUI_ButtonDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.callback = reinterpret_cast<ERUI_ButtonCallback>(callback);
        desc.user_data = user_data;
        desc.enabled = enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_button(
            draft_->provider_, page_, &desc, &row), L"add_button");
        return row;
    }

    template <void (*Function)() noexcept>
    RowHandle add_button(
        std::wstring_view label,
        std::wstring_view help,
        bool enabled = true) noexcept {
        return add_button(label, help, &button_thunk<Function>, nullptr, enabled);
    }

    RowHandle add_toggle(
        std::wstring_view label,
        std::wstring_view help,
        std::uint8_t initial_value,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr,
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return ERUI_INVALID_ROW;
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_toggle text");
            return ERUI_INVALID_ROW;
        }
        ERUI_ToggleDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.changed_callback = reinterpret_cast<ERUI_ValueChangedCallback>(callback);
        desc.user_data = user_data;
        desc.initial_value = initial_value;
        desc.enabled = enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_toggle(
            draft_->provider_, page_, &desc, &row), L"add_toggle");
        return row;
    }

    RowHandle add_slider(
        std::wstring_view label,
        std::wstring_view help,
        const SliderOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return ERUI_INVALID_ROW;
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_slider text");
            return ERUI_INVALID_ROW;
        }
        ERUI_SliderDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.changed_callback = reinterpret_cast<ERUI_ValueChangedCallback>(callback);
        desc.user_data = user_data;
        desc.minimum = options.minimum;
        desc.maximum = options.maximum;
        desc.step = options.step;
        desc.initial_value = options.initial_value;
        desc.enabled = options.enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_slider(
            draft_->provider_, page_, &desc, &row), L"add_slider");
        return row;
    }

    RowHandle add_inline_choice(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        return add_choice_impl(
            label, help, options, callback, user_data,
            draft_ ? draft_->api_.add_inline_choice : nullptr,
            L"add_inline_choice");
    }

    RowHandle add_popup_choice(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        return add_choice_impl(
            label, help, options, callback, user_data,
            draft_ ? draft_->api_.add_popup_choice : nullptr,
            L"add_popup_choice");
    }

    RowHandle add_text_input(
        std::wstring_view label,
        std::wstring_view help,
        const TextInputOptions& options,
        TextInputChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return ERUI_INVALID_ROW;
        }
        if (!draft_->api_.add_text_input ||
            (draft_->api_.capabilities & ERUI_CAP_TEXT_INPUT) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_text_input");
            return ERUI_INVALID_ROW;
        }
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            !detail::view_size_fits(options.initial_value.size()) ||
            !detail::view_size_fits(options.placeholder.size()) ||
            options.maximum_length > ERUI_TEXT_INPUT_MAX_LENGTH ||
            options.initial_value.size() >
                (options.maximum_length == 0
                    ? ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH
                    : options.maximum_length) ||
            (!callback && user_data)) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_text_input");
            return ERUI_INVALID_ROW;
        }

        ERUI_TextInputDesc description{};
        description.size = sizeof(description);
        description.label = detail::utf16_view(label);
        description.help = detail::utf16_view(help);
        description.initial_value = detail::utf16_view(
            options.initial_value);
        description.placeholder = detail::utf16_view(options.placeholder);
        description.changed_callback =
            reinterpret_cast<ERUI_TextInputChangedCallback>(callback);
        description.user_data = user_data;
        description.maximum_length = options.maximum_length;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_text_input(
            draft_->provider_, page_, &description, &row),
            L"add_text_input");
        return row;
    }

    template <auto Function>
    RowHandle add_text_input(
        std::wstring_view label,
        std::wstring_view help,
        const TextInputOptions& options) noexcept {
        using Expected = void (*)(const TextInputChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "TextInput callback must be void(const TextInputChange&) noexcept");
        return add_text_input(
            label,
            help,
            options,
            &text_input_thunk<Function>);
    }

    template <auto Function, typename State>
    RowHandle add_text_input(
        std::wstring_view label,
        std::wstring_view help,
        const TextInputOptions& options,
        State& state) noexcept {
        using Expected = void (*)(State&, const TextInputChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "Stateful TextInput callback must be void(State&, const TextInputChange&) noexcept");
        return add_text_input(
            label,
            help,
            options,
            &stateful_text_input_thunk<Function, State>,
            &state);
    }

    RowHandle add_color_picker(
        std::wstring_view label,
        std::wstring_view help,
        const ColorPickerOptions& options,
        ColorPickerChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return ERUI_INVALID_ROW;
        }
        if (!draft_->api_.add_color_picker ||
            (draft_->api_.capabilities & ERUI_CAP_COLOR_PICKER) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_color_picker");
            return ERUI_INVALID_ROW;
        }
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            (!callback && user_data)) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_color_picker");
            return ERUI_INVALID_ROW;
        }

        ERUI_ColorPickerDesc description{};
        description.size = sizeof(description);
        description.label = detail::utf16_view(label);
        description.help = detail::utf16_view(help);
        description.changed_callback =
            reinterpret_cast<ERUI_ColorPickerChangedCallback>(callback);
        description.user_data = user_data;
        description.initial_value = detail::native_color(
            options.initial_value);
        description.enabled = options.enabled ? 1u : 0u;
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_color_picker(
            draft_->provider_, page_, &description, &row),
            L"add_color_picker");
        return row;
    }

    template <auto Function>
    RowHandle add_color_picker(
        std::wstring_view label,
        std::wstring_view help,
        const ColorPickerOptions& options) noexcept {
        using Expected = void (*)(const ColorPickerChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "ColorPicker callback must be void(const ColorPickerChange&) noexcept");
        return add_color_picker(
            label,
            help,
            options,
            &color_picker_thunk<Function>);
    }

    template <auto Function, typename State>
    RowHandle add_color_picker(
        std::wstring_view label,
        std::wstring_view help,
        const ColorPickerOptions& options,
        State& state) noexcept {
        using Expected = void (*)(State&, const ColorPickerChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "Stateful ColorPicker callback must be void(State&, const ColorPickerChange&) noexcept");
        return add_color_picker(
            label,
            help,
            options,
            &stateful_color_picker_thunk<Function, State>,
            &state);
    }

    Page add_submenu(
        std::wstring_view label,
        std::wstring_view help,
        std::wstring_view page_title = {},
        std::wstring_view page_help = {},
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            !detail::view_size_fits(page_title.size()) ||
            !detail::view_size_fits(page_help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_submenu text");
            return {};
        }
        ERUI_SubmenuDesc desc{};
        desc.size = sizeof(desc);
        desc.label = detail::utf16_view(label);
        desc.help = detail::utf16_view(help);
        desc.page_title = detail::utf16_view(page_title);
        desc.page_help = detail::utf16_view(page_help);
        desc.enabled = enabled ? 1u : 0u;
        ERUI_PageHandle child{};
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_submenu(
            draft_->provider_, page_, &desc, &child, &row), L"add_submenu");
        return Page(draft_, child);
    }

    Page& set_presentation(const PagePresentation& presentation) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return *this;
        if (!detail::view_size_fits(presentation.menu_title.size()) ||
            !detail::view_size_fits(presentation.page_title.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"set_page_presentation text");
            return *this;
        }
        ERUI_PagePresentationDesc desc{};
        desc.size = sizeof(desc);
        desc.menu_title = detail::utf16_view(presentation.menu_title);
        desc.page_title = detail::utf16_view(presentation.page_title);
        desc.formatter = reinterpret_cast<ERUI_PageTitleFormatter>(
            presentation.formatter);
        desc.user_data = presentation.user_data;
        draft_->record(draft_->api_.set_page_presentation(
            draft_->provider_, page_, &desc), L"set_page_presentation");
        return *this;
    }

    Page& set_presentation(
        std::wstring_view menu_title,
        std::wstring_view page_title = {}) noexcept {
        PagePresentation presentation{};
        presentation.menu_title = menu_title;
        presentation.page_title = page_title;
        return set_presentation(presentation);
    }

    template <StaticPageTitleFormatter Formatter>
    Page& set_presentation(
        std::wstring_view menu_title = {},
        std::wstring_view page_title = {}) noexcept {
        PagePresentation presentation{};
        presentation.menu_title = menu_title;
        presentation.page_title = page_title;
        presentation.formatter = &page_title_formatter_thunk<Formatter>;
        return set_presentation(presentation);
    }

    bool valid() const noexcept {
        return draft_ && draft_->open() && page_ != ERUI_INVALID_PAGE;
    }

private:
    friend class Menu;
    Page(std::shared_ptr<detail::Draft> draft, ERUI_PageHandle page) noexcept
        : draft_(std::move(draft)), page_(page) {}

    template <void (*Function)() noexcept>
    static void ERUI_CALL button_thunk(void*) noexcept { Function(); }

    template <auto Function>
    static void ERUI_CALL text_input_thunk(
        void*,
        const ERUI_TextInputChangeContext* context) noexcept {
        TextInputChange change{};
        if (detail::make_text_input_change(context, change)) {
            Function(change);
        }
    }

    template <auto Function, typename State>
    static void ERUI_CALL stateful_text_input_thunk(
        void* user_data,
        const ERUI_TextInputChangeContext* context) noexcept {
        auto* state = static_cast<State*>(user_data);
        TextInputChange change{};
        if (state && detail::make_text_input_change(context, change)) {
            Function(*state, change);
        }
    }

    template <auto Function>
    static void ERUI_CALL color_picker_thunk(
        void*,
        const ERUI_ColorPickerChangeContext* context) noexcept {
        ColorPickerChange change{};
        if (detail::make_color_picker_change(context, change)) {
            Function(change);
        }
    }

    template <auto Function, typename State>
    static void ERUI_CALL stateful_color_picker_thunk(
        void* user_data,
        const ERUI_ColorPickerChangeContext* context) noexcept {
        auto* state = static_cast<State*>(user_data);
        ColorPickerChange change{};
        if (state && detail::make_color_picker_change(context, change)) {
            Function(*state, change);
        }
    }

    using AddChoiceFunction = ERUI_Result (ERUI_CALL*)(
        ERUI_ProviderHandle,
        ERUI_PageHandle,
        const ERUI_ChoiceDesc*,
        ERUI_RowHandle*);

    RowHandle add_choice_impl(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback,
        void* user_data,
        AddChoiceFunction function,
        const wchar_t* operation) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return ERUI_INVALID_ROW;
        }
        if (!function || !detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) || !options.values ||
            options.count == 0 || options.count > 32 ||
            options.initial_index >= options.count) {
            draft_->record(ERUI_INVALID_ARGUMENT, operation);
            return ERUI_INVALID_ROW;
        }
        try {
            std::vector<ERUI_Utf16View> native_options;
            native_options.reserve(options.count);
            for (std::size_t index = 0; index < options.count; ++index) {
                if (options.values[index].empty() ||
                    !detail::view_size_fits(options.values[index].size())) {
                    draft_->record(ERUI_INVALID_ARGUMENT, operation);
                    return ERUI_INVALID_ROW;
                }
                native_options.push_back(
                    detail::utf16_view(options.values[index]));
            }
            ERUI_ChoiceDesc description{};
            description.size = sizeof(description);
            description.label = detail::utf16_view(label);
            description.help = detail::utf16_view(help);
            description.changed_callback =
                reinterpret_cast<ERUI_ValueChangedCallback>(callback);
            description.user_data = user_data;
            description.options = native_options.data();
            description.option_count =
                static_cast<std::uint32_t>(native_options.size());
            description.initial_index = options.initial_index;
            ERUI_RowHandle row{};
            draft_->record(function(
                draft_->provider_, page_, &description, &row), operation);
            return row;
        } catch (...) {
            draft_->record(ERUI_OUT_OF_MEMORY, operation);
            return ERUI_INVALID_ROW;
        }
    }

    template <StaticPageTitleFormatter Formatter>
    static ERUI_Result ERUI_CALL page_title_formatter_thunk(
        void*,
        const PageTitleFormatContext* context,
        std::uint16_t* output,
        std::uint32_t output_capacity,
        std::uint32_t* out_length) noexcept {
        return Formatter(context, output, output_capacity, out_length);
    }

    std::shared_ptr<detail::Draft> draft_{};
    ERUI_PageHandle page_{};
};

class InputActionDevices;

class InputAction {
public:
    InputAction() = default;

    [[nodiscard]] bool valid() const noexcept {
        return provider_ != ERUI_INVALID_PROVIDER &&
            action_ != ERUI_INVALID_INPUT_ACTION && api_.set_action_inputs &&
            api_.get_action_inputs && api_.get_action_default_inputs &&
            api_.reset_action_inputs;
    }
    explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] InputActionHandle handle() const noexcept { return action_; }

    ERUI_Result bind(const ActionInputs& inputs) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        const ERUI_ActionInputs& native = detail::ActionInputsAccess::native(inputs);
        if (!detail::valid_action_inputs(native)) return ERUI_INVALID_ARGUMENT;
        return api_.set_action_inputs(provider_, action_, &native);
    }
    ERUI_Result unbind() const noexcept {
        ActionInputs current{};
        const ERUI_Result result = query_inputs(current);
        if (result != ERUI_OK) return result;
        ActionInputs update{};
        if (current.controller_supported()) update.controller();
        if (current.keyboard_supported()) update.keyboard();
        if (current.mouse_supported()) update.mouse();
        return bind(update);
    }
    ERUI_Result reset_to_defaults() const noexcept {
        return valid()
            ? api_.reset_action_inputs(
                provider_, action_, ERUI_INPUT_DEVICE_ALL)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result query_inputs(ActionInputs& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        ERUI_ActionInputs native{};
        native.size = sizeof(native);
        const ERUI_Result result =
            api_.get_action_inputs(provider_, action_, &native);
        if (result != ERUI_OK) return result;
        if (!detail::valid_action_inputs(native)) return ERUI_INTERNAL_ERROR;
        output = detail::ActionInputsAccess::from_native(native);
        return ERUI_OK;
    }
    ERUI_Result query_default_inputs(ActionInputs& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        ERUI_ActionInputs native{};
        native.size = sizeof(native);
        const ERUI_Result result =
            api_.get_action_default_inputs(provider_, action_, &native);
        if (result != ERUI_OK) return result;
        if (!detail::valid_action_inputs(native)) return ERUI_INTERNAL_ERROR;
        output = detail::ActionInputsAccess::from_native(native);
        return ERUI_OK;
    }
    [[nodiscard]] ActionInputs inputs() const noexcept {
        ActionInputs output{};
        (void)query_inputs(output);
        return output;
    }
    [[nodiscard]] ActionInputs default_inputs() const noexcept {
        ActionInputs output{};
        (void)query_default_inputs(output);
        return output;
    }
    [[nodiscard]] bool is_bound() const noexcept {
        const ActionInputs value = inputs();
        return value.controller_input().has_value() ||
            value.keyboard_input().has_value() || value.mouse_input().has_value();
    }
    [[nodiscard]] bool is_fully_bound() const noexcept {
        const ActionInputs value = inputs();
        bool supported = false;
        bool complete = true;
        if (value.controller_supported()) {
            supported = true;
            complete = complete && value.controller_input().has_value();
        }
        if (value.keyboard_supported()) {
            supported = true;
            complete = complete && value.keyboard_input().has_value();
        }
        if (value.mouse_supported()) {
            supported = true;
            complete = complete && value.mouse_input().has_value();
        }
        return supported && complete;
    }
    [[nodiscard]] bool is_default() const noexcept {
        ActionInputs current{};
        ActionInputs defaults{};
        return query_inputs(current) == ERUI_OK &&
            query_default_inputs(defaults) == ERUI_OK && current == defaults;
    }

    [[nodiscard]] InputActionDevices devices() const noexcept;

private:
    friend class InputSection;
    friend class AssignmentChange;
    friend class InputActionDevices;
    template <typename, ERUI_InputDevices> friend class InputDeviceAssignment;

    InputAction(ERUI_Api api, ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action) noexcept
        : api_(api), provider_(provider), action_(action) {}
    ERUI_Result reset_device(ERUI_InputDevices device) const noexcept {
        return valid() ? api_.reset_action_inputs(provider_, action_, device)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }

    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    ERUI_InputActionHandle action_{};
};

template <typename Input, ERUI_InputDevices Device>
class InputDeviceAssignment {
public:
    InputDeviceAssignment() = default;

    ERUI_Result bind(Input input) const noexcept {
        ActionInputs update{};
        set(update, input);
        return action_.bind(update);
    }
    ERUI_Result unbind() const noexcept {
        ActionInputs update{};
        set_unbound(update);
        return action_.bind(update);
    }
    ERUI_Result reset_to_default() const noexcept {
        return action_.reset_device(Device);
    }
    [[nodiscard]] bool is_supported() const noexcept {
        ActionInputs value{};
        return action_.query_inputs(value) == ERUI_OK && supported(value);
    }
    [[nodiscard]] bool is_bound() const noexcept {
        return input().has_value();
    }
    [[nodiscard]] bool is_default() const noexcept {
        ActionInputs current{};
        ActionInputs defaults{};
        return action_.query_inputs(current) == ERUI_OK &&
            action_.query_default_inputs(defaults) == ERUI_OK &&
            state(current) == state(defaults) && input(current) == input(defaults);
    }
    [[nodiscard]] std::optional<Input> input() const noexcept {
        ActionInputs value{};
        return action_.query_inputs(value) == ERUI_OK
            ? input(value) : std::nullopt;
    }
    [[nodiscard]] std::optional<Input> default_input() const noexcept {
        ActionInputs value{};
        return action_.query_default_inputs(value) == ERUI_OK
            ? input(value) : std::nullopt;
    }

private:
    friend class InputActionDevices;
    explicit InputDeviceAssignment(InputAction action) noexcept
        : action_(action) {}

    static bool supported(const ActionInputs& value) noexcept {
        if constexpr (Device == ERUI_INPUT_DEVICE_CONTROLLER) {
            return value.controller_supported();
        } else if constexpr (Device == ERUI_INPUT_DEVICE_KEYBOARD) {
            return value.keyboard_supported();
        } else {
            return value.mouse_supported();
        }
    }
    static InputSlotState state(const ActionInputs& value) noexcept {
        if constexpr (Device == ERUI_INPUT_DEVICE_CONTROLLER) {
            return value.controller_state();
        } else if constexpr (Device == ERUI_INPUT_DEVICE_KEYBOARD) {
            return value.keyboard_state();
        } else {
            return value.mouse_state();
        }
    }
    static std::optional<Input> input(const ActionInputs& value) noexcept {
        if constexpr (Device == ERUI_INPUT_DEVICE_CONTROLLER) {
            return value.controller_input();
        } else if constexpr (Device == ERUI_INPUT_DEVICE_KEYBOARD) {
            return value.keyboard_input();
        } else {
            return value.mouse_input();
        }
    }
    static void set(ActionInputs& value, Input input_value) noexcept {
        if constexpr (Device == ERUI_INPUT_DEVICE_CONTROLLER) {
            value.controller(input_value);
        } else if constexpr (Device == ERUI_INPUT_DEVICE_KEYBOARD) {
            value.keyboard(input_value);
        } else {
            value.mouse(input_value);
        }
    }
    static void set_unbound(ActionInputs& value) noexcept {
        if constexpr (Device == ERUI_INPUT_DEVICE_CONTROLLER) {
            value.controller();
        } else if constexpr (Device == ERUI_INPUT_DEVICE_KEYBOARD) {
            value.keyboard();
        } else {
            value.mouse();
        }
    }

    InputAction action_{};
};

using ControllerActionInput = InputDeviceAssignment<ControllerButton,
    ERUI_INPUT_DEVICE_CONTROLLER>;
using KeyboardActionInput = InputDeviceAssignment<KeyboardKey,
    ERUI_INPUT_DEVICE_KEYBOARD>;
using MouseActionInput = InputDeviceAssignment<MouseButton,
    ERUI_INPUT_DEVICE_MOUSE>;

class InputActionDevices {
public:
    [[nodiscard]] ControllerActionInput controller() const noexcept {
        return ControllerActionInput(action_);
    }
    [[nodiscard]] KeyboardActionInput keyboard() const noexcept {
        return KeyboardActionInput(action_);
    }
    [[nodiscard]] MouseActionInput mouse() const noexcept {
        return MouseActionInput(action_);
    }

private:
    friend class InputAction;
    explicit InputActionDevices(InputAction action) noexcept : action_(action) {}
    InputAction action_{};
};

inline InputActionDevices InputAction::devices() const noexcept {
    return InputActionDevices(*this);
}

class AssignmentChange {
public:
    [[nodiscard]] std::string_view action_id() const noexcept {
        if (!change_ || (change_->action_id.length != 0 &&
                !change_->action_id.data)) return {};
        return {change_->action_id.data ? change_->action_id.data : "",
            change_->action_id.length};
    }
    [[nodiscard]] InputAction action() const noexcept {
        return change_ ? InputAction(api_, provider_, change_->action)
            : InputAction{};
    }
    [[nodiscard]] ActionInputs previous() const noexcept {
        return change_ && detail::valid_action_inputs(change_->previous)
            ? detail::ActionInputsAccess::from_native(change_->previous)
            : ActionInputs{};
    }
    [[nodiscard]] ActionInputs current() const noexcept {
        return change_ && detail::valid_action_inputs(change_->current)
            ? detail::ActionInputsAccess::from_native(change_->current)
            : ActionInputs{};
    }
    [[nodiscard]] AssignmentChangeReason reason() const noexcept {
        return change_ ? static_cast<AssignmentChangeReason>(change_->reason)
            : AssignmentChangeReason::unknown;
    }
    [[nodiscard]] bool controller_changed() const noexcept {
        return changed(ERUI_INPUT_DEVICE_CONTROLLER);
    }
    [[nodiscard]] bool keyboard_changed() const noexcept {
        return changed(ERUI_INPUT_DEVICE_KEYBOARD);
    }
    [[nodiscard]] bool mouse_changed() const noexcept {
        return changed(ERUI_INPUT_DEVICE_MOUSE);
    }

private:
    friend class AssignmentChangeRange;
    friend class StorageSection;
    AssignmentChange(ERUI_Api api, ERUI_ProviderHandle provider,
        const ERUI_AssignmentChange* change) noexcept
        : api_(api), provider_(provider), change_(change) {}
    [[nodiscard]] bool changed(ERUI_InputDevices device) const noexcept {
        return change_ && (change_->changed_devices & device) == device;
    }
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    const ERUI_AssignmentChange* change_{};
};

class AssignmentChangeRange {
public:
    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = AssignmentChange;
        using iterator_category = std::forward_iterator_tag;

        AssignmentChange operator*() const noexcept {
            return AssignmentChange(api_, provider_, current_);
        }
        Iterator& operator++() noexcept { ++current_; return *this; }
        Iterator operator++(int) noexcept {
            Iterator copy = *this;
            ++*this;
            return copy;
        }
        friend bool operator==(const Iterator& left,
            const Iterator& right) noexcept { return left.current_ == right.current_; }
        friend bool operator!=(const Iterator& left,
            const Iterator& right) noexcept { return !(left == right); }

    private:
        friend class AssignmentChangeRange;
        Iterator(ERUI_Api api, ERUI_ProviderHandle provider,
            const ERUI_AssignmentChange* current) noexcept
            : api_(api), provider_(provider), current_(current) {}
        ERUI_Api api_{};
        ERUI_ProviderHandle provider_{};
        const ERUI_AssignmentChange* current_{};
    };

    [[nodiscard]] Iterator begin() const noexcept {
        return Iterator(api_, provider_, changes_);
    }
    [[nodiscard]] Iterator end() const noexcept {
        return Iterator(api_, provider_, changes_ ? changes_ + count_ : nullptr);
    }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

private:
    friend class AssignmentsChangedEvent;
    AssignmentChangeRange(ERUI_Api api, ERUI_ProviderHandle provider,
        const ERUI_AssignmentChange* changes, std::uint32_t count) noexcept
        : api_(api), provider_(provider), changes_(changes), count_(count) {}
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    const ERUI_AssignmentChange* changes_{};
    std::uint32_t count_{};
};

class AssignmentsChangedEvent {
public:
    [[nodiscard]] ERUI_ProviderHandle provider_handle() const noexcept {
        return context_ ? context_->provider : ERUI_INVALID_PROVIDER;
    }
    [[nodiscard]] AssignmentChangeRange changes() const noexcept {
        return context_ ? AssignmentChangeRange(api_, context_->provider,
            context_->changes, context_->change_count)
            : AssignmentChangeRange({}, ERUI_INVALID_PROVIDER, nullptr, 0);
    }

private:
    template <auto> friend struct detail::StaticAssignmentsChangedState;
    template <auto, typename> friend struct detail::StatefulAssignmentsChangedState;
    template <auto, typename> friend struct detail::OwnedAssignmentsChangedState;
    friend class StorageSection;
    AssignmentsChangedEvent(ERUI_Api api,
        const ERUI_AssignmentsChangedContext* context) noexcept
        : api_(api), context_(context) {}
    ERUI_Api api_{};
    const ERUI_AssignmentsChangedContext* context_{};
};

template <typename T>
class StorageRead {
public:
    [[nodiscard]] ERUI_Result result() const noexcept { return result_; }
    [[nodiscard]] bool found() const noexcept {
        return result_ == ERUI_OK && value_.has_value();
    }
    explicit operator bool() const noexcept { return found(); }
    [[nodiscard]] const T& value() const& { return value_.value(); }
    [[nodiscard]] T& value() & { return value_.value(); }
    [[nodiscard]] T&& value() && { return std::move(value_).value(); }

private:
    friend class StorageSection;
    ERUI_Result result_{ERUI_NOT_FOUND};
    std::optional<T> value_{};
};

class StorageSection;

class Storage {
public:
    Storage() = default;

    [[nodiscard]] bool valid() const noexcept {
        return provider_ != ERUI_INVALID_PROVIDER &&
            storage_ != ERUI_INVALID_STORAGE && api_.storage_load &&
            api_.storage_save && api_.storage_get_info;
    }
    explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] StorageHandle handle() const noexcept { return storage_; }
    ERUI_Result load() const noexcept {
        return valid() ? api_.storage_load(provider_, storage_)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result save() const noexcept {
        return valid() ? api_.storage_save(provider_, storage_)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result query_info(StorageInfo& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        ERUI_StorageInfo native{};
        native.size = sizeof(native);
        const ERUI_Result result =
            api_.storage_get_info(provider_, storage_, &native);
        if (result != ERUI_OK) return result;
        if (native.flags != 0 || native.reserved[0] != 0 ||
            native.reserved[1] != 0 || native.loaded > 1 || native.dirty > 1) {
            return ERUI_INTERNAL_ERROR;
        }
        output.loaded = native.loaded != 0;
        output.dirty = native.dirty != 0;
        output.current_revision = native.current_revision;
        output.last_saved_revision = native.last_saved_revision;
        return ERUI_OK;
    }
    [[nodiscard]] bool is_loaded() const noexcept {
        StorageInfo value{};
        return query_info(value) == ERUI_OK && value.loaded;
    }
    [[nodiscard]] bool is_dirty() const noexcept {
        StorageInfo value{};
        return query_info(value) == ERUI_OK && value.dirty;
    }
    [[nodiscard]] std::uint64_t current_revision() const noexcept {
        StorageInfo value{};
        return query_info(value) == ERUI_OK ? value.current_revision : 0;
    }
    [[nodiscard]] std::uint64_t last_saved_revision() const noexcept {
        StorageInfo value{};
        return query_info(value) == ERUI_OK ? value.last_saved_revision : 0;
    }

    [[nodiscard]] StorageSection section(std::string_view name) const noexcept;

private:
    friend class Menu;
    friend class StorageSection;
    Storage(ERUI_Api api, ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage) noexcept
        : api_(api), provider_(provider), storage_(storage) {}
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
    ERUI_StorageHandle storage_{};
};

namespace detail {

template <typename> struct dependent_false : std::false_type {};

template <typename T>
inline ERUI_Result encode_storage_value(const T& value,
    std::string& output) noexcept {
    try {
        if constexpr (std::is_same_v<T, std::string>) {
            output = value;
        } else if constexpr (std::is_same_v<T, std::string_view>) {
            output.assign(value.data(), value.size());
        } else if constexpr (std::is_same_v<T, bool>) {
            output = value ? "true" : "false";
        } else if constexpr (std::is_integral_v<T>) {
            char buffer[(std::numeric_limits<T>::digits10) + 4]{};
            const auto converted = std::to_chars(
                std::begin(buffer), std::end(buffer), value);
            if (converted.ec != std::errc{}) return ERUI_INVALID_ARGUMENT;
            output.assign(buffer, converted.ptr);
        } else if constexpr (std::is_floating_point_v<T>) {
            char buffer[(std::numeric_limits<T>::max_digits10) + 16]{};
            const auto converted = std::to_chars(
                std::begin(buffer), std::end(buffer), value,
                std::chars_format::general,
                std::numeric_limits<T>::max_digits10);
            if (converted.ec != std::errc{}) return ERUI_INVALID_ARGUMENT;
            output.assign(buffer, converted.ptr);
        } else {
            static_assert(dependent_false<T>::value,
                "Unsupported ERNativeUI storage value type");
        }
        return ERUI_OK;
    } catch (...) {
        return ERUI_OUT_OF_MEMORY;
    }
}

template <typename T>
inline ERUI_Result decode_storage_value(std::string_view input,
    T& output) noexcept {
    if constexpr (std::is_same_v<T, std::string>) {
        try {
            output.assign(input.data(), input.size());
            return ERUI_OK;
        } catch (...) {
            return ERUI_OUT_OF_MEMORY;
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (input == "true") { output = true; return ERUI_OK; }
        if (input == "false") { output = false; return ERUI_OK; }
        return ERUI_STORAGE_FORMAT_ERROR;
    } else if constexpr (std::is_integral_v<T> ||
        std::is_floating_point_v<T>) {
        T parsed{};
        const auto converted = std::from_chars(
            input.data(), input.data() + input.size(), parsed);
        if (converted.ec != std::errc{} ||
            converted.ptr != input.data() + input.size()) {
            return ERUI_STORAGE_FORMAT_ERROR;
        }
        output = parsed;
        return ERUI_OK;
    } else {
        static_assert(dependent_false<T>::value,
            "Unsupported ERNativeUI storage value type");
    }
}

} // namespace detail

class StorageSection {
public:
    StorageSection() = default;

    [[nodiscard]] bool valid() const noexcept {
        return storage_.valid() && !section_.empty();
    }
    explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] Storage config() const noexcept { return storage_; }
    [[nodiscard]] std::string_view name() const noexcept { return section_; }

    template <typename T>
    [[nodiscard]] StorageRead<T> get(std::string_view key) const noexcept {
        StorageRead<T> output{};
        if constexpr (std::is_same_v<T, ActionInputs>) {
            if (!valid_key(key) || !storage_.api_.storage_get_action_inputs) {
                output.result_ = valid() ? ERUI_INVALID_ARGUMENT
                    : ERUI_INVALID_HANDLE;
                return output;
            }
            const ERUI_StorageKey native_key = make_key(key);
            ERUI_ActionInputs native{};
            native.size = sizeof(native);
            output.result_ = storage_.api_.storage_get_action_inputs(
                storage_.provider_, storage_.storage_, &native_key, &native);
            if (output.result_ == ERUI_OK) {
                if (!detail::valid_action_inputs(native)) {
                    output.result_ = ERUI_STORAGE_FORMAT_ERROR;
                } else {
                    output.value_ = detail::ActionInputsAccess::from_native(native);
                }
            }
            return output;
        } else {
            std::string raw{};
            output.result_ = get_utf8(key, raw);
            if (output.result_ != ERUI_OK) return output;
            T value{};
            output.result_ = detail::decode_storage_value<T>(raw, value);
            if (output.result_ == ERUI_OK) output.value_ = std::move(value);
            return output;
        }
    }

    ERUI_Result set(std::string_view key,
        const ActionInputs& value) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!valid_key(key) || !storage_.api_.storage_set_action_inputs) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_ActionInputs& native = detail::ActionInputsAccess::native(value);
        if (!detail::valid_action_inputs(native)) return ERUI_INVALID_ARGUMENT;
        const ERUI_StorageKey native_key = make_key(key);
        return storage_.api_.storage_set_action_inputs(
            storage_.provider_, storage_.storage_, &native_key, &native);
    }
    ERUI_Result set(std::string_view key, std::string_view value) const noexcept {
        return set_utf8(key, value);
    }
    ERUI_Result set(std::string_view key, const char* value) const noexcept {
        return value ? set_utf8(key, value)
            : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
    }
    template <typename T,
        std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    ERUI_Result set(std::string_view key, T value) const noexcept {
        std::string encoded{};
        const ERUI_Result result = detail::encode_storage_value(value, encoded);
        return result == ERUI_OK ? set_utf8(key, encoded) : result;
    }
    ERUI_Result erase(std::string_view key) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!valid_key(key) || !storage_.api_.storage_erase) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_StorageKey native_key = make_key(key);
        return storage_.api_.storage_erase(
            storage_.provider_, storage_.storage_, &native_key);
    }
    ERUI_Result apply(const AssignmentChange& change) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!change.change_ ||
            !storage_.api_.storage_apply_assignment_changes) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_StringView section = detail::string_view(section_);
        return storage_.api_.storage_apply_assignment_changes(
            storage_.provider_, storage_.storage_, &section,
            change.change_, 1);
    }
    ERUI_Result apply(const AssignmentsChangedEvent& event) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!event.context_ ||
            !storage_.api_.storage_apply_assignment_changes) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_StringView section = detail::string_view(section_);
        return storage_.api_.storage_apply_assignment_changes(
            storage_.provider_, storage_.storage_, &section,
            event.context_->changes, event.context_->change_count);
    }

private:
    friend class Storage;
    StorageSection(Storage storage, std::string section) noexcept
        : storage_(storage), section_(std::move(section)) {}
    [[nodiscard]] bool valid_key(std::string_view key) const noexcept {
        return valid() && detail::valid_storage_identifier(key);
    }
    [[nodiscard]] ERUI_StorageKey make_key(std::string_view key) const noexcept {
        ERUI_StorageKey result{};
        result.size = sizeof(result);
        result.section = detail::string_view(section_);
        result.key = detail::string_view(key);
        return result;
    }
    ERUI_Result get_utf8(std::string_view key,
        std::string& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!valid_key(key) || !storage_.api_.storage_get_utf8) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_StorageKey native_key = make_key(key);
        try {
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                std::uint32_t required{};
                ERUI_Result result = storage_.api_.storage_get_utf8(
                    storage_.provider_, storage_.storage_, &native_key,
                    nullptr, 0, &required);
                if (result != ERUI_OK) return result;
                std::string value(required, '\0');
                if (required == 0) { output.clear(); return ERUI_OK; }
                std::uint32_t actual{};
                result = storage_.api_.storage_get_utf8(
                    storage_.provider_, storage_.storage_, &native_key,
                    value.data(), static_cast<std::uint32_t>(value.size()),
                    &actual);
                if (result == ERUI_BUFFER_TOO_SMALL) continue;
                if (result != ERUI_OK) return result;
                if (actual > value.size()) return ERUI_INTERNAL_ERROR;
                value.resize(actual);
                output.swap(value);
                return ERUI_OK;
            }
            return ERUI_BUFFER_TOO_SMALL;
        } catch (...) {
            return ERUI_OUT_OF_MEMORY;
        }
    }
    ERUI_Result set_utf8(std::string_view key,
        std::string_view value) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!valid_key(key) || value.size() > ERUI_STORAGE_MAX_VALUE_BYTES ||
            !storage_.api_.storage_set_utf8) return ERUI_INVALID_ARGUMENT;
        const ERUI_StorageKey native_key = make_key(key);
        const ERUI_StringView native_value = detail::string_view(value);
        return storage_.api_.storage_set_utf8(
            storage_.provider_, storage_.storage_, &native_key, &native_value);
    }

    Storage storage_{};
    std::string section_{};
};

inline StorageSection Storage::section(std::string_view name) const noexcept {
    if (!valid() || !detail::valid_storage_identifier(name)) {
        return {};
    }
    try {
        return StorageSection(*this, std::string(name));
    } catch (...) {
        return {};
    }
}

inline void persist_assignment_changes(StorageSection& storage,
    const AssignmentsChangedEvent& event) noexcept {
    if (storage.apply(event) == ERUI_OK) (void)storage.config().save();
}

namespace detail {

inline bool valid_assignments_changed_context(
    const ERUI_AssignmentsChangedContext* context) noexcept {
    if (!context || context->size < sizeof(*context) || context->flags != 0 ||
        context->reserved != 0 || context->provider == ERUI_INVALID_PROVIDER ||
        (context->change_count != 0 && !context->changes)) return false;
    for (std::uint32_t index = 0; index < context->change_count; ++index) {
        const ERUI_AssignmentChange& change = context->changes[index];
        if (change.size < sizeof(change) || change.flags != 0 ||
            change.action == ERUI_INVALID_INPUT_ACTION ||
            (change.action_id.length != 0 && !change.action_id.data) ||
            !valid_action_inputs(change.previous) ||
            !valid_action_inputs(change.current) ||
            change.reason > ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS ||
            change.changed_devices == ERUI_INPUT_DEVICE_NONE ||
            (change.changed_devices & ~ERUI_INPUT_DEVICE_ALL) != 0 ||
            change.reserved[0] != 0 || change.reserved[1] != 0) return false;
    }
    return true;
}

template <auto Function>
struct StaticAssignmentsChangedState final : RetainedCallbackState {
    ERUI_Api api{};
    static void ERUI_CALL invoke(void* opaque,
        const ERUI_AssignmentsChangedContext* context) noexcept {
        auto* self = static_cast<StaticAssignmentsChangedState*>(opaque);
        if (self && valid_assignments_changed_context(context)) {
            Function(AssignmentsChangedEvent(self->api, context));
        }
    }
};

template <auto Function, typename State>
struct StatefulAssignmentsChangedState final : RetainedCallbackState {
    ERUI_Api api{};
    State* state{};
    static void ERUI_CALL invoke(void* opaque,
        const ERUI_AssignmentsChangedContext* context) noexcept {
        auto* self = static_cast<StatefulAssignmentsChangedState*>(opaque);
        if (self && self->state && valid_assignments_changed_context(context)) {
            Function(*self->state, AssignmentsChangedEvent(self->api, context));
        }
    }
};

template <auto Function, typename State>
struct OwnedAssignmentsChangedState final : RetainedCallbackState {
    ERUI_Api api{};
    State state{};
    static void ERUI_CALL invoke(void* opaque,
        const ERUI_AssignmentsChangedContext* context) noexcept {
        auto* self = static_cast<OwnedAssignmentsChangedState*>(opaque);
        if (self && valid_assignments_changed_context(context)) {
            Function(self->state, AssignmentsChangedEvent(self->api, context));
        }
    }
};

} // namespace detail

class InputSection {
public:
    InputSection() = default;

    InputAction add_action(
        std::string_view action_id,
        std::wstring_view label,
        const ActionInputs& default_inputs,
        InputActionActivatedCallback callback,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!draft_->api_.add_input_action ||
            (draft_->api_.capabilities & ERUI_CAP_INPUT_BINDINGS) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_input_action");
            return {};
        }
        const ERUI_ActionInputs& native_defaults =
            detail::ActionInputsAccess::native(default_inputs);
        if (!detail::valid_machine_identifier(action_id) || label.empty() ||
            !callback ||
            default_inputs.empty() || !detail::valid_action_inputs(native_defaults) ||
            !detail::view_size_fits(action_id.size()) ||
            !detail::view_size_fits(label.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_input_action");
            return {};
        }

        ERUI_InputActionDesc description{};
        description.size = sizeof(description);
        description.action_id = detail::string_view(action_id);
        description.label = detail::utf16_view(label);
        description.default_inputs = native_defaults;
        description.activated_callback = callback;
        description.user_data = user_data;
        ERUI_InputActionHandle action{ERUI_INVALID_INPUT_ACTION};
        const ERUI_Result result = draft_->api_.add_input_action(
            draft_->provider_, section_, &description, &action);
        draft_->record(result, L"add_input_action");
        if (result == ERUI_OK && action == ERUI_INVALID_INPUT_ACTION) {
            draft_->record(ERUI_INTERNAL_ERROR, L"add_input_action");
        }
        return result == ERUI_OK
            ? InputAction(draft_->api_, draft_->provider_, action)
            : InputAction{};
    }

    template <auto Function>
    InputAction add_action(std::string_view action_id,
        std::wstring_view label,
        const ActionInputs& default_inputs = ActionInputs::all()) noexcept {
        using Expected = void (*)(const ActionActivation&) noexcept;
        static_assert(std::is_same_v<decltype(Function), Expected>,
            "Action callback must be void(const ActionActivation&) noexcept");
        return add_action(action_id, label, default_inputs,
            &action_thunk<Function>, nullptr);
    }
    template <auto Function, typename State>
    InputAction add_action(std::string_view action_id,
        std::wstring_view label,
        const ActionInputs& default_inputs,
        State& state) noexcept {
        using Expected = void (*)(State&, const ActionActivation&) noexcept;
        static_assert(std::is_same_v<decltype(Function), Expected>,
            "Stateful action callback must be void(State&, const ActionActivation&) noexcept");
        return add_action(action_id, label, default_inputs,
            &stateful_action_thunk<Function, State>, &state);
    }
    [[nodiscard]] bool valid() const noexcept {
        return draft_ && draft_->open() &&
            section_ != ERUI_INVALID_INPUT_SECTION;
    }

private:
    friend class InputBindings;
    InputSection(std::shared_ptr<detail::Draft> draft,
        ERUI_InputSectionHandle section) noexcept
        : draft_(std::move(draft)), section_(section) {}
    template <auto Function>
    static void ERUI_CALL action_thunk(void*,
        const ERUI_InputActionActivatedContext* context) noexcept {
        ActionActivation activation{};
        if (detail::make_action_activation(context, activation)) {
            Function(activation);
        }
    }
    template <auto Function, typename State>
    static void ERUI_CALL stateful_action_thunk(void* user_data,
        const ERUI_InputActionActivatedContext* context) noexcept {
        auto* state = static_cast<State*>(user_data);
        ActionActivation activation{};
        if (state && detail::make_action_activation(context, activation)) {
            Function(*state, activation);
        }
    }
    std::shared_ptr<detail::Draft> draft_{};
    ERUI_InputSectionHandle section_{};
};

class InputBindings {
public:
    InputBindings() = default;

    InputSection add_section(std::wstring_view label) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!draft_->api_.add_input_section ||
            (draft_->api_.capabilities & ERUI_CAP_INPUT_BINDINGS) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_input_section");
            return {};
        }
        if (label.empty() || !detail::view_size_fits(label.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_input_section");
            return {};
        }
        ERUI_InputSectionDesc description{};
        description.size = sizeof(description);
        description.label = detail::utf16_view(label);
        ERUI_InputSectionHandle section{ERUI_INVALID_INPUT_SECTION};
        const ERUI_Result result = draft_->api_.add_input_section(
            draft_->provider_, &description, &section);
        draft_->record(result, L"add_input_section");
        if (result == ERUI_OK && section == ERUI_INVALID_INPUT_SECTION) {
            draft_->record(ERUI_INTERNAL_ERROR, L"add_input_section");
        }
        return result == ERUI_OK ? InputSection(draft_, section) : InputSection{};
    }

    ERUI_Result on_assignments_changed(
        AssignmentsChangedCallback callback,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return ERUI_INVALID_HANDLE;
        }
        if (!draft_->api_.set_assignments_changed_handler || !callback) {
            draft_->record(callback ? ERUI_NOT_SUPPORTED : ERUI_INVALID_ARGUMENT,
                L"set_assignments_changed_handler");
            return callback ? ERUI_NOT_SUPPORTED : ERUI_INVALID_ARGUMENT;
        }
        ERUI_AssignmentsChangedHandlerDesc description{};
        description.size = sizeof(description);
        description.callback = callback;
        description.user_data = user_data;
        const ERUI_Result result = draft_->api_.set_assignments_changed_handler(
            draft_->provider_, &description);
        draft_->record(result, L"set_assignments_changed_handler");
        return result;
    }

    template <auto Function>
    ERUI_Result on_assignments_changed() noexcept {
        using Expected = void (*)(const AssignmentsChangedEvent&) noexcept;
        static_assert(std::is_same_v<decltype(Function), Expected>,
            "Assignment callback must be void(const AssignmentsChangedEvent&) noexcept");
        try {
            using State = detail::StaticAssignmentsChangedState<Function>;
            auto state = std::make_unique<State>();
            state->api = draft_ ? draft_->api_ : ERUI_Api{};
            return install_callback_state(std::move(state), &State::invoke);
        } catch (...) {
            return ERUI_OUT_OF_MEMORY;
        }
    }
    template <auto Function, typename State,
        std::enable_if_t<!std::is_same_v<
            std::decay_t<State>, StorageSection>, int> = 0>
    ERUI_Result on_assignments_changed(State& state) noexcept {
        using Expected = void (*)(State&, const AssignmentsChangedEvent&) noexcept;
        static_assert(std::is_same_v<decltype(Function), Expected>,
            "Stateful assignment callback must be void(State&, const AssignmentsChangedEvent&) noexcept");
        try {
            using CallbackState =
                detail::StatefulAssignmentsChangedState<Function, State>;
            auto callback_state = std::make_unique<CallbackState>();
            callback_state->api = draft_ ? draft_->api_ : ERUI_Api{};
            callback_state->state = &state;
            return install_callback_state(
                std::move(callback_state), &CallbackState::invoke);
        } catch (...) {
            return ERUI_OUT_OF_MEMORY;
        }
    }
    template <auto Function>
    ERUI_Result on_assignments_changed(StorageSection storage) noexcept {
        using Expected = void (*)(StorageSection&,
            const AssignmentsChangedEvent&) noexcept;
        static_assert(std::is_same_v<decltype(Function), Expected>,
            "Storage assignment callback must be void(StorageSection&, const AssignmentsChangedEvent&) noexcept");
        try {
            using CallbackState =
                detail::OwnedAssignmentsChangedState<Function, StorageSection>;
            auto callback_state = std::make_unique<CallbackState>();
            callback_state->api = draft_ ? draft_->api_ : ERUI_Api{};
            callback_state->state = std::move(storage);
            return install_callback_state(
                std::move(callback_state), &CallbackState::invoke);
        } catch (...) {
            return ERUI_OUT_OF_MEMORY;
        }
    }
    ERUI_Result persist_assignments_to(StorageSection storage) noexcept {
        return on_assignments_changed<&persist_assignment_changes>(
            std::move(storage));
    }
    [[nodiscard]] bool valid() const noexcept {
        return draft_ && draft_->open() && !draft_->failed_;
    }

private:
    friend class Menu;
    explicit InputBindings(std::shared_ptr<detail::Draft> draft) noexcept
        : draft_(std::move(draft)) {}
    template <typename State>
    ERUI_Result install_callback_state(std::unique_ptr<State> state,
        AssignmentsChangedCallback callback) noexcept {
        if (!state) return ERUI_OUT_OF_MEMORY;
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return ERUI_INVALID_HANDLE;
        }
        State* const raw = state.get();
        if (!draft_->retain_callback_state(std::move(state))) {
            draft_->record(ERUI_OUT_OF_MEMORY, L"retain assignment callback");
            return ERUI_OUT_OF_MEMORY;
        }
        const ERUI_Result result = on_assignments_changed(callback, raw);
        if (result != ERUI_OK) {
            draft_->discard_callback_state(raw);
            return result;
        }
        return ERUI_OK;
    }
    std::shared_ptr<detail::Draft> draft_{};
};

class Menu {
public:
    Page root() noexcept { return Page(draft_, draft_ ? draft_->root_ : 0); }
    Page page(BuiltinPage builtin_page) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!draft_->api_.get_builtin_page ||
            (draft_->api_.capabilities & ERUI_CAP_BUILTIN_PAGES) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"get_builtin_page");
            return {};
        }
        ERUI_PageHandle page_handle{ERUI_INVALID_PAGE};
        const ERUI_Result result = draft_->api_.get_builtin_page(
            draft_->provider_,
            static_cast<ERUI_BuiltinPage>(builtin_page),
            &page_handle);
        draft_->record(result, L"get_builtin_page");
        if (result != ERUI_OK || page_handle == ERUI_INVALID_PAGE) return {};
        return Page(draft_, page_handle);
    }
    InputBindings input_bindings() noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_ ||
            !draft_->api_.add_input_section || !draft_->api_.add_input_action ||
            (draft_->api_.capabilities & ERUI_CAP_INPUT_BINDINGS) == 0) {
            if (draft_) draft_->record(ERUI_NOT_SUPPORTED, L"input_bindings");
            return {};
        }
        return InputBindings(draft_);
    }
    Storage storage(
        const StorageOptions& options = StorageOptions::provider_default()) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!draft_->api_.open_storage ||
            (draft_->api_.capabilities & ERUI_CAP_STORAGE) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"open_storage");
            return {};
        }
        if (!detail::view_size_fits(options.path.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"open_storage");
            return {};
        }
        ERUI_StorageDesc description{};
        description.size = sizeof(description);
        description.location = static_cast<ERUI_StorageLocation>(
            options.location);
        description.path = detail::utf16_view(options.path);
        ERUI_StorageHandle handle{ERUI_INVALID_STORAGE};
        const ERUI_Result result = draft_->api_.open_storage(
            draft_->provider_, &description, &handle);
        draft_->record(result, L"open_storage");
        if (result == ERUI_OK && handle == ERUI_INVALID_STORAGE) {
            draft_->record(ERUI_INTERNAL_ERROR, L"open_storage");
        }
        return result == ERUI_OK
            ? Storage(draft_->api_, draft_->provider_, handle)
            : Storage{};
    }
    [[nodiscard]] bool supports(Capability capability) const noexcept {
        const auto bit = static_cast<ERUI_Capabilities>(capability);
        return draft_ && (draft_->api_.capabilities & bit) == bit;
    }
    [[nodiscard]] std::uint32_t api_version() const noexcept {
        return draft_ ? draft_->api_.api_version : 0;
    }
private:
    friend class Connection;
    explicit Menu(std::shared_ptr<detail::Draft> draft) noexcept
        : draft_(std::move(draft)) {}
    std::shared_ptr<detail::Draft> draft_{};
};

class Registration {
public:
    Registration() = default;

    ERUI_Result set_value(RowHandle row, std::uint8_t value) const noexcept {
        return valid() ? api_.set_row_value(provider_, row, value)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result get_value(RowHandle row, std::uint8_t& value) const noexcept {
        return valid() ? api_.get_row_value(provider_, row, &value)
            : static_cast<ERUI_Result>(ERUI_INVALID_HANDLE);
    }
    ERUI_Result set_text(
        RowHandle row,
        std::wstring_view value) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::text_input) ||
            !api_.set_text_input_value) {
            return ERUI_NOT_SUPPORTED;
        }
        if (!detail::view_size_fits(value.size()) ||
            value.size() > ERUI_TEXT_INPUT_MAX_LENGTH) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_Utf16View native_value = detail::utf16_view(value);
        return api_.set_text_input_value(provider_, row, &native_value);
    }
    ERUI_Result get_text(
        RowHandle row,
        std::wstring& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::text_input) ||
            !api_.get_text_input_value) {
            return ERUI_NOT_SUPPORTED;
        }
        try {
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                std::uint32_t required{};
                ERUI_Result result = api_.get_text_input_value(
                    provider_, row, nullptr, 0, &required);
                if (result != ERUI_OK) return result;
                if (required == 0) {
                    std::wstring empty{};
                    output.swap(empty);
                    return ERUI_OK;
                }

                std::vector<std::uint16_t> storage(required);
                std::uint32_t actual{};
                result = api_.get_text_input_value(
                    provider_,
                    row,
                    storage.data(),
                    static_cast<std::uint32_t>(storage.size()),
                    &actual);
                if (result == ERUI_BUFFER_TOO_SMALL) continue;
                if (result != ERUI_OK) return result;
                if (actual > storage.size()) return ERUI_INTERNAL_ERROR;

                std::wstring value(actual, L'\0');
                if (actual != 0) {
                    std::memcpy(
                        value.data(),
                        storage.data(),
                        static_cast<std::size_t>(actual) *
                            sizeof(std::uint16_t));
                }
                output.swap(value);
                return ERUI_OK;
            }
            return ERUI_BUFFER_TOO_SMALL;
        } catch (...) {
            return ERUI_OUT_OF_MEMORY;
        }
    }
    ERUI_Result set_color(
        RowHandle row,
        Color value) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::color_picker) ||
            !api_.set_color_picker_value) {
            return ERUI_NOT_SUPPORTED;
        }
        const ERUI_Color native_value = detail::native_color(value);
        return api_.set_color_picker_value(provider_, row, &native_value);
    }
    ERUI_Result get_color(
        RowHandle row,
        Color& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::color_picker) ||
            !api_.get_color_picker_value) {
            return ERUI_NOT_SUPPORTED;
        }
        ERUI_Color native_value{};
        const ERUI_Result result = api_.get_color_picker_value(
            provider_, row, &native_value);
        if (result != ERUI_OK) return result;
        if (native_value.reserved != 0) return ERUI_INTERNAL_ERROR;
        output = detail::color_from_native(native_value);
        return ERUI_OK;
    }
    [[nodiscard]] bool supports(Capability capability) const noexcept {
        const auto bit = static_cast<ERUI_Capabilities>(capability);
        return valid() && (api_.capabilities & bit) == bit;
    }
    [[nodiscard]] std::uint32_t api_version() const noexcept {
        return valid() ? api_.api_version : 0;
    }
    ERUI_Result alert(
        std::wstring_view message,
        AlertOptions options,
        AlertCallback callback = nullptr,
        void* user_data = nullptr) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (message.empty() || !detail::view_size_fits(message.size()) ||
            (!callback && user_data) ||
            static_cast<std::uint32_t>(options.buttons) >
                ERUI_ALERT_BUTTONS_DISMISS_ONLY ||
            static_cast<std::uint32_t>(options.placement) >
                ERUI_ALERT_PLACEMENT_CENTER) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::unique_ptr<detail::AlertCallbackState> callback_state{};
        if (callback) {
            try {
                callback_state =
                    std::make_unique<detail::AlertCallbackState>();
                callback_state->callback = callback;
                callback_state->user_data = user_data;
            } catch (...) {
                return ERUI_OUT_OF_MEMORY;
            }
        }
        ERUI_AlertDesc description{};
        description.size = sizeof(description);
        description.message = detail::utf16_view(message);
        description.callback = callback
            ? &detail::alert_callback_thunk
            : nullptr;
        description.user_data = callback_state.get();
        description.buttons = static_cast<ERUI_AlertButtons>(options.buttons);
        description.placement = static_cast<ERUI_AlertPlacement>(
            options.placement);
        const ERUI_Result result = api_.enqueue_alert(provider_, &description);
        if (result == ERUI_OK) callback_state.release();
        return result;
    }
    ERUI_Result alert(
        std::wstring_view message,
        AlertCallback callback = nullptr,
        void* user_data = nullptr) const noexcept {
        return alert(message, AlertOptions{}, callback, user_data);
    }
    bool valid() const noexcept { return provider_ != ERUI_INVALID_PROVIDER; }
    ERUI_ProviderHandle provider_handle() const noexcept { return provider_; }

private:
    friend class Connection;
    Registration(ERUI_Api api, ERUI_ProviderHandle provider)
        : api_(api), provider_(provider) {}
    ERUI_Api api_{};
    ERUI_ProviderHandle provider_{};
};

class RegistrationResult {
public:
    explicit operator bool() const noexcept { return success_; }
    bool success() const noexcept { return success_; }
    const Error& error() const noexcept { return error_; }
    Registration& value() noexcept { return registration_; }
    const Registration& value() const noexcept { return registration_; }

private:
    friend class Connection;
    bool success_{};
    Error error_{};
    Registration registration_{};
};

template <typename Builder>
RegistrationResult Connection::register_menu(
    const ProviderOptions& options,
    Builder&& builder) const noexcept {
    RegistrationResult output{};
    try {
        if (!detail::api_complete_for_version(
                api_, ERUI_API_VERSION_1_1)) {
            output.error_ = Error(ErrorCode::incompatible_api,
                ERUI_UNSUPPORTED_VERSION,
                L"A valid ERNativeUI API 1.1 connection is required.");
            return output;
        }
        if (!detail::valid_machine_identifier(options.provider_id)) {
            output.error_ = Error(ErrorCode::registration_failed,
                ERUI_INVALID_ARGUMENT,
                L"Provider ID must contain 1 to 255 ASCII letters, digits, "
                L"'.', '_', or '-'.");
            return output;
        }
        if (!detail::view_size_fits(options.display_name.size())) {
            output.error_ = Error(ErrorCode::registration_failed,
                ERUI_INVALID_ARGUMENT,
                L"Provider display name is too large.");
            return output;
        }
        ERUI_ProviderDesc desc{};
        desc.size = sizeof(desc);
        desc.api_version = api_.api_version;
        desc.root_priority = options.root_priority;
        desc.owner_module = options.owner_module;
        desc.provider_id = detail::string_view(options.provider_id);
        desc.display_name = detail::utf16_view(options.display_name);
        ERUI_ProviderHandle provider{};
        ERUI_PageHandle root{};
        const ERUI_Result started = api_.register_provider(
            &desc, &provider, &root);
        if (started != ERUI_OK) {
            output.error_ = Error(ErrorCode::registration_failed, started,
                std::wstring(L"Provider registration failed: ") +
                detail::result_name(started));
            return output;
        }

        detail::ProviderAbortGuard provider_guard(api_, provider);
        auto draft = std::make_shared<detail::Draft>(api_, provider, root);
        detail::DraftGuard draft_guard(draft);
        Menu menu(draft);
        try {
            builder(menu);
        } catch (...) {
            output.error_ = Error(ErrorCode::builder_exception,
                ERUI_INTERNAL_ERROR, L"The menu builder threw an exception.");
            return output;
        }
        if (draft->failed_) {
            output.error_ = draft->error_;
            return output;
        }
        const ERUI_Result committed = api_.commit_provider(provider);
        if (committed != ERUI_OK) {
            output.error_ = Error(ErrorCode::registration_failed, committed,
                std::wstring(L"Provider commit failed: ") +
                detail::result_name(committed));
            return output;
        }
        draft->mark_committed();
        provider_guard.release();
        output.registration_ = Registration(api_, provider);
        output.success_ = true;
        return output;
    } catch (...) {
        output.error_ = Error(ErrorCode::operation_failed,
            ERUI_OUT_OF_MEMORY);
        return output;
    }
}

} // namespace erui
