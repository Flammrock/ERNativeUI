# Native UI input and event dispatch

Status: initial static reconstruction, supplemented by the runtime behavior
already validated by ERNativeUI

This note maps the native input path around `CS::MenuWindow`, the Game Options
dialogs, native jobs, and the first confirmed native-to-Scaleform event
boundary. It is an analysis document, not a public ABI. All class names come
from validated MSVC RTTI; all other names are analytical descriptions unless
the text says otherwise.

The RVAs below apply only to the analyzed executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The evidence terms used here are:

- **Confirmed**: encoded in validated RTTI/control flow, or reproduced by a
  bounded in-game test.
- **Inferred**: strongly supported by call shape and context, but not yet
  proven as the exact native type or semantic name.
- **Hypothesis**: a concrete target for a future test.
- **Rejected**: contradicted by static structure or a controlled runtime test.

Bulk scanner output belongs under the ignored `research-work/` tree. This
document records the durable conclusions only.

## Executive model

The current evidence shows two related, but different, input routes:

```text
ordinary native menu page
    caller supplies frame float + pointer to an input-enabled byte
        -> MenuWindow virtual slot 2
        -> native row/control objects consume the local input gate
        -> page-specific virtual slot 11 updates the selected controller
        -> retained native callbacks / state changes
        -> existing Scaleform objects are updated through native bindings

TextInputDialog
    real input-enabled byte
        +-> MenuWindow slot 2 receives a forced zero byte
        |      (ordinary page movement is suppressed)
        `-> embedded CS::TextInput receives the real byte
               +-> CSEzMenuViewerPad accept/cancel predicates
               +-> copied Win32 keyboard/mouse messages
               `-> synthesized event -> movie virtual slot +0x118

Back / close action
    concrete page Back method -> MenuWindow slot 12
        -> build intrusive action/job objects
        -> attempt bounded insertion into page-owned queue at page +0x10
           (the submitted pointer is consumed whether insertion succeeds or not)
        -> a later MenuWindow slot-2 invocation calls 0x7AA1F0
        -> if the active slot is empty, promote at most one queued task
        -> update the active slot through 0x7AA480
        -> retire it after a terminal status
```

This refines several earlier working assumptions:

- **Confirmed:** the `uint8_t*` passed in `R8` to the menu frame functions is
  an input **gate**, not a raw controller or mouse event structure.
- **Confirmed:** normal Game Options navigation is primarily dispatched by
  native menu/control objects. The ordinary `MenuWindow` route does not need
  to send every action through a GFx event API.
- **Confirmed:** the dedicated text editor does synthesize keyboard and mouse
  events and passes them to its movie through a virtual call.
- **Confirmed:** the helper at `0x7AA0D0` temporarily retains an intrusive
  object, attempts bounded insertion, and consumes the caller's pointer even
  when insertion is rejected; it is not itself the synchronous Back
  dispatcher.
- **Confirmed:** the base `MenuWindow` frame advances that container through
  `0x7AA1F0`; the concrete task's update remains responsible for the actual
  transition state.

## Validated RTTI layers

The following relationships are encoded in the executable's MSVC RTTI. They
are important because similarly named UI objects do not share one layout or
one dispatch contract.

| Class | Vtable RVA | Relevant base relationship |
|---|---:|---|
| `CS::MenuWindow` | `0x2A96AE0` | `SceneObjModifier -> DLReferenceCountObject -> MenuJobRunnable -> ComponentStack` |
| `CS::MenuWindowProxy` | `0x2A97398` | small `DLNonCopyable` owner/proxy; not a `MenuWindow` |
| `CS::SceneObjProxy` | `0x2A97AF0` | derives from `ComponentProxy` |
| `CS::SceneObjStateControl` | `0x2A97E58` | separate state-control object |
| `CS::SceneObjTab` | `0x2A97EC8` | derives from `SceneObjProxy` |
| `CS::PropertyEditDialog` | `0x2B04E08` | derives through `GenericListSelectDialog` to `MenuWindow` |
| `CS::OptionSettingDialog` | `0x2B14888` | derives from `PropertyEditDialog` |
| `CS::PadSettingDialog` | `0x2B16DD8` | derives from `OptionSettingDialog` |
| `CS::SliderCtrl` | `0x2A980B0` | a specialized `MenuWindow` |
| `CS::SpinCtrl` | `0x2A98298` | a `SceneObjModifier`, not a `MenuWindow` |
| `CS::TextInput` | `0x2A98340` | a small `SceneObjProxy`, not a dialog |
| `CS::TextInputController` | `0x2B1B128` | a `PropertyController` |
| `CS::TextInputDialog` | `0x2B2B908` | a complete `MenuWindow` |
| `CS::MenuJob` | `0x2AAB700` | intrusive `DLReferenceCountObject` |
| `CS::MenuWindowJob` | `0x2AAC868` | derives from `MenuJob` |
| `CS::MenuWindowJobArray` | `0x2AAC9F8` | separate `MenuJob` aggregation type |

