# ABI stability contract

`include/ernativeui/erui.h` is the binary contract. It is valid C and C++.
`ERNativeUI.hpp` is a source-only C++17 wrapper; its classes never cross the
DLL boundary.

## Stable rules

- Windows x64, natural alignment, with no packing pragma.
- Fixed-width integers and opaque 64-bit handles.
- `uint32_t` results, flags, sizes, and descriptor booleans.
- UTF-8 and explicit `uint16_t` UTF-16 pointer/length views.
- `ERUI_CALL` (`__cdecl`) callbacks and entry points.
- Size-prefixed descriptors and one size-negotiated function table.
- No STL, RTTI, exception, virtual object, allocator, or cross-module free.
- Zero is invalid for every handle; live object handles are globally unique.
- The host copies every input string and choice-option array before the API
  call returns.

On x64, the API 1.0 layouts are tested under MSVC and MinGW-w64:

| Type | Bytes |
| --- | ---: |
| `ERUI_StringView`, `ERUI_Utf16View` | 16 |
| `ERUI_GameLanguageInfo` | 32 |
| `ERUI_ProviderDesc` | 56 |
| `ERUI_ButtonDesc`, `ERUI_ToggleDesc` | 64 |
| `ERUI_ChoiceDesc` | 72 |
| `ERUI_SliderDesc`, `ERUI_SubmenuDesc` | 80 |
| `ERUI_PageTitleFormatContext` | 56 |
| `ERUI_PagePresentationDesc` | 64 |
| `ERUI_AlertDesc` | 48 |
| `ERUI_Api` / `ERUI_API_V1_0_SIZE` | 128 |

The tests also pin critical offsets and verify that `ERUI_GetApi` does not
write through a trailing canary.

`ERUI_GameLanguageInfo.identifier` points to immutable host-owned UTF-8
storage that remains valid until process exit. The C++ wrapper copies it into
an owned `std::string` snapshot.

## Version request and table sizing

The client initializes `ERUI_Api.size` and requests the exact version compiled
into its header. The current host accepts `ERUI_API_VERSION_1_0`, requires its
complete table prefix, writes at most the supplied size, and returns 1.0 in
`api_version`. Unknown versions are rejected with
`ERUI_UNSUPPORTED_VERSION`; the host does not assume that an unknown minor is
compatible or silently clamp it to 1.0.

Size prefixes still make each supported contract mechanically extensible and
protect both sides from overwrite. Existing fields, function pointers, and
constants are never reordered or renumbered. A future API version must define
its compatibility behavior explicitly rather than relying on its encoded
major alone.

Every descriptor must include the complete 1.0 prefix. A 1.0 host reads only
known fields from a larger descriptor. Flags and reserved fields must remain
zero until their semantics are defined.

## Compiler boundary

The public targets are:

```text
ERNativeUI::CABI   include path only, suitable for C clients
ERNativeUI::SDK    CABI + C++17 feature, for ERNativeUI.hpp
```

Neither has a link library. Supported client combinations include:

```text
MSVC host <-> MSVC client
MSVC host <-> MinGW-w64 client
MSVC host <-> clang-cl client
C client   <-> C++ host
```

The raw header is compiled as C11 and the wrapper as C++17 in CMake tests.
Release validation additionally runs strict MinGW C11/C++17 compilation with
pedantic warnings treated as errors. These are reproducible commands, not an
automatic attempt to invoke a second compiler from every CMake build.

The host binary itself requires an MSVC-compatible ABI for a private native
Elden Ring `std::function` interface. This does not weaken client compatibility
because no C++ object crosses the public ABI.

The raw alert callback carries its response as the fixed-width
`ERUI_AlertResponse` (`uint32_t`). The C++17 wrapper preserves the scoped
`erui::AlertResponse` interface with a small client-side asynchronous thunk;
it does not reinterpret and call one function-pointer type as the other. Its
temporary callback state is allocated and destroyed by code in the same client
DLL, so neither the MSVC host nor another runtime frees client memory.

## Ownership and failure

Input views are borrowed for one call. Embedded NUL, malformed UTF-16, invalid
reserved fields, and over-limit inputs are rejected. Values and handles are
host-owned. Row callback code and `user_data` are provider-owned and must
remain valid through game exit; committed modules are pinned. Alert message
text is copied during `enqueue_alert`; its callback and `user_data` must remain
valid until asynchronous completion. A page-title formatter and its
`user_data` need remain valid only until commit returns, because the host calls
it during compilation and caches the copied result.

Commit waits for final native readiness. After a host failure, retained
function tables return `ERUI_HOST_FAILED` for operational calls. The ABI never
transfers an allocation that another compiler/runtime must destroy.
