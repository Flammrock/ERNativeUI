# Enabled controls

Some ERNativeUI controls accept an `enabled` value while the menu is being
built. Depending on the native widget, it controls either whether the row is
included at all or whether the player may interact with a visible row.

The registration-time value is copied when the provider commits. API 1.4 can
later change interaction and disabled appearance for a retained, materialized
ordinary row through `row.edit()`, but it cannot resurrect a Button,
ColorPicker, or Submenu that `enabled = false` caused the construction path to
omit.

## What `false` looks like

Elden Ring does not provide the same disabled presentation for every native
widget, so `false` has two possible results:

| Control | Result when `enabled` is `false` | Uses a page slot? |
|---|---|---:|
| Button | The row is omitted | No |
| Submenu | The navigation row is omitted, so its child page is unreachable | No |
| ColorPicker | The row is omitted | No |
| Toggle | The row remains visible in Elden Ring's native disabled state | Yes |
| Slider | The row remains visible in Elden Ring's native disabled state | Yes |

Inline Choice, Popup Choice, and TextInput do not expose an `enabled` option.
When one of those controls should not exist, conditionally skip its `add_*`
call while building the menu.

## Example: show a read-only Slider

Suppose a Slider displays a setting owned by an optional backend. If that
backend is absent, keeping the value visible can be more helpful than hiding
it:

```cpp
#include <ernativeui/ERNativeUI.hpp>

erui::SliderRow add_strength_slider(
    erui::Page page,
    bool backend_available) noexcept
{
    erui::SliderOptions options{};
    options.minimum = 0;
    options.maximum = 100;
    options.step = 5;
    options.initial_value = 50;
    options.enabled = backend_available;

    return page.add_slider(
        L"Effect Strength",
        backend_available
            ? L"Adjust the optional effect."
            : L"Install the optional backend to change this value.",
        options);
}
```

When `backend_available` is `false`, the Slider is still visible but the
player cannot change it.

## Example: conditionally include a Button

A Button has no native greyed-out appearance. Passing `false` therefore
removes it entirely:

```cpp
#include <ernativeui/ERNativeUI.hpp>

void rebuild_cache() noexcept
{
    // Perform one short action or queue longer work.
}

erui::ButtonRow add_rebuild_button(
    erui::Page page,
    bool cache_supported) noexcept
{
    return page.add_button<&rebuild_cache>(
        L"Rebuild Cache",
        L"Rebuild this mod's cache.",
        cache_supported);
}
```

When `cache_supported` is `false`, the Button is not rendered and does not
consume a pagination slot. Its callback must still be a valid function because
ERNativeUI validates the complete Button declaration during registration.

## When interaction can change later

Use `enabled` for the row's initial construction state. Changing the original
C++ variable later does nothing because ERNativeUI copied it.

For a condition that changes at runtime, retain the typed row and commit its
availability explicitly:

```cpp
ERUI_Result result = strength_row.edit()
    .interactive(backend_available)
    .availability_appearance(
        erui::AvailabilityAppearance::follow_interactivity)
    .commit();
```

Here `backend_available` is an ordinary client-owned `bool`. The appearance
call is optional: `follow_interactivity` is already the default, while
`normal` and `disabled` deliberately decouple presentation from interaction.
The row must have been materialized during registration. See
[Enable or disable rows at runtime](../../row-updates/availability.md) for the
complete capability checks, supported families, and focus behavior.

Programmatic value methods change a control's value, not its interaction
state. A callback should still validate any gameplay precondition that may
change between presentation and activation.

[Controls index](../README.md) |
[Capabilities](../../capabilities.md) |
[Button](../button.md)
