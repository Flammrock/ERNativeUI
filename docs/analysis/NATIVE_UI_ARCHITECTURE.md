# Native UI architecture

Status: first-pass synthesis of confirmed executable, GFX, and runtime evidence

This document joins the focused reverse-engineering notes into one end-to-end
model of Elden Ring's native UI. It explains how a GFX asset becomes a live
Scaleform movie, how native `MenuWindow` pages and controllers operate beside
that movie, how named scene objects are reached, how input and page jobs flow,
and which owners perform teardown.

It is a research map, not a game ABI. Class names come from validated MSVC RTTI
where stated; other names are analytical. Every RVA and object offset is a
private, build-specific implementation detail and must not cross ERNativeUI's
public C ABI.

## Build and evidence labels

The native addresses in this document apply only to:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The diagrams use these labels:

- **[C] Confirmed**: encoded in the tested executable/GFX or reproduced in a
  bounded in-game test.
- **[S] Strong inference**: call shape, ownership, and public Scaleform
  behavior agree, but an exact native type or original symbol is absent.
- **[U] Unknown**: a required relationship has not yet been recovered and is
  not safe to fill in by proximity or analogy.

"Confirmed" does not mean stable across game versions. Production still needs
a semantic signature, instruction and relationship validation, dependency
gating, and a fail-closed optional feature.

## Executive model: two coordinated planes

The UI is best understood as a presentation/runtime plane beside a native
control plane. The known scene-object bridge joins them; the ordinary row
binder remains only partially recovered.

```text
Scaleform asset and runtime plane
---------------------------------
[C] 02_040_optionsetting.gfx + font.swf + external images
          |
          | [C] game resource path and repository
          v
[C] GFX resource/capability caches
          |
          | [C] loader and MovieDef wrapper
          v
[C] CSScaleformMovieDef ------> raw GFx::MovieDef
          |
          | [S] CreateInstance call shape
          v
[C] CSScaleformSwfPlayer -----> raw GFx::Movie
          |
          | [S] Movie::Advance call shape
          v
[U] complete capture/render submission through render bridge/command queue


Native page and control plane
-----------------------------
[C] Game Options top -> category selector -> OptionSettingDialog panel
          |
          +-- [C] native row constructors -> page/controller collections
          |                                    |
          |                                    | [U] shared row/widget binder
          |                                    v
          |                         [C] Item_N_0/Widgets/{...} instances
          |
          +-- [C] SceneObjProxy -> CSScaleformValue -> existing GFx object
          |                 ^
          |                 `---- confirmed GetMember/text operations
          |
          +-- [C] MenuWindow frame dispatch -> native controllers/callbacks
          |
          +-- [C] Back -> intrusive page task queue -> active-task updates
          |
          `-- [C] popup descriptor -> MenuWindowJob -> CSPopupMenu update/poll

Cross-plane relationships
-------------------------
[C] Native code can resolve and mutate selected existing movie objects.
[S] Native row/controller changes drive the visible row widgets.
[U] The exact ordinary-row binder and the concrete type at
    SceneObjProxy +0x20 that connects a page/proxy to its player/movie.
```

This split prevents two common mistakes:

1. A static GFX placement is a visual instance, not a native row, focus target,
   callback, or page entry.
2. A loaded `GFx::Movie` is not by itself a usable Elden Ring screen; it still
   needs native ownership, update/render registration, input/focus, and
   deterministic teardown.

## 1. Asset and presentation layer

### Static GFX structure

**[C]** `02_040_optionsetting.gfx` is an uncompressed GFX/SWF 11 movie using
ActionScript 3. It imports `font.swf`, declares external image resources, and
contains the static Game Options display hierarchy. Its main named objects are
`StatusBar`, `WindowList`, `BackTabList`, `TabList`, and `MenuTitle`.

The Controller panel is a set of static row placements:

```text
WindowList.ControllSetting
`-- Item_N_0
    `-- Widgets
        |-- Button
        |-- TextInput
        |-- ComboBox
        `-- Slider
