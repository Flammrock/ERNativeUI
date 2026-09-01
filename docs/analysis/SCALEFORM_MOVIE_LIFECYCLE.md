# Scaleform movie loading and lifecycle

Status: static reconstruction for one executable build; suitable as a research
map, not as a stable ABI

This note follows Elden Ring's Scaleform path from startup, through GFX resource
resolution and `GFx::MovieDef` creation, to a registered movie instance and its
per-frame advance. It also records the matching destruction path and a useful
negative result about `FSCommand`.

The main result is that a custom movie is not created by one self-contained
"load GFX" call. The game owns several layers:

```text
CSScaleformImp
`-- CSScaleformSystem
    |-- CSScaleformLoader (derived from GFx::Loader)
    |   `-- loader states: file opener, image creator, log, translator, ...
    |-- GFX resource and MovieDef caches
    |   `-- CSScaleformMovieDef
    |       `-- raw GFx::MovieDef
    |-- active CSScaleformSwfPlayer collection
    |   `-- raw GFx::Movie instance
    `-- render bridge and CSScaleformThreadCommandQueue
```

A safe future custom-interface implementation therefore needs the game's
resource naming, movie-definition cache, player registration, update phase,
render bridge, input route, and teardown order. Calling the lowest loader
routine alone would create only part of that lifecycle.

## Build and evidence rules

All RVAs in this document refer to this exact executable:

- product version: `2.7.0.0`;
- SHA-256:
  `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`;
- PE timestamp: `0x69E9C9B9`;
- image size: `0x5E09600`.

The terms below are used deliberately:

- **Confirmed** means the relationship is encoded directly in the executable:
  a validated RTTI/vtable relationship, an exact call, an exact field access,
  a literal reference, or raw bytes at an exact target.
- **Strongly inferred** means the native call shape and surrounding ownership
  match a documented Scaleform operation, but the statically linked function
  has no exported symbol that proves its original name.
- **Unresolved** means that an address is a useful navigation anchor but is not
  safe to call or hook with a guessed signature.

The analysis used PE sections and exception-function boundaries from `.pdata`,
validated MSVC RTTI/vtables, bounded disassembly, direct-call scans, and
RIP-relative literal references. It did not modify the active Ghidra project or
the executable. Reproducible generated address inventories remain under the
ignored `research-work/movie-lifecycle/` directory.

## Startup state machine

**Confirmed:** `CS::CSScaleformStep` registers 19 named state functions in a
table rooted at RVA `0x3D872E0`. The registration code is in
`0xAE9E0..0xAEC11`.

| Index | Function RVA | Name encoded in the executable |
|---:|---:|---|
| 0 | `0xD6FD60` | `STEP_Init` |
| 1 | `0xD6FBB0` | `STEP_Begin` |
| 2 | `0xD6FDD0` | `STEP_InitUnicodeLoad` |
| 3 | `0xD707C0` | `STEP_WaitUnicodeLoad` |
| 4 | `0xD6FE70` | `STEP_Init_forGfxFileLoad` |
| 5 | `0xD70950` | `STEP_Wait_forGfxFileLoad` |
| 6 | `0xD6FF00` | `STEP_Init_forResidentTextureLoad` |
| 7 | `0xD70AC0` | `STEP_Wait_forResidentTextureLoad` |
| 8 | `0xD6FD00` | `STEP_FontSetup` |
| 9 | `0xD70370` | `STEP_RegistMainFont` |
| 10 | `0xD703D0` | `STEP_RegistSubFont` |
| 11 | `0xD6FED0` | `STEP_Init_forResidentResourceLoad` |
| 12 | `0xD709B0` | `STEP_Wait_forResidentResourceLoad` |
| 13 | `0xD6FB30` | `SETP_Init_forMapTileMaskLoad` |
| 14 | `0xD708A0` | `STEP_Wait_ForMapTileMaskLoad` |
| 15 | `0xD70430` | `STEP_UpdateB` |
| 16 | `0xD70060` | `STEP_Init_forResidentTextureReload` |
| 17 | `0xD70D10` | `STEP_Wait_forResidentTextureReload` |
| 18 | `0xD6FC60` | `STEP_Finish` |

`SETP_Init_forMapTileMaskLoad` is the spelling present in the binary. The index
table establishes the state identities; it does not prove that every run
executes them once in simple numeric order.

