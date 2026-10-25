#ifndef ERNATIVEUI_ERUI_H_INCLUDED
#define ERNATIVEUI_ERUI_H_INCLUDED

/*
 * ERNativeUI public binary interface.
 *
 * This header is intentionally valid C99 and C++. Do not add STL types,
 * C++ classes, exceptions, compiler-owned strings, or allocator ownership to
 * this boundary. All input strings are copied before an API call returns.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(_WIN64)
#  error ERNativeUI supports Windows x64 clients only.
#endif

#if defined(_WIN32)
#  define ERUI_CALL __cdecl
#  if defined(ERUI_HOST_EXPORTS)
#    define ERUI_EXPORT __declspec(dllexport)
#  else
#    define ERUI_EXPORT
#  endif
#else
#  define ERUI_CALL
#  define ERUI_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Public API contract versions; independent of the ERNativeUI release version. */
#define ERUI_VERSION_ENCODE(major, minor) \
    ((((uint32_t)(major)) << 16u) | ((uint32_t)(minor) & 0xFFFFu))
#define ERUI_API_VERSION_1_0 ERUI_VERSION_ENCODE(1u, 0u)
#define ERUI_API_VERSION_1_1 ERUI_VERSION_ENCODE(1u, 1u)
#define ERUI_API_VERSION_1_2 ERUI_VERSION_ENCODE(1u, 2u)
#define ERUI_API_VERSION_1_3 ERUI_VERSION_ENCODE(1u, 3u)
#define ERUI_API_VERSION_1_4 ERUI_VERSION_ENCODE(1u, 4u)
#define ERUI_API_VERSION_CURRENT ERUI_API_VERSION_1_4

typedef uint32_t ERUI_Result;
enum {
    ERUI_OK = 0u,
    ERUI_INVALID_ARGUMENT = 1u,
    ERUI_HOST_NOT_READY = 2u,
    ERUI_HOST_FAILED = 3u,
    ERUI_UNSUPPORTED_VERSION = 4u,
    ERUI_DUPLICATE_PROVIDER_ID = 5u,
    ERUI_INVALID_HANDLE = 6u,
    ERUI_ALREADY_COMMITTED = 7u,
    ERUI_REGISTRATION_CLOSED = 8u,
    ERUI_OUT_OF_MEMORY = 9u,
    ERUI_CALLBACK_REJECTED = 10u,
    ERUI_INTERNAL_ERROR = 11u,
    ERUI_QUEUE_FULL = 12u,
    ERUI_NOT_SUPPORTED = 13u,
    ERUI_BUFFER_TOO_SMALL = 14u,
    ERUI_DUPLICATE_ACTION_ID = 15u,
    ERUI_NOT_FOUND = 16u,
    ERUI_STORAGE_NOT_LOADED = 17u,
    ERUI_STORAGE_ALREADY_LOADED = 18u,
    ERUI_STORAGE_IO_ERROR = 19u,
    ERUI_STORAGE_FORMAT_ERROR = 20u,
    ERUI_DUPLICATE_TAB_ID = 21u,
    ERUI_TAB_LIMIT_REACHED = 22u,
    ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED = 23u
};

typedef uint64_t ERUI_ProviderHandle;
typedef uint64_t ERUI_PageHandle;
typedef uint64_t ERUI_RowHandle;
typedef uint64_t ERUI_InputSectionHandle;
typedef uint64_t ERUI_InputActionHandle;
typedef uint64_t ERUI_StorageHandle;

#define ERUI_INVALID_PROVIDER ((ERUI_ProviderHandle)0u)
#define ERUI_INVALID_PAGE ((ERUI_PageHandle)0u)
#define ERUI_INVALID_ROW ((ERUI_RowHandle)0u)
#define ERUI_INVALID_INPUT_SECTION ((ERUI_InputSectionHandle)0u)
#define ERUI_INVALID_INPUT_ACTION ((ERUI_InputActionHandle)0u)
#define ERUI_INVALID_STORAGE ((ERUI_StorageHandle)0u)

/*
 * Stable logical destinations for rows contributed to Elden Ring's built-in
 * Configuration pages. These values are contiguous ERNativeUI identifiers;
 * they are not Elden Ring's private native category IDs. COUNT is a sentinel
 * for iteration and is never accepted as a page.
 */
typedef uint32_t ERUI_BuiltinPage;
enum {
    ERUI_BUILTIN_PAGE_GAME_OPTIONS = 0u,
    ERUI_BUILTIN_PAGE_CAMERA_OPTIONS = 1u,
    ERUI_BUILTIN_PAGE_DISPLAY = 2u,
    ERUI_BUILTIN_PAGE_SOUND = 3u,
    ERUI_BUILTIN_PAGE_NETWORK = 4u,
    ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE = 5u,
    ERUI_BUILTIN_PAGE_GRAPHICS = 6u,
    ERUI_BUILTIN_PAGE_COUNT = 7u
};

typedef uint64_t ERUI_Capabilities;
enum {
    ERUI_CAP_TOGGLE = UINT64_C(1) << 0,
    ERUI_CAP_SLIDER = UINT64_C(1) << 1,
    ERUI_CAP_BUTTON = UINT64_C(1) << 2,
    ERUI_CAP_SUBMENU = UINT64_C(1) << 3,
    ERUI_CAP_PAGINATION = UINT64_C(1) << 4,
    ERUI_CAP_HOST_OWNED_VALUES = UINT64_C(1) << 5,
    ERUI_CAP_PAGE_PRESENTATION = UINT64_C(1) << 6,
    ERUI_CAP_ALERT = UINT64_C(1) << 7,
    ERUI_CAP_INLINE_CHOICE = UINT64_C(1) << 8,
    ERUI_CAP_POPUP_CHOICE = UINT64_C(1) << 9,
    ERUI_CAP_GAME_LANGUAGE = UINT64_C(1) << 10,
    ERUI_CAP_TEXT_INPUT = UINT64_C(1) << 11,
    ERUI_CAP_COLOR_PICKER = UINT64_C(1) << 12,
    ERUI_CAP_BUILTIN_PAGES = UINT64_C(1) << 13,
    ERUI_CAP_INPUT_BINDINGS = UINT64_C(1) << 14,
    ERUI_CAP_STORAGE = UINT64_C(1) << 15,
    ERUI_CAP_NUMERIC_SLIDER = UINT64_C(1) << 16,
    ERUI_CAP_CUSTOM_SLIDER = UINT64_C(1) << 17,
    ERUI_CAP_CUSTOM_TABS = UINT64_C(1) << 18,
    ERUI_CAP_TITLE_SCREEN_TEXT = UINT64_C(1) << 19,
    ERUI_CAP_UPDATES = UINT64_C(1) << 20,
    ERUI_CAP_ROW_AVAILABILITY = UINT64_C(1) << 21
};

/*
 * API 1.3 tab icons use row-major RGBA bytes for an exact 32x32 image (4096
 * bytes total). Each pixel stores red, green, blue, then alpha. Alpha 0 is
 * transparent, alpha 255 is opaque, and values between them are translucent.
 * Supply the colour bytes directly; the host performs the native rendering
 * conversion. The host copies the complete image before add_tab returns.
 * PNG/JPEG data, alternate sizes, row padding, and client-owned image
 * lifetimes are not part of API 1.3.
 */
#define ERUI_TAB_ICON_WIDTH 32u
#define ERUI_TAB_ICON_HEIGHT 32u
#define ERUI_TAB_ICON_RGBA8_BYTE_COUNT \
    (ERUI_TAB_ICON_WIDTH * ERUI_TAB_ICON_HEIGHT * 4u)
/* Process-wide committed custom-tab ceiling in API 1.3. */
#define ERUI_CUSTOM_TAB_MAX_COUNT 1024u
/* Maximum UTF-16 code units in a custom-tab title, excluding its terminator. */
#define ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS 95u
/* Maximum UTF-16 code units in one title-screen line. */
#define ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS 255u
/* Process-wide committed title-screen line ceiling in API 1.3. */
#define ERUI_TITLE_SCREEN_TEXT_MAX_LINES 16u

typedef uint32_t ERUI_TabIconFormat;
enum {
    ERUI_TAB_ICON_FORMAT_NONE = 0u,
    ERUI_TAB_ICON_FORMAT_RGBA8 = 1u
};

typedef struct ERUI_TabIconDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_TabIconFormat format;
    uint32_t byte_count;
    const uint8_t* pixels;
    uint32_t reserved[2];
} ERUI_TabIconDesc;

/* Device alternatives belonging to one logical input action. */
typedef uint32_t ERUI_InputDevices;
enum {
    ERUI_INPUT_DEVICE_NONE = 0u,
    ERUI_INPUT_DEVICE_CONTROLLER = 1u << 0,
    ERUI_INPUT_DEVICE_KEYBOARD = 1u << 1,
    ERUI_INPUT_DEVICE_MOUSE = 1u << 2,
    ERUI_INPUT_DEVICE_ALL = ERUI_INPUT_DEVICE_CONTROLLER |
        ERUI_INPUT_DEVICE_KEYBOARD |
        ERUI_INPUT_DEVICE_MOUSE
};

/*
 * Stable semantic input identifiers. These are ERNativeUI values, not Elden
 * Ring's private native tokens. Zero and COUNT are never valid assignments.
 */
