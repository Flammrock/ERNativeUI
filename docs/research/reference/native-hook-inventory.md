# Current native hook and call inventory

Status: **source-verified production inventory** for the current ERUI API 1.1
implementation. This page was reconciled against the current host, runtime,
resolver, and feature backends; it is not copied from the historical symbol
CSV or the earlier research notebooks.

The names below are ERNativeUI's analytical source names, not symbols supplied
by FromSoftware. The 2.7.0.0 baseline remains the
[reference executable](../README.md#reference-build); explicit 2.7.1.0 deltas
are shown where a production profile differs. Generic resolver fallback
constants are not presented as current addresses: they are accepted only when
their local validation still succeeds, while the unique AOB result is
authoritative.

This inventory documents private implementation boundaries. Mod clients must
use the public [C ABI](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) or
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp).

## Reading the inventory

The current source uses four resolution classes:

| Class | Production rule |
|---|---|
| Semantic AOB | Scan executable sections, require exactly one match, and derive a `rel32` call target when the semantic anchor is a caller rather than the callee entry. |
| Validated fallback | If an AOB is missing or ambiguous, accept the source fallback RVA only when it is in executable memory and matches the required local bytes or structure. It is never an unchecked fallback. |
| Build-gated AOB | First select one recognized PE timestamp/image-size profile, then resolve each entry with a unique AOB or validated fallback. TextInput uses this class because it retains layout-coupled native state. |
| Exact-RVA feature backend | Select one recognized PE timestamp/image-size profile, fetch every feature-local RVA from it, and validate the expected entry bytes plus required data anchors. ColorPicker and Input Bindings use this class. |

The complete executable SHA-256 is recorded for research and release testing,
but the injected runtime does not calculate the hash. Its recognized profiles
are:

| Game build | PE timestamp | Image size |
|---|---:|---:|
| `2.7.0.0` | `0x69E9C9B9` | `0x5E09600` |
| `2.7.1.0` | `0x6A96B418` | `0x5E0DA00` |

Both fields must match one profile. Local byte, vtable, executable-memory, and
data validation then remains mandatory. The complete comparison is recorded
in the [2.7.1.0 update note](../game-updates/elden-ring-2.7.1.0.md).

Hooks are installed with SafetyHook unless the table says otherwise. All
SafetyHook inline hooks are prepared disabled, their original trampolines are
published, and only then are they enabled. The native text resolver uses
MinHook. The shared Scaleform resolver bridge is a SafetyHook mid-hook that
changes a register only in an owned, thread-local construction scope.

## Installation coordinator

The host does not install a fixed global hook set. It freezes provider
registration, compiles one immutable menu, and derives the required native
features from reachable rows. The relevant control flow is in
the current online [`host_dllmain.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_dllmain.cpp),
[`runtime.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/runtime.cpp), and
[`hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp).

| Compiled requirement | Resolution/install consequence | Failure scope |
|---|---|---|
| Any menu runtime | The base row path and Game Options materializer are mandatory. | Installation fails before becoming ready. |
| Rows on another supported built-in settings page | That page's materializer is resolved and hooked independently. | Only that destination is disabled; other pages remain active. |
| Button, ColorPicker, pagination, or submenu route | Action-row construction dependencies are required. | Installation fails if a required shared dependency is absent. |
| Submenu or continuation page | Subpage materializer and native page-open route are required. | Installation fails. Optional title presentation may still degrade separately. |
| Popup choice | Constructor/callable dependencies and both scoped list hooks are required. | Installation fails and the partial hook set is reset. |
| TextInput | All build-gated TextInput interfaces and the shared page-frame hook are required. | Installation fails. A later row-construction fault disables TextInput for that process. |
| Native alert | Custom text, page-frame/Back gates, and the semantic dialog transport are attempted. | Alert support is disabled, but ordinary menu rows remain active. |
| ColorPicker | Native alert transport, exact ColorPicker backend, and preview-producer hook are required. | If a ColorPicker row was declared, installation fails rather than silently omitting it. |
| Input Bindings | Exact backend preparation and all nine binding hooks are required. | Installation fails and unpublished state is cleared. |
| Page titles or standalone ColorPicker widget | The shared Scaleform path mid-hook is attempted. | Rows remain active. Titles are disabled; ColorPicker preserves ordinary button presentation when its standalone widget cannot be routed. |

The base resolver completes before core hooks are installed. A missing required
address therefore produces the explicit "no hooks installed" path. Later
feature installers create complete disabled hook groups and unwind on a hard
failure. Built-in destination hooks and native alerts are the deliberate
feature-local exceptions.

## Core settings pages, rows, navigation, and text

The shared address set and its dependency predicates live in
the current online [`addresses.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.hpp) and
[`addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp). The detours and install policy
live in [`hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp).

