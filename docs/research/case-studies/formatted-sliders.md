# Numeric and custom slider values

This case study records how ERNativeUI established that Elden Ring's native
slider can safely present signed decimals, units, prefixes, and arbitrary
per-position labels. The public controls are documented in
[Numeric Slider](../../guides/controls/numeric-slider.md) and
[Custom Slider](../../guides/controls/custom-slider.md).

The names in this document are analytical labels. RVAs and object offsets are
evidence for the named executables, not part of the ERUI ABI.

## Reference builds

The relevant native refresh remained at the same RVA in both validated
executables:

| Property | Elden Ring 2.7.0.0 | Elden Ring 2.7.1.0 |
|---|---:|---:|
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` | `1A3547101327F65D0C76DA2F9190AC0AA66871EA42BAE2AECC61E11A8B597891` |
| PE timestamp | `0x69E9C9B9` | `0x6A96B418` |
| Slider refresh RVA | `0x74FA20` | `0x74FA20` |

Production still derives or validates the boundary at runtime. Equal RVAs in
two builds do not make an address universal.

## Starting observation

The stock public Slider exposed by API 1.0 stores one `uint8_t` and Elden Ring
renders that byte as an integer. A compatible third-party menu demonstrated
native rows displaying values such as `x0.6` and `3.0s`, which proved that the
visual result was possible without replacing the slider bar.

Static clean-room inspection of the user-supplied third-party binary showed a
useful distinction: it retained an index-like byte backing value and replaced
only the displayed text after the native refresh. No third-party source or
binary material is included in ERNativeUI.

That suggested two independent questions:

1. Which native function refreshes the slider value text?
2. Can ERNativeUI identify only its own rows at that boundary?

## Recovering `CS::SliderCtrl`

RTTI and vtable analysis identified the `CS::SliderCtrl` family. Vtable slot
13 points to RVA `0x74FA20` on both reference builds. Decompilation and bounded
live observation established this effective shape:

```cpp
void slider_refresh(SliderCtrl* self);
```

The refresh:

1. compares the current and baseline values;
2. applies the native Normal or Modified state;
3. resolves `Text_0` from the row's Scaleform movie context and writes the
   current integer;
4. resolves `Text_1` and writes the baseline integer; and
5. updates the slider geometry.

The relevant reference-build fields are:

| Offset | Observed role |
|---:|---|
| `+0xA38` | Scaleform movie context used for `Text_0` and `Text_1` |
| `+0xD28` | Current native position |
| `+0xD2C` | Baseline/default native position |
| `+0xD30` | Minimum native position |
| `+0xD34` | Maximum native position |
| `+0xD38` | Native position step |
| `+0xD90` | Pointer to the externally owned backing byte |

**Confirmed:** the bar geometry and the visible value are separate effects of
the same refresh. Calling the original first and then replacing `Text_0` and
`Text_1` preserves the native bar, arrows, selection, and Modified state.

## The construction-time identity problem

The existing native AddSlider path creates a
`PropertySliderController<unsigned char>` containing the `SliderCtrl`. Its
constructor performs an initial refresh before the outer property controller
stores the external backing-byte pointer at `SliderCtrl + 0xD90`.

Matching only that pointer therefore recognizes every later refresh but misses
the first visible text assignment.

The probe solved this with two bounded identities:

- during one ERNativeUI-owned AddSlider call, thread-local state names the
  exact descriptor currently being constructed; and
- after construction, the immutable backing-byte pointer maps the native
  object to its compiled ERNativeUI row.

Unrelated sliders match neither identity and receive only the untouched
original refresh.

## Controlled probe

A temporary provider page registered three ordinary native sliders:

| Logical mapping | Native positions | Expected initial text |
|---|---:|---|
| `index / 10`, prefix `x` | `0..20` | `x0.6` |
| `(index - 10) / 10` | `0..20` | `-0.5` |
| `index / 10`, suffix `s` | `0..100` | `3.0s` |

The refresh detour called the original exactly once, matched only an owned
probe row, produced bounded UTF-16 text without allocation, and replaced both
native text members. The in-game test confirmed:

- decimal steps updated correctly in both directions;
- the signed slider crossed `-0.1`, `0.0`, and `0.1` correctly;
- prefix and suffix text remained attached;
- closing and reopening the page retained the formatted presentation; and
- ordinary sliders were unchanged.

This promoted the presentation boundary from a research probe to an API 1.2
implementation candidate.

## Why the public model is index-backed

The native backing value still has only 256 possible byte states. ERNativeUI
therefore treats it as a private position, not as the client-facing number.

For Numeric Slider, exact base-10 integers describe an arithmetic sequence:

```text
logical scaled value = minimum scaled value + index * scaled step
displayed value      = scaled value / 10^decimal_places
```

For Custom Slider, the same logical index selects one copied UTF-16 label.
The original formatter never runs during a native refresh.

This design provides three safety properties:

- no floating-point drift accumulates while the player moves left or right;
- no client callback or allocator runs on Elden Ring's UI thread; and
- the public index is independent of the private byte implementation.

The host copies every formatted value into its own immutable storage before
installing the refresh detour. That storage and the disabled SafetyHook
trampoline are retained until process exit: a UI thread may already have
branched into the detour when the target entry is restored, so freeing either
object during teardown would create a narrow use-after-free window. This
matches ERNativeUI's pinned-host and no-hot-unload lifetime contract.

The 2-through-256 position limit follows from the proven native backing. It is
not an arbitrary wrapper limit.

## API 1.4 live table replacement

API 1.4 reuses the same owned-row identity to replace a Numeric Slider's or
Custom Slider's complete display table after registration. The host first
validates and copies a same-cardinality table, stages a new immutable bank,
and publishes that bank with a dedicated per-row repaint generation at the
shared UI update boundary. Logical numeric values and custom indices do not
change.

A currently materialized row does not wait for player movement. On the next
safe UI frame, row presentation revalidates the materializer-owned embedded
`SliderCtrl`, its vtable/movie identity, and its exact backing-byte pointer.
It then calls the retained original refresh trampoline once and reapplies
`Text_0` and `Text_1` from the same retained immutable bank. The bridge checks
the backing pointer again after the original returns. A row binding consumes
each repaint generation once; bounded retries cover a controller that is not
yet safe to address.

This does not introduce a second long-lived native pointer cache. The only
controller identity comes from the existing physical-row binding and is
validated for the call. Repaint authorization is also independent of the
optional availability color-transform attachment, so the separately
negotiated row-configuration capability does not depend on disabled styling.

### API 1.4 live validation

On 2026-10-28, the focused Tarnished UI Showcase scene was confirmed working
on executable 2.7.2.0 after the explicit refresh was deployed. Both sliders'
selected-value labels now change without moving their values. The Release
build passed all 99 tests, and the deployed host matched the tested artifact's
SHA-256.

The final runtime session logged one `Formatted-slider native range
validation failed` warning (`fault=1`, `seh=0x00000000`). This existing guard
returns before assigning either text when the native range/current/default
fields do not match the owned label table. No explicit UI-thread repaint
failure or exception was logged. The warning does not include the sampled
range or construction context, so its exact trigger cannot be established
from that log. Retain the guard and investigate this observation during the
final diagnostic review before stable publication; the visual confirmation
alone does not prove the warning harmless.

## Reproduction procedure

To revalidate this boundary after a game update:

1. Locate `CS::SliderCtrl` through its RTTI/vtable family rather than assuming
   the historical RVA.
2. Identify the slot that references both `Text_0` and `Text_1`, compare its
   control-flow and field accesses with the table above, and record any drift.
3. Confirm the AddSlider construction order and specifically whether the
   external backing pointer is still assigned after the constructor's initial
   refresh.
4. Install a bounded original-first probe for one uniquely owned row.
5. Log only the first observation for each owned object/value; do not log
   every frame.
6. Exercise negative, zero, positive, prefixed, suffixed, and arbitrary text
   mappings; close and reopen the page.
7. Verify an ordinary game slider and a byte-valued ERNativeUI Slider remain
   unchanged.
8. Test hook chaining separately with every supported mod that detours the
   same refresh function.

Do not promote a nearby match merely because it writes a Scaleform text field.
The function must also preserve the slider state, geometry, current/baseline
semantics, construction ordering, and owned-row isolation established here.

## Public boundary

API 1.2 exposes two separate controls because their client contracts differ:

- Numeric Slider accepts an exact arithmetic range through a friendly C++
  decimal wrapper and reports the number plus its logical index.
- Custom Slider accepts all display strings during registration and later
  reports only the selected logical index.

Both append new C function-table entries after the frozen API 1.1 prefix.
Clients compiled against API 1.0 or 1.1 never observe or depend on this hook.

Return to the [research index](../README.md).
