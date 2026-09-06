# ERNativeUI validation

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

- C ABI layout, the frozen API 1.0 function-table prefix, and the additive
  API 1.1 TextInput, ColorPicker, BuiltinPage, InputBindings, and Storage
  blocks;
- Steam language-token classification, transient/terminal readiness
  settlement, guarded Steam initialization, bounded fail-open behavior, and
  unknown-locale preservation;
- C++17 wrapper compilation;
- explicit API 1.1 connection polling, clean API 1.0-only host rejection,
  canary-bounded API writes, and failed-host results;
- provider-ID grammar, length boundaries, case-sensitive identity, provider
  isolation, unique typed-object handles, rollback, ordering, per-destination
  built-in handles, and startup freeze behavior;
- host-owned values and deterministic multi-provider pagination on independent
  built-in destinations;
- distinct inline/popup choice rows, one-based native-state conversion,
  programmatic writes, invalid-state repair, and 1/32 structural boundaries;
- disabled/unreachable menu routes and capacity-specific pagination;
- TextInput and ColorPicker canonical state, callback semantics, strict-C and
  C++17 contracts, button/submenu bridges, Back routing, and GFX parsing;
- provider-local action identity and duplicate rejection, input-action callback
  bridges, compiled section/action text, coherent three-device publication,
  independent released-to-pressed edges, same-frame device-mask coalescing,
  revision changes, and suppression/re-arm state;
- the header-only `ActionInputs` text codec under strict C99 and C++17,
  including every semantic input name, all slot-state combinations, malformed
  input, bounded-buffer behavior, host-storage consistency, and multi-unit C
  linkage.

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

Those commands are compile-only smoke checks. They do not exercise dynamic
API negotiation or prove that an MSVC-built host can safely invoke a callback
compiled by MinGW-w64.

The complete cross-toolchain runtime gate is manual today and is not part of
the GitHub CI job. Before a release advertises MinGW-w64 compatibility:

- [ ] Build the current host with the normal MSVC x64 preset.
- [ ] Build an x86-64 MinGW-w64 strict-C client from the current header and a
  C++17 client from the current wrapper. Also build the retained frozen C and
  C++ clients for every earlier API in the current host release-major line,
  plus every prior-major API that the release explicitly retains.
- [ ] Ensure each client uses the header-only dynamic boundary—no import
  library—and resolves `ERUI_GetApi` from the MSVC-built DLL at runtime.
- [ ] Exercise exact-version negotiation, bounded table publication,
  provider registration, at least one callback-bearing row, commit, and
  representative getter/setter calls from both C and C++ clients.
- [ ] Cause the MSVC host to invoke each MinGW client's callback. Verify the
  callback count, arguments, and `user_data`, including one value callback and
  one no-value action callback.
- [ ] Record the MSVC and MinGW-w64 versions, build commands, tested API
  versions, and results with the release validation record.

This compatibility matrix is cumulative within a host release major. A host
release `X.Y.Z` must continue passing the frozen clients for every previously
published ERUI API `X.W` from that same major line. Removing one of those APIs
requires a host major-version change. After such a change, earlier-major APIs
remain in the runtime matrix only when the new release explicitly retains
them; their frozen snapshots remain immutable either way.

The isolated `ERNativeUIAbiHostCurrent` fixture currently has no test-only
callback trigger, so it cannot by itself complete the last callback step.
Until that runner is extended, use temporary MinGW client DLLs with the MSVC
production host in the offline game. Do not ship those diagnostic clients.

After `cmake --install`, configure `examples/` and each standalone example
directory independently with `CMAKE_PREFIX_PATH` set to the install prefix:
`template`, `localized_greeting`, `tarnished_ui_showcase`, and
`showcase_screenshot_helper`. Inspect each client DLL's imports: no client
should import `ERNativeUI.dll`. Inspect the host exports: only `ERUI_GetApi` is
public.

## Packaging validation

Use the install or CPack ZIP output, not a manual ZIP of the working folder:

```bat
cmake --install build/preset-release --config Release --prefix dist\ERNativeUI
cmake --build build/preset-release --config Release --target package
```

The resulting package must contain the MIT license, third-party notices, full
dependency license texts, only the two documented optional patched GFX movies,
and both `bin/examples/ShowcaseScreenshotHelper.dll` and its intentional
source-controlled `ShowcaseScreenshotHelper.ini`. It must not contain an
import library, static library, unmodified or research-only game assets, a log,
a runtime-generated provider configuration such as
`mods/<provider_id>/config.ini`, an old archive, or a local build directory.

