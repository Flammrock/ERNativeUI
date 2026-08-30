# Native dialogs and modal input research

This document explains the native generic alerts used by ERNativeUI, their
verified button/placement mappings, and the extra work required to make them
truly modal over a custom Advanced Settings page. The class and function names
are research labels, not official FromSoftware symbols.

The important result is that displaying a blocking task and owning input focus
are related but separate problems. A correct alert needs all of the following:

1. Put an owned native task in `CSPopupMenu + 0x298` without replacing a game
   task already there.
2. Let only `CSPopupMenu` see a private enabled input byte for that task.
3. Suspend the custom page frame dispatcher while the task is present.
4. Suppress native Back for the captured custom page until that task is
   dismissed, without suppressing the popup's own action path.

This combination was tested with controller input: alerts appeared in both
placements, page selection no longer moved underneath them, every supported
action and Back response was observed, and alerts could be opened again.

## Build identity and address policy

The investigation used the 28 August 2026 Windows build: PE timestamp
`0x69E9C9B9`, image size `0x5E09600`. All RVAs in this document are
**observations for that build**, not API constants and not proof that a
function has the same meaning in a later executable.

Production code prefers a unique semantic AOB and resolves relative call
targets from it. A fallback RVA is accepted only when its validation bytes
still match. The authoritative patterns are kept next to their consumers in
`src/addresses.cpp` and `src/native_dialog.cpp`; copying the bytes into this
document would create a second source that could silently become stale.

| Research label | 2026-08-28 RVA | Production location method |
| --- | ---: | --- |
| outer popup-slot input test | `0x76799F` | research anchor only; contained in the popup-update call-site signature |
| `CSPopupMenu::update` | `0x7EF6D0` | relative target of a unique two-call semantic AOB |
| native failed-save alert path | `0x7F14C0` | research anchor only; its install call site anchors the reusable helper |
| intrusive task-slot assignment | `0x7AA2E0` | relative target of the unique `+0x298` install AOB |
| task-slot update/retirement | `0x7AA480` | research anchor; reached by the original popup update, not called directly by ERNativeUI |
| Advanced Settings page frame dispatcher | `0x958FF0` | direct AOB (`kPageFramePattern`) plus byte-validated fallback |
| native Back wrapper | `0x747CD0` | direct AOB (`kNativeBackPattern`) plus byte-validated fallback |
| rejected page candidate | `0x9536F0` | diagnostic-only current-build candidate; not a production interface |

The dialog transport also derives its descriptor helpers, builder, frontend
manager slot, intrusive-reference release helper, and `MenuWindowJob` polling
function from unique semantic patterns. The polling hook observes only the
exact ERNativeUI task long enough to capture its native response kind; the
popup slot remains the lifecycle/ownership authority. None of these private
addresses is exposed through the public ABI.

## Verified layouts, placement, and responses

The public API exposes semantic choices rather than Elden Ring's raw builder
and label numbers:

| Public C choice | C++ choice | Visible native buttons |
| --- | --- | --- |
| `ERUI_ALERT_BUTTONS_OK` | `erui::AlertButtons::ok` | OK |
| `ERUI_ALERT_BUTTONS_CANCEL` | `erui::AlertButtons::cancel` | CANCEL |
| `ERUI_ALERT_BUTTONS_YES` | `erui::AlertButtons::yes` | YES |
| `ERUI_ALERT_BUTTONS_NO` | `erui::AlertButtons::no` | NO |
| `ERUI_ALERT_BUTTONS_OK_CANCEL` | `erui::AlertButtons::ok_cancel` | OK, CANCEL |
| `ERUI_ALERT_BUTTONS_YES_NO` | `erui::AlertButtons::yes_no` | YES, NO |
| `ERUI_ALERT_BUTTONS_DISMISS_ONLY` | `erui::AlertButtons::dismiss_only` | none |