```

**[C]** Every physical row contains all four widget placements. The native
layer selects/configures the presentation actually used for that row.

**[U]** The complete shared discriminator that connects a normal native row
record to one widget instance has not been recovered. The TextInput-specific
helper at `0x86BCB0` proves that native code can hide the other widgets and
show TextInput, but it does not yet provide a general row binder ABI.

The movie's own ActionScript is presentation-oriented: its exported classes
contain frame stops and animation loops, but no menu-specific listener,
`ExternalInterface`, or FSCommand dispatch. This is evidence about
`02_040_optionsetting.gfx`, not every movie in the game.

**[C]** The executable also contains distinct resource names
`02_990_TextInput` and `02_991_TextInput2` used by four native
`SoftwareKeyboardJob` factory variants. Each variant allocates a `0x1A8`-byte
job through common constructor `0x81CCB0`, which also creates and registers a
`MenuMemberJob` adapter. The `02_990` variant uses message ID `0x470AE`, a
limit-like value `0x10`, and sets configuration flag bit 1; the three `02_991`
variants use IDs `0x7BCDC`, `0x1E08D`, and `0x1E08C`, use value `8`, and clear
that bit. The field's unit, flag meaning, and localized message semantics are
**[U]**.

**[U]** The available direct-call graph does not connect this job family to
`TextInputDialog` or a platform API. The resource names and native RTTI do not
prove Steam integration, device selection, or that these movies share the
inline row lifecycle. They must not be conflated with the read-only TextInput
placement inside `02_040`.

### Offline structural extension

**[C]** The optional ERNativeUI GFX patch can add validated Controller
`Item_N_0` placements and the nested Button label field expected by native
binding. The game works with the original six-row movie and the validated
thirteen-row presentation.

This extension changes visual capacity only. It does not register more rows in
the native page, create callbacks, or extend the native tab collection. The
asset contains nine visible tab slots. Exact-build analysis now confirms that
the top dialog owns a fixed-capacity-ten native category list, but category
record semantics and any presentation beyond the nine stock placements remain
**[U]**; see the [Game Options native class map](GAME_OPTIONS_CLASS_MAP.md).

## 2. Resource, loader, and movie runtime

### System and loader owners

**[C]** `CSScaleformImp` owns a `0x1E30`-byte `CSScaleformSystem`. Confirmed
service, cache, and collection fields include:

| System offset | Confirmed object or structure |
|---:|---|
| `+0x950` | owned allocation paired with `Scaleform::System::Destroy` during teardown; exact class unresolved |
| `+0x958` | `CSScaleformLog*` |
| `+0x960` | `CSScaleformThreadCommandQueue*` |
| `+0x968` | renderer/Scaleform bridge; exact class unresolved |
| `+0x970` | `CSScaleformLoader*` derived from `GFx::Loader` |
| `+0x978` | resident GFX resource collection |
| `+0x990` | on-demand GFX resource cache |
| `+0x9A8` | movie-definition/cache tree |
| `+0x9C0` and following fields | player/name and active-player structures |
| `+0xBE8/+0xCF0` | fixed-capacity secondary player-pointer vector and count; maximum `0x20` |

The loader installs a game file opener, image creator, log, translator-related
states, and a `CSScaleformFsCommandHandler`. The installed FSCommand override
at `0xD6D790` is exactly `ret 0`; it is a confirmed no-op, not the ordinary
Game Options event route.

### Resource lookup and caches

**[C]** `0xD7D540` formats and queries `menu:/Win/%s.gfx`; when that does not
resolve, it tries `menu:/%s.gfx`. It returns game resource data/capability
ownership, not a live movie.

The resolver feeds three confirmed layers:

| RVA | Role |
|---:|---|
| `0xD78A60` | preload the 113-entry built-in GFX catalog into resident storage |
| `0xD79140` | acquire/cache a GFX resource on demand |
| `0xD7C370` | acquire/cache a `CSScaleformMovieDef` |

The external images and `font.swf` dependency therefore participate in a game
resource pipeline. Copying a sprite definition or passing a loose filesystem
path does not reproduce that pipeline.

**[C]** Definition-cache insertion is conditional. `0xD7C370` always checks
the tree at `+0x9A8`, but it inserts a newly loaded definition only when the
lazy `Scaleform.SwfCache` setting is enabled and descriptor byte `+0x04` is
nonzero.

Nearby confirmed format clusters construct menu Scaleform-layout bundle paths
(`.sblytbnd`), extension-parameterized resource paths, and menu texture-package
paths (`.tpf`/`tpfbhd`). Their path construction is confirmed; the repository
handles, asynchronous states, and destruction contracts they feed remain
**[U]**.

### Movie definition, player, and update

The confirmed ownership chain is:

```text
0xD7C370 acquire/cache definition
    -> 0xD72CC0 prepare loader arguments
    -> 0x112CF30 enter linked GFx loader implementation       [S CreateMovie]
    -> 0xD72BB0 allocate CSScaleformMovieDef                  [C]
         +0x10 raw GFx::MovieDef*
         +0x18 owning system/context