**Rejected:** `CS::TextInput`, `CS::TextInputController`, and
`CS::TextInputDialog` are not interchangeable names for one widget. They are
the scene proxy, property-controller, and full-dialog layers respectively.

The bounded RTTI export also fixes the exact metadata anchors. The decorated
class name begins `0x10` bytes into each MSVC type descriptor; these descriptor
RVAs must not be confused with the string RVAs printed by a raw string scan.

| Class | Type descriptor | Complete-object locator | Primary vtable |
|---|---:|---:|---:|
| `CS::MenuWindow` | `0x3C934B0` | `0x32ED4F8` | `0x2A96AE0` |
| `CS::SceneObjModifier` | `0x3C934D8` | `0x32ED1D0` | `0x2A94308` |
| `CS::SceneObjProxy` | `0x3C94568` | `0x32EE1D8` | `0x2A97AF0` |
| `CS::TextInput` | `0x3C94680` | `0x32EE4F8` | `0x2A98340` |
| `CS::TextInputController` | `0x3CD6F10` | `0x3325A08` | `0x2B1B128` |
| `CS::TextInputDialog` | `0x3CE0D30` | `0x332D208` | `0x2B2B908` |

All six locators describe a primary subobject (`object offset 0`, constructor
displacement `0`). The `TextInputDialog` table reuses `MenuWindow` slots 4
through 12 exactly, while replacing slots 0 through 3. In particular, its
slot 12 is the same `0x747CD0` native Back implementation. This is a direct
RTTI/vtable relationship, not an inference from similar control flow.

The distinction between `SliderCtrl` and `SpinCtrl` is equally material.
`SliderCtrl` overrides several `MenuWindow` methods and adds a fourteenth
virtual slot; `SpinCtrl` has only three entries and no `MenuWindow` base.
Proximity of their RTTI records is not evidence of a shared ABI.

`MenuWindowProxy` has one deleting-destructor-style virtual method at
`0x749E00`; its observed allocation size is `0xF8`. Both virtual methods of
`SceneObjProxy` (`0x74C940` and `0x74C930`) simply return `this+0x28`, and
`SceneObjTab` reuses them. `SceneObjStateControl` has a deleting-destructor
entry at `0x74E100`. These are useful ownership/accessor facts, but none is an
input dispatcher by itself.

## `CS::MenuWindow` virtual surface

The `MenuWindow` vtable at `0x2A96AE0` has 13 entries. The table below records
observable effects without inventing source-level method names.

| Slot | RVA | Confirmed narrow effect | Semantic confidence |
|---:|---:|---|---|
| 0 | `0x735100` | Calls virtual slot 1 with a zero flag and releases the allocation through an allocator interface. | lifetime/release effect confirmed; exact source declaration unknown |
| 1 | `0x744F60` | Runs cleanup and conditionally frees a `0xA38` allocation according to the deleting-destructor flag. | deleting-destructor pattern |
| 2 | `0x7463C0` | Main per-frame native row/control dispatcher. Receives `(this, float, uint8_t*)`. | confirmed |
| 3 | `0x746000` | Writes zero to caller-supplied output storage and returns it. | effect confirmed; descriptor meaning unknown |
| 4 | `0x745150` | Iterates the row range at `+0x1F8..+0x200`, stride `0x140`, and emits row-derived state into caller storage. | effect confirmed; operation name unknown |
| 5 | `0x7455E0` | Calls virtual slot 10 and returns its logical inverse. | confirmed |
| 6 | `0x745D80` | Iterates the same `0x140`-stride row range and builds another row-derived result. | effect confirmed; operation name unknown |
| 7 | `0x746A20` | Checks state at `+0x350`, builds temporary data, then calls virtual slots 4 and 9. | likely refresh/rebind; not named |
| 8 | `0x7469C0` | Assigns UTF-16 text through the object at page `+0x2F8`. | confirmed specialized setter |
| 9 | `0x746880` | Builds native text data, then calls `ScaleformValue_SetTextW` through state rooted at `+0x358`. | confirmed narrow effect |
| 10 | `0x735150` | Returns whether the pointer at page `+0x98` is non-null. | confirmed |
| 11 | `0x746820` | Packages the frame float and dispatches to the subobject at page `+0x50` through `0x734D70`. | confirmed base update effect |
| 12 | `0x747CD0` | Builds and submits the page action used by native Back/close. | runtime and static behavior confirmed |