typedef uint32_t ERUI_ControllerButton;
enum {
    ERUI_CONTROLLER_BUTTON_INVALID = 0u,
    ERUI_CONTROLLER_BUTTON_DPAD_UP = 1u,
    ERUI_CONTROLLER_BUTTON_DPAD_DOWN = 2u,
    ERUI_CONTROLLER_BUTTON_DPAD_LEFT = 3u,
    ERUI_CONTROLLER_BUTTON_DPAD_RIGHT = 4u,
    ERUI_CONTROLLER_BUTTON_FACE_SOUTH = 5u,
    ERUI_CONTROLLER_BUTTON_FACE_EAST = 6u,
    ERUI_CONTROLLER_BUTTON_FACE_WEST = 7u,
    ERUI_CONTROLLER_BUTTON_FACE_NORTH = 8u,
    ERUI_CONTROLLER_BUTTON_LEFT_SHOULDER = 9u,
    ERUI_CONTROLLER_BUTTON_RIGHT_SHOULDER = 10u,
    ERUI_CONTROLLER_BUTTON_LEFT_TRIGGER = 11u,
    ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER = 12u,
    ERUI_CONTROLLER_BUTTON_LEFT_STICK = 13u,
    ERUI_CONTROLLER_BUTTON_RIGHT_STICK = 14u,
    ERUI_CONTROLLER_BUTTON_COUNT = 15u
};

/* Letter names denote physical US/QWERTY positions. Elden Ring localizes the
 * displayed keycap for the active keyboard layout. */
typedef uint32_t ERUI_KeyboardKey;
enum {
    ERUI_KEYBOARD_KEY_INVALID = 0u,
    ERUI_KEYBOARD_KEY_DIGIT_1 = 1u,
    ERUI_KEYBOARD_KEY_DIGIT_2 = 2u,
    ERUI_KEYBOARD_KEY_DIGIT_3 = 3u,
    ERUI_KEYBOARD_KEY_DIGIT_4 = 4u,
    ERUI_KEYBOARD_KEY_DIGIT_5 = 5u,
    ERUI_KEYBOARD_KEY_DIGIT_6 = 6u,
    ERUI_KEYBOARD_KEY_DIGIT_7 = 7u,
    ERUI_KEYBOARD_KEY_DIGIT_8 = 8u,
    ERUI_KEYBOARD_KEY_DIGIT_9 = 9u,
    ERUI_KEYBOARD_KEY_DIGIT_0 = 10u,
    ERUI_KEYBOARD_KEY_BACKSPACE = 11u,
    ERUI_KEYBOARD_KEY_TAB = 12u,
    ERUI_KEYBOARD_KEY_Q = 13u,
    ERUI_KEYBOARD_KEY_W = 14u,
    ERUI_KEYBOARD_KEY_E = 15u,
    ERUI_KEYBOARD_KEY_R = 16u,
    ERUI_KEYBOARD_KEY_T = 17u,
    ERUI_KEYBOARD_KEY_Y = 18u,
    ERUI_KEYBOARD_KEY_U = 19u,
    ERUI_KEYBOARD_KEY_I = 20u,
    ERUI_KEYBOARD_KEY_O = 21u,
    ERUI_KEYBOARD_KEY_P = 22u,
    ERUI_KEYBOARD_KEY_ENTER = 23u,
    ERUI_KEYBOARD_KEY_LEFT_CONTROL = 24u,
    ERUI_KEYBOARD_KEY_A = 25u,
    ERUI_KEYBOARD_KEY_S = 26u,
    ERUI_KEYBOARD_KEY_D = 27u,
    ERUI_KEYBOARD_KEY_F = 28u,
    ERUI_KEYBOARD_KEY_G = 29u,
    ERUI_KEYBOARD_KEY_H = 30u,
    ERUI_KEYBOARD_KEY_J = 31u,
    ERUI_KEYBOARD_KEY_K = 32u,
    ERUI_KEYBOARD_KEY_L = 33u,
    ERUI_KEYBOARD_KEY_LEFT_SHIFT = 34u,
    ERUI_KEYBOARD_KEY_Z = 35u,
    ERUI_KEYBOARD_KEY_X = 36u,
    ERUI_KEYBOARD_KEY_C = 37u,
    ERUI_KEYBOARD_KEY_V = 38u,
    ERUI_KEYBOARD_KEY_B = 39u,
    ERUI_KEYBOARD_KEY_N = 40u,
    ERUI_KEYBOARD_KEY_M = 41u,
    ERUI_KEYBOARD_KEY_RIGHT_SHIFT = 42u,
    ERUI_KEYBOARD_KEY_LEFT_ALT = 43u,
    ERUI_KEYBOARD_KEY_SPACE = 44u,
    ERUI_KEYBOARD_KEY_NUMPAD_7 = 45u,
    ERUI_KEYBOARD_KEY_NUMPAD_8 = 46u,
    ERUI_KEYBOARD_KEY_NUMPAD_9 = 47u,
    ERUI_KEYBOARD_KEY_NUMPAD_4 = 48u,
    ERUI_KEYBOARD_KEY_NUMPAD_5 = 49u,
    ERUI_KEYBOARD_KEY_NUMPAD_6 = 50u,
    ERUI_KEYBOARD_KEY_NUMPAD_1 = 51u,
    ERUI_KEYBOARD_KEY_NUMPAD_2 = 52u,
    ERUI_KEYBOARD_KEY_NUMPAD_3 = 53u,
    ERUI_KEYBOARD_KEY_NUMPAD_0 = 54u,
    ERUI_KEYBOARD_KEY_NUMPAD_ENTER = 55u,
    ERUI_KEYBOARD_KEY_RIGHT_CONTROL = 56u,
    ERUI_KEYBOARD_KEY_RIGHT_ALT = 57u,
    ERUI_KEYBOARD_KEY_HOME = 58u,
    ERUI_KEYBOARD_KEY_ARROW_UP = 59u,
    ERUI_KEYBOARD_KEY_PAGE_UP = 60u,
    ERUI_KEYBOARD_KEY_ARROW_LEFT = 61u,
    ERUI_KEYBOARD_KEY_ARROW_RIGHT = 62u,
    ERUI_KEYBOARD_KEY_END = 63u,
    ERUI_KEYBOARD_KEY_ARROW_DOWN = 64u,
    ERUI_KEYBOARD_KEY_PAGE_DOWN = 65u,
    ERUI_KEYBOARD_KEY_INSERT = 66u,
    ERUI_KEYBOARD_KEY_DELETE = 67u,
    ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY = 68u,
    ERUI_KEYBOARD_KEY_NUMPAD_SUBTRACT = 69u,
    ERUI_KEYBOARD_KEY_NUMPAD_ADD = 70u,
    ERUI_KEYBOARD_KEY_NUMPAD_DECIMAL = 71u,
    ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE = 72u,
    ERUI_KEYBOARD_KEY_COUNT = 73u
};

typedef uint32_t ERUI_MouseButton;
enum {
    ERUI_MOUSE_BUTTON_INVALID = 0u,
    ERUI_MOUSE_BUTTON_LEFT = 1u,
    ERUI_MOUSE_BUTTON_RIGHT = 2u,
    ERUI_MOUSE_BUTTON_MIDDLE = 3u,
    ERUI_MOUSE_BUTTON_4 = 4u,
    ERUI_MOUSE_BUTTON_5 = 5u,
    ERUI_MOUSE_BUTTON_WHEEL_UP = 6u,
    ERUI_MOUSE_BUTTON_WHEEL_DOWN = 7u,
    ERUI_MOUSE_BUTTON_COUNT = 8u
};

typedef uint32_t ERUI_InputSlotState;
enum {
    ERUI_INPUT_SLOT_ABSENT = 0u,
    ERUI_INPUT_SLOT_UNBOUND = 1u,
    ERUI_INPUT_SLOT_BOUND = 2u
};

typedef struct ERUI_ControllerInput {
    ERUI_InputSlotState state;
    ERUI_ControllerButton input;
} ERUI_ControllerInput;

typedef struct ERUI_KeyboardInput {
    ERUI_InputSlotState state;
    ERUI_KeyboardKey input;
} ERUI_KeyboardInput;

typedef struct ERUI_MouseInput {
    ERUI_InputSlotState state;
    ERUI_MouseButton input;
} ERUI_MouseInput;

/*
 * One logical action has at most one alternative for each device family.
 * This is a fixed-layout value embedded in other API 1.1 structures; size
 * must equal sizeof(ERUI_ActionInputs), not describe an extensible prefix.
 * ABSENT means unsupported in defaults and complete snapshots, but means
 * "leave unchanged" when passed to set_action_inputs.
 */
typedef struct ERUI_ActionInputs {
    uint32_t size;
    uint32_t flags;
    ERUI_ControllerInput controller;
    ERUI_KeyboardInput keyboard;
    ERUI_MouseInput mouse;
    uint32_t reserved[2];
} ERUI_ActionInputs;

/* Longest canonical ActionInputs text payload, excluding a terminator. */
#define ERUI_ACTION_INPUTS_TEXT_MAX_BYTES 67u

/*
 * TextInput lengths are counts of UTF-16 code units, matching
 * ERUI_Utf16View.length. They are not byte counts or Unicode grapheme counts.
 * The API 1.1-and-later contract accepts maximum_length values in 1..35; zero
 * selects ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH.
 */
