# Native UI input and event dispatch

This page maps the native input path around `CS::MenuWindow`, ordinary
settings controls, text editing, Back jobs, and generic popup jobs. It records
specific native-to-Scaleform boundaries without claiming that Elden Ring has
one universal UI event bus.

## Reference build and evidence boundary

All RVAs and offsets refer to this exact Windows executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

Class names come from validated MSVC RTTI; other native names are analytical.
Every address and layout is build-locked, private evidence.

- **Confirmed** means validated RTTI/control flow or a bounded live test
  establishes the fact.
- **Inferred** means call shape and context agree but the exact native type or
  semantic name has not been proved.
- **Hypothesis** is a concrete test target.
- **Rejected** means static structure or a controlled test contradicted it.

Bulk output and probe logs remain local under ignored `research-work/` paths.

## End-to-end model

```text
ordinary native menu page
    frame float + pointer to input-enabled byte
        -> MenuWindow virtual slot 2
        -> native row/control objects consume a local input gate
        -> page virtual slot 11 updates the selected controller
        -> retained callbacks and native state changes
        -> native bindings update existing Scaleform objects

TextInputDialog
    real input-enabled byte
        +-> base MenuWindow slot 2 receives forced zero
        |      (ordinary page input is suppressed)
        `-> embedded CS::TextInput receives the real gate
               +-> CSEzMenuViewerPad commit/cancel predicates
               +-> copied Win32 keyboard/mouse records
               `-> synthesized event -> movie virtual slot +0x118

Back / close
    concrete page Back -> MenuWindow slot 12
        -> construct intrusive action/job
        -> consume into queue at page +0x10
        -> later base-frame update calls 0x7AA1F0
        -> promote at most one queued task when active slot is empty
        -> update active task through 0x7AA480
        -> retire after terminal status
```

Confirmed conclusions:

- The `uint8_t*` supplied in `R8` to menu-frame functions is an input gate,
  not a controller, mouse, or keyboard packet.
- Ordinary settings navigation is primarily native controller behavior; it
  need not send every action through a GFx event API.
- The dedicated text editor does synthesize keyboard/mouse events and call its
  movie.
- `0x7AA0D0` temporarily retains a task, attempts bounded insertion, and
  consumes the caller's pointer even when insertion fails. It is not a
  synchronous Back dispatcher.
- Base `MenuWindow` advances the page-owned task aggregate through
  `0x7AA1F0`; the concrete task commits the later transition.

## Relevant RTTI layers

Similarly named objects do not share one layout or dispatch contract:

| Class | Vtable RVA | Relevant relationship |
|---|---:|---|
| `CS::MenuWindow` | `0x2A96AE0` | `SceneObjModifier -> DLReferenceCountObject -> MenuJobRunnable -> ComponentStack` |
| `CS::MenuWindowProxy` | `0x2A97398` | Small `DLNonCopyable` proxy; not a `MenuWindow`. |
| `CS::SceneObjProxy` | `0x2A97AF0` | Derives from `ComponentProxy`. |
| `CS::SceneObjStateControl` | `0x2A97E58` | Separate state-control object. |
| `CS::SceneObjTab` | `0x2A97EC8` | Derives from `SceneObjProxy`. |
| `CS::PropertyEditDialog` | `0x2B04E08` | Derives through `GenericListSelectDialog` to `MenuWindow`. |
| `CS::OptionSettingDialog` | `0x2B14888` | Derives from `PropertyEditDialog`. |
| `CS::PadSettingDialog` | `0x2B16DD8` | Derives from `OptionSettingDialog`. |
| `CS::SliderCtrl` | `0x2A980B0` | Specialized `MenuWindow`. |
| `CS::SpinCtrl` | `0x2A98298` | `SceneObjModifier`, not a `MenuWindow`. |
| `CS::TextInput` | `0x2A98340` | Small `SceneObjProxy`, not a dialog. |
| `CS::TextInputController` | `0x2B1B128` | `PropertyController`. |
| `CS::TextInputDialog` | `0x2B2B908` | Complete `MenuWindow`. |
| `CS::MenuJob` | `0x2AAB700` | Intrusive `DLReferenceCountObject`. |
| `CS::MenuWindowJob` | `0x2AAC868` | Derives from `MenuJob`. |
| `CS::MenuWindowJobArray` | `0x2AAC9F8` | Separate `MenuJob` aggregation type. |

