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
#include <cmath>
#include <cstring>
#include <cstdint>
#include <exception>
#include <functional>
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
using PageHandle = ERUI_PageHandle;
using InputSectionHandle = ERUI_InputSectionHandle;
using InputActionHandle = ERUI_InputActionHandle;
using StorageHandle = ERUI_StorageHandle;
using ButtonCallback = void (ERUI_CALL*)(void*) noexcept;
using ValueChangedCallback = void (ERUI_CALL*)(void*, std::uint8_t) noexcept;
using TextInputChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_TextInputChangeContext*) noexcept;
using ColorPickerChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_ColorPickerChangeContext*) noexcept;
using NumericSliderChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_NumericSliderChangeContext*) noexcept;
using CustomSliderChangedCallback = void (ERUI_CALL*)(
    void*, const ERUI_CustomSliderChangeContext*) noexcept;
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
    numeric_slider = ERUI_CAP_NUMERIC_SLIDER,
    custom_slider = ERUI_CAP_CUSTOM_SLIDER,
    custom_tabs = ERUI_CAP_CUSTOM_TABS,
    title_screen_text = ERUI_CAP_TITLE_SCREEN_TEXT,
    updates = ERUI_CAP_UPDATES,
    row_availability = ERUI_CAP_ROW_AVAILABILITY,
    row_visibility = ERUI_CAP_ROW_VISIBILITY,
    submenu_updates = ERUI_CAP_SUBMENU_UPDATES,
    tab_updates = ERUI_CAP_TAB_UPDATES,
};

enum class AvailabilityAppearance : ERUI_AvailabilityAppearance {
    follow_interactivity =
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY,
    normal = ERUI_AVAILABILITY_APPEARANCE_NORMAL,
    disabled = ERUI_AVAILABILITY_APPEARANCE_DISABLED,
};

enum class TabIconFormat : ERUI_TabIconFormat {
    rgba8 = ERUI_TAB_ICON_FORMAT_RGBA8,
};

enum class TitleScreenCorner : ERUI_TitleScreenCorner {
    top_left = ERUI_TITLE_SCREEN_CORNER_TOP_LEFT,
    top_right = ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT,
    bottom_left = ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT,
    bottom_right = ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT,
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
template <typename> class RowEditDraft;
}

class Submenu;
class SubmenuEdit;
class Tab;
class TabEdit;

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

struct TabIconView {
    // Exact 32x32, tightly packed, row-major RGBA8 pixels. Menu::add_tab and
    // TabEdit::icon each copy the bytes before returning.
    const std::uint8_t* pixels{};
    std::size_t byte_count{};
    TabIconFormat format{TabIconFormat::rgba8};
};

struct TabOptions {
    // Stable provider-local ID. The diagnostic/order key is
    // "provider_id/tab_id". This view and all other data are copied by
    // Menu::add_tab before it returns.
    std::string_view tab_id{};
    // 1..ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS UTF-16 code units.
    std::wstring_view title{};
    // Lower values sort first. This is a hint rather than a physical slot;
    // ERNativeUI.ini may provide a player override.
    std::int32_t preferred_order{100};
    // Empty selects ERNativeUI's default triangle icon.
    TabIconView icon{};
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

struct NumericSliderOptions {
    double minimum{0.0};
    double maximum{100.0};
    double step{1.0};
    // Off-step values are rounded down to a step measured from minimum before
    // the menu is published.
    double initial_value{0.0};
    // Empty selects the smallest exact decimal precision accepted by the API.
    std::optional<std::uint32_t> decimal_places{};
    std::wstring_view prefix{};
    std::wstring_view suffix{};
    bool enabled{true};
};

struct NumericSliderChange {
    ERUI_ProviderHandle provider{};
    RowHandle row{};
    double value{};
    std::int64_t scaled_value{};
    std::uint32_t decimal_places{};
    std::uint32_t index{};
    std::uint32_t value_count{};
};

struct CustomSliderOptions {
    std::uint32_t value_count{};
    std::uint32_t initial_index{};
    bool enabled{true};
    // Invoked synchronously once per index by add_custom_slider. The returned
    // strings are copied by the host before registration continues.
    std::function<std::wstring(std::uint32_t, std::uint32_t)> format_value{};
};

struct CustomSliderChange {
    ERUI_ProviderHandle provider{};
    RowHandle row{};
    std::uint32_t index{};
    std::uint32_t value_count{};
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

class TextStyle {
public:
    constexpr TextStyle() noexcept = default;

    constexpr TextStyle& color(Color value) & noexcept {
        color_ = value;
        flags_ |= ERUI_TEXT_STYLE_CUSTOM_COLOR;
        return *this;
    }

    constexpr TextStyle&& color(Color value) && noexcept {
        color_ = value;
        flags_ |= ERUI_TEXT_STYLE_CUSTOM_COLOR;
        return std::move(*this);
    }

    constexpr TextStyle& bold(bool enabled = true) & noexcept {
        set(ERUI_TEXT_STYLE_BOLD, enabled);
        return *this;
    }

    constexpr TextStyle&& bold(bool enabled = true) && noexcept {
        set(ERUI_TEXT_STYLE_BOLD, enabled);
        return std::move(*this);
    }

    constexpr TextStyle& italic(bool enabled = true) & noexcept {
        set(ERUI_TEXT_STYLE_ITALIC, enabled);
        return *this;
    }

    constexpr TextStyle&& italic(bool enabled = true) && noexcept {
        set(ERUI_TEXT_STYLE_ITALIC, enabled);
        return std::move(*this);
    }

    constexpr TextStyle& underline(bool enabled = true) & noexcept {
        set(ERUI_TEXT_STYLE_UNDERLINE, enabled);
        return *this;
    }

    constexpr TextStyle&& underline(bool enabled = true) && noexcept {
        set(ERUI_TEXT_STYLE_UNDERLINE, enabled);
        return std::move(*this);
    }

private:
    friend class StyledText;

    constexpr void set(
        ERUI_TextStyleFlags flag,
        bool enabled) noexcept {
        if (enabled) flags_ |= flag;
        else flags_ &= ~flag;
    }

    ERUI_TextStyleFlags flags_{};
    Color color_{};
};

/*
 * Owned structured text. append() records formatting as UTF-16 ranges; the
 * wrapper passes plain text plus those ranges across the C ABI, never raw
 * Scaleform markup supplied by a client.
 */
class StyledText {
public:
    StyledText() noexcept = default;

    explicit StyledText(std::wstring_view value) noexcept {
        append(value);
    }

    StyledText& append(std::wstring_view value) & noexcept {
        append_impl(value, nullptr);
        return *this;
    }

    StyledText&& append(std::wstring_view value) && noexcept {
        append_impl(value, nullptr);
        return std::move(*this);
    }

    StyledText& append(
        std::wstring_view value,
        TextStyle style) & noexcept {
        append_impl(value, &style);
        return *this;
    }

    StyledText&& append(
        std::wstring_view value,
        TextStyle style) && noexcept {
        append_impl(value, &style);
        return std::move(*this);
    }

    [[nodiscard]] bool empty() const noexcept { return text_.empty(); }
    [[nodiscard]] std::wstring_view text() const noexcept { return text_; }
    [[nodiscard]] std::size_t span_count() const noexcept {
        return spans_.size();
    }

private:
    template <typename>
    friend class detail::RowEditDraft;
    friend class SubmenuEdit;

    static bool same_style(
        const ERUI_TextStyleSpan& left,
        const ERUI_TextStyleSpan& right) noexcept {
        return left.flags == right.flags &&
            left.color.red == right.color.red &&
            left.color.green == right.color.green &&
            left.color.blue == right.color.blue;
    }

    void append_impl(
        std::wstring_view value,
        const TextStyle* style) noexcept {
        if (error_ != ERUI_OK || value.empty()) return;
        if (text_.size() > ERUI_STYLED_TEXT_MAX_CODE_UNITS ||
            value.size() >
                ERUI_STYLED_TEXT_MAX_CODE_UNITS - text_.size()) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }

        const std::uint32_t offset =
            static_cast<std::uint32_t>(text_.size());
        try {
            text_.append(value.data(), value.size());
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
            return;
        }

        if (!style || style->flags_ == 0) return;

        ERUI_TextStyleSpan span{};
        span.size = sizeof(span);
        span.flags = style->flags_;
        span.offset = offset;
        span.length = static_cast<std::uint32_t>(value.size());
        span.color.red = style->color_.red;
        span.color.green = style->color_.green;
        span.color.blue = style->color_.blue;

