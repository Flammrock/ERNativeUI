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
  examples, tools, and a version-pinned link to the online documentation.

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
        `-- examples\
```

For the Nexus package, extract its contents into `mod\ERNativeUI`. For the
GitHub package, copy the required contents of `bin` there.

- `ERNativeUI.dll` is required.
- `ERNativeUI.ini` is optional; the DLL creates its embedded default when it
  is missing.
- `locales` contains translations for host-owned navigation text.
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

## 4. Runtime GFX presentation

There is no separate GFX installation step. By default, ERNativeUI completes
supported settings and title movies in memory while Elden Ring loads them.
This supplies expanded ordinary-page capacity, TextInput and ColorPicker
presentation, and registered API 1.3 title-screen text without a loose `menu`
directory or a Mod Engine 2 mod-loader entry.

If another mod supplies one of the same movies, ERNativeUI supports it when
the relevant layout is structurally compatible. Unrecognized layouts are left
unchanged. Read
[Runtime GFX completion and presentation](../guides/presentation-and-gfx.md)
for the exact behavior.

## 5. Runtime configuration

`ERNativeUI.ini` belongs beside `ERNativeUI.dll`. A missing file is generated
from the embedded default; an existing file is never overwritten.

Important defaults are:

```ini
[Logging]
EnableLog = 0

[Localization]
Language = auto

[Runtime]
RegistrationQuietMs = 750
RegistrationMaxWaitMs = 5000
SteamLanguageWaitMs = 5000
InjectionCooldownMs = 250

[GFX]
EnableRuntimePatching = 1

[Diagnostics]
EnableDiagnostics = 0
```

Logging is disabled for normal play. When `EnableLog = 1`, the host writes
`ERNativeUI.log` beside the DLL. Enable logging and diagnostics only for a
bounded troubleshooting session; with logging disabled, the host does not
create, truncate, append to, or flush a log file.

`[GFX] EnableRuntimePatching = 1` is the normal default; an existing INI that
omits this new key also uses `1`. Set it to `0` only when a compatibility or
troubleshooting test needs ERNativeUI to leave the effective GFX bytes
completely unchanged, then fully restart the game. In that mode, pages
paginate against the capacity already present in the loaded movie and custom
tabs remain available. ERNativeUI does not add rows or TextInput/ColorPicker
artwork, but compatible structures already authored by the effective movie
remain usable and the controls retain their functional fallbacks.
`MinimumTabSlots` cannot grow the movie, and API 1.3 title-screen text is
unavailable. The host therefore does not advertise
`Capability::title_screen_text` while the setting is disabled.

Locale files use effective language identifiers and belong in `locales` beside
the host. `Language = auto` follows Elden Ring's current Steam game language.
To force one locale for ERNativeUI and every connected client mod, replace
`auto` with an exact lowercase identifier such as `french` or `vietnamese`,
then restart the game. Explicit identifiers may contain only ASCII lowercase
letters, digits, `_`, or `-`, with a maximum length of 64.

An override selects ERNativeUI's effective language; it does not translate
Elden Ring itself. A matching `locales/<identifier>.ini` translates host-owned
navigation text. Client mods receive the same exact identifier and remain
responsible for translations they provide. A missing locale file, missing
key, or invalid UTF-8 value falls back to embedded English for the affected
host text. An invalid `Language` value safely falls back to `auto`.

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
  [Game and mod compatibility](../COMPATIBILITY.md#solid-uncapper).
- Seamless Co-op 2.0.1 was live-tested with Elden Ring 2.7.0.0, with no
  incompatibility observed for that exact combination. See
  [Game and mod compatibility](../COMPATIBILITY.md#seamless-co-op).
- Elden Ring 2.7.1.0 title layouts were observed with Shadow of the Erdtree
  both absent and present. Final live A/B validation of API 1.3's adaptive
  title spacing is still pending; see the compatibility guide for scope.
- Host and committed client DLLs must remain loaded until process exit.
- If another mod supplies the same GFX path, its mod-loader order still decides
  which complete file reaches the game. ERNativeUI then completes that file
  only when its relevant structure is recognized.

For failures, read [Common errors](common-errors.md). A useful report includes
the ERNativeUI release, Elden Ring executable version, DLL order, other loaded
mods, and the first failed address or hook line from a temporary diagnostic
log.

Return to the [documentation home](../README.md) or the
[main project page](../../README.md).