0xD72C30 call raw MovieDef virtual +0xC0                     [S CreateInstance]
    -> 0xD719C0 construct CSScaleformSwfPlayer                [C]
         +0x08 CSScaleformSystem*
         +0x10 retained CSScaleformMovieDef*
         +0x18 raw GFx::Movie*
         +0x20 copied movie descriptor
    -> 0xD7C900 create/replace/register player                [C]
```

**[C]** The wrapper at `0xD72CC0` supplies zero per-call loader flags.
`0x112CF30` rejects an empty converted path or a loader whose internal pointer
at `+0x08` is null; otherwise it ORs the loader's `+0x18` flags into the call
to `0x1165A60`.

**[C]** Player creation has at least two post-construction routes. One calls
`0xD74140` and appends the player to `+0xBE8/+0xCF0`; another calls the
zero-time advance/capture helper `0xD736E0`. Descriptor byte `+0x04` also
selects behavior involving the `+0x9C0` name tree and `+0xD00` list. The
descriptor values and the two route booleans remain **[U]** at the semantic
level.

The high-level built-in-ID and caller-descriptor open paths are at `0xD78E10`
and `0xD7A370`. They are useful observation anchors because they include more
game ownership than the raw loader. They are not yet approved ERNativeUI call
interfaces.

**[C]** The system frame function at `0xD7ADE0` iterates an active player
collection and calls `0xD73850` for each non-null player.

Later in that function it scans `+0xBE8/+0xCF0`; when unresolved predicate
`0xD736D0(player)` succeeds, it removes a matching entry from the
`+0x9D8` collection under the `+0xDA0` lock and erases the secondary-vector
pointer. The coordination is confirmed, but the predicate's semantic name is
**[U]**.

**[S]** The raw movie virtual call at `+0xC0` inside `0xD73850` is
`GFx::Movie::Advance`: it receives a floating-point frame delta, catch-up count
`2`, and a capture boolean in the exact Microsoft x64 register positions of
the public method. An initialization path at `0xD736E0` calls the same slot
with delta `0.0` and capture enabled.

**[U]** The complete capture/render submission, the exact player registration
with the object at system `+0x968`, and the allowed thread/phase for creating a
new movie remain unresolved. This is why the shipped-movie lifecycle map does
not yet amount to custom-movie support.

## 3. Native page, job, and controller layer

### RTTI hierarchy

The native UI types form distinct layers:

```text
OptionSettingDialog
`-- PropertyEditDialog
    `-- GenericListSelectDialog
        `-- MenuWindow
            `-- SceneObjModifier
                `-- DLReferenceCountObject

PadSettingDialog
`-- OptionSettingDialog

OptionSettingTopDialog
`-- MenuWindow

OptionSettingTopDialog::_SettingTabControl
`-- SceneObjModifier

