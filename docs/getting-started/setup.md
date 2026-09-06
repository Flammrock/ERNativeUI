# Install the ERNativeUI SDK

This guide prepares the files used by both the CMake and Visual Studio paths.
It is for mod authors building a client DLL. Players installing an existing
mod should instead use the
[player installation instructions](player-installation.md).

## Requirements

You need:

- 64-bit Windows;
- Elden Ring for Steam;
- Mod Engine 2 configured to launch the game offline with Easy Anti-Cheat
  disabled;
- a 64-bit C++ compiler with C++17 support; and
- the `ERNativeUI-X.Y.Z-windows-x64.zip` SDK from GitHub Releases.

For the CMake path, also install:

- CMake 3.24 or newer; and
- Visual Studio 2022 Build Tools, Visual Studio 2022, or another supported x64
  toolchain.

For the Visual Studio-only path, install Visual Studio 2022 with the
**Desktop development with C++** workload. You do not need to install CMake
for that path.

You do **not** need Ghidra, FFDec, an ERNativeUI source checkout, or an import
library to build an ordinary client mod.

## 1. Download the correct release asset

Open [ERNativeUI Releases](https://github.com/Flammrock/ERNativeUI/releases),
choose the SDK whose API and features you want to build against, and download:

```text
ERNativeUI-X.Y.Z-windows-x64.zip
```

You do not have to choose the newest SDK. An older SDK deliberately gives the
client its older API surface and usually lets it run with more host releases;
a newer SDK gives the client newer features and therefore raises its minimum
host requirement. For example:

- the ERNativeUI 1.0.0 SDK builds an API 1.0 client, which works with the 1.0.0
  host and later compatible 1.x hosts; and
- the ERNativeUI 1.1.0 SDK builds an API 1.1 client, which requires host 1.1.0
  or a later compatible 1.x release.

Within one release major, newer hosts retain every earlier API from the same
major line. In general, a host release `X.Y.Z` supports every already-published
ERUI API `X.W` introduced by that release or an earlier `X.*.*` release. A
future host with a different release major may drop the earlier API-major line
unless its release documentation explicitly preserves it. Release and API
numbers are still separate concepts; read [Versioning](../versioning.md) for
the complete policy.

Do not download the Nexus archive for development. It is a compact player
package and does not contain the complete SDK.

The remaining examples in the current documentation use the 1.1.0 SDK because
they teach the current API 1.1 surface. When intentionally targeting an older
API, use the headers and documentation from that SDK's release tag and put its
release version in your build configuration.

## 2. Extract the SDK

Extract the archive to a stable path with no files overwritten by Mod Engine
2. This guide uses:

```text
C:\SDK\ERNativeUI-1.1.0-windows-x64
```

The directory you will give to CMake is the one whose direct children are
`bin`, `include`, `lib`, and `share`:

```text
C:\SDK\ERNativeUI-1.1.0-windows-x64\
|-- bin\
|   |-- ERNativeUI.dll
|   |-- ERNativeUI.ini
|   |-- locales\
|   |-- menu\
|   `-- examples\
|-- include\
|   `-- ernativeui\
|       |-- erui.h
|       `-- ERNativeUI.hpp
|-- lib\
|   `-- cmake\
|       `-- ERNativeUI\
|           |-- ERNativeUIConfig.cmake
|           |-- ERNativeUIConfigVersion.cmake
|           `-- ERNativeUITargets.cmake
`-- share\
    `-- ERNativeUI\
        |-- docs\
        |-- examples\
        `-- licenses\
```

If extracting the ZIP creates one additional outer directory, open it and use
the inner directory that actually contains those four children.

## 3. Choose how the client consumes the headers

There are two supported approaches. They produce the same kind of client DLL.

### Option A: let CMake find the extracted SDK

Your `CMakeLists.txt` uses:

```cmake
find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)
target_link_libraries(MyMod PRIVATE ERNativeUI::SDK)
```

`find_package` searches for `ERNativeUIConfig.cmake`. It cannot discover a ZIP
in Downloads automatically, so you must tell CMake where the extracted SDK is.
Use either of these methods:

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:\SDK\ERNativeUI-1.1.0-windows-x64"
```

or the more specific package-directory form:

```bat
cmake -S . -B build -A x64 -DERNativeUI_DIR="C:\SDK\ERNativeUI-1.1.0-windows-x64\lib\cmake\ERNativeUI"
```

- `CMAKE_PREFIX_PATH` points at the SDK root containing `include` and `lib`.
- `ERNativeUI_DIR` points directly at the directory containing
  `ERNativeUIConfig.cmake`.
- You need only one of them.

After CMake finds the package, `ERNativeUI::SDK` adds the public include path
and C++17 requirement to your target. `target_link_libraries` is standard
CMake spelling for consuming a target; here that target is header-only. No
`.lib` is linked and no copy of the host is built into your DLL.

The `1.1.0` in `find_package(ERNativeUI 1.1.0 ...)` is a minimum
**ERNativeUI release/package version**, not the ERUI runtime API number. With
the package's `SameMajorVersion` policy, it means “at least release 1.1.0,
within release major 1.” For example, a compatible 1.1.2 or 1.2.0 package may
satisfy it, while 1.0.0 and 2.0.0 do not. It is usually better than requiring
exactly 1.1.0 because it accepts compatible fixes. There is deliberately no
`EXACT` keyword in the recommended command.

A lower `find_package` minimum does not ask a newer SDK to expose its older
header. If a mod intentionally targets API 1.0, make CMake resolve the actual
1.0.0 SDK—preferably by pointing `ERNativeUI_DIR` directly at that extracted
package—or copy the two 1.0 headers into the project. An `EXACT` package
requirement is also reasonable when reproducibly pinning that older SDK. The
client may still run with a later compatible 1.x host; only its build headers
are pinned.

The headers then request ERUI API 1.1 from the loaded host at game runtime.
That is a separate check. See [Versioning](../versioning.md) for the complete
distinction.

### Option B: copy only the two public headers

Projects that do not use CMake package discovery may copy:

```text
include\ernativeui\erui.h
include\ernativeui\ERNativeUI.hpp
```

Keep the directory name and both files together, for example:

```text
HelloTarnished\
`-- third_party\
    `-- ERNativeUI\
        `-- include\
            `-- ernativeui\
                |-- erui.h
                `-- ERNativeUI.hpp
```

Add `third_party\ERNativeUI\include` to your compiler's include directories,
then use:

```cpp
#include <ernativeui/ERNativeUI.hpp>
```

The `.hpp` is the C++17 wrapper and internally includes the strict-C `erui.h`.
A pure-C client includes only `erui.h`. Copy the two files from the same SDK
release; never mix header revisions.

## 4. Keep SDK files and runtime files separate

The extracted directory is your development SDK. For an in-game test, copy
runtime files from its `bin` directory into the Mod Engine 2 mod directory.
Do not configure `find_package` to search inside the Mod Engine 2 directory,
and do not load `ERNativeUI.dll` from the SDK directory.

Continue with one of these complete project guides:

- [Hello, Tarnished! with CMake](first-mod.md)
- [Hello, Tarnished! with Visual Studio](visual-studio.md)

Return to the [getting-started index](README.md).