### Production detours

| Target in current source | Condition | Owner and scope | Resolution | Behavior on failure |
|---|---|---|---|---|
| `GameOptionsHandlerFn` | Always, because row injection is the runtime's base feature. | The live Game Options page. Calls the complete validated chain once, publishes the current page for modal gating, reads visual capacity and the post-chain materialized count, then injects only the fitting root slice subject to the cooldown. Earlier compatible rows therefore consume real capacity. | Direct semantic AOB, then byte-validated fallback. | Hard installation failure. |
| Camera, Display, Sound, Network, Keyboard/Mouse, and Graphics `BuiltinPanelMaterializerFn` entries | Only when providers declared rows for that destination. | One concrete native category page per detour. Calls vanilla once, validates the page/vtable, reads live native count and visual capacity, then appends only the fitting slice. | Independent direct semantic AOBs, each with a byte-validated fallback. | Local degradation for that destination; installation continues. |
| `SubHandlerFn` | Any provider submenu, root continuation, or built-in continuation. | The existing Advanced Settings-style physical page. Resolves only ERNativeUI-owned pending/bound routes; unrelated pages are passed to vanilla. | Direct semantic AOB, then byte-validated fallback. | Hard installation failure when the compiled menu needs the route. |
| `TextResolverFn` | Custom text enabled; the production host enables it. | Five-argument native UI result-object resolver. Rewrites only host text IDs, scoped root-button IDs, and live dialog IDs; every other ID calls vanilla. | Direct semantic AOB/prologue, then byte-validated fallback. | Hard core-hook failure. Uses MinHook rather than SafetyHook. |
| `PageFrameFn` | Required by TextInput; also reused when native alerts are available. | One exact ERNativeUI physical settings page. Synchronizes pending TextInput values and ColorPicker previews, then suppresses only that page's frame input while an owned alert holds the popup slot. | Direct semantic AOB, then byte-validated fallback. | Hard for a declared TextInput. For alerts alone, alert support degrades locally. |
| `NativeBackFn` | Resolved when pagination/built-in continuation needs native Back; detoured as part of alert input ownership. | Native page action wrapper. Allows the owned popup's secondary action, blocks underlying Back during an alert, and records parent restoration after a normal Back. | Direct semantic AOB, then byte-validated fallback. | Hard when required for pagination. Missing alert-only gating disables alerts without disabling rows. |
| `ScaleformPathResolverFn` mid-hook | A complete title bridge or an installed ColorPicker backend. | Only `MenuTitle/Text_0` during an owned page construction, or `Widgets/Button` during an owned ColorPicker row construction. Thread-local identity leaves all unrelated path lookups untouched. | Direct semantic AOB, then byte-validated fallback. | Presentation-only degradation; no row or callback is removed. |

The six generic built-in destinations intentionally exclude the real
Controller Settings/Button Settings screen. API 1.1 extends that specialized
screen only through the Input Bindings backend. See
[Settings pages and pagination](../case-studies/settings-pages-and-pagination.md).

### Called but not detoured

