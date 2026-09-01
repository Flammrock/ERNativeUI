# ERNativeUIGfxPatcher

`ERNativeUIGfxPatcher.exe` can transform the user's own PC copies of:

```text
menu\win\02_040_optionsetting.gfx
menu\win\02_042_pc_graphicsetting.gfx
```

Controller row expansion and root button-label repair apply only to `02_040`.
The optional character-name TextInput presentation applies independently to
either movie, which lets the root and ERNativeUI Advanced Settings subpages
use the same idle appearance.

ERNativeUI releases include both optional pre-patched host movies used by the
recommended installation: the 13-row Controller asset and the Advanced
Settings asset with matching TextInput presentation. The patcher is provided
so each transformation is reproducible: it reads an original file extracted
from the user's installed game, makes a narrow structural change, verifies the
result, and writes a separate output file.

## v0.8.0 Controller Settings patch

The PC `ControllSetting` sprite contains six generic visual row placements:

```text
Item_0_0 ... Item_5_0
```

Elden Ring normally occupies four of them, leaving two visible slots for rows
injected afterward. The retained v0.8.0 patch adds:

```text
Item_6_0
```

at the same spacing and depth convention used by the other Game Options
panels. This creates seven visual slots, so the Controller Settings page can
show four vanilla rows plus three ERNativeUI rows.

The patcher does not invent or redistribute a widget. It clones a compatible
generic `Item_6_0` placement already present elsewhere in the same user-owned
GFX, updates the `ControllSetting` sprite length and movie length, then parses
the completed output again to verify the requested row count.

## Root button row labels

Controller Settings uses `MENU_PC_SettingParts`, while Graphics subpages use
`MENU_ItemParts`. The Controller `Button` widget originally exposes only its
right-side `Text_0` value and lacks the second field required for an injected
button's left row label.

The patcher adds `Text_1` with a nested child named `Text`, cloning the native
Controller `Caption` presentation so button labels use the same position,
filter and shadow as slider and choice labels. See
`tools/gfx_patcher/README.md` for the display tree, validation rules, and the
failed wrapper probe that established the exact binding contract.

## Optional TextInput appearance

`--text-input-presentation character-name` adds the framed, red-empty-state
idle appearance used by character-name rows. It is opt-in; omitting the option
keeps the native TextInput appearance in that movie.

The generated `CharacterFrame` child references the game's existing
`MENU_FL_Cursor_EntWaku.tga` through a fresh movie-local external-image ID.
No texture is copied into the patcher or generated file. The change affects
the shared idle row widget only; Elden Ring's separate `02_990_TextInput`
typing editor is left intact.

The root Controller Settings page is hosted by `02_040`; ERNativeUI's current
custom subpages are hosted by `02_042`. Patching only one file therefore
changes only that host. The generated external-image and wrapper definitions
are deliberately inserted before the TextInput sprite that uses them. FFDec
can parse a forward reference, but the game did not render it during the
controlled root-row experiment.

## Build

Run:

```bat
build.bat
```

Output:

```text
build\preset-release\Release\ERNativeUIGfxPatcher.exe
```

## One-command 13-row patch

Pass the PC GFX extracted with UXM:

```bat
build\preset-release\Release\ERNativeUIGfxPatcher.exe ^
  --input "C:\path\to\UXM\menu\win\02_040_optionsetting.gfx" ^
  --output "gfx_patch_output\menu\win\02_040_optionsetting.gfx" ^
  --controller-rows 13
```

The source file is never modified. Output:

```text
gfx_patch_output\
└─ menu\
   └─ win\
      └─ 02_040_optionsetting.gfx
```

Copy the generated `menu` directory into the Mod Engine 2 mod directory that
already contains your loose assets.

## Direct command line

Inspect without writing:

```bat
build\preset-release\Release\ERNativeUIGfxPatcher.exe ^
  --inspect ^
  --input "C:\path\to\menu\win\02_040_optionsetting.gfx"
```

Generate the 13-row Controller Settings panel:

```bat
build\preset-release\Release\ERNativeUIGfxPatcher.exe ^
  --input "C:\path\to\original\02_040_optionsetting.gfx" ^
  --output "C:\path\to\ModEngine2\mod\menu\win\02_040_optionsetting.gfx" ^
  --controller-rows 13 ^
  --text-input-presentation character-name ^
  --overwrite
```

Remove the `--text-input-presentation` line when only row capacity and root
button-label support are wanted.

Generate the Advanced Settings TextInput presentation independently:

```bat
build\preset-release\Release\ERNativeUIGfxPatcher.exe ^
  --input "C:\path\to\original\02_042_pc_graphicsetting.gfx" ^
  --output "C:\path\to\ModEngine2\mod\menu\win\02_042_pc_graphicsetting.gfx" ^
  --text-input-presentation character-name ^
  --overwrite
```

The tool detects the host from the TextInput structure. It rejects
`--controller-rows` for `02_042`, which has no `ControllSetting` panel.

The structural patcher accepts target counts from 6 through 13. ERNativeUI
ships the visually largest supported 13-row layout as an optional convenience;
the unmodified six-row game asset remains fully supported.

## Safety checks

The patcher refuses to write when:

- the input is not an uncompressed `GFX` movie;
- its declared movie length or tag boundaries are invalid;
- non-zero unknown data follows the declared movie;
- the supported TextInput child structure cannot be identified uniquely;
- for a Controller row patch, `WindowList.ControllSetting` cannot be
  identified uniquely;
- the existing `Item_N_0` sequence is non-contiguous or uses an unknown layout;
- a compatible simple donor placement does not exist inside that same GFX;
- a requested new display depth is already occupied;
- a partial or conflicting `CharacterFrame`/external-image definition exists;
- input and output paths are identical;
- the completed output does not parse back to the requested row count.

The output is written through a temporary file and moved into place only after
all transformations and verification succeed.

## Uninstall

Delete the loose file:

```text
mod\menu\win\02_040_optionsetting.gfx
mod\menu\win\02_042_pc_graphicsetting.gfx
```

Elden Ring will fall back to its original archive copy. The DLL itself does not
modify the installed game archives.
