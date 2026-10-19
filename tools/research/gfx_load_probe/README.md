# Scaleform GFX-load research probe

`ERNativeUIGfxLoadProbe.dll` is a temporary research DLL for Elden Ring
2.7.1.0. It locates `CS::CSScaleformFileOpener::OpenFile` and observes these
two exact movie paths:

- `data0:/menu/win/02_040_optionsetting.gfx`
- `data0:/menu/win/02_042_pc_graphicsetting.gfx`

The current third-stage probe can also substitute two exact, prevalidated GFX
sidecars after the game's original `OpenFile` implementation returns. This is
an experiment that proves the file-open seam and `Scaleform::MemoryFile`
lifetime behavior. It is not the production runtime transformer.

In particular, the detour does not parse or transform GFX data. Any required
changes must already exist in the replacement sidecars before the game starts.

## Safety model

The probe always calls the original `OpenFile` function first. For a matching
target, it then validates the returned object's exact known
`Scaleform::MemoryFile` vtable, virtual slots, reference count, validity flag,
cursor, byte pointer, and length.

The observation path copies at most 4 MiB from the borrowed byte view into a
probe-owned static buffer. It snapshots the object metadata before and after
the copy and accepts the capture only when both snapshots match. A worker
thread writes the independent copy beside the probe for offline inspection.

Substitution is more restrictive:

- Both replacement sidecars must be available and must match their exact
  expected physical length, declared GFX length, and FNV-1a identity.
- The effective source returned by the game must match the exact stock source
  length and FNV-1a identity established by the capture stage.
- The returned object must still match the validated `MemoryFile` state and
  the captured source pointer and length.
- The probe changes only the `MemoryFile` byte pointer, length, and cursor view
  after the original call. It does not invoke a Scaleform file method, replace
  the file object, or change its ownership or reference count.
- Replacement bytes live in fixed, process-lifetime static buffers, so the
  substituted pointer remains valid for the rest of the process.

If either sidecar is missing or invalid, substitution is disabled for both
targets. If any source or object validation fails, that target remains on the
original bytes. Observation and capture continue in either case. This
fail-open behavior keeps an unavailable or mismatched research replacement
from blocking the game's normal file load.

The probe supports only the validated Elden Ring 2.7.1.0 executable profile:
its PE timestamp, image size, method entry, vtable slot, `MemoryFile` vtable,
and known virtual slots are checked before use. The known 2.7.0.0 method RVA is
not sufficient evidence for support, so the probe deliberately refuses other
profiles.

## Build

Configure an x64 MSVC build with:

```text
-DERNATIVEUI_BUILD_GFX_LOAD_RESEARCH_PROBE=ON
```

Build the `ERNativeUIGfxLoadProbe` target and load the resulting DLL through
the same DLL loader used for ERNativeUI. The probe writes
`ERNativeUIGfxLoadProbe.log` beside its DLL.

## Replacement sidecars

To enable the substitution stage, put these two files beside the probe DLL:

```text
ERNativeUIGfxLoadProbe-replacement-02_040_optionsetting.gfx
ERNativeUIGfxLoadProbe-replacement-02_042_pc_graphicsetting.gfx
```

This build accepts only the canonical replacement outputs whose identities
are compiled into the probe. A differently patched file, even if it is a
valid GFX movie, is rejected. When both files pass validation, the log reports
`Live substitution mode: enabled`; otherwise it reports that substitution is
disabled and leaves the original views untouched.

Loose `menu/win` overrides should not be enabled during this test. Otherwise
their effects cannot be distinguished from successful pointer substitution.

## Controlled test run

1. Confirm Elden Ring is closed.
2. Place the probe DLL and both exact replacement sidecars together in the
   configured mod directory.
3. Start the game from a fresh process. The hook must be installed before the
   first System-menu load because Scaleform may cache the resulting movie.
4. Open System once and inspect Game Options and Camera Options.
5. Visit Graphics and open Advanced Settings so the PC graphics movie is
   requested if it loads lazily.
6. Exercise the custom-tab strip and the controls whose presentation is
   supplied by the replacements, including TextInput and ColorPicker where
   available.
7. Close System, reopen it, and repeat a short navigation pass to check cached
   movie behavior and lifetime stability.
8. Exit the game normally and inspect `ERNativeUIGfxLoadProbe.log`.

For each successful target, the log should contain a validated capture and a
`substitution[...] APPLIED` record. Captures of the original effective input
are written as:

```text
ERNativeUIGfxLoadProbe-capture-02_040_optionsetting.gfx
ERNativeUIGfxLoadProbe-capture-02_042_pc_graphicsetting.gfx
```

The log also records the effective path, call-site RVA, open flags, returned
object, dynamic vtable, the first twelve vtable entry RVAs, validated
`MemoryFile` metadata, GFX header, declared length, and diagnostic FNV-1a hash.

Reopening System may produce no second `OpenFile` call because Scaleform can
reuse its cached movie definition. That is expected lifecycle evidence, not a
probe failure.
