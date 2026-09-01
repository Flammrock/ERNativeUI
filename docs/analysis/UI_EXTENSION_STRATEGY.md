# UI extension strategy

This document turns the current reverse-engineering evidence into a practical
architecture for extending Elden Ring's UI. It is intentionally conservative:
an operation is recommended for production only when its construction,
ownership, input, and teardown behavior have all been observed.

All native names are ERNativeUI analytical names. Addresses and object offsets
are private, build-specific implementation details, never part of the public
ERNativeUI ABI.

## Executive recommendation

ERNativeUI should remain a native UI orchestrator first and a Scaleform bridge
second:

```text
mod clients
    -> stable versioned C ABI
    -> ERNativeUI-owned declarative model and state
    -> highest proven Elden Ring native page/row/dialog interface
    -> existing game movie and native input/focus/lifetime machinery

                                   narrow fallback only
                            -> resolved existing GFx object
                            -> proved operation with bounded lifetime
```

This layering already works because native constructors provide more than
pixels: they also connect focus, controller/mouse input, sounds, callbacks,
page transitions, and destruction. Directly adding a movie clip would provide
none of those relationships by itself.

## What can safely be extended now

### Existing Game Options views

**Production-ready.** ERNativeUI can extend Controller Settings and construct
owned child pages using the confirmed native row functions for toggles,
sliders, inline choices, popup choices, action rows, and submenus. Native Back
performs the actual page pop. The optional GFX patch increases visible row
capacity and repairs the Controller action-row label, but the native six-row
asset remains supported.

The safe pattern is:

1. enter through the validated Controller or subpage handler;
2. call the original handler when extending a vanilla page;
3. add rows only while the live page and its native construction context are
   valid;
4. retain state for exactly as long as native rows may reference it;
5. navigate through the game's open-subpage and Back functions.

This pattern should be generalized to Audio, Graphics, Network, or other tabs
only after each tab's handler, live page type, row count/capacity, and teardown
have been confirmed. Similar-looking panels in the same GFX are evidence of a
shared presentation vocabulary, not proof of a shared native object ABI. The
[Game Options native class map](GAME_OPTIONS_CLASS_MAP.md) establishes the
top/category/composite split and narrows the remaining per-panel work.

### Existing named Scaleform objects

**Production-ready only for the narrow title/text bridge.** The path resolver
constructs an owning `0x60`-byte `SceneObjProxy`; slash-separated members are
resolved by a confirmed GetMember-equivalent; the UTF-16 setter modifies an
existing TextField; and the embedded value has a known destructor. This is
appropriate for short, synchronous text changes on a live movie context.

Do not generalize this into an arbitrary `GFx::Value` API yet. Text color and
TextField scroll operations are statically mapped, but have no current public
feature requiring their exposure. Invoke, SetMember, CreateObject, AttachMovie,
and a general property setter remain unproved.

### Offline GFX extension

**Production-ready for narrow, structural transformations.** The patcher can
clone validated placements, preserve tag lengths/depths, and verify the result.
This is suitable for presentation capacity or a missing named field. It must
remain optional where a native fallback exists.

Offline placement is not runtime registration. Adding a row, tab, or widget to
the file does not add it to the game's native model or event routes.

### Generic dialogs

**Production-ready within the discovered variants.** The descriptor, builder,
blocking task slot, update, poll responses, and retirement path are known for
the supported centered/bottom and zero/one/two-button forms. Dialog scheduling
must stay on its owned FIFO and UI update boundary. Unknown builder kinds or
descriptor fields are not extension points.

## Safe native hook boundaries

Prefer a high-level function when it already represents the full user action
or object lifecycle. Current safe boundaries have one or more of these traits:

- a unique semantic signature plus build validation;
- a typed call contract exercised repeatedly in game;
- an original trampoline that can be called exactly once;
- a narrow owner/page identity test before changing behavior;
- a complete cleanup or retirement path;
- failure that can disable one optional feature without disabling the host.

