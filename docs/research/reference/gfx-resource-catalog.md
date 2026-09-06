# GFX resource catalog and reuse boundaries

This reference explains how Elden Ring names, finds, instantiates, and binds
the GFX resources relevant to ERNativeUI. Its purpose is to make asset reuse
reviewable without treating executable strings as a public asset API.

The safe order of preference is:

1. Use a native page, row, dialog, or controller constructor. Its game-owned
   object already supplies presentation, input, focus, update, and teardown.
2. If the movie is already live, resolve a validated named object and perform
   only an operation whose value ownership is understood.
3. Apply an offline structural patch only to a narrowly validated movie while
   preserving its definitions, dependencies, and binding names.
4. Treat opening another complete movie as research until its descriptor,
   player registration, layer, input route, and teardown have all been
   observed.

A resource-name string proves only that the executable contains and references
that literal. It does not prove that the resource is resident, available in
every game edition, loaded by the referencing function, or safe to construct.
The larger ownership model is described in
[Elden Ring native UI system](../architecture/native-ui-system.md).

## Reference build and evidence boundary

Every RVA and native offset on this page belongs to this exact Windows image:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The entries were curated from the local, ignored
`research-work/exports/string_references.csv`, bounded executable analysis,
static movie inspection, and controlled live tests. Function names beginning
with `FUN_` are Ghidra defaults, not FromSoftware symbols. Evidence labels use
the definitions in the [research index](../README.md#evidence-labels):

- **Confirmed** means static control/data flow, a validated GFX structure, or
  a controlled live test directly establishes the relationship.
- **Inferred** means several observations agree but part of the native
  contract is still missing.
- **Lead only** means an address or literal is useful for navigation, but no
  load, presentation, or ownership behavior is assigned to it.

These coordinates are research evidence, not public ERNativeUI ABI and not
portable addresses for another game build.

## Five distinct resource layers

```text
logical movie descriptor / built-in numeric ID
        |
        v
menu:/Win/<stem>.gfx, then menu:/<stem>.gfx
        |
        v
game resource capability
    |-- resident collection             CSScaleformSystem +0x978
    `-- on-demand cache                  CSScaleformSystem +0x990/+0x998
        |
        v
CSScaleformMovieDef cache                CSScaleformSystem +0x9A8
        |
        v
CSScaleformSwfPlayer + raw GFx::Movie
        |
        v
live named display objects / SceneObjProxy bindings
```

Finding one layer does not create the next:

- a descriptor is not resource bytes;
- cached resource bytes are not a `MovieDef`;
- a `MovieDef` is not a registered, advanced, and rendered player;
- a player is not a native page or focus owner; and
- a named sprite placement is not a native row, callback, or input target.

ERNativeUI currently reuses presentation through higher native owners or a
proven operation on an existing live object. It does not expose raw resource,
movie, or Scaleform pointers to client mods.

## Naming and path conventions

### Logical movie names and loose files

High-signal resources use extensionless logical stems such as:

```text
02_040_OptionSetting
02_042_PC_GraphicSetting
02_990_TextInput
```

The observed form has a two-digit family, a three-digit member, and a
descriptive suffix. That is a naming convention, not a recovered schema;
the digits alone do not establish ownership, layer, or constructor shape.

Native descriptors preserve capitalization. Loose PC files conventionally
appear under lowercase extracted paths, for example:

```text
logical stem:  02_040_OptionSetting
loose file:    menu/win/02_040_optionsetting.gfx
```

Native code should preserve the descriptor it observed. A Mod Engine
deployment should mirror the spelling and path of the user's extracted PC
asset rather than blindly lowercasing an arbitrary future stem.

### Virtual resource paths

The common acquisition routine at RVA `0xD7D540` owns the confirmed
Windows-first fallback:

| Format literal | String RVA | Reference RVA | Use |
|---|---:|---:|---|
| `menu:/Win/%s.gfx` | `0x2BC0310` | `0xD7D586` | First GFX lookup |
| `menu:/%s.gfx` | `0x2BC0338` | `0xD7D6FF` | Non-Windows fallback after a miss |

The `%s` value comes from descriptor offset `+0x08`. The routine returns
resource data/capability ownership; it does not instantiate a movie.

Nearby resource-path builders are useful dependency anchors:

| Function | Literal/reference evidence | Bounded conclusion |
|---:|---|---|
| `0xD78F10` (`FUN_140D78F10`, size `0x224`) | `menu:/%s%s%s.sblytbnd`, `menu:/%s%s.sblytbnd`, and `menu:/%s.sblytbnd` at `0xD78FFB`, `0xD79025`, `0xD79073`, and `0xD79088` | **Confirmed:** constructs several fallback shapes for menu Scaleform-layout bundles. Whether it opens the result itself is unresolved. |
| `0xD79290` (`FUN_140D79290`, size `0x2DE`) | `menu:/%s%s%s.%s`, `menu:/%s%s.%s`, and `menu:/%s.%s` at `0xD7939A`, `0xD793DC`, `0xD79448`, and `0xD79476`; also `tpfbhd` | **Confirmed:** constructs extension-parameterized menu paths, including texture-package-header candidates. Returned ownership is unresolved. |
| `0xD7D850` (`FUN_140D7D850`, size `0x279`) | `menutpfbnd:/00_Solo/%s.tpf`, `menutpfbnd:/71_MapTile/%s.tpf`, `menu:/%s%s.tpf`, and menu category literals | **Inferred:** high-signal texture path/category router. It is not evidence that arbitrary client textures can be registered. |

### Paths inside a live movie

These names address objects in a live movie rather than files in the resource
repository:

```text
WindowList/ControllSetting/Item_0_0/Widgets/Button
Widgets/TextInput/Text_0
MenuTitle/Text_0
```

The validated resolver at `0x74B140` splits `/` and performs component-wise
member lookup. Instance names are case-sensitive binding data; the game asset
really uses `ControllSetting`, so code must not correct its spelling.

An executable path can also differ from the final visible field selected by a
later native binding. `MenuTitle/Text_0` was observed at a constructor
boundary, while the inspected outer movie displays
`MenuTitle/StaticText_101003`. A live target must be verified instead of
treating similar names as aliases. The complete ownership contract is in
[Scene-object proxy and native Scaleform bridge](scene-object-bridge.md).

## Resident, on-demand, definition, and player caches

| RVA or field | Confirmed role |
|---|---|
| `CSScaleformSystem + 0x978` | Resident GFX resource collection |
| `0xD78A60` | Preload the 113-entry built-in descriptor catalog and retain resources in resident storage |
| `CSScaleformSystem + 0x990/+0x998` | On-demand GFX resource cache |
| `0xD79140` | Look up an on-demand resource; resolve and insert it after a miss |
| `CSScaleformSystem + 0x9A8` | `CSScaleformMovieDef` cache/tree |
| `0xD7C370` | Obtain resource data and acquire or reuse a cached movie definition |
| `0xD78E10` | Open a built-in catalog movie by bounded numeric ID and register a player |
| `0xD7A370` | Open and register a player from a caller-supplied 16-byte descriptor |

The built-in preloader also handles region- and language-derived resource
names. A logical stem should therefore not be converted directly into a disk
path by the runtime host.

The following catalog does not label a movie resident or on-demand unless its
route was observed. Registration/name references are not enough; probe the
`0xD78A60`, `0xD79140`, and `0xD7C370` boundaries to make that distinction.

## Curated high-signal movie stems

Every stem below also has a reference inside the executable's name/type
registration material at `FUN_1400AEEF0` (`0xAEEF0`). The registration
reference is separated so it cannot be mistaken for a loader callsite.

| Logical stem | String RVA | Non-registration reference clusters | Registration reference | Evidence and reuse value |
|---|---:|---|---:|---|
| `02_040_OptionSetting` | `0x2AC11E8` | `FUN_140809660`: `0x8096CE`, `0x8096D5` | `0xAF8F4` | **Confirmed movie and live use.** It is the outer Configuration movie used by native settings rows. The optional patch extends row capacity and supplies repaired action-label, TextInput, and ColorPicker presentation. |
| `02_042_PC_GraphicSetting` | `0x2AC12D8` | `FUN_140808860`: `0x8088E9`, `0x8088F0` | `0xAF91E` | **Confirmed movie and live use.** It is the Advanced Settings-style physical page used by provider subpages and continuation slices. Its optional TextInput and ColorPicker presentation must be patched independently from `02_040`. |
| `02_044_PC_TextSelect` | `0x2B14850` | `FUN_14094FDB0`: `0x94FF46`, `0x94FF4D`; `FUN_14094EEA0`: `0x94EF31`, `0x94EF38` | `0xAF92C` | **Lead only.** Useful for studying PC text/list selection; the name and references do not prove a reusable constructor. |
| `02_160_KeyConfiguration` | `0x2AC1348` | `FUN_140808510`: `0x808615`, `0x80861C`; `FUN_140808770`: `0x8087EC`, `0x8087F3` | `0xAF948` | **Lead only for direct movie reuse.** ERNativeUI's confirmed input-binding integration reaches the native screen and row model rather than opening this stem directly. |
| `02_990_TextInput` | `0x2AC5A78` | `FUN_14081CFD0`: `0x81D0BB`, `0x81D0C2` | `0xAF68F` | **Confirmed active editor and live use.** The character-name activation route selects this 400-pixel editor through `0x81D610 -> 0x81CFD0`; controlled tests established its 16-character behavior. |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081D160`: `0x81D24B`, `0x81D252`; `FUN_14081D2F0`: `0x81D3DB`, `0x81D3E2`; `FUN_14081D480`: `0x81D56B`, `0x81D572` | `0xAF69D` | **Confirmed active editor family; one route tested live.** The generic row factory can select the narrower editor through `0x81D700`; it reproduced an eight-character limit. The other two wrappers are statically mapped but not behaviorally classified. |
| `01_010_MessageBox` | `0x2AADC38` | `FUN_1407BB9B0`: `0x7BBAE2`, `0x7BBAE9`; `FUN_1407BBE90`: `0x7BC002`, `0x7BC009`; `FUN_1407B7E90`: `0x7B7F24`, `0x7B7F2B`; `FUN_1407B05B0`: `0x7B064D`, `0x7B0654` | `0xAEF40` | **High-signal lead.** Its name fits the working native alert family, but the confirmed job-builder path has not been tied conclusively to this exact movie. |
| `01_011_MessageBox_Small` | `0x2AAFD98` | `FUN_1407FA960`: `0x7FAA10`, `0x7FAA17`; `FUN_1407BD6E0`: `0x7BD8F4`, `0x7BD900`; `FUN_1407BBD80`: `0x7BBE15`, `0x7BBE1C`; `FUN_1407B84E0`: `0x7B8575`, `0x7B857C` | `0xAF02A` | **High-signal variant lead.** Its presentation and relationship to a builder remain unresolved. |
| `01_013_MessageBox_Small_NB` | `0x2AAFDC8` | `FUN_1407BD6E0`: `0x7BD8DA`, `0x7BD900` | `0xAF159` | **High-signal variant lead.** Expanding `NB` as "no button" remains a hypothesis until a natural caller and presentation are observed. |

For the production conclusions behind these rows, see the
[TextInput](../case-studies/text-input.md),
[Input Bindings](../case-studies/input-bindings.md),
[Native Dialogs](../case-studies/native-dialogs.md), and
[GFX presentation](../case-studies/gfx-presentation.md) case studies.

## Curated in-movie and dependency anchors

| Literal | String RVA | Reference RVA(s) | Bounded interpretation |
|---|---:|---|---|
| `Widgets/TextInput` | `0x2AD74E8` | Notably `0x86BD90` in `0x86BCB0`; other widget-selection references exist | **Confirmed:** `0x86BCB0` hides Slider, ComboBox, DropdownList, and Button, then selects TextInput. Other references require individual classification. |
| `Widgets/TextInput/Text_0` | `0x2B1AF58` | `0x976F6E` | **Confirmed:** a higher text-row producer resolves the displayed-value member before creating `TextInputController`. |
| `Widgets/TextInput/Input` | `0x2B1AF78` | `0x976FBE` | **Lead only:** native code resolves this name, but no matching static child exists in the inspected `02_040`; it may be runtime-created or variant-dependent. |
| `TextInput/Text_0` | `0x2B2B970` | `0x9B9E6D` | **Confirmed:** standalone `TextInputDialog` construction anchor. |
| `MenuTitle/Text_0` | `0x2A96C20` | Fourteen references, including `0x742931`, `0x7D4160`, and `0x96AD71` | **Confirmed as a common native title-source path.** Its broad use requires each caller to be classified before hooking. |
| `font.swf` | `0x2BC02D0` | `0xD783C5` | **Confirmed:** game font/translation loader-state path; also imported by inspected `02_040`. |
| `gfxfontlib.swf` | `0x2CC3B28`, `0x2CC41D0` | `0x116920E`, `0x116FF89`, `0x116F109` | **Confirmed generic Scaleform runtime strings, not an Elden Ring movie-open wrapper.** |

The inspected `02_040` declares 84 external `.tga` images through
`DefineExternalImage2`; their names and bytes are intentionally not cataloged.
Copying a sprite that uses them without its resource resolver is incomplete.

The TextInput presentation work established one narrower result: a
movie-local `DefineExternalImage2`, wrapper sprite, scaling grid, and direct
`PlaceObject3` placement can render without a new `DoABC` class or
`SymbolClass` entry when all definitions precede the consuming sprite. That is
a property of the validated direct display-list route, not a general rule for
runtime ActionScript construction. See the
[GFX presentation case study](../case-studies/gfx-presentation.md).

## Supported and unsupported reuse boundaries

| Reuse strategy | Current boundary |
|---|---|
| Native constructors | **Production.** Existing settings and dialog constructors select game presentation while retaining native focus, callbacks, sound, and teardown. |
| Named object in an existing movie | **Production only for validated operations.** The host resolves a path through `0x74B140`, owns the complete `0x60`-byte proxy, performs a known operation such as UTF-16 text assignment through `0x74AE50`, and destroys its embedded value through `0xD81590`. |
| Offline extension of an inspected movie | **Supported for the patcher's validated transforms.** Definitions, depths, transforms, lengths, character IDs, imports, and idempotence are checked. Character IDs remain movie-local. |
| Open a complete existing movie | **Research boundary.** `0xD78E10` and `0xD7A370` are high-level observation anchors, but a public route still needs descriptor invariants, layer/viewport policy, input/focus ownership, cache behavior, thread, failure, and deterministic close/release. |
| Copy a sprite between movies | **Unsupported.** A sprite definition does not carry its transitive character definitions, class bindings, fonts, images, or native instance-name contract. |
| Generic client-facing Scaleform API | **Unsupported.** The current bridge does not establish general `SetMember`, `Invoke`, `AttachMovie`, stage construction, or a safe persistent `GFx::Value` lifetime. |

Public installation and transformation options are documented in
[Presentation and optional GFX assets](../../guides/presentation-and-gfx.md)
and the [GFX patcher README](../../../tools/gfx_patcher/README.md).

## Reproduce the catalog

### 1. Query exact names

The generated CSV remains local. Query a bounded list rather than publishing
the executable's complete string inventory:

```powershell
$gfxNames = @(
    '02_040_OptionSetting',
    '02_990_TextInput',
    '02_991_TextInput2'
)

Import-Csv .\research-work\exports\string_references.csv |
    Where-Object { $gfxNames -contains $_.value } |
    Sort-Object value, reference_rva |
    Select-Object string_rva, value, reference_rva, reference_type
```

For discovery, use a narrowly bounded expression such as
`^02_[0-9]{3}_.*Text.*$`, inspect the small result, and then return to exact
names. Do not commit the unfiltered CSV.

### 2. Map references to owners

Use `research-work/exports/functions.csv` or the bounded `Anchors` query in the
[Ghidra workflow](../tools/ghidra-workflow.md). Classify each result as one of:

- registration/name material;
- resource descriptor production;
- resident preload;
- on-demand acquisition;
- `MovieDef` acquisition;
- player creation/registration; or
- page/controller code selecting an already-live object.

Two adjacent references can indicate a copy or constructor path, but do not
by themselves reveal a function signature.

### 3. Follow a bounded ownership graph

| Boundary | Question answered |
|---:|---|
| `0xD7D540` | Does this path acquire GFX resource bytes? |
| `0xD78A60` | Is it reached through resident preload? |
| `0xD79140` | Is it acquired through the on-demand cache? |
| `0xD7C370` | Does it become or reuse a cached `MovieDef`? |
| `0xD7C900` | Does it create/register a `CSScaleformSwfPlayer`? |
| `0xD7ADE0` / `0xD73850` | Is the player in the normal per-frame advance collection? |

Stop when the graph reaches a generic allocator, string utility, or linked GFx
implementation without a typed game owner. Lower-level code is not
automatically a safer hook.

### 4. Inspect a lawfully extracted candidate

Keep original assets outside the repository, record their hashes, and export
only local structural data with FFDec:

```powershell
ffdec.bat -onerror abort -swf2xml `
    'C:\path\to\candidate.gfx' `
    'research-work\gfx\candidate.xml'

ffdec.bat -onerror abort -export script `
    'research-work\gfx\candidate-scripts' `
    'C:\path\to\candidate.gfx'
```

Inspect the GFX/SWF and ActionScript versions, stage, frames, labels, imports,
external images, exported symbols, `SymbolClass` entries, scripts/listeners,
exact instance names, shared definitions versus placements, and all font,
texture, and nested-movie dependencies.

`ERNativeUIGfxPatcher.exe --inspect` is a structural check for compatible
`02_040_optionsetting.gfx` files. It is not a generic catalog parser and should
reject unrelated candidates.

### 5. Observe one natural lifecycle

Before calling a candidate, observe one vanilla open and close. A bounded
probe should record only:

- descriptor or numeric ID and callsite;
- resident, on-demand, definition, and player boundaries reached;
- thread ID;
- returned owner/player pointer and vtable;
- insertion into and removal from the active-player collection; and
- retain/release transitions.

Cap the probe by count and time, avoid localized user text, and do not retain
an extra game reference. Reproduce the same high-level call only after the
natural lifecycle is understood.

## Promotion checklist for a complete movie

An asset should enter a production reuse table only when every applicable item
is satisfied:

1. Exact executable and asset hashes are recorded.
2. Logical stem, platform/fallback path, and descriptor layout agree.
3. Resident or on-demand behavior is observed rather than guessed.
4. `MovieDef` and player ownership are followed through final release.
5. Update/render registration and viewport/layer behavior are confirmed.
6. Controller, mouse, keyboard, Back, and modal focus behavior are tested.
7. Font, image, locale, and region dependencies resolve through game owners.
8. Failure disables only the optional feature and leaves native UI intact.
9. Address discovery uses a validated relation/signature, not a bare RVA.
10. No client receives a raw game or Scaleform pointer that it can outlive.

## Legal and artifact boundary

- ELDEN RING's executable, original GFX/SWF files, fonts, textures, and other
  game data remain the property of their respective owners. ERNativeUI's MIT
  license does not relicense them.
- Keep the executable, unmodified extracted movies, FFDec XML/script/image
  exports, full string indexes, and bulk disassembly under ignored
  `research-work/` or another local directory.
- Publish derived facts, small analytical diagrams, hashes, RVAs, signatures,
  original tooling, and narrowly written interoperability documentation rather
  than proprietary asset dumps.
- The optional modified `02_040_optionsetting.gfx` and
  `02_042_pc_graphicsetting.gfx` are a deliberate existing distribution
  decision documented in
  [`THIRD_PARTY_NOTICES.txt`](../../../THIRD_PARTY_NOTICES.txt). That decision
  does not authorize adding other extracted GFX, `font.swf`, TGA images, or a
  complete asset catalog.
- When tooling needs original asset bytes, ask users to extract them from
  their lawfully owned game. Prefer a reproducible transform over distributing
  another unmodified movie.

The unresolved boundary is ownership, not discovery: a name alone cannot say
who advances, renders, focuses, and releases the resulting movie. Until that
contract is proved end to end, catalog entries remain navigation anchors
rather than a public `open_gfx(name)` interface.
