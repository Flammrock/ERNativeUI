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

## What's new in 1.3.0

ERNativeUI 1.3.0 first publishes ERUI API 1.3 while retaining the API 1.0,
1.1, and 1.2 contracts used by existing client binaries.

- Client mods can add their own top-level System tabs with independent titles,
  optional custom icons, ordinary paginated content, player ordering, and
  native overflow navigation.
- Client mods can add short status or version lines to any title-screen corner
  using Elden Ring's native color or an explicit RGB color.
- Runtime GFX completion supplies the validated page capacities and missing
  TextInput and ColorPicker presentation without loose ERNativeUI GFX files.
- Players can request a minimum of eight, nine, or ten physical System slots
  while runtime patching is enabled; ERNativeUI never shrinks a compatible
  movie that already has more slots. Additional logical tabs remain reachable
  through the shifting `...` viewport.

See the [visual 1.3.0 showcase](https://github.com/Flammrock/ERNativeUI/blob/main/docs/whats-new/1.3.0.md)
or the [API 1.2 to 1.3 migration guide](https://github.com/Flammrock/ERNativeUI/blob/ERNativeUI-v1.3.0/docs/migrations/1.2-to-1.3.md).

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

To try the broad settings and controls demonstration, add
`mod\ERNativeUI\examples\TarnishedUIShowcase.dll` after the host. Do not load
the example DLLs unless you want their demonstrations.

`TitleScreenTextExample.dll` can be loaded after the host for the focused API
1.3 title-screen demonstration. `CustomTabsStress.dll` intentionally registers
50 tabs and 1,000 rows; reserve it for isolated integration and performance
testing rather than normal play.

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

`[Tabs] MinimumTabSlots = 8` requests the vanilla minimum and never reduces a
compatible movie that already contains more slots. With runtime GFX patching
enabled, players may request 8, 9, or 10; ten is Elden Ring's hard native
ceiling. This does not limit registered custom tabs: ERNativeUI keeps
additional logical tabs reachable through its shifting `...` viewport. There
is no `MaximumTabSlots` setting, and changing the minimum requires a restart.

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
- ERNativeUI 1.3.0's API 1.3 paths were live-validated on Elden Ring
  executables 2.7.1.0 and 2.7.2.0. The retained 2.7.0.0 profile supports API
  1.0 through 1.2 paths, but build-locked API 1.3 capabilities such as custom
  tabs are unavailable there.
- For Solid Uncapper, list `Solid Uncapper.dll` before `ERNativeUI.dll` and
  consult the exact tested matrix in [Game and mod compatibility](https://github.com/Flammrock/ERNativeUI/blob/main/docs/COMPATIBILITY.md).
  Solid Uncapper 2.3 coexistence was verified on executables 2.7.0.0 and
  2.7.1.0. It has not been regression-tested with ERNativeUI on 2.7.2.0.
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