| Boundary | Used for | Resolution and requirement |
|---|---|---|
| `BufferConstructorFn on_off_list` and `menu_context` | Native toggle/slider support objects. | Direct AOB plus validated fallback; mandatory in the base row path. |
| `TextReferenceFn text_ref_help` and `text_ref_label` | Temporary native label/help references for rows. | Help uses direct AOB; label derives a `rel32` call target. Both have validation and are mandatory. |
| `root_button_text_references` and `root_button_display_text` | The richer reference pair and display value used by native action rows. | Direct AOB plus validated fallback; conditional on required action rows. |
| `AddToggleFn`, `AddSliderFn`, and `AddInlineChoiceFn` | Construct native toggle, slider, and inline selector rows. | Semantic call-site AOB to `rel32` target, then validated callee fallback; mandatory in the current base row path. |
| `ChoiceContextConstructorFn` and `ChoiceListBuilderFn` | Create the inline-choice context and owned option list. | Direct AOB plus validated fallback. Although resolved through the optional helper, base-row completeness makes them mandatory. |
| `AddButtonFn` | Construct action, submenu, navigation, and ColorPicker controller rows. | Direct AOB plus validated fallback; conditional on the compiled menu's action needs. |
| `DestroyTextReferencesFn` | Release temporary row text-reference sources after the native row clones them. | Direct AOB plus validated fallback; required with all row constructors. |
| `OpenSubPageFn` | Push a real native child page for submenus and Next. | Direct AOB plus validated fallback; required with physical child routes. |
| `NativeBackFn` direct call | Perform Previous through the game's own fade/task/stack path. | Same resolved wrapper as the Back gate; required when pagination may need Previous. |
| `ScaleformTextSetterFn` and `ScaleformResultDestructorFn` | Write a captured physical-page title and release a temporary scene-object result. | Optional direct AOBs plus validated fallbacks; incomplete title bridge disables custom titles only. |

### Game Options action-row text contract

Game Options action rows need a richer native text path than action rows on a
child page. The two resolved helpers above construct different temporary
objects, and treating them as the ordinary label/help pair is what originally
produced missing or corrupted left-side labels.

The high-level five-argument UI resolver returns an object with this observed
layout:

| Offset | Field |
|---:|---|
| `+0x00` | Direct resolved-text pointer |
| `+0x10` | Inline UTF-16 storage, or an allocation pointer when capacity exceeds seven code units |
| `+0x20` | UTF-16 length |
| `+0x28` | Capacity; values through seven use inline storage |

The root action display-text temporary occupies `0x60` bytes. Its allocator
owner is at `+0x08`, its allocation pointer is at `+0x10`, and its capacity is
at `+0x28`. When the capacity is greater than seven, cleanup calls the owner
vtable method at byte offset `+0x68` with that allocation. The separate root
text-reference storage is a `0x140`-byte caller allocation; its nested
help/native reference begins at `+0x38`. These are validated caller-side
storage requirements, not recovered complete C++ class sizes.

The native root constructor asks the text resolver for fixed vanilla IDs
`0x1B199` (label) and `0x0BC2` (help). ERNativeUI establishes a thread-local
construction scope that remaps only those two requests to the current row's
host IDs; all unrelated resolver calls remain vanilla. After the native row
has cloned its inputs, ERNativeUI destroys the temporary references and
releases any out-of-line display-text allocation through the matching native
owner.

Presentation has one further requirement: the Game Options GFX must expose the
left-side field at `Button/Text_1/Text`. The optional bundled patch supplies
that missing path with the same alignment and filters as other row labels.
This GFX field fixes presentation only; it does not replace any native call or
hook. Current implementation anchors are
[`native_menu.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_menu.cpp),
[`native_text_result.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_text_result.cpp), and
[`root_button_text_override.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/root_button_text_override.hpp).

The current source validates live page identity, counts, capacity, and route
ownership again at the call site. Address resolution is not treated as proof
that an arbitrary pointer is a compatible page.

## Popup-choice rows

Popup choice uses the generic AOB/fallback resolver in `addresses.cpp`, then a
narrow two-hook bridge in
the current online [`native_popup_choice.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_popup_choice.cpp).

### Detours

| Target | Scope | Resolution | Failure behavior |
|---|---|---|---|
| `PopupChoiceListProviderFn` | Reads the provider's captured state, maps it to one exact compiled popup row, and establishes a nested thread-local ownership scope around the original provider call. | Direct semantic AOB plus validated fallback. | Hard when any reachable popup-choice row exists. |
| `PopupChoiceListTemplateFn` | Calls the original initializer, then rebuilds the fixed native list only inside the owned provider scope. Vanilla lists remain untouched. | Direct semantic AOB plus validated fallback. | Hard at install. A runtime structural mismatch leaves the vanilla template intact or exposes only its fully constructed prefix. |

The inner template hook is enabled before the outer provider hook; removal
stops the outer hook first. This ordering prevents an outer owned scope from
entering after the inner bridge has disappeared.

### Called or retained boundaries

