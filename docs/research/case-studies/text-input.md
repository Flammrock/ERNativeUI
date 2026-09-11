# Native text input

Status: **Confirmed** on the [reference build](../README.md#reference-build)
and implemented as a current ERUI API 1.1 capability. ERUI API versions and
ERNativeUI package versions track different contracts; see
[Versioning](../../versioning.md).

This page records the evidence behind the implementation. Mod authors who
only want to add a field should use the
[TextInput guide](../../guides/controls/text-input.md). Native names below are analytical
names unless explicitly identified as RTTI.

## Reference build

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

Every RVA and object offset on this page belongs to that executable. Current
patterns and validation in the source are authoritative; these numbers are
navigation aids, not an ABI or a promise for another game build.

## Elden Ring 2.7.1.0 profile update

Static comparison confirmed that the six production TextInput boundaries and
their required vtables retain their 2.7.0.0 RVAs in game version `2.7.1.0`
(PE timestamp `0x6A96B418`, image size `0x5E0DA00`). The row producer contains
updated relative displacements to helpers that moved by `0x70`, but its entry,
control flow, bounds accesses, call shape, entry validator, and inspected
vtables show no detected contract change. The complete host subsequently
reached readiness during the live 2.7.1.0 compatibility regression.

Production now selects a distinct profile for each exact PE identity even
though their TextInput RVAs currently match. See the
[2.7.1.0 update record](../game-updates/elden-ring-2.7.1.0.md) for the complete
comparison and recorded live compatibility matrix.

## Questions and layers

The investigation deliberately treated these as separate questions:

1. Which settings widget displays an idle value and placeholder?
2. Which native producer creates a focusable row and owns its value?
3. Which job opens the editable field, commits or cancels it, and releases
   input afterward?
4. Is a length value a preset, a bit mask, or a per-row numeric maximum?
5. Which visual changes require an optional GFX file rather than a DLL hook?

The resulting model has three layers:

| Layer | Established role |
|---|---|
| Settings GFX | Presents the idle `Widgets/TextInput` row. |
| Native row/controller | Binds the label, placeholder, stable value, focus, and activation callback. |
| Software-keyboard/editor job | Loads `02_990_TextInput`, owns the active edit buffer, and reports confirmation. |

**Rejected:** finding a `DefineEditText` field alone is enough to implement
text input. The settings fields are authored read-only, and focus, input,
commit, cancellation, and teardown all come from native code.

## Static derivation

### GFX presentation anchors

`02_040_optionsetting.gfx` contains the shared `TextInput` sibling used by
physical settings rows:

```text
Item_N_0
`-- Widgets
    |-- Button
    |-- TextInput
    |   |-- Text_0
    |   |-- TextOnEmpty
    |   |-- Caption
    |   `-- Cursor
    |-- ComboBox
    `-- Slider
```

Each `Item_N_0` placement creates a distinct runtime `Widgets` instance, but
the character definitions are shared. Both `Text_0` and `TextOnEmpty` are
`DefineEditText` characters marked read-only, and the sprite has no custom
ActionScript class. **Confirmed:** this movie can display the row; it does not
contain the active editor.

The two native editor resources explain the first prototype's behavior:

| Resource | Editable field | Local placement | Native preset |
|---|---:|---:|---:|
| `02_990_textinput.gfx` | about 400 px wide | `-8,+2` px | 16 |
| `02_991_textinput2.gfx` | about 214 px wide | `+152,-25` px | 8 |

The character-creation Name row has a third, separate presentation in
`04_010_chrmake_commandlist.gfx`. Its idle `Value/Text/Text_0` is framed by
`MENU_FL_Cursor_EntWaku`; its hidden `Value/Input` child is only a positioning
marker. Activation code obtains that marker's world position and opens
`02_990_TextInput`. **Confirmed:** no editable field hidden in the inspected
`04_*chrmake*` movies replaces the dedicated editor movie.

### Executable anchors

A section-aware string scan found these paths and RTTI names. File offsets
were mapped through their actual PE sections rather than through one assumed
image-wide delta.

| Anchor | Reference-build RVA |
|---|---:|
| `Widgets/TextInput` | `0x2AD74E8` |
| `Widgets/TextInput/Text_0` | `0x2B1AF58` |
| `Widgets/TextInput/Input` | `0x2B1AF78` |
| `TextInput/Text_0` | `0x2B2B970` |
| `.?AVTextInput@CS@@` | `0x3C94690` |
| `.?AVTextInputController@CS@@` | `0x3CD6F20` |
| `.?AVTextInputDialog@CS@@` | `0x3CE0D40` |

The decorated-name addresses are strings, not type descriptors. The validated
MSVC type descriptors begin 16 bytes earlier. RTTI and constructor vtable
writes separated `CS::TextInput`, `CS::TextInputController`, and
`CS::TextInputDialog` into distinct native objects.

Cross-references to `Widgets/TextInput` led to discriminator `0x86BCB0`. It
hides the other settings widgets, shows TextInput, allocates a `0x290`-byte
controller, calls its constructor at `0x977CB0`, and attaches it to a temporary
`CS::EditProperty` row. Its sole direct caller led to producer
`0x976EF0`.

The producer's recovered boundary is:

| Argument | Confirmed role |
|---:|---|
| 1 | page/owner |
| 2 | native row text references |
| 3 | stable bound `CS::MenuString` value |
| 4 | erased editor factory; consumed into the activation closure |
| 5 | initial native string |
| 6 | placeholder native string |
| 7 | nonempty completion action |
| 8 | optional disabled predicate |
| 9 | enabled byte |

The producer appends the completed row to the page collection at `+0x1268`,
whose count is at `+0x1AF0`, and tears down its temporary row. Postconditions
used by production check the `CS::EditProperty` vtable, controller vtable, row
count increase, and bound-value identity rather than trusting a non-null
return.

### Editor factory and maximum

RTTI and bounded decompilation recovered a `0x1A8`-byte
`SoftwareKeyboardJob` family. Four wrappers select one of two editor movies:

| Wrapper | Factory | Resource | Preset |
|---:|---:|---|---:|
| `0x81D610` | `0x81CFD0` | `02_990_TextInput` | 16 |
| `0x81D700` | `0x81D160` | `02_991_TextInput2` | 8 |
| `0x81D7F0` | `0x81D2F0` | `02_991_TextInput2` | 8 |
| `0x81D8E0` | `0x81D480` | `02_991_TextInput2` | 8 |

Both vanilla inline-row call sites build the `0x81D700` factory through
`0x915D70`. Character creation instead calls `0x81D610`. The latter was the
safer general editor because its 400-pixel field matches the settings row.

The character-name factory writes the same value to configuration offsets
`+0x60` and `+0x6C`. After the configuration is copied into the job those
fields are at job-relative `+0xC0` and `+0xCC`. Later code copies the pair into
the dialog, and the Scaleform helper applies the secondary value as the text
object's maximum count. **Confirmed:** both fields must be changed coherently;
patching only an immediate or one copy was not promoted.

The current exact-build production anchors are:

| Boundary | RVA |
|---|---:|
| TextInput row producer | `0x976EF0` |
| native menu-string constructor | `0x5EE0F0` |
| borrowed menu-string constructor | `0x6766F0` |
| native menu-string destructor | `0x1BCC60` |
| editor-factory builder | `0x915D70` |
| character-name editor wrapper | `0x81D610` |
| `TextInputController` vtable | `0x2B1B128` |
| `SoftwareKeyboardJob` vtable | `0x2AC5AD0` |

## Bounded live sequence and results

Reproduce this only offline with Easy Anti-Cheat disabled. Record the complete
executable identity, use temporary in-memory instrumentation, arm only one
field at a time, expire each capture after 10 seconds, and cap every site at
32 records. Forward the original exactly once and log normalized caller RVAs,
not only ASLR addresses.

1. Add one private row through `0x976EF0`, initially forwarding the vanilla
   `0x81D700` editor profile. Confirm once, cancel once, close the page, and
   reopen it.
2. Repeat with the ABI-compatible `0x81D610` profile. Observe the job vtable,
   its two limit fields before modification, completion count, and the stable
   bound-value identity.
3. Wrap `0x81D610` with a stack-local copy of the input point, add eight pixels
   to X, and leave Y unchanged. Do not write into a game or Scaleform object.
4. On independent root and subpage rows, patch the coherent limit pair to 3
   and 7. Then repeat with 17 and 35. Try exactly one character beyond each
   limit and reopen every row after confirmation.
5. Test the character-name idle-frame patch on a root row hosted by
   `02_040_optionsetting.gfx`, then independently on an Advanced Settings row
   hosted by `02_042_pc_graphicsetting.gfx`.
6. For each host, cover empty input, changed confirmation, unchanged
   confirmation, cancellation, Back while editing, pagination away and back,
   repeated activation, and page close/reopen.

The observed results were:

| Experiment | Result | Evidence label |
|---|---|---|
| `02_991` profile | Editing and persistence worked, but input stopped at 8 and appeared about 152 px right/25 px high. | **Confirmed** |
| `02_990` profile | Accepted 16 ASCII characters, rejected 17, persisted, and hid the idle value while editing. | **Confirmed** |
| X correction | `+8` pixels corrected the resource-local `-8` placement without changing commit behavior. | **Confirmed** |
| Limits 3 and 7 | Each rejected the next ASCII character on both host movies. | **Confirmed** |
| Limits 17 and 35 | Each accepted its declared maximum and rejected the next ASCII character on both hosts. | **Confirmed** |
| Confirmation | A changed value committed once and survived page recreation. | **Confirmed** |
| Cancellation | No completion callback was observed and canonical state remained unchanged. | **Confirmed** |

These tests prove ordinary per-row numeric limits from 1 through the published
maximum of 35 for the tested route. They do not prove an unrestricted upper
bound or how every Unicode grapheme is perceived by the native editor.

## Failed and rejected prototypes

- **Rejected:** use `02_991_TextInput2` as the general editor. It imposed the
  eight-unit matchmaking profile and wrong geometry exactly predicted by the
  static movie.
- **Rejected:** copy character creation's world-position offset. Its idle
  field is 524 pixels wide while the active editor is 400 pixels wide; the
  settings idle and active fields are both 400 pixels wide. Only the editor's
  own `-8`-pixel local placement needed correction.
- **Rejected:** a visual change to `02_040` should appear on an ERNativeUI
  subpage. The subpage is hosted by `02_042`; this was a wrong-host test, not
  evidence that the deployed root movie was stale.
- **Rejected:** an FFDec-parseable forward character reference is sufficient
  for Elden Ring's loader. A root test changed the placeholder to red but did
  not render the bracket whose resource definitions followed first use.
- **Rejected:** the direct bracket image requires a copied `DoABC` class or
  `SymbolClass` entry. Moving the external-image definition, wrapper, and
  scaling grid before the TextInput sprite rendered it without either class
  binding.
- **Rejected:** patch one limit field or assume powers of two. The recovered
  route carries a coherent pair, and 3, 7, 17, and 35 all behaved as numeric
  maxima.
- **Rejected:** invoke client code directly from the native completion stack.
  That stack still owns game-side write-back and borrowed values.

## Ownership, threading, and teardown

The host creates one heap-stable `TextInputState` for each logical row. It
copies the initial value and placeholder during registration. That state owns
the canonical `std::wstring`, revision, pending confirmed snapshots, and an
opaque `0x40`-byte native menu-string slot. Logical state is independent of a
physical `Item_N_0` reused by pagination.

Before native row construction, the UI-frame pump synchronizes canonical text
into a process-stable shadow and reconstructs the borrowed native menu string.
The row controller retains only that stable native value's address. Temporary
text references, initial string, placeholder wrapper, editor factory, and
erased callables are constructed and destroyed with their recovered native or
MSVC ABI pairs.

The active editor owns a private copy of its starting text:

```text
programmatic write while editor is open
|-- Cancel  -> keep the newer programmatic canonical value
`-- Confirm -> native result replaces it and becomes canonical
```

The native confirmation action runs on the game UI path. It validates and
copies the native value into host-owned state, queues a changed snapshot only
when the value differs, and requests native shadow normalization on the next
page frame. `poll_changes()` later invokes the public callback on the host
worker, outside the native hook and state lock. Cancel, unchanged confirmation,
and `set_text_input_value` do not queue a client callback.

At host teardown, every constructed native menu string is destroyed before
the lookup tables and bindings are cleared. ERNativeUI is process-pinned;
hot-unloading a client or host while native rows or callbacks can still refer
to their state is outside the supported lifecycle.

## Production and fail-closed decision

API 1.1 exposes UTF-16 views and logical handles only. It does not expose a
native menu string, editor factory, packed job, vtable, or Scaleform object.
Lengths are public UTF-16 code-unit counts. Zero selects 16; explicit maxima
must be in `1..35` and are fixed when the row is registered.

Production requires an exact recognized timestamp/image-size pair, then
selects that build's TextInput profile and validates every entry with its full
expected pattern. It also validates page identity, row count bounds, the
created row/controller vtables, and the exact bound-value pointer. If any
required boundary is absent, ambiguous, faults, or fails a postcondition,
installation fails closed rather than calling a nearby function or publishing
a partially working row.

The API capability belongs to finished ERUI API 1.1. Its position in the
append-only table and the frozen API 1.0 prefix are tested independently of
the ERNativeUI package release number.

## Optional GFX boundary

The native settings movies already contain `Widgets/TextInput`; editing,
focus, confirmation, cancellation, and value persistence do not require a GFX
patch. Without the optional artwork, the row uses the native plain settings
presentation.

The GFX patcher optionally adds the character-name bracket and red empty-state
presentation to both supported host movies. It references the game's existing
`MENU_FL_Cursor_EntWaku.tga`; it does not bundle that texture. New
movie-local definitions precede their first use, and a second patch run is
byte-for-byte idempotent. This is presentation only: it neither installs the
editor nor changes the public length contract.

## Unresolved facts

- The exact human-facing vanilla fields represented by the two recovered
  `CS::MatchingDialog` producer call sites remain unnamed.
- No direct-call edge has been proved between this SoftwareKeyboardJob route
  and the separately recovered `CS::TextInputDialog` constructor.
- The native editor's treatment of combining sequences, surrogate pairs, and
  other non-ASCII input still needs a dedicated live matrix. The public host
  boundary continues to count UTF-16 code units regardless.
- Numeric result meanings outside this exact editor route are not established.
- TextInput presentation and focus on every supported built-in settings
  destination have not all been independently characterized.
- The behavior must be revalidated after every executable change, including
  confirm, cancel, Back, pagination, page destruction, and repeated opening.

## Current source anchors

- Exact-build interfaces and pattern validation:
  [`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp)
- Native row construction, editor adapter, limit patch, synchronization, and
  postconditions:
  [`src/native_text_input.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_text_input.cpp)
- Host-owned canonical state and confirmed-change queue:
  [`src/text_input_state.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/text_input_state.hpp) and
  [`src/text_input_state.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/text_input_state.cpp)
- UI-frame synchronization and row materialization:
  [`src/hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp) and
  [`src/native_menu.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_menu.cpp)
- Registration, descriptor validation, getters/setters, and callback bridge:
  [`src/host_registry.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_registry.cpp)
- Optional presentation transformer and structural checks:
  [`tools/gfx_patcher/gfx_patch.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tools/gfx_patcher/gfx_patch.cpp)
- API negotiation and behavior fixtures:
  [`tests/abi/current/text_input_contract_test.c`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/current/text_input_contract_test.c),
  [`tests/host_registry_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/host_registry_test.cpp), and
  [`tests/gfx_patcher_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/gfx_patcher_test.cpp)
- Normative C ABI and C++17 wrapper:
  [`include/ernativeui/erui.h`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) and
  [`include/ernativeui/ERNativeUI.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp)
