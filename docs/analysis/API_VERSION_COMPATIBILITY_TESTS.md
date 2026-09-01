# API version compatibility test plan

Status: compatibility foundation implemented; API 1.1 remains unreleased

This document defines the compatibility contract and the tests that protect it
before API 1.1 is frozen. The development host now accepts exact 1.0 and 1.1
requests, returns the corresponding prefix, and binds each provider to the
version-specific registration trampoline. The final 1.1 table may still grow
as other accepted 1.1 features are appended.

The purpose of the plan is to continuously answer both questions:

- Can a mod compiled with the released 1.0 C header or C++ wrapper use the
  latest host without being rebuilt?
- Does a newer client fail or explicitly downgrade safely when it encounters
  an older host?

The existing `ERNativeUI.AbiCCompile`, `ERNativeUI.ClientHeader`, registry, and
package-consumer tests use the mutable headers in `include/ernativeui`. They
validate the current source tree, but they cannot prove compatibility with a
previously released client after those headers change.

## Implemented directory layout

```text
tests/abi/
|-- CMakeLists.txt
|-- README.md
|-- cmake/
|   `-- AddAbiRelease.cmake
|-- current/
|   |-- c_layout_test.c
|   |-- cpp_header_test.cpp
|   |-- api_negotiation_test.cpp
|   |-- descriptor_guard_test.cpp
|   `-- wrapper_fallback_test.cpp
|-- support/
|   |-- current_host_dllmain.cpp
|   |-- compat_control.h
|   |-- guarded_buffer_win32.hpp
|   `-- host_loader.c
`-- releases/
    |-- CMakeLists.txt
    `-- v1_0/
        |-- ORIGIN.md
        |-- release.cmake
        |-- include/
        |   `-- ernativeui/
        |       |-- erui.h
        |       `-- ERNativeUI.hpp
        |-- layout_test.c
        |-- c_client_test.c
        |-- cpp_client_test.cpp
        |-- current_host_smoke_test.c
        |-- current_host_wrapper_smoke_test.cpp
        `-- contract_host.cpp
```

`current/` continues to test the headers under active development.
`releases/` contains immutable client snapshots. The two purposes must remain
separate: a test of the latest header is not a historical compatibility test.

The implemented target and principal CTest names are:

| Purpose | CMake target | CTest name |
|---|---|---|
| Current C layout | `ERNativeUIAbiCurrentCLayout` | `ERNativeUI.ABI.Current.CLayout` |
| Current C++ header | `ERNativeUIAbiCurrentCppHeader` | `ERNativeUI.ABI.Current.CppHeader` |
| Current production negotiation | `ERNativeUIAbiNegotiationCurrent` | `ERNativeUI.ABI.Negotiation.CurrentProduction` |
| Guarded provider/TextInput descriptors | `ERNativeUIAbiDescriptorGuardCurrent` | `ERNativeUI.ABI.GuardedProvider.CurrentProduction` |
| Current strict-C TextInput contract | `ERNativeUIAbiCurrentTextInputContract` | `ERNativeUI.ABI.TextInput.CurrentProductionC` |
| Latest test host | `ERNativeUIAbiHostCurrent` | helper target, not a test |
| Frozen 1.0 C/current-host smoke client | `ERNativeUIAbiV1_0ToCurrentC` | `ERNativeUI.ABI.Compat.V1_0.CToCurrent` |
| Frozen 1.0 C++/current-host smoke client | `ERNativeUIAbiV1_0ToCurrentCpp` | `ERNativeUI.ABI.Compat.V1_0.CppToCurrent` |
| Frozen 1.0 contract host | `ERNativeUIAbiHostV1_0Contract` | helper target, not a test |
| New client, old-host negotiation | `ERNativeUIAbiNegotiationCurrent` | `ERNativeUI.ABI.Compat.CurrentToV1_0.Negotiation` |
| New C++ wrapper, explicit downgrade | `ERNativeUIAbiCurrentWrapperFallback` | `ERNativeUI.ABI.Compat.CurrentToV1_0.Wrapper` |

The host DLL targets must use dedicated runtime directories even though their
output name is `ERNativeUI.dll`. This prevents them from colliding with the
production host target or with another compatibility-host version.

## What the compatibility tests actually run

The important tests are executable compatibility tests, not source-level
comparisons. They build a small client once with a frozen released SDK and
then load a newer test host:

```text
ERNativeUIAbiV1_0ToCurrentC.exe
  compiled only with releases/v1_0/include
        |
        | LoadLibraryW(absolute current-host path)
        v
abi/runtime/current/ERNativeUI.dll
  production ERUI_GetApi + production registry behavior
  native game hooks are never installed
```

The test host runs without starting Elden Ring. Its bootstrap opens
registration and enters `runtime_ready` without native hooks so
`commit_provider` observes a completed host state. It reuses the real API-table
and registry implementation; an unrelated fake table could keep passing after
production code regressed. The current-host smoke clients cover negotiation,
registration, commit, handles, and get/set ownership. Rich callback controls
remain isolated in the frozen contract fixture and ordinary production
registry unit tests.

Each CTest case is a fresh process and receives one absolute DLL path. Both
the current test host and frozen 1.0 contract host are named
`ERNativeUI.dll`, but live in different directories:

```cmake
set_target_properties(ERNativeUIAbiHostCurrent PROPERTIES
  OUTPUT_NAME ERNativeUI
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/abi/runtime/current")
set_target_properties(ERNativeUIAbiHostV1_0Contract PROPERTIES
  OUTPUT_NAME ERNativeUI
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/abi/runtime/v1_0")

target_include_directories(ERNativeUIAbiClientV1_0C PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/releases/v1_0/include")

add_test(NAME ERNativeUI.ABI.Compat.V1_0.CToCurrent
  COMMAND ERNativeUIAbiClientV1_0C
          "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
add_test(NAME ERNativeUI.ABI.Compat.CurrentToV1_0.StrictReject
  COMMAND ERNativeUIAbiClientCurrent
          "$<TARGET_FILE:ERNativeUIAbiHostV1_0Contract>" strict)
add_test(NAME ERNativeUI.ABI.Compat.CurrentToV1_0.Fallback
  COMMAND ERNativeUIAbiClientCurrent
          "$<TARGET_FILE:ERNativeUIAbiHostV1_0Contract>" fallback)
```

The client first calls `LoadLibraryW` with that exact path. The frozen C++
wrapper subsequently performs its real `GetModuleHandleW(L"ERNativeUI.dll")`
and `GetProcAddress("ERUI_GetApi")` flow, which resolves the already-loaded
test DLL. An import library is never involved.

### Concrete old-table canary test

The released 1.0 API table ends at byte 128. A 1.1 host must not fill its new
function pointers when the caller selected 1.0, even if those pointers exist
in the host's own larger structure:

```c
typedef struct ApiBlock {
    ERUI_Api api;              /* exactly 128 bytes in frozen 1.0 */
    unsigned char canary[32];
} ApiBlock;

_Static_assert(sizeof(ERUI_Api) == 128u, "not the frozen 1.0 header");
_Static_assert(offsetof(ApiBlock, canary) == 128u, "unexpected padding");

ApiBlock block;
memset(&block, 0, sizeof(block));
memset(block.canary, 0xA5, sizeof(block.canary));
block.api.size = ERUI_API_V1_0_SIZE;

ERUI_GetApiFn get_api = resolve_get_api(absolute_dll_path);
CHECK(get_api(ERUI_API_VERSION_1_0, &block.api) == ERUI_OK);
CHECK(block.api.size == 128u);
CHECK(block.api.api_version == ERUI_API_VERSION_1_0);
CHECK(all_bytes_equal(block.canary, sizeof(block.canary), 0xA5));
```

The client then uses only the returned pointers to register a provider, add
representative 1.0 rows, and commit. This catches both a table overrun and a
host that returns 1.1 in `api_version`; the published 1.0 C++ wrapper requires
the returned version to equal 1.0 exactly.

### Concrete frozen-wrapper test

The C++ test includes only `releases/v1_0/include` and uses the real published
header-only wrapper:

```cpp
Observation observation{};
erui::ProviderOptions options{};
options.provider_id = "tests.frozen-v1-0";
options.display_name = L"Frozen 1.0 client";
options.owner_module = GetModuleHandleW(nullptr);

auto result = erui::register_menu(options, [&](erui::Menu& menu) {
    auto root = menu.root();
    button = root.add_button(
        L"Old button", L"", &button_callback, &observation);
    toggle = root.add_toggle(
        L"Old toggle", L"", 0, &value_callback, &observation);
});

CHECK(result.success());
```

This is intentionally redundant with the raw-C test. It additionally catches
changes to the wrapper's exact-version checks, loader behavior, templates,
callback thunks, and assumptions about capability bits.

### Concrete descriptor guard-page test

