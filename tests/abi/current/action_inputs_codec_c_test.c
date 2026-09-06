#include <ernativeui/erui.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int erui_action_inputs_codec_other_translation_unit(void);

#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            fprintf(stderr, "check failed at line %d: %s\n",                 \
                __LINE__, #expression);                                        \
            return 0;                                                          \
        }                                                                      \
    } while (0)

static const char* const k_controller_names[ERUI_CONTROLLER_BUTTON_COUNT] = {
    "", "dpad-up", "dpad-down", "dpad-left", "dpad-right",
    "face-south", "face-east", "face-west", "face-north",
    "left-shoulder", "right-shoulder", "left-trigger", "right-trigger",
    "left-stick", "right-stick",
};

static const char* const k_keyboard_names[ERUI_KEYBOARD_KEY_COUNT] = {
    "",
    "digit-1", "digit-2", "digit-3", "digit-4", "digit-5",
    "digit-6", "digit-7", "digit-8", "digit-9", "digit-0",
    "backspace", "tab",
    "key-q", "key-w", "key-e", "key-r", "key-t", "key-y",
    "key-u", "key-i", "key-o", "key-p",
    "enter", "left-control",
    "key-a", "key-s", "key-d", "key-f", "key-g", "key-h",
    "key-j", "key-k", "key-l", "left-shift",
    "key-z", "key-x", "key-c", "key-v", "key-b", "key-n", "key-m",
    "right-shift", "left-alt", "space",
    "numpad-7", "numpad-8", "numpad-9", "numpad-4", "numpad-5",
    "numpad-6", "numpad-1", "numpad-2", "numpad-3", "numpad-0",
    "numpad-enter", "right-control", "right-alt",
    "home", "arrow-up", "page-up", "arrow-left", "arrow-right", "end",
    "arrow-down", "page-down", "insert", "delete",
    "numpad-multiply", "numpad-subtract", "numpad-add", "numpad-decimal",
    "numpad-divide",
};

static const char* const k_mouse_names[ERUI_MOUSE_BUTTON_COUNT] = {
    "", "left", "right", "middle", "button4", "button5", "wheel-up",
    "wheel-down",
};

static ERUI_ActionInputs empty_inputs(void) {
    ERUI_ActionInputs result = {0};
    result.size = (uint32_t)sizeof(result);
    return result;
}

static int same_inputs(
    const ERUI_ActionInputs* left,
    const ERUI_ActionInputs* right) {
    return memcmp(left, right, sizeof(*left)) == 0;
}

static ERUI_StringView bytes(const char* data, size_t length) {
    ERUI_StringView result;
    result.data = data;
    result.length = (uint32_t)length;
    result.reserved = 0u;
    return result;
}

static int format_equals(
    const ERUI_ActionInputs* inputs,
    const char* expected) {
    unsigned char guarded[96];
    uint32_t required = UINT32_C(0xDEADBEEF);
    const size_t expected_length = strlen(expected);

    memset(guarded, 0xA5, sizeof(guarded));
    CHECK(ERUI_FormatActionInputs(inputs, (char*)guarded,
        (uint32_t)sizeof(guarded), &required) == ERUI_OK);
    CHECK(required == expected_length);
    CHECK(memcmp(guarded, expected, expected_length) == 0);
    CHECK(guarded[expected_length] == 0xA5u);
    return 1;
}

static int parse_equals(
    const char* text,
    size_t length,
    const ERUI_ActionInputs* expected) {
    ERUI_ActionInputs parsed;
    ERUI_StringView view = bytes(text, length);

    memset(&parsed, 0xA5, sizeof(parsed));
    CHECK(ERUI_ParseActionInputs(&view, &parsed) == ERUI_OK);
    CHECK(same_inputs(&parsed, expected));
    CHECK(parsed.size == sizeof(parsed));
    CHECK(parsed.flags == 0u);
    CHECK(parsed.reserved[0] == 0u && parsed.reserved[1] == 0u);
    return 1;
}

static int parse_c_string_equals(
    const char* text,
    const ERUI_ActionInputs* expected) {
    return parse_equals(text, strlen(text), expected);
}

