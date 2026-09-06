# ERNativeUI client-mod template

This is a buildable starting point for a real client mod. It registers native
settings through the public C ABI via the header-only C++17 wrapper; it never
links an ERNativeUI import library and never installs game hooks itself.

## Rename checklist

1. Rename the directory and `MyERNativeUIMod` CMake target.
2. Replace `my-elden-ring-mod` with a stable, case-sensitive ID for your mod.
   Use 1..255 ASCII letters, digits, `.`, `_`, or `-`; keep it distinct from
   other loaded providers, and never change or localize it between ordinary
   releases.
3. Replace `My Mod`, the sample rows, state, and callbacks.
4. Keep the module handle captured by `DllMain` and register on a worker after
   `DllMain` returns.
5. Keep every callback `noexcept`, short, and valid until process exit.
6. Do not hot-unload a committed provider; ERNativeUI pins it deliberately.

## Localization

Call `erui::connect()` once, then query
`connection.value().game_language()` before registration so the provider
display name can be localized too. The returned `LanguageInfo` owns its data:

- `available()` says whether Steam supplied a language;
- `known` provides ERNativeUI's convenient Elden Ring language enum;
- `identifier` preserves Steam's exact UTF-8 token, including future or
  community identifiers that map to `GameLanguage::unknown`.

The template translates its first row into French as a compact pattern. Expand
`text_for()` with your translations and keep English as the fallback. The
[Localized Greeting](../localized_greeting/README.md) example covers all
currently reported Elden Ring Steam languages in a minimal DLL.

The comments in [mod_main.cpp](mod_main.cpp) explain each lifetime and thread
rule beside the relevant code.

## What the template demonstrates

The template is a broad starting point, not an input-bindings example. Its
commented source demonstrates:

- connecting and registering safely outside `DllMain`;
- basic rows, a submenu, and placement on a supported built-in page;
- localized display text with an English fallback;
- a native alert and safe callback lifetimes;
- TextInput and ColorPicker state; and
- one optional input action with explicit persistence.

Keep the pieces your mod needs and delete the sample features it does not.
The focused guides explain each feature without requiring you to understand
the entire template first.

## Optional input action and storage

The source includes one input action so a new project has a working example of
Button Settings and Keyboard/Mouse Settings integration. It also shows how a
mod may explicitly load and save that action through provider storage. Neither
feature is required to register an ordinary menu.

See [Input bindings](../../docs/guides/input-bindings.md) for action IDs,
device slots, activation behavior, and assignment callbacks. See
[Provider storage](../../docs/guides/storage.md) for the separate INI service,
typed values, paths, and its explicit `load()`/`save()` lifecycle. The detailed
comments remain beside the corresponding code in [mod_main.cpp](mod_main.cpp).

If your mod already owns a configuration backend, keep using it. The
`ActionInputs` text codec is header-only and can serialize assignments without
loading or connecting to `ERNativeUI.dll`; provider storage itself is a
host-backed API 1.1 service.

## Build inside the ERNativeUI tree

The top-level build includes this target automatically:

```bat
cmake -S ..\.. -B ..\..\build -A x64
cmake --build ..\..\build --config Release --target MyERNativeUIMod
```

The DLL is staged at:

```text
build\deploy\Release\examples\MyERNativeUIMod.dll
```

## Build against an installed SDK

First obtain an SDK prefix such as `C:\sdk\ERNativeUI` in one of two ways:

- extract the Windows release archive so that `include`, `lib`, and `bin` are
  direct children of that directory; or
- configure and build the full ERNativeUI source tree, then install that build.

For the second option, the following install command assumes that the
top-level `build` directory was already created by the commands in the previous
section:

```bat
cmake --install ..\..\build --config Release --prefix C:\sdk\ERNativeUI
```

Then configure this template independently against either SDK prefix:

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\sdk\ERNativeUI
cmake --build build --config Release
```

`find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)` supplies `ERNativeUI::SDK`, which
contains headers and a C++17 compile requirement only. A MinGW-w64 client may
use the same installed package; the host itself remains an MSVC-ABI binary.

## Deploy

Copy the renamed DLL beside your other Mod Engine 2 client mods and list the
canonical host before this client:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\MyMod\\MyERNativeUIMod.dll",
]
```