Examples are the Controller/subpage handlers, native Back, the specific text
resolver, popup-choice provider/template pair, and the dialog update/poll pair.
The path-resolver mid hook is safe only because it changes one exact path in a
short owned-page scope and otherwise preserves execution.

Hook chaining is not automatically safe. ERNativeUI's Solid Uncapper support
is a specific protocol: resolve pristine targets, wait for the expected set of
foreign detours, validate their owner module, then chain. A partially patched
set or an unknown detour is rejected. This policy should be repeated per known
integration, not replaced by generic "follow any jump" behavior.

## Event dispatch, input, and focus

The native page contains an active-task/queue subobject at `+0x10`. The
recovered path creates typed page actions, enqueues them, promotes one task on
the next base frame, invokes its virtual update, and retires it after a
terminal result. Native Back action `3` uses this route. That is enough to use
the proven Back wrapper, but not enough to publish a generic event API:
concrete task factories, payload schemas, target ownership, result meanings,
and most action kinds remain unknown.

Input ownership belongs beside the native page/popup that consumes it, not in
client callbacks and not in the GFX timeline. The inspected Game Options movie
contains no menu input dispatcher in ActionScript. Existing rows inherit
native focus automatically; dialogs require the proved page-frame and Back
gates while the popup owns input. The unreleased TextInput implementation now
uses the native producer, controller, character-name editor factory, and
completion action as one lifecycle. Unicode/IME behavior, cancel paths,
controller keyboard behavior, focus restoration, and teardown remain explicit
1.1 live-validation gates rather than assumptions made from the GFX timeline.

Client callbacks should receive normalized ERNativeUI events after native
state has changed. They should never receive raw page pointers, GFx values,
dispatcher nodes, or an opportunity to call game UI functions from arbitrary
threads.

## Creating a custom interface from existing GFX

There are three materially different designs:

1. **Compose a custom page from native rows.** This is the recommended and
   currently working design. It reuses native Game Options presentation and
   behavior without creating a new movie.
2. **Extend an existing movie structurally.** Appropriate for bounded static
   capacity or named presentation fields. Native binding must already exist or
   be separately recovered.
3. **Load and own a new movie.** This is a research target, not a production
   capability. It requires loader/resource registration, `font.swf` and
   external-image resolution, movie/player ownership, viewport/render
   attachment, advance/input integration, and deterministic teardown.

Reusing a sprite definition is insufficient for option 3. Character IDs are
local to a movie, imported resources require their native resolver, and a
runtime instance still needs a native owner and event route. The nine static
tab placements likewise do not prove an expandable tab container.

## Lifetimes and thread rules

- Perform UI mutation only on the observed UI construction/update paths. No
  evidence permits Scaleform calls from a worker thread.
- Treat page, row, popup, movie-context, and proxy pointers as non-owning unless
  a specific retain/release contract has been recovered.
- Destroy every temporary `SceneObjProxy` through its embedded
  `CSScaleformValue` destructor; do not copy the embedded GFx bytes casually.
- Keep row backing state and erased native callback objects alive until the
  owning page is retired.
- Install a dialog job through the consuming intrusive slot and release only
  the returned temporary reference according to the confirmed count protocol.
- Do not support hot unload while hooks, native callbacks, rows, or jobs can
  still target ERNativeUI code or memory.

## Compatibility and update risks

| Risk | Required mitigation |
|---|---|
| Game update changes code/layout | Hash/build identity, semantic AOBs, instruction validation, dependency gating, and fail-closed feature installation. |
| Another mod hooks the same function | Explicitly supported chain with module and whole-hook-set validation; otherwise report incompatibility without guessing. |
| Optional GFX differs or is absent | Inspect structural capacity at runtime and retain a native-layout fallback. |
| Shared sprite edit affects unrelated panels | Patch only the intended definition/placement and compare the output structurally. |
| Client compiled against an older wrapper | Keep the C ABI versioned and append-only within a major version; negotiate structure sizes and capabilities. |
| Multiple clients compete for pages/titles/input | Central host ownership, deterministic ordering, page-scoped identity, and one input owner at a time. |
| Native callback outlives client DLL | Host-owned trampoline/state, unregister semantics, and retirement before accepting unload; otherwise document unload as unsupported. |