Useful confirmed transitions include:

- `STEP_Init` lazily creates the startup Scaleform implementation stored at
  global RVA `0x3D871B8` and marks the step initialized;
- `STEP_Init_forGfxFileLoad` starts GFX loading through the implementation;
- `STEP_Wait_forGfxFileLoad` polls the matching completion condition;
- `STEP_FontSetup` enters the font setup path;
- `STEP_Finish` releases the startup resources and sets the current state to
  `-1`.

The `CSScaleformStep` vtable is at `0x2BBD870`. Its deleting destructor is
slot 1 (`0xD6F270`) and deletes a `0xC0`-byte object. Most other slots are task
framework methods or adjusted thunks; semantic names have not been assigned to
them.

There are references to both globals `0x3D871B8` and `0x3D871C0`. The first is
used throughout the startup step and menu code. The second is lazily initialized
by `0xD6D1A0`. Their precise engine-level ownership distinction is still
unresolved, so they must not be treated as interchangeable singleton pointers.

## Scaleform system and loader construction

### Top-level owners

**Confirmed:** `CSScaleformImp` has vtable RVA `0x2BBC418`. Its constructor at
`0xD6C230..0xD6C2C0` allocates a `0x1E30`-byte `CSScaleformSystem`, calls the
system constructor at `0xD76140`, and stores the resulting pointer at
`CSScaleformImp + 0x08`.

The constructor then reaches RVA `0xD78020`. That target is protected/opaque in
the static image, so this analysis does not assign it a source-level name. The
matching implementation destructor reaches another opaque target at
`0xD77F20`.

The following `CSScaleformSystem` fields are confirmed by constructor,
initialization, use, and destruction accesses:

| Offset | Owned or referenced object |
|---:|---|
| `+0x950` | owned allocation paired with the exported `Scaleform::System::Destroy` during teardown; exact class unresolved |
| `+0x958` | `CSScaleformLog*` |
| `+0x960` | `CSScaleformThreadCommandQueue*` |
| `+0x968` | renderer/Scaleform bridge object; exact class unresolved |
| `+0x970` | `CSScaleformLoader*` |
| `+0x978` | resident GFX resource collection |
| `+0x990` | on-demand GFX resource cache |
| `+0x9A8` | movie-definition/cache tree |
| `+0x9C0` | player/name cache structure |
| `+0x9D8` | beginning of a per-frame player-pointer collection |
| `+0xBE0` | element count used with the collection at `+0x9D8` |
| `+0xBE8` | fixed-capacity secondary player-pointer vector used by one creation route |
| `+0xCF0` | element count for the vector at `+0xBE8`; insertion enforces a maximum of `0x20` |
| `+0xD00` | another active/known-player collection |
| `+0xD10..+0xD58` | nine resident resource handles |
| `+0xDA0` | lock used while traversing the per-frame collection |
| `+0xDD0` | render job/handle |

Several fields are partially overlapping members of larger native containers;
the table records observed access points, not complete C++ type declarations.

### Runtime service initialization

**Confirmed:** `0xD79EF0..0xD7A280` initializes the runtime services:

1. It allocates a `0x10`-byte command queue, invokes the Scaleform queue
   constructor at `0x11639F0`, installs the
   `CSScaleformThreadCommandQueue` vtable `0x2BC0148`, and stores it at
   system offset `+0x960`.
2. It creates a larger renderer/Scaleform bridge with the command queue and
   stores it at `+0x968`.
3. It calls `0xD72820` and stores the resulting `CSScaleformLoader*` at
   `+0x970`.

`CSScaleformLoader` is a `0x28`-byte object derived from `GFx::Loader`. Its
constructor is `0xD71860`, its vtable is `0x2BBE448`, and offset `+0x20` stores
the owning `CSScaleformSystem*`. Vtable slot 1 (`0xD723C0`) is its deleting
destructor and calls the exported `Scaleform::GFx::Loader::~Loader` at
`0x112CE00`.

### Loader state installation

**Confirmed:** `0xD72820..0xD72BB0` constructs the loader and installs GFx
states through loader vtable slot 2 (`+0x10`, function RVA `0xD73810`). That
wrapper forwards a numeric state type and a state object to the underlying
loader/state bag.