MenuWindowJob
`-- MenuJob
    `-- DLReferenceCountObject
```

`SliderCtrl` is a `MenuWindow` specialization. `SpinCtrl` is a
`SceneObjModifier`, not a `MenuWindow`. `TextInput` is a `SceneObjProxy`,
`TextInputController` is a `PropertyController`, and `TextInputDialog` is a
complete `MenuWindow`. Similar names or nearby RTTI records do not imply a
shared callable layout.

### Page creation and ERNativeUI materialization

**[C]** The current ERNativeUI path operates at this high native layer:

```text
Controller Settings handler 0x959DF0
    -> vanilla builds/publishes the live root page
    -> ERNativeUI calls native row constructors on that page

retained submenu/Next callback
    -> GameOptions_OpenSubpage 0x950850
    -> subpage handler 0x95B770 receives the physical child page
    -> ERNativeUI materializes one owned logical slice

Previous
    -> validate exactly one slice backward
    -> native Back 0x747CD0
    -> republish the already constructed parent after native pop
```

ERNativeUI's "custom page" is therefore a logical model materialized inside an
existing native Advanced Settings-style page. It is not a separately loaded
GFX movie.

**[C]** The game row interfaces already used in production construct toggle,
byte slider, inline choice, popup choice, and action rows. Native text-reference
objects supply labels/help; stable byte storage or retained erased callables
supply state and behavior. Temporary text-reference inputs are destroyed after
the constructors clone or retain what they require.

**[S]** Those constructors populate the page's row/controller structures that
the base frame dispatcher later traverses. The exact mapping from each public
constructor result to the `0x140`-byte row records and the visual
`Item_N_0` instance remains incomplete.

### `MenuWindow` state used by the current map

Important confirmed page fields and virtual routes are:

| Field or slot | Confirmed narrow role |
|---|---|
| page `+0x10` | page-owned active intrusive task plus queued task container, advanced once per base slot-2 invocation |
| page `+0x1F8..+0x200` | row range, `0x140`-byte records, used by the base frame dispatcher |
| page `+0x230..+0x28F` | persistent `SceneObjProxy` path result used by title construction |
| page `+0x3B0` | guard/input state consulted by frame and Back paths |
| page `+0xA38` | selected index/control state in Game Options specialization |
| page `+0x1260` | selected-controller collection root in `0x976D40` |
| vtable slot 2, `0x7463C0` | main native row/control frame dispatcher |
| vtable slot 11, base `0x746820` | base update route |
| Game Options slot 11, `0x958FF0` | selected-controller frame pass |
| vtable slot 12, `0x747CD0` | construct/submit Back or close action |

The page layouts beyond their observed fields are not complete C++
declarations. In particular, an offset used by one derived page must not be
assumed portable to another menu family.

## 4. Scene-object and value bridge

### Owning proxy result

**[C]** `Scaleform_ResolvePath` at `0x74B140` creates a `0x60`-byte
`SceneObjProxy` result:

```text
SceneObjProxy
+0x00  SceneObjProxy vtable after derived construction
+0x08  self link
+0x10  self link
+0x18  self link
+0x20  owner/movie context pointer                         [U concrete type]
+0x28  CSScaleformValue (0x38 bytes)
       +0x08 GFx::Value base/storage begins
