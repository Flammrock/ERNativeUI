# Release versions and public API versions

ERNativeUI has two independent version numbers. Their major/minor components
happen to coincide at `1.1`, but they answer different questions and must not
be compared as though they were one value.

| Version axis | Baseline in these docs | What it describes | Where it is checked |
|---|---:|---|---|
| ERNativeUI release/package version | **1.1.0** | The version of the distributed project, SDK archive, CMake package, host, documentation, and release notes. It follows Semantic Versioning. | At configure time by CMake and when selecting a release/download. |
| ERUI public API version | **1.1** | The binary function-table contract understood by a client and `ERNativeUI.dll`. | At runtime through `ERUI_GetApi`, or by the C++ wrapper during `erui::connect()`. |

This documentation describes the finished ERUI API 1.1 contract and the
ERNativeUI 1.1.0 release line that first packages it. Their matching numbers
are convenient, not a rule that couples the two version systems.

## Release and package version

The three-part ERNativeUI version identifies a release such as `1.1.0`. It is
used for release tags, archive names such as
`ERNativeUI-1.1.0-windows-x64.zip`, changelog entries, CPack metadata, and the
installed CMake package.

In a client project:

```cmake
find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)
target_link_libraries(MyMod PRIVATE ERNativeUI::SDK)
```

the `1.1.0` requirement is a **minimum release/package version requirement**.
CMake reads `ERNativeUIConfigVersion.cmake`; it does not load
`ERNativeUI.dll`, call
`ERUI_GetApi`, or inspect the runtime function table.

The installed package currently uses CMake's `SameMajorVersion` compatibility
rule. A request for `1.1.0` means at least release `1.1.0` within release
major `1`: it rejects 1.0.0 as too old and 2.0.0 as a different major
version, while a compatible 1.1.x or 1.2.x package may satisfy it. There is no
`EXACT` keyword in the recommended command. Use
`find_package(ERNativeUI 1.1.0 EXACT CONFIG REQUIRED)` only when intentionally
requiring that one package release; accepting compatible fixes is normally
preferable.

This CMake compatibility check selects an SDK package; it does not select an
older API inside a newer SDK. For example, lowering the requirement to `1.0.0`
while CMake resolves the 1.1.0 package still gives the project the 1.1.0
headers and its API 1.1 wrapper. To target API 1.0 intentionally, point
`ERNativeUI_DIR` or `CMAKE_PREFIX_PATH` exclusively at the extracted 1.0.0 SDK,
copy that release's two public headers into the project, or use `EXACT` for a
reproducibly pinned SDK build. This build-time choice does not stop players
from using a later compatible host DLL at runtime.

`find_package` must also know where the extracted SDK lives. Point
`CMAKE_PREFIX_PATH` at the directory containing `bin`, `include`, `lib`,
and `share`, or point `ERNativeUI_DIR` directly at
`lib/cmake/ERNativeUI`. The
[SDK setup guide](getting-started/setup.md#option-a-let-cmake-find-the-extracted-sdk)
shows both commands.

Use `ERNativeUI::CABI` instead of `ERNativeUI::SDK` for a strict-C client. That
changes the header target, not which version axis `find_package` checks.

## ERUI public API version

The public C header defines the binary API versions independently:

```c
ERUI_API_VERSION_1_0
ERUI_API_VERSION_1_1
ERUI_API_VERSION_CURRENT
```

A strict-C client first resolves the exported function as an `ERUI_GetApiFn`,
then requests the contract it was compiled to understand:

```c
ERUI_Api api = {0};
api.size = ERUI_API_V1_1_SIZE;

ERUI_Result result = get_api(ERUI_API_VERSION_1_1, &api);
```

The request negotiates a function-table layout. It does not ask whether the
DLL's release filename or package version contains `1.1`. A successful result
means the loaded host supplied the requested API contract within the caller's
declared table size.

The C++17 wrapper performs the same negotiation through:

```cpp
const auto connection = erui::connect();
```

The current wrapper requests ERUI API 1.1 exactly. If the loaded host exposes
only API 1.0, connection fails cleanly; a successful CMake configure against an
SDK on the build machine cannot make an older DLL on the player's machine
support a newer runtime API.

API version and capability bits remain distinct parts of the C contract. The
current C++ wrapper accepts API 1.1 only when its complete required table and
capability set are present; nearby `supports` checks still document which
surface each declaration needs. Successful negotiation does not guarantee that
every game-version-sensitive native path can be installed, so operations can
still fail safely for reasons unrelated to either version number.

The authoritative definitions are in the
[strict-C ABI header](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h). The
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp) implements its connection
policy in terms of that C contract.

