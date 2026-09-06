# Tarnished UI Showcase

`TarnishedUIShowcase.dll` is the shipped integration example. It demonstrates:

- runtime localization for all 15 language identifiers currently reported by
  Elden Ring's Steamworks interface, with English fallback;
- one explicit API 1.1 connection reused for localization and registration;
- host-owned toggle and slider values;
- value and button callbacks;
- a dedicated choice submenu comparing one inline row with independent native
  popup lists containing two, four, and eight options;
- a programmatic `Registration::set_value` update;
- a capability-gated TextInput submenu with the default 16-unit limit and an
  explicit 35-UTF-16-unit field;
- a nested logical page with 32 generated action rows;
- automatic Next/Previous pagination through Elden Ring's native page stack;
- the default native localized-OK alert;
- a dedicated popup submenu covering every bottom/center placement and
  dismiss-only, OK, CANCEL, YES, NO, OK+CANCEL, and YES+NO button layout;
- asynchronous popup completion logging with result and response details;
- a custom outer title and per-slice title formatter;
- a native alert action registered through the public BuiltinPage API on Game
  Options, Camera Options, Display, Sound, Network, Keyboard/Mouse Settings,
  and Graphics;
- production input-action sections demonstrating all-device,
  keyboard/mouse-only, and controller-only defaults, with sections appearing
  only in the native binding screens their actions support;
- stable nonlocalized action IDs, process-lifetime callbacks,
  `ActionActivation` device-mask diagnostics, and explicit provider storage.

The showcase declares semantic defaults, then loads optional overrides from
`mods/tarnished-showcase/config.ini` beside
`ERNativeUI.dll`. Its provider-wide callback explicitly applies player
assignment events in memory and saves the document. This persistence is
showcase policy, not automatic host behavior. The callbacks are global
released-to-pressed actions and do not consume the underlying game input, so
use neutral test inputs when exercising them.

Build it with the full project or independently against an installed SDK:

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\sdk\ERNativeUI
cmake --build build --config Release
```

When copying the full staged tree to `mod\ERNativeUI`, load it after the host:

```toml
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\TarnishedUIShowcase.dll",
]
```

The showcase is intentionally a demonstration rather than a reusable mod.
Use [../template](../template) as the starting point for your own project.