`TextInput`, `TextInputController`, and `TextInputDialog` are therefore the
scene proxy, property controller, and full focus-owning dialog respectively.
They are not interchangeable spellings of one widget. See the
[full RTTI reference](../reference/native-ui-class-hierarchy.md).

## `CS::MenuWindow` virtual surface

The primary vtable at `0x2A96AE0` has 13 entries. Effects are named only as
narrowly as the evidence permits.

| Slot | RVA | Confirmed narrow effect |
|---:|---:|---|
| 0 | `0x735100` | Calls slot 1 with zero and releases through an allocator interface. |
| 1 | `0x744F60` | Cleanup and conditional free of a `0xA38` allocation; deleting-destructor pattern. |
| 2 | `0x7463C0` | Main per-frame row/control dispatcher, `(this, float, uint8_t*)`. |
| 3 | `0x746000` | Writes zero into caller output and returns it; descriptor meaning unresolved. |
| 4 | `0x745150` | Iterates `+0x1F8..+0x200`, stride `0x140`, and emits row-derived state. |
| 5 | `0x7455E0` | Calls slot 10 and returns its logical inverse. |
| 6 | `0x745D80` | Iterates the same rows and builds another row-derived result. |
| 7 | `0x746A20` | Checks `+0x350`, builds temporary data, calls slots 4 and 9; likely refresh/rebind. |
| 8 | `0x7469C0` | Assigns UTF-16 text through the object at `+0x2F8`. |
| 9 | `0x746880` | Builds native text data and calls the text setter through state at `+0x358`. |
| 10 | `0x735150` | Tests whether pointer `+0x98` is non-null. |
| 11 | `0x746820` | Packages frame float and dispatches subobject `+0x50` through `0x734D70`. |
| 12 | `0x747CD0` | Builds and submits the native page action used by Back/close. |

Only slots 2, 11, and 12 have roles strong enough for this dispatch model.
The narrower effects remain recorded to prevent plausible but unsupported
renaming.

### Base frame dispatcher

RVA `0x7463C0` receives page in `RCX`, a float in `XMM1`, and an input-byte
pointer in `R8`. Calling the float delta time remains inferred until its
producer is mapped. Its control flow is confirmed:

1. update page subobject `+0x120` through `0x74D070`;
2. copy `*R8` to a local gate, except page `+0x3B0 != 0` forces zero;
3. perform state checks at `+0x1E8` and `+0x10`, then call slots 10 and 7;
4. iterate `+0x1F8..+0x200` in `0x140`-byte records, call row `+0x38` slot 2,
   and, when row `+0x138` is active, invoke row `+0xF8` and clear the gate;
5. update frontend state, page task aggregate `+0x10` through `0x7AA1F0`,
   and independent task slot `+0xA28` through `0x7AA480`;
6. call page slot 11 with the frame float and local gate; and
7. iterate `+0x218..+0x220` in `0xA0`-byte entries and invoke each `+0x98`
   subobject.

The exact controller types at row offsets `+0x38`, `+0xF8`, and `+0x138`
remain unresolved. Because many unrelated menu families call `0x7463C0`, a
global detour has a much larger compatibility surface than a page-specific
hook.

### OptionSetting specialization

`OptionSettingDialog` and `PadSettingDialog` slot 2 is thunk `0x9217C0`, which
jumps to `0x7463C0`. Their slot 11 is `0x958FF0`, which reaches
`0x976D40`. The latter:

- reads selection/controller state at page `+0xA38`;
- accesses the controller collection at `+0x1260`;
- calls base slot 11 at `0x746820`;
- iterates controllers and calls `0x86C260`, enabling only the selected
  eligible entry; and
- calls `0x9778B0` when selection changes.

The split explains why suppressing only one plausible routine may leave input
alive: slot 2 owns the inherited row pass and slot 11 owns selected-controller
work. **Rejected:** slot 4 at `0x9536F0` is the sole navigation authority;
suppressing it did not stop movement.

