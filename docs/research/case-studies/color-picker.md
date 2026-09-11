# Native color picker

Status: **Confirmed** on the [reference build](../README.md#reference-build)
and implemented as a current ERUI API 1.1 capability. ERUI API versions and
ERNativeUI package versions are independent; see
[Versioning](../../versioning.md).

For ordinary client use, read the [ColorPicker guide](../../guides/controls/color-picker.md).
This case study records how the character-creation editor, settings-row
activation, standalone GFX presentation, and callback lifetime were recovered.
Native names below are analytical unless identified otherwise.

## Reference build

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

All RVAs, vtables, field offsets, and scheduler values on this page are
build-locked observations. The public API exposes none of them.

## Elden Ring 2.7.1.0 profile update

Static comparison added an explicit profile for game version `2.7.1.0` (PE
timestamp `0x6A96B418`, image size `0x5E0DA00`). Three called boundaries moved:

| Boundary | 2.7.0.0 | 2.7.1.0 |
|---|---:|---:|
| game allocation dispatch | `0x1EBBCD0` | `0x1EBBD40` |
| temporary SceneObjProxy destructor | `0xD81590` | `0xD81600` |
| Scaleform color-transform setter | `0xD85610` | `0xD85680` |

All other feature-local RVAs, the movie-name data, and the inspected object
identity anchors remain stable. Production still validates every selected
entry and data anchor. The 2.7.1.0 Solid Uncapper candidate also exposed a
runtime detour at the unchanged visibility-setter RVA; the narrowly supported
chain is documented below. See the
[2.7.1.0 update record](../game-updates/elden-ring-2.7.1.0.md) for comparison
method and the completed compatibility regression.

## Questions and layers

The visible color row is not one indivisible feature. The investigation asked:

1. Which native job opens the character-creation color editor?
2. How are the initial color, live changes, accept, and cancel represented?
3. Can an ordinary settings action controller activate that job safely?
4. Can a settings movie display the native bracketed swatch without inheriting
   Button's inner animation?
5. How should logical values survive physical-row reuse and page recreation?

The resulting implementation combines three independent layers:

| Layer | Established role |
|---|---|
| Native settings action controller | Owns selection, mouse/controller activation, and the row callback. |
| Character-creation modal job | Owns the palette, editor movie, input, accept/cancel, and native dialog lifetime. |
| Optional `Widgets/ColorPicker` GFX sibling | Displays brackets and the current RGB swatch while the modal is closed. |

There is no recovered generic OptionSetting "add color row" constructor.
ERNativeUI reuses the proven action controller, but redirects that one
controller's widget root to a purpose-built sibling during construction.

## Static derivation

### Modal path

The natural control was the character-creation color editor. Static references
to its movie name and the observed construction path yielded these exact-build
boundaries:

| Boundary | Reference-build RVA |
|---|---:|
| build `MenuWindowJob` | `0x7AD980` |
| consuming page-queue submission | `0x7AA0D0` |
| native heap provider | `0x7A8120` |
| game allocator dispatch | `0x1EBBCD0` |
| palette constructor | `0x77C620` |
| palette destructor | `0x77CA10` |
| populate palette | `0x77CF90` |
| scene-proxy bridge | `0x7460D0` |
| temporary SceneObjProxy destructor | `0xD81590` |
| color-control dialog constructor | `0x8B6F90` |
| `04_031_ChrMake_ColorEditor` literal | `0x2AB8DE8` |

The page supplies its existing component stack at `+0x50` and its job queue at
`+0x10`. The recovered movie descriptor uses kind 8, flag 1, and the validated
`04_031_ChrMake_ColorEditor` name. A native palette is constructed in `0x980`
bytes, populated with parameter ID `0x44D`, and contains 143 entries on the
reference build. The dialog itself is allocated as `0x17A0` bytes through the
game heap.

The dialog constructor receives a converted scene proxy, palette, initial
packed color, and a live erased callable. The callable reports the current
packed color and a still-uninterpreted special-entry flag. The page queue
consumes the resulting job pointer. **Confirmed:** a successful submission
clears the caller's pointer; treating it as still caller-owned would double
release it.

Observation of the exact submitted job at the normal `MenuWindowJob` poll
established terminal kind 2 as accept and kind 3 as cancel for this editor.
Those values are not claimed to describe every Elden Ring job.

### Settings action seam

Ghidra decompilation of action-widget producer `0x86B940` shows a hard-coded
selection sequence:

1. hide `Widgets/Slider`;
2. hide `Widgets/ComboBox`;
3. hide `Widgets/DropdownList`;
4. hide `Widgets/TextInput`;
5. show `Widgets/Button`;
6. construct the native action controller; and
7. attach it through helper `0x86C280`.

The attach helper stores the retained controller at row offset `+0x78`,
resolves a child named `Cursor` relative to the selected widget, and initially
hides it. **Confirmed:** a substitute widget root must provide `Cursor`.
Static analysis does not make every later use of that child a general contract.

Production scopes the substitution to one construction:

```text
registered color row
  -> thread-local row/state/page identity
  -> preflight Widgets/ColorPicker and its Color child
  -> hide Button only after preflight succeeds
  -> replace only this source proxy's exact Widgets/Button lookup
  -> original native producer constructs the ordinary action controller
```

All unrelated path resolutions and action rows continue through the original
function. The narrow bridge uses the shared Scaleform resolver at `0x74B140`;
it does not install a second competing resolver hook.

### Standalone GFX widget

The patcher creates a hidden sibling in each recognized settings widget
container:

```text
Widgets
|-- Button
|-- TextInput
|-- ComboBox
|-- Slider
`-- ColorPicker
    |-- Cursor
    |-- CharacterFrame
    |-- Color
    |-- ColorFrame
    |-- transparent native value field
    `-- native left row label
```

The stable native paths are:

```text
Widgets/ColorPicker
Widgets/ColorPicker/Cursor
Widgets/ColorPicker/Color
```

The generated sprite uses these depths:

| Depth | Role |
|---:|---|
| 1 | native row background |
| 2 | named full-row `Cursor` borrowed from TextInput |
| 3 | `MENU_FL_Cursor_EntWaku` bracket frame |
| 5 | named generated `Color` fill |
| 7 | `MENU_FL_ColorWaku` foreground |
| 10 | transparent native action value binding |
| 12 | visible native left label |

The transparent value field remains because the controller expects the host's
native text name. Game Options and Advanced Settings reverse the roles of
`Text_0` and `Text_1`, so the patcher builds a host-specific definition rather
than assuming one layout. External-image definitions, wrappers, grids, and
generated shapes receive free movie-local IDs and precede their first use.
No game texture, ActionScript block, or `SymbolClass` entry is copied.

At runtime the root movie context is already rooted at `WindowList`. Its fill
path is therefore
`ControllSetting/Item_N_0/Widgets/ColorPicker/Color`, not a path beginning
with `WindowList/`. Advanced Settings uses the complete
`GraphicOption/Item_N_0/Widgets/ColorPicker/Color` path. Other supported
built-in panels have their own exact names in the production host map.

### Solid Uncapper visibility-setter overlap

ColorPicker does not detour the Scaleform visibility setter at `0x734190`; it
calls that game entry to hide or show validated widget objects. On the first
2.7.1.0 compatibility candidate that entry had already been replaced by Solid
Uncapper's property-set detour, so the ordinary pristine-byte check correctly
rejected ColorPicker and the host's atomic install removed the otherwise ready
menu hooks.

The compatibility path is intentionally narrower than generic hook chaining:

1. after selecting an exact game profile, capture the pristine entry address
   and its first 16 bytes before Solid Uncapper's worker installs hooks;
2. after the shared Solid Uncapper stabilization wait, accept only the
   unchanged pristine entry or an `FF 25` absolute-indirect jump into
   executable memory owned by `Solid Uncapper.dll`;
3. revalidate the approved shape and ownership immediately before publishing
   the ColorPicker native call table; and
4. retain the game entry address as `set_visible`, allowing the normal call to
   traverse Solid Uncapper's detour and its vanilla trampoline.

Static inspection confirmed that the observed Solid Uncapper detour preserves
the setter arguments and invokes its stored original before post-processing.
No copied prologue is executed and ERNativeUI never calls the detour
destination directly. Every other ColorPicker address, pattern, data anchor,
and modal dependency remains under the ordinary pristine validation policy.
The 2.7.1.0 chain was subsequently live-validated with Solid Uncapper 2.3
loaded first, and the pristine path was validated without it. The 2.7.0.0
profile was regression-tested in both configurations as well.

## Bounded live sequence and results

Reproduce only offline with Easy Anti-Cheat disabled and the executable hash
recorded. Use temporary in-memory hooks. Arm one editor for at most 10 seconds,
cap each observation site at 32 records, retain only the exact constructed job
identity, and forward every original exactly once.

1. In character creation, open one native color editor. Change a palette
   entry, press Triangle to enter the R/G/B and hue controls, then cancel.
   Reopen and repeat with confirm. Finally reopen, make no change, and exercise
   both cancel and confirm as controls.
2. Observe the palette constructor/populator, color-control constructor,
   job-builder return, consuming submission, live color callable, and poll for
   only that session. Record normalized call sites, page, job, palette count,
   initial/current colors, and terminal kind.
3. Add a private ERNativeUI action row that submits the same recovered modal.
   Test focus, mouse/controller activation, accept, cancel, Back, page close,
   and repeated opening before changing its presentation.
4. Add a hidden swatch inside Button as a presentation probe. Then test the
   standalone sibling, first on Game Options and then on an Advanced Settings
   subpage.
5. Give the root and subpage rows different initial RGB values. Change each in
   turn, confirm, switch pages, reopen, and verify that only the selected
   logical row changes and that the visible swatch refreshes immediately.
6. Repeat with an unchanged confirmation, cancellation, a programmatic write
   while the editor is open, pagination away and back, and an ordinary Button
   reusing the same physical slot.

The decisive results were:

| Observation | Result | Evidence label |
|---|---|---|
| Palette construction | Parameter `0x44D` produced 143 entries. | **Confirmed** |
| Terminal poll | Kind 2 accepted the observed color; kind 3 cancelled it. | **Confirmed for this job** |
| Public color | Editor exposed R, G, and B controls; no alpha control was observed. | **Confirmed** |
| Standalone root/subpage swatches | Native brackets, fill, label, and row selection rendered correctly. | **Confirmed** |
| Acceptance | Canonical value and the active swatch updated; a changed value produced one later client callback. | **Confirmed** |
| Cancellation/unchanged confirmation | Canonical value remained unchanged and no change callback was delivered. | **Confirmed** |
| Independent rows | Root and subpage values no longer shared storage and survived recreation independently. | **Confirmed** |
| Missing custom widget | The ordinary action Button remained usable and still opened the editor. | **Confirmed fallback** |

The observed internal packed value was
`R | (G << 8) | (B << 16) | (A << 24)`. Production supplies opaque alpha
`0xFF` but exposes only named RGB bytes. The packed representation is private.

## Failed and rejected prototypes

- **Rejected:** ship the first `ColorPreview` nested inside Button. It proved
  that the frame and runtime tint worked, but inherited Button's blinking
  inner value rectangle.
- **Rejected:** move the overlay to another Button depth. The unwanted
  animation remained because it belonged to Button's presentation timeline.
- **Rejected:** hide Button's complete cursor. That also removed or changed
  the ordinary full-row hover/selection state.
- **Rejected:** fix the appearance with recurring runtime visibility tricks.
  Physical rows are reused, and a shared Button character must retain normal
  behavior for every non-color action.
- **Rejected:** include `WindowList/` in the root refresh path. Accepted state
  persisted, but the visible root swatch did not update until the page was
  closed and rebuilt.
- **Rejected:** identify logical color rows by one shared callback/value pair.
  That prototype linked the root and subpage colors. Production keys preview
  and modal state to the exact host-owned logical row state.
- **Rejected:** use a higher-level color-transform wrapper that changed channel
  order. The direct Scaleform setter preserved the observed R/G/B packing.
- **Rejected:** expose alpha, the special-palette Boolean, palette ID, or
  scheduler kinds in API 1.1. Their general semantics were not established.
- **Rejected:** assume an FFDec-parseable forward character reference renders
  in game. As with the TextInput frame, definitions must precede direct use.

## Ownership, threading, and teardown

Each logical row owns an independent `ColorPickerState`. Its canonical opaque
RGB value and pending-preview flag are atomic; physical GFX slots are merely
ephemeral presentations. Before a page materializes, ERNativeUI hides every
custom ColorPicker instance because vanilla widget producers do not know that
the new sibling exists. Construction binds only the current logical state to
the current page and row index.

Only one native color-editor session may be active process-wide. Before
submission, production verifies that the page has no active or queued job and
that its component count is within the recovered bound. A session retains:

- the exact logical row state and changed action;
- the initial and latest native colors;
- the exact submitted job identity;
- the preview location; and
- a shared palette owner captured by the native factory.

The palette uses its confirmed native destructor before aligned storage is
released. A temporary converted SceneObjProxy uses its native destructor. A
successfully constructed ColorControlDialog and consumed job belong to the
game. If a native constructor faults after allocation, production deliberately
does not guess a deleting destructor or raw-free contract; one bounded leak is
safer than a double free or use-after-free.

The live native completion only records the editor's latest packed value. The
normal job poll recognizes accept/cancel for the exact job, updates canonical
state on accept, and moves the completed session out of the native path. The
public callback is invoked later by the host worker, outside the native hook
and mutex. Cancel, unchanged accept, and `set_color_picker_value` do not invoke
client code.

A programmatic write during an open editor follows last-completed-action
semantics:

```text
set_color while open
|-- Cancel  -> preserve the programmatic value
`-- Confirm -> replace it with the player's accepted value
```

Preview writes occur at a known UI-frame boundary and only for the page the
game currently proves alive. A closed-page update remains pending until its
logical row is materialized again. If page/job teardown releases the native
factory without a terminal poll, canonical state is preserved and no synthetic
client callback is invented from that uncertain context.

ColorPicker and the generic alert transport share modal coordination. An
active alert blocks opening the editor; alerts queued during the editor wait
in their FIFO until the color session terminates. Hot unload with an active
native session is unsupported, and the host is designed to remain pinned for
the process lifetime.

## Production and fail-closed decision

The backend requires one exact recognized PE timestamp/image-size pair,
selects its complete ColorPicker RVA profile, validates every selected entry
with a long semantic pattern, validates the movie name, and preflights the
palette, page, widget, and job relationships before use. If a registered
ColorPicker requires a native backend that cannot be installed, host hook
installation fails closed rather than silently omitting the rows or calling a
plausible nearby function.

The sole non-pristine exception is the early-captured Solid Uncapper
visibility-setter chain described above. It is owner-, encoding-, snapshot-,
and build-gated, then revalidated at publication. It does not authorize a
foreign detour on the action producer, path resolver, color-transform setter,
job poll, modal gates, or any other ColorPicker boundary.

The path override is active only for the exact `Widgets/Button` lookup, exact
source proxy, and thread-local color-row construction scope. The custom Button
is hidden only after `Widgets/ColorPicker` and its `Color` child both pass
preflight. A missing or incomplete optional GFX patch therefore preserves the
ordinary action presentation instead of leaving a half-hidden row.

API 1.1 exposes named RGB channels, logical row/provider handles, and a
confirmation-only callback. It is capability-gated and appended after the
frozen API 1.0 table. Those ABI guarantees are tested independently of the
ERNativeUI package release number.

## Optional GFX boundary

The native modal does not require the custom sibling. Without patched GFX, a
ColorPicker is a functional ordinary action Button that opens the same native
editor.

The bracketed idle swatch does require the optional transformations of
`02_040_optionsetting.gfx` and `02_042_pc_graphicsetting.gfx`. The patcher
references Elden Ring's existing `MENU_FL_Cursor_EntWaku.tga` and
`MENU_FL_ColorWaku.tga` resources without bundling their texture content. It
recognizes and removes only the two complete earlier Button-backed prototype
layouts, rejects partial/conflicting names or depths, preserves definition
order, and verifies byte-for-byte idempotence.

This GFX extension changes presentation only. It does not create the native
modal, own the RGB value, or broaden the public API.

## Unresolved facts

- The original semantic class names and full layouts of several recovered
  palette/job helpers are not known; analytical names must remain labeled.
- The special-palette-entry Boolean is retained only long enough for native
  correctness and has no established public meaning.
- No safe public contract has been established for alpha, alternate palettes,
  palette entry enumeration, or arbitrary color-editor modes.
- Terminal kinds 2 and 3 are confirmed only for this captured editor job.
- Live presentation and focus have not been independently characterized on
  every API 1.1 built-in destination.
- Apart from the narrowly recognized Solid Uncapper visibility-setter chain,
  coexistence with a mod detouring the action producer, Scaleform resolver,
  job poll, modal gates, or another ColorPicker boundary requires a separate
  compatibility implementation and test.
- Every game update requires the complete accept/cancel, mouse/controller,
  navigation, repeated-open, physical-row-reuse, and teardown matrix again.

## Current source anchors

- Native modal construction, exact-build validation, session ownership,
  widget routing, preview refresh, and callback dispatch:
  [`src/color_picker.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/color_picker.cpp)