| State type | Installed object or evidence |
|---:|---|
| `2` | the system's `CSScaleformLog` state |
| `3` | object created through `0xD717E0`; exact class unresolved |
| `5` | `CSScaleformFsCommandHandler` |
| `0xC` | `CSScaleformFileOpener` |
| `0xE` | `CSScaleformImageCreator` |
| `0xF` | object created through `0x11637C0`; exact class unresolved |
| `0x28` | additional GFx state; exact class unresolved |
| `0x29` | additional GFx state; exact class unresolved |

The class-specific names are proven by RTTI and vtables. Numeric names for the
unresolved GFx state enum values are intentionally not guessed.

The separate routine `0xD783B0..0xD785D9` references `font.swf` and queries or
sets loader states with type values `0x14`, `0x17`, and `1`. It can install a
`CSScaleformArabicTranslator`. This is a font/translation configuration path,
but its direct caller and exact execution phase remain unresolved.

## GFX resource resolution

### Windows path and fallback

**Confirmed:** `0xD7D540..0xD7D84E` is the common GFX resource acquisition
routine. It contains the only direct code references in this cluster to:

- `menu:/Win/%s.gfx` at RVA `0x2BC0310`, referenced at `0xD7D586`;
- `menu:/%s.gfx` at RVA `0x2BC0338`, referenced at `0xD7D6FF`.

The routine receives an output object in `RCX` and a 16-byte movie/resource
descriptor in `RDX`; the string used for `%s` is obtained from descriptor
offset `+0x08`. It formats and queries the `menu:/Win/` path first. If that
lookup does not produce a resource, it formats and queries the path without
`Win/` as a fallback. It copies the returned resource bytes/ownership into the
output and propagates a status byte from resource offset `+0x28` to output
offset `+0x28`.

This function resolves game resource data. It does not itself instantiate a
Scaleform movie.

### Resident and on-demand caches

Three direct callers establish the layers above the resolver:

| Caller RVA | Resolver call RVA | Confirmed role |
|---:|---:|---|
| `0xD78A60` | `0xD78AE1` | preload the built-in GFX descriptor catalog |
| `0xD79140` | `0xD791A8` | acquire and cache a GFX resource on demand |
| `0xD7C370` | `0xD7C56A` | acquire resource data for a movie definition |

`0xD78A60..0xD78E0F` iterates `0x71` (113) 16-byte descriptors beginning at
RVA `0x3B41370`. It resolves each one, passes it through the native resource
repository, and retains the resulting objects in the system collection at
`+0x978`. It also has additional region/language-derived resource names.

`0xD79140..0xD7928A` checks the cache rooted at system offsets
`+0x990/+0x998`; on a miss, it resolves the GFX resource and inserts the
returned resource capability into that cache.

These counts and addresses are useful for navigation. The built-in descriptor
names are game data and are not reproduced in this repository.

## Movie definition loading

**Confirmed:** `0xD7C370..0xD7C8F1` is the movie-definition acquisition path.
It first searches the system cache rooted at `+0x9A8`. An existing entry owns a
retained `CSScaleformMovieDef*` at entry offset `+0x250`.

On a cache miss, the function:

1. resolves the GFX resource through `0xD7D540`;
2. passes the loader at system offset `+0x970`, an output intrusive pointer,
   and the resolved wide path to `0xD72CC0`;
3. returns the resulting `CSScaleformMovieDef` owner and conditionally inserts
   it into the cache.

The insertion condition is now confirmed rather than implicit. The function
lazily reads the boolean setting named `Scaleform.SwfCache`; a newly loaded
definition is stored at cache-entry `+0x250` only when that setting is enabled
and descriptor byte `+0x04` is nonzero. Existing entries are still looked up
before this gate. Consequently, `+0x9A8` is a definition cache, but a miss is
not guaranteed to populate it.

`0xD72CC0..0xD72E48` builds Scaleform load arguments, calls `0x112CF30`, and
wraps the returned raw definition through `0xD72BB0`. `0x112CF30` validates the
converted path and the loader's internal pointer at loader offset `+0x08`.
When both are present it calls `0x1165A60` with the per-call flags ORed with
the loader flags at `+0x18`; `0xD72CC0` supplies zero per-call flags in this
path.

