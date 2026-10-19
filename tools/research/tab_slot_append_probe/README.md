# Native tab-record append proof

This temporary DLL proves one narrowly scoped ownership transition for Elden
Ring 2.7.1.0: while the Configuration top constructor is still populating its
owned fixed vector, copy-construct the stock Graphics record until the vector
contains either nine or ten records.

The research target is selected at configure time and must be `9` or `10`.
The same target is compiled into both the isolated GFX-growth host and the
append probe, preventing mismatched authored geometry and native counts.

Before any append, the early constructor hook resolves every required live
display object from that exact movie:

- target 9: front/back `Item_0_8` and `Item_0_9`;
- target 10: front/back `Item_0_8`, `Item_0_9`, and `Item_0_10`.

Every temporary owning Scaleform value must exist, share the current movie's
validated lineage, have a distinct child payload, and be destroyed exactly.
The native mutation is refused if any proof fails. This is stronger than
retaining raw pointers across System constructions, which would be vulnerable
to allocator address reuse.

The probe does not write the vector count or selector `+0xD0` directly. It
calls the game's validated fixed-vector append helper before the constructor's
first count publication. Elden Ring then obtains the native list count and
passes it through its normal selector-count setter.

For target 10, the two appends are validated independently. After each call,
the probe requires the exact intermediate count, unchanged early selector
state, correct category prefix, and independent copies of the Graphics scalar
and presentation data. Any partial or ambiguous mutation fail-fasts because no
safe ownership rollback has been proven.

The final record sequence is:

```text
target 9:  0,1,2,3,5,6,7,8,8
target 10: 0,1,2,3,5,6,7,8,8,8
```

Late and post-constructor validation additionally requires:

- native count and naturally propagated selector `+0xD0` equal the target;
- exact one-dimensional selector geometry of `target + 1` authored cells;
- selected index in `-1..target-1`;
- the same live movie identity and native class/vtable profile; and
- the complete expected record sequence and owned-copy checkpoints.

## Build

Choose one target and use it for both builds:

```powershell
$target = 10

cmake -S . -B build/gfx-tab-growth-probe -A x64 `
  -DBUILD_TESTING=OFF `
  -DERNATIVEUI_BUILD_HOST=ON `
  -DERNATIVEUI_BUILD_EXAMPLES=OFF `
  -DERNATIVEUI_BUILD_GFX_PATCHER=OFF `
  -DERNATIVEUI_BUILD_GFX_TAB_GROWTH_RESEARCH_HOST=ON `
  -DERNATIVEUI_TAB_SLOT_RESEARCH_TARGET=$target
cmake --build build/gfx-tab-growth-probe --config Release `
  --target ERNativeUIHost

cmake -S . -B build/tab-slot-append-probe -A x64 `
  -DBUILD_TESTING=OFF `
  -DERNATIVEUI_BUILD_HOST=OFF `
  -DERNATIVEUI_BUILD_EXAMPLES=OFF `
  -DERNATIVEUI_BUILD_GFX_PATCHER=OFF `
  -DERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE=ON `
  -DERNATIVEUI_ENABLE_TAB_SLOT_APPEND_RESEARCH_MUTATION=OFF `
  -DERNATIVEUI_TAB_SLOT_RESEARCH_TARGET=$target
cmake --build build/tab-slot-append-probe --config Release `
  --target ERNativeUITabSlotAppendProbe
```

The default probe is observation-only. After its same-construction live-cell
evidence is accepted, build mutation in a separate tree by changing only:

```powershell
-DERNATIVEUI_ENABLE_TAB_SLOT_APPEND_RESEARCH_MUTATION=ON
```

## Controlled run

Keep the research host's public setting at eight:

```ini
[Logging]
EnableLog = 1

[Tabs]
MinimumTabSlots = 8

[Diagnostics]
EnableDiagnostics = 0
```

Load only the target-matched research host and append probe:

```toml
external_dlls = [
    "mod\\ERNativeUIGfxTabGrowthProbe\\ERNativeUI.dll",
    "mod\\ERNativeUITabSlotAppendProbe\\ERNativeUITabSlotAppendProbe.dll",
]
```

Do not load NativeTweaks, a client DLL, another ERNativeUI host, a loose
Configuration GFX, or another tab probe. The fixed probe log is written beside
the DLL as `ERNativeUITabSlotAppendProbe.log`.

An observation-only success retains native count and `D0` at eight while
proving all target cells and exact `target + 1` geometry. A mutation success
must report the selected target for native count and `D0`, plus one append
checkpoint for target 9 or two for target 10.

After mutation succeeds, traverse every position one press at a time, wrap in
both directions, select the final position with the mouse, close with Back,
and repeat the open/close cycle. The added entries are duplicate Graphics
carriers; custom tabs are intentionally absent from this fixture.

Stop immediately on a black screen, frozen navigation, missing Back action,
or process termination. This is a research fixture, not a redistributable
runtime dependency.