The game localizes these labels. `ERUI_ALERT_PLACEMENT_BOTTOM` /
`erui::AlertPlacement::bottom` uses the lower presentation and is the C++
default. `ERUI_ALERT_PLACEMENT_CENTER` /
`erui::AlertPlacement::center` uses the centered presentation. C++
`AlertOptions` defaults to `ok` plus `bottom`; callers change only the fields
they need.

The completion callback receives both an operational `ERUI_Result` and a
semantic response:

- `primary` is the only visible button in a one-button layout. Any successful
  close of a one-button alert, including Back, is normalized to primary.
- In a two-button layout, the left button is `primary`; the right button and
  the controller/keyboard Back action are `secondary`.
- A successful no-button `dismiss_only` alert reports `dismissed`.
- `none` means the alert failed operationally or no trustworthy native
  response was available. Consumers should inspect `ERUI_Result` first.

The equivalent C names are `ERUI_ALERT_RESPONSE_PRIMARY`,
`ERUI_ALERT_RESPONSE_SECONDARY`, `ERUI_ALERT_RESPONSE_DISMISSED`, and
`ERUI_ALERT_RESPONSE_NONE`. Back above is the action consumed by the popup;
the page's own native Back path remains suppressed so it cannot close the page
under the alert.

### Recovered private mapping

The exhaustive live matrix established these current-build native values:

| Placement | Zero-button builder | One-button builder | Two-button builder |
| --- | ---: | ---: | ---: |
| center | `6` | `1` | `2` |
| bottom | `9` | `7` | `8` |

Descriptor label `1` means OK, `2` means CANCEL, `3` means YES, and `4`
means NO. A missing button uses no label. The native completion payload uses
kind `2` for the primary action and kind `3` for the secondary action; a real
Back on either verified two-button layout also produced kind `3`.

The response kind is sampled from the exact ERNativeUI `MenuWindowJob` before
native code retires the outer task. Capturing it does not complete the public
request by itself: normal removal of the remembered task from the remembered
popup owner's `+0x298` slot still authorizes successful completion.

These numbers describe Elden Ring internals and are deliberately hidden by the
public enums. They can change with a game update without changing the C ABI.
The host selects builder count from `ERUI_AlertButtons`, selects placement
independently, validates the native response against the requested layout, and
returns only the normalized public response.

## The blocking `+0x298` lifecycle

`CSPopupMenu + 0x298` is an intrusive task pointer, not a Boolean modal flag.
It must be kept distinct from the byte at `+0x290`, which the game's own
failed-save path uses as a request condition.

The native path at current-build RVA `0x7F14C0` provided the ownership model:

1. It checks `popup + 0x290` and other native preconditions.
2. It refuses to build another task while `popup + 0x298` is occupied.
3. It builds the task through the popup context rooted at `popup + 0x10`.
4. It assigns the task to `popup + 0x298` through the intrusive-pointer helper
   at current-build RVA `0x7AA2E0`.
5. The assignment consumes the submitted pointer, gives the slot its owning
   reference, and returns a temporary reference which the caller releases.

Every `CSPopupMenu::update` frame later passes the `+0x298` slot to the native
task updater at current-build call site `0x7EFC36` (target `0x7AA480`). When the
task reports completion, native code clears and releases the slot. ERNativeUI
uses the same ownership convention, observes the exact task pointer that it
submitted, and treats the slot transition away from that pointer as the
authoritative end of the blocking alert.

The host also retains the exact popup owner pointer. After each popup update
it compares the instance that actually updated, that instance's `+0x298`
slot, and the remembered owner/task pair. This covers native code clearing or
replacing the slot between two observed frames, as well as replacement of the
popup singleton itself; modal input ownership cannot remain latched merely
because ERNativeUI missed the exact true-to-false transition or retained a
stale owner address.

A readable null slot on the same popup owner is normal dismissal and completes
the public callback with `ERUI_OK` plus the validated, normalized response. A
different owner or replacement task is an ownership failure and completes with
`ERUI_INTERNAL_ERROR` plus `none`. A transient fault while reading the current
popup does not guess: ownership remains active and the next frame retries.

This leads to three safety rules in the implementation:

- If `+0x298` is already occupied, an ERNativeUI request waits; it never
  overwrites a game alert or another task.
