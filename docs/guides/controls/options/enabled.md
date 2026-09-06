# Enabled controls

Some ERNativeUI controls accept an `enabled` value while the menu is being
built. Depending on the native widget, it controls either whether the row is
included at all or whether the player may interact with a visible row.

The value is fixed when the provider commits. API 1.1 cannot change a row from
enabled to disabled, or from disabled to enabled, while the game is running.

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

erui::RowHandle add_strength_slider(
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

erui::RowHandle add_rebuild_button(
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

## When availability can change later

Use `enabled` only for a condition that is known while registering the menu
and remains stable for the process. Changing the original C++ variable later
does nothing; ERNativeUI copied its value.

If a Button must remain visible while its action is temporarily unavailable,
leave it enabled and let its callback check the mod's current state. The
callback may then do nothing or show a native alert explaining why the action
cannot run. See [Native dialogs](../../native-dialogs.md).

Programmatic value methods such as `Registration::set_value()` change a
control's value, not its enabled state.

[Controls index](../README.md) |
[Availability and capabilities](../../availability.md) |
[Button](../button.md)
