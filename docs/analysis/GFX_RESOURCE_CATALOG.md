# GFX resource catalog and reuse boundaries

Status: curated exact-build catalog; safe instantiation remains narrower than
resource discovery

This note answers a practical question: **how can ERNativeUI reuse existing
Elden Ring GFX assets without treating the executable's string table as a
public asset API?**

The short answer is:

1. Prefer native page, row, dialog, and controller constructors. They select
   the game's existing GFX and already own input, focus, update, and teardown.
2. When a movie is already live, resolve a validated named object and use only
   operations whose value lifetime is understood.
3. Use offline structural modification only for a narrowly validated movie
   and preserve its dependencies and binding names.
4. Treat opening another built-in movie as research until its descriptor,
   player registration, layer placement, input route, and teardown have all
   been observed.

A GFX resource name in the executable proves that native code knows the
literal. It does **not** by itself prove that the resource is resident, that it
is present in every game edition, that the referencing function loads it, or
that calling a nearby function is safe.

## Build and evidence scope

The executable coordinates in this document apply only to:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The curated names and references came from the already-generated,
read-only `research-work/exports/string_references.csv`, the current static
movie inspections, and the established resource/movie lifecycle map. The
active Ghidra database was not opened for this pass.

Evidence labels follow the analysis index:

- **Confirmed** means static control/data flow, a validated GFX structure, or
  a controlled live test establishes the relationship.
- **Inferred** means the evidence strongly suggests the role, but a complete
  type or call contract is still missing.
- **Lead only** means the literal and its references are useful navigation
  anchors; no loader or presentation behavior is assigned yet.

## Do not conflate the five resource layers

Existing assets pass through several independently owned layers:

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

Loading or locating one layer does not create the next one:

- a descriptor is not resource bytes;
- cached resource bytes are not a `MovieDef`;
- a `MovieDef` is not a registered, advanced, and rendered player;
- a player is not a native menu page or focus owner; and
- a named sprite placement is not a native row, callback, or input target.

This is the central constraint on reuse. ERNativeUI can safely reuse an asset
today when it enters through a higher native owner that already supplies the
remaining layers, or when it performs one proven operation on an existing
live object.

## Naming and path conventions

### Logical movie names

High-signal UI resources use an extensionless logical stem such as:

```text
02_040_OptionSetting
02_042_PC_GraphicSetting
02_990_TextInput
```

The observed convention is a two-digit family, a three-digit member, and a
descriptive suffix. That is a naming pattern, not a recovered schema: do not
infer ownership, layer, or constructor signature from the numbers alone.

Native descriptors preserve capitalization. Loose PC files conventionally
appear under a lowercase extracted path, for example:

```text
logical stem:  02_040_OptionSetting
loose file:    menu/win/02_040_optionsetting.gfx
```

Keep these concepts separate. Native code should preserve the descriptor it
observed; a Mod Engine deployment should mirror the path and spelling used by
the user's extracted PC asset. Do not derive one form by blindly lowercasing
arbitrary future names.

### Virtual resource paths

The common GFX resource acquisition routine at `0xD7D540` owns the confirmed
Windows-first fallback:

| Format literal | String RVA | Reference RVA | Use |
|---|---:|---:|---|
| `menu:/Win/%s.gfx` | `0x2BC0310` | `0xD7D586` | first GFX lookup |
| `menu:/%s.gfx` | `0x2BC0338` | `0xD7D6FF` | non-Windows fallback after a miss |

The `%s` comes from descriptor offset `+0x08`. The function returns resource
data/capability ownership; it does not instantiate a movie.