- The submitted pointer is retained only according to the native intrusive
  reference contract. Violating that contract permits leaks, double releases,
  and use-after-free crashes; early prototypes exhibited those failure classes.
- Completion is associated with the exact submitted pointer. A merely non-null
  `+0x298` is not enough to claim that ERNativeUI owns the dialog.

## What the third argument gates

The third argument of the relevant frame-update functions is a pointer to a
one-byte input-enabled value. It is not the blocking task itself.

In the current build, the outer menu update does this before calling the lower
frontend and popup layers:

```text
0x767989  initialize the stack byte to 1
0x76799F  test CSPopupMenu + 0x298
            and change the byte to 0 when that slot is occupied
0x7679C1  call the lower frontend update with R8 = &byte
0x7679DB  call CSPopupMenu::update with R8 = &byte
0x767A17  forward the same byte to a later menu-task stack
```

The zero passed to the lower layers is useful: it prevents ordinary menu input
while a blocking task exists. Passing that same zero to `CSPopupMenu`, however,
made the custom alert visible but left its OK button unable to accept input.

The working transport therefore preserves the game's zero for the page below
and gives only `CSPopupMenu::update` a private byte set to one, and only while
`popup + 0x298` equals ERNativeUI's submitted task. The original popup update
continues to poll and retire `+0x298`; the detour does not reproduce that
state machine.

This split fixed popup input without restoring input to unrelated menu layers.
It was still not sufficient for the Advanced Settings page because that
concrete page has an additional frame path described below.

## Advanced Settings page frame dispatcher

The confirmed concrete page boundary is current-build RVA `0x958FF0`. Its
Windows x64 ABI is:

```cpp
using PageFrameFn = void(__fastcall*)(
    void* page,          // RCX
    float frame_value,   // XMM1
    std::uint8_t* input_enabled // R8
);
```

The register assignments matter. Floating-point arguments consume their ABI
position, so the second parameter arrives in `XMM1`, not `XMM0`; the third
parameter remains in `R8`. Static disassembly corroborates the type: the
prologue saves `RCX`, `XMM1`, and `R8`, performs concrete-page work, restores
the same three values, and tail-calls the next frame routine. The downstream
body reads the byte behind the saved `R8` only at its input decisions.

A normal typed inline detour supplied the decisive runtime evidence. During
discovery, returning without calling the original only when an ERNativeUI
alert owned the blocking slot produced all four expected effects:

- the alert still rendered;
- controller OK worked;
- visual selection on the Advanced Settings page stopped moving;
- dismissing and reopening the alert continued to work.

Production uses that same typed early return while ERNativeUI owns the exact
blocking task. At all other times it forwards the original page, scalar, and
input pointer unchanged. A separate native Back gate covers the concrete
pre-tail Cancel path.

A later cleanup experiment tried to preserve the whole frame routine by
forwarding a private input byte set to zero. Static analysis supported the
argument type, but the live result was decisive: selection and Back remained
blocked, while the popup's OK action stopped dismissing the dialog. The page
frame therefore has an additional confirmation side effect or ordering
relationship not represented by that one byte. That refinement is rejected;
do not reintroduce it without new runtime evidence.

### Task- and page-scoped modal ownership

The frame gate is deliberately narrower than "a dialog exists":

```text
ERNativeUI task is exactly the current +0x298 task
AND the dispatcher page is the captured ERNativeUI physical page
        -> suspend that page's call to 0x958FF0
ERNativeUI task is exactly the current +0x298 task
AND the native Back action is 3
AND execution is outside the owned CSPopupMenu update
        -> suppress menu Back
```

The root and subpage materializers record the exact physical page for frame
suppression. Native Back needs a different discriminator: a root trace showed
that Controller Settings passes a companion owner object rather than the frame
page. Menu Back uses action `3`, but a two-button dialog also uses action `3`
for its right/secondary response. The secondary response is emitted
synchronously from the owned `CSPopupMenu` update, so the Back gate permits it
only in that scope and suppresses action `3` elsewhere while exact task
ownership is active. Task ownership also prevents stale pages from being gated
after dismissal and leaves game-owned dialogs alone.

