# ERNativeUI

ERNativeUI is a shared native-menu host for Elden Ring mods. Install one copy
of `ERNativeUI.dll`; compatible client mods register their menus through it.
The DLLs in `examples/` are optional demonstrations, not dependencies for
ordinary client mods.

## Requirements

- Elden Ring for Windows x64, run offline with Easy Anti-Cheat disabled.
- Mod Engine 2 or another compatible DLL loader.
- An ERNativeUI release that explicitly supports your Elden Ring executable.
  Check the [release notes](https://github.com/Flammrock/ERNativeUI/releases)
  after every game update.

## Install with Mod Engine 2

1. Extract this archive as `mod\ERNativeUI` inside your Mod Engine 2 setup.
2. Add the host before any ERNativeUI client DLLs in the existing
   `external_dlls` list. Preserve the entries already used by other mods.

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\SomeClientMod\\SomeClientMod.dll"
]
```

To try the full demonstration, add
`mod\ERNativeUI\examples\TarnishedUIShowcase.dll` after the host. Do not load
the example DLLs unless you want their example menus.

## Runtime GFX completion

By default, ERNativeUI expands supported settings pages and supplies its
TextInput and ColorPicker presentation in memory while the game loads the
effective GFX. No loose ERNativeUI GFX files or mod-loader entry are normally
required.

Structurally compatible GFX replacements from other mods remain supported.
Unrecognized layouts are left unchanged instead of being patched by guesswork.
Read [Runtime GFX completion and presentation](https://github.com/Flammrock/ERNativeUI/blob/main/docs/guides/presentation-and-gfx.md)
when combining ERNativeUI with another mod that replaces the same movies.

Players can set `[GFX] EnableRuntimePatching = 0` in `ERNativeUI.ini` and
restart to prevent ERNativeUI from installing its GFX hook. The effective GFX
then remains untouched: custom tabs and functional TextInput/ColorPicker
fallbacks remain available, but ERNativeUI adds no rows or widget presentation
and API 1.3 title-screen text is disabled. Compatible structures already
authored into the effective movie remain available.

## Configuration

`ERNativeUI.ini` belongs beside `ERNativeUI.dll`. The host creates its embedded
default when the file is missing and never overwrites an existing file.
Logging is disabled by default; enable it only while troubleshooting. Locale
INI files belong in `locales/` beside the host. By default ERNativeUI follows
Elden Ring's Steam game language; `[Localization] Language` can select an
explicit host-wide locale for ERNativeUI and connected client mods.

See the [player installation guide](https://github.com/Flammrock/ERNativeUI/blob/main/docs/getting-started/player-installation.md)
for complete setup, runtime configuration, and verification details.

## Compatibility and support

- Only one canonical `ERNativeUI.dll` should be loaded.
- For Solid Uncapper, list `Solid Uncapper.dll` before `ERNativeUI.dll` and
  consult the exact tested matrix in [Game and mod compatibility](https://github.com/Flammrock/ERNativeUI/blob/main/docs/COMPATIBILITY.md).
- Seamless Co-op 2.0.1 was live-tested with Elden Ring 2.7.0.0, with no
  incompatibility observed for that exact combination.
- Shadow of the Erdtree title-screen layout and adaptive spacing were
  live-tested on Elden Ring 2.7.1.0 with the DLC both absent and present; see
  the compatibility matrix for the exact scope.

If startup fails after a game update, do not reuse an old RVA or disable
validation. Follow [Common errors](https://github.com/Flammrock/ERNativeUI/blob/main/docs/getting-started/common-errors.md),
then report the ERNativeUI release, Elden Ring executable version, DLL order,
and first failed address or hook line through [GitHub Issues](https://github.com/Flammrock/ERNativeUI/issues)
or the [Nexus Mods page](https://www.nexusmods.com/eldenring/mods/10767).

## Credits

- **[An Thanh Le](https://github.com/AnThanhLe)** — contributed the Vietnamese translation.

The complete mod-author SDK, examples, documentation, source, and license are
available in the [ERNativeUI repository](https://github.com/Flammrock/ERNativeUI).
