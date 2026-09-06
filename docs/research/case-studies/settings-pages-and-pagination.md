# Settings pages, pagination, and native Back

This case study explains how ERNativeUI found a safe point for adding rows to
Elden Ring's Configuration pages, how it derives page capacity from the live
object, and why **Previous Page** invokes the game's real Back path. The names
used here are analytical labels, not recovered FromSoftware symbols.

For the public API, read [Menus, pages, and pagination](../../guides/menus-and-pages.md).
This page records implementation evidence and a procedure another researcher
can repeat.

## Reference build

The decisive observations used this Windows executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

All RVAs below are navigation aids for that exact image. Production derives
and validates native boundaries at runtime; neither an RVA nor a nearby match
is part of the ERUI ABI. See the [methodology](../methodology.md) and
[historical seed map](../address-map/README.md).

## Questions

The investigation separated four questions that initially looked like one:

1. At what point may another native row be appended and still bind to GFX?
2. Which built-in destinations share that construction model?
3. How many additional rows fit in the currently loaded movie?
4. How can a continuation return through the real page stack rather than
   imitating its animation?

This separation mattered. A correct page pointer observed after GFX binding
was still too late for a new row, and a correct materializer on a full panel
could produce no visible probe.

## Confirmed page-construction boundary

The Configuration category factory at reference-build RVA `0x93D730` returns
or creates the selected panel. A return hook mapped categories, vtables, row
counts, and capacities, but an action appended there did not render: the
row-to-Scaleform binding phase had already passed.

Static call-site analysis exposed a two-argument callback stored by each
ordinary panel wrapper:

```cpp
using BuiltinPanelMaterializerFn = void(__fastcall*)(
    void* page,
    void* menu_option_data);
```

A bounded detour called this callback's original implementation exactly once,
then appended one action row before returning. The row rendered, received
focus, invoked its callback, opened a native alert, dismissed correctly, and
survived reopen and tab switching. This established the materializer as the
usable mutation window.

The same procedure produced this sparse native map:

| Native category | Player-facing destination | Reference-build materializer |
|---:|---|---:|
| `0` | Game Options | `0x959DF0` |
| `1` | Camera Options | `0x959320` |
| `2` | Display | `0x95D540` |
| `3` | Sound | `0x959090` |
| `5` | Network | `0x95AD00` |
| `7` | Keyboard/Mouse Settings | `0x95CB30` |
| `8` | Graphics | `0x95C050` |

Native categories `4`, `6`, and `9` are not gaps in the public enum that a
client may address. Category `6`, Controller Settings, uses the specialized
`PadSettingDialog`; API 1.1 integrates actions through its Input Bindings
model. Brightness and Game End were not proven useful and safe as ordinary
provider destinations.

**Confirmed:** the supported non-controller pages use the ordinary
`OptionSettingDialog` family. **Rejected:** every visible Configuration tab
can therefore be treated as the same row page.

## Live capacity instead of parsing a loose GFX

The first implementation parsed a loose `02_040_optionsetting.gfx` at host
startup. A controlled comparison then changed only the movie's number of row
placements and sampled the same constructed page before and after provider
injection:

| Page field | Six-row movie | Seven-row movie | Changed after injection? |
|---|---:|---:|---|
| `+0xB14`, `uint32_t` | 6 | 7 | No |
| `+0xD84`, `uint32_t` | 7 | 8 | No |
| `+0xD88`, `uint32_t` | 6 | 7 | No |

The candidate at `+0x88` was rejected because it did not follow the six-row
case. `+0xB14` was promoted as the live visual capacity after it also reported
10 and 15 for generated layouts and pagination followed those values.

Rows 14 and 15 remained selectable but overflowed the panel artwork. That
established 13 as the supported expanded presentation, not as a universal
native object limit.

For ordinary built-in pages, the post-vanilla row count is the bounded
`uint64_t` at `page + 0x1AF0`. Production first checks the expected ordinary
page identity and reasonable ranges, then computes:

```text
remaining first-page slots = visual capacity - native row count
```

Camera Options provided the important negative control: its stock seven rows
already occupied all seven authored slots. The materializer hook was correct,
but no provider row could appear. Expanding its panel to 13 slots made the
unchanged probe visible and interactive.

**Confirmed:** the DLL can obtain capacity from live memory and does not need
to locate or parse a GFX file. **Rejected:** absence of a row on a full panel
proves that its hook or constructor is wrong.

## Logical pagination

Provider pages remain logical objects. At registry freeze, the compiler merges
provider rows and turns each logical page into physical slices. A slice
reserves native action rows for navigation only when required:

```text
first slice:   content, then Next
middle slice:  Previous, content, then Next
final slice:   Previous, then content
single slice:  content only
```

The first built-in slice uses its measured remaining capacity. Continuations
use the validated Advanced Settings-style page capacity. If the first native
panel has no free slot, ERNativeUI cannot place even a Next action and skips
that destination safely.

