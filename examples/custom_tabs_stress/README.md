# Custom Tabs Stress

`CustomTabsStress.dll` is a focused API 1.3 integration example. It registers:

- exactly 50 custom tabs as a stable stress workload;
- one deterministic numbered 32x32 RGBA8 icon with a fully
  transparent background per tab; and
- 20 independent slider rows per tab, for 1,000 rows in total.

The row count deliberately exercises ERNativeUI's automatic pagination. The
example never assumes a particular native GFX capacity. Its tab count is not
derived from ERNativeUI's process-wide maximum, so raising that limit does not
silently make this example heavier.

## Important: run this example alone

Custom-tab capacity is shared by every ERNativeUI provider in the process; it
is not a per-mod allowance. This example deliberately requests 50 slots and is
meant for repeatable stress testing, screenshots, and implementation study. Do
not load another custom-tab client at the same time. If fewer than 50 slots
remain, this provider's transactional registration fails instead of publishing
a partial menu.

For an ordinary mod, start with the
[Custom Configuration tabs guide](../../docs/guides/custom-tabs.md) and the
[client-mod template](../template/README.md), then register only the tabs the
feature genuinely needs.

## Ordering and diagnostics

The provider-scoped keys are:

```text
ernativeui.example.custom-tabs-stress/tab-01
...
ernativeui.example.custom-tabs-stress/tab-50
```

Each tab supplies a monotonically increasing preferred order. A player may
override individual positions in `ERNativeUI.ini`, for example:

```ini
[TabOrder]
ernativeui.example.custom-tabs-stress/tab-25 = 1
ernativeui.example.custom-tabs-stress/tab-01 = 500
```

When `[Logging] EnableLog = 1`, `ERNativeUI-tabs.log` reports the resolved order
and any invalid or stale override. ERNativeUI does not create or update this
file while logging is disabled; an older report may therefore remain on disk.

## Build inside the ERNativeUI tree

From the repository root, configure with examples enabled and then build only
this target:

```bat
cmake -S . -B build -A x64 -DERNATIVEUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target CustomTabsStress
```

The staged DLL is written to:

```text
build\deploy\Release\examples\CustomTabsStress.dll
```

## Build against an installed SDK

The standalone project intentionally uses `find_package(ERNativeUI CONFIG
REQUIRED)` without a package-version constraint. The SDK package release and
the negotiated runtime API are separate versions; compilation requires the API
1.3 declarations, and the C++ wrapper rejects a host that cannot negotiate the
complete API 1.3 table.

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\sdk\ERNativeUI
cmake --build build --config Release
```

Load the canonical host before this client:

```toml
[modengine]
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\CustomTabsStress.dll",
]
```