static int parse_fails(
    const char* text,
    size_t length,
    ERUI_Result expected_result) {
    ERUI_ActionInputs output;
    ERUI_ActionInputs before;
    ERUI_StringView view = bytes(text, length);

    memset(&output, 0xA5, sizeof(output));
    before = output;
    CHECK(ERUI_ParseActionInputs(&view, &output) == expected_result);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    return 1;
}

static int format_rejects(const ERUI_ActionInputs* inputs) {
    unsigned char output[96];
    unsigned char before[96];
    uint32_t length = UINT32_C(0xDEADBEEF);

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    CHECK(ERUI_FormatActionInputs(inputs, (char*)output,
        (uint32_t)sizeof(output), &length) == ERUI_INVALID_ARGUMENT);
    CHECK(length == 0u);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    return 1;
}

static int test_golden_controller_names(void) {
    uint32_t value;
    for (value = 1u; value < ERUI_CONTROLLER_BUTTON_COUNT; ++value) {
        ERUI_ActionInputs inputs = empty_inputs();
        ERUI_ActionInputs expected = empty_inputs();
        char encoded[64];
        const int written = snprintf(encoded, sizeof(encoded),
            "controller:%s", k_controller_names[value]);
        CHECK(written > 0 && (size_t)written < sizeof(encoded));
        inputs.controller.state = ERUI_INPUT_SLOT_BOUND;
        inputs.controller.input = value;
        expected.controller = inputs.controller;
        CHECK(format_equals(&inputs, encoded));
        CHECK(parse_c_string_equals(encoded, &expected));
    }
    return 1;
}

static int test_golden_keyboard_names(void) {
    uint32_t value;
    for (value = 1u; value < ERUI_KEYBOARD_KEY_COUNT; ++value) {
        ERUI_ActionInputs inputs = empty_inputs();
        ERUI_ActionInputs expected = empty_inputs();
        char encoded[64];
        const int written = snprintf(encoded, sizeof(encoded),
            "keyboard:%s", k_keyboard_names[value]);
        CHECK(written > 0 && (size_t)written < sizeof(encoded));
        inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
        inputs.keyboard.input = value;
        expected.keyboard = inputs.keyboard;
        CHECK(format_equals(&inputs, encoded));
        CHECK(parse_c_string_equals(encoded, &expected));
    }
    return 1;
}

static int test_golden_mouse_names(void) {
    uint32_t value;
    for (value = 1u; value < ERUI_MOUSE_BUTTON_COUNT; ++value) {
        ERUI_ActionInputs inputs = empty_inputs();
        ERUI_ActionInputs expected = empty_inputs();
        char encoded[64];
        const int written = snprintf(encoded, sizeof(encoded),
            "mouse:%s", k_mouse_names[value]);
        CHECK(written > 0 && (size_t)written < sizeof(encoded));
        inputs.mouse.state = ERUI_INPUT_SLOT_BOUND;
        inputs.mouse.input = value;
        expected.mouse = inputs.mouse;
        CHECK(format_equals(&inputs, encoded));
        CHECK(parse_c_string_equals(encoded, &expected));
    }
    return 1;
}

static void set_representative_slot(
    ERUI_ActionInputs* inputs,
    unsigned device,
    ERUI_InputSlotState state) {
    uint32_t input = 0u;
    if (state == ERUI_INPUT_SLOT_BOUND) {
        input = device == 0u ? ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER
            : device == 1u ? ERUI_KEYBOARD_KEY_Q
                           : ERUI_MOUSE_BUTTON_4;
    }
    if (device == 0u) {
        inputs->controller.state = state;
        inputs->controller.input = input;
    } else if (device == 1u) {
        inputs->keyboard.state = state;
        inputs->keyboard.input = input;
    } else {
        inputs->mouse.state = state;
        inputs->mouse.input = input;
    }
}

