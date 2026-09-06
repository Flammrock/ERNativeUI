# Native input bindings

This case study records how ERNativeUI recovered Elden Ring's native binding
screens and promoted that work into the current API 1.1 input-binding backend.
It is an implementation and evidence record, not a client API tutorial. Mod
authors should start with [Native input bindings](../../guides/input-bindings.md).

Native names on this page are analytical names. RVAs, object offsets, token
codes, and vtables are private build-specific evidence, not ERNativeUI ABI.
The evidence labels follow the [research methodology](../methodology.md).

## Question

> Can independent client mods append real section headers and remappable
> actions to Elden Ring's scrolling Button Settings and Keyboard/Mouse
> Settings screens, preserve the game's capture and presentation behavior,
> and observe the resulting controller, keyboard, and mouse activations
> globally without extending or corrupting Elden Ring's fixed save bank?

The answer is **Confirmed** for the semantic input set and exact game build
documented below. The production boundary is a combination of:

- native list extension for presentation and player remapping;
- host-owned semantic assignments rather than invented game action IDs;
- the game's own token conversion and player-0 physical-input query path;
- bounded edge/event queues between the native input thread and host worker;
- explicit suppression while a remapper, modal, or native text editor owns
  input; and
- opt-in provider storage, outside Elden Ring's configuration bank.

## Exact build identity and provenance

All native claims and RVAs on this page refer to this Windows Steam image:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| File size | `87024720` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |
| Live-validation dates | 2026-09-02 through 2026-09-03 |

The runtime image base is intentionally omitted because ASLR changes it.
Every address below is an RVA relative to this exact image.