Next calls the recovered native child-page push, reference-build RVA
`0x950850`, and records a host-owned route from the resulting native page to
the logical destination and slice. This preserves the game's focus and
transition machinery.

## Finding the real Back path

Calling the child-page function again cannot implement Previous: it adds
another page rather than restoring the actual parent. The Back investigation
therefore observed a real player cancellation.

### Bounded live procedure

1. Install temporary in-memory observation hooks only; do not patch the
   executable on disk.
2. Arm tracing after selecting the experimental Previous row.
3. Expire it after 12 seconds and cap every site and record count.
4. Press the real Back/Cancel control once.
5. Repeat from a fresh page and compare caller RVAs and object relationships,
   not absolute ASLR addresses or human timing.

Early probes near plausible menu handlers did not execute. A probe at the UI
event-submission helper did, with two independent samples:

```text
caller RVA: 0x747BEA
RCX: current page + 0x10
RDI: current page
elapsed after arming: 141 ms, then 94 ms
```

PE exception metadata placed the call inside `0x747850..0x747CD0`. Walking to
its only direct caller identified the wrapper beginning at `0x747CD0`. That
wrapper receives the page in `RCX`, retains the complete 64-bit `RDX` action,
constructs the nontrivial event object, submits it to the page queue, and sets
a guard. Its established boundary is:

```cpp
using NativeBackFn = void(__fastcall*)(
    void* page,
    std::uint64_t packed_action);
```

The concrete settings Back method constructed `{3, 0}` and passed the full
payload through the wrapper. ERNativeUI's synthetic Previous therefore uses
the same packed value `3`; it does not call the inner submitter with a guessed
stack object.

**Confirmed:** submission is asynchronous. A later page-frame/job path owns
the fade, pop, parent restoration, and selection state. **Rejected:** fading
the current page or reconstructing the prior rows is equivalent to Back.

## Reproduce the static analysis

Import the exact reference executable with the
[Ghidra workflow](../tools/ghidra-workflow.md), then keep each query bounded.
Useful seeds for this case are:

```text
0x93D730  category panel factory/dispatcher
0x959DF0  Game Options materializer
0x959320  Camera Options materializer
0x95D540  Display materializer
0x959090  Sound materializer
0x95AD00  Network materializer
0x95CB30  Keyboard/Mouse materializer
0x95C050  Graphics materializer
0x95B770  provider-child materializer
0x950850  child-page push
0x747CD0  native Back wrapper
```

For each candidate:

1. export `FunctionContext` and direct callers/callees;
2. compare prologue and call-site bytes with the AOB derivation in
   the current online [`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp);
3. inspect the caller that supplies the page and menu-data arguments;
4. verify the original is called exactly once before an append probe;
5. pair every page identity with its vtable, capacity, and post-vanilla count;
6. exercise focus, callback, Back, reopen, and repeated tab switching; and
7. remove the observation hook after recording the bounded result.

Nearby RVAs and similar prologues are search hints only. A candidate is not
promoted until its argument roles, call order, native object validation, and
live behavior agree.

## Production safety decisions

- Required interfaces use a unique executable-section AOB, or a fallback RVA
  whose bytes are validated before use.
- Relative-call patterns resolve and validate their target; the call-site
  address is not mistaken for the function.
- Each additional built-in destination is optional and independently hooked.
  Failure disables that destination rather than guessing a page layout.
- The original materializer runs once before provider rows are appended.
- Capacity and row count are read only from a validated live page and inside
  the established native scope.
- Previous accepts only a route exactly one slice behind the current one and
  verifies the destination/category/capacity relationship before native Back.
- Structured exception handling is a final diagnostic boundary, never a
  substitute for signature, object, and argument validation.
- Client mods see only stable logical handles and cannot call these functions.

Current production resolution is in
[`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp), hook coordination in
[`src/hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp), compilation in
[`src/menu_compiler.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/menu_compiler.cpp), pagination planning
in [`src/pagination.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/pagination.cpp), and route validation in
[`src/submenu_navigation.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/submenu_navigation.cpp).

## What this does not prove

Appending rows to existing panels does not prove that ERNativeUI can safely
create a new top-level tab. The stock top dialog owns a fixed native category
vector, separate GFX tab placements, a selection controller, cached panel
factories, and paired teardown. Their complete record/icon/factory ownership
and a scalable tab presentation remain API 1.2 research targets.

Likewise, these settings-page results do not establish a universal constructor
for Site of Grace pages or arbitrary game page families. Each family needs its
own construction, focus, event, and retirement evidence.

The lower-level category records, panel factories, and tab dispatcher are
mapped in the [Configuration class map](../architecture/configuration-class-map.md).
The production boundary is summarized in the
[current native hook inventory](../reference/native-hook-inventory.md).
