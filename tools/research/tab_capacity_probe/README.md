# Live Configuration-tab capacity probe

`ERNativeUITabCapacityProbe.dll` is an isolated, observation-only research DLL
for Elden Ring 2.7.1.0. It answers one narrow question: how many physical tab
cells did the live Scaleform selector discover from the GFX that was actually
loaded?

The probe does not register tabs, rewrite category records, alter selection,
patch GFX bytes, or use ERNativeUI's production logger. It installs only:

- a mandatory post-original observer on the top-dialog builder at `0x807430`;
- an optional post-original observer on the selected-index getter at
  `0x73AC70`, but only when that entry is still pristine.

It deliberately does **not** hook category append, tab dispatch, selection or
`BackTabList` callbacks, the composite frame, or any production ERNativeUI
seam. The optional getter is allowed to fail without disabling the mandatory
constructor snapshot.

## What the fields mean

The snapshot keeps two different counts separate:

| Log field | Native field | Meaning |
|---|---:|---|
| `categoryRecords` | top `+0x1760` | Number of constructed category records. |
| `activeSelectable(D0)` | selector `+0xD0` | Number of records selectable by the native selector. |
| `selected(D4)` | selector `+0xD4` | Current selected physical record index. |
| `discoveredTrailingExtent(D8)` | selector `+0xD8` | Horizontal/inner extent: the trailing `N` in `Item_0_N`, plus one. |
| `discoveredLeadingExtent(DC)` | selector `+0xDC` | Vertical/outer extent: the leading index in `Item_0_N`, plus one. |

During construction, native function `0x738AB0` discovers display children by
resolving `Item_%d_%d` through the selector's display adapter. Static call-flow
recovery establishes `D8` as the trailing/horizontal extent and `DC` as the
leading/vertical extent. The probe still records both raw fields and uses the
orientation-neutral product `D8 * DC`, printed as `discoveredCells`, as the
capacity invariant. The probe also validates that the live adapter still
routes the lookup through `0x737E10` before accepting the snapshot.

`activeSelectable(D0)` is **not** authored GFX capacity. In particular, the
extra `Item_0_N` child seen in both known movies can exist while remaining
outside the selectable record bound. The probe reports the difference as
`spareDiscoveredCells`; it does not label that spare as a transition or staging
host because that role is not yet proven.

## Build

Use an x64 MSVC-compatible build:

```powershell
cmake -S . -B build-tab-capacity -A x64 `
  -DBUILD_TESTING=OFF `
  -DERNATIVEUI_BUILD_HOST=OFF `
  -DERNATIVEUI_BUILD_EXAMPLES=OFF `
  -DERNATIVEUI_BUILD_GFX_PATCHER=OFF `
  -DERNATIVEUI_BUILD_TAB_CAPACITY_RESEARCH_PROBE=ON

cmake --build build-tab-capacity --config Release `
  --target ERNativeUITabCapacityProbe
```

The output is:

```text
build-tab-capacity/Release/ERNativeUITabCapacityProbe.dll
```

## Load order

Always list the probe **last**, after the host and client DLLs belonging to the
chosen matrix case. Its worker waits briefly, logs the loaded module paths, and
then requires its mandatory builder entry to remain pristine.

Never load the canonical `ERNativeUI.dll` and NativeTweaks' bundled/private
`ERNativeUI.dll` in the same process. The third matrix case must use exactly the
host shipped for that NativeTweaks fixture.

The fixed output file is created beside the probe DLL and overwritten on each
launch:

```text
ERNativeUITabCapacityProbe.log
```

The first lines include absolute paths for the log, probe,
`ERNativeUI.dll` (when loaded), and `NativeTweaks.dll` (when loaded). Wait for
`probe ready` before opening System.

## Exact test matrix

Keep each run separate and preserve its log under a descriptive name before
starting the next run.

Disable every older tab probe and the stress/showcase clients for all three
runs; those DLLs would either claim research seams or change the active record
count.