| Boundary | Role | Resolution |
|---|---|---|
| `PopupChoiceConstructorFn` | Builds the action-style selector from value, commit, and presentation callables. | Direct semantic AOB plus validated fallback. |
| value-provider and selection invoke entries | Validate the erased callable contracts used for read and commit. | Direct AOB plus validated fallback. |
| value, selection, and presentation callable vtables | Construct native-compatible erased callable shells around host-owned stable state. | Scan non-executable sections for one three-entry vtable whose invoke slot equals the resolved function and whose entries are executable; otherwise validate the known fallback vtable. |
| `TextReferenceFn text_ref_label` | Reconstruct each owned option label in the native list. | Shared core boundary. |

All dependencies and both hooks are conditional on at least one compiled popup
choice. Any unresolved constructor, invoke, vtable, provider, or template
rejects installation. The complete contract and fixed 32-entry list ownership
are in [Native popup-choice rows](../case-studies/popup-choice.md).

## TextInput

TextInput adds no feature-local detour. It uses exact native construction calls
and the shared `PageFrameFn` hook described above. The address block in
the current online [`addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp) first selects a recognized PE profile,
then uses build-gated AOBs with validated fallbacks.

| Boundary | Reference-build RVA | Production role |
|---|---:|---|
| `TextInputRowProducerFn` | `0x976EF0` | Construct one native editable settings row and retain its bound value/editor callable. |
| `NativeMenuStringConstructorFn` | `0x5EE0F0` | Construct the host-owned native value and temporary initial-text objects. |
| `NativeMenuStringBorrowedConstructorFn` | `0x6766F0` | Construct the temporary borrowed placeholder. |
| `NativeMenuStringDestructorFn` | `0x1BCC60` | Destroy host-owned or temporary native menu strings through the matching native contract. |
| `TextInputEditorFactoryBuilderFn` | `0x915D70` | Build the erased editor-factory callable passed to the row producer. |
| `TextInputEditorFactoryFn` | `0x81D610` | Create the character-creation-style editor job with its native input and length policy. |

All six RVAs are identical in the 2.7.1.0 profile. Their entry patterns and
the supporting vtables were independently compared before the new identity
was admitted.

Before publishing a row, production validates the parent as the expected
OptionSetting or PadSetting page and bounds its property count. After the call,
it requires exactly one new EditProperty row, the expected TextInput controller
vtable, and the exact host-owned bound-value pointer. Editor activation also
checks the resulting job vtable. A mismatch or native exception disables all
TextInput rows for the rest of the process rather than continuing with an
uncertain native object.

Preparation rejects reuse of one TextInput state by multiple logical rows.
Programmatic updates are staged from arbitrary host/client activity and copied
into the native string only at the page-frame UI boundary. See
[Native TextInput](../case-studies/text-input.md).

## Native generic dialogs

The alert backend in
the current online [`native_dialog.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog.cpp) has no RVA fallback
table. It resolves a set of unique semantic call sites and derives their
`rel32` targets and RIP-relative owner slot. All boundaries must resolve before
any of its four disabled hooks are published.

### Detours

| Target | Owner and scope | Resolution | Failure behavior |
|---|---|---|---|
| lower frontend update | Skips the complete lower frontend tree only while the exact ERNativeUI job owns the popup blocking slot. This stops title/character-creation siblings from consuming the alert's input. | Derived as the first target of the unique two-call frontend update anchor, then validated against its concrete entry bytes. | Alert transport is unavailable; menu rows remain active. |
| TitleTop dialog update | Runs the title task with a private disabled-input byte while an owned alert is active, preserving its normal visual/lifetime frame without input bleed. | Unique direct semantic AOB. | Same local alert degradation. |
| popup update | Gives only the owned popup a private enabled-input byte, observes blocking-slot completion, services queued work, and establishes the narrow scope in which secondary action `3` is valid popup input. | Derived as the second target of the unique frontend update anchor. | Same local alert degradation. |
| `MenuWindowJob` poll | Calls vanilla, normalizes left/right terminal results for the exact submitted alert job, and also forwards ColorPicker job observations. | Unique direct semantic AOB; reference release target is derived from a call inside the resolved function. | Same local alert degradation; a declared ColorPicker consequently cannot install. |

### Called or discovered boundaries