A canary detects writes beyond an output. A no-access page detects reads past
an old input structure. Allocate two Windows pages, protect the second one,
and place the final byte of a frozen descriptor immediately before it:

```cpp
SYSTEM_INFO info{};
GetSystemInfo(&info);
auto* pages = static_cast<std::byte*>(VirtualAlloc(
    nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

DWORD old_protection{};
VirtualProtect(
    pages + info.dwPageSize,
    info.dwPageSize,
    PAGE_NOACCESS,
    &old_protection);

auto* description = reinterpret_cast<ERUI_ProviderDesc*>(
    pages + info.dwPageSize - sizeof(ERUI_ProviderDesc));
*description = make_valid_frozen_v1_0_provider();
description->size = 56u;

CHECK(api.register_provider(description, &provider, &root) == ERUI_OK);
```

A correct 1.1 host reads only bytes 0..55. Reading a field appended at byte 56
raises an access violation in that test process instead of accidentally
succeeding because ordinary adjacent memory happened to be readable. Each
descriptor and output structure gets an exact-size case and a size-minus-one
case. Guard cases run separately so a failure names the exact structure.

### Callback and copied-input test control

The test host exposes a small test-only C control surface, never installed as
part of the SDK. Operations such as `TriggerButton(row)`,
`TriggerValue(row, 73)`, `FormatTitle(page, 2, 3)`, and
`CompleteAlert(ERUI_OK, ERUI_ALERT_RESPONSE_SECONDARY)` drive the stored
registry/model path. They do not call the client's function pointer directly.

The client verifies invocation count, calling convention, exact `user_data`,
value/context/response, and callback lifetime. It also changes or destroys its
source strings and choice arrays immediately after each add call; a control
query must still observe the original host-owned copies. This proves the
ownership contract rather than merely proving that the call returned success.

### TextInput maximum and copy tests

The current development TextInput descriptor is 96 bytes, with
`maximum_length` at byte 88 and the final reserved word at byte 92. Current C
and C++17 layout tests pin those values. A guard-page process passes both a
95-byte prefix and an exact 96-byte descriptor through the production-backed
host table, so the host cannot accidentally treat an undersized description
as complete.

A strict-C dynamic client negotiates API 1.1 from the production-backed test
DLL and exercises `add_text_input`, `set_text_input_value`, and
`get_text_input_value` through the exported table. The ordinary production
Registry test independently inspects the compiled row/state and drives the
same native-commit queue used by the host worker. Together these implemented
scenarios verify:

- zero normalizes to the negotiated 1.1 default of 16, while explicit 1, 3,
  7, 16, 17, and 35 retain their effective values;
- unsupported maxima, nonzero reserved data, and an initial value beyond the
  effective maximum return `ERUI_INVALID_ARGUMENT` without publishing a row;
- an exact-limit initial or programmatic value succeeds, while limit-plus-one
  leaves the canonical value and output handle unchanged;
- no rejection path clamps or stores a prefix;
- initial values are copied before the add call returns, and the Registry's
  compiled label, help, and placeholder remain host-owned;
- a changed accepted value invokes the callback once with an immutable copied
  snapshot, while same-value confirmation, cancellation, and programmatic set
  invoke it zero times; and
- a 1.0-bound provider passed to all three TextInput pointers receives
  `ERUI_NOT_SUPPORTED` before any descriptor, view, row, or output buffer is
  read or changed.

Cancellation and true native-editor acceptance cannot be synthesized by the
game-independent DLL. They remain live release gates. The native Unicode
boundary matrix must likewise add precomposed, combining, CJK, and
surrogate-pair values before 1.1 freezes; the host already rejects malformed
UTF-16 at its public boundary. There is intentionally no post-registration
maximum setter in the 1.1 TextInput prefix; adding one later requires new
active-editor and lowering-below-current-value scenarios.

### New client against an old host

The reverse test first requests 1.1 from the frozen 1.0 contract host:

```text
zero complete 1.1 table and set its capacity
request 1.1
expect ERUI_UNSUPPORTED_VERSION
expect every table byte unchanged
```

The explicit fallback case then reinitializes the table and requests 1.0. It
must receive version 1.0, size 128, no TextInput capability, and null bytes in
the client's appended function slots. Old operations may be used; a TextInput
operation must report unavailable and must never call a null pointer. Strict
rejection and fallback are separate tests so fallback cannot hide an invalid
first response.