#define ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH 16u
#define ERUI_TEXT_INPUT_MAX_LENGTH 35u

/*
 * Numeric and custom sliders are index-backed native sliders. Their logical
 * position count is therefore bounded independently of the values displayed
 * to the player. Numeric decimal places describe an exact base-10 scale.
 */
#define ERUI_SLIDER_MIN_VALUE_COUNT 2u
#define ERUI_SLIDER_MAX_VALUE_COUNT 256u
#define ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES 9u
#define ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH 64u

/* Counted byte view. Each field documents its own encoding and grammar. */
typedef struct ERUI_StringView {
    const char* data;
    uint32_t length;
    uint32_t reserved;
} ERUI_StringView;

/*
 * Header-only ActionInputs text codec.
 *
 * These helpers do not load or call ERNativeUI.dll. They provide the same
 * stable, human-readable representation used by ERNativeUI provider storage,
 * so a client may persist bindings with its own storage implementation.
 *
 * Canonical output lists present devices in controller, keyboard, mouse
 * order. ABSENT slots are omitted and UNBOUND slots use the literal
 * "unbound". Output is counted ASCII/UTF-8 and is never NUL-terminated.
 * Call ERUI_FormatActionInputs with output == NULL and output_capacity == 0
 * to query the required payload length. out_length is mandatory. A short
 * buffer returns ERUI_BUFFER_TOO_SMALL and the required length; other
 * argument failures set it to zero.
 *
 * ERUI_ParseActionInputs accepts device fields in any order, but rejects
 * duplicates, whitespace, unknown names, and malformed text. An empty view
 * produces an ActionInputs value with all three slots ABSENT. Malformed text
 * returns ERUI_STORAGE_FORMAT_ERROR. The caller need not initialize
 * out_inputs; it is completely initialized on success. Output ActionInputs
 * values and character buffers remain unchanged whenever an operation fails.
 */

static inline int ERUI_DetailActionInputTextEquals(
    const char* data,
    uint32_t length,
    const char* expected) {
    uint32_t index = 0u;
    while (expected[index] != '\0') {
        if (index >= length || data[index] != expected[index]) return 0;
        ++index;
    }
    return index == length;
}

static inline uint32_t ERUI_DetailActionInputTextLength(const char* text) {
    uint32_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

static inline const char* ERUI_DetailControllerButtonText(
    ERUI_ControllerButton input) {
    static const char* const names[ERUI_CONTROLLER_BUTTON_COUNT] = {
        "", "dpad-up", "dpad-down", "dpad-left", "dpad-right",
        "face-south", "face-east", "face-west", "face-north",
        "left-shoulder", "right-shoulder", "left-trigger",
        "right-trigger", "left-stick", "right-stick"
    };
    return input > ERUI_CONTROLLER_BUTTON_INVALID &&
            input < ERUI_CONTROLLER_BUTTON_COUNT
        ? names[input]
        : (const char*)0;
}

static inline const char* ERUI_DetailKeyboardKeyText(ERUI_KeyboardKey input) {
    static const char* const names[ERUI_KEYBOARD_KEY_COUNT] = {
        "",
        "digit-1", "digit-2", "digit-3", "digit-4", "digit-5",
        "digit-6", "digit-7", "digit-8", "digit-9", "digit-0",
        "backspace", "tab",
        "key-q", "key-w", "key-e", "key-r", "key-t", "key-y",
        "key-u", "key-i", "key-o", "key-p",
        "enter", "left-control",
        "key-a", "key-s", "key-d", "key-f", "key-g", "key-h",
        "key-j", "key-k", "key-l", "left-shift",
        "key-z", "key-x", "key-c", "key-v", "key-b", "key-n",
        "key-m", "right-shift", "left-alt", "space",
        "numpad-7", "numpad-8", "numpad-9", "numpad-4", "numpad-5",
        "numpad-6", "numpad-1", "numpad-2", "numpad-3", "numpad-0",
        "numpad-enter", "right-control", "right-alt",
        "home", "arrow-up", "page-up", "arrow-left", "arrow-right",
        "end", "arrow-down", "page-down", "insert", "delete",
        "numpad-multiply", "numpad-subtract", "numpad-add",
        "numpad-decimal", "numpad-divide"
    };
    return input > ERUI_KEYBOARD_KEY_INVALID &&
            input < ERUI_KEYBOARD_KEY_COUNT
        ? names[input]
        : (const char*)0;
}

static inline const char* ERUI_DetailMouseButtonText(ERUI_MouseButton input) {
    static const char* const names[ERUI_MOUSE_BUTTON_COUNT] = {
        "", "left", "right", "middle", "button4", "button5",
        "wheel-up", "wheel-down"
    };
    return input > ERUI_MOUSE_BUTTON_INVALID &&
            input < ERUI_MOUSE_BUTTON_COUNT
        ? names[input]
        : (const char*)0;
}

static inline int ERUI_DetailValidActionInputSlot(
    ERUI_InputSlotState state,
    uint32_t input,
    uint32_t count) {
    if (state == ERUI_INPUT_SLOT_ABSENT ||
        state == ERUI_INPUT_SLOT_UNBOUND) {
        return input == 0u;
    }
    return state == ERUI_INPUT_SLOT_BOUND && input != 0u && input < count;
}

static inline int ERUI_DetailValidActionInputs(
    const ERUI_ActionInputs* inputs) {
    return inputs != (const ERUI_ActionInputs*)0 &&
        inputs->size == (uint32_t)sizeof(ERUI_ActionInputs) &&
        inputs->flags == 0u &&
        inputs->reserved[0] == 0u && inputs->reserved[1] == 0u &&
        ERUI_DetailValidActionInputSlot(
            inputs->controller.state,
            inputs->controller.input,
            ERUI_CONTROLLER_BUTTON_COUNT) &&
        ERUI_DetailValidActionInputSlot(
            inputs->keyboard.state,
            inputs->keyboard.input,
            ERUI_KEYBOARD_KEY_COUNT) &&
        ERUI_DetailValidActionInputSlot(
            inputs->mouse.state,
            inputs->mouse.input,
            ERUI_MOUSE_BUTTON_COUNT);
}

static inline void ERUI_DetailCopyActionInputText(
    char* output,
    uint32_t* cursor,
    const char* text) {
    uint32_t index = 0u;
    while (text[index] != '\0') {
        output[*cursor] = text[index];
        ++(*cursor);
        ++index;
    }
}

static inline ERUI_Result ERUI_CALL ERUI_FormatActionInputs(
    const ERUI_ActionInputs* inputs,
    char* output,
    uint32_t output_capacity,
    uint32_t* out_length) {
    const char* devices[3];
    const char* assignments[3];
    uint32_t field_count = 0u;
    uint32_t required = 0u;
    uint32_t index;
    uint32_t cursor = 0u;

    if (out_length == (uint32_t*)0) return ERUI_INVALID_ARGUMENT;
    *out_length = 0u;
    if (!ERUI_DetailValidActionInputs(inputs) ||
        (output == (char*)0 && output_capacity != 0u)) {
        return ERUI_INVALID_ARGUMENT;
    }

    if (inputs->controller.state != ERUI_INPUT_SLOT_ABSENT) {
        devices[field_count] = "controller";
        assignments[field_count] =
            inputs->controller.state == ERUI_INPUT_SLOT_UNBOUND
            ? "unbound"
            : ERUI_DetailControllerButtonText(inputs->controller.input);
        ++field_count;
    }
    if (inputs->keyboard.state != ERUI_INPUT_SLOT_ABSENT) {
        devices[field_count] = "keyboard";
        assignments[field_count] =
            inputs->keyboard.state == ERUI_INPUT_SLOT_UNBOUND
            ? "unbound"
            : ERUI_DetailKeyboardKeyText(inputs->keyboard.input);
        ++field_count;
    }
    if (inputs->mouse.state != ERUI_INPUT_SLOT_ABSENT) {
        devices[field_count] = "mouse";
        assignments[field_count] =
            inputs->mouse.state == ERUI_INPUT_SLOT_UNBOUND
            ? "unbound"
            : ERUI_DetailMouseButtonText(inputs->mouse.input);
        ++field_count;
    }

    for (index = 0u; index < field_count; ++index) {
        if (index != 0u) ++required;
        required += ERUI_DetailActionInputTextLength(devices[index]);
        ++required;
        required += ERUI_DetailActionInputTextLength(assignments[index]);
    }
    *out_length = required;

    if (output == (char*)0 && output_capacity == 0u) return ERUI_OK;
    if (output_capacity < required) return ERUI_BUFFER_TOO_SMALL;

    for (index = 0u; index < field_count; ++index) {
        if (index != 0u) output[cursor++] = ',';
        ERUI_DetailCopyActionInputText(output, &cursor, devices[index]);
        output[cursor++] = ':';
        ERUI_DetailCopyActionInputText(output, &cursor, assignments[index]);
    }
    return ERUI_OK;
}

static inline int ERUI_DetailParseControllerButton(
    const char* text,
    uint32_t length,
    ERUI_ControllerInput* output) {
    ERUI_ControllerButton input;
    if (ERUI_DetailActionInputTextEquals(text, length, "unbound")) {
        output->state = ERUI_INPUT_SLOT_UNBOUND;
        output->input = ERUI_CONTROLLER_BUTTON_INVALID;
        return 1;
    }
    for (input = 1u; input < ERUI_CONTROLLER_BUTTON_COUNT; ++input) {
        if (ERUI_DetailActionInputTextEquals(
                text, length, ERUI_DetailControllerButtonText(input))) {
            output->state = ERUI_INPUT_SLOT_BOUND;
            output->input = input;
            return 1;
        }
    }
    return 0;
}

static inline int ERUI_DetailParseKeyboardKey(
    const char* text,
    uint32_t length,
    ERUI_KeyboardInput* output) {
    ERUI_KeyboardKey input;
    if (ERUI_DetailActionInputTextEquals(text, length, "unbound")) {
        output->state = ERUI_INPUT_SLOT_UNBOUND;
        output->input = ERUI_KEYBOARD_KEY_INVALID;
        return 1;
    }
    for (input = 1u; input < ERUI_KEYBOARD_KEY_COUNT; ++input) {
        if (ERUI_DetailActionInputTextEquals(
                text, length, ERUI_DetailKeyboardKeyText(input))) {
            output->state = ERUI_INPUT_SLOT_BOUND;
            output->input = input;
            return 1;
        }
    }
    return 0;
}

static inline int ERUI_DetailParseMouseButton(
    const char* text,
    uint32_t length,
    ERUI_MouseInput* output) {
    ERUI_MouseButton input;
    if (ERUI_DetailActionInputTextEquals(text, length, "unbound")) {
        output->state = ERUI_INPUT_SLOT_UNBOUND;
        output->input = ERUI_MOUSE_BUTTON_INVALID;
        return 1;
    }
    for (input = 1u; input < ERUI_MOUSE_BUTTON_COUNT; ++input) {
        if (ERUI_DetailActionInputTextEquals(
                text, length, ERUI_DetailMouseButtonText(input))) {
            output->state = ERUI_INPUT_SLOT_BOUND;
            output->input = input;
            return 1;
        }
    }
    return 0;
}

static inline ERUI_Result ERUI_CALL ERUI_ParseActionInputs(
    const ERUI_StringView* text,
    ERUI_ActionInputs* out_inputs) {
    ERUI_ActionInputs parsed;
    ERUI_InputDevices seen = ERUI_INPUT_DEVICE_NONE;
    uint32_t cursor = 0u;

    if (text == (const ERUI_StringView*)0 ||
        out_inputs == (ERUI_ActionInputs*)0 ||
        text->reserved != 0u ||
        (text->data == (const char*)0 && text->length != 0u)) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (text->length > ERUI_ACTION_INPUTS_TEXT_MAX_BYTES) {
        return ERUI_STORAGE_FORMAT_ERROR;
    }

    parsed.size = (uint32_t)sizeof(parsed);
    parsed.flags = 0u;
    parsed.controller.state = ERUI_INPUT_SLOT_ABSENT;
    parsed.controller.input = ERUI_CONTROLLER_BUTTON_INVALID;
    parsed.keyboard.state = ERUI_INPUT_SLOT_ABSENT;
    parsed.keyboard.input = ERUI_KEYBOARD_KEY_INVALID;
    parsed.mouse.state = ERUI_INPUT_SLOT_ABSENT;
    parsed.mouse.input = ERUI_MOUSE_BUTTON_INVALID;
    parsed.reserved[0] = 0u;
    parsed.reserved[1] = 0u;
    while (cursor < text->length) {
        uint32_t item_end = cursor;
        uint32_t colon = UINT32_MAX;
        uint32_t index;
        const char* value;
        uint32_t value_length;
        ERUI_InputDevices bit;
        int valid;

        while (item_end < text->length && text->data[item_end] != ',') {
            if (text->data[item_end] == '\0') {
                return ERUI_STORAGE_FORMAT_ERROR;
            }
            ++item_end;
        }
        if (item_end == cursor) return ERUI_STORAGE_FORMAT_ERROR;
        for (index = cursor; index < item_end; ++index) {
            if (text->data[index] == ':') {
                if (colon != UINT32_MAX) return ERUI_STORAGE_FORMAT_ERROR;
                colon = index;
            }
        }
        if (colon == UINT32_MAX || colon == cursor ||
            colon + 1u == item_end) {
            return ERUI_STORAGE_FORMAT_ERROR;
        }

        value = text->data + colon + 1u;
        value_length = item_end - colon - 1u;
        bit = ERUI_INPUT_DEVICE_NONE;
        valid = 0;
        if (ERUI_DetailActionInputTextEquals(
                text->data + cursor, colon - cursor, "controller")) {
            bit = ERUI_INPUT_DEVICE_CONTROLLER;
            valid = ERUI_DetailParseControllerButton(
                value, value_length, &parsed.controller);
        } else if (ERUI_DetailActionInputTextEquals(
                text->data + cursor, colon - cursor, "keyboard")) {
            bit = ERUI_INPUT_DEVICE_KEYBOARD;
            valid = ERUI_DetailParseKeyboardKey(
                value, value_length, &parsed.keyboard);
        } else if (ERUI_DetailActionInputTextEquals(
                text->data + cursor, colon - cursor, "mouse")) {
            bit = ERUI_INPUT_DEVICE_MOUSE;
            valid = ERUI_DetailParseMouseButton(
                value, value_length, &parsed.mouse);
        }
        if (!valid || bit == ERUI_INPUT_DEVICE_NONE || (seen & bit) != 0u) {
            return ERUI_STORAGE_FORMAT_ERROR;
        }
        seen |= bit;

        if (item_end == text->length) break;
        cursor = item_end + 1u;
        if (cursor == text->length) return ERUI_STORAGE_FORMAT_ERROR;
    }

    if (!ERUI_DetailValidActionInputs(&parsed)) {
        return ERUI_STORAGE_FORMAT_ERROR;
    }
    *out_inputs = parsed;
    return ERUI_OK;
}

/* User-facing text is UTF-16 and does not depend on compiler wchar_t mode. */
typedef struct ERUI_Utf16View {
    const uint16_t* data;
    uint32_t length;
    uint32_t reserved;
} ERUI_Utf16View;

/*
 * Opaque RGB color shared by color pickers and styled text. reserved must be
 * zero. The explicit channels keep Elden Ring's private packed-color
 * representation out of the public ABI.
 */
typedef struct ERUI_Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t reserved;
} ERUI_Color;