Only slots 2, 11, and 12 are presently suitable anchors for the input-flow
model. The other effects are retained because they prevent future analysis
from assigning a plausible but unsupported name to a slot.

### Main frame/input dispatcher: slot 2

`0x7463C0` receives the page in `RCX`, a float in `XMM1`, and a pointer to a
byte in `R8`. The float is consistently propagated as per-frame timing data;
calling it delta time remains an **inference** until its producer is mapped.

The following control flow is **confirmed**:

1. It updates the subobject at page `+0x120` through `0x74D070`.
2. It copies `*R8` into a local input byte, except that page state
   `page+0x3B0 != 0` forces the local byte to zero.
3. It performs additional page-state checks rooted at `+0x1E8` and `+0x10`,
   then calls virtual slots 10 and 7.
4. It iterates `[page+0x1F8, page+0x200)` in `0x140`-byte row records. A
   subobject at row `+0x38` receives the local input-byte pointer through its
   virtual slot 2. When a second subobject at row `+0x138` reports active, the
   object at row `+0xF8` is invoked and the local input byte is cleared.
5. It updates FE-manager state, advances the page-owned task queue at `+0x10`
   through `0x7AA1F0`, and updates the independent object at `+0xA28` through
   `0x7AA480`.
6. It calls the page's virtual slot 11 with the frame float and local gate.
7. It iterates another collection at `+0x218..+0x220`, stride `0xA0`, and
   invokes a subobject at each entry's `+0x98`.

This establishes a native row/controller pipeline. It does not yet identify
the exact concrete types at row `+0x38`, `+0xF8`, and `+0x138`, nor the raw
device state from which those controllers derive their actions.

The bounded call graph finds `0x7463C0` as a direct target from many unrelated
menu-window specializations, including `TextInputDialog::slot2`. That breadth
corroborates its role as the shared base frame dispatcher, and is also why a
global detour here would have a substantially larger compatibility surface
than ERNativeUI's current page-specific `0x958FF0` gate.

### Game Options specialization: slot 11

`OptionSettingDialog` and `PadSettingDialog` use `0x9217C0` in slot 2; that
function is a direct jump to the base dispatcher at `0x7463C0`. They therefore
share the main `MenuWindow` row pass.

Their slot 11 is `0x958FF0`. After page-specific preliminary work it reaches
`0x976D40`, also used by `PropertyEditDialog`. `0x976D40`:

- reads the selected index/control state at page `+0xA38`;
- works with a controller collection rooted at page `+0x1260`;
- calls the base `MenuWindow` slot-11 implementation at `0x746820`;
- iterates the controller collection and calls `0x86C260`, enabling only the
  selected eligible entry; and
- calls `0x9778B0` when the selected index changes.

This split explains why gating only one apparent settings-page routine can
leave another input path alive: slot 2 performs the inherited row pass, while
slot 11 performs selected-controller work later in the same frame.

**Rejected by runtime test:** `OptionSettingDialog` slot 4 at `0x9536F0` is
not the sole navigation/input authority. Suppressing it did not stop page
movement.

## Concrete slider and spin-control dispatch

The control RTTI leads to two concrete mixed-device update paths. These do not
yet prove every row's type, but they show how native values, cursor state, pad
predicates, Scaleform hit testing, and retained callbacks meet.