A mixed-table case obtains both prefixes from the current host, registers one
provider through each version-specific registration pointer, and deliberately
passes the 1.0-bound provider to every 1.1-only function. Each call must return
`ERUI_NOT_SUPPORTED` without reading or changing TextInput state. This proves
that provider version binding is enforced after registration rather than
being only a property of the table used to obtain a handle.

Together, these layers catch different failures:

| Test | Regression caught |
|---|---|
| Frozen layout/integrity | Published headers were altered accidentally |
| Raw C dynamic client | Prefix, calling convention, or function order changed |
| Table canary | New host wrote beyond the selected old table |
| Frozen C++ client | Real wrapper/loader/source contract stopped working |
| Guard page | New host read an appended field from an old descriptor |
| Test-control callbacks | Signature, lifetime, user data, or copied ownership changed |
| Reverse strict/fallback | Version negotiation or feature gating became untruthful |
| Mixed-table provider | A lower-version provider gained access to a newer operation |
| Separate processes/paths | The wrong same-named DLL contaminated a result |

## Immutable released-header rule

Each released-version folder must contain real copies of the exact published
C header and C++ wrapper. It must not include the current headers through a
symlink, forwarding include, generated alias, or `ERNativeUI::CABI` /
`ERNativeUI::SDK` target.

The 1.0 snapshot must come from:

| Property | Value |
|---|---|
| Git tag | `ERNativeUI-v1.0.0` |
| Commit | `668f2f09ab3a175969db07a7c7063761ea2906f1` |
| `erui.h` Git blob | `d6503fabcd8f7265ce904ba492b26ac8ab562f85` |
| `ERNativeUI.hpp` Git blob | `1fe27782e3ea24d1e1f7ba7a12f50562fa19d1ce` |

`ORIGIN.md` records this provenance. `release.cmake` records the CMake-safe
token (`v1_0`), encoded API version, API-prefix size, and required C/C++
language standards. A configure-time integrity check fails if a frozen file
changes accidentally.

Snapshot client targets receive only their versioned include directory. They
also compile assertions for `ERUI_API_VERSION_CURRENT` and
`ERUI_API_V1_0_SIZE == 128`, so an include-order mistake cannot silently make
them use the latest header.

## Development 1.1 negotiation contract

The following contract is implemented now and remains mandatory when the final
1.1 prefix grows before release:

TextInput's three appended pointers occupy bytes 128 through 151. The current
development `ERUI_API_V1_1_SIZE` is therefore 152, but it is intentionally not
frozen: it will grow as Color Picker and any other accepted 1.1 additions are
appended before release. Once 1.1 ships, that final prefix becomes immutable.
Negotiation always returns the complete prefix defined by the selected SDK,
never a caller-selected per-feature subset.

1. Public structures and the `ERUI_Api` table remain append-only within major
   version 1. Existing fields are never reordered, removed, retyped, or given
   a different calling convention.
2. `requested_version` selects a supported, exact API prefix. A 1.1 host
   accepts requests for 1.0 and 1.1; it does not reinterpret a request for
   1.99 as the latest available version.
3. On entry, `out_api->size` is the caller's writable capacity. It must be at
   least the prefix size for the requested version.
4. The host writes no more than both the caller's capacity and the selected
   version's prefix size. Requesting 1.0 can therefore never write a 1.1
   function pointer beyond byte 128.
5. On success, `out_api->api_version` is the selected version and
   `out_api->size` is its prefix size. Capabilities are masked to features
   exposed by that selected version.
6. A 1.0 request to a 1.1-or-newer host returns the complete 1.0 table with
   version 1.0. This is required because the released 1.0 C++ wrapper checks
   the returned version for exact equality.
7. An unsupported-version result does not partially populate the caller's
   table. Null output and an output capacity smaller than the requested
   prefix return `ERUI_INVALID_ARGUMENT`.
8. A provider is bound to the table version that created it. The recommended
   implementation returns version-specific `register_provider` trampolines
   from the 1.0 and 1.1 tables. Each trampoline validates the descriptor's
   declared `api_version` and stores its own selected version, rather than
   trusting that client-written field as proof of a previous stateless
   `ERUI_GetApi` call. A newer host accepts the old version and old
   `struct_size`, reads only fields present within that size, and supplies
   defaults for appended fields.
9. New callback-context fields are appended. When a callback was registered
   by a 1.0 provider, the host preserves the 1.0 callback signature, field
   offsets, context `size` value, user-data value, and documented lifetime.

