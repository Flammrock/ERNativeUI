# Elden Ring native UI system

This page is the compact mental model used to choose safe ERNativeUI hook
boundaries. It synthesizes static executable analysis, GFX inspection, current
production behavior, and bounded live tests on the
[reference build](../README.md#reference-build).

Native class and function labels on this page are analytical names. Identifiers
beginning with `ERUI_` instead belong to the public C ABI. Every RVA and native
object offset is build-locked and private; the
[public API](../../features.md) exposes only copied values, opaque ERNativeUI
handles, and normalized callbacks.

## End-to-end model

```text
GFX / SWF assets                         native menu control
----------------                         -------------------
static named placements                 MenuWindow-derived page
        |                                      |
        v                                      v
game resource repository                native row constructors
        |                                      |
        v                                      v
MovieDef -> movie/player <------?------ row/controller collections
        |                                      |
        v                                      v
advance / capture / render              input, focus, jobs, callbacks
        ^                                      |
        |                                      v
existing-object bridge <----------- SceneObjProxy / GFx value
```

- **Confirmed:** Both sides exist and current features use the native page and
  row layer as their primary integration boundary.
- **Inferred:** Ordinary native row/controller changes drive the visible
  `Item_N_0/Widgets/...` instances.
- **Hypothesis:** Recovering the shared row-to-widget binder will explain the
  remaining discriminator, enabled-state, focus, and value-presentation
  details.
- **Rejected:** A static placement alone is a functional row, and a loaded
  movie alone is a complete Elden Ring screen.

The missing middle is why ERNativeUI composes existing native pages instead
of exposing raw Scaleform objects to client mods.

## 1. Static assets are presentation

- **Confirmed:** `02_040_optionsetting.gfx` is the outer Configuration movie.
  Its display tree contains `WindowList`, `TabList`, `BackTabList`,
  `StatusBar`, and `MenuTitle`.
- **Confirmed:** The visible Game Options panel is named
  `WindowList.ControllSetting`; the spelling is an internal asset name, not a
  reference to the separate Controller Settings screen.
- **Confirmed:** A physical `Item_N_0` placement instantiates a shared generic
  row definition. Its `Widgets` object contains named presentation choices
  such as Button, TextInput, ComboBox, and Slider. The optional ERNativeUI
  patch adds a standalone ColorPicker sibling.
- **Confirmed:** `02_042_pc_graphicsetting.gfx` hosts the Advanced
  Settings-style physical page used for provider subpages and continuation
  slices.
- **Confirmed:** The optional patch can expand validated Game and Camera
  placements to 13 rows and add presentation resources for labels, text input,
  and color swatches. `ERNativeUI.dll` does not parse or patch GFX at runtime.
- **Rejected:** Cloning `Item_N_0` registers another native row or callback.
  It changes visual capacity only.

Each placement owns a runtime instance, while the underlying character
definitions are shared inside the movie. A structural patch must therefore
preserve definition order, character IDs, depths, transforms, scaling grids,
and movie/tag lengths.

## 2. Resource and movie runtime

- **Confirmed:** A native resolver builds `menu:/Win/%s.gfx` and then
  `menu:/%s.gfx`. Resident resources, an on-demand resource cache, and a
  movie-definition cache sit above the loader.
- **Confirmed:** Elden Ring wraps a raw `GFx::MovieDef` in
  `CSScaleformMovieDef`, then retains it in `CSScaleformSwfPlayer` beside a raw
  movie pointer. A system frame path advances active players.
- **Inferred:** The raw virtual calls matching public Scaleform call shapes are
  MovieDef `CreateInstance`, Movie `Advance`, and, in the dedicated text
  editor, Movie `HandleEvent`.
- **Hypothesis:** The still-unmapped render/capture registration around the
  renderer bridge and command queue is part of the minimum safe custom-movie
  owner.
- **Rejected:** The installed Elden Ring `CSScaleformFsCommandHandler` is the
  normal OptionSetting dispatcher. Its recovered override returns
  immediately; generic Scaleform FSCommand strings elsewhere do not overturn
  that game-specific result.

Resource lookup and movie construction are useful observation anchors, not
approved constructors. A provider-owned movie would also need fonts and image
resolution, layer/viewport registration, update and rendering, input/focus,
and deterministic removal.

## 3. Native pages and rows

- **Confirmed:** The Configuration family includes a top dialog, native
  category selection, `OptionSettingDialog` panels, and the specialized
  `PadSettingDialog` used by Controller Settings.
- **Confirmed:** The current host detours the Game Options materializer and
  independent Camera, Display, Sound, Network, Keyboard/Mouse, and Graphics
  materializers. It calls vanilla exactly once, validates the live page,
  observes its row count and visual capacity, and then adds only provider rows
  that fit or a Next row.
- **Confirmed:** Toggle, slider, inline choice, popup choice, action, TextInput,
  and ColorPicker features enter through recovered native row/controller
  paths. The host owns stable values, text, and callback adapters for as long
  as native objects can reference them.
- **Confirmed:** A provider submenu is a logical ERNativeUI page materialized
  in the game's existing Advanced Settings physical page. It is not a new GFX
  movie.
- **Inferred:** The native constructors populate the same row/controller
  collections traversed by the page's frame dispatcher. The exact generic
  binder from those records to named GFX widgets remains incomplete.

API 1.1's built-in-page feature is finished and uses those independent native
materializers. The real Controller Settings page is deliberately not a generic
built-in destination; API 1.1 reaches it through the specialized Input
Bindings model instead.

## 4. Pagination, Back, and page jobs

- **Confirmed:** Logical content is compiled into physical slices. Next pushes
  a real native child page and records an owned route.
- **Confirmed:** Previous validates that its target is exactly one slice back,
  then invokes native Back with the packed action observed from a real Back
  press. Elden Ring performs the fade, stack pop, and parent restoration.
- **Confirmed:** Back does not synchronously pop the screen. It builds a page
  action, submits it to the page-owned intrusive task container, and a later
  frame promotes and updates the active task.
- **Inferred:** The concrete task that commits the final fade/pop transition
  and the semantic names of its status values have not been fully recovered.
- **Rejected:** Calling the inner event-submission helper with a guessed action
  object is safe. The known wrapper is used because it constructs and owns the
  required action.

The live visual capacity comes from the constructed native page rather than
from a loose GFX parse. A full panel can have no room even for Next; that
destination then fails locally instead of overflowing the native list.

## 5. Existing-object Scaleform bridge

- **Confirmed:** The path resolver constructs an owning `0x60`-byte
  `SceneObjProxy` result. It resolves slash-separated existing members and
  embeds a referenced `CSScaleformValue` that needs its observed destructor.
- **Confirmed:** The narrow bridge can assign UTF-16 text to a resolved
  TextField. It backs the current physical-page title work.
- **Confirmed:** Non-final path components have a native fixed-buffer
  precondition; the resolver is also variadic rather than a generic plain
  three-argument string function.
- **Inferred:** Several nearby wrappers operate on TextField color and scroll
  state, but not every exact public-SDK property name is proven.
- **Rejected:** The enumerated wrapper cluster contains a general Invoke,
  SetMember, CreateObject, CreateArray, or AttachMovie facility. Those
  operations may exist elsewhere, but they have not been recovered with an
  Elden Ring owner and lifetime.

Never cache a raw GFx value or proxy across page replacement. Resolve, perform
the proved operation inside the live scope, and release through the matching
native path.

## 6. Input and focus

- **Confirmed:** The base `MenuWindow` frame dispatcher receives an input-gate
  byte, walks native rows/controllers, clears the local gate after a consuming
  operation, and advances page-owned jobs.
- **Confirmed:** Button actions invoke retained callables synchronously on the
  native UI path. Host-observed value, text, color, binding, and alert
  completion callbacks are normalized and delivered on the ERNativeUI worker
  according to the public callback contract.
- **Confirmed:** The native TextInput editor advances the underlying page with
  a forced-zero gate, gives the real gate to its embedded editor, translates
  copied Windows key/character/mouse records, and commits or cancels through
  native actions. This is the exclusive-focus lifecycle used by API 1.1.
- **Confirmed:** API 1.1 Input Bindings extends the specialized native binding
  screens and observes released-to-pressed global input edges. It deliberately
  does not consume the game's underlying action.
- **Inferred:** The complete ordinary controller/mouse/keyboard route before
  each row subobject is not mapped as one reusable dispatcher API.

Input ownership stays beside the native object that consumes it. Client
callbacks receive semantic values and device masks, never page pointers,
native input packets, or a license to invoke UI functions from their own
thread.

## 7. Native modal dialogs

- **Confirmed:** Supported alerts build a native descriptor and intrusive
  `MenuWindowJob`, publish it only into an empty game blocking slot, allow the
  owned popup to consume input, poll primary/secondary completion, and retire
  job and custom text before dispatching the client completion.
- **Confirmed:** The host never replaces a game-owned task in that slot.
- **Confirmed:** Page-frame and Back gates use exact popup, job, and page
  identity rather than a global "dialog visible" flag.
- **Rejected:** Action value `3` can be suppressed globally. It means page Back
  in the underlying menu but also participates in the secondary popup action;
  context and ownership are required.

Unknown dialog builder kinds and descriptor fields are not variants merely
because changing them once produced a visible object.

## 8. Ownership and teardown rules

| Native state | Established rule |
|---|---|
| Scene-object path result | Destroy the embedded referenced Scaleform value; byte-copying is not retention. |
| Native row state and erased callable | Keep host-owned backing alive for the complete installed menu lifetime. |
| Page action/job | Follow its intrusive retain/consume/retire protocol; submission is not completion. |
| Popup blocking job | Publish only into the confirmed empty owner slot and retire after native completion. |
| Movie player and definition | Player destruction releases its raw movie before its retained definition; the full cross-plane page/player order is still incomplete. |
| Client provider | Hot unload is unsupported while native rows, hooks, jobs, or callbacks can target client/host code. |

- **Hypothesis:** A thread and reentrancy census across construction, movie
  advance, row actions, popup update, and teardown will identify a narrower
  internal UI scheduler contract.
- **Rejected:** A caught access violation substitutes for ownership proof.

## Choosing an extension layer

Use the highest layer that already owns the behavior:

| Need | Preferred layer | Status |
|---|---|---|
| Rows in supported settings panels | Native page materializer and row constructors | **Confirmed**, current API 1.1 |
| Subpages and pagination | Native child-page push and Back/task path | **Confirmed**, current API 1.1 |
| Existing title text | Scoped SceneObjProxy/TextField bridge | **Confirmed**, narrow current use |
| Optional capacity or widget artwork | Offline structural GFX patch | **Confirmed**, optional presentation |
| Alerts and selection dialogs | Exact native descriptor/job lifecycle | **Confirmed**, current API 1.1/1.0 features |
| Custom top-level Configuration tabs | Native category records plus a virtualized presentation window | **Hypothesis**, exploratory API 1.2 work |
| Site of Grace entries | Independently recover that menu family | **Hypothesis**, exploratory API 1.2 work |
| Provider-loaded custom GFX | Full resource/movie/render/input owner | **Hypothesis**, exploratory API 1.2 work |
| Generic raw Scaleform API | Typed value, invocation, callback, and lifetime abstraction | **Hypothesis**, not a public feature |

The [methodology](../methodology.md) defines when a hypothesis may move into
production. The [address map](../address-map/README.md) preserves the
historical exact-build seed set; current source remains the production truth.

## Highest-value unknown seams

1. The shared native row/controller-to-widget binder.
2. The concrete owner at `SceneObjProxy + 0x20` and its relationship to the
   page and `CSScaleformSwfPlayer`.
3. Complete capture/render registration around the Scaleform render bridge.
4. The generic ordinary-row input route and an explicit thread/reentrancy map.
5. Complete native tab record semantics, icon/text binding, overflow, and
   destruction.
6. Independent page/action maps for Site of Grace and other menu families.
7. A proved Invoke/SetMember/object-creation wrapper only if a concrete
   feature requires it.

The detailed supporting maps are:

- [Configuration class map](configuration-class-map.md)
- [Scaleform movie lifecycle](scaleform-movie-lifecycle.md)
- [Input and event dispatch](input-and-event-dispatch.md)
- [Native UI class hierarchy](../reference/native-ui-class-hierarchy.md)
- [Scene-object bridge](../reference/scene-object-bridge.md)