        if (!spans_.empty() &&
            spans_.back().offset + spans_.back().length == span.offset &&
            same_style(spans_.back(), span)) {
            spans_.back().length += span.length;
            return;
        }
        if (spans_.size() >= ERUI_STYLED_TEXT_MAX_SPANS) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        try {
            spans_.push_back(span);
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    std::wstring text_{};
    std::vector<ERUI_TextStyleSpan> spans_{};
    ERUI_Result error_{ERUI_OK};
};

struct TitleScreenTextOptions {
    // One player-facing line. CR, LF, U+0085, U+2028, and U+2029 are not
    // accepted. The host copies this view before the call returns, so
    // temporary localized storage is safe during registration.
    std::wstring_view text{};
    // Empty preserves Elden Ring's native title-screen text color.
    std::optional<Color> color{};
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

inline std::uint64_t decimal_scale(std::uint32_t decimal_places) noexcept {
    static constexpr std::uint64_t scales[] = {
        UINT64_C(1),
        UINT64_C(10),
        UINT64_C(100),
        UINT64_C(1000),
        UINT64_C(10000),
        UINT64_C(100000),
        UINT64_C(1000000),
        UINT64_C(10000000),
        UINT64_C(100000000),
        UINT64_C(1000000000),
    };
    return decimal_places <= ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES
        ? scales[decimal_places]
        : 0;
}

inline bool scale_decimal_value(
    double value,
    std::uint32_t decimal_places,
    std::int64_t& output) noexcept {
    const std::uint64_t integer_scale = decimal_scale(decimal_places);
    if (integer_scale == 0 || !std::isfinite(value)) return false;

    // Perform the conversion with binary64 on every supported compiler. In
    // particular, MSVC treats long double as double while MinGW may not; the
    // public wrapper must accept or reject the same input on both toolchains.
    const double scaled = value * static_cast<double>(integer_scale);
    if (!std::isfinite(scaled)) return false;
    const double rounded = std::round(scaled);

    // INT64_MAX rounds to +2^63 when represented as a double. Comparing with
    // a converted INT64_MAX would therefore permit an out-of-range cast.
    // Use the exact binary powers that bound the signed 64-bit interval.
    constexpr double signed_minimum = -9223372036854775808.0; // -2^63
    constexpr double signed_upper_bound = 9223372036854775808.0; // +2^63
    if (rounded < signed_minimum || rounded >= signed_upper_bound) {
        return false;
    }

    // Decimal inputs such as 0.29 can land one representable binary value
    // beside the expected integer after multiplication by 100. Accept at
    // most two local ULPs of that unavoidable conversion noise; do not use a
    // broad absolute epsilon that would silently accept a genuinely
    // off-precision value.
    if (scaled != rounded) {
        const double adjacent = std::nextafter(scaled, rounded);
        const double ulp = std::fabs(adjacent - scaled);
        if (ulp == 0.0 || std::fabs(scaled - rounded) > ulp * 2.0) {
            return false;
        }
    }
    output = static_cast<std::int64_t>(rounded);
    return true;
}

inline bool infer_decimal_value(
    double value,
    ERUI_DecimalValue& output) noexcept {
    for (std::uint32_t decimal_places = 0;
         decimal_places <= ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES;
         ++decimal_places) {
        std::int64_t scaled{};
        if (!scale_decimal_value(value, decimal_places, scaled)) continue;
        output.scaled_value = scaled;
        output.decimal_places = decimal_places;
        output.reserved = 0;
        return true;
    }
    return false;
}

inline double unscale_decimal_value(
    std::int64_t scaled_value,
    std::uint32_t decimal_places) noexcept {
    const std::uint64_t scale = decimal_scale(decimal_places);
    return scale == 0
        ? 0.0
        : static_cast<double>(scaled_value) / static_cast<double>(scale);
}

struct PreparedNumericSlider {
    std::int64_t minimum{};
    std::int64_t maximum{};
    std::int64_t step{};
    std::int64_t initial_value{};
    std::uint32_t decimal_places{};
    std::uint32_t value_count{};
};

inline bool prepare_numeric_slider(
    const NumericSliderOptions& options,
    PreparedNumericSlider& output) noexcept {
    std::uint32_t decimal_places{};
    if (options.decimal_places.has_value()) {
        decimal_places = *options.decimal_places;
        if (decimal_places > ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES) {
            return false;
        }
        if (!scale_decimal_value(
                options.minimum, decimal_places, output.minimum) ||
            !scale_decimal_value(
                options.maximum, decimal_places, output.maximum) ||
            !scale_decimal_value(options.step, decimal_places, output.step) ||
            !scale_decimal_value(
                options.initial_value, decimal_places, output.initial_value)) {
            return false;
        }
    } else {
        bool found = false;
        for (decimal_places = 0;
             decimal_places <= ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES;
             ++decimal_places) {
            if (scale_decimal_value(
                    options.minimum, decimal_places, output.minimum) &&
                scale_decimal_value(
                    options.maximum, decimal_places, output.maximum) &&
                scale_decimal_value(
                    options.step, decimal_places, output.step) &&
                scale_decimal_value(
                    options.initial_value,
                    decimal_places,
                    output.initial_value)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    if (output.minimum >= output.maximum || output.step <= 0 ||
        output.initial_value < output.minimum ||
        output.initial_value > output.maximum) {
        return false;
    }
    const std::uint64_t range =
        static_cast<std::uint64_t>(output.maximum) -
        static_cast<std::uint64_t>(output.minimum);
    const std::uint64_t step = static_cast<std::uint64_t>(output.step);
    if (range % step != 0) return false;
    const std::uint64_t intervals = range / step;
    if (intervals < ERUI_SLIDER_MIN_VALUE_COUNT - 1u ||
        intervals > ERUI_SLIDER_MAX_VALUE_COUNT - 1u) {
        return false;
    }
    output.decimal_places = decimal_places;
    output.value_count = static_cast<std::uint32_t>(intervals + 1u);
    return true;
}

inline bool make_numeric_slider_change(
    const ERUI_NumericSliderChangeContext* context,
    NumericSliderChange& output) noexcept {
    if (!context ||
        context->size < sizeof(ERUI_NumericSliderChangeContext) ||
        context->flags != 0 || context->value.reserved != 0 ||
        context->value.decimal_places >
            ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES ||
        context->provider == ERUI_INVALID_PROVIDER ||
        context->row == ERUI_INVALID_ROW ||
        context->value_count < ERUI_SLIDER_MIN_VALUE_COUNT ||
        context->value_count > ERUI_SLIDER_MAX_VALUE_COUNT ||
        context->index >= context->value_count ||
        context->reserved[0] != 0 || context->reserved[1] != 0) {
        return false;
    }
    output.provider = context->provider;
    output.row = context->row;
    output.value = unscale_decimal_value(
        context->value.scaled_value, context->value.decimal_places);
    output.scaled_value = context->value.scaled_value;
    output.decimal_places = context->value.decimal_places;
    output.index = context->index;
    output.value_count = context->value_count;
    return true;
}

inline bool make_custom_slider_change(
    const ERUI_CustomSliderChangeContext* context,
    CustomSliderChange& output) noexcept {
    if (!context ||
        context->size < sizeof(ERUI_CustomSliderChangeContext) ||
        context->flags != 0 ||
        context->provider == ERUI_INVALID_PROVIDER ||
        context->row == ERUI_INVALID_ROW ||
        context->value_count < ERUI_SLIDER_MIN_VALUE_COUNT ||
        context->value_count > ERUI_SLIDER_MAX_VALUE_COUNT ||
        context->index >= context->value_count ||
        context->reserved[0] != 0 || context->reserved[1] != 0) {
        return false;
    }
    output.provider = context->provider;
    output.row = context->row;
    output.index = context->index;
    output.value_count = context->value_count;
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

inline bool valid_tab_icon(TabIconView icon) noexcept {
    if (!icon.pixels && icon.byte_count == 0) return true;
    if (!icon.pixels ||
        icon.byte_count != ERUI_TAB_ICON_RGBA8_BYTE_COUNT) {
        return false;
    }
    return icon.format == TabIconFormat::rgba8;
}

inline bool valid_title_screen_text(std::wstring_view text) noexcept {
    if (text.empty() ||
        text.size() > ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS ||
        !view_size_fits(text.size())) {
        return false;
    }
    for (std::size_t index = 0; index < text.size(); ++index) {
        const std::uint16_t unit = static_cast<std::uint16_t>(text[index]);
        if (unit == 0 || unit == static_cast<std::uint16_t>('\r') ||
            unit == static_cast<std::uint16_t>('\n') || unit == 0x0085u ||
            unit == 0x2028u || unit == 0x2029u) {
            return false;
        }
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (++index >= text.size()) return false;
            const std::uint16_t low =
                static_cast<std::uint16_t>(text[index]);
            if (low < 0xDC00u || low > 0xDFFFu) return false;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return false;
        }
    }
    return true;
}

inline bool valid_title_screen_corner(TitleScreenCorner corner) noexcept {
    switch (corner) {
    case TitleScreenCorner::top_left:
    case TitleScreenCorner::top_right:
    case TitleScreenCorner::bottom_left:
    case TitleScreenCorner::bottom_right:
        return true;
    }
    return false;
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
    case ERUI_DUPLICATE_TAB_ID: return L"duplicate provider-local tab ID";
    case ERUI_TAB_LIMIT_REACHED: return L"process-wide custom-tab limit reached";
    case ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED:
        return L"process-wide title-screen text limit reached";
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
                    ERUI_CAP_INPUT_BINDINGS | ERUI_CAP_STORAGE |
                    ERUI_CAP_NUMERIC_SLIDER |
                    ERUI_CAP_CUSTOM_SLIDER |
                    ERUI_CAP_CUSTOM_TABS |
                    ERUI_CAP_TITLE_SCREEN_TEXT |
                    ERUI_CAP_UPDATES |
                    ERUI_CAP_ROW_AVAILABILITY |
                    ERUI_CAP_ROW_VISIBILITY |
                    ERUI_CAP_SUBMENU_UPDATES |
                    ERUI_CAP_TAB_UPDATES)) == 0;
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
            (api.capabilities & ERUI_CAP_STORAGE) != 0 &&
            (api.capabilities &
                (ERUI_CAP_NUMERIC_SLIDER | ERUI_CAP_CUSTOM_SLIDER |
                    ERUI_CAP_CUSTOM_TABS |
                    ERUI_CAP_TITLE_SCREEN_TEXT |
                    ERUI_CAP_UPDATES |
                    ERUI_CAP_ROW_AVAILABILITY |
                    ERUI_CAP_ROW_VISIBILITY |
                    ERUI_CAP_SUBMENU_UPDATES |
                    ERUI_CAP_TAB_UPDATES)) == 0;
    }
    if (version == ERUI_API_VERSION_1_2) {
        return api.size == ERUI_API_V1_2_SIZE && api.add_text_input &&
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
            api.add_numeric_slider && api.set_numeric_slider_value &&
            api.get_numeric_slider_value && api.add_custom_slider &&
            api.set_custom_slider_index && api.get_custom_slider_index &&
            (api.capabilities & ERUI_CAP_TEXT_INPUT) != 0 &&
            (api.capabilities & ERUI_CAP_COLOR_PICKER) != 0 &&
            (api.capabilities & ERUI_CAP_BUILTIN_PAGES) != 0 &&
            (api.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0 &&
            (api.capabilities & ERUI_CAP_STORAGE) != 0 &&
            (api.capabilities & ERUI_CAP_NUMERIC_SLIDER) != 0 &&
            (api.capabilities & ERUI_CAP_CUSTOM_SLIDER) != 0 &&
            (api.capabilities & ERUI_CAP_CUSTOM_TABS) == 0 &&
            (api.capabilities & ERUI_CAP_TITLE_SCREEN_TEXT) == 0 &&
            (api.capabilities & ERUI_CAP_UPDATES) == 0 &&
            (api.capabilities & ERUI_CAP_ROW_AVAILABILITY) == 0 &&
            (api.capabilities & ERUI_CAP_ROW_VISIBILITY) == 0 &&
            (api.capabilities & ERUI_CAP_SUBMENU_UPDATES) == 0 &&
            (api.capabilities & ERUI_CAP_TAB_UPDATES) == 0;
    }
    if (version == ERUI_API_VERSION_1_3) {
        return api.size == ERUI_API_V1_3_SIZE && api.add_text_input &&
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
            api.add_numeric_slider && api.set_numeric_slider_value &&
            api.get_numeric_slider_value && api.add_custom_slider &&
            api.set_custom_slider_index && api.get_custom_slider_index &&
            api.add_tab && api.add_title_screen_text &&
            (api.capabilities & ERUI_CAP_TEXT_INPUT) != 0 &&
            (api.capabilities & ERUI_CAP_COLOR_PICKER) != 0 &&
            (api.capabilities & ERUI_CAP_BUILTIN_PAGES) != 0 &&
            (api.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0 &&
            (api.capabilities & ERUI_CAP_STORAGE) != 0 &&
            (api.capabilities & ERUI_CAP_NUMERIC_SLIDER) != 0 &&
            (api.capabilities & ERUI_CAP_CUSTOM_SLIDER) != 0 &&
            (api.capabilities & ERUI_CAP_UPDATES) == 0 &&
            (api.capabilities & ERUI_CAP_ROW_AVAILABILITY) == 0 &&
            (api.capabilities & ERUI_CAP_ROW_VISIBILITY) == 0 &&
            (api.capabilities & ERUI_CAP_SUBMENU_UPDATES) == 0 &&
            (api.capabilities & ERUI_CAP_TAB_UPDATES) == 0;
    }
    if (version == ERUI_API_VERSION_1_4) {
        return api.size == ERUI_API_V1_4_SIZE && api.add_text_input &&
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
            api.add_numeric_slider && api.set_numeric_slider_value &&
            api.get_numeric_slider_value && api.add_custom_slider &&
            api.set_custom_slider_index && api.get_custom_slider_index &&
            api.add_tab && api.add_title_screen_text && api.commit_updates &&
            (api.capabilities & ERUI_CAP_TEXT_INPUT) != 0 &&
            (api.capabilities & ERUI_CAP_COLOR_PICKER) != 0 &&
            (api.capabilities & ERUI_CAP_BUILTIN_PAGES) != 0 &&
            (api.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0 &&
            (api.capabilities & ERUI_CAP_STORAGE) != 0 &&
            (api.capabilities & ERUI_CAP_NUMERIC_SLIDER) != 0 &&
            (api.capabilities & ERUI_CAP_CUSTOM_SLIDER) != 0 &&
            ((api.capabilities & ERUI_CAP_ROW_AVAILABILITY) == 0 ||
             (api.capabilities & ERUI_CAP_UPDATES) != 0) &&
            ((api.capabilities & ERUI_CAP_ROW_VISIBILITY) == 0 ||
             (api.capabilities & ERUI_CAP_UPDATES) != 0) &&
            ((api.capabilities & ERUI_CAP_SUBMENU_UPDATES) == 0 ||
             ((api.capabilities & ERUI_CAP_UPDATES) != 0 &&
              (api.capabilities & ERUI_CAP_SUBMENU) != 0)) &&
            ((api.capabilities & ERUI_CAP_TAB_UPDATES) == 0 ||
             ((api.capabilities & ERUI_CAP_UPDATES) != 0 &&
              (api.capabilities & ERUI_CAP_CUSTOM_TABS) != 0));
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
                ERUI_API_VERSION_1_4,
                ERUI_API_V1_4_SIZE,
                result.api);
            if (status == ERUI_OK) {
                if (!api_complete_for_version(
                        result.api, ERUI_API_VERSION_1_4)) {
                    result.error = Error(ErrorCode::incompatible_api,
                        ERUI_UNSUPPORTED_VERSION,
                        L"ERNativeUI returned an incomplete or incompatible API 1.4 table.");
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
                        ? L"ERNativeUI API 1.4 is required; update ERNativeUI.dll."
                        : std::wstring(L"ERNativeUI API 1.4 negotiation failed: ") +
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

enum class RowContextState : std::uint8_t {
    pending,
    committed,
    aborted,
};

class RowContext {
public:
    RowContext(ERUI_Api api, ERUI_ProviderHandle provider) noexcept
        : api(api), provider(provider) {}

    [[nodiscard]] bool active() const noexcept {
        return state.load(std::memory_order_acquire) !=
            RowContextState::aborted;
    }

    [[nodiscard]] bool committed() const noexcept {
        return state.load(std::memory_order_acquire) ==
            RowContextState::committed;
    }

    void mark_committed() noexcept {
        RowContextState expected = RowContextState::pending;
        (void)state.compare_exchange_strong(
            expected,
            RowContextState::committed,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    void abort_if_pending() noexcept {
        RowContextState expected = RowContextState::pending;
        (void)state.compare_exchange_strong(
            expected,
            RowContextState::aborted,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    ERUI_Api api{};
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};

private:
    std::atomic<RowContextState> state{RowContextState::pending};
};

class Draft {
public:
    Draft(ERUI_Api api, ERUI_ProviderHandle provider, ERUI_PageHandle root)
        : api_(api), provider_(provider), root_(root),
          row_context_(std::make_shared<RowContext>(api, provider)) {}
    ~Draft() = default;
    Draft(const Draft&) = delete;
    Draft& operator=(const Draft&) = delete;

    [[nodiscard]] bool open() const noexcept {
        return open_.load(std::memory_order_acquire);
    }

    void close() noexcept {
        if (row_context_) row_context_->abort_if_pending();
        open_.store(false, std::memory_order_release);
    }

    void mark_committed() noexcept {
        // Provider callbacks are process-lifetime by contract. A committed
        // provider's module is pinned by the host, so transfer these small
        // client-side thunk states to the same lifetime. Failed drafts still
        // destroy their states normally.
        for (auto& state : callback_states_) state.release();
        callback_states_.clear();
        if (row_context_) row_context_->mark_committed();
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
    std::shared_ptr<RowContext> row_context_{};

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
        return api_.api_version == ERUI_API_VERSION_1_4 &&
            api_.size == ERUI_API_V1_4_SIZE;
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

class ButtonRowEdit;
class ToggleRowEdit;
class SliderRowEdit;
class NumericSliderRowEdit;
class CustomSliderRowEdit;
class InlineChoiceRowEdit;
class PopupChoiceRowEdit;
class TextInputRowEdit;
class ColorPickerRowEdit;

class RowRef {
public:
    RowRef() noexcept = default;
    RowRef(const RowRef&) noexcept = default;
    RowRef(RowRef&&) noexcept = default;
    RowRef& operator=(const RowRef&) noexcept = default;
    RowRef& operator=(RowRef&&) noexcept = default;
    ~RowRef() = default;

    [[nodiscard]] RowHandle handle() const noexcept { return row_; }
    [[nodiscard]] bool valid() const noexcept {
        return row_ != ERUI_INVALID_ROW && context_ && context_->committed() &&
            context_->provider != ERUI_INVALID_PROVIDER;
    }

    operator RowHandle() const noexcept { return row_; }

    friend bool operator==(const RowRef& left, const RowRef& right) noexcept {
        return left.context_.get() == right.context_.get() &&
            left.row_ == right.row_;
    }
    friend bool operator!=(const RowRef& left, const RowRef& right) noexcept {
        return !(left == right);
    }
    friend bool operator==(const RowRef& left, RowHandle right) noexcept {
        return left.row_ == right;
    }
    friend bool operator==(RowHandle left, const RowRef& right) noexcept {
        return left == right.row_;
    }
    friend bool operator!=(const RowRef& left, RowHandle right) noexcept {
        return !(left == right);
    }
    friend bool operator!=(RowHandle left, const RowRef& right) noexcept {
        return !(left == right);
    }

protected:
    RowRef(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : context_(std::move(context)), row_(row) {}

    ERUI_Result set_byte_value(std::uint8_t value) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if (!context->api.set_row_value) return ERUI_NOT_SUPPORTED;
        return context->api.set_row_value(
            context->provider, row_, value);
    }

    ERUI_Result get_byte_value(std::uint8_t& output) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if (!context->api.get_row_value) return ERUI_NOT_SUPPORTED;
        std::uint8_t value{};
        const ERUI_Result result = context->api.get_row_value(
            context->provider, row_, &value);
        if (result == ERUI_OK) output = value;
        return result;
    }

    ERUI_Result set_numeric_value(double value) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_NUMERIC_SLIDER) == 0 ||
            !context->api.set_numeric_slider_value) {
            return ERUI_NOT_SUPPORTED;
        }
        ERUI_DecimalValue native_value{};
        if (!detail::infer_decimal_value(value, native_value)) {
            return ERUI_INVALID_ARGUMENT;
        }
        return context->api.set_numeric_slider_value(
            context->provider, row_, &native_value);
    }

    ERUI_Result get_numeric_value(double& output) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_NUMERIC_SLIDER) == 0 ||
            !context->api.get_numeric_slider_value) {
            return ERUI_NOT_SUPPORTED;
        }
        ERUI_DecimalValue native_value{};
        const ERUI_Result result = context->api.get_numeric_slider_value(
            context->provider, row_, &native_value);
        if (result != ERUI_OK) return result;
        if (native_value.reserved != 0 ||
            native_value.decimal_places >
                ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES) {
            return ERUI_INTERNAL_ERROR;
        }
        output = detail::unscale_decimal_value(
            native_value.scaled_value, native_value.decimal_places);
        return ERUI_OK;
    }

    ERUI_Result set_custom_index(std::uint32_t index) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_CUSTOM_SLIDER) == 0 ||
            !context->api.set_custom_slider_index) {
            return ERUI_NOT_SUPPORTED;
        }
        return context->api.set_custom_slider_index(
            context->provider, row_, index);
    }

    ERUI_Result get_custom_index(std::uint32_t& output) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_CUSTOM_SLIDER) == 0 ||
            !context->api.get_custom_slider_index) {
            return ERUI_NOT_SUPPORTED;
        }
        std::uint32_t index{};
        const ERUI_Result result = context->api.get_custom_slider_index(
            context->provider, row_, &index);
        if (result == ERUI_OK) output = index;
        return result;
    }

