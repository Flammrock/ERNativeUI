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
  or a later compatible 1.x release; and
- the ERNativeUI 1.2.0 SDK builds an API 1.2 client, which requires host 1.2.0
  or a later compatible 1.x release; and
- the ERNativeUI 1.3.0 SDK builds an API 1.3 client, which requires host 1.3.0
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

The headers selected at build time determine the exact ERUI API requested by
the client. Features documented as API 1.3, including custom tabs and
title-screen text, require the ERNativeUI 1.3.0 SDK or a later compatible 1.x
SDK retaining API 1.3. When intentionally targeting an older API, use the
headers and documentation from that SDK's release tag and put its release
version in your build configuration.

## 2. Extract the SDK

Extract the archive to a stable path with no files overwritten by Mod Engine
2. To keep the commands independent from the package-release number, this
guide assumes you renamed the extracted SDK directory to:

```text
C:\SDK\ERNativeUI-current
```

The directory you will give to CMake is the one whose direct children are
`bin`, `include`, `lib`, and `share`:

```text
C:\SDK\ERNativeUI-current\
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
        |-- DOCUMENTATION.md
        |-- examples\
        `-- licenses\
```

`DOCUMENTATION.md` links to the documentation stored under this SDK's exact
GitHub release tag. The screenshot-heavy documentation tree is not duplicated
inside the binary archive.

If extracting the ZIP creates one additional outer directory, open it and use
the inner directory that actually contains those four children.

## 3. Choose how the client consumes the headers

There are two supported approaches. They produce the same kind of client DLL.

### Option A: let CMake find the extracted SDK

Your `CMakeLists.txt` uses:

```cmake
find_package(ERNativeUI CONFIG REQUIRED)
target_link_libraries(MyMod PRIVATE ERNativeUI::SDK)
```

`find_package` searches for `ERNativeUIConfig.cmake`. It cannot discover a ZIP
in Downloads automatically, so you must tell CMake where the extracted SDK is.
Use either of these methods:

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:\SDK\ERNativeUI-current"
```

or the more specific package-directory form:

```bat
cmake -S . -B build -A x64 -DERNativeUI_DIR="C:\SDK\ERNativeUI-current\lib\cmake\ERNativeUI"
```

- `CMAKE_PREFIX_PATH` points at the SDK root containing `include` and `lib`.
- `ERNativeUI_DIR` points directly at the directory containing
  `ERNativeUIConfig.cmake`.
- You need only one of them.

After CMake finds the package, `ERNativeUI::SDK` adds the public include path
and C++17 requirement to your target. `target_link_libraries` is standard
CMake spelling for consuming a target; here that target is header-only. No
`.lib` is linked and no copy of the host is built into your DLL.

The current source walkthrough uses the API 1.4 pre-release headers. API
1.4 is not frozen or published yet, so do not invent a permanent minimum
package version for it. During development, resolve the matching installed SDK:

```cmake
find_package(ERNativeUI CONFIG REQUIRED)
```

After API 1.4 is published, use its first release as the minimum package
version. That version is a package constraint, not a runtime API selector;
avoid `EXACT` unless a reproducible build deliberately requires one precise
SDK package.

A lower `find_package` minimum does not ask a newer SDK to expose its older
header. If a mod intentionally targets API 1.0, make CMake resolve the actual
1.0.0 SDK—preferably by pointing `ERNativeUI_DIR` directly at that extracted
package—or copy the two 1.0 headers into the project. An `EXACT` package
requirement is also reasonable when reproducibly pinning that older SDK. The
client may still run with a later compatible 1.x host; only its build headers
are pinned.

The headers then request their exact ERUI API contract from the loaded host at
game runtime. The current source headers request API 1.4; frozen SDK headers
continue to request their published 1.0, 1.1, 1.2, or 1.3 contract. That is a
separate check from CMake package selection. See
[Versioning](../versioning.md) for the complete distinction.

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