```

Both proxy value accessors return `this + 0x28`. The embedded
`CSScaleformValue` owns referenced GFx storage and must be destroyed through
`0xD81590`; copying only its bytes is not a valid retain operation.

**[C]** The base helper at `0x733F00` initially installs the
`ComponentProxy` vtable and the three self-links. The derived path constructors
then overwrite only `+0x00` with the `SceneObjProxy` vtable. It is therefore a
base-prefix initializer, not a complete `SceneObjProxy` constructor.

### Path and supported operations

**[C]** `0xD81710` splits slash-separated paths and resolves each component.
Its direct-member helper `0xD81680` accepts object-like GFx values and calls the
source object interface at virtual offset `+0x20`. This is a confirmed
GetMember-equivalent operation with explicit intermediate-value cleanup.

`0x74B140` is also a `printf`-style variadic formatter before that traversal,
not a plain three-argument resolver. The current literal paths contain no
format directives. Within `0xD81710`, every non-final component is copied
through a fixed buffer and must be at most `0x20` bytes or the native range
check fails; the final component is passed directly.

Confirmed direct operations on an already resolved TextField include:

- UTF-16 text assignment through `0x74AE50`;
- text color assignment;
- horizontal and vertical scroll queries and updates; and
- overflow/scroll-bound queries.

ERNativeUI currently exposes only the title/text use that has a complete
feature need and validated lifetime. The other mapped TextField operations are
research anchors, not automatically public API candidates.

### What the bridge does not prove

The enumerated wrapper family contains no confirmed equivalent of:

- arbitrary `SetMember`;
- ActionScript `Invoke`;
- `CreateObject` or `CreateArray`;
- `AttachMovie` or runtime display-object insertion; or
- native event-listener registration.

Those operations may exist elsewhere in the linked Scaleform runtime. Their
existence in a public Scaleform SDK or in diagnostic strings does not establish
an Elden Ring wrapper signature, owner, thread, or lifetime. ERNativeUI must
not claim or expose them yet.

## 5. Input, focus, and UI event flow

### Ordinary rows and page input

**[C]** `MenuWindow` slot 2 at `0x7463C0` receives `(page, frame_float,
uint8_t* input_enabled)`. The byte is an input gate, not a raw event packet.
The function:

1. derives a local gate and forces it to zero when page `+0x3B0` requires it;
2. iterates `0x140`-byte native row records;
3. calls row/controller subobjects with that gate;
4. clears the local gate when an active row operation consumes it;
5. updates page-owned jobs/state; and
6. calls the page's slot 11 and another controller collection.

**[C]** `OptionSettingDialog` uses a slot-2 thunk to this base dispatcher. Its
slot 11 at `0x958FF0` reaches `0x976D40`, which updates only the selected
eligible controller in the collection rooted at page `+0x1260`.

**[U]** The full route from XInput/DirectInput, mouse cursor, or ordinary
keyboard navigation to the concrete row subobjects is not mapped. No evidence
shows that ordinary Game Options actions are all passed through a raw GFx event
method; the recovered frame path is principally native.

### Retained callbacks and ERNativeUI publication

**[C]** Action rows retain an MSVC-compatible erased callable and invoke it
synchronously from native UI processing. Toggle, slider, and inline-choice
rows retain pointers to stable host bytes. Popup-choice rows retain three native
callable objects and a separate one-based state byte.

ERNativeUI polls value rows on its own worker and publishes normalized client
callbacks after observing changes. That worker is not a proven context for
calling private game UI functions. Client callbacks must receive normalized
data, not native page, controller, movie, or `GFx::Value` pointers.

### Dedicated native text editor

The game contains a separate, more explicit input route:

```text
TextInputDialog slot 2 at 0x9BA0A0
    -> base MenuWindow slot 2 with a forced-zero gate
    -> embedded TextInput at dialog +0xA98 with the real gate
    -> pad accept/cancel predicates
    -> engine-copied Win32 key/char/mouse records
    -> raw movie virtual +0x118                         [S HandleEvent]