    ERUI_Result set_text_value(std::wstring_view value) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_TEXT_INPUT) == 0 ||
            !context->api.set_text_input_value) {
            return ERUI_NOT_SUPPORTED;
        }
        if (!detail::view_size_fits(value.size()) ||
            value.size() > ERUI_TEXT_INPUT_MAX_LENGTH) {
            return ERUI_INVALID_ARGUMENT;
        }
        const ERUI_Utf16View native_value = detail::utf16_view(value);
        return context->api.set_text_input_value(
            context->provider, row_, &native_value);
    }

    ERUI_Result get_text_value(std::wstring& output) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_TEXT_INPUT) == 0 ||
            !context->api.get_text_input_value) {
            return ERUI_NOT_SUPPORTED;
        }
        try {
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                std::uint32_t required{};
                ERUI_Result result = context->api.get_text_input_value(
                    context->provider, row_, nullptr, 0, &required);
                if (result != ERUI_OK) return result;
                if (required == 0) {
                    std::wstring empty{};
                    output.swap(empty);
                    return ERUI_OK;
                }

                std::vector<std::uint16_t> storage(required);
                std::uint32_t actual{};
                result = context->api.get_text_input_value(
                    context->provider,
                    row_,
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

    ERUI_Result set_color_value(Color value) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_COLOR_PICKER) == 0 ||
            !context->api.set_color_picker_value) {
            return ERUI_NOT_SUPPORTED;
        }
        const ERUI_Color native_value = detail::native_color(value);
        return context->api.set_color_picker_value(
            context->provider, row_, &native_value);
    }

    ERUI_Result get_color_value(Color& output) const noexcept {
        const auto* context = active_context();
        if (!context) return ERUI_INVALID_HANDLE;
        if ((context->api.capabilities & ERUI_CAP_COLOR_PICKER) == 0 ||
            !context->api.get_color_picker_value) {
            return ERUI_NOT_SUPPORTED;
        }
        ERUI_Color native_value{};
        const ERUI_Result result = context->api.get_color_picker_value(
            context->provider, row_, &native_value);
        if (result != ERUI_OK) return result;
        if (native_value.reserved != 0) return ERUI_INTERNAL_ERROR;
        output = detail::color_from_native(native_value);
        return ERUI_OK;
    }

    [[nodiscard]] std::shared_ptr<detail::RowContext> edit_context()
        const noexcept {
        return context_;
    }

    [[nodiscard]] RowHandle edit_handle() const noexcept { return row_; }

private:
    [[nodiscard]] const detail::RowContext* active_context() const noexcept {
        return row_ != ERUI_INVALID_ROW && context_ && context_->active() &&
                context_->provider != ERUI_INVALID_PROVIDER
            ? context_.get()
            : nullptr;
    }

    std::shared_ptr<detail::RowContext> context_{};
    RowHandle row_{ERUI_INVALID_ROW};
};

namespace detail {

/*
 * Shared implementation for the nine public, type-specific edit drafts.
 * Drafts own their strings, do nothing when destroyed, and may be committed
 * exactly once. The host copies the complete one-item batch synchronously.
 */
template <typename Derived>
class RowEditDraft {
public:
    RowEditDraft(const RowEditDraft&) = delete;
    RowEditDraft& operator=(const RowEditDraft&) = delete;
    RowEditDraft(RowEditDraft&&) noexcept = default;
    RowEditDraft& operator=(RowEditDraft&&) noexcept = default;
    ~RowEditDraft() = default;