Proximity of RVAs, matching visual appearance, or a plausible decompiler type
is not compatibility evidence.

## Explicit no-go areas

Until further proof exists, production code must not:

- patch `eldenring.exe` on disk or use permanent byte-code instrumentation;
- call a candidate function solely because it is near a known UI function;
- expose raw native pointers, vtables, RVAs, or GFx values through the public
  API;
- invoke unknown object-interface/movie virtual slots;
- synthesize dispatcher nodes or submit unknown event kinds;
- instantiate or attach a movie clip without its owner and destruction path;
- load a custom GFX without proving resource resolution, render/advance
  registration, input routing, and teardown;
- infer editable TextInput behavior from its cursor sprite or `readOnly` fields;
- treat the native fixed capacity of ten, or the nine visible GFX placements,
  as an expandable registry or assume that adding a placement registers a
  tab;
- follow arbitrary third-party detours or support hot unload opportunistically;
- allow client code to mutate UI from background threads.

## Recommended phased architecture

### Phase 1: consolidate the proven host

Keep one ERNativeUI host responsible for scanning, hooks, page identity,
native backing storage, dialogs, localization, pagination, and deterministic
client ordering. Keep the public surface declarative and C-compatible. Expose
capability queries so optional native features fail independently.

### Phase 2: generalize native Game Options panels

Map each desired vanilla tab from its handler down to the shared row binder.
Only then add a host-owned `target_panel` concept. Validate per-panel capacity,
allowed row kinds, page lifetime, title target, and Back behavior. This enables
Audio/Graphics/etc. without making Scaleform itself the public abstraction.

### Phase 3: validate and freeze the recovered TextInput lifecycle

The native row producer/controller, stable `CS::MenuString` backing,
character-name editor activation, per-row maximum patch, and confirmed-value
callback are integrated behind the unfrozen API 1.1 capability. Complete the
keyboard/controller/mouse, Unicode/IME, cancel, focus, pagination, modal, and
teardown matrix before freezing that API. The remaining shared widget-binder
work is still useful for future row types, but TextInput no longer depends on
discovering a generic discriminator first.

### Phase 4: native tab registration

The top dialog's fixed-capacity-ten `MenuOptionCategory` container,
selected-index dispatch, and composite panel cache are now mapped. Recover the
remaining `0x88`-byte record fields, visible-tab-to-factory mapping, icon/name
binding, mouse/shoulder-button behavior, and complete retirement contract; see
the [Game Options native class map](GAME_OPTIONS_CLASS_MAP.md). The host should
own a virtualized tab registry for an unbounded public API; the static
nine-slot asset may become a presentation window, not a presumed hard limit.

### Phase 5: other menu families

Treat Site of Grace or other menus as separate native families. Recover their
page/row constructors and action route independently, then adapt them behind a
common ERNativeUI declarative model. Do not reuse Game Options object layouts
without proof.

### Phase 6: advanced Scaleform services

Only after call sites prove signatures and lifetimes, add internal operations
for SetMember, Invoke, display information, arrays/objects, and event bridging.
Keep these internal until several features establish a stable abstraction.

### Phase 7: custom movie hosting

This is the last phase. Recover the loader, resource registry, movie creation,
render/advance integration, input/focus adapter, and teardown as a complete
subsystem. A host-owned custom-view service can then isolate client mods from
all raw Scaleform objects and version-specific details.

## Promotion checklist

A research result may enter production only when it has:

1. a build-locked function and call-site derivation;
2. a validated signature and object offsets;
3. confirmed owner, thread, and lifetime rules;
4. bounded positive and negative live tests;
5. a cleanup path tested during Back, page replacement, and game shutdown;
6. feature-local failure behavior;
7. compatibility analysis for shared hooks;
8. documentation in this directory and a guarded address-map entry where
   appropriate.

This sequence deliberately favors native composition over visually impressive
but ownerless Scaleform manipulation. It is the shortest path to a powerful UI
framework that remains recoverable after game updates and coexistence issues.
