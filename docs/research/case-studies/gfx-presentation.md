# GFX presentation and custom row widgets

This case study records how ERNativeUI extended Elden Ring's settings movies
without confusing a static Scaleform placement with a working native control.
It covers the optional row-capacity patch, the Game Options action-label
repair, and the TextInput and ColorPicker presentations shipped with ERUI API
1.1.

For installation and command-line usage, read
[Presentation and optional GFX assets](../../guides/presentation-and-gfx.md)
and the [patcher README](../../../tools/gfx_patcher/README.md). This page is
the evidence and reproduction record behind those assets.

## Reference artifacts and provenance

The static analysis used JPEXS Free Flash Decompiler (FFDec) 26.2.1, the
repository's structural inspector/patcher, controlled in-game comparisons,
and GFX files extracted from the researcher's own Steam installation. The
unmodified game files and FFDec exports remain outside the repository.

The corresponding executable was:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The checked movie samples were:

| Artifact | Bytes | SHA-256 | Provenance |
|---|---:|---|---|
| Unmodified `02_040_optionsetting.gfx` | 44,016 | `2B09D4E64EAB964F8D4045D25960F4C6CE1806D6596C75A21F28F334D0CD10B3` | User-extracted six-row reference; not committed |
| Shipped `02_040_optionsetting.gfx` | 45,129 | `5085358397E17271654EDA15A4D0299C57A06772A2937C3FF074197D820FCEFA` | Thirteen-row Game/Camera Options, action-label repair, TextInput frame, and standalone ColorPicker |
| Unmodified `02_042_pc_graphicsetting.gfx` | 27,056 | `E87477878203AC04104FDC3617D870BD44A0BF8C14F1A8E20922F59605867DAC` | User-extracted fifteen-row Advanced Settings source; not committed |
| Shipped `02_042_pc_graphicsetting.gfx` | 27,574 | `687082466CA2C16BF5E50F1D21BCA9C3C6E2330B45BB8C79D92D773200FE2890` | Advanced Settings TextInput frame and standalone ColorPicker |

Applying all supported transformations to the recorded `02_040` source
produced the checked-in asset byte for byte and added 1,113 bytes. Character
IDs below describe these exact movies only; the patcher discovers structure
instead of treating those IDs as a cross-version interface.

## Definitions, placements, and runtime instances

Three layers must remain separate:

- A **character definition** is a reusable sprite, text field, shape, or image
  in one movie's character dictionary.
- A **placement** puts a character at a depth and transform in a timeline and
  may give that instance a name.
- A **runtime instance** is the object Scaleform creates from a placement when
  the movie runs.

**Confirmed:** every `Item_N_0` placement in a settings panel can reference
the same generic-item definition. Each placement nevertheless creates a
distinct runtime row and therefore a distinct runtime `Widgets` child.

This explains two apparently opposite results:

- adding another `Item_N_0` placement adds visual capacity without duplicating
  all widget definitions; and
- changing a shared `TextInput`, `Button`, or `Widgets` definition affects
  every runtime row that uses it.

**Rejected:** a tree diagram in which every authored `Item_N_0` owns a private
copy of every widget. The instances are per placement; their definitions are
shared.

## Confirmed host trees

### Game Options and Camera Options

In `02_040_optionsetting.gfx`, both panels are children of `WindowList` and
use the same generic row definition in the reference movie:

```text
WindowList
|-- ControllSetting
|   |-- Item_0_0  --\
|   |-- Item_1_0    | placements of generic item character 126
|   `-- Item_N_0  --/
`-- CameraSetting
    |-- Item_0_0  --\
    |-- Item_1_0    | placements of generic item character 126
    `-- Item_N_0  --/

generic item character 126
`-- Widgets                         character 125
    |-- Button                      character 97
    |-- TextInput                   character 103
    |-- ComboBox                    character 116
    `-- Slider                      character 124