    Derived& label(std::wstring_view value) & noexcept {
        set_text(value, label_, ERUI_ROW_UPDATE_FIELD_LABEL);
        return derived();
    }

    Derived&& label(std::wstring_view value) && noexcept {
        set_text(value, label_, ERUI_ROW_UPDATE_FIELD_LABEL);
        return std::move(derived());
    }

    Derived& label(const StyledText& value) & noexcept {
        set_styled_text(value, label_, ERUI_ROW_UPDATE_FIELD_LABEL);
        return derived();
    }

    Derived&& label(const StyledText& value) && noexcept {
        set_styled_text(value, label_, ERUI_ROW_UPDATE_FIELD_LABEL);
        return std::move(derived());
    }

    Derived& label(StyledText&& value) & noexcept {
        set_styled_text(
            std::move(value), label_, ERUI_ROW_UPDATE_FIELD_LABEL);
        return derived();
    }

    Derived&& label(StyledText&& value) && noexcept {
        set_styled_text(
            std::move(value), label_, ERUI_ROW_UPDATE_FIELD_LABEL);
        return std::move(derived());
    }

    Derived& help(std::wstring_view value) & noexcept {
        set_text(value, help_, ERUI_ROW_UPDATE_FIELD_HELP);
        return derived();
    }

    Derived&& help(std::wstring_view value) && noexcept {
        set_text(value, help_, ERUI_ROW_UPDATE_FIELD_HELP);
        return std::move(derived());
    }

    Derived& help(const StyledText& value) & noexcept {
        set_styled_text(value, help_, ERUI_ROW_UPDATE_FIELD_HELP);
        return derived();
    }

    Derived&& help(const StyledText& value) && noexcept {
        set_styled_text(value, help_, ERUI_ROW_UPDATE_FIELD_HELP);
        return std::move(derived());
    }

    Derived& help(StyledText&& value) & noexcept {
        set_styled_text(
            std::move(value), help_, ERUI_ROW_UPDATE_FIELD_HELP);
        return derived();
    }

    Derived&& help(StyledText&& value) && noexcept {
        set_styled_text(
            std::move(value), help_, ERUI_ROW_UPDATE_FIELD_HELP);
        return std::move(derived());
    }

    Derived& interactive(bool value) & noexcept {
        set_interactive(value);
        return derived();
    }

    Derived&& interactive(bool value) && noexcept {
        set_interactive(value);
        return std::move(derived());
    }

    Derived& availability_appearance(
        AvailabilityAppearance value) & noexcept {
        set_availability_appearance(value);
        return derived();
    }

    Derived&& availability_appearance(
        AvailabilityAppearance value) && noexcept {
        set_availability_appearance(value);
        return std::move(derived());
    }

    Derived& visible(bool value) & noexcept {
        set_visible(value);
        return derived();
    }

    Derived&& visible(bool value) && noexcept {
        set_visible(value);
        return std::move(derived());
    }

    [[nodiscard]] ERUI_Result commit() noexcept {
        if (committed_) return ERUI_ALREADY_COMMITTED;
        committed_ = true;
        if (error_ != ERUI_OK) return error_;
        if (fields_ == 0) return ERUI_INVALID_ARGUMENT;
        if (row_ == ERUI_INVALID_ROW || !context_ ||
            !context_->committed() ||
            context_->provider == ERUI_INVALID_PROVIDER) {
            return ERUI_INVALID_HANDLE;
        }
        if ((context_->api.capabilities & ERUI_CAP_UPDATES) == 0 ||
            !context_->api.commit_updates) {
            return ERUI_NOT_SUPPORTED;
        }
        constexpr ERUI_RowUpdateFields availability_fields =
            ERUI_ROW_UPDATE_FIELD_INTERACTIVE |
            ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
        if ((fields_ & availability_fields) != 0 &&
            (context_->api.capabilities &
                ERUI_CAP_ROW_AVAILABILITY) == 0) {
            return ERUI_NOT_SUPPORTED;
        }
        if ((fields_ & ERUI_ROW_UPDATE_FIELD_VISIBLE) != 0 &&
            (context_->api.capabilities & ERUI_CAP_ROW_VISIBILITY) == 0) {
            return ERUI_NOT_SUPPORTED;
        }

        ERUI_RowUpdateDesc row_update{};
        row_update.size = sizeof(row_update);
        row_update.row = row_;
        row_update.fields = fields_;
        if ((fields_ & ERUI_ROW_UPDATE_FIELD_LABEL) != 0) {
            row_update.label = styled_text_view(label_);
        }
        if ((fields_ & ERUI_ROW_UPDATE_FIELD_HELP) != 0) {
            row_update.help = styled_text_view(help_);
        }
        if ((fields_ & ERUI_ROW_UPDATE_FIELD_CAPTION) != 0) {
            row_update.caption = styled_text_view(caption_);
        }
        if ((fields_ & ERUI_ROW_UPDATE_FIELD_INTERACTIVE) != 0) {
            row_update.interactive = interactive_ ? 1u : 0u;
        }
        if ((fields_ &
                ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE) != 0) {
            row_update.availability_appearance =
                static_cast<ERUI_AvailabilityAppearance>(appearance_);
        }
        if ((fields_ & ERUI_ROW_UPDATE_FIELD_VISIBLE) != 0) {
            row_update.visible = visible_ ? 1u : 0u;
        }

        ERUI_UpdateDesc update{};
        update.size = sizeof(update);
        update.kind = ERUI_UPDATE_KIND_ROW;
        update.payload = &row_update;

        ERUI_UpdateBatchDesc batch{};
        batch.size = sizeof(batch);
        batch.updates = &update;
        batch.update_count = 1u;
        batch.update_stride = sizeof(update);
        return context_->api.commit_updates(context_->provider, &batch);
    }

protected:
    RowEditDraft(
        std::shared_ptr<RowContext> context,
        RowHandle row) noexcept
        : context_(std::move(context)), row_(row) {}

    void set_caption(std::wstring_view value) noexcept {
        set_text(value, caption_, ERUI_ROW_UPDATE_FIELD_CAPTION);
    }

    void set_caption(const StyledText& value) noexcept {
        set_styled_text(value, caption_, ERUI_ROW_UPDATE_FIELD_CAPTION);
    }

    void set_caption(StyledText&& value) noexcept {
        set_styled_text(
            std::move(value), caption_, ERUI_ROW_UPDATE_FIELD_CAPTION);
    }

    void set_interactive(bool value) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        interactive_ = value;
        fields_ |= ERUI_ROW_UPDATE_FIELD_INTERACTIVE;
    }

    void set_availability_appearance(
        AvailabilityAppearance value) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        switch (value) {
        case AvailabilityAppearance::follow_interactivity:
        case AvailabilityAppearance::normal:
        case AvailabilityAppearance::disabled:
            appearance_ = value;
            fields_ |=
                ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
            return;
        default:
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
    }

    void set_visible(bool value) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        visible_ = value;
        fields_ |= ERUI_ROW_UPDATE_FIELD_VISIBLE;
    }

private:
    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }

    void set_text(
        std::wstring_view value,
        StyledText& destination,
        ERUI_RowUpdateFields field) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (!view_size_fits(value.size()) ||
            ((field == ERUI_ROW_UPDATE_FIELD_LABEL ||
              field == ERUI_ROW_UPDATE_FIELD_CAPTION) && value.empty())) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }

        StyledText replacement(value);
        if (replacement.error_ != ERUI_OK) {
            error_ = replacement.error_;
            return;
        }
        try {
            destination = std::move(replacement);
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    void set_styled_text(
        const StyledText& value,
        StyledText& destination,
        ERUI_RowUpdateFields field) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (value.error_ != ERUI_OK) {
            error_ = value.error_;
            return;
        }
        if ((field == ERUI_ROW_UPDATE_FIELD_LABEL ||
             field == ERUI_ROW_UPDATE_FIELD_CAPTION) &&
            value.text_.empty()) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        try {
            destination = value;
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    void set_styled_text(
        StyledText&& value,
        StyledText& destination,
        ERUI_RowUpdateFields field) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (value.error_ != ERUI_OK) {
            error_ = value.error_;
            return;
        }
        if ((field == ERUI_ROW_UPDATE_FIELD_LABEL ||
             field == ERUI_ROW_UPDATE_FIELD_CAPTION) &&
            value.text_.empty()) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        try {
            destination = std::move(value);
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    static ERUI_StyledTextView styled_text_view(
        const StyledText& value) noexcept {
        static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
            "ERNativeUI requires Windows UTF-16 wchar_t");
        ERUI_StyledTextView result{};
        result.size = sizeof(result);
        result.text.data = value.text_.empty()
            ? nullptr
            : reinterpret_cast<const std::uint16_t*>(value.text_.data());
        result.text.length = static_cast<std::uint32_t>(value.text_.size());
        result.spans = value.spans_.empty() ? nullptr : value.spans_.data();
        result.span_count = static_cast<std::uint32_t>(value.spans_.size());
        result.span_stride = sizeof(ERUI_TextStyleSpan);
        return result;
    }

    std::shared_ptr<RowContext> context_{};
    RowHandle row_{ERUI_INVALID_ROW};
    ERUI_RowUpdateFields fields_{};
    ERUI_Result error_{ERUI_OK};
    bool committed_{};
    bool interactive_{};
    bool visible_{};
    AvailabilityAppearance appearance_{
        AvailabilityAppearance::follow_interactivity};
    StyledText label_{};
    StyledText help_{};
    StyledText caption_{};
};

} // namespace detail

/*
 * One atomic edit of a submenu opener and its child-page text. The draft owns
 * every string and is inert until commit(), matching the ordinary row-edit
 * lifecycle. Submenu availability, visibility, and removal deliberately remain
 * outside this edit surface.
 */
class SubmenuEdit final {
public:
    SubmenuEdit(const SubmenuEdit&) = delete;
    SubmenuEdit& operator=(const SubmenuEdit&) = delete;
    SubmenuEdit(SubmenuEdit&&) noexcept = default;
    SubmenuEdit& operator=(SubmenuEdit&&) noexcept = default;
    ~SubmenuEdit() = default;

    SubmenuEdit& label(std::wstring_view value) & noexcept {
        set_text(value, label_, ERUI_SUBMENU_UPDATE_FIELD_LABEL);
        return *this;
    }
    SubmenuEdit&& label(std::wstring_view value) && noexcept {
        set_text(value, label_, ERUI_SUBMENU_UPDATE_FIELD_LABEL);
        return std::move(*this);
    }
    SubmenuEdit& label(const StyledText& value) & noexcept {
        set_styled_text(value, label_, ERUI_SUBMENU_UPDATE_FIELD_LABEL);
        return *this;
    }
    SubmenuEdit&& label(const StyledText& value) && noexcept {
        set_styled_text(value, label_, ERUI_SUBMENU_UPDATE_FIELD_LABEL);
        return std::move(*this);
    }
    SubmenuEdit& label(StyledText&& value) & noexcept {
        set_styled_text(
            std::move(value), label_, ERUI_SUBMENU_UPDATE_FIELD_LABEL);
        return *this;
    }
    SubmenuEdit&& label(StyledText&& value) && noexcept {
        set_styled_text(
            std::move(value), label_, ERUI_SUBMENU_UPDATE_FIELD_LABEL);
        return std::move(*this);
    }

