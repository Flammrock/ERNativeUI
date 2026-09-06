# ERNativeUI ABI compatibility tests

This subtree separates the mutable SDK under `include/ernativeui` from
immutable snapshots of interfaces that have already shipped. Its purpose is
to execute clients built against earlier APIs against a newer host, not merely
to recompile their source with the newest header. That includes both
historical binaries and clients that intentionally keep using an older SDK.

The v1.0 snapshot is copied byte-for-byte from the `ERNativeUI-v1.0.0` Git
tag. `releases/v1_0/ORIGIN.md` records its commit and blob identities, while
`release.cmake` pins independent SHA-256 digests checked at configure time.
Never update a frozen snapshot to make a compatibility test pass.

## Layout

```text
tests/abi/
|-- current/                  layout, negotiation, guard, and 1.1 C ABI tests
|-- support/                  test-only loader and control ABI
|-- cmake/                    reusable release-target helper
`-- releases/
    `-- v1_0/
        |-- include/          exact released C and C++ headers
        |-- ORIGIN.md         tag, commit, Git blobs, and file hashes
        |-- release.cmake     frozen API metadata and SHA-256 pins
        |-- layout_test.c
        |-- c_client_test.c
        |-- cpp_client_test.cpp
        |-- current_host_smoke_test.c
        |-- current_host_wrapper_smoke_test.cpp
        |-- pending_readiness_lifecycle_test.cpp
        `-- contract_host.cpp
