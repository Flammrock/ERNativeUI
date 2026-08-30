# Native addresses and pagination research

The action-style selector/list path is documented separately in
[Native popup-choice rows](NATIVE_POPUP_CHOICES.md).

This document explains the native Elden Ring interfaces used by ERNativeUI,
how pagination works, and how native **Previous Page** was identified. The
names below are research labels, not official FromSoftware symbols.

The separate blocking-task lifecycle and modal input gates used by
generic OK alerts are documented in
[Native dialogs and modal input research](NATIVE_DIALOGS.md).

## Build identity and terminology

The Back investigation used the 28 August 2026 Windows update: PE timestamp
`0x69E9C9B9`, image size `0x5E09600`. The tested runtime base was
`0x00007FF773C60000`.

An RVA is independent of ASLR: `RVA = runtime address - module base`. An AOB
is a byte pattern; `??` masks relocation-sensitive operands such as relative
call displacements. `src/addresses.cpp` scans executable sections first. A
fallback RVA is accepted only if its bytes match a validation pattern. An RVA
alone is not evidence that a function retained its meaning after an update.

## Maintained native interfaces

Most fallbacks are from the earlier supported build; the AOB located the moved
function on the investigated build. Patterns and ABI typedefs in
`src/addresses.cpp` and `src/addresses.hpp` are authoritative.

| Source name | Purpose | Fallback RVA | 2026-08-28 RVA | Method |
| --- | --- | ---: | ---: | --- |
| `hub handler` | Controller Settings hook | `0x958C50` | `0x959DF0` | direct AOB |
| `subpage handler` | Native child-page hook | `0x95A5D0` | `0x95B770` | direct AOB |
| `open subpage` | Push a native child page | `0x94FA20` | `0x950850` | direct AOB |
| `on/off list` | Construct toggle choice data | `0x955550` | `0x955550` | extended direct AOB |
| `menu context` | Construct row context | `0x869300` | `0x86A2F0` | direct AOB |
| `text ref help` | Construct help reference | `0x760790` | `0x7615E0` | direct AOB |
| `text ref label` | Construct label reference | `0x760970` | `0x7617C0` | call-target AOB |
| `add toggle` | Add native toggle row | `0x948FA0` | `0x94A140` | call target + prologue |
| `add choice` | Add inline discrete-choice row | `0x949120` | `0x94A2C0` | unique call target + prologue |
| `choice context` | Construct choice-row input context | — | `0x86A410` | direct prologue |
| `choice list builder` | Construct the native inline option container/template | — | `0x956CF0` | direct prologue |
| `add slider` | Add native slider row | `0x9493F0` | `0x94A590` | call target + prologue |
| `add button` | Add native action row | `0x9298A0` | `0x92AA40` | direct AOB |
| `destroy text refs` | Release temporary row texts | `0x742C90` | `0x743AE0` | direct AOB |
| `native text resolver` | Resolve custom message IDs | `0x7633D0` | `0x764220` | direct AOB |
| `native Back` | Pop current native page | `0x747CD0` | `0x747CD0` | direct AOB + prefix validation |
| `page frame` | Gate page input beneath an owned alert | `0x958FF0` | `0x958FF0` | direct semantic AOB + validated fallback |

## How native Back was found

Pagination already pushed continuations through Elden Ring's native
`open subpage`. Previous therefore needed to perform the same pop as the real
Back/Cancel control, not reconstruct an earlier slice.

### Bounded observation

Temporary SafetyHook mid-hooks were installed by the DLL; `eldenring.exe` was
never modified. A trace was armed only after Previous Page was selected,
expired after 12 seconds, and capped its output. Early probes near plausible
menu handlers did not execute during a real Back. A probe on the UI event
submission routine at RVA `0x7AA0D0` did.

Two independent real-Back presses produced the same evidence:

```text
event submission caller RVA: 0x747BEA
RCX: current page + 0x10
RDI: current page
elapsed after arming: 141 ms and 94 ms
```

The human/controller delay was irrelevant: the repeated caller RVA and object
relationship were stable while absolute page addresses changed.

### Disassemble upward

PE exception data (`.pdata`) placed the event call in the function at
`0x747850..0x747CD0`. Its prologue copies `RCX` (page) to `RDI` and `RDX`
(an opaque action object) to `RSI`. Near the captured site it does:

```asm
lea  rcx, [rdi+10h]  ; page event queue
mov  rdx, <event>
call 7AA0D0h         ; submit UI event
```

Calling that function directly would be unsafe because its second argument is
nontrivial. Its only direct caller was `0x747DB9`, in the wrapper beginning at
`0x747CD0`. That wrapper takes the page in `RCX`, checks `page + 0x3B0`, builds
the action object on its stack, calls `0x747850`, and sets the guard byte. Its
effective boundary is:

```cpp
using NativeBackFn = void(__fastcall*)(
    void* page,
    std::uint64_t action);
```

The wrapper stores the complete incoming `RDX` payload and tests its low DWORD
through `0x7AA090`; kind `2` selects an alternate internal branch. The
concrete page's real Back method at current-build RVA `0x9580C0` constructs
the two-DWORD payload `{3, 0}` through `0x7AA060`, loads its full 64 bits into
`RDX`, and calls this wrapper through vtable slot `+0x60`. Production detours
therefore preserve and forward the full 64-bit payload for game-originated
calls. ERNativeUI's synthetic Previous Page passes the same packed value `3`
instead of inventing kind `0` or relying on incidental register contents.
The temporary discovery hooks were then removed.

### Production safeguards

The native Back address requires a unique AOB or a byte-validated fallback.
The call has an SEH logging boundary. Before it is made, code also verifies
that the native page has a known route and the target is exactly one slice
earlier. Root continuation slice 1 is specially required to target the
existing Controller Settings root with the same detected capacity.