    SubmenuEdit& help(std::wstring_view value) & noexcept {
        set_text(value, help_, ERUI_SUBMENU_UPDATE_FIELD_HELP);
        return *this;
    }
    SubmenuEdit&& help(std::wstring_view value) && noexcept {
        set_text(value, help_, ERUI_SUBMENU_UPDATE_FIELD_HELP);
        return std::move(*this);
    }
    SubmenuEdit& help(const StyledText& value) & noexcept {
        set_styled_text(value, help_, ERUI_SUBMENU_UPDATE_FIELD_HELP);
        return *this;
    }
    SubmenuEdit&& help(const StyledText& value) && noexcept {
        set_styled_text(value, help_, ERUI_SUBMENU_UPDATE_FIELD_HELP);
        return std::move(*this);
    }
    SubmenuEdit& help(StyledText&& value) & noexcept {
        set_styled_text(
            std::move(value), help_, ERUI_SUBMENU_UPDATE_FIELD_HELP);
        return *this;
    }
    SubmenuEdit&& help(StyledText&& value) && noexcept {
        set_styled_text(
            std::move(value), help_, ERUI_SUBMENU_UPDATE_FIELD_HELP);
        return std::move(*this);
    }

    SubmenuEdit& caption(std::wstring_view value) & noexcept {
        set_text(value, caption_, ERUI_SUBMENU_UPDATE_FIELD_CAPTION);
        return *this;
    }
    SubmenuEdit&& caption(std::wstring_view value) && noexcept {
        set_text(value, caption_, ERUI_SUBMENU_UPDATE_FIELD_CAPTION);
        return std::move(*this);
    }
    SubmenuEdit& caption(const StyledText& value) & noexcept {
        set_styled_text(value, caption_, ERUI_SUBMENU_UPDATE_FIELD_CAPTION);
        return *this;
    }
    SubmenuEdit&& caption(const StyledText& value) && noexcept {
        set_styled_text(value, caption_, ERUI_SUBMENU_UPDATE_FIELD_CAPTION);
        return std::move(*this);
    }
    SubmenuEdit& caption(StyledText&& value) & noexcept {
        set_styled_text(
            std::move(value), caption_, ERUI_SUBMENU_UPDATE_FIELD_CAPTION);
        return *this;
    }
    SubmenuEdit&& caption(StyledText&& value) && noexcept {
        set_styled_text(
            std::move(value), caption_, ERUI_SUBMENU_UPDATE_FIELD_CAPTION);
        return std::move(*this);
    }

    SubmenuEdit& page_title(std::wstring_view value) & noexcept {
        set_page_text(value, page_title_,
            ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE, false);
        return *this;
    }
    SubmenuEdit&& page_title(std::wstring_view value) && noexcept {
        set_page_text(value, page_title_,
            ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE, false);
        return std::move(*this);
    }

    SubmenuEdit& page_help(std::wstring_view value) & noexcept {
        set_page_text(value, page_help_,
            ERUI_SUBMENU_UPDATE_FIELD_PAGE_HELP, true);
        return *this;
    }
    SubmenuEdit&& page_help(std::wstring_view value) && noexcept {
        set_page_text(value, page_help_,
            ERUI_SUBMENU_UPDATE_FIELD_PAGE_HELP, true);
        return std::move(*this);
    }

    [[nodiscard]] ERUI_Result commit() noexcept {
        if (committed_) return ERUI_ALREADY_COMMITTED;
        committed_ = true;
        if (error_ != ERUI_OK) return error_;
        if (fields_ == 0) return ERUI_INVALID_ARGUMENT;
        if (row_ == ERUI_INVALID_ROW || page_ == ERUI_INVALID_PAGE ||
            !context_ || !context_->committed() ||
            context_->provider == ERUI_INVALID_PROVIDER) {
            return ERUI_INVALID_HANDLE;
        }
        constexpr ERUI_Capabilities required =
            ERUI_CAP_UPDATES | ERUI_CAP_SUBMENU_UPDATES;
        if ((context_->api.capabilities & required) != required ||
            !context_->api.commit_updates) {
            return ERUI_NOT_SUPPORTED;
        }

        ERUI_SubmenuUpdateDesc submenu_update{};
        submenu_update.size = sizeof(submenu_update);
        submenu_update.row = row_;
        submenu_update.page = page_;
        submenu_update.fields = fields_;
        if ((fields_ & ERUI_SUBMENU_UPDATE_FIELD_LABEL) != 0) {
            submenu_update.label = styled_text_view(label_);
        }
        if ((fields_ & ERUI_SUBMENU_UPDATE_FIELD_HELP) != 0) {
            submenu_update.help = styled_text_view(help_);
        }
        if ((fields_ & ERUI_SUBMENU_UPDATE_FIELD_CAPTION) != 0) {
            submenu_update.caption = styled_text_view(caption_);
        }
        if ((fields_ & ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE) != 0) {
            submenu_update.page_title = detail::utf16_view(page_title_);
        }
        if ((fields_ & ERUI_SUBMENU_UPDATE_FIELD_PAGE_HELP) != 0) {
            submenu_update.page_help = detail::utf16_view(page_help_);
        }

        ERUI_UpdateDesc update{};
        update.size = sizeof(update);
        update.kind = ERUI_UPDATE_KIND_SUBMENU;
        update.payload = &submenu_update;

        ERUI_UpdateBatchDesc batch{};
        batch.size = sizeof(batch);
        batch.updates = &update;
        batch.update_count = 1u;
        batch.update_stride = sizeof(update);
        return context_->api.commit_updates(context_->provider, &batch);
    }

private:
    friend class Submenu;

    SubmenuEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row,
        PageHandle page) noexcept
        : context_(std::move(context)), row_(row), page_(page) {}

    void set_text(
        std::wstring_view value,
        StyledText& destination,
        ERUI_SubmenuUpdateFields field) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (!detail::view_size_fits(value.size()) ||
            ((field == ERUI_SUBMENU_UPDATE_FIELD_LABEL ||
              field == ERUI_SUBMENU_UPDATE_FIELD_CAPTION) && value.empty())) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        StyledText replacement(value);
        if (replacement.error_ != ERUI_OK) {
            error_ = replacement.error_;
            return;
        }
        try {
            destination = std::move(replacement);
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    void set_styled_text(
        const StyledText& value,
        StyledText& destination,
        ERUI_SubmenuUpdateFields field) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (value.error_ != ERUI_OK ||
            ((field == ERUI_SUBMENU_UPDATE_FIELD_LABEL ||
              field == ERUI_SUBMENU_UPDATE_FIELD_CAPTION) &&
             value.text_.empty())) {
            error_ = value.error_ != ERUI_OK
                ? value.error_
                : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
            return;
        }
        try {
            destination = value;
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    void set_styled_text(
        StyledText&& value,
        StyledText& destination,
        ERUI_SubmenuUpdateFields field) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (value.error_ != ERUI_OK ||
            ((field == ERUI_SUBMENU_UPDATE_FIELD_LABEL ||
              field == ERUI_SUBMENU_UPDATE_FIELD_CAPTION) &&
             value.text_.empty())) {
            error_ = value.error_ != ERUI_OK
                ? value.error_
                : static_cast<ERUI_Result>(ERUI_INVALID_ARGUMENT);
            return;
        }
        try {
            destination = std::move(value);
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    void set_page_text(
        std::wstring_view value,
        std::wstring& destination,
        ERUI_SubmenuUpdateFields field,
        bool empty_allowed) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (!detail::view_size_fits(value.size()) ||
            (!empty_allowed && value.empty())) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        try {
            if (value.empty()) destination.clear();
            else destination.assign(value.data(), value.size());
            fields_ |= field;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    static ERUI_StyledTextView styled_text_view(
        const StyledText& value) noexcept {
        static_assert(sizeof(wchar_t) == sizeof(std::uint16_t),
            "ERNativeUI requires Windows UTF-16 wchar_t");
        ERUI_StyledTextView result{};
        result.size = sizeof(result);
        result.text.data = value.text_.empty()
            ? nullptr
            : reinterpret_cast<const std::uint16_t*>(value.text_.data());
        result.text.length = static_cast<std::uint32_t>(value.text_.size());
        result.spans = value.spans_.empty() ? nullptr : value.spans_.data();
        result.span_count = static_cast<std::uint32_t>(value.spans_.size());
        result.span_stride = sizeof(ERUI_TextStyleSpan);
        return result;
    }

    std::shared_ptr<detail::RowContext> context_{};
    RowHandle row_{ERUI_INVALID_ROW};
    PageHandle page_{ERUI_INVALID_PAGE};
    ERUI_SubmenuUpdateFields fields_{};
    ERUI_Result error_{ERUI_OK};
    bool committed_{};
    StyledText label_{};
    StyledText help_{};
    StyledText caption_{};
    std::wstring page_title_{};
    std::wstring page_help_{};
};

/*
 * One atomic edit of a registered custom tab. The page and all of its rows
 * retain their identities; only the selected presentation/placement fields
 * change. An empty icon view restores ERNativeUI's default triangle icon.
 * Every supported field, including an icon, can publish atomically in the
 * active System movie. If that movie closes during an icon handoff, the desired
 * generation remains queued and is applied automatically after the first
 * reopen. Icon edits are for infrequent configuration changes rather than
 * animation because their immutable native image roots are process-retained.
 */
class TabEdit final {
public:
    TabEdit(const TabEdit&) = delete;
    TabEdit& operator=(const TabEdit&) = delete;
    TabEdit(TabEdit&&) noexcept = default;
    TabEdit& operator=(TabEdit&&) noexcept = default;
    ~TabEdit() = default;

    TabEdit& title(std::wstring_view value) & noexcept {
        set_title(value);
        return *this;
    }
    TabEdit&& title(std::wstring_view value) && noexcept {
        set_title(value);
        return std::move(*this);
    }

    TabEdit& icon(TabIconView value) & noexcept {
        set_icon(value);
        return *this;
    }
    TabEdit&& icon(TabIconView value) && noexcept {
        set_icon(value);
        return std::move(*this);
    }

    TabEdit& default_icon() & noexcept {
        set_icon({});
        return *this;
    }
    TabEdit&& default_icon() && noexcept {
        set_icon({});
        return std::move(*this);
    }

    TabEdit& visible(bool value) & noexcept {
        if (!committed_ && error_ == ERUI_OK) {
            visible_ = value;
            fields_ |= ERUI_TAB_UPDATE_FIELD_VISIBLE;
        }
        return *this;
    }
    TabEdit&& visible(bool value) && noexcept {
        static_cast<TabEdit&>(*this).visible(value);
        return std::move(*this);
    }

    TabEdit& preferred_order(std::int32_t value) & noexcept {
        if (!committed_ && error_ == ERUI_OK) {
            preferred_order_ = value;
            fields_ |= ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
        }
        return *this;
    }
    TabEdit&& preferred_order(std::int32_t value) && noexcept {
        static_cast<TabEdit&>(*this).preferred_order(value);
        return std::move(*this);
    }

    [[nodiscard]] ERUI_Result commit() noexcept {
        if (committed_) return ERUI_ALREADY_COMMITTED;
        committed_ = true;
        if (error_ != ERUI_OK) return error_;
        if (fields_ == 0) return ERUI_INVALID_ARGUMENT;
        if (page_ == ERUI_INVALID_PAGE || !context_ ||
            !context_->committed() ||
            context_->provider == ERUI_INVALID_PROVIDER) {
            return ERUI_INVALID_HANDLE;
        }
        constexpr ERUI_Capabilities required =
            ERUI_CAP_UPDATES | ERUI_CAP_TAB_UPDATES;
        if ((context_->api.capabilities & required) != required ||
            !context_->api.commit_updates) {
            return ERUI_NOT_SUPPORTED;
        }

        ERUI_TabIconDesc icon{};
        ERUI_TabUpdateDesc tab_update{};
        tab_update.size = sizeof(tab_update);
        tab_update.page = page_;
        tab_update.fields = fields_;
        if ((fields_ & ERUI_TAB_UPDATE_FIELD_TITLE) != 0) {
            tab_update.title = detail::utf16_view(title_);
        }
        if ((fields_ & ERUI_TAB_UPDATE_FIELD_ICON) != 0 &&
            !icon_pixels_.empty()) {
            icon.size = sizeof(icon);
            icon.format = static_cast<ERUI_TabIconFormat>(icon_format_);
            icon.byte_count = static_cast<std::uint32_t>(
                icon_pixels_.size());
            icon.pixels = icon_pixels_.data();
            tab_update.icon = &icon;
        }
        if ((fields_ & ERUI_TAB_UPDATE_FIELD_VISIBLE) != 0) {
            tab_update.visible = visible_ ? 1u : 0u;
        }
        if ((fields_ & ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER) != 0) {
            tab_update.preferred_order = preferred_order_;
        }

        ERUI_UpdateDesc update{};
        update.size = sizeof(update);
        update.kind = ERUI_UPDATE_KIND_TAB;
        update.payload = &tab_update;

        ERUI_UpdateBatchDesc batch{};
        batch.size = sizeof(batch);
        batch.updates = &update;
        batch.update_count = 1u;
        batch.update_stride = sizeof(update);
        return context_->api.commit_updates(context_->provider, &batch);
    }

private:
    friend class Tab;

    TabEdit(
        std::shared_ptr<detail::RowContext> context,
        PageHandle page) noexcept
        : context_(std::move(context)), page_(page) {}

    void set_title(std::wstring_view value) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (value.empty() ||
            value.size() > ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS ||
            !detail::view_size_fits(value.size())) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        try {
            title_.assign(value.data(), value.size());
            fields_ |= ERUI_TAB_UPDATE_FIELD_TITLE;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    void set_icon(TabIconView value) noexcept {
        if (committed_ || error_ != ERUI_OK) return;
        if (!detail::valid_tab_icon(value)) {
            error_ = ERUI_INVALID_ARGUMENT;
            return;
        }
        try {
            if (value.pixels) {
                icon_pixels_.assign(
                    value.pixels, value.pixels + value.byte_count);
                icon_format_ = value.format;
            } else {
                icon_pixels_.clear();
                icon_format_ = TabIconFormat::rgba8;
            }
            fields_ |= ERUI_TAB_UPDATE_FIELD_ICON;
        } catch (...) {
            error_ = ERUI_OUT_OF_MEMORY;
        }
    }

    std::shared_ptr<detail::RowContext> context_{};
    PageHandle page_{ERUI_INVALID_PAGE};
    ERUI_TabUpdateFields fields_{};
    ERUI_Result error_{ERUI_OK};
    bool committed_{};
    bool visible_{};
    std::int32_t preferred_order_{};
    std::wstring title_{};
    std::vector<std::uint8_t> icon_pixels_{};
    TabIconFormat icon_format_{TabIconFormat::rgba8};
};

class ButtonRow;
class ToggleRow;
class SliderRow;
class NumericSliderRow;
class CustomSliderRow;
class InlineChoiceRow;
class PopupChoiceRow;
class TextInputRow;
class ColorPickerRow;

class ButtonRowEdit final : public detail::RowEditDraft<ButtonRowEdit> {
public:
    ButtonRowEdit(const ButtonRowEdit&) = delete;
    ButtonRowEdit& operator=(const ButtonRowEdit&) = delete;
    ButtonRowEdit(ButtonRowEdit&&) noexcept = default;
    ButtonRowEdit& operator=(ButtonRowEdit&&) noexcept = default;
    ~ButtonRowEdit() = default;

    ButtonRowEdit& caption(std::wstring_view value) & noexcept {
        set_caption(value);
        return *this;
    }

    ButtonRowEdit&& caption(std::wstring_view value) && noexcept {
        set_caption(value);
        return std::move(*this);
    }

    ButtonRowEdit& caption(const StyledText& value) & noexcept {
        set_caption(value);
        return *this;
    }

    ButtonRowEdit&& caption(const StyledText& value) && noexcept {
        set_caption(value);
        return std::move(*this);
    }

    ButtonRowEdit& caption(StyledText&& value) & noexcept {
        set_caption(std::move(value));
        return *this;
    }

    ButtonRowEdit&& caption(StyledText&& value) && noexcept {
        set_caption(std::move(value));
        return std::move(*this);
    }
private:
    friend class ButtonRow;
    ButtonRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<ButtonRowEdit>(std::move(context), row) {}
};

class ToggleRowEdit final : public detail::RowEditDraft<ToggleRowEdit> {
public:
    ToggleRowEdit(const ToggleRowEdit&) = delete;
    ToggleRowEdit& operator=(const ToggleRowEdit&) = delete;
    ToggleRowEdit(ToggleRowEdit&&) noexcept = default;
    ToggleRowEdit& operator=(ToggleRowEdit&&) noexcept = default;
    ~ToggleRowEdit() = default;

private:
    friend class ToggleRow;
    ToggleRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<ToggleRowEdit>(std::move(context), row) {}
};

class SliderRowEdit final : public detail::RowEditDraft<SliderRowEdit> {
public:
    SliderRowEdit(const SliderRowEdit&) = delete;
    SliderRowEdit& operator=(const SliderRowEdit&) = delete;
    SliderRowEdit(SliderRowEdit&&) noexcept = default;
    SliderRowEdit& operator=(SliderRowEdit&&) noexcept = default;
    ~SliderRowEdit() = default;
private:
    friend class SliderRow;
    SliderRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<SliderRowEdit>(std::move(context), row) {}
};

class NumericSliderRowEdit final
    : public detail::RowEditDraft<NumericSliderRowEdit> {
public:
    NumericSliderRowEdit(const NumericSliderRowEdit&) = delete;
    NumericSliderRowEdit& operator=(const NumericSliderRowEdit&) = delete;
    NumericSliderRowEdit(NumericSliderRowEdit&&) noexcept = default;
    NumericSliderRowEdit& operator=(NumericSliderRowEdit&&) noexcept = default;
    ~NumericSliderRowEdit() = default;
private:
    friend class NumericSliderRow;
    NumericSliderRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<NumericSliderRowEdit>(
            std::move(context), row) {}
};

class CustomSliderRowEdit final
    : public detail::RowEditDraft<CustomSliderRowEdit> {
public:
    CustomSliderRowEdit(const CustomSliderRowEdit&) = delete;
    CustomSliderRowEdit& operator=(const CustomSliderRowEdit&) = delete;
    CustomSliderRowEdit(CustomSliderRowEdit&&) noexcept = default;
    CustomSliderRowEdit& operator=(CustomSliderRowEdit&&) noexcept = default;
    ~CustomSliderRowEdit() = default;
private:
    friend class CustomSliderRow;
    CustomSliderRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<CustomSliderRowEdit>(
            std::move(context), row) {}
};

class InlineChoiceRowEdit final
    : public detail::RowEditDraft<InlineChoiceRowEdit> {
public:
    InlineChoiceRowEdit(const InlineChoiceRowEdit&) = delete;
    InlineChoiceRowEdit& operator=(const InlineChoiceRowEdit&) = delete;
    InlineChoiceRowEdit(InlineChoiceRowEdit&&) noexcept = default;
    InlineChoiceRowEdit& operator=(InlineChoiceRowEdit&&) noexcept = default;
    ~InlineChoiceRowEdit() = default;
private:
    friend class InlineChoiceRow;
    InlineChoiceRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<InlineChoiceRowEdit>(
            std::move(context), row) {}
};

class PopupChoiceRowEdit final
    : public detail::RowEditDraft<PopupChoiceRowEdit> {
public:
    PopupChoiceRowEdit(const PopupChoiceRowEdit&) = delete;
    PopupChoiceRowEdit& operator=(const PopupChoiceRowEdit&) = delete;
    PopupChoiceRowEdit(PopupChoiceRowEdit&&) noexcept = default;
    PopupChoiceRowEdit& operator=(PopupChoiceRowEdit&&) noexcept = default;
    ~PopupChoiceRowEdit() = default;

    PopupChoiceRowEdit& caption(std::wstring_view value) & noexcept {
        set_caption(value);
        return *this;
    }

    PopupChoiceRowEdit&& caption(std::wstring_view value) && noexcept {
        set_caption(value);
        return std::move(*this);
    }

    PopupChoiceRowEdit& caption(const StyledText& value) & noexcept {
        set_caption(value);
        return *this;
    }

    PopupChoiceRowEdit&& caption(const StyledText& value) && noexcept {
        set_caption(value);
        return std::move(*this);
    }

    PopupChoiceRowEdit& caption(StyledText&& value) & noexcept {
        set_caption(std::move(value));
        return *this;
    }

    PopupChoiceRowEdit&& caption(StyledText&& value) && noexcept {
        set_caption(std::move(value));
        return std::move(*this);
    }
private:
    friend class PopupChoiceRow;
    PopupChoiceRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<PopupChoiceRowEdit>(
            std::move(context), row) {}
};

class TextInputRowEdit final
    : public detail::RowEditDraft<TextInputRowEdit> {
public:
    TextInputRowEdit(const TextInputRowEdit&) = delete;
    TextInputRowEdit& operator=(const TextInputRowEdit&) = delete;
    TextInputRowEdit(TextInputRowEdit&&) noexcept = default;
    TextInputRowEdit& operator=(TextInputRowEdit&&) noexcept = default;
    ~TextInputRowEdit() = default;
private:
    friend class TextInputRow;
    TextInputRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<TextInputRowEdit>(
            std::move(context), row) {}
};

class ColorPickerRowEdit final
    : public detail::RowEditDraft<ColorPickerRowEdit> {
public:
    ColorPickerRowEdit(const ColorPickerRowEdit&) = delete;
    ColorPickerRowEdit& operator=(const ColorPickerRowEdit&) = delete;
    ColorPickerRowEdit(ColorPickerRowEdit&&) noexcept = default;
    ColorPickerRowEdit& operator=(ColorPickerRowEdit&&) noexcept = default;
    ~ColorPickerRowEdit() = default;
private:
    friend class ColorPickerRow;
    ColorPickerRowEdit(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : detail::RowEditDraft<ColorPickerRowEdit>(
            std::move(context), row) {}
};

class ButtonRow final : public RowRef {
public:
    ButtonRow() noexcept = default;
    [[nodiscard]] ButtonRowEdit edit() const noexcept {
        return ButtonRowEdit(edit_context(), edit_handle());
    }

private:
    friend class Page;
    ButtonRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class ToggleRow final : public RowRef {
public:
    ToggleRow() noexcept = default;
    [[nodiscard]] ToggleRowEdit edit() const noexcept {
        return ToggleRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_value(bool value) const noexcept {
        return set_byte_value(value ? 1u : 0u);
    }
    ERUI_Result get_value(bool& output) const noexcept {
        std::uint8_t value{};
        const ERUI_Result result = get_byte_value(value);
        if (result == ERUI_OK) output = value != 0;
        return result;
    }

private:
    friend class Page;
    ToggleRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class SliderRow final : public RowRef {
public:
    SliderRow() noexcept = default;
    [[nodiscard]] SliderRowEdit edit() const noexcept {
        return SliderRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_value(std::uint8_t value) const noexcept {
        return set_byte_value(value);
    }
    ERUI_Result get_value(std::uint8_t& output) const noexcept {
        return get_byte_value(output);
    }

private:
    friend class Page;
    SliderRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class NumericSliderRow final : public RowRef {
public:
    NumericSliderRow() noexcept = default;
    [[nodiscard]] NumericSliderRowEdit edit() const noexcept {
        return NumericSliderRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_value(double value) const noexcept {
        return set_numeric_value(value);
    }
    ERUI_Result get_value(double& output) const noexcept {
        return get_numeric_value(output);
    }

private:
    friend class Page;
    NumericSliderRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class CustomSliderRow final : public RowRef {
public:
    CustomSliderRow() noexcept = default;
    [[nodiscard]] CustomSliderRowEdit edit() const noexcept {
        return CustomSliderRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_index(std::uint32_t index) const noexcept {
        return set_custom_index(index);
    }
    ERUI_Result get_index(std::uint32_t& output) const noexcept {
        return get_custom_index(output);
    }

private:
    friend class Page;
    CustomSliderRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class InlineChoiceRow final : public RowRef {
public:
    InlineChoiceRow() noexcept = default;
    [[nodiscard]] InlineChoiceRowEdit edit() const noexcept {
        return InlineChoiceRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_index(std::uint8_t index) const noexcept {
        return set_byte_value(index);
    }
    ERUI_Result get_index(std::uint8_t& output) const noexcept {
        return get_byte_value(output);
    }

private:
    friend class Page;
    InlineChoiceRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class PopupChoiceRow final : public RowRef {
public:
    PopupChoiceRow() noexcept = default;
    [[nodiscard]] PopupChoiceRowEdit edit() const noexcept {
        return PopupChoiceRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_index(std::uint8_t index) const noexcept {
        return set_byte_value(index);
    }
    ERUI_Result get_index(std::uint8_t& output) const noexcept {
        return get_byte_value(output);
    }

private:
    friend class Page;
    PopupChoiceRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class TextInputRow final : public RowRef {
public:
    TextInputRow() noexcept = default;
    [[nodiscard]] TextInputRowEdit edit() const noexcept {
        return TextInputRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_text(std::wstring_view value) const noexcept {
        return set_text_value(value);
    }
    ERUI_Result get_text(std::wstring& output) const noexcept {
        return get_text_value(output);
    }

private:
    friend class Page;
    TextInputRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class ColorPickerRow final : public RowRef {
public:
    ColorPickerRow() noexcept = default;
    [[nodiscard]] ColorPickerRowEdit edit() const noexcept {
        return ColorPickerRowEdit(edit_context(), edit_handle());
    }

    ERUI_Result set_color(Color value) const noexcept {
        return set_color_value(value);
    }
    ERUI_Result get_color(Color& output) const noexcept {
        return get_color_value(output);
    }

private:
    friend class Page;
    ColorPickerRow(
        std::shared_ptr<detail::RowContext> context,
        RowHandle row) noexcept
        : RowRef(std::move(context), row) {}
};

class Page {
public:
    Page() = default;

    ButtonRow add_button(
        std::wstring_view label,
        std::wstring_view help,
        ButtonCallback callback,
        void* user_data = nullptr,
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_button text");
            return {};
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
        return make_row<ButtonRow>(row);
    }

    template <void (*Function)() noexcept>
    ButtonRow add_button(
        std::wstring_view label,
        std::wstring_view help,
        bool enabled = true) noexcept {
        return add_button(label, help, &button_thunk<Function>, nullptr, enabled);
    }

    ToggleRow add_toggle(
        std::wstring_view label,
        std::wstring_view help,
        std::uint8_t initial_value,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr,
        bool enabled = true) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_toggle text");
            return {};
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
        return make_row<ToggleRow>(row);
    }

    SliderRow add_slider(
        std::wstring_view label,
        std::wstring_view help,
        const SliderOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size())) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_slider text");
            return {};
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
        return make_row<SliderRow>(row);
    }

    NumericSliderRow add_slider(
        std::wstring_view label,
        std::wstring_view help,
        const NumericSliderOptions& options,
        NumericSliderChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return {};
        }
        if (!draft_->api_.add_numeric_slider ||
            (draft_->api_.capabilities & ERUI_CAP_NUMERIC_SLIDER) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_numeric_slider");
            return {};
        }
        detail::PreparedNumericSlider prepared{};
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            !detail::view_size_fits(options.prefix.size()) ||
            !detail::view_size_fits(options.suffix.size()) ||
            (!callback && user_data) ||
            !detail::prepare_numeric_slider(options, prepared)) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_numeric_slider");
            return {};
        }

        ERUI_NumericSliderDesc description{};
        description.size = sizeof(description);
        description.label = detail::utf16_view(label);
        description.help = detail::utf16_view(help);
        description.changed_callback =
            reinterpret_cast<ERUI_NumericSliderChangedCallback>(callback);
        description.user_data = user_data;
        description.minimum_scaled_value = prepared.minimum;
        description.maximum_scaled_value = prepared.maximum;
        description.step_scaled_value = prepared.step;
        description.initial_scaled_value = prepared.initial_value;
        description.decimal_places = prepared.decimal_places;
        description.enabled = options.enabled ? 1u : 0u;
        description.prefix = detail::utf16_view(options.prefix);
        description.suffix = detail::utf16_view(options.suffix);
        ERUI_RowHandle row{};
        draft_->record(draft_->api_.add_numeric_slider(
            draft_->provider_, page_, &description, &row),
            L"add_numeric_slider");
        return make_row<NumericSliderRow>(row);
    }

    template <auto Function>
    NumericSliderRow add_slider(
        std::wstring_view label,
        std::wstring_view help,
        const NumericSliderOptions& options) noexcept {
        using Expected = void (*)(const NumericSliderChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "NumericSlider callback must be void(const NumericSliderChange&) noexcept");
        return add_slider(
            label,
            help,
            options,
            &numeric_slider_thunk<Function>);
    }

    template <auto Function, typename State>
    NumericSliderRow add_slider(
        std::wstring_view label,
        std::wstring_view help,
        const NumericSliderOptions& options,
        State& state) noexcept {
        using Expected = void (*)(
            State&, const NumericSliderChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "Stateful NumericSlider callback must be void(State&, const NumericSliderChange&) noexcept");
        return add_slider(
            label,
            help,
            options,
            &stateful_numeric_slider_thunk<Function, State>,
            &state);
    }

    CustomSliderRow add_custom_slider(
        std::wstring_view label,
        std::wstring_view help,
        const CustomSliderOptions& options,
        CustomSliderChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return {};
        }
        if (!draft_->api_.add_custom_slider ||
            (draft_->api_.capabilities & ERUI_CAP_CUSTOM_SLIDER) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_custom_slider");
            return {};
        }
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            options.value_count < ERUI_SLIDER_MIN_VALUE_COUNT ||
            options.value_count > ERUI_SLIDER_MAX_VALUE_COUNT ||
            options.initial_index >= options.value_count ||
            !options.format_value || (!callback && user_data)) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_custom_slider");
            return {};
        }

        try {
            std::vector<std::wstring> values;
            values.reserve(options.value_count);
            for (std::uint32_t index = 0;
                 index < options.value_count;
                 ++index) {
                std::wstring value = options.format_value(
                    index, options.value_count);
                if (value.empty() ||
                    value.size() > ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH ||
                    !detail::view_size_fits(value.size())) {
                    draft_->record(
                        ERUI_INVALID_ARGUMENT, L"add_custom_slider value");
                    return {};
                }
                values.push_back(std::move(value));
            }

            std::vector<ERUI_Utf16View> native_values;
            native_values.reserve(values.size());
            for (const std::wstring& value : values) {
                native_values.push_back(detail::utf16_view(value));
            }

            ERUI_CustomSliderDesc description{};
            description.size = sizeof(description);
            description.label = detail::utf16_view(label);
            description.help = detail::utf16_view(help);
            description.changed_callback =
                reinterpret_cast<ERUI_CustomSliderChangedCallback>(callback);
            description.user_data = user_data;
            description.values = native_values.data();
            description.value_count = options.value_count;
            description.initial_index = options.initial_index;
            description.enabled = options.enabled ? 1u : 0u;
            ERUI_RowHandle row{};
            draft_->record(draft_->api_.add_custom_slider(
                draft_->provider_, page_, &description, &row),
                L"add_custom_slider");
            return make_row<CustomSliderRow>(row);
        } catch (const std::bad_alloc&) {
            draft_->record(ERUI_OUT_OF_MEMORY, L"add_custom_slider");
            return {};
        } catch (...) {
            draft_->record(ERUI_CALLBACK_REJECTED, L"add_custom_slider formatter");
            return {};
        }
    }

    template <auto Function>
    CustomSliderRow add_custom_slider(
        std::wstring_view label,
        std::wstring_view help,
        const CustomSliderOptions& options) noexcept {
        using Expected = void (*)(const CustomSliderChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "CustomSlider callback must be void(const CustomSliderChange&) noexcept");
        return add_custom_slider(
            label,
            help,
            options,
            &custom_slider_thunk<Function>);
    }

    template <auto Function, typename State>
    CustomSliderRow add_custom_slider(
        std::wstring_view label,
        std::wstring_view help,
        const CustomSliderOptions& options,
        State& state) noexcept {
        using Expected = void (*)(
            State&, const CustomSliderChange&) noexcept;
        static_assert(std::is_same<decltype(Function), Expected>::value,
            "Stateful CustomSlider callback must be void(State&, const CustomSliderChange&) noexcept");
        return add_custom_slider(
            label,
            help,
            options,
            &stateful_custom_slider_thunk<Function, State>,
            &state);
    }

    InlineChoiceRow add_inline_choice(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        return make_row<InlineChoiceRow>(add_choice_impl(
            label, help, options, callback, user_data,
            draft_ ? draft_->api_.add_inline_choice : nullptr,
            L"add_inline_choice"));
    }

    PopupChoiceRow add_popup_choice(
        std::wstring_view label,
        std::wstring_view help,
        const ChoiceOptions& options,
        ValueChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        return make_row<PopupChoiceRow>(add_choice_impl(
            label, help, options, callback, user_data,
            draft_ ? draft_->api_.add_popup_choice : nullptr,
            L"add_popup_choice"));
    }

    TextInputRow add_text_input(
        std::wstring_view label,
        std::wstring_view help,
        const TextInputOptions& options,
        TextInputChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return {};
        }
        if (!draft_->api_.add_text_input ||
            (draft_->api_.capabilities & ERUI_CAP_TEXT_INPUT) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_text_input");
            return {};
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
            return {};
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
        return make_row<TextInputRow>(row);
    }

    template <auto Function>
    TextInputRow add_text_input(
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
    TextInputRow add_text_input(
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

    ColorPickerRow add_color_picker(
        std::wstring_view label,
        std::wstring_view help,
        const ColorPickerOptions& options,
        ColorPickerChangedCallback callback = nullptr,
        void* user_data = nullptr) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) {
            return {};
        }
        if (!draft_->api_.add_color_picker ||
            (draft_->api_.capabilities & ERUI_CAP_COLOR_PICKER) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_color_picker");
            return {};
        }
        if (!detail::view_size_fits(label.size()) ||
            !detail::view_size_fits(help.size()) ||
            (!callback && user_data)) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_color_picker");
            return {};
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
        return make_row<ColorPickerRow>(row);
    }

    template <auto Function>
    ColorPickerRow add_color_picker(
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
    ColorPickerRow add_color_picker(
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

    Submenu add_submenu(
        std::wstring_view label,
        std::wstring_view help,
        std::wstring_view page_title = {},
        std::wstring_view page_help = {},
        bool enabled = true) noexcept;

    Page& set_presentation(const PagePresentation& presentation) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return *this;
        if (!detail::view_size_fits(presentation.menu_title.size()) ||
            !detail::view_size_fits(presentation.page_title.size()) ||
            (custom_tab_ && presentation.page_title.size() >
                ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS)) {
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
    friend class Submenu;
    friend class Tab;
    Page(std::shared_ptr<detail::Draft> draft,
        ERUI_PageHandle page,
        bool custom_tab = false) noexcept
        : draft_(std::move(draft)), page_(page), custom_tab_(custom_tab) {}

    template <typename Row>
    [[nodiscard]] Row make_row(RowHandle row) const noexcept {
        if (!draft_ || !draft_->row_context_ ||
            row == ERUI_INVALID_ROW || draft_->failed_) {
            return {};
        }
        return Row(draft_->row_context_, row);
    }

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

    template <auto Function>
    static void ERUI_CALL numeric_slider_thunk(
        void*,
        const ERUI_NumericSliderChangeContext* context) noexcept {
        NumericSliderChange change{};
        if (detail::make_numeric_slider_change(context, change)) {
            Function(change);
        }
    }

    template <auto Function, typename State>
    static void ERUI_CALL stateful_numeric_slider_thunk(
        void* user_data,
        const ERUI_NumericSliderChangeContext* context) noexcept {
        auto* state = static_cast<State*>(user_data);
        NumericSliderChange change{};
        if (state && detail::make_numeric_slider_change(context, change)) {
            Function(*state, change);
        }
    }

    template <auto Function>
    static void ERUI_CALL custom_slider_thunk(
        void*,
        const ERUI_CustomSliderChangeContext* context) noexcept {
        CustomSliderChange change{};
        if (detail::make_custom_slider_change(context, change)) {
            Function(change);
        }
    }

    template <auto Function, typename State>
    static void ERUI_CALL stateful_custom_slider_thunk(
        void* user_data,
        const ERUI_CustomSliderChangeContext* context) noexcept {
        auto* state = static_cast<State*>(user_data);
        CustomSliderChange change{};
        if (state && detail::make_custom_slider_change(context, change)) {
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
    bool custom_tab_{};
};

/*
 * A submenu is both its child-page builder and a stable reference to the row
 * that opens that page. Inheriting Page preserves every existing add_* call
 * and permits an explicit conversion/slice to Page for older source code.
 */
class Submenu final : public Page {
public:
    Submenu() noexcept = default;

    [[nodiscard]] bool valid() const noexcept {
        return Page::valid() ||
            (opener_ != ERUI_INVALID_ROW && child_ != ERUI_INVALID_PAGE &&
             context_ && context_->committed() &&
             context_->provider != ERUI_INVALID_PROVIDER);
    }
    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] SubmenuEdit edit() const noexcept {
        return SubmenuEdit(context_, opener_, child_);
    }

    [[nodiscard]] RowHandle opener_handle() const noexcept {
        return opener_;
    }

    [[nodiscard]] PageHandle page_handle() const noexcept {
        return child_;
    }

private:
    friend class Page;

    Submenu(
        std::shared_ptr<detail::Draft> draft,
        PageHandle child,
        RowHandle opener) noexcept
        : Page(draft, child),
          context_(draft ? draft->row_context_ : nullptr),
          opener_(opener),
          child_(child) {}

    std::shared_ptr<detail::RowContext> context_{};
    RowHandle opener_{ERUI_INVALID_ROW};
    PageHandle child_{ERUI_INVALID_PAGE};
};

/*
 * A custom tab is also its ordinary page builder. Inheriting Page preserves
 * existing code that stores the result of Menu::add_tab as Page while auto
 * receives the richer API 1.4 reference.
 */
class Tab final : public Page {
public:
    Tab() noexcept = default;

    [[nodiscard]] bool valid() const noexcept {
        return Page::valid() ||
            (page_ != ERUI_INVALID_PAGE && context_ &&
             context_->committed() &&
             context_->provider != ERUI_INVALID_PROVIDER);
    }
    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] TabEdit edit() const noexcept {
        return TabEdit(context_, page_);
    }

    [[nodiscard]] PageHandle page_handle() const noexcept {
        return page_;
    }

private:
    friend class Menu;

    Tab(
        std::shared_ptr<detail::Draft> draft,
        PageHandle page) noexcept
        : Page(draft, page, true),
          context_(draft ? draft->row_context_ : nullptr),
          page_(page) {}

    std::shared_ptr<detail::RowContext> context_{};
    PageHandle page_{ERUI_INVALID_PAGE};
};

inline Submenu Page::add_submenu(
    std::wstring_view label,
    std::wstring_view help,
    std::wstring_view page_title,
    std::wstring_view page_help,
    bool enabled) noexcept {
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
    PageHandle child{};
    RowHandle row{};
    draft_->record(draft_->api_.add_submenu(
        draft_->provider_, page_, &desc, &child, &row), L"add_submenu");
    if (draft_->failed_ || child == ERUI_INVALID_PAGE ||
        row == ERUI_INVALID_ROW) {
        return {};
    }
    return Submenu(draft_, child, row);
}

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
    void add_title_screen_text(
        TitleScreenCorner corner,
        const TitleScreenTextOptions& options) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return;
        if (!draft_->api_.add_title_screen_text ||
            (draft_->api_.capabilities & ERUI_CAP_TITLE_SCREEN_TEXT) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_title_screen_text");
            return;
        }
        if (!detail::valid_title_screen_corner(corner) ||
            !detail::valid_title_screen_text(options.text)) {
            draft_->record(
                ERUI_INVALID_ARGUMENT, L"add_title_screen_text");
            return;
        }

        ERUI_TitleScreenTextDesc description{};
        description.size = sizeof(description);
        description.text = detail::utf16_view(options.text);
        description.corner = static_cast<ERUI_TitleScreenCorner>(corner);
        if (options.color) {
            description.flags = ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR;
            description.color = detail::native_color(*options.color);
        }
        draft_->record(draft_->api_.add_title_screen_text(
            draft_->provider_, &description), L"add_title_screen_text");
    }
    Tab add_tab(const TabOptions& options) noexcept {
        if (!draft_ || !draft_->open() || draft_->failed_) return {};
        if (!draft_->api_.add_tab ||
            (draft_->api_.capabilities & ERUI_CAP_CUSTOM_TABS) == 0) {
            draft_->record(ERUI_NOT_SUPPORTED, L"add_tab");
            return {};
        }
        if (!detail::valid_machine_identifier(options.tab_id) ||
            options.title.empty() ||
            options.title.size() >
                ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS ||
            !detail::view_size_fits(options.title.size()) ||
            !detail::valid_tab_icon(options.icon)) {
            draft_->record(ERUI_INVALID_ARGUMENT, L"add_tab");
            return {};
        }

        ERUI_TabIconDesc icon{};
        const ERUI_TabIconDesc* icon_pointer = nullptr;
        if (options.icon.pixels) {
            icon.size = sizeof(icon);
            icon.format = static_cast<ERUI_TabIconFormat>(
                options.icon.format);
            icon.byte_count = ERUI_TAB_ICON_RGBA8_BYTE_COUNT;
            icon.pixels = options.icon.pixels;
            icon_pointer = &icon;
        }

        ERUI_TabDesc description{};
        description.size = sizeof(description);
        description.tab_id = detail::string_view(options.tab_id);
        description.title = detail::utf16_view(options.title);
        description.preferred_order = options.preferred_order;
        description.icon = icon_pointer;

        ERUI_PageHandle page_handle{ERUI_INVALID_PAGE};
        const ERUI_Result result = draft_->api_.add_tab(
            draft_->provider_, &description, &page_handle);
        draft_->record(result, L"add_tab");
        if (result == ERUI_OK && page_handle == ERUI_INVALID_PAGE) {
            draft_->record(ERUI_INTERNAL_ERROR, L"add_tab");
        }
        return result == ERUI_OK && page_handle != ERUI_INVALID_PAGE
            ? Tab(draft_, page_handle)
            : Tab{};
    }
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
    ERUI_Result set_numeric_slider_value(
        RowHandle row,
        double value) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::numeric_slider) ||
            !api_.set_numeric_slider_value) {
            return ERUI_NOT_SUPPORTED;
        }
        ERUI_DecimalValue native_value{};
        if (!detail::infer_decimal_value(value, native_value)) {
            return ERUI_INVALID_ARGUMENT;
        }
        return api_.set_numeric_slider_value(
            provider_, row, &native_value);
    }
    ERUI_Result get_numeric_slider_value(
        RowHandle row,
        double& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::numeric_slider) ||
            !api_.get_numeric_slider_value) {
            return ERUI_NOT_SUPPORTED;
        }
        ERUI_DecimalValue native_value{};
        const ERUI_Result result = api_.get_numeric_slider_value(
            provider_, row, &native_value);
        if (result != ERUI_OK) return result;
        if (native_value.reserved != 0 ||
            native_value.decimal_places >
                ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES) {
            return ERUI_INTERNAL_ERROR;
        }
        output = detail::unscale_decimal_value(
            native_value.scaled_value, native_value.decimal_places);
        return ERUI_OK;
    }
    ERUI_Result set_custom_slider_index(
        RowHandle row,
        std::uint32_t index) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::custom_slider) ||
            !api_.set_custom_slider_index) {
            return ERUI_NOT_SUPPORTED;
        }
        return api_.set_custom_slider_index(provider_, row, index);
    }
    ERUI_Result get_custom_slider_index(
        RowHandle row,
        std::uint32_t& output) const noexcept {
        if (!valid()) return ERUI_INVALID_HANDLE;
        if (!supports(Capability::custom_slider) ||
            !api_.get_custom_slider_index) {
            return ERUI_NOT_SUPPORTED;
        }
        return api_.get_custom_slider_index(provider_, row, &output);
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
                api_, ERUI_API_VERSION_1_4)) {
            output.error_ = Error(ErrorCode::incompatible_api,
                ERUI_UNSUPPORTED_VERSION,
                L"A valid ERNativeUI API 1.4 connection is required.");
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