```

The contract host is deliberately a reverse-direction fixture: it exposes
only the released v1.0 prefix, allowing the current 1.1 C++ wrapper to prove
that it rejects an older host without silently downgrading. The raw C
negotiation test separately proves that callers which explicitly request the
released 1.0 prefix can still obtain it. The root build also creates a
game-independent current host from the production negotiation and registry
sources. Frozen C and C++17 smoke clients load that DLL in isolated processes,
proving the other direction without launching Elden Ring or installing native
hooks.
The current strict-C TextInput client uses the same DLL to cover the complete
1, 3, 7, 16, 17, and 35-unit boundary matrix plus copied input and getter/
setter buffer semantics. A second strict-C client covers independent
ColorPicker rows, all RGB channel boundaries, getter/setter behavior,
wrong-row rejection, reserved-field validation, and rejection through a
provider negotiated as v1.0. Its C++17 counterpart exercises the typed
callback overload and `Registration::set_color`/`get_color` against the same
production host. Guard-page cases protect the frozen provider, the current
96-byte TextInput, and the current 64-byte ColorPicker descriptor boundaries.
Dedicated strict-C and C++17 BuiltinPage clients verify all seven stable
provider-owned destinations, Game Options root
identity, distinct/repeated handles, ordinary row insertion, invalid values,
and rejection for a provider negotiated through API 1.0.
The strict-C InputBindings client verifies the complete 320-byte 1.1 table,
semantic defaults, sparse updates, per-device reset, provider-wide stable
action IDs, independent sections and handles, optional action outputs,
callback validation, exact 1.0 provider rejection, and the duplicate-ID result
contract. A C++17 client drives the typed action/device facades and the
provider-wide assignment event through the connection fixture. It also proves
that storage remains explicit while covering typed values, `ActionInputs`, and
event-batch persistence. A separate strict-C storage client crosses the real
production ABI with a process-unique file in the Windows temporary directory;
it covers lifecycle, two-call reads, CRUD, action serialization, event-batch
application, revision state, and explicit save. Guard-page cases pin all new
descriptor/value boundaries and prove that a 1.0-bound provider is rejected
before any 1.1-only pointer is read.

The header-only `ActionInputs` codec has independent strict-C99 and C++17
tests that run without loading a host DLL. Golden tables pin every public
controller, keyboard, and mouse spelling; state combinations, parse ordering,
malformed input, bounded writes, output preservation, and multi-translation-
unit C linkage are covered explicitly. The host-storage tests then verify that
the same codec is used at the real INI boundary.

`ERNativeUIAbiConnectionHost` is a second, deliberately narrow fixture for the
header-only 1.1 `Connection` contract. It counts `ERUI_GetApi`, language,
registration, commit, and abort calls and can return a bounded sequence of
`ERUI_HOST_NOT_READY` results. The corresponding test proves that `connect()`
is the only negotiation step, that language lookup is explicit and optional,
and that `Connection::register_menu()` reuses its stored table without
reconnecting or querying language.

`ERNativeUIAbiHostV1_0Pending` uses the production API, registry, and Steam
language implementation while deliberately holding the fixture in the real
startup interval where registration is open and language is still pending.
The frozen-header lifecycle test proves that a 1.0 table remains available,
its historical language call waits for settlement, and its synchronous
provider commit completes when the runtime becomes ready.

## Freeze a newly released API

Create one immutable snapshot for each public **API version**, not for every
project patch release that continues to expose the same API. The snapshot can
be finalized only after the release tag exists, because its provenance must
identify the exact published files. For example, after the first package that
ships API 1.1 is tagged:

1. Create `releases/v1_1` beside `v1_0`. Copy `erui.h` and
   `ERNativeUI.hpp` byte-for-byte from the clean release-tag checkout into
   `releases/v1_1/include/ernativeui`. Do not copy mutable post-release
   headers from `main`.
2. Add `ORIGIN.md` following `v1_0/ORIGIN.md`. Record the release tag, its
   commit, both header Git blob IDs, and both file SHA-256 values. Verify the
   Git identities with `git rev-list -n 1 <tag>` and
   `git rev-parse <tag>:include/ernativeui/<header>`; verify the files with
   `Get-FileHash -Algorithm SHA256` or an equivalent SHA-256 tool.
3. Add `release.cmake` with token `v1_1`, the encoded API version, the frozen
   Windows-x64 function-table prefix size, the required C/C++ language
   standards, and the two lowercase SHA-256 values. The reusable helper checks
   those hashes during CMake configuration. The API-version and prefix-size
   fields are descriptive metadata today, so `layout_test.c` must also assert
   the released version, prefix size, offsets, sizes, and alignment directly.
4. Add version-owned `layout_test.c`, `c_client_test.c`,
   `cpp_client_test.cpp`, and `contract_host.cpp` fixtures. Pin every released
   layout and semantic contract introduced by that API. The contract host must
   expose exactly that version and reject newer requests; the clients must
   negotiate, register, exercise callbacks through test-only controls, and
   verify failure/output-preservation rules.
5. Add frozen-client-to-current-host C and C++ smoke tests, plus any lifecycle
   fixture needed by that release, following the v1.0 targets in the parent
   `tests/abi/CMakeLists.txt`. These tests are the opposite compatibility
   direction from current-client-to-frozen-contract-host tests. The helper's
   generic current-host branch is not wired by the root build today, so do not
   assume this direction appeared merely because the release was registered.
6. Append `erui_add_abi_release(v1_1)` to
   `releases/CMakeLists.txt`. Keep `erui_add_abi_release(v1_0)` and every older
   line. Never replace the previous release with the newest one.
7. Configure from the repository root and run the complete suite. For a host
   release `X.Y.Z`, confirm that clients frozen at **every** previously
   published API `X.W` in that release-major line still run against the new
   host. Also test every prior-major API that the release explicitly retains,
   and confirm that the newest client rejects an older host exactly as its
   connection contract specifies.

After committing a snapshot, never edit its headers or provenance to make a
failure pass. A failure for an earlier API from the current host release-major
line is a host regression and must be fixed. Dropping that API requires a host
major-version change and explicit migration documentation. A new host major
may omit prior-major APIs unless it explicitly retains them, but their frozen
snapshots and provenance remain unchanged.

## Cross-toolchain runtime gate

The ordinary Windows build uses MSVC for both host and test clients. The two
standalone GCC/G++ commands in the
[validation checklist](../../VALIDATION.md#cross-toolchain-client-validation)
are useful header checks, but compilation and layout assertions alone cannot
prove calling conventions, dynamic negotiation, or callbacks across
toolchains.

Before publishing an API for which MinGW-w64 compatibility is advertised, run
an x86-64 MinGW C client and C++17 wrapper client against an MSVC-built current
host. Each client must resolve `ERUI_GetApi` dynamically without an import
library, negotiate its intended API version, register and commit at least one
callback-bearing control, call host functions, and receive at least one
host-to-client callback with its expected arguments and `user_data`. Include
both the newest headers and every earlier API in the current host
release-major line, plus every prior-major API that the release explicitly
retains.

This cross-toolchain runtime matrix is **manual today and is not run by GitHub
CI**. `ERNativeUIAbiHostCurrent` uses the production negotiation and registry
sources, but it currently exposes no test-only callback trigger. The existing
contract and connection fixtures can trigger callbacks, but neither alone
proves a MinGW client callback from that current production-host fixture.
Until a callback-capable isolated runner is added, perform the complete check
with temporary MinGW client DLLs loaded beside the MSVC production host in the
offline game, and record compiler versions, commands, API versions, and callback
results in the release validation record. Do not count the compile-only smoke
commands as completion of this gate.

## Integration

From the repository root, add this directory only while tests are enabled:

```cmake
add_subdirectory(tests/abi)
```

When configured from the repository root, `ERNativeUIAbiHostCurrent` enables
the frozen-client-to-current-host smoke tests automatically. A standalone
`tests/abi` configure intentionally omits that project-owned production target;
layout tests, the frozen v1.0 contract host, and current-client-to-v1.0
rejection tests still build and run. The explicit-connection fixture and test
also remain available in a standalone build.

This subtree may also be configured directly on Windows:

```powershell
cmake -S tests/abi -B build/abi -A x64
cmake --build build/abi --config Release
ctest --test-dir build/abi -C Release --output-on-failure
```

`support/compat_control.h` is test-only. It lets clients ask the frozen v1.0
contract host to trigger callbacks through stored registry state. It is never
installed and is not part of ERNativeUI's public ABI.
`support/connection_control.h` is likewise test-only; it configures and
observes the explicit-connection fixture and can emit a synchronous simulated
player assignment without changing the public table.
`support/pending_v1_0_control.h` releases the two startup gates in the focused
frozen-client lifecycle fixture; it is not exported by the production host.