/*
 * Structured text formatting used by API 1.4 updates. Offsets and lengths are
 * measured in UTF-16 code units and must not divide a surrogate pair. Spans
 * must be non-empty, ordered, non-overlapping, and contained by text. Plain
 * text is represented by a view with span_count == 0.
 *
 * Bold and italic request matching font faces from the effective Scaleform
 * font configuration. They are intentionally preserved in the ABI even when
 * the active game font does not provide a visibly different face.
 */
#define ERUI_STYLED_TEXT_MAX_CODE_UNITS 4096u
#define ERUI_STYLED_TEXT_MAX_SPANS 256u

typedef uint32_t ERUI_TextStyleFlags;
enum {
    ERUI_TEXT_STYLE_CUSTOM_COLOR = 1u << 0,
    ERUI_TEXT_STYLE_BOLD = 1u << 1,
    ERUI_TEXT_STYLE_ITALIC = 1u << 2,
    ERUI_TEXT_STYLE_UNDERLINE = 1u << 3
};

typedef struct ERUI_TextStyleSpan {
    uint32_t size;
    ERUI_TextStyleFlags flags;
    uint32_t offset;
    uint32_t length;
    ERUI_Color color;
    uint32_t reserved[3];
} ERUI_TextStyleSpan;

/*
 * spans points to span_count records separated by span_stride bytes. API 1.4
 * requires span_stride == sizeof(ERUI_TextStyleSpan), exact descriptor sizes,
 * zero view flags/reserved fields, and at most ERUI_STYLED_TEXT_MAX_SPANS.
 * The host copies the text and spans before the API call returns.
 */
typedef struct ERUI_StyledTextView {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View text;
    const ERUI_TextStyleSpan* spans;
    uint32_t span_count;
    uint32_t span_stride;
    uint32_t reserved[2];
} ERUI_StyledTextView;

/*
 * API 1.4 update envelope. A single commit validates and copies the complete
 * batch before publishing it as one logical UI generation. No partial update
 * is published when validation or allocation fails.
 *
 * updates points to update_count records separated by update_stride bytes.
 * API 1.4 requires update_stride == sizeof(ERUI_UpdateDesc), exact descriptor
 * sizes, zero flags/reserved fields, and 1..ERUI_UPDATE_MAX_COUNT items.
 */
#define ERUI_UPDATE_MAX_COUNT 4096u

typedef uint32_t ERUI_UpdateKind;
enum {
    ERUI_UPDATE_KIND_ROW = 1u
};

