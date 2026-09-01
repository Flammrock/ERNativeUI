# Scaleform/GFX presentation model

Status: initial static model, native dispatch still under investigation

This document records what is currently known about Elden Ring's PC Game
Options movies, how ERNativeUI reaches them, and which extension mechanisms have
actually been demonstrated. It deliberately separates the static GFX display
tree from Elden Ring's native page, row, input, and event objects.

The names used for native functions are ERNativeUI research names, not
FromSoftware symbols. See the evidence vocabulary in [the analysis
index](README.md).

## Evidence and provenance

The static observations below were reproduced with:

- JPEXS Free Flash Decompiler (FFDec) 26.2.1;
- `ERNativeUIGfxPatcher.exe` from this repository;
- the user's extracted PC `02_040_optionsetting.gfx` and
  `02_042_pc_graphicsetting.gfx`;
- ERNativeUI's source and the bounded live observations already documented in
  [native addresses and pagination](../NATIVE_ADDRESSES_AND_PAGINATION.md) and
  [page presentation](../PAGE_PRESENTATION.md).

The corresponding game executable is version 2.7.0.0 with PE timestamp
`0x69E9C9B9` and image size `0x5E09600`.

| Movie | Size | SHA-256 | Role |
|---|---:|---|---|
| User-extracted original | 44,016 bytes | `2B09D4E64EAB964F8D4045D25960F4C6CE1806D6596C75A21F28F334D0CD10B3` | Six-row reference kept outside the repository |
| Repository `02_040` asset | 44,409 bytes | `2A58DA5629C3E35F91676EBA42CB2AF131922EA929F898DEF57C49F1E17B5F89` | Optional thirteen-row, Button-label, and character-name TextInput presentation patch, confirmed in game |
| User-extracted Steam `02_042` source | 27,056 bytes | `E87477878203AC04104FDC3617D870BD44A0BF8C14F1A8E20922F59605867DAC` | Unmodified fifteen-row Advanced Settings host used as the patch input |
| Repository `02_042` asset | 27,196 bytes | `8E8F1CA0A7EDF82D2A081CCF1E915C06998B900D387A940008CC2A9C182D7C6B` | Optional character-name TextInput presentation, confirmed in game |

Applying the repository patcher to that original with `--controller-rows 13`
and `--text-input-presentation character-name` produced the repository
`02_040` asset byte for byte. The combined operation added 393 bytes. Full XML exports,
ActionScript exports, raw game assets, and other generated research artifacts
stay in the ignored `research-work/` directory.

## Terminology

Three different things are easy to conflate:

- A **character definition** is one reusable sprite, text field, shape, or
  image definition in the GFX/SWF character dictionary.
- A **placement** puts a character at a depth, transform, and optional instance
  name inside a timeline.
- A **runtime instance** is the display object Scaleform creates from a
  placement when the movie runs.

Consequently, all Controller rows reference the same generic-row definition,
but each `Item_N_0` placement creates its own runtime row instance. Each
runtime row therefore has its own `Widgets` child instance even though the
underlying `Widgets` and widget definitions are shared.

This distinction is also important for patching: changing a shared definition
affects every runtime instance that references it, while adding a placement
only adds another instance of the existing definition.

## Confirmed file-level properties

`02_040_optionsetting.gfx` is an uncompressed Scaleform GFX/SWF version 11
movie using ActionScript 3. Its declared stage is 38,400 by 21,600 twips,
equivalent to 1920 by 1080 pixels at the conventional 20 twips per pixel. It
runs at 30 frames per second and its main timeline has 19 frames.

The movie contains:

- 84 `DefineExternalImage2` resources referencing game `.tga` assets;
- one `ImportAssets2` dependency on `font.swf`;
- 23 `DefineEditText` fields using `MenuFont_01`;
- 78 sprite definitions in the patched movie;
- one `DoABC2` ActionScript block and one `SymbolClass` table.

The main timeline exposes these named objects:

