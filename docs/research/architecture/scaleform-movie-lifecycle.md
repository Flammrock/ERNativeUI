# Scaleform movie loading and lifecycle

This page follows Elden Ring's Scaleform path from startup through GFX
resource resolution, `GFx::MovieDef` creation, player registration, per-frame
advance, and destruction. It also preserves the confirmed negative result for
the game's installed `FSCommand` handler.

The central result is that a custom interface cannot be created safely by one
isolated "load GFX" call:

```text
CSScaleformImp
`-- CSScaleformSystem
    |-- CSScaleformLoader (derived from GFx::Loader)
    |   `-- file opener, image creator, log, translator, and other states
    |-- GFX-resource and MovieDef caches
    |   `-- CSScaleformMovieDef
    |       `-- raw GFx::MovieDef
    |-- active CSScaleformSwfPlayer collections
    |   `-- raw GFx::Movie
    `-- render bridge and CSScaleformThreadCommandQueue
```

A complete custom-movie owner must account for resource naming, definition
caching, player registration, update/render phases, input/focus, and teardown.

## Reference build and evidence vocabulary

All RVAs on this page refer to this exact Windows executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

- **Confirmed** means an exact RTTI/vtable relationship, call, field access,
  literal reference, or instruction sequence proves the relationship.
- **Inferred** means the native call shape and ownership match a documented
  Scaleform operation, but no symbol proves the original name.
- **Unresolved** means the address is useful for navigation but not safe to
  call through a guessed signature.
- **Rejected** records a path that the evidence rules out.

The analysis used PE sections, `.pdata` function bounds, validated MSVC RTTI,
bounded disassembly/decompilation, direct-call scans, and RIP-relative literal
references. Generated reports remain local under the ignored
`research-work/movie-lifecycle/` directory. The executable was not patched.

## Startup state machine

**Confirmed:** `CS::CSScaleformStep` registers 19 named state functions in a
table rooted at RVA `0x3D872E0`; registration occurs in
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

`SETP_Init_forMapTileMaskLoad` is the spelling stored in the binary. The table
identifies states; it does not prove that all runs execute them exactly once
or in simple numeric order.

Confirmed transitions include:

- `STEP_Init` lazily creates the implementation stored at global RVA
  `0x3D871B8`;
- `STEP_Init_forGfxFileLoad` starts GFX loading;
- `STEP_Wait_forGfxFileLoad` polls its completion;
- `STEP_FontSetup` enters font setup; and
- `STEP_Finish` releases startup resources and sets current state to `-1`.

The `CSScaleformStep` vtable is `0x2BBD870`. Slot 1 at `0xD6F270` is a
deleting destructor for a `0xC0`-byte object. Most other slots remain task
framework functions or adjusted thunks. Globals `0x3D871B8` and `0x3D871C0`
have distinct lazy-initialization paths; their engine ownership relationship
is unresolved, so they must not be treated as interchangeable singletons.

## System and loader construction

### Top-level owners

**Confirmed:** `CSScaleformImp` has vtable `0x2BBC418`. Constructor
`0xD6C230..0xD6C2C0` allocates a `0x1E30`-byte `CSScaleformSystem`, calls
system constructor `0xD76140`, and stores the result at implementation `+0x08`.

Construction later reaches protected target `0xD78020`; destruction reaches
`0xD77F20`. Their exact roles are unresolved.

Observed `CSScaleformSystem` access points are:

| Offset | Owned or referenced object |
|---:|---|
| `+0x950` | Owned allocation paired with exported `Scaleform::System::Destroy`; exact class unresolved. |
| `+0x958` | `CSScaleformLog*` |
| `+0x960` | `CSScaleformThreadCommandQueue*` |
| `+0x968` | Renderer/Scaleform bridge; exact class unresolved. |
| `+0x970` | `CSScaleformLoader*` |
| `+0x978` | Resident GFX-resource collection |
| `+0x990` | On-demand GFX-resource cache |
| `+0x9A8` | Movie-definition/cache tree |
| `+0x9C0` | Player/name cache structure |
| `+0x9D8` | Start of a per-frame player-pointer collection |
| `+0xBE0` | Count for the `+0x9D8` collection |
| `+0xBE8` | Fixed-capacity secondary player-pointer vector |
| `+0xCF0` | Count for `+0xBE8`; insertion enforces maximum `0x20` |
| `+0xD00` | Another active/known-player collection |
| `+0xD10..+0xD58` | Nine resident-resource handles |
| `+0xDA0` | Lock used while traversing the per-frame collection |
| `+0xDD0` | Render job/handle |

