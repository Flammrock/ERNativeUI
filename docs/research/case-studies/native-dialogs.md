# Native generic dialogs

Status: **Confirmed** on the [reference build](../README.md#reference-build).
The native names on this page are analytical names, not symbols supplied by
FromSoftware.

For the supported client API, use the
[native dialogs guide](../../guides/native-dialogs.md).
This case study records why the private transport is trusted and how to
revalidate it after a game update.

## Question and scope

The research question was:

> Can ERNativeUI build Elden Ring's generic message dialog, place it in the
> game's blocking-popup lifecycle, observe which action closed it, and give it
> exclusive input without replacing a game-owned task?

The scope is deliberately narrow: zero, one, or two native buttons; bottom or
center placement; asynchronous completion; and correct modal behavior over a
settings page, the title screen, and character creation. Arbitrary captions,
titles, icons, text fields, and unknown builder variants are outside the
confirmed boundary.

## Derivation

Natural game dialogs supplied the behavioral control. Startup connection
errors and the Church of Vows absolution message demonstrated the desired
native presentation and an independently functioning OK path.

- **Confirmed:** A native failed-save path on the reference build constructs a
  dialog task through a context rooted at the popup object plus `0x10`, then
  installs it into the popup object's intrusive task slot at `+0x298`.
- **Confirmed:** The `+0x298` field is an owned task pointer, not a modal
  Boolean. The nearby `+0x290` request condition is a different field.
- **Confirmed:** The task-slot assignment consumes the submitted reference,
  gives the slot ownership, and returns a temporary reference that must be
  released.
- **Confirmed:** The normal popup update polls and ultimately clears/releases
  this slot. The transition from the exact submitted job to null on the same
  popup owner is the authoritative successful dismissal.
- **Confirmed:** The job poll exposes native result kind `2` for the left
  action and `3` for the right action. Public responses are normalized by the
  requested layout rather than exposing those private numbers.
- **Inferred:** `CSPopupMenu`, `CSMenuManImp`, and `MenuWindowJob` describe the
  recovered roles accurately. Their complete original class layouts and all
  builder semantics are not known.

Current production does not start from a copied RVA. It finds a unique
semantic wrapper, derives the frontend-manager slot and relative call targets,
and separately validates the lower frontend, title task, popup update,
descriptor helpers, intrusive assignment, job poll, and release path. The
historical build RVAs remain useful only for navigating that exact image:

| Boundary | Reference-build RVA | Evidence/use |
|---|---:|---|
| Complete lower frontend update | `0x76F640` | **Confirmed** relative target of the outer two-call update anchor |
| Popup update | `0x7EF6D0` | **Confirmed** second target of the same anchor |
| Failed-save dialog path | `0x7F14C0` | **Confirmed** static ownership model; not called directly |
| Consuming task-slot assignment | `0x7AA2E0` | **Confirmed** target derived from the `+0x298` install call site |
| Advanced Settings page frame | `0x958FF0` | **Confirmed** typed input boundary used as a defensive page gate |
| Native Back wrapper | `0x747CD0` | **Confirmed** typed action boundary |

The patterns beside their consumers in
the current online [`src/native_dialog.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog.cpp) and
[`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp) are authoritative. The
[historical address map](../address-map/README.md) is not a production lookup
table.

## Bounded live procedure

Reproduce this only offline, with Easy Anti-Cheat disabled, on a recorded
executable hash. Use temporary instrumentation; do not patch `eldenring.exe`.

1. Add an opt-in probe that recognizes only the observed popup owner and the
   exact job pointer submitted during the armed test. Arm it for one dialog,
   expire it after 10 seconds, and cap each site at 32 records.
2. As a control, trigger a natural game dialog. Record normalized caller RVAs,
   popup pointer, `+0x290`, `+0x298`, submitted/returned references, poll kind,
   and the slot transition. Do not dereference any other candidate pointer.
3. Repeat the control once. Heap addresses may change; owner/slot relationships
   and normalized call sites must agree.
4. With the slot empty, submit one temporary descriptor at a time. Exercise
   the finite matrix of zero, one, and two buttons in both placements. For
   visible buttons, activate every position and repeat once using Back.
5. Open one owned dialog over each of: an ERNativeUI settings page, the title
   screen, and character creation. Try navigation, confirm, and Back below the
   popup; only the popup action may advance.
6. Queue two owned dialogs and observe their FIFO handoff. In a separate
   control, enqueue while a game-owned task occupies `+0x298`; the custom
   request must wait rather than replace it.
7. Remove the probe build after collecting the bounded log. Promote a changed
   signature only after static callers, arguments, ownership, dismissal, and
   teardown still agree.

## Results

The finite presentation matrix produced these values on the reference build:

| Placement | No buttons | One button | Two buttons |
|---|---:|---:|---:|
| Center | builder `6` | builder `1` | builder `2` |
| Bottom | builder `9` | builder `7` | builder `8` |

- **Confirmed:** Native label `1` renders OK, `2` CANCEL, `3` YES, and `4` NO.
- **Confirmed:** OK+CANCEL renders left/right and returns primary/secondary;
  YES+NO behaves the same way. The right action and Back dismiss correctly.
- **Confirmed:** A no-button layout can be dismissed and is normalized to the
  public `dismissed` response.
- **Confirmed:** Skipping the complete lower frontend while the exact task is
  owned blocks underlying title, character-creation, and settings input. The
  outer frame still updates the popup separately, so its buttons remain live.
- **Confirmed:** The specialized title task is given a private disabled byte;
  the owned popup alone receives a private enabled byte.
- **Confirmed:** Native action `3` is overloaded. Outside the owned popup
  update it is page Back and is suppressed; synchronously inside that update
  it is the two-button dialog's secondary response and is forwarded.

The public builder/label translation is isolated in
[`src/native_dialog_presentation.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog_presentation.hpp).
Clients therefore depend on semantic buttons and responses, not these native
numbers.

## Modal input path and page-frame ABI

Displaying the task and giving it exclusive input were separate problems.
The outer menu update creates one stack byte with value `1`, tests the popup
task slot, and forwards a pointer to that byte through two calls:

```text
outer menu update
|-- complete lower frontend update   R8 = pointer to game input byte
`-- CSPopupMenu::update              R8 = pointer to same byte
```

When a normal blocking task occupies `CSPopupMenu + 0x298`, the game changes
the byte to zero before those calls. Reusing that zero unchanged was not
sufficient for an ERNativeUI-owned dialog: the underlying page stopped, but
the popup's own OK action also stopped responding.

The exact reference-build instruction anchors were `0x767989` for byte
initialization, `0x76799F` for the `+0x298` test, `0x7679C1` for the lower
frontend call, and `0x7679DB` for the popup call. These are navigation points
for the recorded hash, not production constants.

The successful division of responsibility is:

```text
exact ERNativeUI task owns popup +0x298
|-- skip the complete lower frontend update
|-- update the specialized title task with a private disabled byte
`-- update CSPopupMenu with a private enabled byte
```

Skipping the complete lower frontend matters because the dispatcher forwards
its input pointer to only one branch. Sibling title-screen and
character-creation tasks receive input through other branches. Gating only a
settings page therefore allowed those screens to keep consuming movement,
Confirm, and Back. The outer frame updates `CSPopupMenu` separately after the
skipped call, so the owned dialog remains interactive.

The additional Advanced Settings page boundary on the reference build is RVA
`0x958FF0`. Its recovered Windows x64 signature is:

```cpp
using PageFrameFn = void(__fastcall*)(
    void* page,                    // RCX
    float frame_value,             // XMM1
    std::uint8_t* input_enabled);  // R8
```

The floating-point register is significant: the scalar is the second ABI
argument and therefore arrives in `XMM1`, not `XMM0`. The prologue saves
`RCX`, `XMM1`, and `R8`, performs page work, restores those values, and
tail-calls the next frame routine. A typed inline detour at the validated
function entry may either forward all three unchanged or return before the
page consumes input while the exact custom alert owns the popup slot.

**Rejected:** forwarding a private zero through this page frame as a cleaner
substitute for the early return. The page stopped moving, but the popup's OK
action stopped dismissing, proving an additional confirmation or ordering
relationship in this path.

**Rejected:** synthesizing a return from a SafetyHook mid-hook by editing only
RIP and RSP. The instrumentation trampoline has its own restoration
assumptions; that experiment froze and crashed before the popup appeared. Use
a typed inline detour for conditional whole-function suppression and reserve
mid-hooks for bounded observation or a proven operand change.

Back needs one further distinction. Native action `3` means page Back outside
the popup, but it is also the secondary/right result emitted synchronously by
a two-button popup. The Back gate therefore:

- forwards action `3` while execution is inside the owned popup update;
- suppresses it elsewhere while the exact popup/job pair is active; and
- forwards it normally at every other time.

Every custom child-page binding retains its native parent. A normal Back can
restore that already-constructed parent without invoking a materializer, so
the detour republishes the restored parent to the page-frame gate after
forwarding the action. This is why modal ownership continues to work after
Next, Previous, and nested-submenu navigation.

## Ownership, threading, and fail-closed behavior

The enqueue path copies at most 4,096 UTF-16 code units and accepts at most 64
outstanding requests process-wide. It is thread-safe. `ERUI_OK` means queued,
not shown or completed.

Native construction, popup update, response observation, and slot retirement
stay on the game's update path. The host remembers the exact popup/job pair:

```text
same owner + same job   -> still active
same owner + null slot  -> normal dismissal
new owner or other job  -> ownership lost; internal error
temporarily unreadable  -> do not guess; retry next frame
```

The host never writes over an occupied slot. It follows the observed intrusive
reference convention, retains custom text beyond native retirement, and
invokes client completion callbacks later on the host worker, outside dialog
locks and outside the native hook. Client callback state must consequently
remain valid and be synchronized.

- **Confirmed:** If any required signature is missing or ambiguous, or any
  required hook cannot be installed, the whole dialog transport remains
  unavailable. Menu features continue; alert calls return
  `ERUI_NOT_SUPPORTED`.
- **Confirmed:** Losing the remembered owner/task relationship completes the
  request with `ERUI_INTERNAL_ERROR`, never as a guessed user response.
- **Confirmed:** A native fault after a consuming submission begins is not
  followed by a speculative release; a rare leak is preferred to a possible
  double release or use-after-free.

## Rejected paths

- **Rejected:** Treating a non-null `+0x298` as ERNativeUI ownership. It could
  be a game or another mod's task.
- **Rejected:** Candidate RVA `0x9536F0` as the Advanced Settings input
  authority. It ran every frame on the expected object, but suppressing it did
  not stop selection movement.
- **Rejected:** Passing a private zero byte through the page frame instead of
  using a typed early return. It blocked page movement and Back, but also made
  the popup's OK action stop dismissing.
- **Rejected:** Synthesizing a return by editing RIP/RSP in a mid-hook. The
  game froze and crashed. The typed inline detour at the validated function
  boundary established the intended behavior safely.
- **Rejected:** An arbitrary delay or page-specific key filter as modal
  ownership. The correct discriminator is the exact popup owner/task pair.

## Limits and next-build checks

- The complete descriptor layout, all builder kinds, and all native poll
  states remain unknown. Do not expose or probe arbitrary numeric variants in
  production.
- The likely `01_010_MessageBox` GFX family has not been tied conclusively to
  this builder path; visual similarity is not proof.
- Modal correctness must be rechecked with mouse/keyboard and controller,
  every supported response, both placements, title, character creation,
  custom pages, reopening, FIFO handoff, and a game-owned dialog after every
  game update.
- Compatibility with another mod detouring one of these exact functions is a
  separate live-test claim; successful standalone resolution does not prove
  hook coexistence.

## Current source anchors

- Transport, resolver, FIFO, intrusive lifetime, modal gates, and callback
  dispatch: [`src/native_dialog.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog.cpp)
- Exact popup/job state machine:
  [`src/dialog_modal_state.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/dialog_modal_state.hpp)
- Public-to-native presentation mapping:
  [`src/native_dialog_presentation.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog_presentation.hpp)
- Defensive page-frame and Back gates:
  [`src/hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp)
- Host-facing transport contract:
  [`src/native_dialog.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog.hpp)
- Presentation normalization and modal ownership tests:
  [`tests/native_dialog_presentation_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/native_dialog_presentation_test.cpp)
  and
  [`tests/dialog_modal_state_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/dialog_modal_state_test.cpp)
- Normative C ABI:
  [`include/ernativeui/erui.h`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h)