```text
MainTimeline
|-- StatusBar              sprite 88
|-- WindowList             sprite 166
|-- BackTabList            sprite 177
|-- TabList                sprite 188
`-- MenuTitle              sprite 192
```

Its only frame scripts stop frames 1, 11, and 19. Frames 11 and 19 correspond
to the `FadeIn` and `FadeOut` labels.

### ActionScript is presentation-only in this movie

FFDec exported 92 ActionScript classes:

- 84 thin `BitmapData` wrappers for external images;
- seven timeline classes for animated or stateful sprites;
- the main timeline class.

The exported code contains frame stops and two animation loops using
`gotoAndPlay("Loop")`. It contains no `addEventListener`, `dispatchEvent`,
`ExternalInterface`, focus handler, mouse handler, or menu-specific input
dispatcher. The `TextInput` sprite has no `SymbolClass` entry and therefore no
custom ActionScript class in this movie.

This confirms only that **this movie** does not implement the Game Options
business logic in ActionScript. It does not prove that no other Elden Ring GFX
movie uses ActionScript events.

## Confirmed display tree

### Native settings panels

`WindowList` is sprite 166. It statically places these named panel instances:

```text
WindowList
|-- CameraSetting          sprite 127
|-- GameEnd                sprite 138
|-- BrightnessSetting      sprite 147
|-- ControllSetting        sprite 148
|-- NetworkSetting         sprite 157
|-- AudioSetting           sprite 158
|-- EnvironmentSetting     sprite 158
|-- PCMouseKey             sprite 159
|-- PCGraphic              sprite 160
`-- PadSetting             sprite 165
```

Several panel names intentionally share the same sprite definition. The
static file does not reveal which native object selects a panel, controls its
visibility, or binds its data.

### Controller rows and shared widgets

In the original movie, `ControllSetting` contains six placements named
`Item_0_0` through `Item_5_0`. The optional repository asset contains thirteen
placements through `Item_12_0`:

```text
WindowList.ControllSetting                 sprite 148
`-- Item_N_0                               placement of sprite 126
    `-- Widgets                            instance of sprite 125
        |-- Button                         instance of sprite 97
        |-- TextInput                      instance of sprite 103
        |-- ComboBox                       instance of sprite 116
        `-- Slider                         instance of sprite 124
```

Every `Item_N_0` placement references character 126. Rows are spaced by 1,000
twips, or 50 pixels: row zero is at Y `-4800` and row twelve is at Y `7200`.
The `Widgets` instance is translated by X `4000` inside the generic row.

All four widget placements exist in each runtime row instance. The GFX alone
does not show the native discriminator that makes one widget the active row
presentation. Native visibility/state selection is therefore still an
inference, not a confirmed implementation detail.

There is a second generic row variant:

```text
generic row                               sprite 156
`-- Widgets                               instance of sprite 155
    |-- Button                            instance of sprite 154
    |-- TextInput                         instance of sprite 103
    |-- ComboBox                          instance of sprite 116
    `-- Slider                            instance of sprite 124