Some offsets belong to overlapping native containers. This is an access map,
not a complete C++ class declaration.

### Runtime services

**Confirmed:** `0xD79EF0..0xD7A280`:

1. allocates a `0x10`-byte command queue, calls constructor `0x11639F0`,
   installs vtable `0x2BC0148`, and stores it at `+0x960`;
2. creates the renderer bridge using that queue and stores it at `+0x968`; and
3. calls `0xD72820` and stores a `CSScaleformLoader*` at `+0x970`.

`CSScaleformLoader` is a `0x28`-byte `GFx::Loader` derivative. Constructor
`0xD71860` installs vtable `0x2BBE448`; `+0x20` stores its owning system.
Slot 1 at `0xD723C0` calls exported
`Scaleform::GFx::Loader::~Loader` at `0x112CE00`.

### Loader states

**Confirmed:** `0xD72820..0xD72BB0` constructs the loader and installs GFx
states through loader slot 2 at `0xD73810`.

| State type | Installed object or evidence |
|---:|---|
| `2` | System `CSScaleformLog` |
| `3` | Object created by `0xD717E0`; class unresolved |
| `5` | `CSScaleformFsCommandHandler` |
| `0xC` | `CSScaleformFileOpener` |
| `0xE` | `CSScaleformImageCreator` |
| `0xF` | Object created by `0x11637C0`; class unresolved |
| `0x28` | Additional GFx state; class unresolved |
| `0x29` | Additional GFx state; class unresolved |

Numeric SDK enum names are not guessed. A separate
`0xD783B0..0xD785D9` path references `font.swf`, accesses state types `0x14`,
`0x17`, and `1`, and can install `CSScaleformArabicTranslator`; its direct
caller and exact phase remain unresolved.

## GFX resource resolution and caches

### Platform path and fallback

**Confirmed:** common acquisition routine `0xD7D540..0xD7D84E` contains the
only direct references in this cluster to:

- `menu:/Win/%s.gfx` at `0x2BC0310`, referenced at `0xD7D586`; and
- `menu:/%s.gfx` at `0x2BC0338`, referenced at `0xD7D6FF`.

It receives an output object in `RCX` and a 16-byte resource descriptor in
`RDX`; the `%s` stem comes from descriptor `+0x08`. It tries the Windows path,
then the non-Windows fallback after a miss, copies returned resource ownership
into the output, and propagates status byte resource `+0x28` to output `+0x28`.
It resolves resource data but does not instantiate a movie.

Three callers establish the layers above it:

| Caller RVA | Resolver call | Confirmed role |
|---:|---:|---|
| `0xD78A60` | `0xD78AE1` | Preload built-in GFX descriptor catalog. |
| `0xD79140` | `0xD791A8` | Acquire and cache a GFX resource on demand. |
| `0xD7C370` | `0xD7C56A` | Acquire resource data for a movie definition. |

`0xD78A60..0xD78E0F` iterates `0x71` (113) 16-byte descriptors at RVA
`0x3B41370`, resolves them through the repository, and retains results in
system collection `+0x978`. `0xD79140..0xD7928A` searches the cache at
`+0x990/+0x998`, resolves a miss, and inserts the returned capability.

## Movie-definition acquisition

**Confirmed:** `0xD7C370..0xD7C8F1` first searches the tree at `+0x9A8`.
An existing entry owns a retained `CSScaleformMovieDef*` at entry `+0x250`.

On a miss it:

1. resolves the resource through `0xD7D540`;
2. calls `0xD72CC0` with loader `+0x970`, an output intrusive pointer, and
   the resolved wide path; and
3. returns the wrapper and conditionally inserts it into the cache.