## Concrete value controls

### `SliderCtrl`

Slot 2 at `0x74F3D0`:

- queries a `CSEzMenuViewerPad` predicate through `0x7598C0`;
- obtains cursor coordinates through `0x758EA0` and `0x758940`, storing them
  at `+0xD48`;
- derives a delta with `0x74F5D0`;
- clamps integer `+0xD28` between `+0xD30/+0xD34` using step `+0xD38`; and
- on change, invokes added slot 13 and retained callback `+0xD88` slot 2.

Both cursor and pad branches are native. The GFX Slider is presentation, not
the sole behavior owner.

### `SpinCtrl`

Slot 2 at `0x750210`:

- stores current integer at `+0x1B8`, bounds at `+0x1B0/+0x1B4`, and steps at
  `+0x1BC/+0x1C0`;
- queries pad predicate `0x759800`;
- obtains cursor coordinates through `0x758940` and tests scene values at
  `+0xD0/+0x170` through `0xD81C80`;
- samples directional/axis state through `0x758B70` and `0x758A90`;
- clamps or wraps according to flags; and
- on change, invokes retained object `+0x200` slot 2 and updates state controls
  at `+0x70/+0x110`.

Use of `0xD81C80` as a display-object hit test is inferred from call shape and
coordinates; its exact native name and coordinate space are unresolved.

## Controller predicate layer

Wrappers near `0x759000` construct erased
`std::function<bool(CS::CSEzMenuViewerPad const&)>` predicates, as confirmed by
RTTI, then use helper `0x75E7C0`.

Inside the text editor, `0x759920` ends editing with state `1`, while
`0x759050` ends it with state `0`. The surrounding dialog commits state `1`
and cancels the other completed state, confirming their contextual commit and
cancel roles. Their exact physical-button mapping remains unknown. Other
predicate wrappers in `TextInputController::slot2` are not named solely from
their shared callable type.

## Dedicated text-input event path

### Ownership and exclusive focus

| Layer | Confirmed role |
|---|---|
| `CS::TextInput` | Scene proxy embedded in the full dialog; exposes the live movie/value owner. |
| `CS::TextInputController` | Property controller; resolves `Text_0`, `GrayoutItem`, and `TextOnEmpty`, and refreshes text through `0x74AE50`. |
| `CS::TextInputDialog` | Full `MenuWindow` owning edit focus, completion, and commit/cancel routing. |

`TextInputDialog` slot 2 at `0x9BA0A0`:

1. calls base `MenuWindow` slot 2 with a forced-zero local gate;
2. stops when closing byte `+0xBB8` is already set;
3. updates embedded `CS::TextInput` at `+0xA98`;
4. sends the real caller gate to editor update `0x7507B0`; and
5. on completion, sets the closing byte. State `1` calls retained callback
   `+0xBB0` with edited text and submits packed action kind `2`; cancellation
   submits kind `3`.

The parent frame continues with input disabled while only the editor receives
the real gate. This is the confirmed native exclusive-focus pattern used by
ERNativeUI TextInput.

### Copied Win32 messages

Editor update `0x7507B0` receives an engine-owned snapshot from `0xD6C870`
and iterates `0x20`-byte records whose leading fields match Win32 `MSG`.

| Message | Confirmed handling |
|---|---|
| `0x0100` `WM_KEYDOWN` | Enter commits, Escape cancels; other keys use `0x750C50` as key-down. |
| `0x0101` `WM_KEYUP` | Uses `0x750C50` as key-up. |
| `0x0102` `WM_CHAR` | Builds native event kind `0x1D`. |
| `0x0104` `WM_SYSKEYDOWN` | Same path as `WM_KEYDOWN`. |
| `0x0105` `WM_SYSKEYUP` | Same path as `WM_KEYUP`. |
| `0x0200` `WM_MOUSEMOVE` | Adjusts coordinates and builds event kind `1`. |
| `0x0201` `WM_LBUTTONDOWN` | Adjusts coordinates, tests through `0xD81C80`, then builds kind `2` when accepted. |
| `0x0202` `WM_LBUTTONUP` | Adjusts coordinates and builds kind `3`. |