```

**[C]** This design gives the editor exclusive focus while the underlying base
page still advances without consuming input. It handles key down/up,
`WM_CHAR`, mouse move, and mouse button records, and commits or cancels through
native page actions.

**[S]** Raw movie virtual offset `+0x118` is
`GFx::Movie::HandleEvent`, based on the synthesized Scaleform-like event
records and call shape.

The static producer/controller/editor path now backs ERNativeUI's unreleased
production TextInput row. Live construction, editing, confirmation,
persistence, root/subpage presentation, and independent limits through 35
UTF-16 code units are confirmed. Unicode/IME behavior, cancellation, focus
restoration across every transition, and teardown remain release gates. The
movie's visual TextInput sprite alone is still insufficient; production uses
the recovered native row and editor lifecycle.

**[C]** A parallel `SoftwareKeyboardJob` path has four wrapper/factory pairs:
`0x81D610 -> 0x81CFD0`, `0x81D700 -> 0x81D160`,
`0x81D7F0 -> 0x81D2F0`, and `0x81D8E0 -> 0x81D480`. All converge on
constructor `0x81CCB0`. The parent update slot (`0x7AC700`) polls and retires
an owned child job; its `MenuMemberJob` slot (`0x81D9D0`) invokes stored target
`0x81DBF0` on the parent. **[U]** The callback's operation and any bridge from
this path to `TextInputDialog` remain unmapped, so these are research anchors,
not production construction APIs.

## 6. Back, page jobs, and modal dialogs

### Back is asynchronous page work

The confirmed Game Options Back path is:

```text
OptionSettingDialog Back method 0x9580C0
    -> packed action {kind=3, flags=0}
    -> MenuWindow slot 12 / native Back 0x747CD0
    -> page/action builder 0x747850
    -> attempted bounded insertion 0x7AA0D0 with owner = page +0x10
       (caller reference is consumed whether insertion succeeds or not)
    -> a later MenuWindow slot-2 invocation calls 0x7AA1F0
    -> if active is empty, promote at most one queued task
    -> active task virtual update through 0x7AA480
    -> [U] concrete task commits fade/pop/transition
```

`0x7AA0D0` temporarily retains the supplied object and attempts insertion into
the page-owned queue. The embedded queue's size is at queue `+0x28`
(aggregate `+0x30`) and an optional maximum is at queue `+0x30` (aggregate
`+0x38`); zero means unbounded. The helper consumes the caller's original
reference regardless of whether a full bounded queue rejects the object.

On each later base slot-2 invocation, `0x7AA1F0` promotes at most one queued
object only when the active pointer is empty, then updates the active slot.
`0x7AA480` retains that task, invokes its virtual slot 2, and clears/releases
the same slot when the first status DWORD is greater than `1`. A task submitted
behind an existing active task is not guaranteed to start on the immediately
following frame. The submit call is therefore not the synchronous visual pop.
Other action kinds, the concrete Back-task type, numeric status meanings, and
the transition-state writes remain unknown; production should call the
validated Back wrapper rather than synthesizing arbitrary queue nodes.

### Generic popup dialog path

Generic alerts use a separate `CSPopupMenu`/`MenuWindowJob` lifecycle:

```text
custom text IDs + 0x28-byte dialog descriptor
    -> builder kind and labels
    -> build intrusive MenuWindowJob 0x7B8660
    -> consuming assignment 0x7AA2E0 into CSPopupMenu +0x298
    -> CSPopupMenu_Update 0x7EF6D0 owns popup input
    -> intrusive task-slot update/retire 0x7AA480
    -> MenuWindowJob poll 0x7AE040
    -> result kind 2 primary / kind 3 secondary