Insertion occurs only when `Scaleform.SwfCache` is enabled and descriptor byte
`+0x04` is nonzero. Existing entries are searched before that gate.

`0xD72CC0..0xD72E48` builds loader arguments, calls `0x112CF30`, and wraps
the raw definition through `0xD72BB0`. `0x112CF30` validates path and loader
internal pointer `+0x08`, then calls `0x1165A60` with per-call flags ORed with
loader flags `+0x18`; this path supplies zero per-call flags.

**Inferred:** `0x112CF30 -> 0x1165A60` is the statically linked equivalent of
`GFx::Loader::CreateMovie`. The path, flags, returned owner, and next-stage
usage match, but neither function has a confirming export name.

### `CSScaleformMovieDef` layout

**Confirmed:** `0xD72BB0..0xD72C2F` allocates this `0x20`-byte wrapper:

| Offset | Field |
|---:|---|
| `+0x00` | Vtable `0x2BBE430` |
| `+0x08` | Intrusive reference count |
| `+0x10` | Raw `GFx::MovieDef*` |
| `+0x18` | Second owner/context; the Scaleform system on this path |

Slot 1 at `0xD72410` is its deleting destructor. It releases the raw
definition through `0x113D990` and may free the wrapper. Slot 0 at `0xD733A0`
participates in engine ownership, but its narrower semantic name is unresolved.

## Movie instance and player registration

### Raw instance creation

**Confirmed call shape:** `0xD72C30..0xD72CB8` invokes raw MovieDef virtual
offset `+0xC0` with an instance parameter block that includes the system's
command queue at `+0x960`.

**Inferred:** this is `MovieDef::CreateInstance`. It returns the object stored
and used as a raw `GFx::Movie*`; the exact game-specific flags remain unknown.

### `CSScaleformSwfPlayer` layout

**Confirmed:** the player has vtable `0x2BBE480`, size `0xE8`, and constructor
`0xD719C0..0xD71C1C`.

| Offset | Confirmed field or role |
|---:|---|
| `+0x00` | Player vtable |
| `+0x08` | Owning `CSScaleformSystem*` |
| `+0x10` | Retained `CSScaleformMovieDef*` |
| `+0x18` | Raw `GFx::Movie*` |
| `+0x20` | Copied 16-byte movie descriptor/name |
| `+0x30` | Owned render/display-related handle |
| `+0x38` | Synchronization object |
| `+0x68` | One-byte initialization/advance state |
| `+0x70` | Optional retained callback/interface |

Constructor calls at raw virtual offsets `+0x158`, `+0x100`, `+0xD0`,
`+0x60`, `+0x70`, and `+0xA8` have initialization, display, viewport, and
policy-like arguments. Exact names remain unresolved.

### Registration paths

**Confirmed:** `0xD7C900..0xD7CE73` can find, remove, or replace a player,
allocates `0xE8` bytes, calls `0xD719C0`, and registers the result.

A still-unresolved Boolean selects two post-construction routes:

- call `0xD74140` and append to the fixed vector at `+0xBE8/+0xCF0`; or
- perform a zero-time initial advance/capture through `0xD736E0`, optionally
  followed by `0xD7BC20` according to another Boolean.

Descriptor byte `+0x04` also affects name caching. Values at least `3` store
the player at cache-entry `+0x250` in tree `+0x9C0`; exact value `2` first
searches collection `+0xD00`. These mechanics are confirmed, but the enum and
Boolean meanings are not.

| RVA | Input route | Confirmed behavior |
|---:|---|---|
| `0xD78E10..0xD78F10` | Built-in numeric ID | Bounds ID to the 113-entry catalog, acquires definition, creates/registers player, returns success. |
| `0xD7A370..0xD7A457` | Caller-supplied 16-byte descriptor | Acquires definition, creates/registers player, returns the player. |

These high-level paths preserve more game invariants than the raw GFx call,
but their descriptor and flags still need bounded runtime validation before
they become callable interfaces.

## Per-frame advance

**Confirmed:** `0xD7ADE0..0xD7B9C4` locks system `+0xDA0`, walks the player
collection at `+0x9D8` using count `+0xBE0`, and calls `0xD73850` for each
non-null player. The direct call is at `0xD7AEC0`.

