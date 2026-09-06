# Availability and capabilities

Each control guide has an **Availability** table. It tells a modder when the
feature entered the public API and which capability represents it.

## Reading an Availability table

| Field | What it means |
|---|---|
| First public API | The first ERNativeUI binary API that contains the feature |
| Capability | The named feature bit advertised by the connected host DLL |
| Optional GFX | Whether bundled GFX files improve or enable the presentation |
| Runtime value | The kind of value, if any, owned by the control after commit |

The public API version and the ERNativeUI release version are different
concepts. For example, a future ERNativeUI 1.4.2 release could still expose
public API 1.1. The release identifies the project build; the public API
identifies the binary contract used by client mods.

## What a capability means

A capability says that the connected `ERNativeUI.dll` exposes the public
operation represented by that capability. For example:

```cpp
erui::Capability::button
erui::Capability::text_input
erui::Capability::color_picker
```

Check the `ConnectionResult` before reading its `Connection`, then use
`supports()` for the feature:

```cpp
#include <ernativeui/ERNativeUI.hpp>

void initialize_menu() noexcept
{
    erui::ConnectionResult connection_result = erui::connect();
    if (!connection_result) {
        return; // ERNativeUI is unavailable; leave this integration disabled.
    }

    const erui::Connection& connection = connection_result.value();
    if (!connection.supports(erui::Capability::color_picker)) {
        return; // This mod can choose a fallback here.
    }

    // Register the menu through `connection` here.
}
```

This happens before registering the menu, so the mod can choose a fallback or
disable only its optional ERNativeUI integration. Do not call `value()` after
a failed result. As explained in the
  [setup guide](../getting-started/setup.md), perform the connection from the
mod's initialization worker, not from `DllMain`.

## Where to check

The same query exists on three objects because each belongs to a different
stage:

| Object | When it exists | Typical reason to check |
|---|---|---|
| `erui::Connection` | After `erui::connect()` succeeds | Decide whether the mod can register its required features |
| `erui::Menu` | Only inside the registration builder | Add or skip one optional part of the menu |
| `erui::Registration` | After the provider commits | Guard a later runtime operation |

The calls at the builder and post-commit stages look like this:

```cpp
const bool available_while_building =
    menu.supports(erui::Capability::color_picker);

const bool available_after_commit =
    registration.supports(erui::Capability::color_picker);
```

Here, `menu` has type `erui::Menu` and `registration` has type
`erui::Registration`.

The current C++17 wrapper negotiates API 1.1 as one complete contract, so a
successful connection normally reports all API 1.1 capabilities. Explicit
checks still document a mod's requirements and keep call sites understandable
if capabilities become optional in a later API.

Prefer `supports(erui::Capability::...)` over comparing API version numbers.
The capability states the requirement directly.

## What a capability does not guarantee

A capability describes the public DLL interface. It does not guarantee that:

- an optional GFX file is installed;
- a specific Elden Ring executable is compatible with the current host;
- every native address required by the registered menu can be resolved; or
- another mod has not introduced an incompatible hook.

Those conditions are validated separately by the host. Registration and
runtime errors must still be handled, and the host log is the detailed source
when native installation fails.

## If a capability is unavailable

Do not call the corresponding builder operation. Either omit that optional
part of the menu or provide a simpler fallback:

```cpp
void add_optional_color_control(
    erui::Menu& menu,
    erui::Page page) noexcept
{
    if (!menu.supports(erui::Capability::color_picker)) {
        return;
    }

    erui::ColorPickerOptions options{};
    options.initial_value = {171, 125, 99};

    page.add_color_picker(
        L"Accent Color",
        L"Choose the accent color.",
        options);
}
```

Calling an unsupported builder operation makes the private registration draft
fail instead of publishing a partially valid menu.

[Guides index](README.md) |
[Controls index](controls/README.md) |
[Enabled controls](controls/options/enabled.md) |
[Feature overview](../features.md)