### `SliderCtrl` virtual slot 2

`SliderCtrl` overrides `MenuWindow` slot 2 with `0x74F3D0`. It receives the
input/context object in `R8` and:

- queries the `CSEzMenuViewerPad`-predicate wrapper at `0x7598C0`;
- when cursor input is available through `0x758EA0`, obtains coordinates via
  `0x758940` and stores them at control `+0xD48`;
- calls `0x74F5D0` to derive a value delta from the current input and control
  state;
- clamps the current integer at `+0xD28` between bounds at `+0xD30` and
  `+0xD34`, using the step at `+0xD38`;
- when the value changes, calls its additional virtual slot 13 and invokes a
  retained callback object at `+0xD88` through that object's virtual slot 2.

The cursor and pad branches are both native. The GFX Slider sprite is the
presentation target, not the sole owner of slider behavior.

### `SpinCtrl` virtual slot 2

`SpinCtrl` virtual slot 2 at `0x750210` is its main input/update method. It:

- keeps the current integer at `+0x1B8`, bounds at `+0x1B0/+0x1B4`, and step
  values at `+0x1BC/+0x1C0`;
- queries a `CSEzMenuViewerPad` predicate through `0x759800`;
- obtains cursor coordinates through `0x758940` and tests them against two
  scene/value objects at `+0xD0` and `+0x170` through `0xD81C80`;
- queries additional named-action wrappers and two signed directional/axis
  samples through `0x758B70` and `0x758A90`;
- clamps/wraps the resulting value according to control flags; and
- invokes the retained object at `+0x200` through virtual slot 2 when the
  value changes, then updates state controls at `+0x70` and `+0x110`.

The call shape of `0xD81C80` and its use with cursor coordinates make a
Scaleform/display-object hit test a **strong inference**; its exact native name
and coordinate-space contract remain unknown. The surrounding value changes
and callbacks are confirmed.

## Controller/pad dispatch

Several small wrappers near `0x759000` construct erased
`std::function<bool(CS::CSEzMenuViewerPad const&)>` predicates. This signature
is **confirmed** by their validated RTTI. The wrappers then call the common
helper at `0x75E7C0`.

In the embedded text editor:

- `0x759920` is checked first; when true, editor state `+0x60` becomes `1` and
  the edit session ends;
- `0x759050` is checked second; when true, editor state `+0x60` becomes `0` and
  the edit session ends.

The surrounding `TextInputDialog` treats state `1` as commit and the other
completed state as cancel. Therefore their **contextual roles** are confirmed
as pad commit and pad cancel. The physical button mapping and the individual
lambda semantics are still unknown.

Other wrappers (`0x759860`, `0x7598C0`, and related helpers) appear in
`TextInputController::slot2` at `0x978260`. Their RTTI proves that the erased
predicates consume `CSEzMenuViewerPad`; it does not yet prove the action
assigned to each predicate.

For ordinary Game Options rows, controller navigation is visibly functional
and reaches the native row/control pass described above. The exact path from
XInput/DirectInput state to `CSEzMenuViewerPad`, and from that abstraction to
each row subobject, remains **unknown**.

## Dedicated text-input dispatch

### Three native layers

Static control flow provides a useful initial separation:

| Layer | Confirmed role |
|---|---|
| `CS::TextInput` | Scene-object proxy embedded in the full text dialog; exposes the live movie/value owner through its small virtual interface. |
| `CS::TextInputController` | Property-layer controller. Its constructor resolves existing names `Text_0`, `GrayoutItem`, and `TextOnEmpty`; its update path uses the existing text setter at `0x74AE50`. |
| `CS::TextInputDialog` | Full `MenuWindow` that owns edit focus, completion state, and commit/cancel routing. |

The strings `Widgets/TextInput`, `Widgets/TextInput/Text_0`, and
`Widgets/TextInput/Input` remain construction anchors. The three short names
above are additional confirmed controller-relative path components; they do
not imply that every path is rooted at the same display object.

### Exclusive focus in `TextInputDialog::slot2`

`TextInputDialog` overrides virtual slot 2 with `0x9BA0A0`. Its essential
sequence is **confirmed**:

1. Call base `MenuWindow` slot 2 (`0x7463C0`) with a local byte that is always
   zero, not with the caller's real input gate.
