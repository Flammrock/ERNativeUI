#include <ernativeui/erui.h>

#include <string.h>

int main(void) {
    ERUI_Api api = {0};
    ERUI_ActionInputs inputs = {0};
    ERUI_ActionInputs parsed = {0};
    ERUI_StringView encoded_view = {0};
    char encoded[ERUI_ACTION_INPUTS_TEXT_MAX_BYTES];
    uint32_t encoded_length = 0u;
    api.size = ERUI_API_CURRENT_SIZE;
    inputs.size = (uint32_t)sizeof(inputs);
    inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    inputs.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    if (ERUI_FormatActionInputs(&inputs, encoded,
            (uint32_t)sizeof(encoded), &encoded_length) != ERUI_OK ||
        encoded_length != sizeof("keyboard:key-q") - 1u ||
        memcmp(encoded, "keyboard:key-q", encoded_length) != 0) {
        return 2;
    }
    encoded_view.data = encoded;
    encoded_view.length = encoded_length;
    if (ERUI_ParseActionInputs(&encoded_view, &parsed) != ERUI_OK ||
        parsed.keyboard.state != ERUI_INPUT_SLOT_BOUND ||
        parsed.keyboard.input != ERUI_KEYBOARD_KEY_Q) {
        return 3;
    }
    return ERUI_API_V1_0_SIZE == 128u &&
            ERUI_API_CURRENT_SIZE == (uint32_t)sizeof(api) &&
            api.size == ERUI_API_CURRENT_SIZE
        ? 0
        : 1;
}
