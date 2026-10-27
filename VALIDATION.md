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

- exact Elden Ring PE identity matching, rejection of mixed timestamp/image-
  size pairs, distinct 2.7.0.0/2.7.1.0/2.7.2.0 native profiles, and the known
  moved core UI, ColorPicker, and Input Bindings RVAs;
- C ABI layout and immutable API 1.0, 1.1, 1.2, and 1.3 release fixtures,
  including the additive TextInput, ColorPicker, BuiltinPage, InputBindings,
  Storage, slider, custom-tab, and title-screen-text blocks;
- the current pre-release API 1.4 layout and negotiation contract, typed C++
  row references, atomic update descriptors, styled text, row availability,
  row visibility, and visibility-state publication/rollback behavior;
- localization-setting grammar and traversal rejection, Steam language-token
  classification, transient/terminal readiness settlement, guarded Steam
  initialization, bounded fail-open behavior, and unknown-locale preservation;
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

### Frozen API 1.3 contract

`tests/abi/releases/v1_3` is the immutable contract published by
`ERNativeUI-v1.3.0`. CMake verifies both frozen header hashes before building
the fixture. Its strict-C and C++17 clients exercise the exact 384-byte API 1.3
table against a version-exact contract host, while separate frozen clients run
against `ERNativeUIAbiHostCurrent`, the production-sourced isolated host.

The current-host smoke clients resolve only the public `ERUI_GetApi` export,
negotiate API 1.3, then register title-screen text, a custom tab, and an
ordinary row before committing. They do not depend on a test-only export.
Together with the retained API 1.0, 1.1, and 1.2 clients, this is the mandatory
backward-compatibility matrix for the current API 1.4 development host and a
required gate before the API 1.4 contract can be frozen and published.

## Cross-toolchain client validation

The host must use an MSVC-compatible C++ ABI because Elden Ring directly
consumes an MSVC `std::function` object in its native action-row constructor.
That restriction is not intended to cross the public C boundary: the API is
designed for MSVC, clang-cl, and MinGW-w64 client mods. A release must not
claim MinGW-w64 runtime compatibility until the complete gate below passes.

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

The API 1.3 release candidate has additional isolated cross-toolchain evidence.
On 2026-10-20 at commit `c2f7f17fd78949e7aaf573614b81332b6f2e1ab4`,
MSVC 19.51.36243 fixtures and MinGW-w64 GCC/G++ 15.2.0 produced these results:

- [x] Strict current-header C11 and C++17 compilation passed.
- [x] Current C/C++ and frozen API 1.1/1.2 C/C++ clients negotiated,
  registered, committed, and exercised representative accessors against the
  production-sourced MSVC host fixture.
- [x] A current C++ client and frozen API 1.0 C/C++ clients received and
  verified callbacks from MSVC-built fixtures, including no-value, value, and
  alert callbacks.
- [x] Every audit client was PE x86-64, resolved `ERUI_GetApi` dynamically,
  and had no `ERNativeUI.dll` import. The production-sourced current fixture
  exported only `ERUI_GetApi`.

This is strong isolated ABI evidence, but it is not the offline-game callback
test below. The 1.3.0 compatibility matrix therefore does not claim complete
in-game MinGW-w64 runtime qualification.

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
`showcase_screenshot_helper`, `custom_tabs_stress`, and `title_screen_text`.
Inspect each client DLL's imports: no client should import `ERNativeUI.dll`.
Inspect the host exports: only `ERUI_GetApi` is public.

## Packaging validation

Use the install or CPack ZIP output, not a manual ZIP of the working folder:

```bat
cmake --install build/preset-release --config Release --prefix dist\ERNativeUI
cmake --build build/preset-release --config Release --target package
```

The resulting package must contain the MIT license, third-party notices, full
dependency license texts, and both
`bin/examples/ShowcaseScreenshotHelper.dll` and its intentional
source-controlled `ShowcaseScreenshotHelper.ini`. It must not contain loose
GFX movies, `ERNativeUIGfxPatcher.exe`, the patcher's developer README, an
import library, static library, unmodified or research-only game assets, a log,
a runtime-generated provider configuration such as
`mods/<provider_id>/config.ini`, an old archive, or a local build directory.

## In-game validation

Run offline with Easy Anti-Cheat disabled and load the host before clients.
Set `[Logging] EnableLog = 1` for checks that inspect `ERNativeUI.log`; restore
the shipped default `0` after testing.
For the shipped showcase, verify:

1. `ERNativeUI.log` reports the expected `Selected exact native profile` game
   version, committed-provider count, and `ERNativeUI host ready`.