## In-game validation

Run offline with Easy Anti-Cheat disabled and load the host before clients.
Set `[Logging] EnableLog = 1` for checks that inspect `ERNativeUI.log`; restore
the shipped default `0` after testing.
For the shipped showcase, verify:

1. `ERNativeUI.log` reports the expected committed-provider count and
   `ERNativeUI host ready`.
2. `Tarnished UI Showcase` rows appear in System -> Game Options.
3. Its provider name, rows, choices, help, and dialog messages use the active
   Elden Ring language; the `Localized Greeting` example does the same.
4. Toggle and slider callbacks update, and `Toggle From Client Code` changes
   the host-owned toggle.
5. Visit Camera Options, Display, Sound, Network, Keyboard/Mouse Settings, and
   Graphics. Verify the showcase's public BuiltinPage action row appears once
   on every page, opens its localized native alert, blocks only the exact
   owning page while open, dismisses with controller and mouse, and works again
   after switching tabs and reopening Configuration. Controller Settings must
   have no generic showcase row.
6. Open `Choice Rows Showcase`. Verify the inline row changes in place and
   the two-, four-, and eight-item popup rows preserve option order and
   independent selections after reopening. Select first/last entries, confirm
   callbacks and get/set indices are zero-based, and verify vanilla Graphics
   quality labels are unchanged before and after.
7. Open `Text Input Showcase` and complete the detailed TextInput matrix below.
8. Exercise both the root and subpage ColorPicker rows. Verify their bracketed
   swatches update immediately, values remain independent, Cancel preserves
   the prior value, same-value confirmation does not notify, and both editors
   reopen after accept/cancel and pagination. While one editor is open, enqueue
   a showcase alert from another client/thread; verify it waits and opens only
   after the color editor closes.
9. The 33-row submenu has three lossless slices.
10. Next opens a real child page and Previous invokes native Back. Add enough
    temporary rows to one non-Game built-in destination to exercise its own
    first-page Next, continuation Previous/Next, and restoration without
    affecting another tab.
11. Manual Back, reopening pages, mouse/controller input, and repeated menu
   visits do not duplicate rows or crash.
12. Both vanilla six-row and optional patched 13-row Game Options GFX layouts
   select the expected capacity in the log.
13. Exercise the showcase alert matrix: all seven button layouts in both bottom
   and center placement. Verify controller/mouse page movement, row activation,
   and the page's native Back path remain blocked while popup actions work.
   Confirm one-button Back reports primary, two-button Back reports secondary,
   dismiss-only reports dismissed, and every variant can reopen.
14. Confirm the log contains only production address/bridge lines, including
    any enabled built-in destination hooks, with no factory observer, hard-coded
    panel-probe text, or passive constructor/widget probe output.
15. For extended integration validation, load the template's root-level alert
   button as a second client. Temporarily enqueue two alerts from one callback
   and verify FIFO dismissal without page input leaking through either dialog;
   do not ship that duplicate-request test in a real mod.

### TextInput live validation (API 1.1)

Run these checks on the default 16-unit field and the extended 35-unit field.
`maximum_length` counts UTF-16 code units, not bytes, Unicode scalar values, or
visible characters. A BMP character normally consumes one unit; a non-BMP
character is encoded as a surrogate pair and consumes two. A base character
plus a combining mark also consumes two units even if it appears as one glyph.

- [ ] Confirm an empty value, one ASCII character, exactly 16 ASCII characters
  in the default field, and exactly 35 in the extended field. At each maximum,
  try one additional ASCII character and verify the confirmed callback never
  exposes an over-limit value.
- [ ] Confirm precomposed accented text such as `é` (U+00E9), then separately
  confirm the decomposed sequence `e` followed by U+0301. Inspect the callback
  value or a bounded diagnostic so the decomposed case is not silently mistaken
  for the one-unit precomposed form.
- [ ] Confirm CJK text containing `漢` (U+6F22) and non-BMP text containing
  U+1F600. Verify U+1F600 consumes two UTF-16 units and that no confirmed value
  contains an unmatched surrogate.
- [ ] For every Unicode category above, reopen the owning page and verify the
  confirmed value persists exactly. Start another edit, change the value, then
  Cancel; verify the previous value remains and no callback fires. Confirm a
  different value and verify exactly one callback fires with the new value,
  then reopen once more to verify persistence.
- [ ] Repeat confirm and Cancel after Next/Previous pagination, with the
  optional GFX files installed and absent. Verify focus returns to the owning
  row, underlying navigation does not receive the editing input, and page
  destruction/recreation does not retain a stale editor or borrowed callback
  view.