- Private native boundary exposed to the menu and dialog coordinators:
  [`src/color_picker.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/color_picker.hpp)
- Independent canonical RGB state and refresh flag:
  [`src/color_picker_state.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/color_picker_state.hpp) and
  [`src/color_picker_state.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/color_picker_state.cpp)
- Action-controller construction and logical row binding:
  [`src/native_menu.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_menu.cpp)
- Shared job-poll observation and modal input coordination:
  [`src/native_dialog.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_dialog.cpp)
- Registration, descriptor validation, setters/getters, and callback bridge:
  [`src/host_registry.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_registry.cpp)
- Standalone widget construction, legacy-prototype migration, and structural
  validation:
  [`tools/gfx_patcher/gfx_patch.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tools/gfx_patcher/gfx_patch.cpp)
- State, ABI, wrapper, and GFX tests:
  [`tests/color_picker_state_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/color_picker_state_test.cpp),
  [`tests/abi/current/color_picker_contract_test.c`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/current/color_picker_contract_test.c),
  [`tests/abi/current/color_picker_wrapper_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/current/color_picker_wrapper_test.cpp),
  and [`tests/gfx_patcher_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/gfx_patcher_test.cpp)
- Normative C ABI and C++17 wrapper:
  [`include/ernativeui/erui.h`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) and
  [`include/ernativeui/ERNativeUI.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp)
