# ERNativeUI GFX patcher internals

`ERNativeUIGfxPatcher` performs optional, offline transformations of Elden
Ring's `menu/win/02_040_optionsetting.gfx` and
`menu/win/02_042_pc_graphicsetting.gfx`. `ERNativeUI.dll` does not parse,
load, or modify GFX files at runtime.

For `02_040`, the patcher expands `WindowList.ControllSetting` to 6–13 visual
rows, expands `WindowList.CameraSetting` from its native 7 rows to as many as
13, and adds the missing left-label field to their shared button widget. For
either movie, it can optionally add the character-name TextInput presentation
and a standalone character-creation ColorPicker widget. It writes a separate
output file; the input is never modified.

`ControllSetting` is Elden Ring's exact internal instance name for the visible
**Game Options** root. It must remain unchanged in binary paths despite its
misleading spelling. The actual **Controller Settings** tab is backed by the
separate `PadSetting`/`PadSettingDialog` path; this patcher does not expand it.

## Relevant display tree

The names preserve Elden Ring's spelling and capitalization:

```text
WindowList
|-- ControllSetting
|   |-- Item_0_0  --\
|   |-- Item_1_0    | placements of the same generic item character
|   `-- Item_12_0 --/
`-- CameraSetting
    |-- Item_0_0  --\
    |-- Item_1_0    | placements of that same generic item character
    `-- Item_12_0 --/

generic item character
`-- Widgets
    |-- Button
    |-- TextInput
    |-- ColorPicker             optional ERNativeUI sibling
    |-- ComboBox
    `-- Slider
```

The original Camera Options panel authors exactly seven slots and uses all
seven on the tested game configuration. It therefore has no spare native
slot. The optional patch adds `Item_7_0` through `Item_12_0` at the same
1,000-twip (50-pixel) spacing, depths, transforms, and shared generic-row
character already validated by the 13-row Game Options layout. Both panels
start at Y `-4800`, so thirteen rows end at Y `7200`, the live-validated visual
limit of this settings design.

Advanced Settings uses the same widget contract through a different host:

```text
GraphicOption
`-- Item_N_0
    `-- Widgets
        |-- Button
        |-- TextInput
        |-- ColorPicker         optional ERNativeUI sibling
        |-- ComboBox
        `-- Slider
```

That movie already contains 15 visual rows and has no
`WindowList.ControllSetting`; the shared TextInput and ColorPicker
presentation transformations still apply to it.

Each `Item_N_0` placement creates a distinct runtime instance of the generic
item and therefore a distinct runtime `Widgets` child. The underlying generic
item and widget character definitions are shared; they are not separately
duplicated in the file for every row. The placement's vertical position
determines the physical row. See the fuller static/runtime distinction in
the [GFX presentation case study](../../docs/research/case-studies/gfx-presentation.md).

Vanilla Game Options' `Button` exposes only `Text_0`, the right-side
button/value text. Unlike the Graphics-page button widget, it has no second
field in which an injected button's left label can render. The patch clones
Game Options' native `Caption` wrapper used by sliders and choices,
preserving its `PlaceObject3` transform, filter and shadow. It renames the
nested binding from `Text_0` to the `Text` name expected below a button's
`Text_1` field and installs this structure:

```text
Button
├── Text_0          right button/value text
└── Text_1
    └── Text        left row label
```

The nested child must be named exactly `Text`. A controlled probe using an
unmodified `Caption` wrapper rendered the same unresolved placeholder glyphs
for every row because its nested child is named `Text_0` and was not bound to
the expected field. Renaming only that binding retains the native appearance.

## Optional character-name TextInput presentation

Passing `--text-input-presentation character-name` decorates the shared idle
`TextInput` widget with the framed value presentation used by character-name
rows. The patcher adds a named `CharacterFrame` child behind `Text_0` and
`TextOnEmpty`, and changes only the empty-state text from native gray to red.
The dedicated `02_990_TextInput` editor that appears while typing is not
modified.

The frame references Elden Ring's existing
`MENU_FL_Cursor_EntWaku.tga`. The patcher generates a new
`DefineExternalImage2` reference, wrapper sprite and scaling grid with free
movie-local character IDs; it does not contain or copy the texture. Because
the generic item and TextInput definitions are shared, the presentation
applies to every TextInput row in the patched host movie.

The generated external-image definition, wrapper, and scaling grid are placed
before the TextInput sprite that references them. FFDec accepts a forward
reference, but Elden Ring's Scaleform loader did not render that form in the
controlled root-row test. Direct character-ID placement does not require
copying the character-creation movie's ActionScript or `SymbolClass` entry.

## Optional standalone ColorPicker widget

Passing `--color-picker-presentation character-creation` adds a real
`ColorPicker` sibling beside `Button`, `TextInput`, `ComboBox`, and `Slider`
in every structurally recognized `Widgets` container. The sibling placement
is hidden by default, so ordinary game and mod rows retain their native
appearance. ERNativeUI selects it only while constructing a color row through
these stable paths:

```text
Widgets
|-- Button                    unchanged native widget
`-- ColorPicker               hidden by default
    |-- Cursor                TextInput's full-row-only cursor
    |-- Color                 direct runtime tint target
    |-- native brackets/frame
    |-- Text_0 or Text_1      transparent binding target
    `-- Text_1 or Text_0      native left row label
```

At runtime the important contract is therefore:

```text
Widgets/ColorPicker
Widgets/ColorPicker/Cursor
Widgets/ColorPicker/Color
```

The `Button` child is not wrapped, cloned at runtime, or decorated with color
artwork. Native construction is redirected to the sibling only for the color
row, just as Elden Ring selects one of the other named row widgets. The
`ColorPicker` definition keeps both native text names because the two
supported settings hosts use opposite value/label bindings:

```text
ColorPicker
|-- Text_0 or Text_1          native right-side value
`-- Text_1 or Text_0          native left row label
```

The value binding is present but fully transparent; the color swatch occupies
that visual area. The row label retains the exact native placement, filter,
and shadow from the corresponding Button. `Cursor` reuses the host's native
TextInput cursor character, which provides the row-wide selection state but
does not contain Button's blinking inner value rectangle. The color brackets,
fill, and foreground are direct children at depths 3, 5, and 7, while the
cursor sits behind them at depth 2. This is why `Color` can be addressed
directly and why no Button-only cursor leaks through the swatch.

The supported definitions use these native bindings:

| Movie / Button sprite | Depth 10 value | Depth 12 label |
|---|---|---|
| `02_040` / `97` (Game Options) | `Text_0` | `Text_1` |
| `02_040` / `154` (alternate settings widget) | `Text_1` | `Text_0` |
| `02_042` / `55` (Advanced Settings) | `Text_1` | `Text_0` |

The visual resource closure is reproduced from the unbound display objects in
`04_010_chrmake_commandlist.gfx`. It reuses the exact CharacterFrame resource,
adds a `MENU_FL_ColorWaku.tga` external-image reference, a vector fill, frame
wrapper and scaling grid, and exposes the direct `Color` child. No texture,
ActionScript, or `SymbolClass` from the character-creation movie is copied.
Every definition is allocated a free movie-local character ID and placed
before its first `ColorPicker` reference.

The patcher also recognizes both complete earlier prototype layouts, where a
hidden `ColorPreview` was nested in Button at depth 5 or depth 7. It removes
only those byte-exact placements, reuses their validated visual resources,
removes the exact prototype-only `ButtonFrame` name, and adds the standalone
sibling. This restores the ordinary Button definition instead of retaining
prototype metadata. Unknown, partial, or conflicting placements are rejected
rather than overwritten.

## Safety and idempotence

The transformer identifies the host and TextInput from their named children,
depths, transforms, and field definitions rather than hard-coded sprite IDs.
For the `02_040` settings host it additionally validates the expected Game
Options and Camera Options panels, item placements, and shared button text
wrapper. It validates free character
IDs,
insertion depth, definition order, and tag boundaries; preserves trailing GFX
padding; and updates the affected sprite and declared movie lengths. A second
run with the same request is byte-for-byte unchanged.

## Build the patcher

From the repository root, run:

```powershell
.\build.bat
```

The executable is written to:

```text
build\preset-release\Release\ERNativeUIGfxPatcher.exe
```

The CMake option `ERNATIVEUI_BUILD_GFX_PATCHER` controls whether the tool is
built. It defaults to `ON` when ERNativeUI is the top-level project.

## Complete command-line reference

| Option | Value | Required | Meaning |
|---|---|---|---|
| `--help`, `-h`, `/?` | none | No | Prints the built-in usage text and exits without reading or writing a file. |
| `--input` | path | Yes | Reads a supported, uncompressed `02_040_optionsetting.gfx` or `02_042_pc_graphicsetting.gfx`. The source is never modified. |
| `--output` | path | Patch mode only | Writes the completed movie here. It must be different from `--input`. Parent directories are created automatically. |
| `--inspect` | none | No | Validates and describes `--input` without applying options or writing output. `--output` is not required in this mode. |
| `--overwrite` | none | No | Allows an existing output file to be replaced. Without this flag, an existing output is an error. |
| `--game-options-rows` | integer `6` through `13` | No | Sets the target row count for `WindowList.ControllSetting` in `02_040`. Omitting it preserves the detected count. Existing rows cannot be removed. |
| `--controller-rows` | integer `6` through `13` | No | Deprecated compatibility spelling for `--game-options-rows`. New commands should use `--game-options-rows`. |
| `--camera-options-rows` | integer `7` through `13` | No | Sets the target row count for `WindowList.CameraSetting` in `02_040`. Omitting it preserves the detected count. Existing rows cannot be removed. |
| `--text-input-presentation` | `native` or `character-name` | No | `character-name` adds the framed, red-empty-state TextInput presentation. `native` is the default and leaves it unchanged. Valid for either supported movie. |
| `--color-picker-presentation` | `native` or `character-creation` | No | `character-creation` adds the standalone bracketed ColorPicker widget. `native` is the default and leaves it unchanged. Valid for either supported movie. |

Row-count options are valid only for `02_040_optionsetting.gfx`.
`02_042_pc_graphicsetting.gfx` has no matching `WindowList` panels, so the
patcher rejects row-count options for that movie. Patching `02_042` also
requires at least one non-native presentation option because there would
otherwise be no requested transformation.

Patching `02_040` always verifies and, when missing, adds its shared
native-style Button label field. This repair does not have a separate
command-line switch.

## Inspect a file

Inspection is the quickest way to confirm which host was extracted and which
patches it already contains:

```powershell
.\build\preset-release\Release\ERNativeUIGfxPatcher.exe `
  --inspect `
  --input "C:\path\to\extracted\menu\win\02_040_optionsetting.gfx"