When the raw movie exists, `0xD73850..0xD7391F` calls movie virtual `+0xC0`
with a float delta from frame/context `+0x08`, catch-up count `2`, and
`capture=false`.

**Inferred:** that virtual is
`GFx::Movie::Advance(float, unsigned, bool)`. The Microsoft x64 register
placement, delta, fixed catch-up count, and Boolean match the public method.

Initialization helper `0xD736E0..0xD73711` calls the same movie slot with
`0.0`, `2`, and `capture=true`, then sets player byte `+0x68`. When another
update flag is enabled, `0xD73850` also builds a viewport-like structure with
`0xD73920` and submits it through movie virtual `+0x60`; the exact method name
remains inferred.

Later, the system scans secondary vector `+0xBE8/+0xCF0`. When predicate
`0xD736D0(player)` succeeds, it removes a matching record from `+0x9D8`
through `0xD81230` under the same lock and erases the secondary entry. The
predicate's semantic meaning remains unresolved.

This proves movie advancement and its owning system loop. It does not prove
complete render submission or input routing.

## Destruction order

**Confirmed:** player destructor `0xD71EC0..0xD72025`, reached by deleting
slot `0xD72480`, frees a `0xE8`-byte object and:

1. detaches a movie-related wrapper obtained through `0xD73530`;
2. releases raw movie `+0x18` through `0x112DEB0` and clears it;
3. releases retained MovieDef wrapper `+0x10` through intrusive ownership;
4. releases optional object `+0x70`; and
5. destroys synchronization/container members.

System destructor `0xD76E20..0xD7759A`, reached by slot `0xD77AD0`, retires
players, caches, renderer bridge, loader, queue, log, and resident resources.
It calls exported `Scaleform::System::Destroy` while retiring field `+0x950`.
Some destruction is deferred through engine owner queues rather than direct
`delete` calls.

The ordering is a strict lifetime constraint. A complex `GFx::Value`, player,
raw movie, and MovieDef cannot be retained independently without reproducing
their reference and thread rules. A future public API must expose host-owned
opaque handles, never raw Scaleform pointers.

## Confirmed no-op FSCommand handler

RTTI establishes
`CSScaleformFsCommandHandler -> GFx::FSCommandHandler -> GFx::State`.
Constructor `0xD6D4E0` creates a `0x18`-byte state installed as loader state
type `5`. Its vtable is `0x2BBC6C8`:

| Slot | Target RVA | Confirmed behavior |
|---:|---:|---|
| 0 | `0xD6D630` | Deleting destructor |
| 1 | `0xD6D790` | Handler override |

The exact bytes at slot 1 are `C2 00 00`, or `ret 0`. The target needs no
`.pdata` entry because it is a trivial leaf. Therefore the installed game
override performs no native dispatch in this build. The generic `FSCommand:`
string in the linked Scaleform runtime does not overturn this game-specific
result.

- **Rejected:** assuming shipped ActionScript reaches an Elden Ring event bus
  through this handler.
- Replacing loader state remains hypothetical until ownership, coverage,
  thread, teardown, and coexistence are proven.
- `ExternalInterface` and function-object callbacks are separate unresolved
  directions.

## Vtable and navigation map

| Object / vtable | Slot | Target | Interpretation |
|---|---:|---:|---|
| `CSScaleformImp` / `0x2BBC418` | 0 | `0xD6D1F0` | Unresolved owner operation |
| `CSScaleformImp` / `0x2BBC418` | 1 | `0xD6C480` | Deleting destructor |
| `CSScaleformMovieDef` / `0x2BBE430` | 0 | `0xD733A0` | Engine release/detach operation |
| `CSScaleformMovieDef` / `0x2BBE430` | 1 | `0xD72410` | Deleting destructor |
| `CSScaleformLoader` / `0x2BBE448` | 1 | `0xD723C0` | Deleting destructor |
| `CSScaleformLoader` / `0x2BBE448` | 2 | `0xD73810` | Forward state type/object to loader |
| `CSScaleformSwfPlayer` / `0x2BBE480` | 0 | `0xD73D70` | Unresolved player operation |
| `CSScaleformSwfPlayer` / `0x2BBE480` | 1 | `0xD72480` | Deleting destructor |
| `CSScaleformSystem` / `0x2BC0130` | 0 | `0xD7F590` | Unresolved system operation |
| `CSScaleformSystem` / `0x2BC0130` | 1 | `0xD77AD0` | Deleting destructor |
| `CSScaleformFsCommandHandler` / `0x2BBC6C8` | 0 | `0xD6D630` | Deleting destructor |
| `CSScaleformFsCommandHandler` / `0x2BBC6C8` | 1 | `0xD6D790` | No-op handler |

