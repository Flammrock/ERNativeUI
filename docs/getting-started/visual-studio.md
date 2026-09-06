# Hello, Tarnished! with Visual Studio

This guide creates the same client as the CMake walkthrough by using only the
Visual Studio interface. You do not need a `CMakeLists.txt`, package manager,
or ERNativeUI `.lib`.

Complete [Install the SDK](setup.md) first. You need Visual Studio 2022 with
the **Desktop development with C++** workload and an extracted ERNativeUI
1.1.0 or later compatible Windows x64 SDK.

## 1. Create a DLL project

1. Open Visual Studio and select **Create a new project**.
2. Search for and select the C++ **Empty Project** template, then select
   **Next**. Starting empty avoids a generated second `DllMain` and
   precompiled-header requirement.
3. Name the project `HelloTarnished`, choose its location, and create it.
4. Right-click the project, open **Properties -> Configuration Properties ->
   General**, and set **Configuration Type** to **Dynamic Library (.dll)**.
5. In the toolbar, select **Release** and **x64**. If x64 is absent, open
   **Build -> Configuration Manager**, create the x64 platform by copying from
   Win32, then select it.

If you use Visual Studio's **Dynamic-Link Library (DLL)** template instead,
delete its generated `dllmain.cpp` before adding this guide's source. Also set
**C/C++ -> Precompiled Headers -> Precompiled Header** to **Not Using** unless
you deliberately keep and include the generated `pch.h`.

Apply every property below to **Configuration: All Configurations** and
**Platform: x64** unless a step says otherwise.

## 2. Give Visual Studio the two headers

Choose one method.

### Reference the extracted SDK

1. Right-click the project and select **Properties**.
2. Open **C/C++ -> General**.
3. Add this directory to **Additional Include Directories**:

   ```text
   C:\SDK\ERNativeUI-1.1.0-windows-x64\include
   ```

4. Keep `%(AdditionalIncludeDirectories)` in the value so inherited paths are
   preserved.

### Copy the headers into the project

Copy exactly these two files from the SDK:

```text
include\ernativeui\erui.h
include\ernativeui\ERNativeUI.hpp
```

Place them under:

```text
HelloTarnished\third_party\ERNativeUI\include\ernativeui\
```

Then add this parent directory to **C/C++ -> General -> Additional Include
Directories**:

```text
$(ProjectDir)third_party\ERNativeUI\include
```

Keep `erui.h` and `ERNativeUI.hpp` from the same release. The C++ header
includes the C header internally.

## 3. Configure the compiler

In project **Properties**:

1. Open **C/C++ -> Language** and set **C++ Language Standard** to
   **ISO C++17 Standard (/std:c++17)** or newer.
2. Open **C/C++ -> Advanced** and set **Compile As** to **Compile as C++ Code
   (/TP)** if the source extension does not already select C++.
3. Optionally add `WIN32_LEAN_AND_MEAN` and `NOMINMAX` under
   **C/C++ -> Preprocessor -> Preprocessor Definitions**.

Do not add an ERNativeUI file under **Linker -> Input -> Additional
Dependencies**. There is no ERNativeUI import `.lib`; the header-only wrapper
finds the separately loaded host DLL at runtime.

## 4. Add the source

Right-click **Source Files -> Add -> New Item**, create
`hello_tarnished.cpp`, and paste the complete source from
[Add the client source](first-mod.md#2-add-the-client-source).

The worker created by `DllMain` is important. Windows holds the loader lock
while executing `DllMain`; calling `erui::connect()` or registering a menu
directly there can deadlock game startup. Keep `DllMain` limited to capturing
the module handle, disabling thread notifications, and starting the worker.

## 5. Build

1. Confirm the toolbar still shows **Release** and **x64**.
2. Select **Build -> Build Solution**.
3. Open **View -> Output** and confirm the build succeeded.

The output is commonly located at one of these paths, depending on the
solution layout:

```text
<solution>\x64\Release\HelloTarnished.dll
<project>\x64\Release\HelloTarnished.dll
```

Visual Studio prints the exact path in the build output. Deploy the `.dll`, not
the `.exp`, `.ilk`, `.lib`, `.obj`, or `.pdb`. A `.lib` generated beside
your own DLL is your project's output metadata; ERNativeUI does not consume it.

## 6. Deploy and test

Follow [Deploy the host and client](first-mod.md#5-deploy-the-host-and-client),
using the DLL produced by Visual Studio. The important rules are:

- copy runtime files from the SDK's `bin` directory into the Mod Engine 2 mod
  directory;
- load one shared `ERNativeUI.dll` before `HelloTarnished.dll` in
  `external_dlls`;
- never copy the SDK headers into the game directory; and
- launch through Mod Engine 2 with Easy Anti-Cheat disabled.

Then follow [Run the test](first-mod.md#6-run-the-test). The finished row lives
under **System -> Game Options** and opens a native **Hello, Tarnished!**
dialog.

## Updating the SDK later

When referencing the SDK include directory, point the project at the new
release's `include` directory. When vendoring headers, replace both public
headers together. Also deploy a host release that supports the API requested
by those headers.

The Visual Studio project has no `find_package` check, so this matching is your
responsibility. Read [Versioning](../versioning.md) before updating or
publishing the dependency.

Return to the [getting-started index](README.md).