```

`ControllSetting` is Elden Ring's exact, case-sensitive instance name for the
visible **Game Options** panel. It is not the Controller Settings screen,
which follows the separate `PadSetting`/`PadSettingDialog` path.

A second generic row variant uses `Widgets` character 155 and `Button`
character 154 while sharing the same TextInput, ComboBox, and Slider
definitions. Other built-in settings panels use that variant.

### Advanced Settings

ERNativeUI continuation and child pages are presented by
`02_042_pc_graphicsetting.gfx`:

```text
GraphicOption                            character 90
`-- Item_N_0                             character 80, N = 0..14
    `-- Widgets                          character 79
        |-- Button                       character 55
        |-- TextInput                    character 61
        |-- ComboBox                     character 72
        `-- Slider                       character 78
```

**Confirmed:** this host already authors fifteen placements and has no
`WindowList.ControllSetting`. Row expansion and the Game Options-specific
Button repair must therefore remain independent from shared TextInput and
ColorPicker transformations.

**Inferred:** native row state selects which already-placed widget is active
for a row. The static movie proves the named destinations, but it does not by
itself reveal the native discriminator or event path.

## Case 1: optional thirteen-row capacity

The unmodified Game Options panel authors six placements, `Item_0_0` through
`Item_5_0`. Elden Ring constructs four vanilla rows in the tested setup, so
two slots remain. Camera Options authors and occupies seven placements, so it
has no spare slot for an injected row.

Both panels use this placement sequence:

```text
Y(N) = -4800 + 1000 * N twips
```

At 20 twips per pixel this is 50-pixel spacing. The thirteenth placement,
`Item_12_0`, lands at Y `7200` and fits the panel artwork. Controlled 15-row
experiments remained selectable but rows 14 and 15 visibly overflowed that
artwork.

**Confirmed:** 13 is the supported visual limit for these two panels on the
reference assets. It is not a universal native row limit. The patcher accepts
Game Options targets from 6 through 13 and Camera Options targets from 7
through 13, refuses shrinking, and clones the existing placement convention.

The DLL does not read the loose movie. Native code reports the current live
capacity from the constructed page, so pagination works with the original
movie, the optional 13-row movie, or another structurally compatible capacity.
The runtime also subtracts the post-materializer live row count. A compatible
mod that adds a row earlier in the native call chain therefore consumes one
of those placements just like an Elden Ring row; the GFX capacity itself does
not change.
See the [settings pages and pagination case study](settings-pages-and-pagination.md)
for the native half.

**Rejected:** failing to see an appended Camera Options row proves that its
native hook is wrong. The unchanged native probe became visible as soon as
the panel had a free placement.

## Case 2: restoring the left label on action rows

The Game Options `Button` definition exposes only `Text_0`, its right-side
button/value field. Native sliders and choices have a separate left Caption,
but a native action appended to this panel originally had nowhere to render
its row label.

The repair clones the native Caption wrapper used by Slider, ComboBox, and
TextInput, preserving its placement, transform, filter, and shadow. Only its
nested binding name changes:

```text
Button
|-- Text_0                   existing right-side action text
`-- Text_1                   cloned Caption wrapper
    `-- Text                 left row label expected by native binding
```

In the reference patched movie the new wrapper is character 193, cloned from
character 101. Those IDs are evidence, not matching rules.

**Rejected:** placing the unmodified Caption wrapper under `Text_1`. Its child
was still named `Text_0`; the native binding missed it and every action label
displayed the same unresolved placeholder glyph sequence. The displayed
glyphs were not themselves reliable evidence of a language. Renaming the
nested child to exactly `Text` preserved the native styling and made every
row's own label appear.

Because Game Options and Camera Options share this Button definition, one
definition repair reaches both panels. The Advanced Settings Button already
has separate value and label fields and does not need this repair.

## Case 3: character-name TextInput presentation

The idle TextInput widgets in both hosts have the same structural contract:

```text
TextInput
|-- row background                       depth 1
|-- [free slot]                           depth 2
|-- Text_0                                depth 3
|-- TextOnEmpty                           depth 4
|-- Caption                               depth 5
`-- Cursor                                depth 7
```

The patcher places a `CharacterFrame` at depth 2 and changes the recognized
empty-state field from gray to red. The frame references Elden Ring's existing
`MENU_FL_Cursor_EntWaku.tga` through a new movie-local
`DefineExternalImage2`, wrapper sprite, and scaling grid. No texture is copied
into the repository or embedded in the generated movie.

**Confirmed:** patching `02_040` changes the Game Options-hosted TextInput;
patching `02_042` independently changes ERNativeUI's Advanced Settings-hosted
TextInput. Both showed the bracketed character-name presentation in game.
The separate `02_990_TextInput` editor used while typing is not modified.

### Definition order was behavioral, not cosmetic

The first generated probe placed the new definition closure after the
TextInput that referenced it. FFDec parsed the forward reference, but Elden
Ring silently omitted the frame in game.

**Rejected:** successful FFDec parsing proves a definition may follow its
first control-tag use in Elden Ring's loader.

The production patcher inserts or relocates the external-image definition,
wrapper, and scaling grid before their earliest TextInput or ColorPicker use.
That ordering rendered correctly in both hosts and is now part of structural
verification.

## Case 4: a standalone ColorPicker sibling

Early probes decorated the ordinary Button with a nested color preview. They
could open the native editor, but Button's own inner selection cursor remained
visible as a blinking rectangle over the swatch. They also left prototype-only
placements inside a shared Button definition.

**Rejected:** ColorPicker should remain artwork nested inside Button with
runtime visibility tricks.

The final patch adds a hidden `ColorPicker` beside the other row widgets:

```text
Widgets
|-- Button
|-- TextInput
|-- ColorPicker                         hidden until selected for a color row
|   |-- row background                  depth 1
|   |-- Cursor                          depth 2
|   |-- unnamed bracket placement       depth 3
|   |-- Color                           depth 5
|   |-- unnamed ColorWaku foreground    depth 7
|   |-- transparent value binding       depth 10
|   `-- native left label               depth 12
|-- ComboBox
`-- Slider
```

The runtime paths are deliberately shallow:

```text
Widgets/ColorPicker
Widgets/ColorPicker/Cursor
Widgets/ColorPicker/Color
```

`Cursor` reuses the host TextInput's full-row cursor, which supplies the
selection state without Button's inner blinking rectangle. `Color` is a
direct tint target. The native Button row background and exact native label
placement retain the host's normal focus and typography.

The two host families reverse their native value/label bindings, so the
generated widget preserves both names:

| Host Button definition | Depth 10 value | Depth 12 label |
|---|---|---|
| `02_040` Game Options, character 97 | `Text_0` | `Text_1` |
| `02_040` alternate widgets, character 154 | `Text_1` | `Text_0` |
| `02_042` Advanced Settings, character 55 | `Text_1` | `Text_0` |

The visual closure was derived from unbound display objects in
`04_010_chrmake_commandlist.gfx`. It references the game's existing
`MENU_FL_ColorWaku.tga`, builds a vector fill plus frame wrapper and scaling
grid, and does not copy the donor movie's ActionScript or `SymbolClass`.

**Confirmed:** the standalone sibling renders the native brackets and current
color, updates independent root and child-page rows, opens the native color
editor, and no longer displays Button's inner cursor.

### Retired prototype cleanup

The patcher recognizes only the two complete historical prototypes with a
hidden Button-nested `ColorPreview` at depth 5 or 7. It removes those exact
placements and the exact prototype-only `ButtonFrame` name, reuses their
validated resource closure, and adds the standalone sibling.

**Confirmed:** unrelated Button children are preserved. **Rejected:** trying
to normalize an unknown, partial, or conflicting prototype. Such input fails
closed instead of overwriting an unrecognized mod's structure.

## Case 5: separating the two page-title layers

The first title prototype changed `Configuration` successfully but left the
requested child-page heading as `Advanced Settings`. That was useful evidence:
the text setter, wrapper lifetime, and resolved object were valid, but the
prototype had reached the wrong presentation layer.

Two movies and two visible fields participate in the tested flow:

| Presentation layer | Owning movie | Visible field |
|---|---|---|
| Outer settings title | `02_040_optionsetting.gfx` | `MenuTitle/StaticText_101003` |
| Inner Advanced Settings-style heading | `02_042_pc_graphicsetting.gfx` | `GraphicOption/StaticText_111114` |

The native resolver was observed receiving `MenuTitle/Text` and
`MenuTitle/Text_0`. Those strings are inputs to native binding code; they are
not the names of the final visible fields in the two inspected movies. Similar
path names must not be treated as aliases without checking the live object and
the static movie tree.

The target was recovered in six bounded steps:

1. Arm a rate-limited path observation only while one known ERNativeUI child
   page is pending.
2. Record the resolver input, returned `0x60`-byte scene-object wrapper, and
   native setter call. The observed UTF-16 value object begins at wrapper
   offset `+0x08`.
3. Change one captured object during that owned construction scope. The outer
   `Configuration` title changed, proving the operation and disproving the
   target assumption at the same time.
4. Compare the extracted tag graphs for `02_040` and `02_042`; this separates
   the outer `MenuTitle` field from the inner `GraphicOption` heading.
5. Redirect resolution only during construction of a pending custom physical
   page, then apply its precompiled inner title. Resolve and set the outer
   field through a separate native temporary and destroy that temporary's
   nested value through the game's matching destructor.
6. Recheck custom inner titles, provider outer titles, the untouched shared
   root, and Next, Previous, and Back navigation in one live matrix.

Production therefore treats the logical page title and the surrounding menu
title as independent outputs. Shared native first pages keep Elden Ring's
localized title; ERNativeUI-owned continuation and provider pages may use
their compiled presentation policy. The public behavior is documented in
[Menus, pages, and pagination](../../guides/menus-and-pages.md#page-titles).

**Confirmed:** the bounded resolver scope and matching destruction contract
change both intended fields without retaining a borrowed Scaleform value.
**Rejected:** writing through every `MenuTitle/Text_0` resolution or retaining
the returned wrapper beyond the page-construction call. Both would confuse
unrelated movies and cross a lifetime boundary that was never established.

The relevant current implementation is split between
[`native_title_bridge.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_title_bridge.hpp),
the path bridge in
[`hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp),
and the compiled presentation state in
[`pagination.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/pagination.cpp).