2. If the closing byte at dialog `+0xBB8` is set, stop.
3. Update the embedded `CS::TextInput` at dialog `+0xA98`.
4. Pass the real caller input gate to the embedded editor update at
   `0x7507B0`.
5. When the editor completes, set the closing byte. State `1` invokes the
   retained callback at dialog `+0xBB0` with the edited string and submits a
   packed action of kind `2`; cancellation submits kind `3`.

This is a concrete native example of correct modal focus ownership: the base
page still advances, but it sees input disabled; only the editor receives the
real gate.

### Windows message snapshot and movie event call

The logical editor update beginning at `0x7507B0` is split across multiple
PE unwind regions, but direct branches join those regions into one operation.
It first processes the pad commit/cancel predicates described above. It then
obtains an engine-owned message snapshot through `0xD6C870` and iterates
`0x20`-byte records. The fields used in each record match the first four
fields of a Win32 `MSG`: window handle, message, `wParam`, and `lParam`.

The following message handling is **confirmed by comparisons to Win32 message
constants**:

| Message | Native handling |
|---|---|
| `0x0100` `WM_KEYDOWN` | Enter commits, Escape cancels; other keys go through `0x750C50` as key-down. |
| `0x0101` `WM_KEYUP` | Goes through `0x750C50` as key-up. |
| `0x0102` `WM_CHAR` | Builds an event record with native kind `0x1D`. |
| `0x0104` `WM_SYSKEYDOWN` | Same key-down path as `WM_KEYDOWN`. |
| `0x0105` `WM_SYSKEYUP` | Same key-up path as `WM_KEYUP`. |
| `0x0200` `WM_MOUSEMOVE` | Adjusts coordinates and builds native event kind `1`. |
| `0x0201` `WM_LBUTTONDOWN` | Adjusts coordinates, performs a movie-side test through `0xD81C80`, then builds native event kind `2` when accepted. |
| `0x0202` `WM_LBUTTONUP` | Adjusts coordinates and builds native event kind `3`. |

`0x750C50` maps common virtual keys, calls `GetKeyboardState` and `ToAscii`,
collects Shift/Control/Alt and lock-key state through `GetKeyState` in
`0x7506D0`, and builds the keyboard event record.

All accepted keyboard, character, and mouse records are passed to the same
virtual call at movie-interface offset `+0x118`. The exact concrete movie type
and virtual-slot declaration are not yet recovered.

- **Confirmed:** this is a native-to-movie event boundary used by the text
  editor.
- **Strongly inferred:** its public Scaleform analogue is
  `GFx::Movie::HandleEvent`, because it accepts the synthesized keyboard and
  mouse event records described above.
- **Unknown:** whether other menus call the same slot, whether the return value
  is a `GFx::Event::EventResult`, and which wrapper owns the movie pointer.

Static import cross-references also show that the text-input cluster is the
only game-side caller of `ToAscii`, six of the executable's seven direct
`GetKeyState` callsites are in its modifier helper, and one of two
`GetKeyboardState` callsites is in its key translator. `PeekMessageW`,
`TranslateMessage`, and `DispatchMessageW` belong to separate global
message-pump functions. Thus `CS::TextInput` consumes an engine-copied message
snapshot; it does not run its own Windows message pump.

No active IME composition path has been established. The executable imports
`ImmDisableIME` but not the common IMM composition/context functions. Compiled
GFx IME support and Steam software-keyboard RTTI remain leads, not proof that
this dialog uses either path.

## Mouse and keyboard outside text entry

Mouse use is confirmed behaviorally for Game Options rows, and the
`SliderCtrl`/`SpinCtrl` methods above recover concrete cursor-hit/value-change
slices. Keyboard navigation exists at the game level. The complete ordinary
page-to-device paths are still not resolved. In particular:

- the `R8` byte passed to `MenuWindow::slot2` cannot contain coordinates or a
  key code;
- the ordinary row pass invokes native controller subobjects whose concrete
  type assignment per row is still unknown;
- the explicit Windows-message-to-movie path above is inside the dedicated
  editor and must not be generalized to every menu; and
- no direct `GFx::Movie::HandleEvent` call has yet been proved in the ordinary
  `OptionSettingDialog` frame path.