| Boundary | Derivation and role |
|---|---|
| frontend-manager/popup slot | RIP-relative data reference in the semantic scheduler wrapper; reaches the current popup owner and its blocking-job slot. |
| dialog initialize, set fields, set fixed/global option, and build | Five `rel32` targets decoded from one unique native dialog wrapper. They build the supported zero-, one-, and two-button descriptors. |
| blocking-job install | `rel32` target of the unique call site that assigns into popup `+0x298`; it consumes the submitted intrusive reference. |
| reference release | `rel32` target inside the validated job poll; closes the returned temporary reference and destroys the object when its native count reaches zero. |

The transport never replaces a non-null game-owned blocking task. Page-frame
and Back gates are installed before the transport and restrict suppression to
the exact popup/job/page relationship. If any semantic signature or hook
creation/enable step fails, the alert hook group is removed and the menu
runtime continues. If ColorPicker was declared, its stronger dependency rule
then converts the missing alert transport into an installation failure.

See [Native generic dialogs](../case-studies/native-dialogs.md) for ownership,
button-result normalization, queueing, and the bounded reproduction procedure.

## ColorPicker

The backend in the current online [`color_picker.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/color_picker.cpp) selects an exact
profile, then validates every RVA below. The recorded SHA-256 is a
release/research identity, not an in-process hash check.

### Feature-local detour

| Target | Reference-build RVA | Scope | Failure behavior |
|---|---:|---|---|
| `GameOptionsActionWidgetProducerFn` | `0x86B940` | During one thread-local ColorPicker row construction, preflights `Widgets/ColorPicker`, hides `Widgets/Button`, and lets the original native action producer bind its controller to the standalone sibling. Unowned rows call vanilla unchanged. | Hard if a ColorPicker row was declared. A later missing GFX widget preserves ordinary button presentation. |

The shared Scaleform path mid-hook supplies the matching one-call path rewrite
from `Widgets/Button` to `Widgets/ColorPicker`. Its failure is presentation
degradation, not backend failure; the action controller and modal route remain
valid.

### Exact called boundaries

| Boundary | Reference-build RVA | Production role |
|---|---:|---|
| build `MenuWindowJob` | `0x7AD980` | Build the page-owned native modal job from a movie descriptor and factory. |
| consuming queue submission | `0x7AA0D0` | Submit the job into the current page's queue and consume the caller's pointer. |
| heap provider | `0x7A8120` | Obtain the native allocation owner. |
| game allocation dispatch | `0x1EBBCD0` | Allocate the palette and color-control objects with native alignment/heap semantics. |
| palette constructor/destructor/populate | `0x77C620` / `0x77CA10` / `0x77CF90` | Own and populate the character-creation palette. |
| scene-proxy bridge/destructor | `0x7460D0` / `0xD81590` | Convert the live page/movie proxy for the editor and release temporary proxy state. |
| color-control constructor | `0x8B6F90` | Construct the `04_031_ChrMake_ColorEditor` controller and live completion callable. |
| movie-name data | `0x2AB8DE8` | Must validate as the expected `04_031_ChrMake_ColorEditor` UTF-16 literal. |
| Scaleform path resolver | `0x74B140` | Resolve only known existing widget/fill paths; also supplies the target for the shared path bridge if the generic title resolver was unavailable. |
| visibility setter | `0x734190` | Hide or show a validated standalone widget object. Normally pristine; the narrowly supported Solid Uncapper call chain is documented below. |
| value-exists/color-transform setter | `0x733FA0` / `0xD85610` | Preflight and update the standalone widget's current RGB swatch. |

The 2.7.1.0 profile changes only these three entries:

| Boundary | 2.7.0.0 | 2.7.1.0 |
|---|---:|---:|
| game allocation dispatch | `0x1EBBCD0` | `0x1EBBD40` |
| temporary SceneObjProxy destructor | `0xD81590` | `0xD81600` |
| Scaleform color-transform setter | `0xD85610` | `0xD85680` |

Every executable entry normally must match its long local pattern, every data
pointer must validate, and the dialog transport must already be active. The
only supported exception is an early-captured visibility setter whose current
entry is either pristine or a validated Solid Uncapper-owned `FF 25` detour;
it is revalidated immediately before publication. One failed validation
rejects ColorPicker installation. The normal dialog job-poll hook observes
accept/cancel for the exact active ColorPicker job. Outstanding native
sessions make hot removal unsupported; the host rejects new requests and
keeps resolved native entries alive through process exit.

See [Native color picker](../case-studies/color-picker.md) and
[GFX presentation](../case-studies/gfx-presentation.md).

## Input Bindings

The backend in
the current online [`native_input_bindings.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_input_bindings.cpp) is
independent of the generic built-in-page materializers. It extends the
specialized scrolling binding model and samples player input globally. Any
declared input action activates the exact recognized-build profile gate and
requires the complete backend.