Do not bundle a private `ERNativeUI.dll` in every mod. Declare the canonical
host as a dependency. If the host is missing or incompatible, the template
logs a connection error and the rest of your mod can continue.
When Solid Uncapper is installed, place it before the host as described in the
[compatibility instructions](../../docs/compatibility.md#solid-uncapper).

## State and callback rules

- The host copies all text/options and owns toggle, slider, inline-choice, and
  popup-choice bytes, TextInput strings, and ColorPicker RGB values. Choice
  indices and callbacks are always zero-based.
- The current C++ wrapper requests the exact API 1.1 table. A host exposing
  only API 1.0 is rejected with a clear connection error instead of silently
  reducing the features available to a newly built client.
- Gate TextInput's builder with
  `menu.supports(erui::Capability::text_input)` to document the row's feature
  dependency at its call site.
- `TextInputOptions::maximum_length` defaults to 16. At the raw C boundary,
  zero also selects 16; explicit values must be 1 through 35 UTF-16 code
  units. It is fixed when the row is registered.
  A confirmed changed value arrives as a borrowed `TextInputChange::value`;
  copy it before the callback returns.
- `Registration::set_text` copies a programmatic value without invoking the
  player-change callback. `Registration::get_text` copies the current value
  into a caller-owned `std::wstring`; both return an `ERUI_Result`.
- Native display changes are applied at the next UI frame. If `set_text` runs
  while the player is editing, Cancel keeps the programmatic value and Confirm
  replaces it with the player's confirmed value.
- ColorPicker is also an API 1.1 capability. Gate it with
  `menu.supports(erui::Capability::color_picker)` before calling
  `add_color_picker`. `erui::Color` exposes named `red`, `green`, and `blue`
  bytes; the native editor is opaque RGB and no public alpha or packed game
  value is exposed.
- A changed accepted color arrives by value in `ColorPickerChange`. Cancel,
  same-value confirmation, and `Registration::set_color` do not notify the
  callback. `Registration::get_color` copies the current canonical value.
  Color change callbacks run on the ERNativeUI worker.
- ERNativeUI permits one native color-editor modal process-wide, while every
  logical row retains independent state. Without the optional bracketed-swatch
  GFX presentation, the row remains a functional action row.
- `menu.root()` and `menu.page(erui::BuiltinPage::game_options)` are the same
  provider-owned Game Options destination. API 1.1 also supports Camera,
  Display, Sound, Network, Keyboard/Mouse, and Graphics through `menu.page`;
  every returned builder accepts the ordinary row and submenu methods.
- Built-in top-level headings belong to Elden Ring. Do not call
  `set_presentation` on a built-in page; use it only on your own submenus.
  Controller Settings is deliberately absent from `BuiltinPage`: add actions
  to its native Button Settings and Keyboard/Mouse Settings screens through
  `menu.input_bindings()`, `InputBindings::add_section`, and
  `InputSection::add_action` instead.
- Input-action callbacks use global rising edges on the host worker. They do
  not consume Elden Ring input, and duplicate physical assignments are
  deliberate. Use stable nonlocalized action IDs. Programmatic binding changes
  are silent; player assignment/clear events are reported only through the
  optional provider-wide assignments callback.
- Storage is an explicit in-memory document with disk backing. Call `load`
  before reads, mutate with `set`/`erase`/`apply`, and call `save` deliberately.
  ERNativeUI never saves a client configuration implicitly.
- Value callbacks run on the ERNativeUI worker; button callbacks run on the
  Elden Ring UI thread.
- Alert completion callbacks run asynchronously on the ERNativeUI worker and
  their `user_data` must remain valid until completion. The completion result
  reports transport success separately from `AlertResponse`: one-button
  layouts normalize successful Back to `primary`, two-button layouts map the
  right button and Back to `secondary`, and `dismiss_only` reports
  `dismissed`.
- `Registration::set_value` is asynchronous toward the native byte. Avoid
  racing frequent programmatic writes against player input; the last write
  observed by the worker wins.
- A `Page` is a builder handle. Do not keep using it after
  `Connection::register_menu` returns; escaped handles become safely invalid.
- Row-callback `user_data` remains client-owned and must outlive the process.
- Page-title formatters run during startup compilation, not when a player
  opens the page. Their borrowed context and output buffer are valid only for
  one call; callback code and any `user_data` must remain valid until
  registration/commit returns.

See the [documentation home](../../docs/README.md),
[feature status](../../docs/features.md), and
[architecture overview](../../docs/how-it-works.md) before publishing a client.
[Versioning](../../docs/versioning.md) explains why the package requirement and
runtime API negotiation are independent. The
[presentation and optional GFX guide](../../docs/guides/presentation-and-gfx.md)
explains the default `Title (n/t)` behavior and optional formatter contract.
