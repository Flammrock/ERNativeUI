# Capabilities

## What a capability is

A capability is a named feature bit advertised by the connected
`ERNativeUI.dll`. It answers one question:

> Can this exact host, API table, game build, configuration, and validated
> native backend provide this feature?

Capabilities let a client mod negotiate host features without guessing
from the ERNativeUI release number or Elden Ring executable version. Check the
capability before using its feature, then still check the `ERUI_Result` returned
by the operation.

A capability is deliberately different from **runtime row availability**:

| Concept | Meaning |
|---|---|
| Capability | Whether the connected host exposes an API feature, such as `color_picker` or `row_visibility` |
| Runtime row availability | Whether one already-registered visible row accepts focus and input, and whether it uses normal or disabled presentation |

The capability named `row_availability` enables the runtime row-availability
API; it does not change the meaning of the word capability. See
[Enable or disable rows at runtime](row-updates/availability.md) for the row
behavior.

The C++ enum is `erui::Capability`. The strict-C API exposes the same bits as
`ERUI_CAP_*` constants in `ERUI_Api::capabilities`.

## Capability index

The capability name in the first column opens its canonical explanation on
this page.

| C++ capability | Strict-C bit | First API | Purpose |
|---|---|---:|---|
| [`erui::Capability::toggle`](#toggle) | `ERUI_CAP_TOGGLE` | 1.0 | Register native Toggle rows |
| [`erui::Capability::slider`](#slider) | `ERUI_CAP_SLIDER` | 1.0 | Register byte-valued Slider rows |
| [`erui::Capability::button`](#button) | `ERUI_CAP_BUTTON` | 1.0 | Register action Button rows |
| [`erui::Capability::submenu`](#submenu) | `ERUI_CAP_SUBMENU` | 1.0 | Register provider-owned subpages |
| [`erui::Capability::pagination`](#pagination) | `ERUI_CAP_PAGINATION` | 1.0 | Let the host create native Previous/Next pagination |
| [`erui::Capability::host_owned_values`](#host-owned-values) | `ERUI_CAP_HOST_OWNED_VALUES` | 1.0 | Read or write committed API 1.0 control values |
| [`erui::Capability::page_presentation`](#page-presentation) | `ERUI_CAP_PAGE_PRESENTATION` | 1.0 | Configure provider page titles and page-number formatting |
| [`erui::Capability::alert`](#alert) | `ERUI_CAP_ALERT` | 1.0 | Queue native modal dialogs |
| [`erui::Capability::inline_choice`](#inline-choice) | `ERUI_CAP_INLINE_CHOICE` | 1.0 | Register in-place choice rows |
| [`erui::Capability::popup_choice`](#popup-choice) | `ERUI_CAP_POPUP_CHOICE` | 1.0 | Register native popup-choice rows |
| [`erui::Capability::game_language`](#game-language) | `ERUI_CAP_GAME_LANGUAGE` | 1.0 | Query Elden Ring's effective language |
| [`erui::Capability::text_input`](#text-input) | `ERUI_CAP_TEXT_INPUT` | 1.1 | Register native text-entry rows |
| [`erui::Capability::color_picker`](#color-picker) | `ERUI_CAP_COLOR_PICKER` | 1.1 | Register native RGB color editors |
| [`erui::Capability::builtin_pages`](#built-in-pages) | `ERUI_CAP_BUILTIN_PAGES` | 1.1 | Target additional built-in settings pages |
| [`erui::Capability::input_bindings`](#input-bindings) | `ERUI_CAP_INPUT_BINDINGS` | 1.1 | Register and manage native input actions |
| [`erui::Capability::storage`](#storage) | `ERUI_CAP_STORAGE` | 1.1 | Use provider-scoped host-owned INI storage |
| [`erui::Capability::numeric_slider`](#numeric-slider) | `ERUI_CAP_NUMERIC_SLIDER` | 1.2 | Register signed or decimal sliders |
| [`erui::Capability::custom_slider`](#custom-slider) | `ERUI_CAP_CUSTOM_SLIDER` | 1.2 | Register sliders with arbitrary display labels |
| [`erui::Capability::custom_tabs`](#custom-tabs) | `ERUI_CAP_CUSTOM_TABS` | 1.3 | Register top-level System tabs |
| [`erui::Capability::title_screen_text`](#title-screen-text) | `ERUI_CAP_TITLE_SCREEN_TEXT` | 1.3 | Add status text to title-screen corners |
| [`erui::Capability::updates`](#updates) | `ERUI_CAP_UPDATES` | 1.4 | Commit runtime row text and style edits |
| [`erui::Capability::row_availability`](#row-availability) | `ERUI_CAP_ROW_AVAILABILITY` | 1.4 | Change row interaction and disabled appearance |
| [`erui::Capability::row_visibility`](#row-visibility) | `ERUI_CAP_ROW_VISIBILITY` | 1.4 | Show or hide rows and rebuild pagination |

## Reading capability requirements

Focused feature guides summarize their requirements in a compact table:

| Field | Meaning |
|---|---|
| First public API | The first ERUI binary API containing the feature |
| Required capability | The runtime feature bit that must be advertised by the connected host |
| GFX presentation | Whether optional runtime GFX completion changes how the feature looks |
| Runtime value | The canonical value, if any, owned by a committed control |

The API number identifies the binary contract in which a feature was added.
The capability says whether the connected host can provide it now. Check both
by negotiating the intended API normally and then querying the capability.

## How to check capabilities

First establish a valid connection. Do not call `value()` when
`erui::connect()` failed:

```cpp
#include <ernativeui/ERNativeUI.hpp>

void initialize_menu() noexcept
{
    erui::ConnectionResult result = erui::connect();
    if (!result) {
        return; // Leave only this optional integration disabled.
    }

    const erui::Connection& connection = result.value();
    if (!connection.supports(erui::Capability::color_picker)) {
        return; // Omit the optional control or use a client-owned fallback.
    }

    // Register the menu through `connection` here.
}
```

The same query exists at each useful lifecycle stage:

| Object | When it exists | Typical use |
|---|---|---|
| `erui::Connection` | After `erui::connect()` succeeds | Decide whether the mod can register its required features |
| `erui::Menu` | Inside the registration builder | Add or omit one optional part of the menu |
| `erui::Registration` | After the provider commits | Guard a later runtime operation |

For example:

```cpp
const bool during_registration =
    menu.supports(erui::Capability::color_picker);

const bool after_commit =
    registration.supports(erui::Capability::color_picker);
```

A strict-C client tests the corresponding bit:

```c
const int has_color_picker =
    (api->capabilities & ERUI_CAP_COLOR_PICKER) != 0;
```

Prefer a capability check over comparing API version numbers. The wrapper
requests one exact API table, but some features inside that table remain
independently gated by the validated runtime environment.

## Capability relationships

Most capabilities can be checked independently. The following combinations
are intentional:

| Operation | Required capabilities |
|---|---|
| Register a Toggle | `toggle` |
| Read or write a committed Toggle | `toggle` and `host_owned_values` |
| Read or write a committed byte Slider | `slider` and `host_owned_values` |
| Read or write a committed Inline Choice | `inline_choice` and `host_owned_values` |
| Read or write a committed Popup Choice | `popup_choice` and `host_owned_values` |
| Read or write Numeric Slider, Custom Slider, TextInput, or ColorPicker values | That row family's own capability; `host_owned_values` is not required |
| Change runtime row text or style | `updates` |
| Change runtime row interaction or disabled appearance | `updates` and `row_availability` |
| Show or hide rows and repaginate | `updates` and `row_visibility` |

`title_screen_text` is also unavailable when the player's runtime GFX
patching option is disabled. `custom_tabs`, `title_screen_text`, and the API
1.4 row-update backends may be withheld independently when their native
runtime boundary cannot be validated. Unrelated capabilities remain usable.

## API 1.0 capabilities

<a id="toggle"></a>
### `toggle`

**C++:** `erui::Capability::toggle`  
**C:** `ERUI_CAP_TOGGLE`

Registers native on/off rows. Reading or writing a committed Toggle also
requires [`host_owned_values`](#host-owned-values). When `toggle` is absent,
omit the row. See [Toggle](controls/toggle.md).

<a id="slider"></a>
### `slider`

**C++:** `erui::Capability::slider`  
**C:** `ERUI_CAP_SLIDER`

Registers byte-valued native Slider rows. Reading or writing a committed
Slider also requires [`host_owned_values`](#host-owned-values). When `slider`
is absent, omit the row. See [Slider](controls/slider.md).

<a id="button"></a>
### `button`

**C++:** `erui::Capability::button`  
**C:** `ERUI_CAP_BUTTON`

Registers an action row that invokes client code when confirmed. A Button has
no canonical value. When `button` is absent, omit the action or expose it
through a non-ERNativeUI fallback. See [Button](controls/button.md).

<a id="submenu"></a>
### `submenu`

**C++:** `erui::Capability::submenu`  
**C:** `ERUI_CAP_SUBMENU`

Registers a navigation row and provider-owned child page. The returned
`erui::Page` is a registration-time builder rather than a runtime screen
pointer. When `submenu` is absent, keep the content on another supported page
or omit it. See [Submenu](controls/submenu.md).

<a id="pagination"></a>
### `pagination`

**C++:** `erui::Capability::pagination`  
**C:** `ERUI_CAP_PAGINATION`

Lets ERNativeUI divide logical content into live native page slices and create
Previous/Next rows. Clients declare logical rows and never create pagination
rows themselves. See [Menus, pages, and pagination](menus-and-pages.md).

<a id="host-owned-values"></a>
### `host_owned_values`

**C++:** `erui::Capability::host_owned_values`  
**C:** `ERUI_CAP_HOST_OWNED_VALUES`

Enables runtime reads and programmatic writes for committed Toggle, byte
Slider, Inline Choice, and Popup Choice values. It supplements the relevant
row-family capability; it does not register those controls by itself. See
[Read or change row values at runtime](row-updates/values.md).

<a id="page-presentation"></a>
### `page_presentation`

**C++:** `erui::Capability::page_presentation`  
**C:** `ERUI_CAP_PAGE_PRESENTATION`

Enables provider submenu headings, base titles, default page-number suffixes,
and the startup title formatter. It does not grant arbitrary access to a live
Scaleform page. See [Page titles](menus-and-pages.md#page-titles).

<a id="alert"></a>
### `alert`

**C++:** `erui::Capability::alert`  
**C:** `ERUI_CAP_ALERT`

Queues native modal messages with supported button layouts, placement, and an
optional completion callback. When `alert` is absent, use the mod's own
non-native notification path or omit the optional prompt. See
[Native dialogs](native-dialogs.md).

<a id="inline-choice"></a>
### `inline_choice`

**C++:** `erui::Capability::inline_choice`  
**C:** `ERUI_CAP_INLINE_CHOICE`

Registers a choice changed in place with left/right input. Runtime index
access also requires [`host_owned_values`](#host-owned-values). See
[Inline Choice](controls/inline-choice.md).

<a id="popup-choice"></a>
### `popup_choice`

**C++:** `erui::Capability::popup_choice`  
**C:** `ERUI_CAP_POPUP_CHOICE`

Registers an action row that opens Elden Ring's native selection list.
Runtime index access also requires
[`host_owned_values`](#host-owned-values). See
[Popup Choice](controls/popup-choice.md).

<a id="game-language"></a>
### `game_language`

**C++:** `erui::Capability::game_language`  
**C:** `ERUI_CAP_GAME_LANGUAGE`

Exposes the process-wide effective Elden Ring language selected by Steam or
the host override. When absent, use the client's documented fallback language.
See [Game language and localization](localization.md).

## API 1.1 capabilities

<a id="text-input"></a>
### `text_input`

**C++:** `erui::Capability::text_input`  
**C:** `ERUI_CAP_TEXT_INPUT`

Registers native text-entry rows and provides their typed runtime text access.
The control remains functional through its documented fallback presentation
when compatible GFX completion is unavailable. See
[TextInput](controls/text-input.md).

<a id="color-picker"></a>
### `color_picker`

**C++:** `erui::Capability::color_picker`  
**C:** `ERUI_CAP_COLOR_PICKER`

Registers native RGB color-editor rows and provides typed runtime color
access. It does not expose alpha. The control retains a functional action-row
fallback when compatible GFX completion is unavailable. See
[ColorPicker](controls/color-picker.md).

<a id="built-in-pages"></a>
### `builtin_pages`

**C++:** `erui::Capability::builtin_pages`  
**C:** `ERUI_CAP_BUILTIN_PAGES`

Lets a provider target supported built-in settings destinations beyond Game
Options, including Camera, Display, Sound, Network, Keyboard/Mouse, and
Graphics. Each destination resolves and paginates independently. See
[Choose a destination](menus-and-pages.md#choose-a-destination).

<a id="input-bindings"></a>
### `input_bindings`

**C++:** `erui::Capability::input_bindings`  
**C:** `ERUI_CAP_INPUT_BINDINGS`

Registers controller, keyboard, and mouse actions in the native binding
screens and enables their runtime assignment operations and callbacks. When
absent, keep the mod's existing input configuration path. See
[Input bindings](input-bindings.md).

<a id="storage"></a>
### `storage`

**C++:** `erui::Capability::storage`  
**C:** `ERUI_CAP_STORAGE`

Provides explicit provider-scoped INI load, mutation, and atomic save
operations. Nothing is loaded or saved automatically. When absent, use the
client mod's own storage backend. See [Provider storage](storage.md).

## API 1.2 capabilities

<a id="numeric-slider"></a>
### `numeric_slider`

**C++:** `erui::Capability::numeric_slider`  
**C:** `ERUI_CAP_NUMERIC_SLIDER`

Registers signed or decimal sliders with exact base-10 values, configurable
steps, prefixes, and suffixes. Its typed runtime value access is part of this
capability and does not require `host_owned_values`. See
[Numeric Slider](controls/numeric-slider.md).

<a id="custom-slider"></a>
### `custom_slider`

**C++:** `erui::Capability::custom_slider`  
**C:** `ERUI_CAP_CUSTOM_SLIDER`

Registers sliders whose positions use arbitrary copied display labels. Its
typed runtime index access is part of this capability and does not require
`host_owned_values`. See [Custom Slider](controls/custom-slider.md).

## API 1.3 capabilities

<a id="custom-tabs"></a>
### `custom_tabs`

**C++:** `erui::Capability::custom_tabs`  
**C:** `ERUI_CAP_CUSTOM_TABS`

Registers provider-owned top-level tabs in the System menu. The capability is
advertised only when the host validates the required native tab backend for
the running game build. When absent, place the content on a supported built-in
page or omit it. See [Custom Configuration tabs](custom-tabs.md).

<a id="title-screen-text"></a>
### `title_screen_text`

**C++:** `erui::Capability::title_screen_text`  
**C:** `ERUI_CAP_TITLE_SCREEN_TEXT`

Adds copied status or version lines to an explicitly selected title-screen
corner. It requires a validated title layout and enabled runtime GFX patching.
When absent, omit the title text without disabling unrelated menu features.
See [Title-screen text](title-screen-text.md).

## API 1.4 capabilities

<a id="updates"></a>
### `updates`

**C++:** `erui::Capability::updates`  
**C:** `ERUI_CAP_UPDATES`

Enables the provider-scoped atomic update boundary, C++ `row.edit().commit()`
drafts, and runtime label, help, supported caption, and structured-text
changes. It does not replace the type-specific value getters and setters. When
absent, keep registration-time text and presentation. See
[Row updates](row-updates/README.md).

<a id="row-availability"></a>
### `row_availability`

**C++:** `erui::Capability::row_availability`  
**C:** `ERUI_CAP_ROW_AVAILABILITY`

Together with [`updates`](#updates), enables `.interactive(...)` and
`.availability_appearance(...)` for supported ordinary rows. Use it when a row
must remain visible but cannot currently be selected or changed. When absent,
keep the row's registration-time interaction state. See
[Enable or disable rows at runtime](row-updates/availability.md).

<a id="row-visibility"></a>
### `row_visibility`

**C++:** `erui::Capability::row_visibility`  
**C:** `ERUI_CAP_ROW_VISIBILITY`

Together with [`updates`](#updates), enables `.visible(...)`, live row
reconstruction, focus repair, and repagination for supported ordinary rows.
When absent, keep every registered row visible. See
[Show or hide rows at runtime](row-updates/visibility.md).

## What a capability does not guarantee

A capability does not guarantee that:

- a later call receives valid handles, values, strings, enums, or descriptors;
- the operation succeeds after runtime state changes;
- an effective settings or title GFX has a structure supported by optional
  presentation completion;
- every unrelated native integration is available; or
- another mod cannot introduce a later hook conflict.

Continue checking every `ERUI_Result`. See the central
[error-handling guide](../reference/error-handling.md) for recoverable errors,
provider-draft failure, and appropriate fallbacks.

## When a capability is unavailable

Do not call the capability-gated operation. Omit only the optional feature or
use a client-owned fallback; do not disable unrelated gameplay code.

For example:

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

Calling an unsupported builder operation causes the private registration draft
to fail rather than publishing a partially valid menu.

[Guides index](README.md) |
[Feature overview](../features.md) |
[Error handling](../reference/error-handling.md)