```

The report includes the detected host, visual row counts, TextInput
presentation, standalone ColorPicker presence, and relevant sprite IDs.

## Reproduce the two bundled GFX files

The committed files in `assets/menu/win` are generated outputs, not suitable
source originals. First extract fresh, unmodified copies of both movies from
the Elden Ring game version being targeted and keep them outside
`assets/menu/win`. The following PowerShell commands are the canonical recipe
for the currently bundled assets. Run them from the repository root and change
only `$OriginalMenu` to the directory containing the extracted originals:

```powershell
$Patcher = ".\build\preset-release\Release\ERNativeUIGfxPatcher.exe"
$OriginalMenu = "C:\path\to\extracted\menu\win"

& $Patcher `
  --input "$OriginalMenu\02_040_optionsetting.gfx" `
  --output ".\assets\menu\win\02_040_optionsetting.gfx" `
  --game-options-rows 13 `
  --camera-options-rows 13 `
  --text-input-presentation character-name `
  --color-picker-presentation character-creation `
  --overwrite

& $Patcher `
  --input "$OriginalMenu\02_042_pc_graphicsetting.gfx" `
  --output ".\assets\menu\win\02_042_pc_graphicsetting.gfx" `
  --text-input-presentation character-name `
  --color-picker-presentation character-creation `
  --overwrite
```

These commands produce the repository's intended presentation:

| Bundled output | Result |
|---|---|
| `assets/menu/win/02_040_optionsetting.gfx` | 13 Game Options rows, 13 Camera Options rows, repaired Button labels, character-name TextInput, and standalone ColorPicker. |
| `assets/menu/win/02_042_pc_graphicsetting.gfx` | Existing Advanced Settings row capacity, character-name TextInput, and standalone ColorPicker. |

Inspect both generated files and run the dedicated test before committing
them:

```powershell
& $Patcher --inspect --input ".\assets\menu\win\02_040_optionsetting.gfx"
& $Patcher --inspect --input ".\assets\menu\win\02_042_pc_graphicsetting.gfx"

ctest --test-dir .\build\preset-release `
  -C Release `
  -R ERNativeUI.GfxPatcher `
  --output-on-failure
```

If the assets were regenerated after an earlier build, run `build.bat` again
to refresh both the staged runtime and `dist/ERNativeUI`.

## Where CMake uses the bundled files

The standard CMake build deliberately does **not** generate the GFX files.
Generation requires game-owned source movies that are not project build
inputs. The repository therefore treats the two committed files under
`assets/menu/win` as the source of truth for staging, testing, and packaging.

All related rules are in the [root CMakeLists.txt](../../CMakeLists.txt):

| CMake location | What to update or inspect |
|---|---|
| `if(ERNATIVEUI_BUILD_GFX_PATCHER)` / target `ERNativeUIGfxPatcher` | Builds `tools/gfx_patcher/main.cpp` and links the transformation core. Change this only when the patcher target itself changes. |
| Target `ERNativeUIGfxPatcherTests` / test `ERNativeUI.GfxPatcher` | Passes both committed asset paths to `tests/gfx_patcher_test.cpp` and validates their expected structure. Update this if asset names or test inputs change. |
| Target `ERNativeUIStageRuntime` | Copies both committed assets into `build/preset-release/deploy/<Configuration>/menu/win`. Update its two `copy_if_different` commands if paths or filenames change. |
| `install(FILES assets/menu/win/...)` | Copies both assets into `dist/ERNativeUI/bin/menu/win` during `cmake --install`. Update this list if a bundled GFX file is added, removed, or renamed. |

For an ordinary game-version refresh, keep the filenames unchanged and run the
two reproduction commands above; no CMake edit is needed. A CMake change is
needed only when the asset set or its paths change.

## Install custom output

For a separate mod, write to any safe output path and then install the result
under its original filename:

```text
<ModEngine2 mod directory>/menu/win/02_040_optionsetting.gfx
<ModEngine2 mod directory>/menu/win/02_042_pc_graphicsetting.gfx
```

The input and output must be different paths. Use files extracted from the
game version being targeted, and never patch Elden Ring's installed archive
in place.

The 13-row asset is optional. ERNativeUI still supports native panel
capacities without it. Game Options retains its smaller native capacity, while
Camera Options has no spare slot on the tested configuration and cannot accept
an extra row until its GFX panel is expanded. Action-row labels in either panel
use the shared `Text_1/Text` field described above.