A likely architecture is that the engine window layer and pad manager update
global snapshots, while native row/control objects query higher-level actions
and cursor state. This is an **inference**, not a recovered call chain.

## Back, close, and the page-owned job container

### Concrete page Back method

`OptionSettingDialog` virtual slot 13 is `0x9580C0`. If page `+0x1D60` is
non-null it calls that object's virtual slot 2. Otherwise it constructs the
packed pair `{kind=3, flags=0}` through the leaf helper at `0x7AA060`, then
calls the page's virtual slot 12 at `0x747CD0`.

This statically connects the concrete settings-page Back method to the same
slot used by ERNativeUI's synthetic Previous action.

### `MenuWindow` slot 12 and asynchronous submission

`0x747CD0`:

- checks the guard at page `+0x3B0`;
- distinguishes at least action kind `2` through the leaf predicate at
  `0x7AA090`;
- calls virtual slot 3 and additional page/Scaleform helpers;
- constructs several erased callback and intrusive task/action objects;
- calls `0x747850`; and
- sets page `+0x3B0` to `1`.

`0x747850` performs additional page and Scaleform-state work, builds an
aggregate action, then calls `0x7AA0D0` with `RCX=page+0x10` and a pointer to
an intrusive object in `RDX`.

`0x7AA0D0` is now statically characterized:

1. When the supplied pointer is non-null, retain a temporary reference through
   `0x1EBBFC0` and pass it to `0x7A96A0` for the queue rooted at `RCX+0x08`.
2. `0x7A96A0` inserts only when its optional maximum at queue `+0x30` is zero,
   or the size at queue `+0x28` is below that maximum. Successful insertion
   through `0x7A9D30` stores its own retained reference and increments the
   size. The helper consumes and clears its temporary in both cases.
3. `0x7AA0D0` then releases the caller's original reference through
   `0x1EBC000` and clears the caller's pointer regardless of insertion success.
4. A release whose previous count was one dispatches virtual slot 0 to destroy
   the object. Consequently, a full bounded queue can reject and destroy the
   submitted object while `0x7AA0D0` still returns normally.

The observed aggregate and embedded-queue fields are:

| Aggregate offset | Queue offset | Recovered role |
|---:|---:|---|
| `+0x00` | - | active intrusive-task pointer |
| `+0x08` | `+0x00` | embedded segmented-deque object |
| `+0x18` | `+0x10` | block-map pointer |
| `+0x20` | `+0x18` | block-map size |
| `+0x28` | `+0x20` | starting offset |
| `+0x30` | `+0x28` | queued size/count |
| `+0x38` | `+0x30` | optional maximum; zero means unbounded |

The map, offset, and size roles follow the push arithmetic in `0x7A9D30`; the
aggregate offsets are therefore exact for the owner passed as `page+0x10`.

There are many callers of `0x7AA0D0`, so it is a generic intrusive-container
operation. The durable analytical description for page `+0x10` is a
**page-owned intrusive action/job queue**, not a fully typed event bus.

The consumer is now statically connected to the base frame pass:

1. Every invocation of `MenuWindow` slot 2 at `0x7463C0` calls `0x7AA1F0`
   once with `RCX=page+0x10`.
2. When the active pointer at subobject offset `+0x00` is null and the queued
   count at `+0x30` is nonzero, `0x7AA1F0` removes the front entry from the
   segmented queue rooted at `+0x08` through `0x7A9600`.
3. `0x7A9600` explicitly retains the front pointer before removing the
   queue-owned entry. Consuming assignment moves that retained result into the
   active slot.
4. `0x7AA480` is a generic intrusive-task-slot updater: it retains the current
   task, invokes virtual slot 2 with the frame-time wrapper, and treats a first
   status DWORD greater than `1` as terminal. It clears/releases the owner slot
   only if that slot still contains the same task, then releases its temporary
   retain. `MenuWindow` also calls it directly for the independent slot at
   page `+0xA28`, and many unrelated callers use the same helper.

If another task is already active, a newly submitted task remains queued;
there is no guarantee that it is promoted on the immediately following slot-2
invocation. `0x7AA1F0` still updates the existing active task on that call.

