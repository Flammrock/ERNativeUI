# Install ERNativeUI as a player

ERNativeUI is a shared dependency for Elden Ring mods that use its native menu
API. Install one copy of the host, then load any compatible client mods after
it.

## Requirements

- Elden Ring for Windows x64.
- Offline play with Easy Anti-Cheat disabled.
- Mod Engine 2 or another compatible DLL loader.
- An ERNativeUI release that explicitly supports the installed Elden Ring
  executable.

Check [GitHub Releases](https://github.com/Flammrock/ERNativeUI/releases) after
an Elden Ring update. An unchanged public API does not make old native
addresses safe on a new executable.

## 1. Download the package

Choose one source:

- [Nexus Mods](https://www.nexusmods.com/eldenring/mods/10767) provides the
  compact player package.
- [GitHub Releases](https://github.com/Flammrock/ERNativeUI/releases/latest)
  provides `ERNativeUI-X.Y.Z-windows-x64.zip`, which also contains the SDK,
  documentation, examples, and tools.

When using the GitHub package, the runtime files are inside its `bin`
directory. Mod authors should keep the complete extracted SDK in a stable
development directory and copy only the required runtime files into Mod
Engine 2.

## 2. Place the runtime files

Create an ERNativeUI directory inside the Mod Engine 2 `mod` directory:

```text
<mod-engine-2>\
|-- config_eldenring.toml
`-- mod\
    `-- ERNativeUI\
        |-- ERNativeUI.dll
        |-- ERNativeUI.ini
        |-- locales\
        |-- menu\
        `-- examples\
```

For the Nexus package, extract its contents into `mod\ERNativeUI`. For the
GitHub package, copy the required contents of `bin` there.

- `ERNativeUI.dll` is required.
- `ERNativeUI.ini` is optional; the DLL creates its embedded default when it
  is missing.
- `locales` contains translations for host-owned navigation text.
- `menu` contains optional GFX presentation assets.
- `examples` contains demonstrations and is not required by ordinary client
  mods.

## 3. Load the host and client mods

List the shared host before ordinary ERNativeUI client DLLs in the existing
`external_dlls` array:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\SomeClientMod\\SomeClientMod.dll"
]
```

Preserve every DLL already required by the player's setup. Do not install a
private ERNativeUI host for each client mod: one canonical `ERNativeUI.dll`
serves all compatible clients in the process.

To try the complete demonstration, add this optional entry after the host:

```toml
"mod\\ERNativeUI\\examples\\TarnishedUIShowcase.dll"
```

The other example DLLs are development aids. Do not load them in a normal
player setup unless their demonstrations are wanted.

## 4. Enable the optional GFX presentation

Core menu behavior has native fallbacks. The bundled loose GFX files add
expanded settings-page capacity and the enhanced TextInput and ColorPicker
presentation.

To enable them, add the ERNativeUI directory to the existing Mod Engine 2
mod-loader list:

```toml
[extension.mod_loader]
enabled = true
mods = [
    { enabled = true, name = "ERNativeUI", path = "mod\\ERNativeUI" },
]
```

Preserve other entries already present in that list. Mod Engine 2 cannot merge
two binary replacements for the same GFX path. Read
[Presentation and optional GFX](../guides/presentation-and-gfx.md) before
combining ERNativeUI with another mod that replaces the same files.

## 5. Runtime configuration

`ERNativeUI.ini` belongs beside `ERNativeUI.dll`. A missing file is generated
from the embedded default; an existing file is never overwritten.

Important defaults are:

```ini
[Logging]
EnableLog = 0

[Runtime]
RegistrationQuietMs = 750
RegistrationMaxWaitMs = 5000
SteamLanguageWaitMs = 5000
InjectionCooldownMs = 250

[Diagnostics]
EnableDiagnostics = 0
```

Logging is disabled for normal play. When `EnableLog = 1`, the host writes
`ERNativeUI.log` beside the DLL. Enable logging and diagnostics only for a
bounded troubleshooting session; with logging disabled, the host does not
create, truncate, append to, or flush a log file.

Locale files use Steam's game-language identifiers and belong in `locales`
beside the host. Missing files and missing keys fall back to embedded English.

## 6. Verify the installation

1. Launch Elden Ring through Mod Engine 2 with Easy Anti-Cheat disabled.
2. Open a Configuration page used by an installed ERNativeUI client mod.
3. Confirm that the client's rows appear and remain responsive with controller,
   keyboard, and mouse input as appropriate.
4. If the optional showcase is loaded, open **System > Game Options** and
   confirm that **Tarnished UI Showcase** appears.

If no ERNativeUI client mod or example is loaded, the host has no provider
content to display.

## Compatibility and troubleshooting

- For Solid Uncapper, follow its exact DLL order and tested matrix in
  [Game and mod compatibility](../compatibility.md#solid-uncapper).
- Seamless Co-op 2.0.1 was live-tested with Elden Ring 2.7.0.0, with no
  incompatibility observed for that exact combination. See
  [Game and mod compatibility](../compatibility.md#seamless-co-op).
- Shadow of the Erdtree remains explicitly unverified until its live test
  matrix is completed.
- Host and committed client DLLs must remain loaded until process exit.
- If another mod supplies the same loose GFX paths, the configured mod-loader
  order determines which complete file is visible; the edits are not merged.

For failures, read [Common errors](common-errors.md). A useful report includes
the ERNativeUI release, Elden Ring executable version, DLL order, other loaded
mods, and the first failed address or hook line from a temporary diagnostic
log.

Return to the [documentation home](../README.md) or the
[main project page](../../README.md).
