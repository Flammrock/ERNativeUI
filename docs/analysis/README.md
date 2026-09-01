# ERNativeUI reverse-engineering notes

This directory is the working research notebook for native Elden Ring UI
features that are not yet part of ERNativeUI's supported public API. It exists
so experiments, failures, object layouts, ownership observations, and address
derivations remain reviewable instead of becoming unexplained constants in the
production code.

These notes are evidence, not an ABI promise. Production signatures and
validation live in `src/addresses.cpp`; the public contract lives in
`include/ernativeui/erui.h`.

## Evidence vocabulary

- **Confirmed**: reproduced in a static artifact or by a controlled in-game
  test, with the tested game build recorded.
- **Inferred**: the evidence strongly supports the statement, but the exact
  native implementation has not yet been observed.
- **Hypothesis**: a candidate explanation or test target.
- **Rejected**: an experiment disproved the candidate. Rejected paths remain
  documented because they prevent future investigators repeating unsafe work.

Every runtime experiment should record the game PE timestamp and image size,
the ERNativeUI commit/build, the exact player actions, the expected result, and
the observed result. Logs must be bounded by count and time. Unknown native
functions must not be called merely because their addresses are close to known
functions.

## Investigations

- [Current native UI hooks and object map](CURRENT_NATIVE_UI_HOOKS.md): the
  production hook/call inventory, confirmed object offsets, state transitions,
  present construction boundary, and prioritized architectural unknowns.
- [Native UI architecture](NATIVE_UI_ARCHITECTURE.md): end-to-end synthesis of
  GFX resources, MovieDef/player runtime, native pages/controllers,
  scene-object values, input/jobs/dialogs, and teardown boundaries.
- [Whole-executable analysis](EXECUTABLE_ANALYSIS.md): reproducible Ghidra
  database, function/RVA mapping, and targeted UI-subsystem reconstruction.
- [Conservative Ghidra profile](GHIDRA_ANALYSIS_PROFILE.md): bounded analyzer
  settings for large optimized PE files and their explicit accuracy tradeoffs.
- [Curated Ghidra export findings](GHIDRA_EXPORT_FINDINGS.md): new dedicated
  TextInput-movie and menu-resource-path leads, plus generic Scaleform runtime
  landmarks that should not yet be treated as game hook boundaries.
- [Native UI class hierarchy](NATIVE_UI_CLASS_HIERARCHY.md): exact recovered
  MenuWindow, SceneObj, and MenuJob base graphs, representative UI families,
  multiple-inheritance locators, and safe interpretation limits.
- [Game Options native class map](GAME_OPTIONS_CLASS_MAP.md): exact
  Configuration top/panel RTTI, fixed category storage, tab-selection and
  panel dispatch, construction evidence, and the remaining Audio/Graphics and
  custom-tab boundaries.
- [GFX resource catalog and reuse boundaries](GFX_RESOURCE_CATALOG.md):
  logical/path conventions, resident versus on-demand layers, curated
  option/dialog/text-input anchors, safe reuse levels, and discovery method.
- [Known native UI symbol map](address-map/README.md): build-locked analytical
  names, evidence, and a guarded Ghidra importer for already-proven interfaces.
- [Scaleform/GFX presentation model](SCALEFORM_GFX_MODEL.md): confirmed display
  tree, shared widget definitions, native path/text bridge, and extension
  boundaries.
- [Scene-object proxy and native Scaleform bridge](SCALEFORM_NATIVE_BRIDGE.md):
  confirmed proxy/value layout, slash-separated member resolution, direct
  TextField operations, and the bounded Invoke/CreateObject negative result.
- [UI extension strategy](UI_EXTENSION_STRATEGY.md): production-safe extension
  layers, hook/input/lifetime boundaries, compatibility policy, no-go areas,
  and the recommended phased ERNativeUI architecture.
- [Scaleform executable surface](SCALEFORM_EXPORTS.md): PE exports, validated
  MSVC RTTI hierarchies, vtable seeds, movie/value/input anchors, and the
  boundary between the statically linked GFx runtime and Elden Ring wrappers.
- [Scaleform movie lifecycle](SCALEFORM_MOVIE_LIFECYCLE.md): startup state
  machine, loader states, GFX path fallback, MovieDef/player ownership,
  per-frame advance, teardown, and the confirmed no-op FSCommand handler.
- [Native UI input and event dispatch](UI_EVENT_DISPATCH.md): `MenuWindow`
  virtual flow, row/controller input gates, Back job submission, dedicated
  text-input keyboard/mouse forwarding, focus, lifetime, and open probes.
- [Scaleform SDK reference model](SCALEFORM_REFERENCE_MODEL.md): public loader,
  movie, value, input, rendering, and lifetime concepts used to guide—but not
  substitute for—the Elden Ring binary analysis.
- [Native text input](TEXT_INPUT.md): confirmed editor resources and native
  activation path, guarded live results, idle-frame GFX prototype, unreleased
  API constraints, and remaining validation questions.
- [API version compatibility test plan](API_VERSION_COMPATIBILITY_TESTS.md):
  implemented frozen-release header fixtures, bidirectional host/client
  negotiation tests, and the remaining ABI regression gates for API 1.1.

As broader Scaleform behavior becomes established, common findings should be
moved into focused documents here and linked from the feature investigation
that supplied the evidence.