static int test_all_slot_state_shapes(void) {
    ERUI_InputSlotState controller_state;
    ERUI_InputSlotState keyboard_state;
    ERUI_InputSlotState mouse_state;

    for (controller_state = ERUI_INPUT_SLOT_ABSENT;
         controller_state <= ERUI_INPUT_SLOT_BOUND; ++controller_state) {
        for (keyboard_state = ERUI_INPUT_SLOT_ABSENT;
             keyboard_state <= ERUI_INPUT_SLOT_BOUND; ++keyboard_state) {
            for (mouse_state = ERUI_INPUT_SLOT_ABSENT;
                 mouse_state <= ERUI_INPUT_SLOT_BOUND; ++mouse_state) {
                ERUI_ActionInputs inputs = empty_inputs();
                ERUI_ActionInputs parsed;
                ERUI_StringView encoded_view;
                char encoded[ERUI_ACTION_INPUTS_TEXT_MAX_BYTES + 1u];
                uint32_t encoded_length = 0u;
                set_representative_slot(&inputs, 0u, controller_state);
                set_representative_slot(&inputs, 1u, keyboard_state);
                set_representative_slot(&inputs, 2u, mouse_state);
                memset(encoded, 0xA5, sizeof(encoded));
                CHECK(ERUI_FormatActionInputs(&inputs, encoded,
                    (uint32_t)sizeof(encoded), &encoded_length) == ERUI_OK);
                CHECK(encoded_length <= ERUI_ACTION_INPUTS_TEXT_MAX_BYTES);
                encoded_view = bytes(encoded, encoded_length);
                memset(&parsed, 0xA5, sizeof(parsed));
                CHECK(ERUI_ParseActionInputs(&encoded_view, &parsed) ==
                    ERUI_OK);
                CHECK(same_inputs(&inputs, &parsed));
            }
        }
    }

    {
        ERUI_ActionInputs empty = empty_inputs();
        ERUI_ActionInputs unbound = empty_inputs();
        ERUI_ActionInputs sparse = empty_inputs();
        set_representative_slot(&unbound, 0u, ERUI_INPUT_SLOT_UNBOUND);
        set_representative_slot(&unbound, 1u, ERUI_INPUT_SLOT_UNBOUND);
        set_representative_slot(&unbound, 2u, ERUI_INPUT_SLOT_UNBOUND);
        set_representative_slot(&sparse, 1u, ERUI_INPUT_SLOT_BOUND);
        set_representative_slot(&sparse, 2u, ERUI_INPUT_SLOT_UNBOUND);
        CHECK(format_equals(&empty, ""));
        CHECK(format_equals(&unbound,
            "controller:unbound,keyboard:unbound,mouse:unbound"));
        CHECK(format_equals(&sparse, "keyboard:key-q,mouse:unbound"));
    }
    return 1;
}

static int test_canonical_order_and_maximum(void) {
    static const char canonical[] =
        "controller:right-trigger,keyboard:key-q,mouse:button4";
    static const char* const permutations[] = {
        "controller:right-trigger,keyboard:key-q,mouse:button4",
        "controller:right-trigger,mouse:button4,keyboard:key-q",
        "keyboard:key-q,controller:right-trigger,mouse:button4",
        "keyboard:key-q,mouse:button4,controller:right-trigger",
        "mouse:button4,controller:right-trigger,keyboard:key-q",
        "mouse:button4,keyboard:key-q,controller:right-trigger",
    };
    static const char longest[] =
        "controller:right-shoulder,keyboard:numpad-multiply,mouse:wheel-down";
    ERUI_ActionInputs expected = empty_inputs();
    ERUI_ActionInputs longest_inputs = empty_inputs();
    size_t index;

    expected.controller.state = ERUI_INPUT_SLOT_BOUND;
    expected.controller.input = ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER;
    expected.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    expected.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    expected.mouse.state = ERUI_INPUT_SLOT_BOUND;
    expected.mouse.input = ERUI_MOUSE_BUTTON_4;
    for (index = 0u; index < sizeof(permutations) / sizeof(permutations[0]);
         ++index) {
        char reformatted[96];
        uint32_t length = 0u;
        ERUI_ActionInputs parsed;
        ERUI_StringView view = bytes(permutations[index],
            strlen(permutations[index]));
        memset(&parsed, 0xA5, sizeof(parsed));
        CHECK(ERUI_ParseActionInputs(&view, &parsed) == ERUI_OK);
        CHECK(same_inputs(&parsed, &expected));
        CHECK(ERUI_FormatActionInputs(&parsed, reformatted,
            (uint32_t)sizeof(reformatted), &length) == ERUI_OK);
        CHECK(length == sizeof(canonical) - 1u);
        CHECK(memcmp(reformatted, canonical, length) == 0);
    }

    longest_inputs.controller.state = ERUI_INPUT_SLOT_BOUND;
    longest_inputs.controller.input = ERUI_CONTROLLER_BUTTON_RIGHT_SHOULDER;
    longest_inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    longest_inputs.keyboard.input = ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY;
    longest_inputs.mouse.state = ERUI_INPUT_SLOT_BOUND;
    longest_inputs.mouse.input = ERUI_MOUSE_BUTTON_WHEEL_DOWN;
    CHECK(sizeof(longest) - 1u == ERUI_ACTION_INPUTS_TEXT_MAX_BYTES);
    CHECK(format_equals(&longest_inputs, longest));
    return 1;
}