2. `Tarnished UI Showcase` rows appear in System -> Game Options.
3. Its provider name, rows, choices, help, and dialog messages use the active
   Elden Ring language; the `Localized Greeting` example does the same.
   Repeat once with `[Localization] Language = french` against a different
   game language and confirm both host pagination and clients use French.
   Repeat with `Language = vietnamese`: host pagination uses the Vietnamese
   locale and the public result preserves `vietnamese` with the unknown enum.
   Restore `Language = auto`, restart, and confirm Steam detection resumes.
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
12. With `[GFX] EnableRuntimePatching = 1` and no loose settings movie, confirm
    runtime GFX completion reports the original and completed capacities and
    the constructed page exposes the completed capacity. Repeat with a
    compatible independently modified movie and verify completed structures
    remain unchanged while missing structures are added. With one compatible
    mod row materialized first, verify the page planner subtracts that row
    from the free slots rather than treating it as provider capacity.
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

### Runtime GFX patching configuration

Complete this A/B matrix before release. Each change requires a full process
restart; do not treat an INI edit made while the game is running as a test.

The API 1.3 release candidate was live-smoke-tested with explicit enabled and
disabled values, and the enabled shipped default was restored. That result
proves configuration selection, but it does not by itself prove every detailed
fallback and capability condition below; unchecked items remain release work.

- [x] Restart with explicit `EnableRuntimePatching = 1`, then with `0`, and
  restore the repository's shipped enabled default.

- [ ] Start with `[GFX] EnableRuntimePatching = 1`. Confirm the runtime loader
  hook installs, supported settings movies reach their documented capacities,
  TextInput and ColorPicker use their completed presentations, and an API 1.3
  connection advertises `ERUI_CAP_TITLE_SCREEN_TEXT`.
- [ ] Remove `EnableRuntimePatching` from an existing INI and restart. Confirm
  the missing key preserves the enabled behavior for upgrades from older
  installations, then restore the explicit shipped default.
- [ ] Set `EnableRuntimePatching = 0`, restart, and confirm the ordinary log
  reports the intentional disabled state without a warning. Confirm no
  per-movie transformation occurs and the effective vanilla or mod-supplied
  GFX capacities remain unchanged.
- [ ] In the disabled run, confirm ordinary and custom pages paginate against
  the unchanged movie's live capacity. Exercise custom-tab L1/R1 navigation,
  wrapping, mouse selection, Back, close, and reopen to prove that the custom
  tab backend remains independent.
- [ ] In the disabled run with the stock, unmodified settings GFX, open
  TextInput and ColorPicker rows. Confirm editing, confirmation, cancellation,
  values, and callbacks remain functional through their fallback
  presentations, while the TextInput brackets/empty state and standalone
  ColorPicker swatch are absent.
- [ ] In the disabled run, confirm a valid API 1.3 connection omits
  `ERUI_CAP_TITLE_SCREEN_TEXT`, guarded clients skip their title lines cleanly,
  and no provider title text is inserted into `05_000_title.gfx`.
- [ ] Restore `EnableRuntimePatching = 1`, restart, and repeat the enabled
  smoke test so the shipped default does not inherit state from the disabled
  run.

### Physical System tab-slot configuration

Use the custom-tabs stress example for all three runs. Fully exit and restart
the game after each INI change; do not compare configurations within one
process.

- [x] Set `[Tabs] MinimumTabSlots = 8`. Confirm the ordinary eight-slot strip,
  custom-tab overflow markers, single and held L1/R1 movement, wrapping,
  pointer selection, Back, close, and reopen.
- [x] Set `MinimumTabSlots = 9`. Confirm nine physical slots are visible and
  selectable, the custom-tab viewport uses the added slot, and all navigation
  checks above remain stable.
- [x] Set `MinimumTabSlots = 10`. Confirm ten physical slots are visible and
  selectable without overlapping the shoulder prompts or leaving the screen,
  and repeat the complete navigation and reopen checks.
- [ ] For each run, confirm the ordinary log reports the requested minimum and
  completed authored GFX capacity as the expected value, with no `WARN`,
  `ERROR`, or `FATAL` entry. The visible selectable slots and successful
  navigation provide the live native-record/selector check without enabling
  high-volume diagnostics.
- [x] After closing System, reopen it immediately and confirm the custom icons
  are already painted on the first visible frame rather than briefly showing
  fallback icons. Repeat after waiting before reopening.
- [x] Restore the repository's shipped `MinimumTabSlots = 8` default after
  testing.
- [ ] Confirm values outside `8..10` fail closed to the default. There is no
  `MaximumTabSlots` setting.

These are physical strip capacities, not custom-tab registration limits. The
stress example still contributes 50 logical custom tabs in every run, and the
viewport must keep all of them reachable.

### Elden Ring 2.7.2.0 live validation

The 1.3.0 release candidate exposing ERUI API 1.3 was live-validated on Elden
Ring App 1.17.2 / executable product version 2.7.2.0. The tested
executable had SHA-256
`F9118FEA2CFD8FA3901E8060BFADB1552CB2471771DED8A5B662C4E298FFB328`, PE
timestamp `0x6AABF7E7`, image size `0x5DFD600`, and file size 86,973,008 bytes.

- [x] The host selected the exact 2.7.2.0 native profile and installed every
  required backend without selecting either older build profile.