Every child-page binding retains the native parent used to open it. Ordinary
Back restores an already-constructed parent without re-entering a materializer,
so the detour republishes that parent to the frame gate after forwarding Back.
This covers shared-root and nested-submenu returns.

The real Cancel/Back command has its own path. While exact task ownership
holds for the captured ERNativeUI page, the host also suppresses the validated
native Back wrapper at current-build RVA `0x747CD0`. Otherwise the original
two-argument wrapper is called with its page and complete 64-bit action payload
unchanged.
ERNativeUI's synthetic Previous Page explicitly uses the real page's packed
Back action `{3, 0}`. The discovery and validation of that wrapper are documented in
[Native addresses and pagination research](NATIVE_ADDRESSES_AND_PAGINATION.md).

## Rejected candidate: `0x9536F0`

RVA `0x9536F0` looked promising because it belongs to the same concrete page
class region as the subpage materializer. A bounded live probe showed a stable
page pointer and second argument and ran approximately once per frame. That
was useful evidence, but frequency and proximity did not establish authority
over navigation.

Its recovered type and body are materially different from the working frame
dispatcher:

```cpp
void* __fastcall candidate(void* page, void* argument);
```

It preserves `RDX`, calls other presentation/update work, conditionally handles
state behind `page + 0x1CE0`, and returns the original second argument in
`RAX`. A typed inline experiment that returned that forwarded argument instead
of calling the original did **not** stop visual selection movement; the popup
continued to work. It is therefore rejected as the page's navigation/input
authority and must not be retained as a production gate.

This is a useful reverse-engineering lesson: a function running at 60 Hz on the
right object can still own presentation or synchronization rather than input.

## Why the mid-hook synthetic return was unsafe

An early experiment placed a SafetyHook mid-hook at the entry of `0x958FF0`
and tried to emulate an immediate native return by changing the captured
context:

```cpp
context.rip = *reinterpret_cast<std::uintptr_t*>(context.rsp);
context.rsp += 8;
```

When the alert was requested, the game froze and then crashed before the
dialog appeared. This result does **not** disprove the semantic role of
`0x958FF0`; the later typed inline detour proved that role.

The unsafe part was synthesizing a function return from inside a mid-hook
callback. The callback runs through SafetyHook's generated instrumentation and
trampoline machinery, not through an ordinary C++ function-call boundary.
Editing only native `RIP` and `RSP` did not reproduce the hook's full execution
and restoration assumptions. Similar shortcuts are even more dangerous after
a native prologue has established nonvolatile registers, a security cookie,
stack temporaries, or unwind state.

Use a typed inline detour at a validated function entry when arguments must be
forwarded or a whole function must be conditionally suppressed. It gives
SafetyHook a normal ABI boundary and a typed original trampoline. Mid-hooks
remain appropriate for bounded observation, or for changing a proven
register/operand while allowing the captured instruction stream to continue.

## Maintenance checklist

After an Elden Ring update:

1. Record the PE timestamp and image size before changing any fallback.
2. Require every transport and page-frame AOB to resolve uniquely, or require
   the fallback bytes to validate exactly. Never trust an unchanged RVA alone.
3. Recheck the `+0x290`/`+0x298` field meanings and the consuming ownership
   behavior around the native install helper.
4. Verify the page-frame caller still supplies `RCX`, `XMM1`, and `R8` with the
   documented types, and that the downstream routine still treats `R8` as an
   input-enabled byte pointer.
5. Test a game-owned dialog separately; ERNativeUI must not claim or replace
   its `+0x298` task.
6. On a custom page, test controller movement, each one- and two-button action,
   Back, `dismiss_only`, both placements, dismissal, and reopening. Queue at
   least two alerts to exercise retirement and handoff.
7. Keep diagnostic hooks bounded and remove rejected probes from production.
   Do not patch `eldenring.exe` and do not emulate returns from mid-hooks.
