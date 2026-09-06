# Input values and `ActionInputs` codec

This is the exhaustive ERUI API 1.1 reference for controller, keyboard, and
mouse values. These are stable ERNativeUI identifiers, not Elden Ring's
private input tokens. Use the names below in source code and the canonical
lowercase spellings when storing an `ActionInputs` value as text.

For the complete action workflow, read
[Native input bindings](../guides/input-bindings.md). To store assignments with
ERNativeUI's optional INI service, read
[Provider storage](../guides/storage.md#persist-input-assignments).
For the native probes and the reason each exclusion exists, read the
[input-bindings case study](../research/case-studies/input-bindings.md).

## Devices and slot states

`erui::InputDevice` identifies which alternative activated an action. An
`ActionActivation::devices` mask may contain more than one bit when several
alternatives rise in the same sampled frame.

| C++ value | Strict-C value | Public value |
|---|---|---:|
| `erui::InputDevice::controller` | `ERUI_INPUT_DEVICE_CONTROLLER` | `1` |
| `erui::InputDevice::keyboard` | `ERUI_INPUT_DEVICE_KEYBOARD` | `2` |
| `erui::InputDevice::mouse` | `ERUI_INPUT_DEVICE_MOUSE` | `4` |

C also defines `ERUI_INPUT_DEVICE_NONE` as `0` and
`ERUI_INPUT_DEVICE_ALL` as `7`. C++ uses `erui::InputDeviceMask`, an alias of
`ERUI_InputDevices`, for the complete bit mask and
`activation.includes(erui::InputDevice::keyboard)` to test one device.

Every `ActionInputs` device slot has one of three states:

| C++ value | Strict-C value | Public value | Meaning |
|---|---|---:|---|
| `erui::InputSlotState::absent` | `ERUI_INPUT_SLOT_ABSENT` | `0` | The device is not present. |
| `erui::InputSlotState::unbound` | `ERUI_INPUT_SLOT_UNBOUND` | `1` | The device is supported but intentionally has no assignment. |
| `erui::InputSlotState::bound` | `ERUI_INPUT_SLOT_BOUND` | `2` | The device is supported and has one semantic input value. |

The meaning of `absent` depends on where the value is used:

| Context | `absent` | `unbound` | `bound` |
|---|---|---|---|
| Action declaration or complete query/event snapshot | Device unsupported | Supported with no assignment | Supported with this assignment |
| `InputAction::bind(...)` sparse update | Leave the device unchanged | Clear the device | Replace the device assignment |
| Persisted sparse override | Inherit the mod's declared default | Preserve an explicit clear | Override the declared default |

`erui::ActionInputs{}` has all three slots absent. The no-argument fluent
methods make one slot present and unbound; the overload taking a value makes
that slot bound:

```cpp
auto patch = erui::ActionInputs{}
    .controller()
    .keyboard(erui::KeyboardKey::key_q)
    .mouse(erui::MouseButton::button4);
```

Use `controller_supported()`, `keyboard_supported()`, and
`mouse_supported()` to distinguish absent slots. The corresponding
`*_state()` method returns the exact state, while `*_input()` returns an
`std::optional` containing a value only for a bound slot. `empty()` means all
three slots are absent.

## Controller buttons

Face-button names describe physical positions rather than platform glyphs:
south is the lower face button, east is right, west is left, and north is
upper. Elden Ring remains responsible for the displayed controller icon.

| C++ value | Strict-C value | Public value | Canonical text |
|---|---|---:|---|
| `erui::ControllerButton::dpad_up` | `ERUI_CONTROLLER_BUTTON_DPAD_UP` | `1` | `dpad-up` |
| `erui::ControllerButton::dpad_down` | `ERUI_CONTROLLER_BUTTON_DPAD_DOWN` | `2` | `dpad-down` |
| `erui::ControllerButton::dpad_left` | `ERUI_CONTROLLER_BUTTON_DPAD_LEFT` | `3` | `dpad-left` |
| `erui::ControllerButton::dpad_right` | `ERUI_CONTROLLER_BUTTON_DPAD_RIGHT` | `4` | `dpad-right` |
| `erui::ControllerButton::face_south` | `ERUI_CONTROLLER_BUTTON_FACE_SOUTH` | `5` | `face-south` |
| `erui::ControllerButton::face_east` | `ERUI_CONTROLLER_BUTTON_FACE_EAST` | `6` | `face-east` |
| `erui::ControllerButton::face_west` | `ERUI_CONTROLLER_BUTTON_FACE_WEST` | `7` | `face-west` |
| `erui::ControllerButton::face_north` | `ERUI_CONTROLLER_BUTTON_FACE_NORTH` | `8` | `face-north` |
| `erui::ControllerButton::left_shoulder` | `ERUI_CONTROLLER_BUTTON_LEFT_SHOULDER` | `9` | `left-shoulder` |
| `erui::ControllerButton::right_shoulder` | `ERUI_CONTROLLER_BUTTON_RIGHT_SHOULDER` | `10` | `right-shoulder` |
| `erui::ControllerButton::left_trigger` | `ERUI_CONTROLLER_BUTTON_LEFT_TRIGGER` | `11` | `left-trigger` |
| `erui::ControllerButton::right_trigger` | `ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER` | `12` | `right-trigger` |
| `erui::ControllerButton::left_stick` | `ERUI_CONTROLLER_BUTTON_LEFT_STICK` | `13` | `left-stick` |
| `erui::ControllerButton::right_stick` | `ERUI_CONTROLLER_BUTTON_RIGHT_STICK` | `14` | `right-stick` |

The table is exhaustive. `ERUI_CONTROLLER_BUTTON_INVALID` (`0`) and
`ERUI_CONTROLLER_BUTTON_COUNT` (`15`) are strict-C sentinels and are never
valid bound assignments. API 1.1 intentionally excludes Back/Start/Menu,
layout-relative Confirm/Cancel aliases, stick axes, and stick-direction
aliases.

## Keyboard keys

Letter and digit names denote physical positions on a US/QWERTY reference
layout; they do not denote a typed Unicode character. Elden Ring applies its
keyboard profile when it displays the keycap. For example, on a French AZERTY
layout, `key_q` is the physical QWERTY-Q position and the game displays `A`.
Likewise, `digit_1` identifies the top-row physical position without implying
Shift or a particular printed character.

| C++ value | Strict-C value | Public value | Canonical text |
|---|---|---:|---|
| `erui::KeyboardKey::digit_1` | `ERUI_KEYBOARD_KEY_DIGIT_1` | `1` | `digit-1` |
| `erui::KeyboardKey::digit_2` | `ERUI_KEYBOARD_KEY_DIGIT_2` | `2` | `digit-2` |
| `erui::KeyboardKey::digit_3` | `ERUI_KEYBOARD_KEY_DIGIT_3` | `3` | `digit-3` |
| `erui::KeyboardKey::digit_4` | `ERUI_KEYBOARD_KEY_DIGIT_4` | `4` | `digit-4` |
| `erui::KeyboardKey::digit_5` | `ERUI_KEYBOARD_KEY_DIGIT_5` | `5` | `digit-5` |
| `erui::KeyboardKey::digit_6` | `ERUI_KEYBOARD_KEY_DIGIT_6` | `6` | `digit-6` |
| `erui::KeyboardKey::digit_7` | `ERUI_KEYBOARD_KEY_DIGIT_7` | `7` | `digit-7` |
| `erui::KeyboardKey::digit_8` | `ERUI_KEYBOARD_KEY_DIGIT_8` | `8` | `digit-8` |
| `erui::KeyboardKey::digit_9` | `ERUI_KEYBOARD_KEY_DIGIT_9` | `9` | `digit-9` |
| `erui::KeyboardKey::digit_0` | `ERUI_KEYBOARD_KEY_DIGIT_0` | `10` | `digit-0` |
| `erui::KeyboardKey::backspace` | `ERUI_KEYBOARD_KEY_BACKSPACE` | `11` | `backspace` |
| `erui::KeyboardKey::tab` | `ERUI_KEYBOARD_KEY_TAB` | `12` | `tab` |
| `erui::KeyboardKey::key_q` | `ERUI_KEYBOARD_KEY_Q` | `13` | `key-q` |
| `erui::KeyboardKey::key_w` | `ERUI_KEYBOARD_KEY_W` | `14` | `key-w` |
| `erui::KeyboardKey::key_e` | `ERUI_KEYBOARD_KEY_E` | `15` | `key-e` |
| `erui::KeyboardKey::key_r` | `ERUI_KEYBOARD_KEY_R` | `16` | `key-r` |
| `erui::KeyboardKey::key_t` | `ERUI_KEYBOARD_KEY_T` | `17` | `key-t` |
| `erui::KeyboardKey::key_y` | `ERUI_KEYBOARD_KEY_Y` | `18` | `key-y` |
| `erui::KeyboardKey::key_u` | `ERUI_KEYBOARD_KEY_U` | `19` | `key-u` |
| `erui::KeyboardKey::key_i` | `ERUI_KEYBOARD_KEY_I` | `20` | `key-i` |
| `erui::KeyboardKey::key_o` | `ERUI_KEYBOARD_KEY_O` | `21` | `key-o` |
| `erui::KeyboardKey::key_p` | `ERUI_KEYBOARD_KEY_P` | `22` | `key-p` |
| `erui::KeyboardKey::enter` | `ERUI_KEYBOARD_KEY_ENTER` | `23` | `enter` |
| `erui::KeyboardKey::left_control` | `ERUI_KEYBOARD_KEY_LEFT_CONTROL` | `24` | `left-control` |
| `erui::KeyboardKey::key_a` | `ERUI_KEYBOARD_KEY_A` | `25` | `key-a` |
| `erui::KeyboardKey::key_s` | `ERUI_KEYBOARD_KEY_S` | `26` | `key-s` |
| `erui::KeyboardKey::key_d` | `ERUI_KEYBOARD_KEY_D` | `27` | `key-d` |
| `erui::KeyboardKey::key_f` | `ERUI_KEYBOARD_KEY_F` | `28` | `key-f` |
| `erui::KeyboardKey::key_g` | `ERUI_KEYBOARD_KEY_G` | `29` | `key-g` |
| `erui::KeyboardKey::key_h` | `ERUI_KEYBOARD_KEY_H` | `30` | `key-h` |
| `erui::KeyboardKey::key_j` | `ERUI_KEYBOARD_KEY_J` | `31` | `key-j` |
| `erui::KeyboardKey::key_k` | `ERUI_KEYBOARD_KEY_K` | `32` | `key-k` |
| `erui::KeyboardKey::key_l` | `ERUI_KEYBOARD_KEY_L` | `33` | `key-l` |
| `erui::KeyboardKey::left_shift` | `ERUI_KEYBOARD_KEY_LEFT_SHIFT` | `34` | `left-shift` |
| `erui::KeyboardKey::key_z` | `ERUI_KEYBOARD_KEY_Z` | `35` | `key-z` |
| `erui::KeyboardKey::key_x` | `ERUI_KEYBOARD_KEY_X` | `36` | `key-x` |
| `erui::KeyboardKey::key_c` | `ERUI_KEYBOARD_KEY_C` | `37` | `key-c` |
| `erui::KeyboardKey::key_v` | `ERUI_KEYBOARD_KEY_V` | `38` | `key-v` |
| `erui::KeyboardKey::key_b` | `ERUI_KEYBOARD_KEY_B` | `39` | `key-b` |
| `erui::KeyboardKey::key_n` | `ERUI_KEYBOARD_KEY_N` | `40` | `key-n` |
| `erui::KeyboardKey::key_m` | `ERUI_KEYBOARD_KEY_M` | `41` | `key-m` |
| `erui::KeyboardKey::right_shift` | `ERUI_KEYBOARD_KEY_RIGHT_SHIFT` | `42` | `right-shift` |
| `erui::KeyboardKey::left_alt` | `ERUI_KEYBOARD_KEY_LEFT_ALT` | `43` | `left-alt` |
| `erui::KeyboardKey::space` | `ERUI_KEYBOARD_KEY_SPACE` | `44` | `space` |
| `erui::KeyboardKey::numpad_7` | `ERUI_KEYBOARD_KEY_NUMPAD_7` | `45` | `numpad-7` |
| `erui::KeyboardKey::numpad_8` | `ERUI_KEYBOARD_KEY_NUMPAD_8` | `46` | `numpad-8` |
| `erui::KeyboardKey::numpad_9` | `ERUI_KEYBOARD_KEY_NUMPAD_9` | `47` | `numpad-9` |
| `erui::KeyboardKey::numpad_4` | `ERUI_KEYBOARD_KEY_NUMPAD_4` | `48` | `numpad-4` |
| `erui::KeyboardKey::numpad_5` | `ERUI_KEYBOARD_KEY_NUMPAD_5` | `49` | `numpad-5` |
| `erui::KeyboardKey::numpad_6` | `ERUI_KEYBOARD_KEY_NUMPAD_6` | `50` | `numpad-6` |
| `erui::KeyboardKey::numpad_1` | `ERUI_KEYBOARD_KEY_NUMPAD_1` | `51` | `numpad-1` |
| `erui::KeyboardKey::numpad_2` | `ERUI_KEYBOARD_KEY_NUMPAD_2` | `52` | `numpad-2` |
| `erui::KeyboardKey::numpad_3` | `ERUI_KEYBOARD_KEY_NUMPAD_3` | `53` | `numpad-3` |
| `erui::KeyboardKey::numpad_0` | `ERUI_KEYBOARD_KEY_NUMPAD_0` | `54` | `numpad-0` |
| `erui::KeyboardKey::numpad_enter` | `ERUI_KEYBOARD_KEY_NUMPAD_ENTER` | `55` | `numpad-enter` |
| `erui::KeyboardKey::right_control` | `ERUI_KEYBOARD_KEY_RIGHT_CONTROL` | `56` | `right-control` |
| `erui::KeyboardKey::right_alt` | `ERUI_KEYBOARD_KEY_RIGHT_ALT` | `57` | `right-alt` |
| `erui::KeyboardKey::home` | `ERUI_KEYBOARD_KEY_HOME` | `58` | `home` |
| `erui::KeyboardKey::arrow_up` | `ERUI_KEYBOARD_KEY_ARROW_UP` | `59` | `arrow-up` |
| `erui::KeyboardKey::page_up` | `ERUI_KEYBOARD_KEY_PAGE_UP` | `60` | `page-up` |
| `erui::KeyboardKey::arrow_left` | `ERUI_KEYBOARD_KEY_ARROW_LEFT` | `61` | `arrow-left` |
| `erui::KeyboardKey::arrow_right` | `ERUI_KEYBOARD_KEY_ARROW_RIGHT` | `62` | `arrow-right` |
| `erui::KeyboardKey::end` | `ERUI_KEYBOARD_KEY_END` | `63` | `end` |
| `erui::KeyboardKey::arrow_down` | `ERUI_KEYBOARD_KEY_ARROW_DOWN` | `64` | `arrow-down` |
| `erui::KeyboardKey::page_down` | `ERUI_KEYBOARD_KEY_PAGE_DOWN` | `65` | `page-down` |
| `erui::KeyboardKey::insert` | `ERUI_KEYBOARD_KEY_INSERT` | `66` | `insert` |
| `erui::KeyboardKey::delete_key` | `ERUI_KEYBOARD_KEY_DELETE` | `67` | `delete` |
| `erui::KeyboardKey::numpad_multiply` | `ERUI_KEYBOARD_KEY_NUMPAD_MULTIPLY` | `68` | `numpad-multiply` |
| `erui::KeyboardKey::numpad_subtract` | `ERUI_KEYBOARD_KEY_NUMPAD_SUBTRACT` | `69` | `numpad-subtract` |
| `erui::KeyboardKey::numpad_add` | `ERUI_KEYBOARD_KEY_NUMPAD_ADD` | `70` | `numpad-add` |
| `erui::KeyboardKey::numpad_decimal` | `ERUI_KEYBOARD_KEY_NUMPAD_DECIMAL` | `71` | `numpad-decimal` |
| `erui::KeyboardKey::numpad_divide` | `ERUI_KEYBOARD_KEY_NUMPAD_DIVIDE` | `72` | `numpad-divide` |

The table is exhaustive: a key not listed there is unsupported.
`ERUI_KEYBOARD_KEY_INVALID` (`0`) and `ERUI_KEYBOARD_KEY_COUNT` (`73`) are
strict-C sentinels and are never valid bound assignments.

Main-cluster and numpad keys are distinct. `home` means the dedicated Home
position, not Numpad 7; `enter` and `numpad_enter` are likewise separate.
Num Lock may affect what a physical numpad key does outside ERNativeUI, so use
the `numpad_*` value for the keypad position itself. The five numpad operators
have working defaults, labels, and runtime activation, but the current native
assignment editor cannot capture them from an unbound cell.

Notable exclusions are Escape, F1-F12, punctuation/OEM positions, Caps Lock,
Num Lock, Windows keys, and modifier chords. Left/right modifier keys listed
in the table are supported as individual assignments; a value such as Ctrl+Q
cannot be represented. Escape cancels the native assignment editor. F1-F12
activated in runtime probes, but that editor could not capture them and their
assignment rows displayed `----`, so they are not safe public values.

## Mouse buttons

| C++ value | Strict-C value | Public value | Canonical text |
|---|---|---:|---|
| `erui::MouseButton::left` | `ERUI_MOUSE_BUTTON_LEFT` | `1` | `left` |
| `erui::MouseButton::right` | `ERUI_MOUSE_BUTTON_RIGHT` | `2` | `right` |
| `erui::MouseButton::middle` | `ERUI_MOUSE_BUTTON_MIDDLE` | `3` | `middle` |
| `erui::MouseButton::button4` | `ERUI_MOUSE_BUTTON_4` | `4` | `button4` |
| `erui::MouseButton::button5` | `ERUI_MOUSE_BUTTON_5` | `5` | `button5` |
| `erui::MouseButton::wheel_up` | `ERUI_MOUSE_BUTTON_WHEEL_UP` | `6` | `wheel-up` |
| `erui::MouseButton::wheel_down` | `ERUI_MOUSE_BUTTON_WHEEL_DOWN` | `7` | `wheel-down` |

The table is exhaustive. `ERUI_MOUSE_BUTTON_INVALID` (`0`) and
`ERUI_MOUSE_BUTTON_COUNT` (`8`) are strict-C sentinels and are never valid
bound assignments. Additional physical side buttons commonly numbered 6
through 8 are excluded from API 1.1 because they were not live-tested with
suitable hardware.

## C++ construction helpers

Every helper returns an `erui::ActionInputs` value. An overload with no input
marks its devices supported and unbound.

| Helper | Result |
|---|---|
| `erui::inputs::all()` | Controller, keyboard, and mouse present-unbound |
| `erui::inputs::all(keyboard, mouse, controller)` | All three bound to the supplied values |
| `erui::inputs::controller()` | Controller present-unbound; others absent |
| `erui::inputs::controller(button)` | Controller bound; others absent |
| `erui::inputs::keyboard_mouse()` | Keyboard and mouse present-unbound; controller absent |
| `erui::inputs::keyboard_mouse(key, mouse)` | Keyboard and mouse bound; controller absent |
| `erui::inputs::keyboard()` | Keyboard present-unbound; others absent |
| `erui::inputs::keyboard(key)` | Keyboard bound; others absent |
| `erui::inputs::mouse()` | Mouse present-unbound; others absent |
| `erui::inputs::mouse(button)` | Mouse bound; others absent |

The argument order of `inputs::all` is deliberately keyboard, mouse,
controller:

```cpp
auto defaults = erui::inputs::all(
    erui::KeyboardKey::key_q,
    erui::MouseButton::button4,
    erui::ControllerButton::right_trigger);
```

Use the fluent form for any other combination or when explicit device names
are clearer:

```cpp
auto defaults = erui::ActionInputs{}
    .mouse()
    .keyboard(erui::KeyboardKey::key_f)
    .controller(erui::ControllerButton::face_north);
```

`erui::ActionInputs::all()` is equivalent to `erui::inputs::all()`.

The fluent member overloads are:

| Member | Effect on that slot |
|---|---|
| `.controller()` | Present-unbound |
| `.controller(ControllerButton)` | Bound to the supplied controller button |
| `.keyboard()` | Present-unbound |
| `.keyboard(KeyboardKey)` | Bound to the supplied keyboard key |
| `.mouse()` | Present-unbound |
| `.mouse(MouseButton)` | Bound to the supplied mouse button |

Each call updates only its own slot, returns `ActionInputs&`, and may therefore
be chained in any order. Calling another overload for the same device replaces
that slot's earlier state.

## Canonical text format

The codec is header-only. It neither loads nor calls `ERNativeUI.dll`, so mods
may use it with ERNativeUI Storage or with any other configuration backend.

Its grammar is:

```text
document         = "" | field ("," field){0,2}
field            = controller | keyboard | mouse
controller       = "controller:" ("unbound" | controller-name)
keyboard         = "keyboard:"   ("unbound" | keyboard-name)
mouse            = "mouse:"      ("unbound" | mouse-name)
```

The names are the exact canonical-text cells in the tables above. Parsing is
ASCII, case-sensitive, and counted; no whitespace, embedded NUL, duplicate
device, unknown name, extra separator, or alternate spelling is accepted.
Fields may be parsed in any order, but formatting always emits present slots
in `controller`, `keyboard`, `mouse` order. Absent slots are omitted and
unbound slots use the literal `unbound`.

Examples:

```text
                                      all slots absent
controller:unbound                    controller supported, not assigned
keyboard:key-q,mouse:unbound          keyboard bound, mouse explicitly clear
controller:right-trigger,keyboard:key-q,mouse:button4
```

The longest canonical payload is exactly
`ERUI_ACTION_INPUTS_TEXT_MAX_BYTES` (`67`) bytes, excluding any terminator:

```text
controller:right-shoulder,keyboard:numpad-multiply,mouse:wheel-down
```

### C++ codec

The explicit overloads preserve the output on failure and return the exact
`ERUI_Result`:

```cpp
std::string text;
ERUI_Result result = erui::format_action_inputs(defaults, text);

erui::ActionInputs parsed;
result = erui::parse_action_inputs(text, parsed);
```

The one-argument convenience overloads return `std::optional` and collapse
all failures to `std::nullopt`:

```cpp
auto text = erui::format_action_inputs(defaults);
auto parsed = text ? erui::parse_action_inputs(*text) : std::nullopt;
```

Formatting an invalid `ActionInputs` returns `ERUI_INVALID_ARGUMENT`.
Allocation failure in the C++ string wrapper returns `ERUI_OUT_OF_MEMORY`,
and an unexpected wrapper failure returns `ERUI_INTERNAL_ERROR`. Malformed or
oversized parsed text returns `ERUI_STORAGE_FORMAT_ERROR`; a C++ string view
too large for the strict-C counted view returns `ERUI_INVALID_ARGUMENT`.

### Strict-C representation

Zero-initialize the fixed-layout value, set `size` to its exact complete size,
and leave `flags` and `reserved` zero:

```c
ERUI_ActionInputs inputs = {0};
inputs.size = (uint32_t)sizeof(inputs);

inputs.controller.state = ERUI_INPUT_SLOT_UNBOUND;
inputs.controller.input = ERUI_CONTROLLER_BUTTON_INVALID;

inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
inputs.keyboard.input = ERUI_KEYBOARD_KEY_Q;
/* inputs.mouse remains ABSENT because its zero state is ABSENT. */
```

For absent or unbound slots, `input` must be the matching zero `INVALID`
sentinel. A bound slot must contain a value greater than `INVALID` and less
than `COUNT`. `ERUI_ActionInputs.size` must equal
`sizeof(ERUI_ActionInputs)`; unlike extensible API descriptors, it is not a
prefix size.

Format with the standard two-call length query. Output is counted and never
NUL-terminated:

```c
uint32_t required = 0;
ERUI_Result result = ERUI_FormatActionInputs(
    &inputs, NULL, 0, &required);

char output[ERUI_ACTION_INPUTS_TEXT_MAX_BYTES + 1];
if (result == ERUI_OK && required <= ERUI_ACTION_INPUTS_TEXT_MAX_BYTES) {
    uint32_t written = 0;
    result = ERUI_FormatActionInputs(
        &inputs, output, required, &written);
    if (result == ERUI_OK) output[written] = '\0'; /* Client-added terminator. */
}
```

A short output buffer returns `ERUI_BUFFER_TOO_SMALL`, reports the required
length, and leaves the buffer unchanged. Other argument errors return
`ERUI_INVALID_ARGUMENT` and leave the buffer unchanged; when `out_length` is
available it is set to zero. Passing a null output with nonzero capacity is
invalid.

Parse a counted `ERUI_StringView`; the caller does not initialize the output:

```c
const char encoded[] = "mouse:button5,keyboard:key-q";
ERUI_StringView view = {
    encoded,
    (uint32_t)(sizeof(encoded) - 1),
    0
};
ERUI_ActionInputs parsed;
ERUI_Result result = ERUI_ParseActionInputs(&view, &parsed);
```

An empty view, including `{NULL, 0, 0}`, succeeds and produces three absent
slots. Invalid view arguments return `ERUI_INVALID_ARGUMENT`; malformed text,
unknown values, duplicates, whitespace, embedded NUL, or more than 67 bytes
return `ERUI_STORAGE_FORMAT_ERROR`. `out_inputs` remains byte-for-byte
unchanged on every failure and is completely initialized on success.

The normative declarations and codec implementation are in
[`erui.h`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h). The corresponding C++17 value
types and helpers are in
[`ERNativeUI.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp).