**Strongly inferred:** the `0x112CF30 -> 0x1165A60` boundary is the statically
linked equivalent of `GFx::Loader::CreateMovie`/movie-definition loading. The
loader, path, flags, return ownership, and next-stage use all match that public
operation, but neither function is a named export in this build.

### `CSScaleformMovieDef` layout and destruction

**Confirmed:** `0xD72BB0..0xD72C2F` allocates and initializes the
`0x20`-byte game wrapper:

| Offset | Field |
|---:|---|
| `+0x00` | vtable `0x2BBE430` |
| `+0x08` | intrusive reference count |
| `+0x10` | raw `GFx::MovieDef*` |
| `+0x18` | second owner/context pointer; in this path, the Scaleform system |

Vtable slot 1 (`0xD72410`) is the deleting destructor. It releases the raw
definition at `+0x10` through `0x113D990`, then optionally frees the
`0x20`-byte wrapper. Vtable slot 0 (`0xD733A0`) participates in the engine's
release/ownership machinery, but a narrower source-level name has not been
proven.

## Movie instance and `CSScaleformSwfPlayer`

### Raw movie creation

**Confirmed call shape:** `0xD72C30..0xD72CB8` invokes virtual offset `+0xC0`
on the raw `GFx::MovieDef*`. It constructs an instance-creation parameter block
and includes the `CSScaleformThreadCommandQueue*` obtained from the owning
system at `+0x960`.

**Strongly inferred:** this virtual call is the Scaleform
`MovieDef::CreateInstance` boundary. It returns the object subsequently stored
and used as a raw `GFx::Movie*`; the command-queue argument and ownership flow
also match movie instantiation. The exact meaning of every flag in the
game-specific wrapper remains unresolved.

### Player constructor and object layout

**Confirmed:** `CSScaleformSwfPlayer` has vtable `0x2BBE480` and size `0xE8`.
The constructor at `0xD719C0..0xD71C1C` calls `0xD72C30` and stores the raw
movie instance at offset `+0x18`.

| Offset | Confirmed field or role |
|---:|---|
| `+0x00` | `CSScaleformSwfPlayer` vtable |
| `+0x08` | owning `CSScaleformSystem*` |
| `+0x10` | retained `CSScaleformMovieDef*` |
| `+0x18` | raw `GFx::Movie*` instance |
| `+0x20` | copied 16-byte movie descriptor/name |
| `+0x30` | owned render/display-related handle |
| `+0x38` | synchronization object |
| `+0x68` | one-byte initialization/advance state |
| `+0x70` | optional retained callback/interface object |

The constructor configures the raw movie through several virtual offsets:
`+0x158`, `+0x100`, `+0xD0`, `+0x60`, `+0x70`, and `+0xA8`. Their arguments
show initialization, display-handle, viewport, and policy-like operations, but
exact method names are not assigned without a named or independently validated
boundary.

### Player registration and high-level open paths

**Confirmed:** `0xD7C900..0xD7CE73` manages creation and registration of a
player. It can find, remove, or replace an existing player for some descriptor
types, allocates `0xE8` bytes for a new player, calls `0xD719C0`, and inserts or
configures the result in the system's player structures.

Two post-construction routes are selected by a boolean argument whose
source-level meaning remains unresolved:

- one route calls `0xD74140` and appends the player to the fixed-capacity
  vector at system `+0xBE8`, counted at `+0xCF0`;
- the other performs the zero-time initial advance/capture through
  `0xD736E0` and can additionally call `0xD7BC20` depending on the second
  boolean argument.

Descriptor byte `+0x04` also controls name-cache behavior: values at least
`3` store the player at entry `+0x250` in the tree rooted at `+0x9C0`, while
the exact value `2` first searches the list at `+0xD00` for a matching name.
These are confirmed mechanics; the descriptor enum and the two booleans do
not yet have safe semantic names.

Two useful higher-level entry points call both movie-definition acquisition and
player creation:

| RVA | Input route | Confirmed behavior |
|---:|---|---|
| `0xD78E10..0xD78F10` | built-in numeric ID | bounds the ID to the 113-entry catalog, gets its descriptor, loads the definition, creates/registers a player, and returns success |
| `0xD7A370..0xD7A457` | caller-supplied 16-byte descriptor | loads the definition, creates/registers a player, and returns the created player |