## Why the numbers can diverge

A project release can change without changing the public binary contract. For
example, these are illustrations of the versioning model, not promises about
future releases:

- ERNativeUI **1.1.1** could fix a native hook or documentation problem while
  continuing to expose ERUI API **1.1** unchanged.
- ERNativeUI **1.2.0** could add tooling or optional runtime behavior while
  still retaining ERUI API **1.1** if no new function-table contract were
  needed.
- A future 1.x release could introduce ERUI API **1.2** and would retain the
  API 1.0 and 1.1 entry points under the same-major compatibility policy.

Conversely, incrementing an ERUI API version is a deliberate binary-contract
decision. It is not an automatic consequence of changing the package's minor
or patch number.

## Same-major host compatibility policy

A mod author may intentionally build against any released SDK, not only the
newest one. The selected headers determine the API that the client knows and
requests at runtime. Choosing an older API trades newer features for a lower
minimum host requirement; it does not make the client request the newest table
automatically.

ERNativeUI preserves earlier APIs within a host release-major line:

> A host release `X.Y.Z` supports every already-published ERUI API `X.W`
> introduced by that release or an earlier `X.*.*` release.

This rule never makes an old host support an API introduced later. It also
does not promise compatibility across a host major-version change. For
example, an ERNativeUI 2.x host may omit ERUI API 1.x unless its release notes
explicitly say that the 1.x contracts are retained.

For the current line, this means:

| Client's chosen SDK/API | Compatible host releases |
|---|---|
| ERNativeUI 1.0.0 SDK / API 1.0 | ERNativeUI 1.0.0 and every later 1.x host |
| ERNativeUI 1.1.0 SDK / API 1.1 | ERNativeUI 1.1.0 and every later 1.x host |

The shared major number defines this compatibility policy; it does not couple
the release minor to the API minor. A release such as 1.5.0 could introduce
API 1.2, and later 1.x hosts would then retain APIs 1.0, 1.1, and 1.2.

```text
build/configure time                         game runtime
--------------------                         ------------
find_package(ERNativeUI 1.1.0)               ERUI_GetApi(API 1.1)
        |                                             |
        v                                             v
select an acceptable SDK package             negotiate a DLL function table
```

Test first with the host packaged beside the SDK you selected. Players may use
a later host from the same compatible release-major line; runtime negotiation
remains the final protection when their installed host is different.

## API 1.0 compatibility in ERNativeUI 1.1.0

ERNativeUI 1.1.0 preserves the released API 1.0 binary contract. A client DLL
built with the 1.0 headers—whether before or after 1.1 was published—can
request API 1.0 from a host exposing API 1.1 and continue using the frozen 1.0
function-table layout. It does not need to be recompiled merely because the
installed host changed to 1.1.0.

Staying on the API 1.0 SDK is a valid deliberate compatibility choice. Source
code intentionally updated to the API 1.1 wrapper uses the API 1.1 connection
lifecycle and must run with a host that supports API 1.1. Do not treat the
preserved API 1.0 path as an automatic downgrade path for a client compiled
with the API 1.1 wrapper.

See the permanent [adjacent API migration guides](migrations/README.md) for
source changes between public API versions. Migration guides describe how to
update client source; they do not replace the older runtime table retained for
clients that continue to use the older SDK.

## Which version should a mod author write down?

Record both when they matter:

- In build instructions, state the chosen **ERNativeUI SDK release** and the
  minimum release/package version used by `find_package`, for example
  ERNativeUI 1.1.0.
- In ABI documentation or a strict-C negotiation call, state the required
  **ERUI API**, for example API 1.1.
- In player requirements, name an ERNativeUI release known to contain the API
  and capabilities your mod uses, then allow later hosts from that compatible
  release-major line.

For the current feature-to-API map, see [Supported features](features.md). For
the host/client lifecycle around negotiation, see
[How ERNativeUI works](how-it-works.md).