## Structural validation and idempotence

The implementation in [`gfx_patch.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tools/gfx_patcher/gfx_patch.cpp)
locates hosts from named children, depths, transforms, edit-field properties,
and surrounding widget structure. It does not identify a current movie from
one hard-coded character ID.

Before producing output it validates, as applicable:

- an uncompressed GFX header, declared movie length, and complete tag bounds;
- only zero padding after the declared movie;
- unique target panels, widgets, names, and depths;
- contiguous `Item_N_0` indices and recognized placement geometry;
- the expected Button label source and TextInput field structure;
- free movie-local character IDs and definition-before-first-use ordering;
- an exact existing presentation or an entirely free insertion site; and
- absence of partial/conflicting CharacterFrame or ColorPicker patches.

The transformer updates the enclosing sprite length and declared movie
length, preserves trailing padding, reparses the completed output, and verifies
the requested structure. The command-line tool writes a temporary file and
moves it into place only after validation; it also refuses identical input and
output paths.

The automated [GFX patcher test](https://github.com/Flammrock/ERNativeUI/blob/main/tests/gfx_patcher_test.cpp) covers
synthetic structures and both checked-in assets. In particular, it verifies:

- every supported row boundary and refusal to shrink;
- TextInput and ColorPicker second-pass byte idempotence;
- the same final bytes whether TextInput or ColorPicker is applied first;
- rejection of occupied and partial/conflicting structures;
- preservation of unrelated Button children and trailing padding; and
- structural inspection of the shipped 13-row and Advanced Settings assets.

**Confirmed:** applying the same requested transformation twice produces the
same bytes. This is idempotence for a recognized movie, not permission to
silently patch an unknown future revision.

## Static GFX versus runtime behavior

| Static movie can provide | Native ERNativeUI/game code must provide |
|---|---|
| A row placement and its visual bounds | A native row model occupying that slot |
| Named Button/TextInput/ColorPicker/ComboBox/Slider destinations | Selection of the correct widget kind |
| Brackets, fill, cursor artwork, text fields, filters, and shadows | Current value, focus, input, callbacks, and validation |
| Additional visible capacity | Pagination based on the constructed page's live capacity |
| A hidden standalone ColorPicker definition | Native editor opening, RGB state, commit/cancel, and teardown |

`ERNativeUI.dll` does not parse, patch, or load these GFX files at runtime.
The files are optional presentation assets; the host uses the game's native
constructors and reads live page state. Where a custom presentation is absent
or fails runtime preflight, the native API keeps a usable fallback when one is
defined—for example, a ColorPicker remains an action row that can open the
native editor.

**Rejected:** adding a placement alone creates a callback, focus target,
native page, event route, or arbitrary new top-level interface.

## Reproducing the static findings

Work only from GFX files extracted from your own current game installation.
Record hashes before changing anything and keep originals, XML exports, and
raw game assets under the ignored `research-work/` directory.

1. Export the tag tree and scripts with the same FFDec version:

   ```powershell
   ffdec.bat -onerror abort -swf2xml `
     'C:\path\to\02_040_optionsetting.gfx' `
     'research-work\gfx\02_040_optionsetting.xml'

   ffdec.bat -onerror abort -export script `
     'research-work\gfx\02_040_scripts' `
     'C:\path\to\02_040_optionsetting.gfx'
   ```

2. Distinguish definitions from placements. Follow each `Item_N_0` character
   reference into its shared generic item, `Widgets`, and widget definitions.
3. Record every relevant name, depth, matrix, filter, scaling grid, image
   reference, and definition position. Do not infer runtime behavior from a
   field or sprite name.
4. Run the repository inspector before writing:

   ```powershell
   ERNativeUIGfxPatcher.exe --inspect `
     --input 'C:\path\to\02_040_optionsetting.gfx'
   ```

5. Apply one transformation at a time to a separate output, inspect it again,
   then apply the same request a second time and compare hashes. The full
   command syntax is in the
   [patcher README](../../../tools/gfx_patcher/README.md#complete-command-line-reference).
6. Build and run `ERNativeUI.GfxPatcher`, which exercises generated fixtures
   and the checked-in release assets.
7. Use a bounded in-game comparison for presentation claims: original versus
   generated movie, root versus child host, focus/hover, value update, commit,
   cancel, page reopen, and two independent rows of the same type.

FFDec may report that `font.swf` was unavailable. That does not invalidate a
tag-tree inventory, but it means an FFDec render is not proof of in-game font
appearance or measurement.

## Game-update workflow

For each changed movie:

1. preserve its filename, byte size, and SHA-256 as a new source identity;
2. inspect it before assuming any old character ID, depth, or panel capacity;
3. compare the named host tree and complete dependency closure with the last
   confirmed sample;
4. let the patcher reject structural drift; do not weaken checks merely to
   obtain output;
5. update a structural matcher only after explaining every changed field;
6. add a regression fixture for the changed structure and rerun idempotence,
   conflict, order-independence, and checked-asset tests;
7. produce a separate output and verify its complete tree and hash; and
8. repeat the bounded in-game matrix before replacing a shipped asset.

A movie that looks similar in FFDec is not automatically compatible. A new
hash requires renewed structural and live evidence.

## Scope of the result

ERUI API 1.1 is current and finished; TextInput and ColorPicker are supported
native controls with optional enhanced presentation. This work proves offline
extension of two existing host movies and runtime binding to known named
widgets.

It does not prove a general runtime `AttachMovie`, arbitrary ActionScript
method invocation, custom movie loader, resource owner, event subscription
bridge, or teardown contract. Loading a provider-authored GFX at runtime and
building wholly custom interfaces remain future research, not an API 1.1
capability or promise.
