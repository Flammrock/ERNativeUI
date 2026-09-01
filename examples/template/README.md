# ERNativeUI client-mod template

This is a buildable starting point for a real client mod. It registers native
settings through the public C ABI via the header-only C++17 wrapper; it never
links an ERNativeUI import library and never installs game hooks itself.

## Rename checklist

1. Rename the directory and `MyERNativeUIMod` CMake target.
2. Replace `com.example.my-elden-ring-mod` with a stable, globally unique
   reverse-DNS provider ID. Never change that ID between ordinary releases.
3. Replace `My Mod`, the sample rows, state, and callbacks.
4. Keep the module handle captured by `DllMain` and register on a worker after
   `DllMain` returns.
5. Keep every callback `noexcept`, short, and valid until process exit.
6. Do not hot-unload a committed provider; ERNativeUI pins it deliberately.

## Localization

Call `erui::query_game_language()` before registration so the provider display
name can be localized too. The returned `LanguageInfo` owns its data:

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

First install ERNativeUI, then configure this directory independently:

```bat
cmake --install ..\..\build --config Release --prefix C:\sdk\ERNativeUI
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\sdk\ERNativeUI
cmake --build build --config Release
```

`find_package(ERNativeUI CONFIG REQUIRED)` supplies `ERNativeUI::SDK`, which
contains headers and a C++17 compile requirement only. A MinGW-w64 client may
use the same installed package; the host itself remains an MSVC-ABI binary.

## Deploy

Copy the renamed DLL beside your other Mod Engine 2 client mods and list the
canonical host first:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\MyMod\\MyERNativeUIMod.dll",
]
```

Do not bundle a private `ERNativeUI.dll` in every mod. Declare the canonical
host as a dependency. If the host is missing, the template logs a registration
error and the rest of your mod can continue.

## State and callback rules

- The host copies all text/options and owns toggle, slider, inline-choice, and
  popup-choice bytes plus TextInput strings. Choice indices and callbacks are
  always zero-based.
- TextInput is an additive API 1.1 capability. Gate its builder with
  `menu.supports(erui::Capability::text_input)` so the same client can retain
  its API 1.0 rows when an older host is installed.
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
- A `Page` is a builder handle. Do not keep using it after `register_menu`
  returns; escaped handles become safely invalid.
- Row-callback `user_data` remains client-owned and must outlive the process.
- Page-title formatters run during startup compilation, not when a player
  opens the page. Their borrowed context and output buffer are valid only for
  one call; callback code and any `user_data` must remain valid until
  registration/commit returns.

See the [API guide](../../docs/API.md),
[ABI contract](../../docs/ABI_STABILITY.md), and
[architecture notes](../../docs/ARCHITECTURE.md) before publishing a client.
The [page-presentation guide](../../docs/PAGE_PRESENTATION.md) explains the
default `Title (n/t)` behavior and optional formatter contract.
