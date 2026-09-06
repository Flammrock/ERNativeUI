# Settings controls

Every settings row belongs to a `Page` during provider registration.
ERNativeUI copies its visible text, owns the canonical UI value for
value-bearing controls after commit, and gives client mods opaque row handles
rather than game pointers. Buttons and Submenus intentionally own no value.

This section documents every settings control in the current C++17 wrapper.
A [Submenu](submenu.md) is included because the player interacts with its
action row like another control, although it creates a child `Page` instead of
owning a value.

## Start with one isolated control

Every control page begins with the smallest useful call for that control. The
examples receive an `erui::Page` explicitly so it is always clear where the row
is added; the surrounding connection and menu setup is explained once in the
[first-mod guide](../../getting-started/first-mod.md).

The screenshots come from the buildable
[Showcase Screenshot Helper](../../../examples/showcase_screenshot_helper/README.md).
It places one documented control inside an otherwise empty Submenu so built-in
Elden Ring rows do not obscure the result. Its INI selects the control at
startup:

```ini
[showcase]
mode=toggle
```

Restart the game, then open **System -> Game Options -> Toggle Showcase**.
Change `mode` to the value named by another control page to reproduce that
page instead. Each guide later adds any state, callbacks, and error handling
that a real mod commonly needs.

![ERNativeUI toggles, a slider, buttons, and submenu rows integrated into Game Options](../../assets/images/controls/overview/game-options-controls-overview.png)

*After learning one isolated row at a time, controls can be combined on a real
page like this larger integration showcase.*

## Choose a control

| Order | Control | What the player does | Stored value | API |
|---:|---|---|---|---|
| 1 | [Button](button.md) | Confirms one action | None | 1.0 |
| 2 | [Submenu](submenu.md) | Opens a provider-owned child page | None; returns a `Page` | 1.0 |
| 3 | [Toggle](toggle.md) | Chooses off or on | `0` or `1` | 1.0 |
| 4 | [Slider](slider.md) | Adjusts a bounded integer | `std::uint8_t` | 1.0 |
| 5 | [Inline Choice](inline-choice.md) | Cycles labels with left/right | Zero-based index | 1.0 |
| 6 | [Popup Choice](popup-choice.md) | Selects from a native list | Zero-based index | 1.0 |
| 7 | [TextInput](text-input.md) | Opens native text editing | UTF-16 string | 1.1 |
| 8 | [ColorPicker](color-picker.md) | Opens the native RGB editor | Three byte channels | 1.1 |

Use an inline choice for a short list that is comfortable to cycle through.
Use a popup choice when seeing all labels before confirming is more useful.
Use a button when the row performs work but does not itself own a setting.

## Shared control concepts

- [Availability and capabilities](../availability.md) explains the Availability
  table found on every control page and how to check host support.
- [Enabled controls](options/enabled.md) explains which controls can be disabled, why
  some remain visible while others are omitted, and why the state is fixed at
  registration time.

## Rules shared by every control

- `label` is required and must not be empty; `help` may be empty.
- Visible strings are UTF-16 `std::wstring_view` values. The host copies them
  synchronously during the builder call. An ordinary visible field accepts at
  most 4,096 valid UTF-16 code units; embedded NUL and invalid surrogate pairs
  are rejected.
- Builder calls happen only inside `Connection::register_menu`. A failed call
  poisons and aborts the private draft instead of publishing a partial menu.
- Callbacks and externally supplied `user_data` must remain valid until process
  exit after a successful commit. Never hot-unload a committed client DLL.
- Every callback must be `noexcept`. Catch allocation, persistence, and other
  failures inside the callback.
- A returned `RowHandle` is provider-owned and type-specific. Retain it only
  when later runtime getters or setters need it. `add_submenu` instead returns
  a child `Page` builder that is valid only during registration.
- Declare logical rows without assuming how many fit on screen. ERNativeUI
  paginates from the capacity of the live native page.

Button callbacks run synchronously on Elden Ring's UI thread. Value-change,
TextInput, and ColorPicker callbacks run on the ERNativeUI worker. Keep all of
them short and synchronize state shared with other threads.

## Reading and changing committed values

Keep the successful `Registration` if code must interact with rows later:

```cpp
erui::Registration registration = registration_result.value();

std::uint8_t number{};
const ERUI_Result value_read = registration.get_value(slider_row, number);
const ERUI_Result value_write = registration.set_value(slider_row, 75);

std::wstring text;
const ERUI_Result text_read = registration.get_text(text_row, text);
const ERUI_Result text_write = registration.set_text(text_row, L"Melina");

erui::Color color{};
const ERUI_Result color_read = registration.get_color(color_row, color);
const ERUI_Result color_write =
    registration.set_color(color_row, {90, 120, 180});
```

Check each `ERUI_Result` against `ERUI_OK` before using a read value or
assuming that a write was accepted. The distinct names above make every
operation visible; real code may return early after the first error.

`get_value` and `set_value` support Toggle, Slider, Inline Choice, and Popup
Choice. TextInput and ColorPicker use their typed methods. Programmatic writes
are silent: they update canonical state and presentation but do not pretend the
player made a change or invoke a change callback.

Destroying the C++ `Registration` value only discards this client-side access
handle. It does not unregister the committed provider.

## Presentation assets

Most controls use stock native widgets. The optional GFX assets provide the
matching left label on some action rows and richer TextInput and ColorPicker
appearances. Their controls remain functional without those assets; see
[Presentation and optional GFX assets](../presentation-and-gfx.md).

For the complete ownership, limits, and error contract, see
[Lifecycle, errors, and limits](../../reference/lifecycle-errors-and-limits.md).

Next: [Button](button.md)