Raw virtual offset `+0xC0` appears on two different interfaces. It is inferred
as `CreateInstance` on a MovieDef and as `Advance` on a Movie. The offset alone
does not identify a function; the concrete object's vtable must be validated.

High-signal reference-build seeds:

| RVA | Analytical role | Evidence |
|---:|---|---|
| `0xD6C230` | Construct implementation and system | Confirmed |
| `0xD76140` | Construct system | Confirmed |
| `0xD79EF0` | Initialize queue, renderer bridge, loader | Confirmed |
| `0xD72820` | Construct/configure loader states | Confirmed |
| `0xD7D540` | Resolve platform GFX path and fallback | Confirmed |
| `0xD78A60` | Preload built-in GFX catalog | Confirmed |
| `0xD79140` | On-demand resource cache | Confirmed |
| `0xD7C370` | Acquire/cache MovieDef wrapper | Confirmed |
| `0xD72CC0` | Load raw definition from wide path | Confirmed wrapper |
| `0x112CF30` | Enter linked loader implementation | Inferred CreateMovie boundary |
| `0xD72BB0` | Allocate/wrap MovieDef | Confirmed |
| `0xD72C30` | Create raw movie instance | Confirmed call shape; name inferred |
| `0xD719C0` | Construct player | Confirmed |
| `0xD7C900` | Create/replace/register player | Confirmed |
| `0xD78E10` | Open built-in catalog movie | Confirmed |
| `0xD7A370` | Open caller-described movie | Confirmed |
| `0xD736E0` | Initial zero-time advance/capture | Name inferred |
| `0xD73850` | Per-player frame step | Advance call inferred |
| `0xD7ADE0` | System player-update loop | Confirmed iteration/call |
| `0xD71EC0` | Destroy player | Confirmed |
| `0xD76E20` | Destroy system | Confirmed |
| `0xD6D790` | Installed FSCommand no-op | Confirmed bytes |

## Safe investigation order

1. Observe high-level descriptor open `0xD7A370` and create/register
   `0xD7C900`; they preserve more engine state than the raw loader.
2. Follow one naturally opened movie through `0xD7C370`, `0xD7C900`, and
   `0xD73850`, recording descriptor bytes, collection membership, thread, and
   final release.
3. Keep `0xD72CC0`, `0x112CF30`, and raw virtuals observational until every
   argument and owner is known.
4. Keep movie/value handles opaque and host-owned.
5. Avoid a global hook on `0xD7D540`; it affects every GFX resource load.
6. Do not replace the FSCommand state merely because the current override is
   empty.

## Remaining blockers for custom GFX

The following are unresolved:

1. complete semantics of the 16-byte movie descriptor and its flags;
2. legal create, mutate, advance, and destroy threads/phases;
3. render/display registration through system `+0x968`;
4. capture/render submission after `Movie::Advance` and command-queue
   synchronization;
5. routing mouse, keyboard, controller, and native menu input to a player;
6. safe game wrappers for Invoke, variable access, and complex values;
7. use of `ExternalInterface` or function objects by a known game movie;
8. cache eviction and removal rules;
9. roles of protected boundaries `0xD78020` and `0xD77F20`; and
10. registration and resource ownership for a wholly new descriptor.

Reproduce the static map with the [Ghidra workflow](../tools/ghidra-workflow.md),
record the exact executable identity, and promote a candidate only after its
callers, arguments, object relationships, live behavior, and paired teardown
agree. Related narrow ownership rules are in the
[scene-object bridge](../reference/scene-object-bridge.md).