typedef uint32_t ERUI_RowUpdateFields;
enum {
    ERUI_ROW_UPDATE_FIELD_LABEL = 1u << 0,
    ERUI_ROW_UPDATE_FIELD_HELP = 1u << 1,
    /*
     * Overrides the right-hand text inside Button, Popup Choice, and Submenu
     * controls. The caption must be non-empty. Popup Choice uses this same
     * field for its selected value, so an override deliberately replaces that
     * visible value while leaving the logical selection unchanged. Other row
     * kinds reject caption updates.
     */
    ERUI_ROW_UPDATE_FIELD_CAPTION = 1u << 2,
    /* Changes whether the row can receive focus or activation. */
    ERUI_ROW_UPDATE_FIELD_INTERACTIVE = 1u << 3,
    /* Selects the row's visual availability independently of interaction. */
    ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE = 1u << 4
};

/*
 * Runtime availability appearance. FOLLOW_INTERACTIVITY uses the disabled
 * presentation when interactive is false and the normal presentation when it
 * is true. NORMAL and DISABLED deliberately decouple appearance from input.
 * API 1.4 supports these update fields for Button, Toggle, byte Slider,
 * NumericSlider, CustomSlider, InlineChoice, PopupChoice, TextInput, and
 * ColorPicker rows. Submenu rows are not supported.
 */
typedef uint32_t ERUI_AvailabilityAppearance;
enum {
    ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY = 0u,
    ERUI_AVAILABILITY_APPEARANCE_NORMAL = 1u,
    ERUI_AVAILABILITY_APPEARANCE_DISABLED = 2u
};

typedef struct ERUI_RowUpdateDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_RowHandle row;
    ERUI_RowUpdateFields fields;
    uint32_t interactive;
    ERUI_StyledTextView label;
    ERUI_StyledTextView help;
    ERUI_StyledTextView caption;
    ERUI_AvailabilityAppearance availability_appearance;
    uint32_t reserved;
} ERUI_RowUpdateDesc;

/*
 * When its field bit is present, interactive must be exactly 0 or 1 and
 * availability_appearance must name a declared value. When the corresponding
 * bit is absent, each scalar must be zero. Availability fields additionally
 * require ERUI_CAP_ROW_AVAILABILITY and a materialized Button, Toggle,
 * Slider, NumericSlider, CustomSlider, InlineChoice, PopupChoice, TextInput,
 * or ColorPicker row. Submenu rows are unsupported; a construction-disabled
 * Button or ColorPicker is omitted by the host and therefore also rejected.
 */

typedef struct ERUI_UpdateDesc {
    uint32_t size;
    ERUI_UpdateKind kind;
    const void* payload;
    uint32_t reserved[2];
} ERUI_UpdateDesc;

typedef struct ERUI_UpdateBatchDesc {
    uint32_t size;
    uint32_t flags;
    const ERUI_UpdateDesc* updates;
    uint32_t update_count;
    uint32_t update_stride;
    uint32_t reserved[2];
} ERUI_UpdateBatchDesc;

/*
 * Convenience classification for the effective language identifier. In
 * automatic mode the identifier comes from Steam; a host configuration may
 * explicitly override it. UNKNOWN means a valid identifier is not known to
 * this host; it does not mean that language selection failed.
 */
typedef uint32_t ERUI_GameLanguage;
enum {
    ERUI_GAME_LANGUAGE_UNKNOWN = 0u,
    ERUI_GAME_LANGUAGE_ENGLISH = 1u,
    ERUI_GAME_LANGUAGE_GERMAN = 2u,
    ERUI_GAME_LANGUAGE_FRENCH = 3u,
    ERUI_GAME_LANGUAGE_ITALIAN = 4u,
    ERUI_GAME_LANGUAGE_KOREAN = 5u,
    ERUI_GAME_LANGUAGE_SPANISH = 6u,
    ERUI_GAME_LANGUAGE_CHINESE_SIMPLIFIED = 7u,
    ERUI_GAME_LANGUAGE_CHINESE_TRADITIONAL = 8u,
    ERUI_GAME_LANGUAGE_RUSSIAN = 9u,
    ERUI_GAME_LANGUAGE_THAI = 10u,
    ERUI_GAME_LANGUAGE_JAPANESE = 11u,
    ERUI_GAME_LANGUAGE_POLISH = 12u,
    ERUI_GAME_LANGUAGE_ARABIC = 13u,
    ERUI_GAME_LANGUAGE_PORTUGUESE_BRAZIL = 14u,
    ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA = 15u
};

/*
 * identifier is the exact effective UTF-8 token (for example "french",
 * "koreana", or an identifier unknown to ERNativeUI). It normally comes from
 * Steam and may instead be selected by the host's localization setting. Its
 * memory is host-owned and remains valid until process exit. The caller
 * initializes size before calling get_game_language.
 */
typedef struct ERUI_GameLanguageInfo {
    uint32_t size;
    ERUI_GameLanguage known_language;
    ERUI_StringView identifier;
    uint32_t reserved[2];
} ERUI_GameLanguageInfo;

/*
 * Page-title formatters write into host-owned storage. output_capacity and
 * out_length are measured in UTF-16 code units; no terminator is required.
 * Returning anything other than ERUI_OK, producing an empty/invalid string,
 * or reporting a length greater than output_capacity selects the host's safe
 * default title instead. All pointers in the context and the output buffer are
 * valid only for the duration of the callback.
 */
#define ERUI_PAGE_TITLE_BUFFER_CAPACITY 4096u

typedef struct ERUI_PageTitleFormatContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle page;
    ERUI_Utf16View base_title;
    uint32_t page_number;
    uint32_t page_count;
    uint32_t reserved[2];
} ERUI_PageTitleFormatContext;

typedef ERUI_Result (ERUI_CALL* ERUI_PageTitleFormatter)(
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    uint16_t* output,
    uint32_t output_capacity,
    uint32_t* out_length);

typedef void (ERUI_CALL* ERUI_ButtonCallback)(void* user_data);
typedef void (ERUI_CALL* ERUI_ValueChangedCallback)(
    void* user_data,
    uint8_t value);

/*
 * Borrowed callback data; the context and value expire when the call ends.
 * The host calls changed_callback once after the player confirms a value that
 * differs from the canonical value. Cancel, identical confirmation, and
 * set_text_input_value do not invoke it.
 */
typedef struct ERUI_TextInputChangeContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_RowHandle row;
    ERUI_Utf16View value;
    uint32_t reserved[2];
} ERUI_TextInputChangeContext;

typedef void (ERUI_CALL* ERUI_TextInputChangedCallback)(
    void* user_data,
    const ERUI_TextInputChangeContext* context);

/*
 * Borrowed callback data that expires when the call ends. The host calls
 * changed_callback once after the player confirms a color that differs from
 * the canonical value. Cancel, identical confirmation, and
 * set_color_picker_value do not invoke it.
 */
typedef struct ERUI_ColorPickerChangeContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_RowHandle row;
    ERUI_Color value;
    uint32_t reserved;
} ERUI_ColorPickerChangeContext;

typedef void (ERUI_CALL* ERUI_ColorPickerChangedCallback)(
    void* user_data,
    const ERUI_ColorPickerChangeContext* context);

/*
 * Exact base-10 value used by API 1.2 numeric sliders. The represented value
 * is scaled_value / (10 ^ decimal_places). reserved must be zero.
 */
typedef struct ERUI_DecimalValue {
    int64_t scaled_value;
    uint32_t decimal_places;
    uint32_t reserved;
} ERUI_DecimalValue;

/*
 * Borrowed callback data that expires when the call ends. index is the
 * zero-based logical position, not Elden Ring's private backing byte.
 */
typedef struct ERUI_NumericSliderChangeContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_RowHandle row;
    ERUI_DecimalValue value;
    uint32_t index;
    uint32_t value_count;
    uint32_t reserved[2];
} ERUI_NumericSliderChangeContext;

typedef void (ERUI_CALL* ERUI_NumericSliderChangedCallback)(
    void* user_data,
    const ERUI_NumericSliderChangeContext* context);

/*
 * Borrowed callback data for an arbitrary index-to-text slider. The host has
 * already copied every display value, so no client formatter runs here.
 */
typedef struct ERUI_CustomSliderChangeContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_RowHandle row;
    uint32_t index;
    uint32_t value_count;
    uint32_t reserved[2];
} ERUI_CustomSliderChangeContext;

typedef void (ERUI_CALL* ERUI_CustomSliderChangedCallback)(
    void* user_data,
    const ERUI_CustomSliderChangeContext* context);

/*
 * Borrowed callback data that expires when the call ends. One logical action
 * owns controller, keyboard, and mouse alternatives. When alternatives become
 * active in the same sampled frame, devices contains their combined bit mask.
 * Callbacks are serialized on the host worker after a released-to-pressed
 * edge; holding an input does not repeat. ERNativeUI does not consume the
 * underlying game input, and every registered binding sharing it may fire.
 */
typedef struct ERUI_InputActionActivatedContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_InputActionHandle action;
    ERUI_InputDevices devices;
    uint32_t reserved;
} ERUI_InputActionActivatedContext;

typedef void (ERUI_CALL* ERUI_InputActionActivatedCallback)(
    void* user_data,
    const ERUI_InputActionActivatedContext* context);

typedef uint32_t ERUI_AssignmentChangeReason;
enum {
    ERUI_ASSIGNMENT_CHANGE_UNKNOWN = 0u,
    ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT = 1u,
    ERUI_ASSIGNMENT_CHANGE_PLAYER_CLEAR = 2u,
    ERUI_ASSIGNMENT_CHANGE_RESET_TO_DEFAULTS = 3u
};