```

The second variant changes only the Button definition and placement depths.
Both variants reuse the exact same TextInput, ComboBox, and Slider character
definitions. Network, keyboard/mouse, graphics, and pad panels use this second
variant for at least some rows; Controller Settings uses sprite 126.

### TextInput presentation

The shared TextInput character is sprite 103:

```text
TextInput                                 sprite 103, one frame
|-- unnamed row/background visual         sprite 89, depth 1
|-- Text_0                                edit-text character 98, depth 3
|-- TextOnEmpty                           edit-text character 99, depth 4
|-- Caption                               sprite 101, depth 5
|   `-- Text_0                            edit-text character 100
`-- Cursor                                sprite 102, depth 7, 60 frames
```

Important confirmed details:

- `Text_0` and `TextOnEmpty` are both `DefineEditText` fields.
- Both are marked `readOnly=true`, `multiline=true`, `wordWrap=true`, and
  `noSelect=false` in the file.
- Neither declares a maximum length.
- Both use the Japanese authoring placeholder `テキスト入力` ("text input").
- `TextOnEmpty` uses HTML formatting and a darker color, consistent with an
  empty-value placeholder.
- `Caption` is the same character 101 used by ComboBox and Slider.
- `Cursor` is a visual timeline. The name alone does not prove that it is a
  text caret; the native code that drives it remains unknown.
- Every one of the movie's 23 edit-text fields is marked `readOnly=true` in
  the asset.

There is **no named child `Input`** beneath sprite 103, no `Input` instance
elsewhere in this movie, and no standalone `Input` literal in its tag or ABC
data. The executable strings `Widgets/TextInput`, `TextInput/Text_0`, and
`TextInput/Input` remain valuable native-code anchors, but the last string
must not be described as a confirmed display-tree path in this asset.

The read-only flag does not prove that editing is impossible: native code may
change TextField properties at runtime, may present a separate input dialog,
or may copy a value obtained elsewhere into `Text_0`. It does prove that
rendering this sprite is not by itself a complete editable control.

### The same TextInput contract in Advanced Settings

The prototype subpage is rendered by `02_042_pc_graphicsetting.gfx`, not by
`02_040_optionsetting.gfx`. The current Steam copy was the unmodified source
for the repository's narrowly patched `02_042` asset recorded above. Its host
tree is:

```text
GraphicOption                            sprite 90
`-- Item_N_0                             placement of sprite 80, N = 0..14
    `-- Widgets                          instance of sprite 79, X = 4000
        |-- Button                       instance of sprite 55, depth 1
        |-- TextInput                    instance of sprite 61, depth 14
        |-- ComboBox                     instance of sprite 72, depth 22
        `-- Slider                       instance of sprite 78, depth 36
```

Unlike Controller Settings, this movie already authors fifteen row
placements. It has no `WindowList.ControllSetting` panel, so Controller row
expansion and Button-label repair must not be prerequisites for a TextInput
presentation patch.

The Advanced Settings TextInput is structurally the same widget under
different movie-local character IDs:

```text
TextInput                                 sprite 61, one frame
|-- unnamed row/background visual         character 47, depth 1, (-15800, -840)
|-- [depth 2 is free]
|-- Text_0                                edit-text character 56, depth 3, (-4360, -640)
|-- TextOnEmpty                           edit-text character 57, depth 4, (-4360, -640)
|-- Caption                               sprite 59, depth 5, (-9120, 0)
`-- Cursor                                sprite 60, depth 7, (0, 0)
```

Characters 56 and 57 have the same field geometry and flags as Controller
characters 98 and 99: centered `MenuFont_01`, a `[-40, 7960] x [-40, 680]`
twip field, font height 480, and the `readOnly`, `multiline`, and `wordWrap`
flags. The only host-level geometry difference relevant to discovery is the
Caption X translation: `-9120` here versus `-9520` in `02_040`. The value and
empty fields themselves are identical, so the character-name frame geometry
can be shared between the two movies.

#### Safe cross-movie discovery and patch constraints

A reusable transformer must discover the widget from structure, not from
sprite 103 or 61. The fail-closed signature used for these two samples is one
sprite with unique named children `Text_0`, `TextOnEmpty`, `Caption`, and
`Cursor`, at depths 3, 4, 5, and 7 with the transforms above. The referenced
value and empty characters must be the supported `DefineEditText` fields, the
placeholder must be the recognized native gray or already-patched red form,
and depth 2 must be free or contain the exact idempotent `CharacterFrame`
placement. A parent `Widgets` sprite containing named `Button`, `TextInput`,
`ComboBox`, and `Slider` children is useful corroborating evidence and guards
against an unrelated sprite acquiring the same child names in a future movie.

Character IDs are local to each GFX. `02_042` uses a dense definition range
through 90 and external-image IDs 1 through 37, so the natural new IDs in the
unmodified sample are 91 for the external image reference and 92 for its
wrapper. A transformer should nevertheless derive IDs from all character
definitions instead of hard-coding those values.

The dependency closure must be ordered before its first use: define the new
`MENU_FL_Cursor_EntWaku` external image reference, wrapper sprite, and scaling
grid before the TextInput sprite, then place the wrapper at depth 2 before the
TextInput's `ShowFrame`. FFDec accepts a forward reference, but the game's
Scaleform loader can silently omit that visual. Direct display-list placement
uses the movie-local character ID; it does not require importing the donor
movie's ActionScript `BitmapData` class or adding a `SymbolClass` entry. The
patch must also update the affected sprite and movie lengths, preserve trailing
padding, reparse the result, and verify byte-idempotence.

For both hosts the imported frame placement is centered on the value field at
`(-487, -320)`, with X scale `1.0`, Y scale `31457/65536`, and RGB multiplier
`225/256`. The wrapper places the 428 by 108 external image at
`(-4193, -1080)` and carries scaling-grid bounds `[-80, 80] x [-160, 180]`.
The red `TextOnEmpty` change and these structural values were applied to a
generated `02_042` probe. On 1 September 2026 the ERNativeUI Text Input
Prototype subpage displayed the native bracket frame and red empty-state text
in game. The presentation is therefore confirmed for both host movies on the
tested build; this is presentation evidence, not by itself a public TextInput
ABI or a claim about future GFX revisions.

The native idle/active split, character-creation donor tree, empty-state style,
and keyboard editor route are documented in [the TextInput
analysis](TEXT_INPUT.md).

### Buttons and the optional label patch

The original Controller Button, sprite 97, contains only its right-side
`Text_0` value wrapper. The other generic row's Button, sprite 154, already has
separate `Text_1` and `Text_0` fields.

The repository patch adds to Controller Button 97:

```text
Button
|-- Text_0                                existing right-side value
`-- Text_1                                new placement of sprite 193
    `-- Text                              edit-text character 100
