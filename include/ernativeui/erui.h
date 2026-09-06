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
#define ERUI_API_VERSION_CURRENT ERUI_API_VERSION_1_1

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
    ERUI_STORAGE_FORMAT_ERROR = 20u
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
    ERUI_CAP_STORAGE = UINT64_C(1) << 15
};

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
 * The current 1.1 contract accepts maximum_length values in 1..35; zero
 * selects ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH.
 */
#define ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH 16u
#define ERUI_TEXT_INPUT_MAX_LENGTH 35u

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
 * Convenience classification for the exact Steam language identifier.
 * UNKNOWN means Steam returned a valid identifier not known to this host;
 * it does not mean that language discovery failed.
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
 * identifier is the exact UTF-8 token returned by Steam (for example
 * "french", "koreana", or an identifier unknown to ERNativeUI). Its memory
 * is host-owned and remains valid until process exit. The caller initializes
 * size before calling get_game_language.
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
 * Native color pickers edit opaque RGB colors. reserved must be zero. The
 * explicit channels keep Elden Ring's private packed-color representation out
 * of the public ABI.
 */
typedef struct ERUI_Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t reserved;
} ERUI_Color;

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
 * user_data must remain valid until commit_provider returns.
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
#define ERUI_API_CURRENT_SIZE ERUI_API_V1_1_SIZE

typedef ERUI_Result (ERUI_CALL* ERUI_GetApiFn)(
    uint32_t requested_version,
    ERUI_Api* out_api);

/*
 * ERUI_GetApi is also the explicit API 1.1 connection handshake. Call it from
 * a worker and retry ERUI_HOST_NOT_READY without reusing a partially written
 * table. A successful API 1.1 request guarantees that startup language
 * discovery has settled; get_game_language may still return
 * ERUI_NOT_SUPPORTED after a definitive Steam incompatibility. API 1.0 keeps
 * its released compatibility lifecycle.
 */
ERUI_EXPORT ERUI_Result ERUI_CALL ERUI_GetApi(
    uint32_t requested_version,
    ERUI_Api* out_api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_ERUI_H_INCLUDED */
