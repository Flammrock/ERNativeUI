# ERNativeUI

ERNativeUI is a shared native-menu host for Elden Ring mods. Install one copy
of `ERNativeUI.dll`; compatible client mods register their menus through it.
The DLLs in `examples/` are optional demonstrations, not dependencies for
ordinary client mods.

## Requirements

- Elden Ring for Windows x64, run offline with Easy Anti-Cheat disabled.
- Mod Engine 2 or another compatible DLL loader; a loose-file loader is needed
  only for the optional GFX presentation.
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

## Optional GFX presentation

The bundled `menu/win/*.gfx` files add expanded settings capacity and native
TextInput/ColorPicker presentation. Core menu behavior has native fallbacks,
so this directory is optional. To enable it, add the extracted ERNativeUI
directory to the existing mod-loader list:

```toml
[extension.mod_loader]
enabled = true
mods = [
    { enabled = true, name = "ERNativeUI", path = "mod\\ERNativeUI" },
]
```

Another mod can replace the same GFX paths; Mod Engine 2 does not merge binary
GFX edits. Read [Presentation and optional GFX](https://github.com/Flammrock/ERNativeUI/blob/main/docs/guides/presentation-and-gfx.md)
before combining such mods.

## Configuration

`ERNativeUI.ini` belongs beside `ERNativeUI.dll`. The host creates its embedded
default when the file is missing and never overwrites an existing file.
Logging is disabled by default; enable it only while troubleshooting. Locale
INI files belong in `locales/` beside the host.

See the [player installation guide](https://github.com/Flammrock/ERNativeUI/blob/main/docs/getting-started/player-installation.md)
for complete setup, runtime configuration, and verification details.

## Compatibility and support

- Only one canonical `ERNativeUI.dll` should be loaded.
- For Solid Uncapper, list `Solid Uncapper.dll` before `ERNativeUI.dll` and
  consult the exact tested matrix in [Game and mod compatibility](https://github.com/Flammrock/ERNativeUI/blob/main/docs/compatibility.md).
- Seamless Co-op 2.0.1 was live-tested with Elden Ring 2.7.0.0, with no
  incompatibility observed for that exact combination.
- Shadow of the Erdtree remains explicitly unverified until its live test
  matrix is completed.

If startup fails after a game update, do not reuse an old RVA or disable
validation. Follow [Common errors](https://github.com/Flammrock/ERNativeUI/blob/main/docs/getting-started/common-errors.md),
then report the ERNativeUI release, Elden Ring executable version, DLL order,
and first failed address or hook line through [GitHub Issues](https://github.com/Flammrock/ERNativeUI/issues)
or the [Nexus Mods page](https://www.nexusmods.com/eldenring/mods/10767).

The complete mod-author SDK, examples, documentation, source, and license are
available in the [ERNativeUI repository](https://github.com/Flammrock/ERNativeUI).