static int test_format_buffer_contract(void) {
    static const char expected[] =
        "controller:right-shoulder,keyboard:numpad-multiply,mouse:wheel-down";
    ERUI_ActionInputs inputs = empty_inputs();
    ERUI_ActionInputs empty = empty_inputs();
    unsigned char output[96];
    unsigned char before[96];
    uint32_t length;

    inputs.controller.state = ERUI_INPUT_SLOT_BOUND;
    inputs.controller.input = ERUI_CONTROLLER_BUTTON_RIGHT_SHOULDER;
    inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    inputs.keyboard.input = ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY;
    inputs.mouse.state = ERUI_INPUT_SLOT_BOUND;
    inputs.mouse.input = ERUI_MOUSE_BUTTON_WHEEL_DOWN;

    length = UINT32_C(0xDEADBEEF);
    CHECK(ERUI_FormatActionInputs(&inputs, NULL, 0u, &length) == ERUI_OK);
    CHECK(length == sizeof(expected) - 1u);

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    length = UINT32_C(0xDEADBEEF);
    CHECK(ERUI_FormatActionInputs(&inputs, (char*)output,
        (uint32_t)(sizeof(expected) - 2u), &length) ==
        ERUI_BUFFER_TOO_SMALL);
    CHECK(length == sizeof(expected) - 1u);
    CHECK(memcmp(output, before, sizeof(output)) == 0);

    memset(output, 0xA5, sizeof(output));
    length = 0u;
    CHECK(ERUI_FormatActionInputs(&inputs, (char*)output,
        (uint32_t)(sizeof(expected) - 1u), &length) == ERUI_OK);
    CHECK(length == sizeof(expected) - 1u);
    CHECK(memcmp(output, expected, length) == 0);
    CHECK(output[length] == 0xA5u);

    memset(output, 0xA5, sizeof(output));
    length = 0u;
    CHECK(ERUI_FormatActionInputs(&inputs, (char*)output,
        (uint32_t)sizeof(output), &length) == ERUI_OK);
    CHECK(memcmp(output, expected, length) == 0);
    CHECK(output[length] == 0xA5u);

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    length = UINT32_C(0xDEADBEEF);
    CHECK(ERUI_FormatActionInputs(&inputs, NULL, 1u, &length) ==
        ERUI_INVALID_ARGUMENT);
    CHECK(length == 0u);
    CHECK(memcmp(output, before, sizeof(output)) == 0);

    length = UINT32_C(0xDEADBEEF);
    CHECK(ERUI_FormatActionInputs(NULL, (char*)output,
        (uint32_t)sizeof(output), &length) == ERUI_INVALID_ARGUMENT);
    CHECK(length == 0u);

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    CHECK(ERUI_FormatActionInputs(&inputs, (char*)output,
        (uint32_t)sizeof(output), NULL) == ERUI_INVALID_ARGUMENT);
    CHECK(memcmp(output, before, sizeof(output)) == 0);

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    length = UINT32_C(0xDEADBEEF);
    CHECK(ERUI_FormatActionInputs(&empty, (char*)output, 0u, &length) ==
        ERUI_OK);
    CHECK(length == 0u);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    CHECK(ERUI_FormatActionInputs(&empty, NULL, 0u, &length) == ERUI_OK);
    CHECK(length == 0u);
    return 1;
}

