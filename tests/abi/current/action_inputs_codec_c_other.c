#include <ernativeui/erui.h>

#include <stdint.h>
#include <string.h>

/*
 * Kept in a second translation unit deliberately. The public codec is
 * header-only, so this catches accidental external-inline linkage that would
 * otherwise produce either duplicate definitions or unresolved symbols for
 * strict-C clients.
 */
int erui_action_inputs_codec_other_translation_unit(void) {
    static const char expected[] = "mouse:button5";
    ERUI_ActionInputs inputs = {0};
    ERUI_ActionInputs parsed = {0};
    ERUI_StringView view = {expected, (uint32_t)(sizeof(expected) - 1u), 0u};
    char output[sizeof(expected)] = {0};
    uint32_t length = 0u;

    inputs.size = (uint32_t)sizeof(inputs);
    inputs.mouse.state = ERUI_INPUT_SLOT_BOUND;
    inputs.mouse.input = ERUI_MOUSE_BUTTON_5;

    if (ERUI_FormatActionInputs(
            &inputs, output, (uint32_t)sizeof(output), &length) != ERUI_OK ||
        length != sizeof(expected) - 1u ||
        memcmp(output, expected, length) != 0) {
        return 0;
    }
    if (ERUI_ParseActionInputs(&view, &parsed) != ERUI_OK ||
        parsed.size != sizeof(parsed) ||
        parsed.mouse.state != ERUI_INPUT_SLOT_BOUND ||
        parsed.mouse.input != ERUI_MOUSE_BUTTON_5) {
        return 0;
    }
    return 1;
}