/*
 * Each element and both complete snapshots are borrowed for the duration of
 * the enclosing callback. changed_devices may contain several device bits.
 */
typedef struct ERUI_AssignmentChange {
    uint32_t size;
    uint32_t flags;
    ERUI_InputActionHandle action;
    ERUI_StringView action_id;
    ERUI_ActionInputs previous;
    ERUI_ActionInputs current;
    ERUI_AssignmentChangeReason reason;
    ERUI_InputDevices changed_devices;
    uint32_t reserved[2];
} ERUI_AssignmentChange;

typedef struct ERUI_AssignmentsChangedContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    const ERUI_AssignmentChange* changes;
    uint32_t change_count;
    uint32_t reserved;
} ERUI_AssignmentsChangedContext;

typedef void (ERUI_CALL* ERUI_AssignmentsChangedCallback)(
    void* user_data,
    const ERUI_AssignmentsChangedContext* context);

typedef struct ERUI_AssignmentsChangedHandlerDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_AssignmentsChangedCallback callback;
    void* user_data;
    uint32_t reserved[2];
} ERUI_AssignmentsChangedHandlerDesc;

/* Alert button layouts are closed choices, not combinable bit flags. */
typedef uint32_t ERUI_AlertButtons;
enum {
    ERUI_ALERT_BUTTONS_OK = 0u,
    ERUI_ALERT_BUTTONS_CANCEL = 1u,
    ERUI_ALERT_BUTTONS_YES = 2u,
    ERUI_ALERT_BUTTONS_NO = 3u,
    ERUI_ALERT_BUTTONS_OK_CANCEL = 4u,
    ERUI_ALERT_BUTTONS_YES_NO = 5u,
    ERUI_ALERT_BUTTONS_DISMISS_ONLY = 6u
};

typedef uint32_t ERUI_AlertPlacement;
enum {
    ERUI_ALERT_PLACEMENT_BOTTOM = 0u,
    ERUI_ALERT_PLACEMENT_CENTER = 1u
};

typedef uint32_t ERUI_AlertResponse;
enum {
    ERUI_ALERT_RESPONSE_NONE = 0u,
    ERUI_ALERT_RESPONSE_PRIMARY = 1u,
    ERUI_ALERT_RESPONSE_SECONDARY = 2u,
    ERUI_ALERT_RESPONSE_DISMISSED = 3u
};

/*
 * Alert completion callbacks are asynchronous and never run inside
 * enqueue_alert. ERUI_OK means the dialog completed normally and response
 * identifies how it completed. For another result, response is NONE because
 * the accepted request could not be presented or completed normally.
 */
typedef void (ERUI_CALL* ERUI_AlertCallback)(
    void* user_data,
    ERUI_Result completion_result,
    ERUI_AlertResponse response);

/*
 * provider_id is a stable, case-sensitive ASCII machine identifier containing
 * 1..255 letters, digits, '.', '_', or '-'. The host copies provider_id and
 * display_name before register_provider returns. provider_id must not be
 * localized or changed between ordinary releases of the client mod, and it
 * must be distinct among providers loaded in the process.
 */
typedef struct ERUI_ProviderDesc {
    uint32_t size;
    uint32_t api_version;
    uint32_t flags;
    int32_t root_priority;
    void* owner_module;
    ERUI_StringView provider_id;
    ERUI_Utf16View display_name;
} ERUI_ProviderDesc;

/*
 * API 1.3 client-owned Configuration tab. tab_id is a stable,
 * case-sensitive provider-local ASCII identifier using the same 1..255
 * character grammar as provider_id. The process-wide identity used by logs
 * and player ordering is "provider_id/tab_id"; '/' is therefore not accepted
 * in either component.
 *
 * title must contain 1..ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS UTF-16 code
 * units. preferred_order is a sorting hint, not a physical slot number: lower
 * values appear first, with stable provider_id/tab_id tie-breaking. A player's
 * ERNativeUI.ini ordering may override it. icon may be NULL to request the
 * host fallback glyph. Otherwise the descriptor and all referenced pixel
 * bytes are copied synchronously before add_tab returns.
 *
 * The returned page is an ordinary provider-owned page. It accepts the same
 * add_* row and presentation functions as built-in and submenu pages and
 * remains valid until the provider registration is aborted or the process
 * ends. tab_id must be unique only within its provider.
 */
typedef struct ERUI_TabDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_StringView tab_id;
    ERUI_Utf16View title;
    int32_t preferred_order;
    uint32_t reserved0;
    const ERUI_TabIconDesc* icon;
    uint32_t reserved[2];
} ERUI_TabDesc;

/*
 * API 1.3 title-screen text. Each successful call registers one line for the
 * provider in one explicitly selected screen corner. Within each corner,
 * calls made by one provider retain their call order; provider blocks are
 * ordered by ascending root_priority, then provider_id, then the provider
 * registration ordinal. The host copies text before add_title_screen_text
 * returns. Text must be a single non-empty line:
 * CR, LF, U+0085, U+2028, and U+2029 are rejected. A line may contain at most
 * ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS UTF-16 code units. At most
 * ERUI_TITLE_SCREEN_TEXT_MAX_LINES committed lines may exist process-wide. An
 * add or commit that would exceed that limit returns
 * ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED; abort the uncommitted draft.
 *
 * With ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR set, color supplies the line's
 * opaque RGB color. Without it, the line inherits the native title-screen
 * color and color's RGB channels are ignored. corner is mandatory and must
 * be one of the four ERUI_TITLE_SCREEN_CORNER_* values; zero is deliberately
 * invalid. color.reserved and every descriptor reserved field must be zero.
 */
typedef uint32_t ERUI_TitleScreenTextFlags;
enum {
    ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR = 1u << 0
};

/* Closed choices, not combinable bit flags. Zero is intentionally invalid. */
typedef uint32_t ERUI_TitleScreenCorner;
enum {
    ERUI_TITLE_SCREEN_CORNER_TOP_LEFT = 1u,
    ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT = 2u,
    ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT = 3u,
    ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT = 4u
};

typedef struct ERUI_TitleScreenTextDesc {
    uint32_t size;
    ERUI_TitleScreenTextFlags flags;
    ERUI_Utf16View text;
    ERUI_Color color;
    ERUI_TitleScreenCorner corner;
    uint32_t reserved[2];
} ERUI_TitleScreenTextDesc;

/*
 * The host copies label before add_input_section returns. Sections are
 * presentation groups shared by the native controller and keyboard/mouse
 * binding screens; their returned handles are valid only for this provider.
 */
typedef struct ERUI_InputSectionDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    uint32_t reserved[2];
} ERUI_InputSectionDesc;

/*
 * action_id is a stable, provider-local ASCII machine identifier made from
 * letters, digits, '.', '_', and '-'; it must not be localized. It is unique
 * across all sections owned by the provider and is used directly as a safe,
 * readable storage key when persisting the player's assignments. The host
 * copies both views and default_inputs before add_input_action returns.
 * activated_callback is required; it and user_data must remain valid for the
 * rest of the process after commit. At least one default-input slot must be
 * present. Present-but-unbound slots are supported without a default binding.
 */
typedef struct ERUI_InputActionDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_StringView action_id;
    ERUI_Utf16View label;
    ERUI_ActionInputs default_inputs;
    ERUI_InputActionActivatedCallback activated_callback;
    void* user_data;
    uint32_t reserved[2];
} ERUI_InputActionDesc;

typedef struct ERUI_ButtonDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ButtonCallback callback;
    void* user_data;
    uint32_t enabled;
    uint32_t reserved;
} ERUI_ButtonDesc;

typedef struct ERUI_ToggleDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ValueChangedCallback changed_callback;
    void* user_data;
    uint8_t initial_value;
    uint8_t reserved8[3];
    uint32_t enabled;
} ERUI_ToggleDesc;

/*
 * Slider values are bytes in the inclusive [minimum, maximum] range. Steps
 * are measured from minimum. An off-step initial_value is rounded down to the
 * preceding step before publication and does not invoke changed_callback.
 */
typedef struct ERUI_SliderDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ValueChangedCallback changed_callback;
    void* user_data;
    int32_t minimum;
    int32_t maximum;
    int32_t step;
    uint8_t initial_value;
    uint8_t reserved8[3];
    uint32_t enabled;
} ERUI_SliderDesc;

/*
 * API 1.2 exact fixed-point slider. All four scaled fields use the shared
 * decimal_places scale. The inclusive range must contain 2..256 positions,
 * and maximum_scaled_value must be reachable from minimum_scaled_value by
 * repeatedly adding step_scaled_value. Prefix and suffix are copied before
 * add_numeric_slider returns. An off-step initial value is rounded down to
 * the preceding position; it must still be inside the inclusive range.
 */
typedef struct ERUI_NumericSliderDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_NumericSliderChangedCallback changed_callback;
    void* user_data;
    int64_t minimum_scaled_value;
    int64_t maximum_scaled_value;
    int64_t step_scaled_value;
    int64_t initial_scaled_value;
    uint32_t decimal_places;
    uint32_t enabled;
    ERUI_Utf16View prefix;
    ERUI_Utf16View suffix;
    uint32_t reserved[2];
} ERUI_NumericSliderDesc;