The numeric result states, concrete Back-task type, and the exact write that
commits the visual fade/pop remain unmapped. This is still not a safe generic
event API: callers must know the concrete task factory, payload, and owner.

**Rejected:** `0x7AA0D0` does not synchronously perform the visual page pop.
The fade/transition occurs during later active-task updates.

## Popup dialogs and `MenuWindowJob`

The generic alert path uses a second native lifecycle:

```text
descriptor + builder kind
    -> build intrusive MenuWindowJob
    -> consume into CSPopupMenu blocking slot
    -> CSPopupMenu_Update owns the popup input gate
    -> MenuWindowJob virtual slot 2 polls scheduler/result state
    -> response kind 2 (primary) or 3 (secondary)
    -> retire native job
    -> ERNativeUI host worker later invokes the client callback
```

`CS::MenuWindowJob` has vtable entries `0x745BE0`, `0x7AD6D0`, and
`0x7AE040`; the last is the production-observed poll method. Its first
scheduler-state `uint32_t` has been live-validated for result kinds `2` and
`3`. The complete scheduler-state type remains unknown.

RTTI also identifies `CS::MenuWindowJobArray`, with vtable entries
`0x745BE0`, `0x7AEDF0`, and `0x7AEF00`. It is a separate `MenuJob` aggregation
type. No current evidence shows that the single generic alert job is an array,
so the two poll contracts must not be conflated.

This job poll is not a raw controller dispatcher. It is a higher-level
completion boundary after the owned popup has processed input. See
[`NATIVE_DIALOGS.md`](../NATIVE_DIALOGS.md) for the production transport and
focus state machine.

## Native/Scaleform boundary: what is and is not proved

| Direction | Boundary | Status |
|---|---|---|
| native -> Scaleform | `Scaleform_ResolvePath` at `0x74B140` resolves an existing named object into a game wrapper. | confirmed |
| native -> Scaleform | `ScaleformValue_SetTextW` at `0x74AE50` changes UTF-16 text on an already resolved value. | confirmed |
| native -> Scaleform | text editor call through movie-interface offset `+0x118` receives synthesized key, character, and mouse records. | call and records confirmed; `GFx::Movie::HandleEvent` identity strongly inferred |
| native -> Scaleform | editor helper `0xD858B0` changes edit/movie state on commit or cancel. | call confirmed; exact method unknown |
| Scaleform -> native | `CS::CSScaleformFsCommandHandler` RTTI and vtable exist. | confirmed static structure |
| Scaleform -> native | `CSScaleformFsCommandHandler` slot 1 at `0xD6D790` is a one-instruction `ret`. | confirmed no-op implementation for this class |

The no-op FSCommand slot rejects the idea that this concrete handler is the
ordinary Game Options input dispatcher. Another derived handler or another
movie may still implement FSCommand. No menu-specific FSCommand use was found
in `02_040_optionsetting.gfx`.

Nothing in the current evidence proves safe arbitrary ActionScript invocation,
display-object creation, listener registration, or custom movie loading. The
working path resolver, text setter, and text-editor event call are three
specific boundaries with different ownership contracts. The proxy/value side
of that contract is mapped in
[`SCALEFORM_NATIVE_BRIDGE.md`](SCALEFORM_NATIVE_BRIDGE.md).

## Lifetime and thread context

### Confirmed lifetime facts

- `MenuWindow`, `MenuJob`, and `MenuWindowJob` all derive from
  `DLReferenceCountObject` according to RTTI.
- Submitted page actions are intrusive references. The observed retain helper
  is `0x1EBBFC0`; release is `0x1EBC000`; final release reaches virtual slot 0.
- `TextInputDialog` owns its `CS::TextInput` inline at `+0xA98`, keeps the
  retained completion callback near `+0xBB0`, and has a closing byte at
  `+0xBB8`.
- Scaleform path results contain referenced values and require their validated
  destructor. A movie/value pointer observed in one frame must not be retained
  across teardown without proving its owner and reference protocol.
- ERNativeUI action-row callbacks run synchronously when the game's retained
  native callable invokes them. Value-change and alert-completion callbacks are
  deliberately transferred to ERNativeUI's host worker.

### Thread status

The static executable establishes call order, but not operating-system thread
identity. It is reasonable to infer that the page frame pass, row callbacks,
text editor, and Scaleform event call normally occur on one UI/game-update
thread; that inference must not become a public guarantee.