Error precedence is also explicit: a null table returns
`ERUI_INVALID_ARGUMENT`; otherwise the host selects the requested version
before reading a version-specific capacity. An unknown version therefore
returns `ERUI_UNSUPPORTED_VERSION` without mutating the table, even when the
capacity field would be too small for a hypothetical version.

This guarantees old-client-to-new-host compatibility. The inverse direction
is necessarily conditional: a new client can use an old host only when it
explicitly accepts the older prefix and gates every newer operation on the
negotiated version and capability bits.

The raw C call must first demonstrate strict behavior: a 1.1 request to a 1.0
host returns `ERUI_UNSUPPORTED_VERSION`. A separate fallback scenario then
deliberately retries 1.0 and verifies that the negotiated table has no 1.1
function or capability. The eventual 1.1 C++ wrapper may use this mechanism,
but it must never call a missing function or pretend that a required 1.1
feature is available.

## Dynamic-load compatibility scenarios

`ERNativeUIAbiHostCurrent` is a game-independent test DLL named
`ERNativeUI.dll`. It must reuse the production API-table negotiation and host
registry implementation; reproducing the table in an unrelated fake would
allow production regressions to pass the tests. A small test-only bootstrap
opens registration, freezes it after the fixture providers commit, and enters
`runtime_ready` without installing Elden Ring hooks.

Every compatibility client runs in its own process and explicitly loads that
DLL. The frozen C++ client then follows the real 1.0 wrapper path:

```text
LoadLibrary(absolute test-host path)
    -> GetModuleHandleW("ERNativeUI.dll")
    -> GetProcAddress("ERUI_GetApi")
    -> ERUI_GetApi(1.0, 128-byte table)
```

The frozen C client obtains the same export directly. The implemented current-
host smoke clients:

- negotiate an exact 1.0 table and protect its 128-byte tail with a canary;
- register a provider using the exact 1.0 descriptor layout;
- add and commit a representative host-owned toggle;
- commit through the returned table rather than link to a host import library;
- exercise get/set ownership through both raw C and the frozen C++17 wrapper;
  and
- verify that the frozen wrapper's real module/export lookup remains valid.

The frozen 1.0 contract fixture separately exercises the richer callback and
copied-input cases. `compat_control.h` defines its small test-only C ABI; it is
never installed or included by the production SDK. The production-backed host
deliberately does not export a second implementation of Registry semantics
merely to make tests easier.

The frozen 1.0 contract host provides the reverse-direction tests without
loading the real game-dependent 1.0 binary. It exposes exactly the released
1.0 prefix and rejects newer requests. It is a contract fixture, not a
substitute for testing the latest production negotiation code.

Because MinGW interoperability is an advertised reason for the strict C ABI,
cross-toolchain dynamic clients remain a 1.1 release gate: CI should build
frozen/current MinGW-w64 C clients and preferably the C++17 wrapper client,
then run them against the MSVC-built current test host. MinGW layout assertions
alone do not prove calling-convention or callback compatibility across the two
toolchains.

## Bounds and corruption checks

Compatibility tests must check memory safety, not only return codes:

- Place a canary immediately after a 128-byte 1.0 API table and verify that a
  newer host never changes it.
- Fill bytes beyond the selected prefix with a nonzero pattern and verify that
  an unsupported or lower-version request leaves them unchanged.
- Test every required-structure boundary with `struct_size` one byte too short
  and with the exact released size.
- Put old descriptors at the end of a committed Windows page followed by a
  `PAGE_NOACCESS` guard page. A host read beyond the declared 1.0 structure
  then fails deterministically instead of passing because adjacent memory was
  readable.
- Verify output handles and tables retain their documented invalid/unchanged
  values on rejection.

The canary tests detect overwrites. The guard-page tests detect accidental
reads of fields appended in 1.1 when an old client supplies only a 1.0 object.

## Adding later versions

`AddAbiRelease.cmake` should expose one helper such as:

```cmake
erui_add_abi_release(v1_0)
```

When 1.1 is released, copy its published headers into `releases/v1_1`, record
their tag, commit, and blob identities, add the release metadata and any
version-specific feature scenario, then add:

```cmake
erui_add_abi_release(v1_1)
```

The helper creates that release's layout checks and C/C++ clients against the
latest host. Previous folders remain unchanged. Consequently, a future 1.2
change tests clients frozen at 1.0, 1.1, and 1.2 rather than only testing the
latest pair.

No released snapshot is updated to make a failing compatibility test pass.
Either the latest host is corrected, or a deliberately incompatible change
requires a new major API version and explicit migration documentation.