/*
 * API 1.2 arbitrary index-to-text slider. The host copies label, help, and
 * every values element before add_custom_slider returns. value_count must be
 * in ERUI_SLIDER_MIN_VALUE_COUNT..ERUI_SLIDER_MAX_VALUE_COUNT; every value
 * must contain 1..ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH UTF-16 code units.
 */
typedef struct ERUI_CustomSliderDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_CustomSliderChangedCallback changed_callback;
    void* user_data;
    const ERUI_Utf16View* values;
    uint32_t value_count;
    uint32_t initial_index;
    uint32_t enabled;
    uint32_t reserved;
} ERUI_CustomSliderDesc;

typedef struct ERUI_ChoiceDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ValueChangedCallback changed_callback;
    void* user_data;
    const ERUI_Utf16View* options;
    uint32_t option_count;
    uint8_t initial_index;
    uint8_t reserved8[3];
} ERUI_ChoiceDesc;

/*
 * The host copies all four views before add_text_input returns. The editable
 * initial_value must contain no more than maximum_length UTF-16 code units.
 * maximum_length is inclusive. Zero selects
 * ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH; explicit values must be in
 * 1..ERUI_TEXT_INPUT_MAX_LENGTH.
 * changed_callback is optional; user_data must be null when it is null.
 */
typedef struct ERUI_TextInputDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_Utf16View initial_value;
    ERUI_Utf16View placeholder;
    ERUI_TextInputChangedCallback changed_callback;
    void* user_data;
    uint32_t maximum_length;
    uint32_t reserved;
} ERUI_TextInputDesc;

/*
 * The host copies label, help, and initial_value before add_color_picker
 * returns. changed_callback is optional; user_data must be null when it is
 * null. initial_value.reserved must be zero.
 */
typedef struct ERUI_ColorPickerDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ColorPickerChangedCallback changed_callback;
    void* user_data;
    ERUI_Color initial_value;
    uint32_t enabled;
} ERUI_ColorPickerDesc;

typedef struct ERUI_SubmenuDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_Utf16View page_title;
    ERUI_Utf16View page_help;
    uint32_t enabled;
    uint32_t reserved;
} ERUI_SubmenuDesc;

/*
 * menu_title changes the native outer heading for a provider-owned submenu.
 * page_title changes the base title for its physical slices. Empty views keep
 * the corresponding host default. formatter is optional and, when present,
 * receives that effective base title once per physical slice during menu
 * compilation. The host copies every view and every successful formatter
 * result; it never assumes ownership of client memory. formatter and
 * user_data must remain valid until commit_provider returns. For a custom-tab
 * page, a non-empty page_title replaces the tab-strip title and is therefore
 * limited to ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS UTF-16 code units.
 */
typedef struct ERUI_PagePresentationDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View menu_title;
    ERUI_Utf16View page_title;
    ERUI_PageTitleFormatter formatter;
    void* user_data;
    uint32_t reserved[2];
} ERUI_PagePresentationDesc;

/*
 * Queues one native Elden Ring message dialog. buttons selects its button
 * layout and placement selects its screen position. The host copies message
 * before enqueue_alert returns. callback is optional; user_data must be null
 * when callback is null. Requests are presented globally in FIFO order, one at
 * a time. callback and user_data are retained asynchronously and must remain
 * valid until callback is invoked.
 */
typedef struct ERUI_AlertDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View message;
    ERUI_AlertCallback callback;
    void* user_data;
    ERUI_AlertButtons buttons;
    ERUI_AlertPlacement placement;
} ERUI_AlertDesc;

/*
 * A provider owns at most one configuration document. Opening creates only an
 * in-memory handle; load and save are always explicit. The default directory
 * uses the provider ID when that cannot create a Windows path alias; otherwise
 * it uses a reserved SHA-256-derived directory so distinct IDs cannot share a
 * file.
 * Relative owner-module paths are validated beneath owner_module's folder at
 * open time. This is path hygiene for trusted in-process mods, not a security
 * boundary against later filesystem/junction changes. A custom beside-module
 * or absolute path must be exclusive to one provider; independently opened
 * documents do not coordinate or merge writes to the same backing file.
 *
 * Section and key identifiers contain 1..255 bytes and accept only ASCII
 * letters, digits, '.', '_', '-', and internal spaces. Leading/trailing
 * spaces and the identifiers "." and ".." are rejected. Values are valid
 * UTF-8 byte strings; embedded NUL, CR, and LF bytes are rejected. Empty
 * values and the characters '=', '#', and ';' are preserved literally.
 *
 * Documents are bounded to the limits below. ERUI_STORAGE_MAX_DECODED_BYTES
 * counts section-name, key, and value bytes in the in-memory document.
 * storage_erase is idempotent: erasing a missing key returns ERUI_OK and does
 * not change the document revision or dirty state.
 */
#define ERUI_STORAGE_MAX_IDENTIFIER_BYTES 255u
#define ERUI_STORAGE_MAX_SECTION_BYTES ERUI_STORAGE_MAX_IDENTIFIER_BYTES
#define ERUI_STORAGE_MAX_KEY_BYTES ERUI_STORAGE_MAX_IDENTIFIER_BYTES
#define ERUI_STORAGE_MAX_VALUE_BYTES 65536u
#define ERUI_STORAGE_MAX_FILE_BYTES 1048576u
#define ERUI_STORAGE_MAX_DECODED_BYTES 262144u
#define ERUI_STORAGE_MAX_SECTIONS 1024u
#define ERUI_STORAGE_MAX_ENTRIES 4096u
#define ERUI_STORAGE_MAX_ASSIGNMENT_CHANGES 4096u

typedef uint32_t ERUI_StorageLocation;
enum {
    ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT = 0u,
    ERUI_STORAGE_LOCATION_OWNER_MODULE_DIRECTORY = 1u,
    ERUI_STORAGE_LOCATION_EXPLICIT_ABSOLUTE = 2u
};

typedef struct ERUI_StorageDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_StorageLocation location;
    uint32_t reserved0;
    ERUI_Utf16View path;
    uint32_t reserved[2];
} ERUI_StorageDesc;

typedef struct ERUI_StorageKey {
    uint32_t size;
    uint32_t flags;
    ERUI_StringView section;
    ERUI_StringView key;
    uint32_t reserved[2];
} ERUI_StorageKey;

typedef struct ERUI_StorageInfo {
    uint32_t size;
    uint32_t flags;
    uint32_t loaded;
    uint32_t dirty;
    uint64_t current_revision;
    uint64_t last_saved_revision;
    uint32_t reserved[2];
} ERUI_StorageInfo;