SEH is a final diagnostic boundary, not a substitute for address and argument
validation.

## How physical-page titles were identified

Page titles cross two different Scaleform assets and should not be inferred
from similar-looking runtime strings:

- `02_040_optionsetting.gfx` is the outer options menu. Its visible
  `MenuTitle/StaticText_101003` field carries the Controller Settings root's
  `Configuration` heading.
- `02_042_pc_graphicsetting.gfx` is the graphics-style native subpage used by
  custom physical pages. Its visible inner heading is
  `GraphicOption/StaticText_111114`.

A temporary, bounded resolver observation recorded the runtime inputs
`MenuTitle/Text` and `MenuTitle/Text_0` while a known custom page route was
pending. Those inputs are lookup paths used by native construction code; they
are not the names of the visible fields above.

The first controlled write is useful evidence precisely because it targeted
the wrong visual layer: it safely changed the outer `Configuration` heading.
That result confirmed the captured Scaleform wrapper, its UTF-16 value at
wrapper plus `0x08`, and the setter call, while disproving the assumption that
the resolved `MenuTitle` object was the inner page heading. Comparing the tag
graphs of both extracted assets then identified
`GraphicOption/StaticText_111114` as the correct inner field.

The page constructor owns a persistent `0x60`-byte path result at page offset
`0x230`; the text setter consumes its value at result offset `0x08`. Production
code redirects that one game-owned result only to the inner heading. Reusing
the same result for the outer heading would overwrite its live value, so the
outer assignment instead mirrors the game's one-shot call sites: resolve
`MenuTitle/StaticText_101003` into an aligned, zero-initialized `0x60`-byte
temporary, set through temporary offset `0x08`, then destroy the nested value
at temporary offset `0x28`.

This work is confined to construction of a pending custom physical page.
Compiled titles are host-owned, the game-owned root is left unchanged, and
the three title helpers are optional: failure to resolve any of them disables
title replacement without disabling menu rows. Their locations remain
version-sensitive implementation details that must be rediscovered and
byte-validated after game updates; fallback RVAs are not public contracts. See
[Page titles and presentation](PAGE_PRESENTATION.md) for the user-facing
policy and formatter lifetime rules.

## Pagination model

A logical page is compiled into physical `PageSlice` objects. Subpages use a
conservative capacity of 15 native rows:

- a single slice uses the full capacity;
- the first overflowing slice reserves one row for Next;
- middle slices reserve Previous and Next rows;
- the final slice reserves one Previous row.

Next pushes a real child page and binds its `PageRoute` to the resulting native
object. Previous pops that object through native Back, allowing Elden Ring to
restore the true parent page, stack, and selection state.

Controller Settings is special because four vanilla rows already exist.
Runtime detection accepts capacities 6 through 13, leaving 2 through 9 custom
slots on a non-overflowing first slice. Continuations use capacity 15.

### Runtime Controller capacity discovery

Early builds found the capacity by locating a loose
`02_040_optionsetting.gfx` and parsing it during host startup. That worked for
the known asset but coupled the runtime DLL to filesystem layout and to the
standalone GFX parser.

A controlled comparison used the same game build and menu configuration with
only the GFX changed:

| Page-object field | Six-row GFX | Seven-row GFX | After injection |
|---|---:|---:|---|
| `page + 0xB14` (`uint32_t`) | 6 | 7 | unchanged |
| `page + 0xD84` (`uint32_t`) | 7 | 8 | unchanged |
| `page + 0xD88` (`uint32_t`) | 6 | 7 | unchanged |

The first broad scan also found `page + 0x88 == 6` in the seven-row case, but
the field did not become 5 with the six-row asset, so it was rejected. Both
exact candidates remained stable before and after ERNativeUI added its rows.
`+0xB14` was selected because it directly stores the capacity and is isolated
between stable control fields; the `+0xD84/+0xD88` pair appears to encode
related collection/index state.

Production reads `page + 0xB14` only after the original Controller Settings
handler has constructed the page and before ERNativeUI injection. The read is
inside an SEH boundary and accepts only the two layouts ERNativeUI supports,
6 through 13. Any access fault or unexpected value retains the conservative
six-row planning fallback. Live tests additionally confirmed that this field
reports 10 and 15 for those generated layouts and that pagination consumes the
values correctly. Rows 14 and 15 remained selectable but overflowed the panel
artwork, establishing 13 as the supported visual maximum. Consequently
`ERNativeUI.dll` no longer searches
for or parses a loose GFX file; the GFX implementation is linked only into
the standalone patcher and its tests.

`tests/pagination_test.cpp` covers named boundaries and a matrix of capacities
3..32 with row counts 0..512. It checks contiguous coverage, native row limits,
navigation placement, non-empty overflow slices, and Previous route validity.

## Maintenance after a game update

1. Preserve the failure log, PE timestamp, and image size.
2. Note whether each AOB is missing, ambiguous, or resolves to a moved RVA.
3. Disassemble old and candidate functions; compare behavior, callers,
   structure offsets, and call targets. Nearby RVAs are only search hints.
4. If ambiguous, extend the AOB with stable instructions. Wildcard relative
   displacements and relocated addresses.
5. Change a fallback only after checking both bytes and semantics. Never skip
   validation merely to let installation continue.
6. Run all tests, then perform a bounded live test through Mod Engine 2.
7. Add the new PE identity and observed RVAs to this document.

For new research, prefer temporary DLL hooks over executable patching. Make
them opt-in, time-bounded, rate-limited, and easy to delete. Compare multiple
captures; one nearby function or plausible register snapshot is insufficient.