Nearby functions construct `.sblytbnd`, extension-parameterized, and menu
texture-package paths. Those routes matter because a GFX can import fonts and
external images, but their full repository handle and teardown contracts are
not yet mapped. See
[`GHIDRA_EXPORT_FINDINGS.md`](GHIDRA_EXPORT_FINDINGS.md#menu-resource-path-construction-clusters).

### In-movie object paths

Paths such as these address objects inside a live movie rather than files in
the resource repository:

```text
WindowList/ControllSetting/Item_0_0/Widgets/Button
Widgets/TextInput/Text_0
MenuTitle/Text_0
```

The validated resolver at `0x74B140` splits `/` and performs component-wise
member lookup. Instance names are case-sensitive binding data; Elden Ring's
spelling `ControllSetting` must not be corrected in code.

An executable path can still differ from the final visible field selected by
a later native binding. For example, `MenuTitle/Text_0` was observed at a
native constructor boundary, while the visible outer title in the inspected
movie is `MenuTitle/StaticText_101003`. Confirm the live target rather than
assuming similar names are aliases.

## Resident, on-demand, definition, and player caches

The distinction is proven at the system level:

| RVA / field | Confirmed role |
|---|---|
| system `+0x978` | resident GFX resource collection |
| `0xD78A60` | preload the 113-entry built-in descriptor catalog and retain resources in resident storage |
| system `+0x990/+0x998` | on-demand GFX resource cache |
| `0xD79140` | look up an on-demand resource; resolve and insert it on a miss |
| system `+0x9A8` | `CSScaleformMovieDef` cache/tree |
| `0xD7C370` | obtain resource data and acquire/cache a movie definition |
| `0xD78E10` | open a built-in catalog movie by bounded numeric ID and register a player |
| `0xD7A370` | open/register a player from a caller-supplied 16-byte descriptor |

The built-in preloader also handles region/language-derived resource names.
That is another reason not to turn a logical stem directly into a loose disk
path inside the runtime host.

The catalog below does **not** label an individual movie resident or
on-demand unless its route has been followed. A reference in
`string_references.csv`, including a reference in registration material, is
insufficient to make that classification. A bounded runtime observation at
`0xD78A60`, `0xD79140`, and `0xD7C370` is the appropriate test.

## Curated high-signal movie stems

This is intentionally not a dump of every UI string. It retains only a small
set with direct relevance to ERNativeUI's option, dialog, and text-input work.
Function names beginning with `FUN_` are Ghidra defaults, not FromSoftware
symbols.

Each stem also has one reference inside `FUN_1400aeef0` (`0xAEEF0`), the
executable's name/type registration material. Those registration references
are listed separately because they must not be mistaken for loader callsites.

| Logical stem | String RVA | Non-registration reference clusters | Registration reference | Current evidence and reuse value |
|---|---:|---|---:|---|
| `02_040_OptionSetting` | `0x2AC11E8` | `FUN_140809660`: `0x8096CE`, `0x8096D5` | `0xAF8F4` | **Confirmed movie and live use.** Static tree, native row binding, titles, six-row fallback, optional thirteen-row patch, and the root TextInput idle-frame prototype are documented. Best current host for ordinary settings reuse. |
| `02_042_PC_GraphicSetting` | `0x2AC12D8` | `FUN_140808860`: `0x8088E9`, `0x8088F0` | `0xAF91E` | **Confirmed movie and live use.** Owns the graphics-style inner subpage used by the current title bridge. Its independently generated framed TextInput idle patch displayed the native brackets and red empty state in game on 1 September 2026; a `02_040` visual edit does not affect this host movie. The original asset remains user-extracted/local. |
| `02_044_PC_TextSelect` | `0x2B14850` | `FUN_14094FDB0`: `0x94FF46`, `0x94FF4D`; `FUN_14094EEA0`: `0x94EF31`, `0x94EF38` | `0xAF92C` | **Lead only.** Compact candidate for studying PC text/list selection; name and references do not prove a reusable constructor. |
| `02_160_KeyConfiguration` | `0x2AC1348` | `FUN_140808510`: `0x808615`, `0x80861C`; `FUN_140808770`: `0x8087EC`, `0x8087F3` | `0xAF948` | **Lead only.** Relevant to keyboard/controller presentation, but no safe page/player contract has been assigned. |
| `02_990_TextInput` | `0x2AC5A78` | `FUN_14081CFD0`: `0x81D0BB`, `0x81D0C2` | `0xAF68F` | **Confirmed active editor and live use.** The native character-name activation route selects this 400-pixel editor through `0x81D610 -> 0x81CFD0`. The prototype accepted 16 ASCII characters, rejected a seventeenth, committed, and persisted the value. See [`TEXT_INPUT.md`](TEXT_INPUT.md). |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081D160`: `0x81D24B`, `0x81D252`; `FUN_14081D2F0`: `0x81D3DB`, `0x81D3E2`; `FUN_14081D480`: `0x81D56B`, `0x81D572` | `0xAF69D` | **Confirmed active editor family and live use for the `0x81D160` route.** The vanilla generic row factory selects this narrower editor through `0x81D700`; the prototype reproduced its eight-character ASCII limit and characteristic offset. The other two wrappers remain statically mapped but not behaviorally classified. See [`TEXT_INPUT.md`](TEXT_INPUT.md). |
| `01_010_MessageBox` | `0x2AADC38` | `FUN_1407BB9B0`: `0x7BBAE2`, `0x7BBAE9`; `FUN_1407BBE90`: `0x7BC002`, `0x7BC009`; `FUN_1407B7E90`: `0x7B7F24`, `0x7B7F2B`; `FUN_1407B05B0`: `0x7B064D`, `0x7B0654` | `0xAEF40` | **High-signal dialog lead.** The name fits the working native alert family, but the current job-builder path has not yet been statically tied to this exact movie. |
| `01_011_MessageBox_Small` | `0x2AAFD98` | `FUN_1407FA960`: `0x7FAA10`, `0x7FAA17`; `FUN_1407BD6E0`: `0x7BD8F4`, `0x7BD900`; `FUN_1407BBD80`: `0x7BBE15`, `0x7BBE1C`; `FUN_1407B84E0`: `0x7B8575`, `0x7B857C` | `0xAF02A` | **High-signal variant lead.** Presentation and builder relation remain unproved. |
| `01_013_MessageBox_Small_NB` | `0x2AAFDC8` | `FUN_1407BD6E0`: `0x7BD8DA`, `0x7BD900` | `0xAF159` | **High-signal variant lead.** The suffix may indicate a no-button variant, but that expansion is a hypothesis until its natural caller/presentation is observed. |

The two dedicated text-input stems are active editor resources separate from
the shared read-only row widget in `02_040`. The native producer, controller,
software-keyboard job family, and character-name activation route are now
mapped and have been exercised by a guarded prototype. A static row placement
still cannot provide editing by itself; the confirmed ownership and activation
path is summarized in [`TEXT_INPUT.md`](TEXT_INPUT.md).

## Curated in-movie and dependency anchors

| Literal | String RVA | Reference RVA(s) | Confirmed or bounded interpretation |
|---|---:|---|---|
| `Widgets/TextInput` | `0x2AD74E8` | notably `0x86BD90` in `0x86BCB0`; other shared widget-selection references exist | `0x86BCB0` hides Slider, ComboBox, DropdownList, and Button, then selects TextInput. Other references must be classified individually. |
| `Widgets/TextInput/Text_0` | `0x2B1AF58` | `0x976F6E` | higher text-row producer resolves the displayed-value member before creating `TextInputController` |
| `Widgets/TextInput/Input` | `0x2B1AF78` | `0x976FBE` | native code resolves this name, but no matching static child exists in the inspected `02_040`; likely runtime-created or variant-dependent, still unresolved |
| `TextInput/Text_0` | `0x2B2B970` | `0x9B9E6D` | standalone `TextInputDialog` construction anchor |
| `MenuTitle/Text_0` | `0x2A96C20` | fourteen references, including `0x742931`, `0x7D4160`, and `0x96AD71` | common native title-source path; broad reference count means each caller must be classified before hooking |
| `font.swf` | `0x2BC02D0` | `0xD783C5` | game font/translation loader-state path; also imported by inspected `02_040` |
| `gfxfontlib.swf` | `0x2CC3B28`, `0x2CC41D0` | `0x116920E`, `0x116FF89`, `0x116F109` | generic linked Scaleform font-library internals, not an Elden Ring movie-open wrapper |

The inspected `02_040` also declares 84 external `.tga` images through
`DefineExternalImage2`. Their names and bytes are intentionally not cataloged
here. Reusing a sprite that depends on them requires the game's image/resource
resolver; copying only the sprite tags is incomplete. The root TextInput
idle-frame prototype further confirms that a new movie-local
`DefineExternalImage2`, wrapper sprite, and scaling-grid definition render
through a direct `PlaceObject3` image placement without a new `DoABC` class or
`SymbolClass` entry, provided those definitions precede the TextInput sprite
that references the wrapper. This is a confirmed boundary for that direct
display-list route, not a general statement about ActionScript class-name
construction. See [`TEXT_INPUT.md`](TEXT_INPUT.md).

## What ERNativeUI can reuse today

### 1. Native constructors that already select game presentation

This remains the preferred production path. Existing native constructors for
toggle, slider, inline choice, popup choice, and action rows populate the
already-live Game Options movie and retain the native state/callback objects.
They inherit focus, controller/mouse behavior, sound, and teardown.

The GFX is reused indirectly, through its real owner. No client-facing asset
name or Scaleform pointer needs to become ABI.

### 2. Existing named objects in a live movie

The current narrow bridge can:

- resolve a slash-separated path through `0x74B140`;
- retain the complete `0x60`-byte `SceneObjProxy` result for its validated
  lifetime;
- set UTF-16 text through `0x74AE50`; and
- destroy the embedded `CSScaleformValue` through `0xD81590`.

This is suitable for known fields such as page titles. It does not imply a
general property setter, `Invoke`, `AttachMovie`, or persistent client-owned
`GFx::Value` API. See
[`SCALEFORM_NATIVE_BRIDGE.md`](SCALEFORM_NATIVE_BRIDGE.md).

### 3. Offline extension of an inspected movie

The `02_040` patch proves that a mod can clone a compatible placement and add
a missing native binding field while preserving movie structure. This works
because the patcher validates the exact donor, target, depths, IDs, tag
lengths, and idempotence.

Offline reuse is local to that movie's character dictionary. Character IDs
are not portable between movies, and a copied definition may depend on
`font.swf`, external images, ActionScript classes, or other definitions. See
the [GFX patcher guide](../GFX_PATCHER.md).

### 4. Opening a complete existing movie

The system now has plausible high-level observation anchors:

- `0xD78E10` for a built-in numeric catalog ID; and
- `0xD7A370` for a caller-supplied native descriptor.

They are safer research targets than calling the raw linked Scaleform loader,
because they acquire a definition and create/register a player. They are not
yet production interfaces. Before exposing either one, ERNativeUI still needs
the descriptor invariants, target layer/z-order, viewport policy, input/focus
owner, cache replacement rules, failure behavior, thread, and deterministic
close/release sequence.

### 5. Copying a sprite across movies

This is currently unsupported. A sprite definition alone does not carry all
of its transitive character definitions, exported class bindings, imported
font symbols, external images, or native instance-name contract. Prefer
opening the complete existing movie or reusing a native constructor until a
dependency-aware linker is deliberately designed and validated.

## Reproducible discovery workflow

### Step 1: query the existing export narrowly

The generated CSV is ignored and stays local. Query exact names instead of
printing every game string:

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

For discovery rather than confirmation, use a narrowly bounded pattern such
as `^02_[0-9]{3}_.*Text.*$`, inspect the small result, then switch back to
exact names. Do not commit the unfiltered CSV or its full string inventory.

### Step 2: map each reference to its containing function

Use `research-work/exports/functions.csv` or the bounded read-only `Anchors`
query documented in [`tools/research/README.md`](../../tools/research/README.md).
Keep these categories separate:

- registration/name material;
- a resource descriptor producer;
- resident preload;
- on-demand acquisition;
- MovieDef acquisition;
- player creation/registration; and
- page/controller code that merely selects an already-live object.

A pair of adjacent string references often reflects a constructor/copy path,
but it is not a function signature.

### Step 3: follow toward a known ownership boundary

Prefer a bounded call graph from the referencing function toward:

| Boundary | Question answered |
|---:|---|
| `0xD7D540` | does this path acquire GFX resource bytes? |
| `0xD78A60` | is it reached through resident preload? |
| `0xD79140` | is it acquired through the on-demand cache? |
| `0xD7C370` | does it become or reuse a cached MovieDef? |
| `0xD7C900` | does it create/register a `CSScaleformSwfPlayer`? |
| `0xD7ADE0` / `0xD73850` | is the player in the normal per-frame advance collection? |

Stop when the graph reaches a generic allocator/string utility or the linked
GFx implementation without a typed game owner. A low-level runtime function
is not automatically a better hook.

### Step 4: inspect a lawfully extracted candidate movie

Keep original files outside the repository. Record the game build and hash,
then export structural data with FFDec:

```powershell
ffdec.bat -onerror abort -swf2xml `
    'C:\path\to\candidate.gfx' `
    'research-work\gfx\candidate.xml'

ffdec.bat -onerror abort -export script `
    'research-work\gfx\candidate-scripts' `
    'C:\path\to\candidate.gfx'
```

Inspect at least:

- SWF/GFX version, ActionScript version, stage, frames, and labels;
- `ImportAssets2`, `DefineExternalImage2`, and exported symbol dependencies;
- `SymbolClass` and ActionScript listeners/commands;
- named placements and exact capitalization;
- shared character definitions versus per-instance placements; and
- font, texture, and nested-movie dependencies.

`ERNativeUIGfxPatcher.exe --inspect` is a structural check specifically for
compatible `02_040_optionsetting.gfx` files. It is not a generic catalog
parser and should reject unrelated candidates.

### Step 5: observe one natural open/close cycle

Before calling anything, observe a vanilla action that naturally opens the
candidate. A bounded probe should record only:

- logical descriptor/ID and callsite;
- resident/on-demand/definition/player boundary reached;
- thread ID;
- returned owner/player pointer and vtable;
- insertion into and removal from the active-player collection; and
- retain/release transitions.

Cap by count and time, avoid logging localized user text, and do not retain an
extra game reference. Only after the natural lifecycle is complete should a
separate controlled test reproduce the same high-level call.

## Promotion checklist for an existing movie

An asset should enter a production reuse table only when all applicable boxes
are satisfied:

1. Exact executable and asset hashes are recorded.
2. Logical stem, platform/fallback path, and descriptor layout agree.
3. Resident or on-demand behavior is observed rather than guessed.
4. MovieDef and player ownership are followed through final release.
5. Update/render registration and viewport/layer behavior are confirmed.
6. Controller, mouse, keyboard, Back, and modal focus behavior are tested.
7. Font, image, locale, and region dependencies resolve through game owners.
8. Failure disables only the optional feature and leaves native UI intact.
9. Address discovery uses a validated relation/signature, not a bare RVA.
10. No client DLL receives a raw game/Scaleform pointer whose lifetime it can
    outlive.

## Legal and artifact boundary

- ELDEN RING's executable, original GFX/SWF files, fonts, textures, and other
  game data remain the property of their respective owners. ERNativeUI's MIT
  license does not relicense those assets.
- Keep the executable, original extracted movies, FFDec XML/script/image
  exports, full string indexes, and bulk disassembly under ignored
  `research-work/` or another local directory.
- Publish derived facts, small analytical diagrams, hashes, RVAs, signatures,
  original tooling, and narrowly written interoperability documentation rather
  than proprietary asset dumps.
- The repository's optional modified `02_040_optionsetting.gfx` and
  `02_042_pc_graphicsetting.gfx` are an explicit existing distribution
  decision documented in
  [`THIRD_PARTY_NOTICES.txt`](../../THIRD_PARTY_NOTICES.txt). It does not grant
  permission to add other extracted GFX, `font.swf`, TGA images, or complete
  asset catalogs.
- Ask users to extract originals from their lawfully owned game when a tool
  needs source asset bytes. Prefer a reproducible patch/transform over
  redistributing another original movie.

## Current safe conclusion

ERNativeUI can already reuse Elden Ring presentation very effectively by
working through native UI constructors and by modifying validated objects in
an already-live Game Options movie. The executable also exposes a coherent
resource, MovieDef, and player lifecycle that makes future whole-movie reuse
feasible.

What is missing is not the asset name. It is the final ownership contract:
which descriptor opens it, where its player is registered, who owns focus and
input, and how it is removed without leaving live Scaleform values. Until that
contract is reproduced end to end, the curated names above remain navigation
anchors rather than a public `open_gfx(name)` API.