These are better observation points than the raw GFx call because they retain
the game's cache and player-registration behavior. Their argument flags and
descriptor invariants still need runtime validation before they become callable
ERNativeUI interfaces.

## Per-frame advance

The executable contains a much stronger update boundary than the RTTI inventory
alone suggested.

**Confirmed:** the system function `0xD7ADE0..0xD7B9C4` locks the structure at
system offset `+0xDA0`, iterates player pointers from the collection beginning
at `+0x9D8` using the count at `+0xBE0`, and calls `0xD73850` for each non-null
player. The direct call is at `0xD7AEC0`.

`0xD73850..0xD7391F` receives a `CSScaleformSwfPlayer*`, a frame/context
structure, and two integer flags. When a raw movie exists, it calls virtual
offset `+0xC0` on that movie with:

- a `float` loaded from frame/context offset `+0x08` in `XMM1`;
- integer `2` in `R8D`;
- `false` in `R9D`.

**Strongly inferred:** raw movie virtual offset `+0xC0` is
`GFx::Movie::Advance(float delta, unsigned frameCatchUpCount, bool capture)`.
The Microsoft x64 register placement, floating-point delta, fixed catch-up value
of 2, and boolean third argument match the public Scaleform method exactly.

There is an independent initialization path at `0xD736E0..0xD73711` that calls
the same raw-movie slot with delta `0.0`, catch-up count `2`, and capture set to
`true`, then sets player byte `+0x68` to 1. It is called during player creation
and from two other lifecycle paths.

When one update flag is enabled, `0xD73850` also builds a display/viewport-like
structure through `0xD73920` and passes it to raw-movie virtual offset `+0x60`
before advancing. The constructor uses the same virtual offset for a
viewport-shaped argument, so a viewport update is likely, but that exact name
remains inferred.

This establishes movie advancement and the owning system loop. It does not yet
establish the complete render submission or native input route. The remainder
of `0xD7ADE0` touches other engine objects and protected paths; assigning all of
those operations by proximity would be unsafe.

Later in the same function, the system scans the secondary vector at
`+0xBE8/+0xCF0`. When predicate `0xD736D0(player)` succeeds, it removes a
matching record from the `+0x9D8` collection through `0xD81230` under the
`+0xDA0` lock and erases that pointer from the secondary vector. This confirms
coordination between the two collections, but it does not prove whether the
predicate means ready, finished, hidden, or another lifecycle state.

## Destruction and release order

**Confirmed:** the `CSScaleformSwfPlayer` destructor is
`0xD71EC0..0xD72025`; vtable slot 1 (`0xD72480`) is the deleting destructor and
frees a `0xE8`-byte object.

The destructor:

1. detaches/cleans a movie-related wrapper obtained through `0xD73530`;
2. releases the raw movie at player offset `+0x18` through `0x112DEB0` and
   clears the field;
3. releases the retained `CSScaleformMovieDef` at `+0x10` through the engine's
   intrusive reference count and its vtable release operation at the last
   reference;
4. releases the optional object at `+0x70` through its virtual interface;
5. destroys the player's synchronization/container members.

The `CSScaleformSystem` destructor is `0xD76E20..0xD7759A`; vtable slot 1
(`0xD77AD0`) is its deleting destructor. It tears down player/cache structures
and releases the renderer bridge, loader, command queue, log, and resident
resources. Near the service teardown, it calls the named export
`Scaleform::System::Destroy` while retiring the owned field at `+0x950`.
Several release operations are routed through engine-global owner queues
rather than direct `delete` calls.

The practical consequence is strict: a complex `GFx::Value`, a player, its raw
movie, and its `MovieDef` cannot be retained independently without reproducing
the corresponding reference and thread rules. A custom API should expose
opaque handles owned by ERNativeUI, not raw Scaleform pointers.

## FSCommand is installed but the game override is a no-op

This is an important negative finding.

**Confirmed:** RTTI establishes
`CSScaleformFsCommandHandler -> GFx::FSCommandHandler -> GFx::State`. Its
constructor at `0xD6D4E0` creates a `0x18`-byte state object, and loader setup
installs it as state type 5. Its vtable is `0x2BBC6C8`:

| Slot | Target RVA | Confirmed behavior |
|---:|---:|---|
| 0 | `0xD6D630` | deleting destructor |
| 1 | `0xD6D790` | handler override |

