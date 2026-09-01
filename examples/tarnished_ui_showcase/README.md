# Tarnished UI Showcase

`TarnishedUIShowcase.dll` is the shipped integration example. It demonstrates:

- runtime localization for all 15 language identifiers currently reported by
  Elden Ring's Steamworks interface, with English fallback;
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
- a custom outer title and per-slice title formatter.

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