- [x] Runtime GFX completion succeeded and the completed settings interfaces
  were usable in game.
- [x] The 50-custom-tab stress client registered 1,000 custom rows. Together
  with the eight native tabs, the runtime exposed 58 logical tabs.
- [x] L1/R1 navigation, wrapping, mouse selection, Back, closing, and
  reopening System all completed successfully.
- [x] The validation run ended with no `WARN`, `ERROR`, or `FATAL` entries in
  `ERNativeUI.log`.

This run did not include Solid Uncapper. Its separately documented 2.7.0.0
and 2.7.1.0 compatibility results must not be extended to 2.7.2.0 without a
dedicated regression.

### Elden Ring 2.7.1.0 and Solid Uncapper regression

The original 1.1.1 compatibility work was live-validated on both Elden Ring
2.7.0.0 and 2.7.1.0, with Solid Uncapper 2.3 loaded first and without Solid
Uncapper. The 1.3.0 release candidate was subsequently live-regression-tested
on 2.7.1.0 with Solid Uncapper loaded first: both title-screen text producers,
the ERNativeUI showcase, and the custom-tab stress example remained visible
and operational. Root pagination was also historically confirmed with both
the original six-row Game Options GFX and a separately transformed 13-row GFX.
Current releases complete compatible settings movies in memory, so keep both
the game-supplied and already-modified input checks below for future
regressions; static pattern matches alone do not prove the runtime hook chain.

- [x] On 2.7.1.0 without Solid Uncapper, confirm the exact profile is selected,
  the host reaches `ERNativeUI host ready`, and ERNativeUI rows and Input
  Bindings appear.
- [x] Load Solid Uncapper before ERNativeUI. Confirm Solid Uncapper's row and
  all ERNativeUI provider rows appear in the same Game Options session.
- [ ] Confirm the log classifies `clear_key_setting` and
  `write_binding_value` independently as pristine or as accepted Solid
  Uncapper-owned detours. A changed entry must use the supported `FF 25`
  absolute-indirect form and resolve into executable memory owned by
  `Solid Uncapper.dll`.
- [ ] Confirm the log independently classifies the ColorPicker Scaleform
  visibility setter at RVA `0x734190` as pristine or as an accepted Solid
  Uncapper-owned detour, then revalidates that entry immediately before the
  ColorPicker backend is published.
- [ ] Confirm every other exact Input Bindings function remains pristine. Test
  one deliberately foreign or malformed entry only in an isolated diagnostic
  build and verify that installation fails closed rather than trusting it.
- [ ] In Button Settings and Keyboard/Mouse Settings, remap and clear both
  ERNativeUI and Solid Uncapper actions. Close and reopen each screen, restart
  the game, and verify presentation and persistence for both mods.
- [ ] Activate controller, keyboard, and mouse actions in gameplay and an
  ordinary menu. Verify both mods' behavior remains reachable through the
  deliberate chain and no action fires twice.
- [ ] With Solid Uncapper loaded, open both the root and subpage ColorPicker,
  accept different colors, and verify each bracketed swatch updates
  immediately and independently. Reopen both editors after accept and cancel.
- [ ] With no loose `02_040_optionsetting.gfx`, confirm runtime completion
  expands the compatible game-supplied panel to 13 rows. The four Elden Ring
  rows plus Solid Uncapper must leave eight free slots. For overflowing
  content, the first slice must contain seven provider rows plus Next, with no
  fourteenth row. Verify Next/Previous and reopen behavior.
- [ ] Supply a compatible independently modified `02_040_optionsetting.gfx`
  that already has 13 rows. Confirm runtime completion preserves the existing
  placements byte-for-byte, does not duplicate presentation definitions, and
  produces the same pagination and reopen behavior.
- [x] Repeat the supported-build startup and pagination regression on Elden
  Ring 2.7.0.0, both with Solid Uncapper 2.3 loaded first and without it.
- [ ] Repeat navigation, TextInput, pagination, and one-/two-button dialog
  checks with Solid Uncapper loaded, then repeat the startup without it to
  exercise the pristine path for all three known overlap entries.

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
- [ ] Repeat confirm and Cancel after Next/Previous pagination first with the
  game-supplied settings movies and then with compatible independently
  modified movies. Verify runtime completion succeeds in both cases, focus
  returns to the owning row, underlying navigation does not receive the
  editing input, and page destruction/recreation does not retain a stale
  editor or borrowed callback view.

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
- [ ] Repeat with the game-supplied settings movies and compatible
  independently modified settings movies. Binding screens must behave
  identically because their model extension requires no
  `02_160_keyconfiguration.gfx` transformation.
- [ ] Restore the original provider configuration after the game
  exits and confirm no client-generated provider configuration is included in
  an SDK or runtime package.

When a game update changes code, follow
[the native research
methodology](docs/research/methodology.md#8-maintain-the-map-after-a-game-update)
and record the new PE identity, AOB result, validated fallback RVA, and live
test evidence. Never patch `eldenring.exe` to distribute ERNativeUI.