The exact raw bytes at the slot-1 target are `C2 00 00`, which disassemble as
`ret 0`. The target has no `.pdata` entry, which is normal for a trivial leaf
with no unwind state. Thus the object is structurally a valid GFx
`FSCommandHandler`, but Elden Ring's override performs no native dispatch in
this build.

The generic Scaleform runtime string `FSCommand:` elsewhere in the executable
does not change this conclusion: it belongs to the linked GFx implementation,
whereas the game-installed override is the exact no-op above.

Consequences:

- shipped ActionScript cannot be assumed to reach an Elden Ring event
  dispatcher through this handler;
- FSCommand is not currently a discovered bridge for ERNativeUI custom events;
- replacing the loader state remains a theoretical extension point, but its
  ownership, movie coverage, thread, and coexistence behavior must be proven
  before any runtime experiment;
- `ExternalInterface` and `FunctionHandler`-backed callbacks remain separate,
  unresolved ActionScript-to-native possibilities.

## Relevant virtual slots

The engine-wrapper vtables below are confirmed RTTI-backed navigation anchors.
Only the slots whose behavior was followed are named.

| Object / vtable RVA | Slot | Target RVA | Current interpretation |
|---|---:|---:|---|
| `CSScaleformImp` / `0x2BBC418` | 0 | `0xD6D1F0` | unresolved component/lazy-owner operation |
| `CSScaleformImp` / `0x2BBC418` | 1 | `0xD6C480` | deleting destructor |
| `CSScaleformMovieDef` / `0x2BBE430` | 0 | `0xD733A0` | engine release/detach operation |
| `CSScaleformMovieDef` / `0x2BBE430` | 1 | `0xD72410` | deleting destructor |
| `CSScaleformLoader` / `0x2BBE448` | 1 | `0xD723C0` | deleting destructor |
| `CSScaleformLoader` / `0x2BBE448` | 2 | `0xD73810` | forward state type/object to underlying loader |
| `CSScaleformSwfPlayer` / `0x2BBE480` | 0 | `0xD73D70` | unresolved player operation |
| `CSScaleformSwfPlayer` / `0x2BBE480` | 1 | `0xD72480` | deleting destructor |
| `CSScaleformSystem` / `0x2BC0130` | 0 | `0xD7F590` | unresolved system operation |
| `CSScaleformSystem` / `0x2BC0130` | 1 | `0xD77AD0` | deleting destructor |
| `CSScaleformFsCommandHandler` / `0x2BBC6C8` | 0 | `0xD6D630` | deleting destructor |
| `CSScaleformFsCommandHandler` / `0x2BBC6C8` | 1 | `0xD6D790` | no-op handler (`ret 0`) |

Two different raw Scaleform interface types both use virtual offset `+0xC0` in
this reconstruction. The offset alone is not a function identity:

- on the raw `GFx::MovieDef`, `+0xC0` is strongly inferred to be
  `CreateInstance`;
- on the resulting raw `GFx::Movie`, `+0xC0` is strongly inferred to be
  `Advance`.

Any runtime probe must first validate the concrete object's vtable. Treating a
virtual offset as globally meaningful would call the wrong function.

## Build-specific navigation map

These analytical names are intentionally descriptive and are not claimed to be
FromSoftware's original symbols.