static int test_invalid_values(void) {
    ERUI_ActionInputs invalid;

    invalid = empty_inputs(); invalid.size = 0u; CHECK(format_rejects(&invalid));
    invalid = empty_inputs(); invalid.size = sizeof(invalid) - 1u;
    CHECK(format_rejects(&invalid));
    invalid = empty_inputs(); invalid.size = sizeof(invalid) + 1u;
    CHECK(format_rejects(&invalid));
    invalid = empty_inputs(); invalid.flags = 1u; CHECK(format_rejects(&invalid));
    invalid = empty_inputs(); invalid.reserved[0] = 1u;
    CHECK(format_rejects(&invalid));
    invalid = empty_inputs(); invalid.reserved[1] = 1u;
    CHECK(format_rejects(&invalid));

#define CHECK_INVALID_SLOT(member, count)                                      \
    do {                                                                       \
        invalid = empty_inputs();                                              \
        invalid.member.state = (ERUI_InputSlotState)3u;                        \
        CHECK(format_rejects(&invalid));                                       \
        invalid = empty_inputs();                                              \
        invalid.member.state = ERUI_INPUT_SLOT_ABSENT;                         \
        invalid.member.input = 1u;                                             \
        CHECK(format_rejects(&invalid));                                       \
        invalid = empty_inputs();                                              \
        invalid.member.state = ERUI_INPUT_SLOT_UNBOUND;                        \
        invalid.member.input = 1u;                                             \
        CHECK(format_rejects(&invalid));                                       \
        invalid = empty_inputs();                                              \
        invalid.member.state = ERUI_INPUT_SLOT_BOUND;                          \
        invalid.member.input = 0u;                                             \
        CHECK(format_rejects(&invalid));                                       \
        invalid = empty_inputs();                                              \
        invalid.member.state = ERUI_INPUT_SLOT_BOUND;                          \
        invalid.member.input = (count);                                        \
        CHECK(format_rejects(&invalid));                                       \
        invalid = empty_inputs();                                              \
        invalid.member.state = ERUI_INPUT_SLOT_BOUND;                          \
        invalid.member.input = UINT32_MAX;                                     \
        CHECK(format_rejects(&invalid));                                       \
    } while (0)

    CHECK_INVALID_SLOT(controller, ERUI_CONTROLLER_BUTTON_COUNT);
    CHECK_INVALID_SLOT(keyboard, ERUI_KEYBOARD_KEY_COUNT);
    CHECK_INVALID_SLOT(mouse, ERUI_MOUSE_BUTTON_COUNT);
#undef CHECK_INVALID_SLOT
    return 1;
}

