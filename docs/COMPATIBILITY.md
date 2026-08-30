# Mod compatibility

## Solid Uncapper

ERNativeUI and Solid Uncapper both extend Game Options and share three native
hook sites: the hub handler, subpage handler, and native text resolver. They are
compatible when Mod Engine 2 lists Solid Uncapper first:

```toml
external_dlls = [
    "mod\\Solid Uncapper\\Solid Uncapper.dll",
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\TarnishedUIShowcase.dll"
]
```

Both DLLs initialize asynchronously, so list order does not itself establish
hook order. When Solid Uncapper is detected, ERNativeUI captures pristine game
addresses immediately, waits up to 20 seconds for the three shared entries to
change and stabilize, verifies that each supported absolute-indirect detour
targets executable code owned by `Solid Uncapper.dll`, and installs its hooks
as the next chain layer.

ERNativeUI refuses partial, foreign, or unsupported shared detours rather than
guessing. If Solid Uncapper has its in-game menu disabled and installs none of
the shared hooks, ERNativeUI continues using the captured native addresses.

The validated call order is:

```text
Elden Ring caller
`-- ERNativeUI detour
    `-- Solid Uncapper detour
        `-- original Elden Ring function
```

Compatibility was exercised with Solid Uncapper 2.3 and Elden Ring executable
2.7.0.0: both roots and subpages, values, repeated cross-navigation, ERNativeUI
pagination and dialogs, restart persistence, and ERNativeUI standalone mode.

## Seamless Co-op

Compatibility with Seamless Co-op has not been tested for this release. No
incompatibility is currently known, but this should not be interpreted as a
compatibility guarantee. Reports should include the ERNativeUI version,
Seamless Co-op version, Elden Ring executable version, DLL loading
configuration, and an ERNativeUI log captured with logging enabled.