### Input-binding live validation (API 1.1)

Run this matrix with at least two providers, two sections, and several actions.
Use stable, distinct provider and action IDs and a bounded diagnostic counter
or log so callback counts can be checked without making input timing ambiguous.

- [ ] Choose one provider that opts into `menu.storage()` and one that does not
  open storage. With the game closed, temporarily move the opted-in provider's
  existing `mods/<provider_id>/config.ini` aside. Confirm opening and loading
  the default storage path treats the missing file as an empty document without
  creating it. Start once with no assigned framework actions and confirm the
  host does not modify any official Elden Ring binding.
- [ ] Open Button Settings. Confirm each provider section appears once in
  deterministic provider/registration order, its actions preserve declaration
  order, every action has one controller assignment cell, headers are
  nonselectable, scrolling is native, and repeated close/reopen creates no
  duplicate rows.
- [ ] Open Keyboard/Mouse Settings. Confirm the same sections and action order
  appear once, with an independently editable keyboard cell and mouse cell for
  every logical action. Verify native scrolling, focus, help, Back, and
  close/reopen behavior.
- [ ] For the controller alternative, assign a face or directional button,
  begin another assignment and Cancel it, Clear the value, then reassign it.
  Confirm the accepted value changes immediately, Cancel preserves the prior
  value, and Clear displays the native unbound value.
- [ ] Repeat the assign/Cancel/Clear/reassign sequence independently for the
  keyboard and mouse alternatives. Put controller, keyboard, and mouse values
  on the same logical action and verify changing one never replaces either of
  the other two.
- [ ] For the opted-in provider, have its assignment-change handler apply the
  event to a storage section and explicitly save the document. Confirm the
  default `mods/<provider_id>/config.ini` is created only by that save. For the
  storage-free provider, confirm the same edits remain runtime-only and no
  provider configuration file is created implicitly.
- [ ] Exercise at least a controller face button, shoulder button, trigger, and
  D-pad direction, an ordinary unmodified keyboard key, and a mouse button.
  Press once, hold, release, and press again: expect one callback on each fresh
  rising edge and no per-frame repeat while held.
- [ ] Close and reopen both binding screens, then exit the game completely and
  start a new process. Confirm the opted-in client explicitly opens and loads
  its storage, reads the saved `ActionInputs`, and binds them before commit;
  the storage-free provider must start from its declared defaults. Confirm
  localized label changes or moving an action to another section do not lose a
  saved assignment when provider ID and action ID remain unchanged.
- [ ] Assign the same physical input to several actions across both providers.
  Confirm no framework conflict disables it and one fresh press invokes every
  matching callback exactly once. Confirm official bindings using the same
  input remain unchanged and still execute normally.
- [ ] Verify global callback observation in gameplay, an ordinary in-game
  menu, the title screen, and character creation. ERNativeUI does not consume
  ordinary input, so use callback counts to distinguish dispatch from whatever
  the game itself also does in each context.
- [ ] While editing ERNativeUI TextInput, enter a bound keyboard character and
  confirm it reaches the editor without invoking the binding callback. Repeat
  in an official native text editor such as character-name entry. Close the
  editor, release the input, and confirm a later fresh press invokes once.
- [ ] While an ERNativeUI native dialog owns input, verify menu movement, row
  activation, and Back do not leak to the underlying title, character-creation,
  root, or subpage UI. Confirm the dialog's own OK/Cancel remains usable and a
  held activation input does not enqueue another binding callback.
- [ ] During both an official binding capture and an ERNativeUI binding
  capture, press another registered action's input. Confirm no framework
  callback fires during remapping. Complete and Cancel captures, release the
  input, then confirm a new press re-arms and invokes normally.
- [ ] Re-test an ordinary official controller row and an ordinary official
  keyboard/mouse row end to end: assign, Cancel, Clear, conflict display, save,
  close/reopen, and process restart must retain vanilla behavior.
- [ ] Repeat with the optional GFX patches both installed and absent. Binding
  screens must behave identically because their model extension requires no
  `02_160_keyconfiguration.gfx` patch.
- [ ] Restore the original provider configuration after the game
  exits and confirm no client-generated provider configuration is included in
  an SDK or runtime package.

When a game update changes code, follow
[the native research
methodology](docs/research/methodology.md#8-maintain-the-map-after-a-game-update)
and record the new PE identity, AOB result, validated fallback RVA, and live
test evidence. Never patch `eldenring.exe` to distribute ERNativeUI.
