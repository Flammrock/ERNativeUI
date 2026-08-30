# ERNativeUI GFX patcher internals

`ERNativeUIGfxPatcher` performs the optional, offline transformation of Elden
Ring's `menu/win/02_040_optionsetting.gfx`. `ERNativeUI.dll` does not parse,
load, or modify GFX files at runtime.

The patcher expands `WindowList.ControllSetting` to 6–13 visual rows and adds
the missing left-label field to its button widget. It writes a separate output
file; the input is never modified.

## Relevant display tree

The names preserve Elden Ring's spelling and capitalization:

```text
WindowList
└── ControllSetting
    ├── Item_0_0
    ├── Item_1_0
    └── ... Item_12_0
        └── Widgets
            ├── Button
            ├── TextInput
            ├── ComboBox
            └── Slider
```

Each `Item_N_0` is a placement that references the same generic item
character; the widgets are therefore shared by every placement, not separately
embedded under `Item_12_0`. Its vertical position determines the physical row.

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

## Safety and idempotence

The transformer validates the expected panel, item placements, button text
wrapper, free character ID, insertion depth, and tag boundaries. It preserves
trailing GFX padding and updates the affected sprite lengths and declared movie
length. If the field and requested row count already exist, a second run is
byte-for-byte unchanged.

## Usage

```powershell
ERNativeUIGfxPatcher.exe `
  --input 02_040_optionsetting.gfx `
  --output 02_040_optionsetting.patched.gfx `
  --controller-rows 13
```

Install the output at:

```text
<ModEngine2 mod directory>/menu/win/02_040_optionsetting.gfx
```

The 13-row asset is optional. ERNativeUI still supports the native Controller
capacity without it, but root action-row labels require the `Text_1/Text`
field described above.