The historical probe notes did not retain a hash or commit identity for every
intermediate ERNativeUI DLL. That is a provenance limitation: it must not be
replaced with a guessed revision. The conclusions were subsequently checked
against the current production source and its focused tests, linked under
[Current source anchors](#current-source-anchors). A new reproduction should
record the ERNativeUI commit and DLL hash in addition to the game identity.

## Static derivation

### 1. Find the dedicated binding dialog

The ordinary Configuration page materializers were the wrong abstraction.
Cross-references to the movie name `02_160_KeyConfiguration`, its dialog
constructor, and its mode dispatch led to the specialized object named
analytically `CS::KeyConfigDialog`.

**Confirmed:** the controller and keyboard/mouse screens share one native
dialog and a dynamic list model. Their GFX hierarchy is:

```text
MainTimeline
└── KeySetting
    ├── ItemList
    │   ├── Item_0_0 ... Item_10_0   primary cells
    │   └── Item_0_1 ... Item_10_1   secondary cells
    ├── ScrollBarV
    └── ScrollBarV2
```

The eleven authored rows are a recycled viewport. `Normal`, `Grayout`,
`PadCategory`, and `KeyCategory` are timeline states of a cell; the category
states have a label but no selectable hit area. The PC cells in
`02_043_pc_keyconfiguration.gfx` use dynamic text fields for key labels.

**Rejected:** adding more static GFX rows is the way to add bindings. It would
duplicate a viewport rather than extend the logical model. The production
feature needs no GFX patch.

### 2. Recover the list and cell shapes

Constructor vtable writes, the list builder at RVA `0x869580`, and its callers
established this build-locked layout:

| Object or field | Offset/size | Evidence |
|---|---:|---|
| binding list inside `KeyConfigDialog` | `+0x1268` | static + live owner checks |
| binding configuration inside dialog | `+0x1290` | static + `config == list + 0x28` live check |
| dialog mode | `+0x1260` | static + both modes live |
| native list item | `0x50` bytes | vector stride and constructors |
| list allocator/vector owner | list `+0x08` | static |
| list begin/end/capacity end | list `+0x10/+0x18/+0x20` | static + append telemetry |
| item kind | item `+0x08` | static + live cells |
| action-definition pointer | item `+0x10` | static + live cells |
| binding-value pointer | item `+0x18` | static + live cells |
| assigning flag | item `+0x20` | static |
| latest captured token | item `+0x24` | bounded capture probe |
| conflict flags | item `+0x30/+0x31` | static + clear/conflict tests |
| accepted keyboard token | item `+0x34` | bounded capture probe |
| action definition | `0x18` bytes | constructor copy and access sites |
| definition label ID/action ID | definition `+0x00/+0x04` | static + live presentation |

Each logical row is represented by two cells:

| Native mode | Section pair | Action pair |
|---|---|---|
| controller (`0`) | controller/category kind `0`, spacer kind `1` | controller kind `0`, spacer kind `1` |
| keyboard/mouse (`1`) | keyboard/category kind `1`, spacer kind `1` | keyboard kind `1`, mouse kind `2` |

An action ID of `-1` normally marks a category. **Confirmed:** when the same
sentinel definition has a non-null binding-value pointer, Elden Ring presents
it as an editable action instead. ERNativeUI gives one logical action separate
mode-specific definitions but one shared value.

### 3. Prove that the game configuration bank is fixed

The configuration object contains exactly 54 game actions. Lookup and mutation
paths bound the action ID to `0..0x35`; the writer at RVA `0x867C60` performs
that comparison as unsigned.

**Rejected:** inventing IDs above `0x35` or extending the game save bank. The
synthetic `-1` ID is rejected by that writer and therefore fails as a no-op if
an unexpected native save path reaches it. ERNativeUI owns the assignment and
lets clients opt into provider storage instead.

### 4. Trace capture and runtime input separately

The editor produces a 12-byte token, while the row retains a 20-byte binding
value. Static cross-references from capture, formatting, and input-manager
update separated those representations from the physical channel queried each
frame.

The discovery-time native path was:

```text
captured InputToken
    -> 0x242960: update one slot in a BindingValue
    -> 0x242B00: recover one slot as an InputToken
    -> 0x240220: resolve its physical input ID
    -> 0x2403C0: classify digital/special or analog
    -> 0x2413F0: obtain the player-0 input object
    -> 0x2667AD0: query a batch of physical states
```

The current implementation does not expose or serialize those numbers. It
maps public semantic enums to a token, calls the native slot writer for the
visible value, and derives a private runtime snapshot through the physical-ID
and analog helpers.

### Current exact-build anchors

These are the addresses checked by the production backend for the reference
image. The resolver also checks local instruction patterns, executable/data
ranges, and later live-object invariants; this table alone is not a resolver.

| Analytical purpose | RVA |
|---|---:|
| build binding list | `0x869580` |
| construct action/category cell | `0x868000` |
| construct spacer | `0x8696B0` |
| append vector cell | `0x869FD0` |
| poll/commit capture | `0x868C60` |
| clear assignment | `0x8687D0` |
| refresh conflict state | `0x868810` |
| bounded game-config writer used as a safety check | `0x867C60` |
| write one native token into a binding value | `0x242960` |
| token to physical ID | `0x240220` |
| analog classification | `0x2403C0` |
| get player input | `0x2413F0` |
| batch-query input states | `0x2667AD0` |
| input-manager update | `0x266A480` |
| input-manager singleton slot | `0x4861D30` |
| `SoftwareKeyboardJob` construct/destroy | `0x81CCB0` / `0x81CE40` |
| `CS::TextInputDialog` construct/destroy | `0x9B9E00` / `0x9B9FC0` |
| `KeyConfigDialog` vtable | `0x2B0DCC0` |
| binding-list vtable | `0x2AD6E70` |
| binding-config vtable | `0x2AD68D0` |
| `SoftwareKeyboardJob` vtable | `0x2AC5AD0` |
| `CS::TextInputDialog` vtable | `0x2B2B908` |

The discovery-time reverse token helper at `0x242B00` was useful for proving
round trips, but it is not a current production dependency. Do not add it to a
new resolver merely because it appears in the historical pipeline.

## Bounded live probes

### Probe design

The temporary action had the exact research ID `research-input-token` and was
observed only when diagnostics were enabled. The capture detour copied fixed
records into a 64-entry single-producer/single-consumer ring. It did not
allocate, lock, format a log line, access disk, or invoke client code in the
hook. The worker emitted at most 64 records; queue-full, producer-thread, and
limit warnings were each bounded to one line.

Each record contained:

```text
Binding probe v1
seq=<n> device=<controller|keyboard|mouse> thread=<tid>
raw={code,kind,auxiliary,physical,analog}
before={word0,word1,word2,word3,word4}
value={word0,word1,word2,word3,word4}
roundtrip={valid,code,kind,auxiliary,physical,analog}
```

The research row bypassed the prototype's persistence path so the experiment
could not restore or overwrite a user's assignment.

To reproduce a device pass:

1. Run the exact image offline with Easy Anti-Cheat disabled and use a build
   containing only the bounded diagnostic action.
2. Open the corresponding native binding screen and locate the probe row.
3. Enter assignment mode, release the control used to enter it, press the
   requested control once, and wait until the displayed value settles.
4. Repeat representative inputs at least twice. If capture cancels or rejects
   an input, record the absence; do not infer a value from the next sequence.
5. As a control, edit and clear one ordinary Elden Ring row, then reopen the
   screen and confirm its behavior is unchanged.
6. Stop after the fixed action list or the 64-record cap. Preserve the game
   identity and normalize all runtime addresses to RVAs.

### Controller results

All accepted controller tokens had kind `0` and auxiliary `0`.

| Control | Token | Physical ID | Query class | Result |
|---|---:|---:|---|---|
| D-pad Up/Down/Left/Right | `07D0/07D1/07D2/07D3` | `40/41/42/43` | digital | **Confirmed live** |
| Face South/East/West/North | `07D4/07D5/07D6/07D7` | `50/51/52/53` | digital | **Confirmed live** |
| L1/R1 | `07D8/07D9` | `48/49` | digital | **Confirmed live** |
| L2/R2 | `0BB8/0BB9` | `11/12` | analog | **Confirmed live** |
| L3/R3 | `07DA/07DB` | `46/47` | digital | **Confirmed live** |
| Back/View/Create family | `07DC` | `45` | digital | **Rejected by editor** |
| Start/Menu/Options family | `07DD` | `44` | digital | **Not delivered in tested path** |
| stick directions/axes | `0BBA..0BC5` | signed axis channels | analog | **Rejected by assignment scan** |

RVA `0x240AF0` constructs the native controller candidate list. RVA
`0x869070` explicitly rejects `0x07DC` and opens native message 600. The
tested Steam/controller path did not deliver `0x07DD`. The assignment list
does not offer stick motion. Static threshold helpers distinguish positive
and negative stick aliases, but the production absolute-magnitude query would
not preserve their polarity, so they remain excluded.

Formatter-recognized `0x0B54`/`0x0B55` values are layout-relative Cancel and
Confirm aliases, not raw buttons. **Rejected:** serializing either alias as a
controller assignment before its regional-layout resolution.

### Keyboard results

Unmodified keyboard inputs had kind `1`, auxiliary `0`, and a physical ID
numerically equal to the token. The public set recovered from static candidate
lists and live tests is compactly represented here:

| Physical US/QWERTY positions | Native code(s) | Evidence |
|---|---|---|
| Digit 1 through Digit 0 | `47..50` | static; endpoints live |
| Backspace, Tab | `53`, `54` | static + live |
| Q through P | `55..5E` | static; layout samples live |
| main Enter, left Ctrl | `61`, `62` | static + live |
| A through L | `63..6B` | static; layout samples live |
| left Shift | `6F` | static + live |
| Z through M | `71..77` | static; layout samples live |
| right Shift, left Alt, Space | `7B`, `7D`, `7E` | static + live |
| numpad 7..9, 4..6, 1..3, 0 | `8C..8E`, `90..92`, `94..97` | static; 7 and 0 live |
| numpad Enter, right Ctrl, right Alt | `AF`, `B0`, `BB` | static + live |
| Home, arrows, page keys, End, Insert, Delete | `BD..C6` | static; samples live |
| numpad Multiply/Subtract/Add/Decimal/Divide | `7C`, `8F`, `93`, `98`, `B9` | static + programmatic live |

This is 72 semantic keyboard values. The editor constructs six modifier
candidates at RVA `0x2403F0` and 62 ordinary candidates at `0x2404F0`. The
runtime list at `0xE32210` has 85 entries: the 68 editor entries plus 17
runtime-only entries.

**Confirmed:** native codes describe physical positions; Elden Ring localizes
their displayed label. On a French AZERTY layout the live probe produced:

| Printed key | Stored token | Stable physical meaning |
|---|---:|---|
| A | `55` | Key Q position |
| Q | `63` | Key A position |
| Z | `56` | Key W position |
| W | `71` | Key Z position |
| comma key right of N | `77` | Key M position |

The display formatter at RVA `0x750EE0` applies keyboard profiles before the
label lookup at `0x751A10`. Static profile 1 exchanges QWERTZ Y/Z, profile 2
uses the AZERTY remap at `0x7517E0`, and profile 3 uses the Dvorak remap at
`0x751890`.

**Confirmed:** chords store the ordinary key in `code` and a modifier family
in `auxiliary`: Ctrl `1`, Alt `2`, Shift `3`. AltGr also produced Alt family
`2`; left/right identity is not retained for the chord. Standalone modifier
keys remain distinct. **Rejected for the current semantic surface:** accepting
a nonzero auxiliary value without complete runtime chord matching.

Escape (`46`) is in native lists but cancels editor capture. F1-F10
(`80..89`) and F11-F12 (`9A..9B`) are recognized at runtime but absent from
both the editor candidates and label lookup. Programmatic F7 and F11 defaults
activated, yet the native row displayed `----`. A scan of 118 menu GFX files,
3,814 external references, and the shared keycap textures found no F-key
sprite; binding cells use dynamic text. **Rejected:** a missing imported GFX
sprite explains the fallback. The limitation is in native capture/label data,
so F1-F12 are not public binding values.

The five numpad operators are a narrower exception: they are runtime-only but
have working native labels. Programmatic defaults for all five displayed and
activated correctly, although the native editor cannot assign them from an
unbound state.

### Mouse results

All accepted mouse tokens had kind `2` and auxiliary `0`.

| Input | Token | Physical ID | Result |
|---|---:|---:|---|
| left button | `02` | `0` | **Confirmed live** |
| right button | `01` | `1` | **Confirmed live** |
| middle button | `08` | `2` | **Confirmed live** |
| button 4 | `03` | `3` | **Confirmed live** |
| button 5 | `04` | `4` | **Confirmed live** |
| wheel up | `09` | `0x401` | **Confirmed live** |
| wheel down | `0A` | `0x402` | **Confirmed live** |
| buttons 6 through 8 | `05..07` | `5..7` | **Confirmed static; not live-tested** |

RVA `0x2409B0` builds the editor list and the runtime list at `0xE32890`
contains the same ten codes. Buttons 6-8 remain outside the public surface
until tested with suitable hardware.

### Programmatic-default control pass

A second bounded section used stable research IDs for F7 (`86`), F11 (`9A`),
and the five numpad operators. For each row, the test recorded its native
label, activated it once with Num Lock enabled where relevant, dismissed the
test alert, closed/reopened the screen, and relaunched without remapping or
clearing.

**Confirmed:** all seven inputs activated on each run. The numpad operators
retained meaningful labels; F7 and F11 consistently rendered `----`. Relaunch
proved repeatable defaults, not persistence. Current ERNativeUI persistence is
explicit and provider-controlled.

## Recovered device-code model

### Native representations

The capture token is exactly three 32-bit words:

```text
InputToken { code, kind, auxiliary }  // 12 bytes
kind: controller=0, keyboard=1, mouse=2
```

The row-owned value is five 32-bit words:

| Offset | Meaning |
|---:|---|
| `+0x00` | controller code |
| `+0x04` | keyboard code |
| `+0x08` | keyboard auxiliary/modifier family |
| `+0x0C` | mouse code |
| `+0x10` | mouse auxiliary |

The exact empty native value is:

```text
{ 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0 }
```

Controller and keyboard use `0xFFFFFFFF` for unbound; mouse uses zero. This
asymmetry is normalized into public `absent`, `unbound`, and `bound` slot
states and never appears in the serialized API representation.

### Public semantic boundary

**Confirmed production decision:** native integers are an adapter detail, not
portable identity. Current API 1.1 exposes 14 controller buttons, 72 keyboard
positions, and 7 mouse inputs. Storage uses stable semantic strings, while
The current online [`input_binding_model.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/input_binding_model.cpp) performs the
exact current-build conversion.

Current exclusions are deliberate:

- controller Back/Start, stick axes and direction aliases, and
  layout-relative Confirm/Cancel;
- keyboard Escape, modifier chords, F1-F12, and punctuation/OEM positions
  absent from both recovered binding lists;
- mouse buttons 6-8; and
- every unknown raw token.

### Production limits

| Limit | Current boundary | Reason |
|---|---:|---|
| nonempty binding sections | 4,096 total | preallocated native catalog guard |
| actions | 4,096 total | preallocated native catalog and queue guard |
| provider/action identifier | 255 ASCII bytes; letters, digits, `.`, `_`, `-` | host identity guard |
| alternatives per action | one controller, one keyboard, one mouse | recovered five-word value |
| player index | `0` | recovered player-input query path |
| tracked concurrent native text editors | 16 | fixed no-allocation hook array |
| analog active threshold | finite `abs(value) > 0.0001` | digital/trigger query normalization |
| visible native rows | 11 recycled placements | viewport, not catalog limit |

The actual supported-device set comes from whether each semantic slot is
present in an action's defaults. A section appears on the controller screen,
the keyboard/mouse screen, or both according to the actions it contains.

## Promotion to production

### List extension and ownership

The list-builder detour calls the original exactly once, then proves all of
the following before appending anything:

- list and config are non-null and `config == list + 0x28`;
- the owner is exactly `list - 0x1268`;
- dialog, list, and config vtables match this build;
- dialog mode and config mode agree and are `0` or `1`;
- vector begin/end/capacity and `0x50` stride are coherent; and
- category/action templates have their expected kind, pointer, ID, and flag
  shape.

Cells are constructed in preallocated scratch with the game's constructors at
`0x868000` and `0x8696B0`, appended through the native helper at `0x869FD0`,
and verified in the resulting vector tail. The ownership proof excludes
stack-local builder callers used for reset and conflict bookkeeping.

The original builder destroys and rebuilds its vector on refresh. Appending
after every successful original build yields one current copy rather than
accumulating rows.

### Assignment, Clear, and conflicts

Capture polling at `0x868C60` calls the original first, preserving native
pending, Cancel, focus, sound, and completion behavior. Terminal success is
accepted only when definition pointer, binding-value pointer, mode, and cell
kind identify an owned slot. The semantic decoder rejects unknown inputs and
modifier chords.

The native poller writes through the binding pointer before returning. If the
captured token is rejected or a concurrent publication cannot be won, the
hook restores the latest canonical complete snapshot so an unsupported value
cannot remain visible.

Clear at `0x8687D0` changes only the selected owned slot. Conflict refresh at
`0x868810` clears conflict bytes for an owned row: duplicate ERNativeUI
assignments are intentional and never enter Elden Ring's fixed 54-action
conflict table. Non-owned rows always retain the original game behavior.

Player edits and client-side sparse updates meet in a revisioned
compare-and-publish value. Only the recovered input-mutation thread writes the
live non-atomic native `BindingValue`; staged client changes are converted at
the input-manager boundary. This avoids losing one device alternative or
allowing a player edit and client update to overwrite one another.

### Global activation dispatch

The input-manager update at `0x266A480` is reached from gameplay, ordinary
menus, and frontend screens. The detour calls the original first, validates
the receiver against singleton slot `0x4861D30`, applies staged values, and
batch-queries every bound alternative through `0x2667AD0`.

Each device slot must first be observed released. Only an inactive-to-active
transition produces a candidate, so holding a control does not repeat. Edges
from alternatives of the same action in one sample coalesce into one device
mask. Delivery is deferred one input-update sample and its assignment revision
is revalidated, covering an editor or modal created later in the activation
frame and a binding changed before delivery.

The native update hook performs no allocation, locking, disk access, log
formatting, UI construction, or client callback. It writes to a bounded queue;
the host worker drains that queue and invokes client callbacks serially.
Callbacks observe input and do not consume Elden Ring's underlying action.

### Focus suppression

Page-name filtering was insufficient because a binding is global. The
production dispatcher suppresses and discards pending edges while any of
these owns input:

- an official or ERNativeUI remapping job;
- an ERNativeUI native modal;
- `SoftwareKeyboardJob`, used by ERNativeUI TextInput and character naming;
- the other recovered `CS::TextInputDialog` editor family.

The two editor families are tracked from their original constructor return
through their original destructor call. Expected vtables and a fixed
16-pointer atomic array guard the lifecycle. Duplicate, missing, overflowing,
or invalid tracking fails closed. After suppression, every slot must be seen
released before it can arm again.

**Confirmed live:** this blocked callback/input leakage during native
remapping, ERNativeUI text input, native character-name input, title-screen
dialogs, and character creation, while gameplay and ordinary-menu activation
continued to work.

### Failure and lifetime rules

Production resolution first requires timestamp `0x69E9C9B9` and image size
`0x5E09600`. Every function is then accepted only at its build-locked RVA when
the expected local instruction pattern and executable-section check pass;
data and vtable addresses must remain inside the image. Live objects are
validated again before dereference or publication. The current byte patterns
live beside the constants in
[`native_input_bindings.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_input_bindings.cpp) and are
the source of truth.

All mutation, editor-lifecycle, runtime-query, and list-builder hooks are
created before publication. Destructor tracking is enabled before constructor
tracking; runtime dispatch becomes live only after both editor families and
all value guards are installed. A missing address or partial hook install
disables the feature rather than publishing unguarded rows.

Unexpected producer threads, lifecycle violations, native faults, or invalid
runtime state disable callbacks. Queue overflow drops excess events from the
fixed-capacity queue and reports the overflow through a bounded warning path.
Rows already visible remain guarded.

Once a row has reached a live dialog, game objects may retain raw definition
and value pointers. ERNativeUI therefore keeps the backing records and
mutation/editor hooks alive until process exit, even if future publication is
stopped. Hot unload is unsupported. Provider callback code is pinned by the
host lifecycle for the same reason.

## Rejected and corrected hypotheses

| Candidate | Result |
|---|---|
| Patch static GFX rows | **Rejected:** eleven placements form a scrolling viewport. |
| Extend the game's 54-action bank | **Rejected:** ID-bounded lookup/write paths and unsafe persistence semantics. |
| Poll Win32 keyboard/mouse state | **Rejected as the production seam:** the game already exposes a unified physical-input pipeline for all devices and contexts. |
| The first probe failed because it crossed six cells of vector capacity | **Rejected:** a probe-only four-cell catalog failed too. Moving probe-only state out of `NativeBinding` restored the validated backing layout. Vector growth near `0x869C20` remains a scalability lead, not the established cause. |
| Keyboard categories use the same `(1,2)` pair as actions | **Rejected:** static flow and telemetry showed category `(1,1)`, action `(1,2)`. Correct classification restored the missing section. |
| F-key labels need a missing sprite import | **Rejected:** dynamic text fields and the full GFX/reference scan showed the fallback originates in native label lookup. |
| Suppress callbacks by current page name | **Rejected:** ownership crosses pages; actual remapper, modal, and editor lifetimes are tracked instead. |

## Unresolved questions

These are research targets, not supported API promises:

- Can controller Back/Start be captured reliably across Steam Input layouts
  and multiple controller families without stealing system actions?
- Do mouse buttons 6-8 preserve the inferred semantics on suitable hardware?
- What signed query contract is needed to expose stick directions without
  activating both sides of an axis?
- Can modifier chords preserve and query their native family semantics without
  ambiguity, including AltGr and left/right modifiers?
- Can F1-F12 gain owned-row capture and text presentation without changing
  native rows that do not belong to ERNativeUI?
- Is a player-0-only action model sufficient for every supported game mode?
- What is the proven large-catalog behavior of native vector growth around
  `0x869C20`, beyond the current bounded catalog and ordinary live tests?
- Which analytical class and field names can be upgraded from inference using
  independent RTTI/caller evidence?
- How should this exact-build hook set coexist with unknown foreign detours,
  and which structural invariants survive the next game executable update?

## Current source anchors

The implementation, not this address list, is authoritative:

| Area | Current source or test |
|---|---|
| exact-build gates, layouts, resolver patterns, list/capture/clear/conflict/editor/input hooks, queues, and teardown | [`src/native_input_bindings.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_input_bindings.cpp) |
| semantic controller/keyboard/mouse tables and sparse/default operations | [`src/input_binding_model.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/input_binding_model.cpp) |
| coherent runtime snapshots and released-to-pressed edge tracking | [`src/binding_runtime_state.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/binding_runtime_state.hpp) |
| provider section/action compilation and localized message IDs | [`src/menu_compiler.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/menu_compiler.cpp) |
| registration validation, provider-wide action identity, callbacks, handles, and live mutation | [`src/host_registry.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_registry.cpp) |
| optional explicit assignment persistence | [`src/host_storage.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_storage.cpp) |
| semantic mapping and sparse-operation tests | [`tests/input_binding_model_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/input_binding_model_test.cpp) |
| publication, revision, coalescing, suppression, and edge tests | [`tests/binding_runtime_state_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/binding_runtime_state_test.cpp) |
| registry and callback ownership tests | [`tests/host_registry_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/host_registry_test.cpp) |
| strict-C API 1.1 binding contract | [`tests/abi/current/input_bindings_contract_test.c`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/current/input_bindings_contract_test.c) |
| C++17 wrapper contract | [`tests/abi/current/input_bindings_wrapper_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/current/input_bindings_wrapper_test.cpp) |

For the broader relationship between native menus, GFX, Scaleform, jobs, and
input ownership, see [Elden Ring native UI system](../architecture/native-ui-system.md).
