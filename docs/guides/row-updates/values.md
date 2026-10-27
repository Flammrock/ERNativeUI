# Read or change row values at runtime

<table>
  <tr>
    <td align="center" valign="middle">
      <a href="../capabilities.md#what-a-capability-is"><img src="../../assets/images/badges/capabilities.svg" alt="Capabilities" width="112" height="22" align="top"></a>
    </td>
    <td valign="middle">
      <a href="../capabilities.md#capability-index"><code>varies by row type</code></a>
    </td>
  </tr>
</table>

Runtime value mutation predates API 1.4. Toggle, byte Slider, and choice
values entered API 1.0; TextInput and ColorPicker followed in API 1.1; Numeric
and Custom Slider values followed in API 1.2. API 1.4 adds convenient methods
directly to retained typed row references.

If typed rows and their lifetime are new to you, read the
[row-update foundation](README.md) first. Value methods do not use
`edit().commit()`: they are direct operations on the row's canonical value.

## Typed value methods

| Row type | Read | Write | First API | Required capabilities |
|---|---|---|---:|---|
| Toggle | `get_value(bool&)` | `set_value(bool)` | 1.0 | `toggle` + `host_owned_values` |
| byte Slider | `get_value(std::uint8_t&)` | `set_value(std::uint8_t)` | 1.0 | `slider` + `host_owned_values` |
| Numeric Slider | `get_value(double&)` | `set_value(double)` | 1.2 | `numeric_slider` |
| Custom Slider | `get_index(std::uint32_t&)` | `set_index(std::uint32_t)` | 1.2 | `custom_slider` |
| Inline Choice | `get_index(std::uint8_t&)` | `set_index(std::uint8_t)` | 1.0 | `inline_choice` + `host_owned_values` |
| Popup Choice | `get_index(std::uint8_t&)` | `set_index(std::uint8_t)` | 1.0 | `popup_choice` + `host_owned_values` |
| TextInput | `get_text(std::wstring&)` | `set_text(std::wstring_view)` | 1.1 | `text_input` |
| ColorPicker | `get_color(erui::Color&)` | `set_color(erui::Color)` | 1.1 | `color_picker` |

Button and Submenu have no canonical value.

## Complete Slider example

The example below assumes normal connection setup has already produced a
valid `erui::Connection` and the mod's `erui::ProviderOptions`. See
[Create your first mod](../../getting-started/first-mod.md) for that setup.

```cpp
#include <ernativeui/ERNativeUI.hpp>

namespace {

erui::Registration g_registration;
erui::SliderRow g_intensity;

bool register_value_example(
    const erui::Connection& connection,
    const erui::ProviderOptions& provider) noexcept
{
    if (!connection.supports(erui::Capability::slider) ||
        !connection.supports(erui::Capability::host_owned_values)) {
        return false;
    }

    const auto result = connection.register_menu(
        provider,
        [](erui::Menu& menu) {
            erui::SliderOptions options{};
            options.minimum = 0;
            options.maximum = 100;
            options.step = 5;
            options.initial_value = 50;

            // The returned typed row retains the context required by its
            // get_value and set_value methods.
            g_intensity = menu.root().add_slider(
                L"Intensity",
                L"Adjust the effect intensity.",
                options);
        });

    if (!result) {
        // result.error().native_result() and result.error().message()
        // describe the registration failure.
        return false;
    }

    g_registration = result.value();
    return g_intensity.valid();
}

ERUI_Result set_high_intensity() noexcept
{
    // 75 is inside the declared 0..100 range and on the five-point step.
    return g_intensity.set_value(75);
}

ERUI_Result read_intensity(std::uint8_t& output) noexcept
{
    // output is written only when get_value returns ERUI_OK.
    return g_intensity.get_value(output);
}

} // namespace
```

Use those helpers by checking their exact result:

```cpp
const ERUI_Result write_result = set_high_intensity();
if (write_result != ERUI_OK) {
    // The canonical value did not change; log or handle write_result.
}

std::uint8_t current{};
const ERUI_Result read_result = read_intensity(current);
if (read_result == ERUI_OK) {
    // current now contains the canonical Slider value.
} else {
    // Do not use current as a retrieved value; handle read_result.
}
```

Only call typed value methods after `register_menu` succeeds. A successful
setter changes the canonical host state synchronously; the native control
catches up at its next safe UI boundary. A programmatic write deliberately
does not invoke the callback used for a player-originated change.

## Validation and normalization

- byte Slider writes clamp to the declared range, then floor to the declared
  step sequence from its minimum;
- Numeric Slider writes normalize to its exact decimal range and step;
- Custom Slider and choice indices reject out-of-range values;
- TextInput copies the supplied UTF-16 text and rejects a value beyond that
  row's configured limit instead of truncating it; and
- ColorPicker stores opaque RGB. It has no public alpha channel.

If a TextInput or ColorPicker editor is already open, a programmatic value
remains canonical through Cancel; a later changed player confirmation wins.
Hiding or disabling a row never erases its value, and reconstruction consumes
the latest canonical state.

Popup Choice `caption(...)` is presentation only. It does not change the
selected index; use `set_index(...)` for that.

## Earlier C++ source style

API 1.3 source retains a raw `erui::RowHandle` and calls the matching method
on `erui::Registration`, for example:

```cpp
registration.set_value(slider_handle, 75);
registration.set_text(text_input_handle, L"Tarnished");
registration.set_color(color_picker_handle, {171, 125, 99});
```

That public behavior remains supported. The typed API 1.4 methods preserve
the provider context and prevent calling the wrong semantic operation on a
row type at compile time.

[Row-update foundation](README.md) | [Text and style](text-and-styles.md) |
[Runtime row availability](availability.md)
