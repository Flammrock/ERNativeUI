# ERNativeUI ABI compatibility tests

This subtree separates the mutable SDK under `include/ernativeui` from
immutable snapshots of interfaces that have already shipped. Its purpose is
to execute old clients against a newer host, not merely to recompile old
source with the newest header.

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
        |-- layout_test.c
        |-- c_client_test.c
        |-- cpp_client_test.cpp
        |-- current_host_smoke_test.c
        |-- current_host_wrapper_smoke_test.cpp
        `-- contract_host.cpp
```

The contract host is deliberately a reverse-direction fixture: it exposes
only the released v1.0 prefix, allowing a current client to prove strict
rejection and explicit fallback. The root build also creates a game-independent
current host from the production negotiation and registry sources. Frozen C
and C++17 smoke clients load that DLL in isolated processes, proving the other
direction without launching Elden Ring or installing native hooks.
The current strict-C TextInput client uses the same DLL to cover the complete
1, 3, 7, 16, 17, and 35-unit boundary matrix plus copied input and getter/
setter buffer semantics. Guard-page cases protect both the frozen provider
descriptor and the current 96-byte TextInput descriptor boundary.

## Integration

From the repository root, add this directory only while tests are enabled:

```cmake
add_subdirectory(tests/abi)
```

When configured from the repository root, `ERNativeUIAbiHostCurrent` enables
the frozen-client-to-current-host smoke tests automatically. A standalone
`tests/abi` configure intentionally omits that project-owned production target;
layout tests, the frozen v1.0 contract host, and current-client-to-v1.0
fallback tests still build and run.

This subtree may also be configured directly on Windows:

```powershell
cmake -S tests/abi -B build/abi -A x64
cmake --build build/abi --config Release
ctest --test-dir build/abi -C Release --output-on-failure
```

`support/compat_control.h` is test-only. It lets clients ask the contract or
current test host to trigger callbacks through stored registry state. It is
never installed and is not part of ERNativeUI's public ABI.