```

Sprite 193 is a clone of the native Caption wrapper, sprite 101. Its sole
binding is renamed from `Text_0` to `Text`; its transform and native drop
shadow remain unchanged. Live tests established that the exact nested name is
required for native action-row label binding.

When the row-expansion/Button-label transformation is isolated, static
comparison confirms that TextInput 103, ComboBox 116, Slider 124, Widgets 125,
generic row 126, and WindowList 166 are unchanged. The bundled asset then
applies the independent character-name presentation transformation to
TextInput 103. That second transformation is optional for native editing, but
provides the bracketed idle field and red empty-value presentation validated
for the public TextInput row.

### Tabs are also statically placed

`TabList` sprite 188 contains:

- `LeftArrow` and `RightArrow` instances;
- nine item instances named `Item_0_0` through `Item_0_8`;
- one `Text_0` field.

`BackTabList` sprite 177 likewise contains nine item instances. This proves
that the current asset supplies nine visible tab slots. It does **not** prove
that nine is a native maximum or that adding a tenth placement would register
a native tab. The native tab collection, index mapping, event routing, and
overflow behavior still need executable analysis.

## Confirmed native-to-Scaleform bridge

ERNativeUI currently uses three private native helpers. Their current RVAs
were resolved by AOB and exercised on the tested executable:

| Research name | RVA | Observed contract |
|---|---:|---|
| Scaleform path resolver | `0x74B140` | `(movie_context, destination, char_path_format, ...) -> destination`; current production paths contain no format directives |
| Scaleform UTF-16 text setter | `0x74AE50` | `(scaleform_value, wchar_text)` |
| `CSScaleformValue` destructor | `0xD81590` | `(value_wrapper_at_result_plus_0x28)` |

The path resolver constructs a 0x60-byte `SceneObjProxy` result. Static analysis
of the complete constructor and the live title calls refines the previously
observed offsets:

- result `+0x00` is the final `SceneObjProxy` vtable (`0x2A97AF0` in this
  build);
- the base initializer at `0x733F00` first installs the `ComponentProxy`
  vtable (`0x2A92B88`) and sets `+0x08`, `+0x10`, and `+0x18` as self-links;
  the derived constructor then replaces only the vtable with
  `SceneObjProxy`'s;
- result `+0x20` retains the source proxy's still-untyped owner/player pointer;
- result `+0x28` is an embedded 0x38-byte `CSScaleformValue`; its
  `GFx::Value` base/subobject begins at result `+0x30`;
- both `SceneObjProxy` virtual accessors (`0x74C940` and `0x74C930`) return
  `this + 0x28`, the `CSScaleformValue` wrapper.

The text setter is passed result `+0x08`, but that address is not itself the
`GFx::Value`. The setter dereferences the self-link to recover the proxy, calls
its second virtual accessor, and receives the wrapper at `+0x28`. The cleanup
helper is passed `+0x28` directly; it installs the `CSScaleformValue` vtable
and releases the contained GFx value beginning at `+0x30`. This
explains why both previously observed offsets were required rather than
representing two unrelated values.

In the observed Game Options page flow:

- a page-owned persistent result occupies page offsets `[0x230, 0x290)`;
- short-lived one-shot resolutions use an aligned, zero-initialized 0x60-byte
  result and explicitly destroy the nested member afterward.

The resolver is also more capable than a single flat lookup. The constructor
at `0x74B610` copies the source proxy's owner, initializes a destination
`CSScaleformValue`, and calls `0xD81710`. That helper splits the supplied path on
`/` and resolves each segment in sequence. Its direct-member helper at
`0xD81680` accepts only object-like GFx value kinds and calls the source
object-interface virtual slot `+0x20` with the source object data, member name,
and destination value. This is the confirmed native implementation behind
paths such as `GraphicOption/StaticText_111114`; it is not merely a string
table convention.

The title bridge has exercised these runtime paths:

```text
MenuTitle/Text_0                       native constructor source path
GraphicOption/StaticText_111114        visible inner subpage heading
MenuTitle/StaticText_101003            visible outer options heading
```

`MenuTitle/Text_0` was observed as a native resolver input, but it is not the
name of the visible field inside `02_040`: that field is
`MenuTitle/StaticText_101003`. Similar names from native code and named display
objects must not be assumed to identify the same visual target.

The page owns the persistent result. ERNativeUI retains only non-owning
pointers from the narrowly matched resolver call until the immediately
following owned subpage-handler callback. It uses an independent temporary
result for the outer title so it does not overwrite the page-owned inner-title
value.

These observations prove resolution and UTF-16 assignment to existing named
objects. They do not yet prove a general property setter, method invocation,
runtime character instantiation, movie loading API, or event subscription API.

## Current ERNativeUI hook levels

ERNativeUI presently operates primarily above Scaleform:

1. The Controller hub detour calls Elden Ring's original handler first.
2. It then calls proved native row constructors with the live native page,
   native text references, state pointers, and row-specific context objects.
3. For an ERNativeUI-owned child page, `open subpage` creates/pushes the native
   page, and the subpage-handler detour materializes that page's native rows.
4. Native Back pops the native page rather than manually hiding a movie clip.
5. The lower path-resolver hook is used only for the narrowly scoped title
   bridge described above.

This architecture is why existing controls inherit native input, focus,
selection, sound, and teardown behavior: ERNativeUI asks the game's native UI
layer to construct rows instead of directly drawing look-alike GFX objects.
The exact native path from a row constructor to the widget instance and event
dispatcher remains a target of the executable analysis.

## What “extend Scaleform” can mean

The phrase covers several materially different techniques. Their current
status is:

| Technique | Status | What is proved |
|---|---|---|
| Call native page/row constructors | Confirmed | Toggle, inline choice, popup choice, slider, button, submenu and navigation paths work |
| Resolve an existing named display object and set UTF-16 text | Confirmed, narrow | Page and outer titles work with proved result lifetimes |
| Add or clone static placements offline | Confirmed | Controller capacity and the missing Button label field can be extended safely |
| Set arbitrary GFx properties | Unknown | Only the specialized text setter is typed |
| Invoke arbitrary ActionScript methods | Unknown | No invoke boundary has been recovered |
| Instantiate a sprite at runtime | Unknown | No `CreateObject`/`AttachMovie` equivalent has been recovered |
| Load a new custom GFX movie | Unknown | Loader, resource ownership, registration, and teardown are unmapped |
| Subscribe a native callback to a GFx event | Unknown | Dispatcher and callback object ABI are unmapped |
| Add an unbounded native tab collection | Unknown | The asset has nine slots, but the native model is unmapped |

For ERNativeUI, the safest first choice remains the highest native interface
that already owns the desired behavior. Direct Scaleform manipulation is
appropriate only when the native layer has no suitable construction path and
the required value/event lifetimes have been recovered.

## Extension constraints established so far

- Adding a display-object placement does not by itself create a native row,
  callback, focus target, or event route.
- Editing a shared sprite definition affects every row instance that
  references it.
- Runtime instance names and placement depths are part of the binding
  contract. A visually correct clone with the wrong nested name can display
  unresolved placeholder glyphs.
- Character IDs are local implementation details of one movie revision.
  Production transformations must locate and validate structure instead of
  blindly assuming an ID.
- A structural patch must preserve tag boundaries, unique depths, sprite
  lengths, total movie length, trailing padding, and idempotence.
- A custom movie using these assets would also need the game's external image
  and `font.swf` resolution behavior; copying a sprite definition alone does
  not establish that resource pipeline.
- UI operations observed so far run as part of the native UI flow. No evidence
  supports calling the bridge from an arbitrary worker thread.
- The optional GFX patch should remain presentation capacity, not a hard
  runtime dependency where a native fallback is possible.

## Working model: confirmed edges and inferred middle

The following high-level model fits all current observations:

```text
native page construction
        |
        v