`0x750C50` maps virtual keys, calls `GetKeyboardState` and `ToAscii`, gathers
modifier/lock state through `GetKeyState` in `0x7506D0`, and builds a keyboard
event. Accepted keyboard, character, and mouse records reach movie-interface
virtual offset `+0x118`.

- The native-to-movie call and event records are **Confirmed**.
- Its correspondence to `GFx::Movie::HandleEvent` is **Inferred**.
- The concrete interface, return type, and use by other menus remain
  **Unresolved**.

The text-input cluster consumes a copied snapshot; it does not run the global
Windows message pump. `PeekMessageW`, `TranslateMessage`, and
`DispatchMessageW` belong to separate functions. No active IME composition
path has been established; compiled GFx IME support and software-keyboard RTTI
are leads only.

Outside editing, the complete ordinary keyboard/mouse/controller route is
still not recovered as one reusable dispatcher. API 1.1 Input Bindings uses a
separate confirmed global physical-state path described in the
[input-bindings case study](../case-studies/input-bindings.md).

## Back and the page-owned task aggregate

`OptionSettingDialog` slot 13 at `0x9580C0` first delegates to object
`+0x1D60` when present. Otherwise it constructs packed pair `{3, 0}` through
`0x7AA060` and calls page slot 12 at `0x747CD0`.

Slot 12 checks guard `+0x3B0`, distinguishes at least action kind `2` through
`0x7AA090`, constructs callbacks/tasks, calls `0x747850`, and sets the guard.
`0x747850` builds an aggregate action and calls `0x7AA0D0` with
`RCX=page+0x10`.

### Submission consumes the pointer

`0x7AA0D0` is confirmed to:

1. retain a temporary through `0x1EBBFC0` and offer it to queue insertion
   `0x7A96A0` at aggregate `+0x08`;
2. insert through `0x7A9D30` only if optional maximum `+0x30` is zero or
   queued size `+0x28` is below it;
3. consume and clear its temporary regardless of insertion; and
4. release and clear the caller's original pointer through `0x1EBC000`
   regardless of insertion success.

A full bounded queue can therefore reject and destroy the submitted task while
the function returns normally.

| Aggregate offset | Embedded queue offset | Recovered role |
|---:|---:|---|
| `+0x00` | - | Active intrusive-task pointer |
| `+0x08` | `+0x00` | Segmented-deque object |
| `+0x18` | `+0x10` | Block-map pointer |
| `+0x20` | `+0x18` | Block-map size |
| `+0x28` | `+0x20` | Starting offset |
| `+0x30` | `+0x28` | Queued size/count |
| `+0x38` | `+0x30` | Optional maximum; zero means unbounded |

The durable description of page `+0x10` is a page-owned intrusive action/job
queue, not a fully typed event bus.

### Promotion and update

Every base slot-2 call invokes `0x7AA1F0` with `page+0x10`. When active
`+0x00` is null and queued count `+0x30` is nonzero, it removes the front entry
through `0x7A9600`, which retains the pointer before dropping queue ownership.
That retained result becomes the active task.

`0x7AA480` retains the active task, calls virtual slot 2 with the frame-time
wrapper, and treats a first status `uint32_t > 1` as terminal. It clears the
owner slot only if it still contains the same task, then releases the temporary
retain. The same helper updates independent slot page `+0xA28` and many
unrelated owners.

If another task is active, a newly submitted task remains queued. Numeric
states, the concrete Back-task type, and the exact fade/pop commit write are
unresolved. **Rejected:** `0x7AA0D0` synchronously performs Back; the visible
transition occurs during later task updates.

## Popup job boundary

Generic alerts use a separate owner slot and a `MenuWindowJob`:

```text
descriptor + builder kind
    -> intrusive MenuWindowJob
    -> consume into CSPopupMenu blocking slot
    -> popup update owns input
    -> MenuWindowJob slot 2 polls result
    -> result kind 2 (primary) or 3 (secondary)
    -> retire job
    -> host worker invokes client completion later
```

