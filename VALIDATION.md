# ERNativeUI 0.9 validation

This checklist separates repeatable build-time evidence from the final
in-game check. A successful compiler run does not prove that game-version
signatures still match the installed Elden Ring executable.

## Automated Windows validation

Configure, build, and run the suite with:

```bat
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

The suite covers:

- C ABI layout and the complete API 1.0 function-table prefix;
- Steam language-token classification and unknown-locale preservation;
- C++17 wrapper compilation;
- version negotiation, canary-bounded API writes, and failed-host results;
- provider isolation, unique typed-object handles, rollback, ordering, and
  startup freeze behavior;
- host-owned values and multi-provider root pagination;
- distinct inline/popup choice rows, one-based native-state conversion,
  programmatic writes, invalid-state repair, and 1/32 structural boundaries;
- disabled/unreachable menu routes and capacity-specific pagination;
- native text results, button/submenu bridges, Back routing, and GFX parsing.

## Cross-toolchain client validation

The host must use an MSVC-compatible C++ ABI because Elden Ring directly
consumes an MSVC `std::function` object in its native action-row constructor.
That restriction does not cross the public C boundary: MSVC, clang-cl, and
MinGW-w64 client mods are supported.

Strict MinGW header smoke checks can be reproduced with:

```bat
gcc -std=c11 -pedantic-errors -Wall -Wextra -Werror -Iinclude ^
  -c tests\abi_c_compile_test.c
g++ -std=c++17 -pedantic-errors -Wall -Wextra -Werror -Iinclude ^
  -c tests\client_header_test.cpp
```

After `cmake --install`, configure `examples/`, `examples/template`, and
`examples/tarnished_ui_showcase` independently with `CMAKE_PREFIX_PATH` set to
the install prefix. Inspect each client DLL's imports: no client should import
`ERNativeUI.dll`. Inspect the host exports: only `ERUI_GetApi` is public.

## Packaging validation

Use the install or CPack ZIP output, not a manual ZIP of the working folder:

```bat
cmake --install build/preset-release --config Release --prefix dist\ERNativeUI
cmake --build build/preset-release --config Release --target package
```

The resulting package must contain the MIT license, third-party notices and
full dependency license texts. It must not contain an import library, static
library, Elden Ring `.gfx` file, log, old archive, or local build directory.

## In-game validation

Run offline with Easy Anti-Cheat disabled and load the host before clients.
For the shipped showcase, verify:

1. `ERNativeUI.log` reports one committed provider and `host ready`.
2. `Tarnished UI Showcase` rows appear in System -> Game Options.
3. Its provider name, rows, choices, help, and dialog messages use the active
   Elden Ring language; the `Localized Greeting` example does the same.
3. Toggle and slider callbacks update, and `Toggle From Client Code` changes
   the host-owned toggle.
4. Open `Choice Rows Showcase`. Verify the inline row changes in place and
   the two-, four-, and eight-item popup rows preserve option order and
   independent selections after reopening. Select first/last entries, confirm
   callbacks and get/set indices are zero-based, and verify vanilla Graphics
   quality labels are unchanged before and after.
5. The 33-row submenu has three lossless slices.
6. Next opens a real child page and Previous invokes native Back.
7. Manual Back, reopening pages, mouse/controller input, and repeated menu
   visits do not duplicate rows or crash.
8. Both vanilla six-row and optional patched 13-row controller GFX layouts
   select the expected capacity in the log.
9. Exercise the showcase alert matrix: all seven button layouts in both bottom
   and center placement. Verify controller/mouse page movement, row activation,
   and the page's native Back path remain blocked while popup actions work.
   Confirm one-button Back reports primary, two-button Back reports secondary,
   dismiss-only reports dismissed, and every variant can reopen.
10. Confirm the log contains only production popup-choice address/bridge lines,
    with no passive constructor/widget probe output.
11. For extended integration validation, load the template's root-level alert
   button as a second client. Temporarily enqueue two alerts from one callback
   and verify FIFO dismissal without page input leaking through either dialog;
   do not ship that duplicate-request test in a real mod.

When a game update changes code, follow
[docs/NATIVE_ADDRESSES_AND_PAGINATION.md](docs/NATIVE_ADDRESSES_AND_PAGINATION.md)
and record the new PE identity, AOB result, validated fallback RVA, and live
test evidence. Never patch `eldenring.exe` to distribute ERNativeUI.