```

**[C]** Centered and bottom variants with zero, one, or two buttons have been
validated. The blocking slot owns the task; game-owned tasks are never
replaced. ERNativeUI uses exact popup/job identity, a page-frame gate, and a
scoped Back gate so the popup alone consumes input, then invokes client
completion later on the host worker.

Unknown descriptor fields, builder kinds, and poll states are not extension
points merely because adjacent values produce a visible dialog.

## 7. Ownership and teardown

The following release contracts are independently confirmed:

| Owner | Owned state | Confirmed retirement |
|---|---|---|
| `SceneObjProxy` | embedded referenced `CSScaleformValue` | destroy nested wrapper through `0xD81590`; intermediate path values are released during traversal |
| native row/page | row state pointers, text, and erased callables retained or cloned by constructors | keep host backing storage alive for the complete installed menu/page reachability; hot unload unsupported |
| page `+0x10` | intrusive action/job objects | bounded insertion retains on success; submission consumes the caller reference even on rejection; terminal active-task status clears/releases the matching slot |
| `CSPopupMenu +0x298` | blocking dialog job | consuming assignment gives slot ownership; update clears/releases after completion |
| `CSScaleformSwfPlayer` | raw movie, retained `CSScaleformMovieDef`, callback/render-related members | destructor `0xD71EC0` releases raw movie before definition owner and remaining members |
| `CSScaleformMovieDef` | raw `GFx::MovieDef` | destructor `0xD72410` releases the raw definition |
| `CSScaleformSystem` | players/caches, render bridge, loader, command queue, log, resident resources | destructor `0xD76E20` tears down the subsystem |
| `TextInputDialog` | inline TextInput proxy, completion callable, completion guard | completion submits the native close action and the destructor releases dialog members; exact focus-release timing still needs live validation |

**[U]** The exact cross-plane order between a particular `MenuWindow` page,
its proxies/controllers, and the associated `CSScaleformSwfPlayer` during every
screen transition is not fully recovered. A pointer observed in one frame must
not be retained across Back, page replacement, or movie teardown without a
specific ownership protocol.

The operating-system thread identity of page update, row callbacks, movie
advance, and teardown also remains **[U]**. Static call order supports a common
UI/game-update-thread inference, but it is not a public guarantee.

## Practical hook-layer comparison

| Layer | Examples | What the layer already owns | Current recommendation |
|---|---|---|---|
| Stable ERNativeUI host/API | declarative pages, rows, values, alerts | client isolation, copied text/options, ordering, capability failure | Public interface. Keep C-compatible, versioned, and free of game pointers. |
| High native page handlers and row constructors | `0x959DF0`, `0x95B770`, native add-row functions | live page identity, native controls, focus, sounds, most row lifetime | Preferred production layer for existing Game Options features. Call vanilla exactly once when extending it. |
| Page frame, Back, and popup job boundaries | `0x958FF0`, `0x747CD0`, `0x7EF6D0`, `0x7AE040` | input ownership, page actions, modal task completion | Production only for the exact validated action/owner identities. Do not generalize action kinds or task layouts. |
| Scene-object/value bridge | `0x74B140`, `0x74AE50`, `0xD81590` | existing named object and referenced-value lifetime | Use narrowly and synchronously for proved operations such as title text. No generic GFx public API yet. |
| Movie lifecycle wrappers | `0xD7C370`, `0xD7C900`, `0xD73850` | definition cache, player registration, per-frame advance | Observation/research anchors. Do not call for custom movies until resource, render, input, thread, and teardown contracts close. |
| Raw GFx object-interface/movie slots | GetMember `+0x20`, Movie `+0xC0`, Movie `+0x118` | low-level value, advance, or event behavior only | Never expose or invoke by offset alone. Validate the concrete object and recover a complete wrapper/lifetime first. |
| Offline GFX transformation | row placements, Button label child | static presentation structure | Safe only for narrow validated structural changes; never treat it as runtime registration. |

The highest proven layer that already owns the required behavior is normally
the safest hook. Descending a layer trades inherited game behavior for more
manual ownership, focus, render, and compatibility obligations.

## Recommendations for ERNativeUI

1. Keep native page/row composition as the primary abstraction. It already
   inherits the game's ordinary focus, navigation, sound, row callbacks, and
   page teardown.
2. Generalize to other Game Options tabs only after mapping each recovered
   category record to its live panel handler, capacity, controller container,
   title target, and Back behavior. The top/category/composite split is mapped
   in the [Game Options native class map](GAME_OPTIONS_CLASS_MAP.md), but
   shared GFX widgets still do not prove shared page layouts.
3. Keep TextInput on its recovered native producer/controller/editor
   lifecycle, and finish its Unicode/IME, cancellation, focus, and teardown
   release matrix before freezing API 1.1.
4. Keep all game and Scaleform objects host-owned and opaque. Public clients
   should exchange copied strings, fixed-width values, handles, and normalized
   callbacks only.
5. Restrict scene-object operations to a live, validated movie context and a
   complete proxy cleanup scope. Do not cache raw values across frames.
6. Treat `0xD7A370`, `0xD7C900`, and related movie functions as observation
   points, not custom-view constructors. A future host-owned custom-view
   service must prove resource injection, font/images, player/render
   registration, input/focus, thread affinity, and teardown as one feature.
7. Do not advertise `Invoke`, arbitrary `SetMember`, runtime clip creation, or
   custom movie loading. None currently has a complete confirmed wrapper and
   ownership contract.
8. Use owner identity and feature-local failure for hooks. Continue explicit
   compatibility protocols for known shared detours rather than following an
   arbitrary jump chain.
9. Keep the optional GFX patch optional and structurally validated. The host
   must continue to function against the native six-row asset.
10. Preserve the no-hot-unload rule until native rows, jobs, callbacks, pages,
    proxies, and future movies can all be retired deterministically.

The phased implementation policy derived from these constraints is documented
in [UI extension strategy](UI_EXTENSION_STRATEGY.md).

## Highest-value unknown seams

The broad architecture is now concrete enough that the remaining work can be
targeted:

1. Recover the shared native row-to-`Item_N_0/Widgets` binder, including widget
   discriminator, caption/value assignment, enabled/focus state, and concrete
   row/controller types.
2. Identify the concrete owner/player type at `SceneObjProxy +0x20` and its
   relationship to `CSScaleformSwfPlayer` and `MenuWindow`.
3. Recover the concrete Back-task vtable/result states and transition writes,
   then map other action kinds without synthesizing them.
4. Map ordinary mouse/keyboard/controller actions into the row subobjects and
   establish whether any non-editor path calls the movie event interface.
5. Recover capture/render submission and the registration performed around
   system `+0x968` and `CSScaleformThreadCommandQueue`.
6. Record thread IDs and reentrancy at page construction, row callbacks,
   movie advance, dialog update/poll, and destruction.
7. Complete TextInput's remaining Unicode/IME, cancellation, focus
   restoration, concurrent programmatic-write, and destruction tests.
8. Locate real `Invoke`, SetMember, object/array creation, or event callback
   wrappers only if a concrete ERNativeUI feature requires them.

## Focused source documents

- [Scaleform/GFX presentation model](SCALEFORM_GFX_MODEL.md): static movie
  structure, widgets, optional patch, and title targets.
- [Scaleform movie loading and lifecycle](SCALEFORM_MOVIE_LIFECYCLE.md):
  resource lookup, loader states, MovieDef/player creation, advance, and
  destruction.
- [Scene-object proxy and native Scaleform bridge](SCALEFORM_NATIVE_BRIDGE.md):
  proxy/value layout, member resolution, TextField operations, and bounded
  negative results.
- [Native UI input and event dispatch](UI_EVENT_DISPATCH.md): MenuWindow frame
  flow, controllers, text-editor events, Back jobs, and popup polling.
- [Game Options native class map](GAME_OPTIONS_CLASS_MAP.md): top-dialog
  construction, fixed category storage, tab input, composite panel ownership,
  and the remaining other-tab/custom-tab boundaries.
- [Current native UI hooks and object map](CURRENT_NATIVE_UI_HOOKS.md): exact
  production detours, called interfaces, state storage, and capability status.
- [Native text input](TEXT_INPUT.md): producer/controller/dialog findings and
  the remaining validation plan.
- [UI extension strategy](UI_EXTENSION_STRATEGY.md): production priorities,
  compatibility rules, and phased implementation guidance.
- [Curated Ghidra export findings](GHIDRA_EXPORT_FINDINGS.md): dedicated
  TextInput movie names, menu layout/texture path builders, and generic GFx
  runtime landmarks that remain research leads.
