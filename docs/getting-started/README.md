# Getting started for mod authors

This section takes you from the GitHub release page to a working
**Hello, Tarnished!** client DLL. No ERNativeUI source build is required.

ERNativeUI has two parts:

- `ERNativeUI.dll` is the shared host loaded once by Mod Engine 2; and
- your mod is a separate client DLL that includes two public headers and asks
  the host to create native UI.

Your client never links an ERNativeUI `.lib`. The public interface is a stable
C ABI with a header-only C++17 wrapper, so MSVC, clang-cl, and MinGW-w64
clients can use the same host.

## Choose your path

### CMake project

1. [Install the SDK](setup.md).
2. [Build Hello, Tarnished! with CMake](first-mod.md).
3. Deploy both DLLs and test the button in Elden Ring.

This is the recommended path for a new cross-editor project. CMake's
`find_package` validates the SDK release and supplies its include directory.

### Visual Studio project without CMake

1. [Install the SDK](setup.md).
2. Follow the dedicated
   [Visual Studio setup guide](visual-studio.md).

Visual Studio only needs the two headers. You may reference their SDK include
directory or copy them into the project.

## After the first button works

- Start a real mod from the fully commented
  [client-mod template](../../examples/template/README.md).
- Browse the [mod-author guides](../guides/README.md) for pages, controls,
  dialogs, localization, bindings, and storage.
- Use [common errors](common-errors.md) if the DLL builds but no menu appears.
- Read [Versioning](../versioning.md) before declaring the SDK and runtime
  requirement for a release.

This documentation describes public API 1.1. API versions and ERNativeUI
release versions are independent, even though the first release containing
API 1.1 is ERNativeUI 1.1.0.