native row model/constructor                    confirmed
        |
        v
row-to-widget binding and property updates      inferred, implementation open
        |
        v
Item_N_0 -> Widgets -> selected widget           confirmed presentation tree
        ^
        |
native input/event dispatch                     inferred, implementation open
```

The two open middle edges are exactly where whole-executable analysis should
focus. The GFX already tells us the destination names; the known native row
constructors give us callers from the other side.

## Executable-analysis targets

The next useful discoveries are, in priority order:

1. Cross-references to `Widgets/TextInput`, `TextInput/Text_0`, and
   `TextInput/Input`, with special attention to why `Input` is absent from the
   static movie tree.
2. The shared function that maps a native row kind to Button, TextInput,
   ComboBox, or Slider presentation.
3. The owner and vtable/type of the `movie_context` argument accepted by the
   path resolver.
4. The concrete type stored in the 0x60-byte path result and the complete
   operation set around its `+0x08` value.
5. The native event submission/dispatch route for row selection, left/right,
   confirm, cancel, focus, and page teardown.
6. The exact relationship between a native page, each native row object, and
   its `Item_N_0` runtime display instance.
7. Any GFx value operations equivalent to get/set member, invoke, create
   object, create array, attach movie, and set display info.
8. The movie loader/resource registry and the destruction path for a loaded
   movie.
9. The native tab descriptor/container, its visual-slot assignment, and
   L1/R1/mouse dispatch.
10. TextInput's mutable string owner, activation object, commit/cancel events,
    validation, maximum length, and focus release.

Each recovered function should be recorded with its RVA, analytical name,
callers/callees, proposed signature, object offsets, evidence level, and game
identity. A nearby function or a plausible decompiler signature is not enough
to promote it into production.

## Reproducing the static inspection

With a user-extracted movie and FFDec 26.2.1:

```powershell
ffdec.bat -onerror abort -swf2xml `
  'C:\path\to\02_040_optionsetting.gfx' `
  'research-work\gfx\02_040_optionsetting.xml'

ffdec.bat -onerror abort -export script `
  'research-work\gfx\02_040_scripts' `
  'C:\path\to\02_040_optionsetting.gfx'
```

The repository's narrow structural inspector can independently verify the
Controller panel and row count:

```powershell
ERNativeUIGfxPatcher.exe --inspect `
  --input 'C:\path\to\02_040_optionsetting.gfx'
```

FFDec may warn that `font.swf` was not loaded during export. That does not
invalidate the tag tree or ActionScript inventory, but visual rendering and
font-dependent measurements should not be inferred from that export alone.