### Production detours

| Target | Reference-build RVA | Owner and scope |
|---|---:|---|
| build key-setting list | `0x869580` | Calls vanilla, validates the exact KeyConfig dialog/list/config shape, then appends host-owned category and action cells for controller or keyboard/mouse mode. |
| poll key capture | `0x868C60` | Marks all native remapping as exclusive input; for an exact owned row, decodes and commits a supported player assignment while preserving vanilla behavior for official rows. |
| clear key setting | `0x8687D0` | Clears only the selected supported slot of an owned action and emits a player-clear change; official rows call vanilla. |
| refresh key conflict | `0x868810` | Forces no conflict for exact owned definitions because duplicate mod bindings are intentional and do not belong to Elden Ring's fixed action bank; official rows call vanilla. |
| input-manager update | `0x266A480` | Calls vanilla, applies staged assignments, queries player-0 physical input, detects released-to-pressed edges, and queues provider events. It does not consume the game's action. |
| SoftwareKeyboardJob constructor/destructor | `0x81CCB0` / `0x81CE40` | Track the lifetime of one native text-editor family so provider bindings cannot fire while text owns keyboard input. |
| TextInputDialog constructor/destructor | `0x9B9E00` / `0x9B9FC0` | Track the second native text-editor family for the same global callback suppression rule. |

All nine hooks are created before any are enabled. Destructors are enabled
before constructors; both editor families must be trackable before global
binding dispatch becomes live. Mutation/input guards are enabled before the
list builder can publish any host pointer. Any create or enable failure clears
the unpublished group and rejects runtime installation.

### Exact called boundaries and anchors

| Boundary | Reference-build RVA | Production role |
|---|---:|---|
| construct action cell | `0x868000` | Build one host-owned native setting cell around an ERNativeUI definition/value. |
| construct category spacer | `0x8696B0` | Build the native section/header cell. |
| append key setting | `0x869FD0` | Append validated temporary cells through the native list/vector owner. |
| write binding value | `0x242960` | Encode a supported native token into the five-word value shown by the binding UI. |
| token physical-ID conversion | `0x240220` | Convert a native assignment token to the physical input ID used by runtime queries. |
| token analog classifier | `0x2403C0` | Preserve whether the physical input must be sampled as analog. |
| get player input | `0x2413F0` | Resolve player 0's input-query owner from the validated input manager. |
| query input states | `0x2667AD0` | Batch-query prepared physical IDs after the native input-manager update. |
| bounded menu-config writer | `0x867C60` | Validation anchor only. Production deliberately does not write provider actions into Elden Ring's fixed save/config bank. |
| input-manager slot | `0x4861D30` | Data anchor proving that the detoured update belongs to the live singleton before sampling. |
| KeyConfig dialog/list/config vtables | `0x2B0DCC0` / `0x2AD6E70` / `0x2AD68D0` | Exact owner checks before list publication or mutation. |
| SoftwareKeyboardJob/TextInputDialog vtables | `0x2AC5AD0` / `0x2B2B908` | Exact identity checks for editor lifetime tracking. |

The 2.7.1.0 profile changes two executable entries:

| Boundary | 2.7.0.0 | 2.7.1.0 |
|---|---:|---:|
| query-input-states forwarding thunk | `0x2667AD0` | `0x2667B40` |
| input-manager update | `0x266A480` | `0x266A4F0` |

The singleton slot, native binding helpers, editor-lifetime functions, and
required vtables retain their baseline RVAs. The query-thunk validator was
also lengthened: it wildcards the six unstable neighboring bytes, then uses a
longer stable prefix from that adjacent function only as uniqueness context.

Each function is fetched at its exact RVA and normally must match its
feature-local pattern. Data anchors must lie inside the current image. The
only supported third-party exception is described below: after an early
pristine capture, the two known Solid Uncapper overlaps may have one narrowly
validated owned-detour form. At runtime, unknown objects, threads, tokens, or
layouts fail closed; callbacks are discarded while remapping, a native text
editor, or an owned dialog has input focus.