### A. Vanilla baseline: probe only

1. Remove or rename every loose override of
   `menu/win/02_040_optionsetting.gfx`.
2. Do not load ERNativeUI, NativeTweaks, or another Configuration-menu mod.
3. Load only `ERNativeUITabCapacityProbe.dll`.
4. Wait for `probe ready`, open System, visit every tab one step at a time,
   hold R1 through the end, hold L1 back to the beginning, close System, and
   repeat twice.

```toml
external_dlls = [
    "mod\\ERNativeUITabCapacityProbe\\ERNativeUITabCapacityProbe.dll",
]

mods = []
```

Expected construction evidence:

```text
categoryRecords=8
activeSelectable(D0)=8
discoveredTrailingExtent(D8)=9
discoveredLeadingExtent(DC)=1
discoveredCells=9
spareDiscoveredCells=1
```

Natural getter observations, when available, should remain in `0..7`.

### B. Modified GFX only: probe plus asset root

1. Install only NativeTweaks' modified
   `menu/win/02_040_optionsetting.gfx` in the active mod asset root.
2. Do not load NativeTweaks.dll or any ERNativeUI host DLL.
3. Load the probe last (it will be the only DLL in this case).
4. Repeat the same controller, mouse, close, and reopen sequence.

```toml
external_dlls = [
    "mod\\ERNativeUITabCapacityProbe\\ERNativeUITabCapacityProbe.dll",
]

mods = [
    { enabled = true, name = "NativeTweaks GFX only", path = "mod\\NativeTweaks" },
]
```

Mod Engine's loose-file root does not execute DLLs merely because they are
present inside that directory, so this case isolates the asset override.

Expected construction evidence:

```text
categoryRecords=8
activeSelectable(D0)=8
discoveredTrailingExtent(D8)=10
discoveredLeadingExtent(DC)=1
discoveredCells=10
spareDiscoveredCells=2
```

This run is the decisive separation between category count and authored live
capacity: only the loaded GFX changes.

### C. Full NativeTweaks fixture: bundled host, client, then probe

1. Use the exact validated NativeTweaks fixture, including its modified GFX.
2. Load NativeTweaks' bundled/private `ERNativeUI.dll` first.
3. Load `NativeTweaks.dll` second.
4. Load `ERNativeUITabCapacityProbe.dll` last.
5. Do **not** load the canonical ERNativeUI host in this run.
6. Repeat the same controller, mouse, close, and reopen sequence.

```toml
external_dlls = [
    "mod\\NativeTweaks\\ERNativeUI.dll",
    "mod\\NativeTweaks\\NativeTweaks.dll",
    "mod\\ERNativeUITabCapacityProbe\\ERNativeUITabCapacityProbe.dll",
]

mods = [
    { enabled = true, name = "NativeTweaks", path = "mod\\NativeTweaks" },
]
```

Expected construction evidence:

```text
categoryRecords=9
activeSelectable(D0)=9
discoveredTrailingExtent(D8)=10
discoveredLeadingExtent(DC)=1
discoveredCells=10
spareDiscoveredCells=1
```

Natural getter observations, when available, should remain in `0..8`.

## Acceptance checks

For every post-constructor snapshot:

- `nativeTypesValid=1` and `recordsValid=1`;
- `recordCountEqualsActive=1`;
- `selectedBoundedByActive=1`;
- `activeFitsDiscoveredGrid=1`;
- `singletonAxis=1`;
- the discovered dimensions change only when the loaded GFX changes;
- the active/selectable count changes only when a ninth record is registered;
- close and reopen produce a fresh `generation` and a fresh snapshot;
- no observed selected index reaches the nonselectable spare cell.

If the optional getter entry is already claimed, the log contains a warning
and the constructor snapshots remain valid. If the mandatory builder entry is
claimed or the executable identity differs, the probe fails closed and installs
no hooks.