`MenuWindowJob` has vtable entries `0x745BE0`, `0x7AD6D0`, and `0x7AE040`;
the last is the observed poll. `MenuWindowJobArray` instead has entries
`0x745BE0`, `0x7AEDF0`, and `0x7AEF00`. It is a separate aggregation class and
must not be assigned the single-alert poll contract. See the
[native-dialog case study](../case-studies/native-dialogs.md).

## Confirmed Scaleform boundaries

| Direction | Boundary | Evidence |
|---|---|---|
| Native to Scaleform | Resolver `0x74B140` obtains an existing named object. | Confirmed |
| Native to Scaleform | Setter `0x74AE50` writes UTF-16 text to a resolved value. | Confirmed |
| Native to Scaleform | Text editor invokes movie virtual `+0x118` with synthesized input records. | Call/records confirmed; HandleEvent name inferred |
| Native to Scaleform | `0xD858B0` changes editor/movie state on commit or cancel. | Call confirmed; exact method unresolved |
| Scaleform to native | `CSScaleformFsCommandHandler` class and state installation exist. | Confirmed |
| Scaleform to native | Installed handler slot at `0xD6D790` returns immediately. | Confirmed no-op |

No evidence here proves arbitrary ActionScript invocation, display-object
creation, listener registration, or custom movie loading. See the
[scene-object bridge](../reference/scene-object-bridge.md) and
[movie lifecycle](scaleform-movie-lifecycle.md) for those boundaries.

## Lifetime and threading

- `MenuWindow`, `MenuJob`, and `MenuWindowJob` use intrusive reference
  ownership.
- Observed retain/release helpers are `0x1EBBFC0` and `0x1EBC000`; final
  release invokes virtual slot 0.
- `TextInputDialog` owns `CS::TextInput` at `+0xA98`, completion callback near
  `+0xBB0`, and closing byte `+0xBB8`.
- Scaleform path results contain referenced values and require the matching
  destructor.
- Native action callables execute synchronously on the game path; ERNativeUI
  transfers public value and completion callbacks to its worker.

Static call order does not establish operating-system thread identity. It is
reasonable but not guaranteed that page frame, row actions, editor dispatch,
and movie event handling normally share one game/UI update thread. Client
callbacks on the host worker must not call private native functions as if they
were inside a page frame. Hot unload remains unsupported.

## Reproduction probes

Every probe must validate the exact executable, be observation-only, cap
records by count and time, reject pointer/vtable mismatches, and never record
entered text.

1. **Thread and reentrancy:** record thread, recursion depth, object/vtable,
   and return RVA at `0x7463C0`, `0x958FF0`, `0x747CD0`, `0x7AA0D0`,
   `0x7507B0`, `0x74B140`, `0x74AE50`, `0x7EF6D0`, and `0x7AE040` for at most
   64 transitions or ten seconds.
2. **Device-isolated row trace:** repeat one page with controller, mouse, and
   keyboard separately; record only row subobject vtables, gate before/after,
   selection before/after, and whether the callback fired.
3. **Editor movie event:** at virtual `+0x118`, record movie/vtable, event kind,
   and return for one key down/up, character, mouse move/click, controller
   commit, and cancel. Do not log `wParam`, characters, clipboard, or text.
4. **Back task:** assign one observation ID at `0x7AA0D0`, then follow that
   exact accepted pointer through `0x7AA1F0`, slot-2 updates, and final
   release without synthesizing a retain.
5. **Focus and teardown:** compare one ordinary page, TextInput dialog, and
   generic alert; verify forced-zero parent gate, child-before-parent cleanup,
   exactly-once release, and parent input restoration.
6. **FSCommand:** first enumerate a concrete derived handler that overrides
   the no-op. Do not install a process-wide hook merely to observe the generic
   runtime.

Open questions remain: the caller/owner of the base input gate, exact row
controller types, complete ordinary device-to-action path, concrete movie
interface at `+0x118`, exact Back-task type and fade/pop write, UI/movie thread
identity, Unicode/IME support, and safe movie invocation outside a native
controller call stack. Use the highest recovered native boundary that already
owns focus and teardown until each narrower question is answered.