After native cells have been published, hot removal retains the mutation and
editor guards plus backing storage because an open KeyConfig dialog may still
hold raw pointers. The production host is pinned for process lifetime. See
[Native input bindings](../case-studies/input-bindings.md) and the public
[input-value/codec reference](../../reference/input-values-and-codec.md).

## Solid Uncapper chaining

The only explicit third-party hook ownership path is in
the current online [`host_dllmain.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_dllmain.cpp). When
`Solid Uncapper.dll` is already loaded, ERNativeUI:

1. resolves and saves the shared menu interfaces, the complete Input Bindings
   interface set, and the ColorPicker visibility-setter entry before Solid
   Uncapper's asynchronous hook installation;
2. watches exactly the Game Options materializer, subpage materializer, and
   text resolver for at most 20 seconds and requires the three entries to be
   unchanged or all changed;
3. requires a 500 ms all-changed stable window;
4. if changed, accepts only the observed absolute-indirect jump shape whose
   destination lies in executable Solid Uncapper image memory; and
5. validates every captured Input Bindings entry again after the wait;
6. requires every input entry except `clear_key_setting` and
   `write_binding_value` to remain pristine;
7. accepts either known input overlap as changed only when its entry is an
   `FF 25` absolute-indirect detour whose resolved destination is executable
   and owned by `Solid Uncapper.dll`;
8. independently applies the same unchanged-or-owned-`FF 25` rule to the
   captured ColorPicker visibility setter at `0x734190`, while every other
   ColorPicker boundary retains pristine validation;
9. revalidates the input entries before native input calls and hook creation,
   and the visibility setter immediately before publishing ColorPicker; and
10. installs ERNativeUI through the captured interface sets and original game
    entry addresses so its hooks and direct calls form the validated chains.

A partial one- or two-target change, an unknown jump shape, or a destination
outside the detected module fails the three-entry menu path. The two input
overlaps and one ColorPicker overlap are checked independently because any of
them can still be pristine; a change to any other exact Input Bindings or
ColorPicker entry fails its feature path. If none of the three menu targets
changes before the timeout, installation proceeds through their captured
pristine entries. This mechanism does not claim compatibility with arbitrary
foreign detours on other feature-local entries.

The complete menu, two-entry Input Bindings, and one-entry ColorPicker chains
were live-tested on 2.7.0.0 and 2.7.1.0 with Solid Uncapper 2.3 loaded first.
The corresponding pristine paths were also regression-tested without Solid
Uncapper.

## Teardown and callback boundary

On normal runtime removal, Input Bindings is stopped before the menu hook set,
then TextInput native strings are released. The menu hook reset disables the
shared path bridge and dialog gates, removes popup-choice hooks, removes core
page hooks, removes the MinHook text resolver, and clears navigation state.
ColorPicker and the dialog transport are removed before the core reset.

Provider-facing callbacks do not execute from these native detours. Detours
update bounded host state or enqueue events; the pinned host worker calls
`poll_changes()`, dispatches native-dialog completions, and dispatches
ColorPicker completions. Native action adapters may run on the UI path, but
the public ownership/thread guarantees remain those documented in
[Lifecycle, errors, and limits](../../reference/lifecycle-errors-and-limits.md).

Hot unloading remains unsupported. This is a lifetime rule, not just an
installer limitation: native rows, erased callables, open pages, dialog jobs,
and binding cells can retain host-owned pointers after their construction call
has returned.

## Source of truth and update checklist

For a game update, use this page as a checklist, not as an address database:

1. record the new executable hash, timestamp, and image size;
2. run the generic resolver and classify every unique AOB, ambiguity, and
   validated fallback result;
3. re-derive dialog call targets and ownership instead of copying historical
   RVAs;
4. port each build-gated TextInput boundary and revalidate row/editor
   postconditions;
5. port every exact ColorPicker and Input Bindings entry, data anchor, vtable,
   field relationship, and teardown path;
6. exercise each conditional install and its negative failure path; and
7. repeat the in-game matrices in the linked case studies before changing the
   supported-build declaration.

The historical [address map](../address-map/README.md) is a navigation aid for
one prior analysis snapshot. Current production authority remains the source
files linked throughout this document. Exploratory API 1.2 targets such as
Site of Grace integration, reusable built-in page styles, provider-loaded GFX,
and custom tabs are intentionally absent: none has a production hook boundary
yet.
