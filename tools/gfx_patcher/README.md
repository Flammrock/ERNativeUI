# ERNativeUI GFX patcher internals

`ERNativeUIGfxPatcher` performs optional, offline transformations of Elden
Ring's `menu/win/02_040_optionsetting.gfx` and
`menu/win/02_042_pc_graphicsetting.gfx`. `ERNativeUI.dll` does not parse,
load, or modify GFX files at runtime.

For `02_040`, the patcher expands `WindowList.ControllSetting` to 6–13 visual
rows and adds the missing left-label field to its button widget. For either
movie, it can optionally add the character-name TextInput presentation. It
writes a separate output file; the input is never modified.

## Relevant display tree

The names preserve Elden Ring's spelling and capitalization:

```text
WindowList
`-- ControllSetting
    |-- Item_0_0  --\
    |-- Item_1_0    | placements of the same generic item character
    `-- Item_12_0 --/

generic item character
`-- Widgets
    |-- Button
    |-- TextInput
    |-- ComboBox
    `-- Slider
```

Advanced Settings uses the same widget contract through a different host:

```text
GraphicOption
`-- Item_N_0
    `-- Widgets
        |-- Button
        |-- TextInput
        |-- ComboBox
        `-- Slider
```

That movie already contains 15 visual rows and has no
`WindowList.ControllSetting`; only the TextInput presentation transformation
applies to it.

Each `Item_N_0` placement creates a distinct runtime instance of the generic
item and therefore a distinct runtime `Widgets` child. The underlying generic
item and widget character definitions are shared; they are not separately
duplicated in the file for every row. The placement's vertical position
determines the physical row. See the fuller static/runtime distinction in
[the Scaleform/GFX presentation model](../../docs/analysis/SCALEFORM_GFX_MODEL.md).

Vanilla Controller Settings' `Button` exposes only `Text_0`, the right-side
button/value text. Unlike the Graphics-page button widget, it has no second
field in which an injected button's left label can render. The patch clones
Controller Settings' native `Caption` wrapper used by sliders and choices,
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

## Safety and idempotence

The transformer identifies the host and TextInput from their named children,
depths, transforms, and field definitions rather than hard-coded sprite IDs.
For the Controller movie it additionally validates the expected panel, item
placements, and button text wrapper. It validates free character IDs,
insertion depth, definition order, and tag boundaries; preserves trailing GFX
padding; and updates the affected sprite and declared movie lengths. A second
run with the same request is byte-for-byte unchanged.

## Usage

```powershell
ERNativeUIGfxPatcher.exe `
  --input 02_040_optionsetting.gfx `
  --output 02_040_optionsetting.patched.gfx `
  --controller-rows 13 `
  --text-input-presentation character-name
```

Patch only the TextInput presentation used by ERNativeUI subpages:

```powershell
ERNativeUIGfxPatcher.exe `
  --input 02_042_pc_graphicsetting.gfx `
  --output 02_042_pc_graphicsetting.patched.gfx `
  --text-input-presentation character-name
```

The patcher detects the host structurally. Passing `--controller-rows` for
`02_042_pc_graphicsetting.gfx` is rejected because that movie has no
Controller Settings panel.

Omit `--text-input-presentation` to preserve the native TextInput appearance.

Install each generated output under its original filename:

```text
<ModEngine2 mod directory>/menu/win/02_040_optionsetting.gfx
<ModEngine2 mod directory>/menu/win/02_042_pc_graphicsetting.gfx
```

The 13-row asset is optional. ERNativeUI still supports the native Controller
capacity without it, but root action-row labels require the `Text_1/Text`
field described above.