static int test_parse_descriptor_and_counted_view_contract(void) {
    static const char expected_text[] = "keyboard:key-q";
    static const char with_suffix[] = "keyboard:key-qTHIS-IS-NOT-IN-THE-VIEW";
    static const char with_nul[] = "keyboard:key-q\0mouse:left";
    ERUI_ActionInputs expected = empty_inputs();
    ERUI_ActionInputs output;
    ERUI_ActionInputs before;
    ERUI_StringView view;
    char exact[sizeof(expected_text) - 1u];

    expected.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    expected.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    memcpy(exact, expected_text, sizeof(exact));
    CHECK(parse_equals(exact, sizeof(exact), &expected));
    CHECK(parse_equals(with_suffix, sizeof(expected_text) - 1u, &expected));
    CHECK(parse_fails(with_nul, sizeof(with_nul) - 1u,
        ERUI_STORAGE_FORMAT_ERROR));

    memset(&output, 0xA5, sizeof(output));
    before = output;
    CHECK(ERUI_ParseActionInputs(NULL, &output) == ERUI_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);

    view = bytes(expected_text, sizeof(expected_text) - 1u);
    CHECK(ERUI_ParseActionInputs(&view, NULL) == ERUI_INVALID_ARGUMENT);

    view.reserved = 1u;
    CHECK(ERUI_ParseActionInputs(&view, &output) == ERUI_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);

    view = bytes(NULL, 1u);
    CHECK(ERUI_ParseActionInputs(&view, &output) == ERUI_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);

    view = bytes(NULL, 0u);
    CHECK(ERUI_ParseActionInputs(&view, &output) == ERUI_OK);
    CHECK(same_inputs(&output, &(ERUI_ActionInputs){
        (uint32_t)sizeof(ERUI_ActionInputs), 0u,
        {ERUI_INPUT_SLOT_ABSENT, 0u},
        {ERUI_INPUT_SLOT_ABSENT, 0u},
        {ERUI_INPUT_SLOT_ABSENT, 0u}, {0u, 0u}}));

    view = bytes("", 0u);
    memset(&output, 0xA5, sizeof(output));
    CHECK(ERUI_ParseActionInputs(&view, &output) == ERUI_OK);
    expected = empty_inputs();
    CHECK(same_inputs(&output, &expected));
    return 1;
}

static int test_malformed_text(void) {
    static const char* const malformed[] = {
        ",", "controller", ":right-trigger", "controller:",
        "controller::right-trigger", "controller:right-trigger:",
        ",controller:right-trigger", "controller:right-trigger,",
        "controller:right-trigger,,mouse:left",
        " controller:right-trigger", "controller:right-trigger ",
        "controller :right-trigger", "controller: right-trigger",
        "controller:\tright-trigger", "controller:right-trigger\n",
        "Controller:right-trigger", "controller:RIGHT-TRIGGER",
        "controller:right_trigger", "gamepad:right-trigger",
        "controller:key-q", "keyboard:right-trigger", "mouse:key-q",
        "controller:invalid", "controller:0", "controller:unboundx",
        "keyboard:f7", "keyboard:escape", "keyboard:Q", "mouse:button-4",
        "mouse:left,mouse:right",
        "controller:unbound,controller:right-trigger",
        "keyboard:key-q,keyboard:key-q",
        "controller:right-trigger;mouse:left",
        "controller:right-trigger\r\nmouse:left",
    };
    static const char non_ascii[] = "keyboard:key-\xC3\xA9";
    static const char invalid_utf8[] = "mouse:\xFF";
    char oversized[ERUI_ACTION_INPUTS_TEXT_MAX_BYTES + 2u];
    size_t index;

    for (index = 0u; index < sizeof(malformed) / sizeof(malformed[0]);
         ++index) {
        CHECK(parse_fails(malformed[index], strlen(malformed[index]),
            ERUI_STORAGE_FORMAT_ERROR));
    }
    CHECK(parse_fails(non_ascii, sizeof(non_ascii) - 1u,
        ERUI_STORAGE_FORMAT_ERROR));
    CHECK(parse_fails(invalid_utf8, sizeof(invalid_utf8) - 1u,
        ERUI_STORAGE_FORMAT_ERROR));
    memset(oversized, 'x', sizeof(oversized));
    CHECK(parse_fails(oversized, sizeof(oversized),
        ERUI_STORAGE_FORMAT_ERROR));
    return 1;
}

int main(void) {
    if (ERUI_ACTION_INPUTS_TEXT_MAX_BYTES != 67u) return 1;
    if (!erui_action_inputs_codec_other_translation_unit()) return 2;
    if (!test_golden_controller_names()) return 3;
    if (!test_golden_keyboard_names()) return 4;
    if (!test_golden_mouse_names()) return 5;
    if (!test_all_slot_state_shapes()) return 6;
    if (!test_canonical_order_and_maximum()) return 7;
    if (!test_format_buffer_contract()) return 8;
    if (!test_invalid_values()) return 9;
    if (!test_parse_descriptor_and_counted_view_contract()) return 10;
    if (!test_malformed_text()) return 11;
    return 0;
}
