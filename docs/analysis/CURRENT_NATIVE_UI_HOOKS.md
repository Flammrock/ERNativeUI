# Current native UI hooks and object map

Status: production inventory for executable analysis

This document is the bridge between ERNativeUI's working implementation and
the broader executable investigation. It records every native UI boundary the
host currently hooks or calls, the object fields it currently relies on, and
the parts of Elden Ring's UI architecture that are still unknown.

It is not a public ABI. The names are ERNativeUI analytical names rather than
FromSoftware symbols, and every address is private, version-sensitive game
implementation detail. The strict public contract remains
[`erui.h`](../../include/ernativeui/erui.h).

## Build identity and evidence key

The **current RVA** values below apply only to this analyzed executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The complete machine-readable map and evidence for those values is
[`eldenring_2.7.0.0_known_symbols.csv`](address-map/eldenring_2.7.0.0_known_symbols.csv).
Some constants in [`addresses.cpp`](../../src/addresses.cpp#L12) are validated
fallback RVAs from the preceding executable revision; production first scans
the loaded image and logs the address actually selected. Do not copy a fallback
constant into a new build map.

This document uses the repository-wide evidence vocabulary:

- **Confirmed** means static structure or controlled in-game behavior has
  reproduced the relationship.
- **Inferred** means the evidence supports the relationship, but the complete
  implementation or type has not been recovered.
- **Hypothesis** is an explicit future analysis target.
- **Rejected** records a disproved candidate.

## Architectural boundary today

ERNativeUI does not currently create arbitrary Scaleform objects. Its primary
extension mechanism is to ask Elden Ring's native Game Options layer to create
real rows and pages. Scaleform is touched directly only to resolve existing
named objects and set the two title fields.

```text
client DLLs
    -> stable ERNativeUI C ABI
    -> frozen/compiled host menu
    -> Game Options handler detours
    -> native page and row constructors
    -> unknown native row-to-widget binder
    -> existing 02_040 / 02_042 Scaleform instances

native input
    -> unknown focus/action dispatcher
    -> native row state or retained callback
    -> ERNativeUI's 50 ms host poll / client callback
```

The first, third, and final edges are implemented. The native
row-to-widget/focus middle is the most important missing part of the model.
The static destination tree is documented in
[`SCALEFORM_GFX_MODEL.md`](SCALEFORM_GFX_MODEL.md).

## Installed hook inventory

These are all production detours installed by the current host. Hooks whose
feature is not present in the compiled menu are omitted at runtime; the native
dialog transport is optional and failure does not disable menu rows. Hook
selection and dependency gating are in
[`install_native_menu_hooks`](../../src/hooks.cpp#L1168).

| Target (analytical name) | Current RVA | Hook | Confirmed production behavior | Source |
|---|---:|---|---|---|
| `GameOptions_ControllerSettingsHandler` | `0x959DF0` | SafetyHook inline | Calls the original six-slot handler first, records the live root page, then injects the root slice after a per-page cooldown. | [`hub_handler_detour`](../../src/hooks.cpp#L395) |
| `GameOptions_SubpageHandler` | `0x95B770` | SafetyHook inline | Resolves a pending/bound ERNativeUI route. Vanilla pages are forwarded; owned pages are materialized by ERNativeUI without calling the vanilla handler. | [`sub_handler_detour`](../../src/hooks.cpp#L495) |
| `NativeUIText_Resolve` | `0x764220` | MinHook inline | Passes unknown IDs through. For a registered ID it first asks the game to construct a valid result from vanilla ID `0x1B20A`, then patches the UTF-16 result. | [`text_resolver_detour`](../../src/hooks.cpp#L272), [`install_text_hook`](../../src/hooks.cpp#L885) |
| `Scaleform_ResolvePath` | `0x74B140` | SafetyHook mid | Observes `R8` as the path and, only during an owned pending page, replaces exact `MenuTitle/Text_0` with `GraphicOption/StaticText_111114`; it also captures `RCX` movie context and `RDX` result storage. | [`observe_scaleform_path`](../../src/hooks.cpp#L89), [`TitleCaptureState`](../../src/native_title_bridge.hpp#L47) |
| `GameOptions_AdvancedSettingsFrame` | `0x958FF0` | SafetyHook inline | Suppresses the exact captured physical page's frame/input routine while ERNativeUI owns a blocking dialog; otherwise forwards `(page, XMM1 float, R8 byte*)` unchanged. | [`page_frame_gate_detour`](../../src/hooks.cpp#L627) |
| `GameOptions_NativeBack` | `0x747CD0` | SafetyHook inline | While an ERNativeUI dialog is active, suppresses action `3` outside the owned popup update. Otherwise forwards the complete 64-bit action and republishes a restored parent page. This function is also called for synthetic Previous. | [`dialog_back_gate_detour`](../../src/hooks.cpp#L668), [`invoke_native_back`](../../src/hooks.cpp#L857) |
| `PopupChoice_PresentationProviderInvoke` | `0x9623A0` | SafetyHook inline | Resolves the captured state pointer at provider `+0x08` against ERNativeUI-owned popup rows and establishes a thread-local row scope around the original call. | [`list_provider_detour`](../../src/native_popup_choice.cpp#L174) |
| `PopupChoice_ConstructListTemplate` | `0x86A610` | SafetyHook inline | Calls the original list initializer, then replaces only an owned row's native elements and labels within the provider's thread-local scope. | [`list_template_detour`](../../src/native_popup_choice.cpp#L189) |
| `CSPopupMenu_Update` | `0x7EF6D0` | SafetyHook inline | Gives only an owned popup a private enabled byte, scopes dialog secondary-action dispatch, observes blocking-slot completion, collects retired text, and pumps the alert FIFO after the normal update. | [`popup_update_detour`](../../src/native_dialog.cpp#L462) |
| `MenuWindowJob_Poll` | `0x7AE040` | SafetyHook inline | Calls the original poll and records response kind `2` or `3` for the active ERNativeUI dialog job. | [`menu_window_job_poll_detour`](../../src/native_dialog.cpp#L515) |

### Hooking libraries and chaining

- SafetyHook owns the typed inline and mid hooks. Typed inline detours are used
  when ERNativeUI may skip the complete native function or must call a typed
  trampoline. The mid hook is used only to observe/change the path argument
  while continuing the native resolver.
- MinHook owns only the high-level text-resolver hook. It is kept because this
  hook predates the other bridges and its five-argument result-object boundary
  is already validated.
- If `Solid Uncapper.dll` is present, ERNativeUI resolves pristine interfaces
  first, waits for all three shared entry points (hub, subpage, text resolver)
  to change, validates that each detour points inside Solid Uncapper, and then
  installs its own chain. A partial or foreign chain is rejected. See
  [`host_dllmain.cpp`](../../src/host_dllmain.cpp#L145) and the validation loop
  at [`host_dllmain.cpp`](../../src/host_dllmain.cpp#L218).
- This is a specific compatibility policy, not a general multi-hook protocol.
  Hot unloading remains unsupported.

No Steam function is hooked. Language discovery makes ordinary read-only
calls through three exported Steamworks flat-C functions; see
[`probe_steam_language`](../../src/steam_language.cpp#L58).

## Native interfaces called by production

The signatures in this section are the current typed boundaries. Opaque
arguments and analytical names must remain opaque until executable analysis
proves the real class/type.

### Pages, rows, text, and navigation

| Interface | Current RVA | Current contract/use | Evidence | Source |
|---|---:|---|---|---|
| `GameOptions_OpenSubpage` | `0x950850` | `void(void** parent_page_slot)`; pushes a real Advanced Settings-style child and later reaches the subpage handler. | Confirmed | [`OpenSubPageFn`](../../src/addresses.hpp#L24), [`request_navigate_page`](../../src/hooks.cpp#L992) |
| `GameOptions_ConstructOnOffList` | `0x955550` | Constructs the toggle's native Yes/No option object into caller storage. | Confirmed behavior; type/size unknown | [`BufferConstructorFn`](../../src/addresses.hpp#L34), [`add_toggle`](../../src/native_menu.cpp#L178) |
| `GameOptions_ConstructMenuContext` | `0x86A2F0` | Constructs a native context; toggle passes the subobject at `+0x0E`. | Confirmed behavior; field meaning unknown | [`add_toggle`](../../src/native_menu.cpp#L188) |
| `TextReference_ConstructHelp` | `0x7615E0` | Constructs a message reference; normal row help is stored at text-reference object `+0x38`. | Confirmed | [`add_toggle`](../../src/native_menu.cpp#L208) |
| `TextReference_ConstructLabel` | `0x7617C0` | Constructs row labels and option labels from a 32-bit message ID. | Confirmed | [`TextReferenceFn`](../../src/addresses.hpp#L35) |
| `GameOptions_ConstructRootButtonTextReferences` | `0x958210` | Builds the richer Controller-root action-row label/help object used by vanilla Advanced Settings. | Confirmed | [`prepare_button_text`](../../src/native_menu.cpp#L471) |
| `TextReference_ConstructRootButtonDisplay` | `0x763BA0` | Constructs the right-side display reference for a Controller-root action row. | Confirmed | [`prepare_button_text`](../../src/native_menu.cpp#L484) |
| `GameOptions_AddToggleRow` | `0x94A140` | `(page, texts, byte*, on_off_list, context+0x0E, enabled)`. Game state is retained through the supplied byte. | Confirmed | [`AddToggleFn`](../../src/addresses.hpp#L36), [`add_toggle`](../../src/native_menu.cpp#L222) |
| `GameOptions_AddInlineChoiceRow` | `0x94A2C0` | `(page, texts, selected_byte*, list, context)`; zero-based value changes in place with left/right input. | Confirmed | [`AddInlineChoiceFn`](../../src/addresses.hpp#L52), [`add_inline_choice`](../../src/native_menu.cpp#L367) |
| `GameOptions_ConstructInlineChoiceContext` | `0x86A410` | Initializes the inline selector's dedicated context into caller storage. | Confirmed behavior; type/size unknown | [`add_inline_choice`](../../src/native_menu.cpp#L383) |
| `GameOptions_BuildInlineChoiceList` | `0x956CF0` | Initializes a native option-list template that ERNativeUI safely destroys and rebuilds. | Confirmed | [`add_inline_choice`](../../src/native_menu.cpp#L386) |
| `GameOptions_AddSliderRow` | `0x94A590` | `(page, texts, byte*, range, current_byte, enabled)`. | Confirmed | [`AddSliderFn`](../../src/addresses.hpp#L43), [`add_slider`](../../src/native_menu.cpp#L242) |
| `GameOptions_AddActionRow` | `0x92AA40` | `(page, texts, display_text, primary std::function<void()>*, secondary*)`; the row clones/retains what it needs before temporary cleanup. | Confirmed on MSVC x64 | [`AddButtonFn`](../../src/addresses.hpp#L81), [`add_button_with_callback`](../../src/native_menu.cpp#L583) |
| `TextReferences_Destroy` | `0x743AE0` | Destroys temporary row text-reference objects after constructors clone their inputs. | Confirmed | [`DestroyTextReferencesFn`](../../src/addresses.hpp#L87) |
| `GameOptions_NativeBack` | `0x747CD0` | `(page, uint64_action)`; packed value `3` reproduces the concrete page's native Back/pop. | Confirmed | [`NativeBackFn`](../../src/addresses.hpp#L25), [`request_navigate_page`](../../src/hooks.cpp#L1011) |

The `0x240` byte buffers used for several opaque constructors are deliberately
oversized guarded caller storage, **not** recovered `sizeof` values. Recovering
the real types, base classes, constructors, and destructors remains open.

### Popup-choice rows

| Interface | Current RVA | Current contract/use | Evidence | Source |
|---|---:|---|---|---|
| `GameOptions_AddPopupChoiceRow` | `0x95D010` | `(page, texts, value_provider, commit_action, presentation_provider) -> row`; all providers are native erased callables. | Confirmed | [`PopupChoiceConstructorFn`](../../src/addresses.hpp#L63), [`add_popup_choice`](../../src/native_menu.cpp#L297) |
| `PopupChoice_ValueProviderInvoke` | `0x962520` | Returns the one-based selected byte at captured state `+0x01`. | Confirmed | [`addresses.cpp`](../../src/addresses.cpp#L110), [`PopupChoiceNativeState`](../../src/popup_choice_state.hpp#L15) |
| `PopupChoice_CommitActionInvoke` | `0x962330` | Copies the event byte to captured state `+0x01`, then refreshes the captured page through vtable offset `+0xA0`. | Confirmed | [`addresses.cpp`](../../src/addresses.cpp#L112), [`add_popup_choice`](../../src/native_menu.cpp#L331) |
| `PopupChoice_PresentationProviderInvoke` | `0x9623A0` | Builds the native popup list and supplies the identity scope used by ERNativeUI's list-template hook. | Confirmed | [`list_provider_detour`](../../src/native_popup_choice.cpp#L174) |
| `PopupChoice_ConstructListTemplate` | `0x86A610` | Initializes a valid list, elements, vtables, and destruction contract before custom substitution. | Confirmed | [`rebuild_list`](../../src/native_popup_choice.cpp#L92) |

The three callable vtables are currently at `0x2B15A78`, `0x2B15AB0`, and
`0x2B15AE8`. Production does not trust those RVAs alone: it resolves each
invoke body and scans non-executable sections for exactly one three-slot
callable vtable whose third slot matches; see
[`resolve_callable_vtable`](../../src/addresses.cpp#L370).

### TextInput rows (unreleased API 1.1)

The production candidate uses the recovered native TextInput row producer,
controller, and character-name editor factory rather than exposing Scaleform
objects to clients. Each logical row receives one immutable host binding with
a stable native `CS::MenuString`, copied placeholder, declared maximum, and
completion action. The single editor-factory adapter validates the returned
`SoftwareKeyboardJob` vtable and its two native 16-unit limit fields before
replacing both with that row's effective 1..35 maximum.

Programmatic state is synchronized from the shared page-frame detour so native
`CS::MenuString` mutation stays on the UI path. Confirmed native values are
validated, copied into host-owned canonical state, and queued for callback
dispatch by the worker; cancellation, unchanged confirmation, and
programmatic writes do not enqueue callbacks. Row construction validates the
page type, appended row/controller vtables, and exact bound-value pointer and
fails the TextInput feature closed on mismatch.

This path has been live-validated for construction, editing, confirmation,
persistence, root/subpage presentation, and independent ASCII limits through
35 UTF-16 code units. Unicode/IME, cancellation, focus restoration across all
transitions, concurrent programmatic writes, and teardown remain API 1.1
release gates. See [native TextInput](TEXT_INPUT.md).

### Existing Scaleform value bridge

| Interface | Current RVA | Current contract/use | Evidence | Source |
|---|---:|---|---|---|
| `Scaleform_ResolvePath` | `0x74B140` | Native contract is `(movie_context, 0x60-byte destination, const char* path_format, ...) -> destination`. Production observes it with a mid-hook and its private typedef mirrors that variadic contract, while calls remain restricted to literal paths with no format directives. | Confirmed | [`ScaleformPathResolverFn`](../../src/addresses.hpp#L101), [`invoke_scaleform_path_resolver`](../../src/hooks.cpp#L137) |
| `ScaleformValue_SetTextW` | `0x74AE50` | `(value_at_result_plus_0x08, const wchar_t*)`; assigns UTF-16 text to an already resolved existing object. | Confirmed, specialized | [`ScaleformTextSetterFn`](../../src/addresses.hpp#L105), [`apply_physical_page_titles`](../../src/hooks.cpp#L192) |
| `CSScaleformValue_Destroy` | `0xD81590` | Destroys the 0x38-byte `CSScaleformValue` embedded at path-result `+0x28`, including its `GFx::Value` base at wrapper `+0x08`. | Confirmed | [`ScaleformResultDestructorFn`](../../src/addresses.hpp#L108), [`apply_physical_page_titles`](../../src/hooks.cpp#L222) |

What this bridge proves is intentionally narrow: native code can find an
existing named object in a live movie and set its text while respecting the
observed wrapper lifetime. It does not yet prove arbitrary property access,
ActionScript invocation, display-object construction, event subscription, or
movie loading.

### Native generic-dialog transport

These functions are resolved semantically from distinctive wrapper callsites
inside [`resolve_transport`](../../src/native_dialog.cpp#L537), rather than
through the main `GameAddresses` table.

| Interface/data | Current RVA | Current contract/use | Evidence | Source |
|---|---:|---|---|---|
| `FEManager_GlobalSlot` | `0x3D6F820` | Dereference global, then owner `+0x80`, to obtain the live `CSPopupMenu`. | Confirmed | [`popup_access`](../../src/native_dialog.cpp#L200) |
| `DialogDescriptor_Initialize` | `0x77BD70` | Initializes a `0x28`-byte caller descriptor. | Confirmed behavior; fields incomplete | [`invoke_schedule_job`](../../src/native_dialog.cpp#L251) |
| `DialogDescriptor_SetMessageAndLabels` | `0x77BE80` | `(descriptor, message_id, primary_label_id, secondary_label_id)`. | Confirmed | [`invoke_schedule_job`](../../src/native_dialog.cpp#L254) |
| `DialogDescriptor_SetFixedOption` | `0x77BE10` | Called with option `2`. Exact field semantics remain unknown. | Confirmed call, option meaning inferred | [`invoke_schedule_job`](../../src/native_dialog.cpp#L259) |
| `Dialog_SetGlobalGenericOption` | `0x77BE30` | Ignores `RCX`, writes singleton option `1`, consumed immediately by the builder. | Confirmed side effect; field name inferred | [`invoke_schedule_job`](../../src/native_dialog.cpp#L260) |
| `Dialog_BuildMenuWindowJob` | `0x7B8660` | `(result, popup+0x10, builder_kind, descriptor) -> intrusive task`. | Confirmed | [`DialogBuildFn`](../../src/native_dialog.cpp#L37), [`invoke_schedule_job`](../../src/native_dialog.cpp#L263) |
| `IntrusiveTaskSlot_AssignConsume` | `0x7AA2E0` | Consumes a job pointer into popup `+0x298`, gives the slot ownership, and returns a temporary reference. | Confirmed | [`BlockingJobInstallFn`](../../src/native_dialog.cpp#L39), [`invoke_schedule_job`](../../src/native_dialog.cpp#L275) |
| `IntrusiveReference_ReleaseCount` | `0x1EBC000` | Called on job `+0x08`; returns the previous count. If it was one, ERNativeUI calls job vtable slot zero to destroy the object. | Confirmed | [`release_job`](../../src/native_dialog.cpp#L100) |
| `CSPopupMenu_Update` | `0x7EF6D0` | `(popup, elapsed_float, input_enabled_ptr)`; hook owns dialog input and scheduling. | Confirmed | [`PopupUpdateFn`](../../src/native_dialog.cpp#L31) |
| `MenuWindowJob_Poll` | `0x7AE040` | `(job, scheduler_state, result)`; the first 32-bit scheduler-state word reports response kind. | Confirmed for kinds `2`/`3`; full status type unknown | [`menu_window_job_poll_detour`](../../src/native_dialog.cpp#L515) |

Dialog builder kinds are confirmed by live presentation tests: centered uses
`6`, `1`, `2` for zero, one, or two buttons; bottom uses `9`, `7`, `8`.
Caption IDs are `1=OK`, `2=CANCEL`, `3=YES`, `4=NO`. Poll kind `2` is the
left/primary response and kind `3` is right/secondary; see
[`native_dialog_presentation.hpp`](../../src/native_dialog_presentation.hpp#L41).

## Confirmed object and field map

Offsets in this table are relative to the object named in the first column.
An allocation size used by ERNativeUI is distinguished from a recovered native
object size.

| Object | Offset/layout | Current interpretation | Evidence/source |
|---|---:|---|---|
| Controller root page | `+0xB14` (`uint32`) | Visual row capacity loaded from `02_040_optionsetting.gfx`; accepted range is 6 through 13. | Confirmed by controlled 6/10/13 comparisons; a 15-row asset proved the visual overflow boundary and is now rejected by policy; [`detect_root_capacity`](../../src/native_menu.cpp#L754) |
| Controller root page | `+0x1AF0` (`uint64`) | Native row count before ERNativeUI injection; currently diagnostic only. | Confirmed readable/correlated; broader container unknown; [`detect_root_capacity`](../../src/native_menu.cpp#L780) |
| Native page | `+0x10` | Active intrusive task plus embedded segmented queue. Submit receives this address; each base `MenuWindow` slot-2 invocation advances it through `0x7AA1F0`. Queue size is aggregate `+0x30`; the optional maximum is aggregate `+0x38`. | Confirmed trace and static consumer; [`UI_EVENT_DISPATCH.md`](UI_EVENT_DISPATCH.md#menuwindow-slot-12-and-asynchronous-submission) |
| Native page | `+0x230 .. +0x28F` | Page-owned persistent Scaleform path result used by title construction. | Confirmed; [`native_title_bridge.hpp`](../../src/native_title_bridge.hpp#L9) |
| Native page | `+0x3B0` (byte) | Guard inspected by the native Back wrapper before it builds/submits the page action. The broader state machine represented by this byte is unknown. | Confirmed narrow use; [`NATIVE_ADDRESSES_AND_PAGINATION.md`](../NATIVE_ADDRESSES_AND_PAGINATION.md#disassemble-upward) |
| Native page | vtable `+0xA0` | Refresh invoked after popup-choice commit. Exact method/class name unknown. | Confirmed body relationship; [`NATIVE_POPUP_CHOICES.md`](../NATIVE_POPUP_CHOICES.md#recovered-call-path) |
| Scaleform path result | size `0x60`; `SceneObjProxy` self-link/text-setter handle `+0x08`; owner pointer `+0x20`; embedded 0x38-byte `CSScaleformValue` `+0x28`; its `GFx::Value` base begins `+0x30` | Proxy used to address an existing movie object. Both virtual value accessors return the wrapper at `this+0x28`; cleanup destroys that wrapper. | Confirmed; [`native_title_bridge.hpp`](../../src/native_title_bridge.hpp#L9), [`SCALEFORM_GFX_MODEL.md`](SCALEFORM_GFX_MODEL.md#confirmed-native-to-scaleform-bridge) |
| Native UI text result | direct pointer `+0x00`; string storage/pointer `+0x10`; length `+0x20`; capacity `+0x28`; inline capacity `7` | High-level five-argument text resolver result patched after a valid vanilla construction. | Confirmed custom-text behavior; [`native_text_result.cpp`](../../src/native_text_result.cpp#L14) |
| Normal row text references | help member `+0x38` | Label object begins at zero; help is constructed as a nested reference. | Confirmed; [`native_menu.cpp`](../../src/native_menu.cpp#L208) |
| Slider range | `0x18` bytes: `int32 min/max/step`, reserved, metadata `0x22308`, reserved | Temporary descriptor consumed by slider constructor. Exact metadata meaning unknown. | Confirmed working layout; [`SliderRange`](../../src/native_menu.cpp#L44) |
| Inline/popup option list | size `0x920`; first element `+0x08`; stride `0x48`; value `element+0x08`; text `element+0x10`; count `+0x910`; capacity 32 | Native option container rebuilt from a valid game template. Inline values are zero-based; popup values are one-based. | Confirmed; [`add_inline_choice`](../../src/native_menu.cpp#L367), [`native_popup_choice.hpp`](../../src/native_popup_choice.hpp#L9) |
| Popup row state | `0x10` bytes; selected byte `+0x01` | Host-owned state retained for the menu lifetime. Native values are `1..N`; public values are `0..N-1`. | Confirmed; [`PopupChoiceNativeState`](../../src/popup_choice_state.hpp#L15) |
| Popup erased callable | `0x40` bytes; vtable `+0x00`; captured state `+0x08`; commit page `+0x10`; inline target/self `+0x38`; invoke slot at vtable `+0x10` | MSVC x64 callable object cloned by the popup row constructor. Other callable slots are not named. | Confirmed working ABI; [`add_popup_choice`](../../src/native_menu.cpp#L315) |
| Action-row `std::function<void()>` | `0x40` bytes on MSVC x64 | Populated primary and empty secondary passed to native action-row constructor. | Confirmed working ABI; [`native_menu.cpp`](../../src/native_menu.cpp#L171) |
| Root action display text | allocation `0x60`; owner `+0x08`; allocation `+0x10`; capacity `+0x28`; release method at owner vtable `+0x68` | Temporary native text object manually released only when capacity is greater than seven. Exact class name unknown. | Confirmed cleanup path; [`release_button_display_text`](../../src/native_menu.cpp#L519) |
| Root action text references | allocation `0x140`; help/native nested reference `+0x38` | Rich layout needed for the Controller page's left-side button label. `0x140` is caller allocation, not proven native size. | Confirmed working construction; [`prepare_button_text`](../../src/native_menu.cpp#L471) |
| `CSPopupMenu` | context `+0x10`; blocking task `+0x298` | Context builds generic dialogs; slot owns the globally blocking task. | Confirmed; [`popup_access`](../../src/native_dialog.cpp#L200) |
| Dialog job | intrusive reference state `+0x08`; destructor vtable slot zero | Reference-counted `MenuWindowJob`-like object returned by the builder. Complete bases/layout unknown. | Confirmed lifetime operations; [`release_job`](../../src/native_dialog.cpp#L100) |
| Dialog scheduler state | first `uint32` | Response/action kind observed after poll. Only kinds 2 and 3 are normalized. | Confirmed narrow field; [`read_native_poll_status`](../../src/native_dialog.cpp#L501) |

### Root button text special case

The native Controller-root action-row text constructor internally requests
fixed vanilla IDs `0x1B199` (label) and `0x0BC2` (help). A thread-local scope
temporarily remaps only those two resolver calls to the row's registered IDs.
This is why root action rows need a different text path from buttons on a
native child page. See
[`root_button_text_override.hpp`](../../src/root_button_text_override.hpp#L7)
and [`prepare_button_text`](../../src/native_menu.cpp#L489).

The optional GFX patch adds the missing nested Controller Button label field;
the native binding requires the exact `Button/Text_1/Text` path. It is a
presentation structure requirement, not another native hook.

## State transitions and ownership

### Root and child-page materialization

```text
Controller handler returns from vanilla work
    -> publish exact root page for dialog gating
    -> read page+0xB14 visual capacity
    -> resolve root physical slice
    -> call native constructors for rows
    -> retain state pointers/callables in native row objects

retained Submenu/Next callback executes
    -> arm {target route, timestamp, parent page}
    -> title resolver observes pending construction and captures wrapper
    -> OpenSubpage(parent slot)
    -> subpage handler binds new physical page to pending route
    -> inject owned slice without vanilla Advanced Settings rows
    -> set inner and outer titles
```

Pending requests expire after two seconds. Bindings are preallocated before
hook installation so route resolution does not allocate in native UI callbacks.
A reopened route replaces its stale page binding. See
[`SubmenuNavigation`](../../src/submenu_navigation.hpp#L23) and
[`SubmenuNavigation::resolve`](../../src/submenu_navigation.cpp#L137).

Next always pushes a real native child page. Previous verifies that the target
is exactly one slice backward, calls native Back with packed action `3`, unbinds
the popped page, and republishes the already constructed parent because native
Back does not re-enter its materializer. See
[`is_previous_route_target`](../../src/pagination.hpp#L133) and
[`request_navigate_page`](../../src/hooks.cpp#L1011).

### Value rows and callbacks

- Toggle, slider, and inline choice rows point native code directly at the
  compiled row's stable byte.
- Popup choice owns a separate one-based native state byte and synchronizes it
  with the public zero-based byte.
- The host worker polls all value rows every 50 ms, normalizes them, and invokes
  the registered value callback once per observed change. See
  [`poll_changes`](../../src/runtime.cpp#L373) and the host loop in
  [`host_dllmain.cpp`](../../src/host_dllmain.cpp#L345).
- Action/submenu/pagination callbacks run synchronously when the native retained
  `std::function` invokes them. The exact game thread identity still needs
  explicit thread-ID evidence; production assumes the native menu callbacks are
  serialized.

### Popup-choice list construction

```text
native row activates presentation provider
    -> provider hook maps captured state to one compiled row
    -> thread-local ActiveRowScope
    -> native provider calls native list-template initializer
    -> template hook calls original, validates/destructs template elements
    -> rebuilds only the owned row's 1..N elements and labels
    -> restore previous thread-local scope

native commit callable
    -> write selected native byte at state+1
    -> refresh page through vtable+0xA0
    -> host poll publishes zero-based value and callback
```

Vanilla selectors never match an ERNativeUI-owned state address, so the two
hooks leave their lists unchanged. Mutation publishes a shrinking/growing
element count around each destructor/constructor so a fault cannot expose
released or half-built elements; see
[`rebuild_list`](../../src/native_popup_choice.cpp#L92).

### Custom text

Compiled menu strings remain stable for the installed menu lifetime. Dialog
strings receive IDs beginning at `0x1F0000`, remain mapped for the complete
native job lifetime, and are retained for another ten seconds after dismissal
before collection. See [`retire_request`](../../src/native_dialog.cpp#L128).

The result patch updates the direct pointer first. It copies into the native
backing string only when the observed capacity is sane and large enough. This
preserves a valid native wrapper while avoiding writes past unknown storage;
see [`patch_native_text_result`](../../src/native_text_result.cpp#L46).

### Native dialog lifecycle

```text
thread-safe enqueue
    -> pending FIFO + stable custom message ID
    -> CSPopupMenu update pump waits for manager and empty +0x298 slot
    -> initialize descriptor / build intrusive job
    -> consuming assignment publishes exact {popup owner, job}
    -> page-frame gate blocks only captured underlying page
    -> Back gate blocks page action 3 outside owned popup update
    -> popup receives private enabled byte and remains interactive
    -> MenuWindowJob poll records primary/secondary result
    -> native code clears +0x298
    -> reconciliation retires job/text and wakes next FIFO request
    -> host worker dispatches client completion outside native hooks
```

The service never replaces a game-owned task already in `+0x298`. Modal state
uses exact owner and job identity rather than a global "dialog visible" flag;
see [`DialogModalState`](../../src/dialog_modal_state.hpp#L18). Action `3` is
overloaded: it is menu Back normally and the right/secondary dialog action
inside the owned `CSPopupMenu` update. The thread-local popup-input scope is the
current discriminator.

## Known event-dispatch slice

The complete dispatcher is not recovered, but the following Back route is
confirmed by bounded live traces and unwind-guided disassembly:

```text
concrete Advanced Settings Back method              RVA 0x9580C0
    -> construct packed action {kind=3, flags=0}    RVA 0x7AA060 (inferred name)
    -> native Back wrapper via vtable +0x60         RVA 0x747CD0
    -> inner page/action dispatcher                 RVA 0x747850
    -> attempt bounded submit at page + 0x10         RVA 0x7AA0D0
       (caller reference consumed even on rejection)
    -> base slot 2 promotes at most one if idle      RVA 0x7AA1F0
    -> active task virtual update/retirement         RVA 0x7AA480
```

The wrapper also tests the low action DWORD through RVA `0x7AA090`; kind `2`
selects a different internal branch whose meaning is not yet mapped. The
analytical evidence is in
[`NATIVE_ADDRESSES_AND_PAGINATION.md`](../NATIVE_ADDRESSES_AND_PAGINATION.md#disassemble-upward).

RVA `0x9536F0` is **rejected** as the Advanced Settings input authority. It
runs approximately once per frame, but a typed suppression did not stop menu
movement. The working frame boundary is `0x958FF0`; see the rejected-candidate
record in [`NATIVE_DIALOGS.md`](../NATIVE_DIALOGS.md#rejected-candidate-0x9536f0).

## What ERNativeUI can and cannot construct today

| Goal | Current status | Actual mechanism/boundary |
|---|---|---|
| Native toggles, byte sliders, inline choices, popup choices, buttons | Confirmed | Call the game's real row constructors on a live Game Options page. |
| Native submenu and pagination pages | Confirmed | Reuse the game's Advanced Settings child-page creation and real Back stack. |
| Existing-object title text | Confirmed, narrow | Resolve existing paths in live movies and use the specialized UTF-16 setter. |
| More Controller visual rows | Confirmed offline | Structurally add static `Item_N_0` placements to `02_040`; runtime reads capacity from the page. |
| Arbitrary property/member access | Unknown | No general `GFx::Value` property API boundary has been typed. |
| Invoke ActionScript | Unknown | No native-to-AS method invocation boundary has been recovered. |
| Create/attach a runtime movie clip | Unknown | No `CreateObject`, `CreateArray`, `AttachMovie`, or equivalent ownership path is known. |
| Load/register a custom GFX movie | Unknown | Resource lookup, movie creation, layer registration, update, draw, and teardown are unmapped. |
| Subscribe native callbacks to GFx events | Unknown | Callback object ABI and dispatcher ownership are unmapped. |
| Add rows to Audio/Graphics/other root tabs | Not implemented | Only the Controller handler is hooked; other page handlers/containers need mapping. |
| Add arbitrary custom tabs | Not implemented | The asset has nine static slots; native tab descriptors, icon binding, indices, and L1/R1/mouse routing are unknown. |
| Extend the Site of Grace menu | Not implemented | Its movie, page class, constructors, action dispatcher, stack, and lifetime have not yet been connected to this graph. |
| Native editable text row | Discovery | The producer/controller/dialog, Win32-message translation, movie-event call, commit/cancel, and destruction paths are substantially mapped statically. A production row still requires unchanged runtime validation of constructor arguments, Unicode/length behavior, focus restoration, pagination, and teardown. |

"Custom page" in the current API therefore means a logical ERNativeUI page
materialized inside Elden Ring's existing Advanced Settings physical page. It
does not mean a separately loaded movie or arbitrary new HUD/menu layer.

## Priority executable-analysis questions

The whole-program Ghidra work should answer these in roughly this order. Each
answer should record the function RVA, callers/callees, typed boundary, object
fields, ownership/destruction, thread context, and direct evidence.

1. **Native page and row types.** Which RTTI/vtables own the hub page,
   Advanced Settings child, row collection, and constructors? What are the real
   sizes behind the current oversized buffers, and which inputs are copied,
   moved, or retained?
2. **Row-to-widget binding.** Which function selects `Button`, `TextInput`,
   `ComboBox`, or `Slider`, maps a native row to `Item_N_0`, and writes caption,
   value, enabled, visibility, and selection state?
3. **Input and focus dispatcher.** Which concrete factories and task types
   produce the action objects queued at `page+0x10`, how is the optional queue
   maximum configured, and where does each task commit its transition? What
   owns the focus target, selection index, mouse hit test, L1/R1 tabs, OK,
   Cancel, left/right, repeat, sound, and teardown? What do action kinds
   besides the confirmed `2` and `3` mean?
4. **Scaleform owner/value API.** The `0x60` result is now confirmed as a
   `SceneObjProxy`; `+0x08` is its self-link/text-setter handle and its embedded
   `CSScaleformValue` begins at `+0x28` and its `GFx::Value` base at `+0x30`.
   The concrete owner/player type at `+0x20`,
   complete property setters, method invocation, array/object creation, and
   reference-management rules still need to be recovered before exposing any
   general bridge.
5. **Movie/resource lifecycle.** How does a menu resolve and load a `.gfx`,
   external images, and `font.swf`; attach it to a frontend layer; advance and
   render it; then unregister and destroy it safely?
6. **TextInput release matrix.** Use the production anchors in
   [`TEXT_INPUT.md`](TEXT_INPUT.md) to finish Unicode/IME and software-keyboard
   counting, cancellation, concurrent programmatic writes, focus release,
   page closure, and destruction.
7. **Other settings tabs.** Identify each root handler, row collection, shared
   base interface, and whether one common hook can extend all tabs without
   hard-coding a handler per page.
8. **Tab model.** Recover descriptor/container capacity, text/icon resources,
   visual-slot assignment, selected index, wrap/overflow, pointer ownership,
   and controller/mouse dispatch before claiming unbounded custom tabs.
9. **Generic native pages and custom layers.** Determine whether the current
   `OpenSubpage` route is one specialization of a reusable page factory or is
   tightly coupled to Graphics/Advanced Settings assets.
10. **Grace menu integration.** Map its class/movie/event graph independently;
    do not assume Game Options row constructors or action codes are portable.
11. **Threading and reentrancy.** Record thread IDs for page construction,
    row callbacks, text resolution, GFx advancement, popup scheduling, and
    teardown. The current code relies on serialization in several native
    callbacks but only the host worker's 50 ms poll is explicitly controlled.
12. **Hook coexistence.** Find stable higher-level virtual/callsite boundaries
    where possible, document chain behavior, and avoid treating the bespoke
    Solid Uncapper policy as universal compatibility.

## Promotion rule

A plausible decompiler result or nearby RVA is not enough to enter production.
A new interface should be promoted only after:

1. static callers, callees, unwind bounds, and object relationships agree;
2. a bounded observation-only probe confirms arguments and thread context;
3. separate live tests establish input, cancellation, lifetime, and teardown;
4. an AOB or semantic relation resolves uniquely and validates its fallback;
5. failure can disable only the optional feature without corrupting existing
   pages or game-owned UI.

This rule preserves the current design principle: use the highest recovered
native boundary that already owns the desired UI behavior, and descend into
Scaleform only when that behavior cannot be obtained safely from the native
layer.