typedef struct ERUI_Api {
    /* Client initializes size; host writes only this many bytes. */
    uint32_t size;
    uint32_t api_version;
    ERUI_Capabilities capabilities;

    ERUI_Result (ERUI_CALL* register_provider)(
        const ERUI_ProviderDesc* description,
        ERUI_ProviderHandle* out_provider,
        ERUI_PageHandle* out_root_page);
    ERUI_Result (ERUI_CALL* add_button)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ButtonDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_toggle)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ToggleDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_slider)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_SliderDesc* description,
        ERUI_RowHandle* out_row);
    /*
     * Both choice functions copy the descriptor and options synchronously.
     * Indices are zero-based. An inline choice changes with left/right input;
     * a popup choice opens Elden Ring's native selection list.
     */
    ERUI_Result (ERUI_CALL* add_inline_choice)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_popup_choice)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_submenu)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle parent_page,
        const ERUI_SubmenuDesc* description,
        ERUI_PageHandle* out_child_page,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* set_page_presentation)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_PagePresentationDesc* description);
    ERUI_Result (ERUI_CALL* commit_provider)(ERUI_ProviderHandle provider);
    ERUI_Result (ERUI_CALL* abort_provider)(ERUI_ProviderHandle provider);
    ERUI_Result (ERUI_CALL* set_row_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint8_t value);
    ERUI_Result (ERUI_CALL* get_row_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint8_t* out_value);
    ERUI_Result (ERUI_CALL* enqueue_alert)(
        ERUI_ProviderHandle provider,
        const ERUI_AlertDesc* description);
    ERUI_Result (ERUI_CALL* get_game_language)(
        ERUI_GameLanguageInfo* out_language);

    /* API 1.1 append-only TextInput block; the API 1.0 prefix ends above. */
    ERUI_Result (ERUI_CALL* add_text_input)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_TextInputDesc* description,
        ERUI_RowHandle* out_row);
    /*
     * Copies value before returning and does not invoke changed_callback.
     * If an editor is already active, Cancel preserves this programmatic
     * value while a later confirmed player value replaces it.
     */
    ERUI_Result (ERUI_CALL* set_text_input_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        const ERUI_Utf16View* value);
    /*
     * output_capacity and out_length count UTF-16 code units. Query with
     * output == NULL and output_capacity == 0. Output is not NUL-terminated.
     * An undersized buffer returns ERUI_BUFFER_TOO_SMALL, reports the required
     * length through out_length, and leaves the output buffer unchanged.
     */
    ERUI_Result (ERUI_CALL* get_text_input_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint16_t* output,
        uint32_t output_capacity,
        uint32_t* out_length);

    /* API 1.1 append-only ColorPicker block. */
    ERUI_Result (ERUI_CALL* add_color_picker)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ColorPickerDesc* description,
        ERUI_RowHandle* out_row);
    /*
     * Copies value before returning and does not invoke changed_callback.
     * If an editor is already active, Cancel preserves this programmatic
     * value while a later confirmed player value replaces it.
     */
    ERUI_Result (ERUI_CALL* set_color_picker_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        const ERUI_Color* value);
    ERUI_Result (ERUI_CALL* get_color_picker_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        ERUI_Color* out_value);

    /*
     * API 1.1 append-only built-in-page block. Returns a provider-owned page
     * handle that accepts the ordinary add_* and add_submenu functions.
     * GAME_OPTIONS returns the same logical page as register_provider's
     * out_root_page. COUNT and unknown values are invalid arguments.
     */
    ERUI_Result (ERUI_CALL* get_builtin_page)(
        ERUI_ProviderHandle provider,
        ERUI_BuiltinPage builtin_page,
        ERUI_PageHandle* out_page);

    /*
     * API 1.1 input-action block. Every action receives one
     * independently assignable controller, keyboard, and mouse alternative.
     * The player edits them through Elden Ring's native binding screens.
     */
    ERUI_Result (ERUI_CALL* add_input_section)(
        ERUI_ProviderHandle provider,
        const ERUI_InputSectionDesc* description,
        ERUI_InputSectionHandle* out_section);
    ERUI_Result (ERUI_CALL* add_input_action)(
        ERUI_ProviderHandle provider,
        ERUI_InputSectionHandle section,
        const ERUI_InputActionDesc* description,
        ERUI_InputActionHandle* out_action);
    ERUI_Result (ERUI_CALL* set_assignments_changed_handler)(
        ERUI_ProviderHandle provider,
        const ERUI_AssignmentsChangedHandlerDesc* description);
    /*
     * set_action_inputs applies every present slot atomically and silently;
     * absent slots remain unchanged. get_* return complete snapshots, where
     * absent means unsupported. reset is also silent.
     */
    ERUI_Result (ERUI_CALL* set_action_inputs)(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        const ERUI_ActionInputs* inputs);
    ERUI_Result (ERUI_CALL* get_action_inputs)(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        ERUI_ActionInputs* out_inputs);
    ERUI_Result (ERUI_CALL* get_action_default_inputs)(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        ERUI_ActionInputs* out_inputs);
    ERUI_Result (ERUI_CALL* reset_action_inputs)(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        ERUI_InputDevices devices);

    /* API 1.1 explicit, provider-owned storage block. */
    ERUI_Result (ERUI_CALL* open_storage)(
        ERUI_ProviderHandle provider,
        const ERUI_StorageDesc* description,
        ERUI_StorageHandle* out_storage);
    ERUI_Result (ERUI_CALL* storage_load)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage);
    ERUI_Result (ERUI_CALL* storage_save)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage);
    /* UTF-8 output is not NUL-terminated; use the ordinary two-call query. */
    ERUI_Result (ERUI_CALL* storage_get_utf8)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        char* output,
        uint32_t output_capacity,
        uint32_t* out_length);
    ERUI_Result (ERUI_CALL* storage_set_utf8)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        const ERUI_StringView* value);
    ERUI_Result (ERUI_CALL* storage_erase)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key);
    ERUI_Result (ERUI_CALL* storage_get_action_inputs)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        ERUI_ActionInputs* out_inputs);
    ERUI_Result (ERUI_CALL* storage_set_action_inputs)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        const ERUI_ActionInputs* inputs);
    /* Validates the complete batch, then applies it atomically in memory. */
    ERUI_Result (ERUI_CALL* storage_apply_assignment_changes)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StringView* section,
        const ERUI_AssignmentChange* changes,
        uint32_t change_count);
    ERUI_Result (ERUI_CALL* storage_get_info)(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        ERUI_StorageInfo* out_info);

    /* API 1.2 append-only exact numeric-slider block. */
    ERUI_Result (ERUI_CALL* add_numeric_slider)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_NumericSliderDesc* description,
        ERUI_RowHandle* out_row);
    /*
     * Programmatic changes are silent. The supplied DecimalValue may use a
     * different valid scale; the host converts it exactly to the row's scale,
     * rejects an inexact conversion, then rounds an off-step value down.
     */
    ERUI_Result (ERUI_CALL* set_numeric_slider_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        const ERUI_DecimalValue* value);
    ERUI_Result (ERUI_CALL* get_numeric_slider_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        ERUI_DecimalValue* out_value);

    /* API 1.2 append-only arbitrary custom-slider block. */
    ERUI_Result (ERUI_CALL* add_custom_slider)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_CustomSliderDesc* description,
        ERUI_RowHandle* out_row);
    /* Programmatic changes are silent; an out-of-range index is rejected. */
    ERUI_Result (ERUI_CALL* set_custom_slider_index)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint32_t index);
    ERUI_Result (ERUI_CALL* get_custom_slider_index)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint32_t* out_index);

    /*
     * API 1.3 append-only custom-tab block. The host copies description,
     * strings, icon descriptor, and pixel bytes before returning. The page
     * handle is provider-owned and works with the ordinary page/row API. The
     * entry is present in every valid 1.3 table; clients must still test
     * ERUI_CAP_CUSTOM_TABS because an unvalidated game build may expose the
     * table while returning ERUI_NOT_SUPPORTED for this feature.
     */
    ERUI_Result (ERUI_CALL* add_tab)(
        ERUI_ProviderHandle provider,
        const ERUI_TabDesc* description,
        ERUI_PageHandle* out_page);
    /*
     * API 1.3 append-only title-screen text block. The host copies the
     * descriptor and text synchronously before returning. This entry is
     * present in every valid 1.3 table; clients must test
     * ERUI_CAP_TITLE_SCREEN_TEXT before calling it because the host may
     * expose the table while returning ERUI_NOT_SUPPORTED for this feature.
     */
    ERUI_Result (ERUI_CALL* add_title_screen_text)(
        ERUI_ProviderHandle provider,
        const ERUI_TitleScreenTextDesc* description);

    /*
     * API 1.4 append-only generic update block. The host validates and copies
     * the complete batch before returning. Row updates are provider-scoped;
     * every row must belong to provider and a row may occur only once in a
     * batch. A view whose field bit is absent must be the canonical zero view
     * (NULL data, zero length, zero reserved). The call is safe from ERNativeUI
     * callbacks: native presentation changes occur later at a safe UI boundary.
     * This entry is present in every valid 1.4 table; test ERUI_CAP_UPDATES
     * before calling it because an unvalidated game build may return
     * ERUI_NOT_SUPPORTED. Availability fields additionally require
     * ERUI_CAP_ROW_AVAILABILITY.
     */
    ERUI_Result (ERUI_CALL* commit_updates)(
        ERUI_ProviderHandle provider,
        const ERUI_UpdateBatchDesc* description);
} ERUI_Api;

/* Complete function-table prefix required by API 1.0 clients. */
#define ERUI_API_V1_0_SIZE ((uint32_t)( \
    offsetof(ERUI_Api, get_game_language) + \
    sizeof(((ERUI_Api*)0)->get_game_language)))

/*
 * Complete function-table size required by API 1.1 clients. The frozen
 * ERUI_API_V1_0_SIZE and its first 128 bytes remain unchanged.
 */
#define ERUI_API_V1_1_SIZE ((uint32_t)( \
    offsetof(ERUI_Api, storage_get_info) + \
    sizeof(((ERUI_Api*)0)->storage_get_info)))

/*
 * Complete function-table size required by API 1.2 clients. The frozen API
 * 1.0 and 1.1 prefixes and their size constants remain unchanged.
 */
#define ERUI_API_V1_2_SIZE ((uint32_t)( \
    offsetof(ERUI_Api, get_custom_slider_index) + \
    sizeof(((ERUI_Api*)0)->get_custom_slider_index)))

/*
 * Complete function-table size required by API 1.3 clients. The frozen API
 * 1.0, 1.1, and 1.2 prefixes and their size constants remain unchanged.
 */
#define ERUI_API_V1_3_SIZE ((uint32_t)( \
    offsetof(ERUI_Api, add_title_screen_text) + \
    sizeof(((ERUI_Api*)0)->add_title_screen_text)))

/*
 * Complete function-table size required by API 1.4 clients. Every earlier
 * published prefix and size constant remains unchanged.
 */
#define ERUI_API_V1_4_SIZE ((uint32_t)( \
    offsetof(ERUI_Api, commit_updates) + \
    sizeof(((ERUI_Api*)0)->commit_updates)))
#define ERUI_API_CURRENT_SIZE ERUI_API_V1_4_SIZE

typedef ERUI_Result (ERUI_CALL* ERUI_GetApiFn)(
    uint32_t requested_version,
    ERUI_Api* out_api);

/*
 * ERUI_GetApi is also the explicit connection handshake. Call it from
 * a worker and retry ERUI_HOST_NOT_READY without reusing a partially written
 * table. A successful API 1.1 or later request guarantees that startup language
 * selection has settled. In automatic mode, get_game_language may still
 * return ERUI_NOT_SUPPORTED after a definitive Steam incompatibility. API 1.0
 * keeps its released compatibility lifecycle.
 */
ERUI_EXPORT ERUI_Result ERUI_CALL ERUI_GetApi(
    uint32_t requested_version,
    ERUI_Api* out_api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_ERUI_H_INCLUDED */