| RVA | Analytical role | Evidence level |
|---:|---|---|
| `0xD6C230` | construct `CSScaleformImp` and owned system | Confirmed |
| `0xD76140` | construct `CSScaleformSystem` | Confirmed |
| `0xD79EF0` | initialize command queue, render bridge, and loader | Confirmed |
| `0xD72820` | construct/configure `CSScaleformLoader` states | Confirmed |
| `0xD7D540` | resolve/acquire `menu:/Win/%s.gfx`, then fallback path | Confirmed |
| `0xD78A60` | preload the 113-entry built-in GFX catalog | Confirmed |
| `0xD79140` | on-demand GFX resource cache acquisition | Confirmed |
| `0xD7C370` | acquire/cache a `CSScaleformMovieDef` | Confirmed |
| `0xD72CC0` | prepare loader arguments and load a raw movie definition | Confirmed wrapper; underlying GFx name inferred |
| `0x112CF30` | enter the statically linked loader implementation | Strongly inferred `CreateMovie` boundary |
| `0xD72BB0` | allocate/wrap `CSScaleformMovieDef` | Confirmed |
| `0xD72C30` | create a raw movie instance from a raw definition | Confirmed call shape; `CreateInstance` name strongly inferred |
| `0xD719C0` | construct `CSScaleformSwfPlayer` | Confirmed |
| `0xD7C900` | create/replace/register a player | Confirmed |
| `0xD78E10` | open a built-in catalog movie by numeric ID | Confirmed |
| `0xD7A370` | open a movie from a caller-supplied descriptor | Confirmed |
| `0xD736E0` | zero-time initial movie advance/capture | Strongly inferred method name |
| `0xD73850` | advance one player for a frame | Strongly inferred raw `Movie::Advance` slot |
| `0xD7ADE0` | system player-update loop and later frame work | Confirmed iteration/call; full function unresolved |
| `0xD71EC0` | destroy `CSScaleformSwfPlayer` | Confirmed |
| `0xD76E20` | destroy `CSScaleformSystem` | Confirmed |
| `0xD6D790` | `CSScaleformFsCommandHandler` override returning immediately | Confirmed raw bytes |

Every address is build-locked. A production hook still needs an AOB signature,
instruction validation, expected callsite/target checks, and a safe failure
mode.

## Recommended extension boundaries

For future ERNativeUI work, the evidence favors this order:

1. Observe the high-level descriptor open path at `0xD7A370` and the
   create/register path at `0xD7C900`. They preserve more engine invariants
   than calling the raw loader.
2. Trace a known, naturally opened movie through `0xD7C370`, `0xD7C900`, and
   `0xD73850` to validate descriptor flags, player collection membership, and
   thread identity.
3. Treat `0xD72CC0`, `0x112CF30`, and raw vtable offsets as observation
   boundaries only until their complete arguments and ownership are proven.
4. Keep movie and value handles opaque and host-owned. Do not let client DLLs
   retain raw `GFx::Movie`, `GFx::MovieDef`, or `GFx::Value` pointers.
5. Continue using validated native scene paths for existing settings screens
   while the missing input and render-registration contracts are investigated.

Hooking the common resolver at `0xD7D540` would affect every GFX load and is too
broad for a first custom-movie experiment. Replacing the FSCommand loader state
is also not a first experiment: the current no-op is informative, but it does
not establish safe replacement lifetime or compatibility with other mods.

## Unresolved questions

The map now establishes load, definition ownership, instance creation, player
registration, per-frame advance, and destruction. The following gaps still
block a production custom-GFX API:

1. What is the complete 16-byte movie descriptor layout, including the meaning
   of its type and flag bytes?
2. Which thread and phase may create, advance, mutate, and destroy each movie?
3. Which exact operation registers a new player's render/display handle with
   the object at system offset `+0x968`?
4. Where is capture/render submission performed after `Movie::Advance`, and
   how is it synchronized with `CSScaleformThreadCommandQueue`?
5. How are mouse, keyboard, controller, and higher native menu actions routed to
   the correct movie/player?
6. Which game wrappers implement `GFx::Movie::Invoke`, variable access, and
   complex `GFx::Value` lifetime?
7. Does any shipped movie use `ExternalInterface` or a native function object,
   given that the installed FSCommand override is empty?
8. What are the eviction/removal rules for the resource, movie-definition, and
   player caches?
9. What do the protected `0xD78020` and `0xD77F20` implementation boundaries
   contribute to registration and shutdown?
10. Can a completely new descriptor be registered without replacing a
    built-in entry, and which resource repository must own its bytes?

These questions are narrower than the original "how does Scaleform work?"
problem and provide concrete next targets for the Ghidra and runtime phases.

## Related notes

- [Scaleform executable surface](SCALEFORM_EXPORTS.md) records the validated
  RTTI, exports, vtable seeds, and string anchors used to begin this analysis.
- [Scaleform SDK reference model](SCALEFORM_REFERENCE_MODEL.md) records the
  public Scaleform concepts used only to name strongly matching native call
  shapes.
- [Scaleform/GFX presentation model](SCALEFORM_GFX_MODEL.md) covers the display
  tree and existing settings-widget side of the integration.
- [Current native UI hooks and object map](CURRENT_NATIVE_UI_HOOKS.md) records
  the already validated ERNativeUI production boundaries.