The host worker's 50 ms poll is a known separate ERNativeUI context. Client
callbacks dispatched there must not call unproven native UI functions as if
they were inside the menu frame. Hot unload is unsupported because native
pages, callbacks, jobs, and Scaleform values can outlive the client call that
created them.

## Bounded runtime probes still needed

Every probe below should verify the exact executable identity first, remain
observation-only, cap records by both count and time, and automatically disable
itself on any pointer/vtable mismatch. Do not log entered text.

### 1. Thread and reentrancy census

Record thread ID, recursion depth, object pointer, vtable, and return address
at these already characterized boundaries:

- `MenuWindow` slot 2 (`0x7463C0`);
- `OptionSettingDialog` slot 11 (`0x958FF0`);
- Back slot 12 (`0x747CD0`);
- intrusive submission/attempted insertion (`0x7AA0D0`);
- text editor update (`0x7507B0`);
- path resolution/text setting (`0x74B140`, `0x74AE50`);
- `CSPopupMenu_Update` (`0x7EF6D0`); and
- `MenuWindowJob` poll (`0x7AE040`).

A useful bound is 64 state-transition records or ten seconds after opening one
test page, whichever occurs first.

### 2. Device-isolated ordinary row trace

Repeat the same single-row page three times, using only controller, only mouse,
and only keyboard. At the row-loop callsites inside `0x7463C0`, record only:

- the three row subobject pointers and their vtables;
- local input-gate value before and after each virtual call;
- selected row index before and after the frame; and
- whether a retained row callback fired.

This should identify which concrete controller consumes each device without
globally hooking every virtual method.

### 3. Text-editor event confirmation

At the callsites that invoke movie offset `+0x118`, record the movie pointer,
vtable, event kind, and return value for one key down/up, one character, one
mouse move/click, controller commit, and controller cancel. Record only the
message class, never `wParam`, character data, clipboard data, or final text.

This can confirm the `GFx::Movie::HandleEvent` inference and determine whether
the event-return value controls further native consumption.

### 4. Back task transition boundary

Assign an observation ID to the intrusive object entering `0x7AA0D0` and
record whether bounded insertion accepts it. For an accepted object, trace
that exact pointer through promotion by `0x7AA1F0`, its virtual-slot-2 update,
and final release. The consumer is known; the goal is now to identify the
concrete Back-task vtable, its result-state sequence, and the point at which
fade/pop state is committed. Never synthesize or retain an extra reference in
this probe.

### 5. Focus and teardown sequence

For one normal page, one `TextInputDialog`, and one generic alert, record only
open/update/close/destructor transitions and thread IDs. Verify that:

- the parent page receives a zero gate while the child owns focus;
- the child closes before its movie/value wrappers are destroyed;
- queued actions release exactly once; and
- the parent regains a live input gate on the following stable frame.

### 6. FSCommand negative check

Before probing FSCommand at runtime, statically enumerate derived vtables that
override the no-op slot. Probe only a concrete shipped movie known to instantiate
one of those derived handlers. A process-wide hook on the generic no-op is not
useful evidence for Game Options.

## Open questions

1. Who calls `MenuWindow` slot 2, and where is its input-enabled byte owned?
2. Which concrete row/controller types occupy `+0x38`, `+0xF8`, and `+0x138`?
3. How are XInput/DirectInput, mouse cursor state, and keyboard navigation
   reduced to native menu actions outside the text editor?
4. What is the concrete movie interface used at virtual offset `+0x118`?
5. What concrete task type is created for Back, what do its result states
   mean, and where does it commit fade/pop state?
6. Which thread advances Scaleform movies, and is it always the menu-update
   thread?
7. How are `TextInputController` and `TextInputDialog` selected/constructed by
   the property editor?
8. Does any supported path provide Unicode/IME composition beyond the observed
   `ToAscii` translation?
9. What are the safe ownership rules for invoking a movie method outside the
   existing native controller call stack?

Until those questions are answered, production work should continue to use
the highest recovered native constructor/dispatcher that already owns focus,
input, and teardown. A nearby RVA or a plausible Scaleform SDK analogue is not
enough to make a callable interface safe.
