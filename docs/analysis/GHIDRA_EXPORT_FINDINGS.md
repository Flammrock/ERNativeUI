# Curated findings from the completed Ghidra exports

This is a bounded review of the existing read-only exports
`research-work/exports/functions.csv` and `string_references.csv`. It does not
repeat the established SceneObjProxy, TextInput controller/dialog, movie
lifecycle, FSCommand, or event-queue findings documented elsewhere. No Ghidra
project or executable was opened for this pass.

The function names `FUN_...` below are Ghidra defaults. Quoted names are
literal strings embedded in the executable; they are not recovered C++
function names. Behavioral descriptions beyond the literal relationship are
marked as leads rather than facts.

## Separate native TextInput movie family

The exports identify two GFX-style resource names that are distinct from the
`02_040_optionsetting.gfx` widget already analyzed:

| Literal | String RVA | Referencing function | Reference RVAs |
|---|---:|---:|---:|
| `02_990_TextInput` | `0x2AC5A78` | `FUN_14081cfd0` (`0x81CFD0`, size `0x18D`) | `0x81D0BB`, `0x81D0C2` |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081d160` (`0x81D160`, size `0x18D`) | `0x81D24B`, `0x81D252` |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081d2f0` (`0x81D2F0`, size `0x18D`) | `0x81D3DB`, `0x81D3E2` |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081d480` (`0x81D480`, size `0x18D`) | `0x81D56B`, `0x81D572` |

Both names are also referenced by `FUN_1400aeef0` at `0xAF68F` and
`0xAF69D`. That low-RVA function belongs to the executable's name/type
registration material; the string reference alone does not make it a loader.

**Concrete new lead:** the three equal-sized `02_991` functions and the
parallel `02_990` function are a compact comparison set for discovering how
dedicated text-entry movies are requested or configured. They should be
examined before attempting to make the read-only TextInput placement in
`02_040` editable. The exports do not prove whether these functions load,
construct, register, or merely select those movies.

## Menu resource-path construction clusters

The completed export groups several related format strings into two functions
that were not called out in the earlier movie-lifecycle inventory:

| Function | Referenced formats | Concrete conclusion |
|---:|---|---|
| `FUN_140d78f10` (`0xD78F10`, size `0x224`) | `menu:/%s%s%s.sblytbnd`, `menu:/%s%s.sblytbnd`, `menu:/%s.sblytbnd` at reference RVAs `0xD78FFB`, `0xD79025`, `0xD79073`, `0xD79088` | One function constructs several fallback shapes for menu Scaleform-layout bundle paths. Whether it opens the result itself is unproved. |
| `FUN_140d79290` (`0xD79290`, size `0x2DE`) | `menu:/%s%s%s.%s`, `menu:/%s%s.%s`, `menu:/%s.%s` at `0xD7939A`, `0xD793DC`, `0xD79448`, `0xD79476`; also `tpfbhd` | One function constructs extension-parameterized menu resource paths, including texture-package-header candidates. Load/lookup ownership is still unproved. |
| `FUN_140d7d850` (`0xD7D850`, size `0x279`) | `menutpfbnd:/00_Solo/%s.tpf`, `menutpfbnd:/71_MapTile/%s.tpf`, and `menu:/%s%s.tpf`; category strings `MENU_Knowledge`, `MENU_Load`, `MENU_Tuto`, `MENU_MapTile`, `MENU_StageImage` | High-signal texture-path/category router for menu assets. It is a resource lead, not evidence that arbitrary client textures can be registered. |

These functions complement the already documented `0xD7D540` GFX path
builder (`menu:/Win/%s.gfx` then `menu:/%s.gfx`). Together they give a focused
resource-resolution neighborhood for a future custom-movie investigation.
They do not establish the repository object, returned handle, asynchronous
state, or destruction contract needed for production use.

## Generic Scaleform runtime surfaces: useful boundaries, poor game hooks

Several high-signal strings land in the statically linked generic Scaleform
runtime rather than an Elden Ring UI wrapper:

| Runtime surface | Literal/reference evidence | Curated interpretation |
|---|---|---|
| Focus-manager API table | `FUN_140f4afd0` (`0xF4AFD0`, size `0x706`) references `captureFocus` (`0xF4B01F`), `modalClip` (`0xF4B1B6`), `moveFocus` (`0xF4B1E5`), `setModalClip` (`0xF4B2EB`), `setControllerFocusGroup` (`0xF4B3F1`), and related query names through `0xF4B623`. | Confirms that the linked GFx runtime contains its multi-controller focus API. The clustered strings are consistent with method registration/dispatch, not proof that Game Options calls these methods. |
| EventDispatcher surface | `FUN_140ff9160` (`0xFF9160`, size `0x54D`) references `dispatchEvent` at `0xFF9194`, plus `clone` and `event`. | Concrete AVM/EventDispatcher implementation lead. No export edge connects it to Elden Ring's native page event queue. |
| Timeline methods | `FUN_140f05530` references `AvmSprite::SpriteGotoAndStop needs one arg` at `0xF05584`; `FUN_140f05690` references the corresponding GotoAndPlay diagnostic at `0xF056E4`. | Confirms native implementations of the generic ActionScript timeline methods. It does not identify a game wrapper or safe direct signature. |

These are valuable landmarks if a future call graph reaches them from a game
wrapper. They are unsafe primary hooks today: they are shared runtime
internals, potentially serve every movie, and have no recovered Elden Ring
owner or lifetime boundary.

## Provenance-only names deliberately not promoted

Strings such as `GfxRepository`, `GfxRepositoryImp`, `ScaleformTexRepository`,
`CSScaleform`, `CSScaleformImp`, `CSScaleformStep`, `CSScaleformSwfPlayer`,
`CSScaleformSystem`, and `CSScaleformValue` reference small functions around
`0xAD5B0..0xAFDA0`. Their repeated paired references are registration/RTTI
evidence already represented by the validated class inventory. They do not
name those small functions as constructors, loaders, or accessors.

Likewise, `FSCommand:` references inside `FUN_140ee6eb0` belong to the generic
runtime. They do not overturn the confirmed finding that Elden Ring's concrete
`CSScaleformFsCommandHandler` override at `0xD6D790` returns immediately.

## Recommended next bounded queries

1. Compare callers and direct callees of `0x81CFD0`, `0x81D160`, `0x81D2F0`,
   and `0x81D480`; identify the object and differing argument that select the
   two dedicated TextInput resources.
2. Trace the returned/consumed value of path builders `0xD78F10`, `0xD79290`,
   and `0xD7D850` into the already mapped loader/repository neighborhood.
3. Search for game-side callers that cross from `CSScaleformSwfPlayer` or the
   dedicated text dialog into `0xF4AFD0`, `0xFF9160`, `0xF05530`, or
   `0xF05690`. Absence of that edge should remain a negative result rather
   than motivation to hook the generic runtime globally.

