# Native text-input investigation

Status: native behavior and both host presentations live-validated;
production candidate integrated into the unfrozen API 1.1 development prefix.
The private prototype is retired; the release validation matrix remains open.

Tested executable for the initial static inventory:

- file: `eldenring.exe`
- modified: 28 August 2026
- PE timestamp used by the current address documentation: `0x69E9C9B9`
- image size used by the current address documentation: `0x5E09600`

The objective is a real native editable row with correct controller, keyboard,
mouse, localized-text, focus, confirmation, cancellation, page-navigation, and
destruction behavior. A field that only renders is not sufficient.

## What is confirmed

### GFX presentation

The PC `02_040_optionsetting.gfx` contains a shared `TextInput` widget in the
generic Controller Settings item character. It is available to each physical
`Item_N_0` placement in the same way as the shared Button, ComboBox, and Slider
widgets. Each placement creates a distinct runtime row and `Widgets` instance,
but all of those instances reference the same underlying character
definitions; the file does not duplicate the complete widget tree per row.

The current GFX patcher's structural knowledge also establishes that TextInput
uses the Controller `Caption` wrapper used by Slider and ComboBox. This proves
that the movie can present the widget; it does **not** identify the native row
constructor or input lifecycle.

FFDec inspection establishes the complete static TextInput subtree:
`Text_0`, `TextOnEmpty`, `Caption/Text_0`, and a 60-frame `Cursor` visual.
Both value fields are `DefineEditText` characters but are marked
`readOnly=true`; the TextInput sprite has no custom ActionScript class. There
is no named `Input` child or standalone `Input` literal anywhere in this
movie. Native code may still change properties or supply editing elsewhere,
so this is a boundary finding rather than evidence that the widget is unusable.
See [the Scaleform/GFX presentation model](SCALEFORM_GFX_MODEL.md) for the
character IDs, placements, and original-versus-patched comparison.

The optional shipped GFX inspected on 31 August 2026 reports 13 Controller
visual placements. Text-input research must also work with the unmodified
six-placement movie.

The two dedicated editor movies explain why an otherwise-correct native row
can still have the wrong field geometry and limit:

| Editor resource | Editable `Text_0` bounds | Placement inside `TextInput` | Static seed | Paired native value |
|---|---:|---:|---|---:|
| `win/02_990_textinput.gfx` | `-40,-40 .. 7960,680` twips (about 400 px wide) | `-160,+40` twips (`-8,+2` px) | empty | `0x10` / 16 |
| `win/02_991_textinput2.gfx` | `-40,-40 .. 4239,680` twips (about 214 px wide) | `+3040,-500` twips (`+152,-25` px) | `WWWWWWWW` | `8` |

Both root movies place their `TextInput` at `(2000,2000)` twips. The relative
placement is therefore the important difference when native code positions
the editor. `02_990` exports `MENU_TextBase_00`/textbox definitions in addition
to its wider editable field; an exported definition is not by itself proof that
the asset is placed on the active display list. `02_991` is the narrow
matchmaking-style field. FFDec 26.2.1 exported these facts from the
user-extracted current game resources without modifying them.

#### Character creation has separate idle and active layers

The character-creation Name row is not an instance of `02_990` while it is
idle. Its read-only presentation is authored in
`04_010_chrmake_commandlist.gfx` under this local character hierarchy:

```text
Item_0_0 (sprite 92)
|-- Value (sprite 77)
|   |-- Text (sprite 52)
|   |   |-- frame (sprite 50 -> external image 31)
|   |   `-- Text_0 (edit-text 51)
|   `-- Input (sprite 76; positioning marker)
`-- Cursor (sprite 49)
    `-- Value (sprite 48 -> external image 32)
```

External image 31 is `MENU_FL_Cursor_EntWaku` (`428x108`) and supplies the
persistent dark bracketed frame. Sprite 50 places it at `(-4193,-1080)` twips
and has a scaling grid of `(-80,-160)..(80,180)`. Sprite 52 places that wrapper
at `(-887,-320)` with Y scale `0.47999573` and RGB multipliers `225/256`.
Edit-text 51 is centered, 24 pixels high, 524 pixels wide, authored white, and
placed at `(-6000,-640)` with a black drop shadow. The selected-row highlight
is a second layer: sprite 48 wraps `MENU_FL_Cursor_Ent` (`428x110`), and sprite
49 aligns it over the static frame. Sprite 92 uses `Normal` frames 1--9 and
`Grayout` frames 10--18; the latter applies an RGB multiplier of `128/256` to
the complete Value subtree.

The observed red localized `None entered` value is not baked into these
characters. Edit-text 51 is white, its Normal parents do not tint it red, and
the relevant ActionScript only stops the Normal and Grayout timelines. The
localized empty text and its presentation style are applied by native code.

Neither `MENU_FL_Cursor_EntWaku` nor `MENU_FL_Cursor_Ent` is defined in the
base or Windows `02_040_optionsetting.gfx`. Its Button, TextInput, ComboBox,
and Slider roots share only `MENU_FL_Cursor_Wide`. The settings TextInput is a
plain centered 400-pixel `Text_0` plus a `TextOnEmpty` field authored as
`#505050`; it has no equivalent bracket subtree. Character IDs are local to a
GFX movie, so IDs 31 and 32 from `04_010` cannot be referenced directly from
`02_040`.

#### Framed-idle prototype: host movie and tag-order boundary

The first framed-idle experiment exposed two independent boundaries that are
easy to conflate.

First, the movie that owns a row matters. The built-in root options tabs and
their physical row widgets are authored by `02_040_optionsetting.gfx`.
ERNativeUI subpages opened through the PC Advanced Settings route are instead
presented by `02_042_pc_graphicsetting.gfx`. A visual patch to the shared
TextInput definition in `02_040` therefore cannot change a TextInput instance
being displayed by `02_042`. The initial subpage observation was not evidence
that the generated `02_040` was missing or stale; it tested the wrong host
movie for that visual change.

The subsequent root-row test isolated the actual `02_040` behavior. The
patched red `TextOnEmpty` color appeared, proving that the deployed movie and
shared TextInput sprite 103 were active, but the new bracket image remained
absent. At that point the generated movie contained all of these pieces:

- a valid `DefineExternalImage2` for `MENU_FL_Cursor_EntWaku`;
- a wrapper `DefineSprite` that directly placed the image with
  `PlaceObject3` and its image flag;
- the character-creation scaling grid and transforms; and
- a depth-2 `CharacterFrame` placement in TextInput sprite 103.

FFDec could parse the output and report the complete tree, but the three new
definitions had been appended near the end of the movie, after sprite 103 had
already referred to the wrapper. This was not accepted by the in-game GFx
loader for the external-image path.

Natural `04_010` also gives every external image a generated `BitmapData`
class in `DoABC` and a matching `SymbolClass` entry. That correlation made a
missing ActionScript binding a reasonable intermediate hypothesis. Static
inspection alone could not distinguish it from the forward reference, and the
hypothesis was recorded only as a candidate rather than treated as a working
contract.

The discriminating build moved `DefineExternalImage2`, its wrapper
`DefineSprite`, and `DefineScalingGrid` ahead of TextInput sprite 103. It did
**not** add `MENU_FL_Cursor_EntWaku` to the target movie's `DoABC` or
`SymbolClass`. The next live test displayed the exact native bracket frame.
This confirms the narrow boundary for the tested movie and game build:

- a `PlaceObject3` direct image placement can render a movie-local
  `DefineExternalImage2` without an ActionScript class or `SymbolClass`
  mapping; and
- the external image and wrapper definitions must precede the sprite that
  refers to the wrapper.

This result does not claim that `SymbolClass` is unnecessary when ActionScript
constructs or resolves a bitmap by class name. It establishes only that class
linkage is outside the direct display-list route used by the idle-frame
prototype. It also explains why FFDec structural validation was necessary but
not sufficient: the decompiler accepted a forward reference that Elden Ring's
runtime did not render.

### Executable string anchors

A read-only scan of the current executable found these literal paths together:

```text
Widgets/TextInput
TextInput/Text_0
TextInput/Input
```

These are promising cross-reference anchors for the native code that selects
the widget, supplies its caption/value, or binds its editable input object.
The executable also contains valid MSVC RTTI for `TextInput`,
`TextInputController`, and `TextInputDialog`. A separate recovered native type
is named `_anon_27CBC08A::SoftwareKeyboardJob`; the name is evidence for the
game-side job's role, not evidence that it calls Steam or any other platform
API.

On the tested executable, GNU `strings -a -t x` and the PE section table gave
the following coordinates. File offsets are recorded so the scan is
reproducible. Widget literals are in `.rdata` (`file offset 0x29B0E00`, RVA
`0x29B2000`), while MSVC type descriptors are in `.data` (`file offset
0x3B13600`, RVA `0x3B15000`); each RVA is mapped through its actual section,
not guessed from a single image-wide delta.

| Literal | File offset | RVA |
|---|---:|---:|
| `Widgets/TextInput` | `0x2AD62E8` | `0x2AD74E8` |
| `Widgets/TextInput/Text_0` | `0x2B19D58` | `0x2B1AF58` |
| `Widgets/TextInput/Input` | `0x2B19D78` | `0x2B1AF78` |
| `TextInput/Text_0` | `0x2B2A770` | `0x2B2B970` |
| `.?AVTextInput@CS@@` | `0x3C92C90` | `0x3C94690` |
| `.?AVTextInputController@CS@@` | `0x3CD5520` | `0x3CD6F20` |
| `.?AVTextInputDialog@CS@@` | `0x3CDF340` | `0x3CE0D40` |

The table records the decorated-name string itself. A validated MSVC type
descriptor begins `0x10` bytes earlier: `0x3C94680`, `0x3CD6F10`, and
`0x3CE0D30` respectively. Their complete-object locators are `0x32EE4F8`,
`0x3325A08`, and `0x332D208`. This distinction corrects the easy mistake of
labeling the raw string address as the descriptor address.

The RTTI coordinates are discovery aids, not candidates for production address
resolution. The corresponding validated hierarchies and primary vtable seeds
separate three layers:

- `CS::TextInput -> SceneObjProxy -> ComponentProxy`, vtable `0x2A98340`;
- `CS::TextInputController -> PropertyController -> DLReferenceCountObject`,
  vtable `0x2B1B128`;
- `CS::TextInputDialog -> MenuWindow -> ... -> ComponentStack`, vtable
  `0x2B2B908`.

See [the Scaleform executable surface](SCALEFORM_EXPORTS.md) for the validated
complete-object locators, vtable entries, and derivation method. Production
code should still resolve a distinctive executable callsite or function
signature and validate its relationships.

### Existing ERNativeUI row construction

ERNativeUI currently resolves native constructors for toggle, inline choice,
slider, action button, and popup choice rows. The clustered current RVAs for
the first three are useful navigation landmarks, not evidence that an adjacent
function is TextInput:

| Interface | Current RVA |
|---|---:|
| add toggle | `0x94A140` |
| add inline choice | `0x94A2C0` |
| add slider | `0x94A590` |
| add button | `0x92AA40` |

The static pass has now identified both the inline controller and the separate
modal editor. This is enough to design bounded runtime probes, but not yet
enough to publish a constructor in the ABI.

## Native call flow recovered from the tested executable

Evidence labels below have deliberately narrow meanings:

- **confirmed**: direct instruction/data-flow or RTTI evidence;
- **inferred**: behavior is strongly indicated by the call sequence, but still
  needs an unchanged forwarding probe in the game;
- **unknown**: no claim is made yet.

### Widget discriminator and higher-level producer

The references to `Widgets/TextInput` do not represent fifteen independent
constructors. Most occur in shared widget-selection helpers that hide
TextInput while selecting Button, ComboBox, or Slider. The function at RVA
`0x86BCB0` is the exceptional path: it hides Slider, ComboBox, DropdownList,
and Button, makes TextInput visible, allocates `0x290` bytes, calls the
TextInputController constructor at `0x977CB0`, and attaches that controller to
a supplied temporary `CS::EditProperty` row. It returns the row pointer, not
the controller pointer. **Confirmed.**

`0x86BCB0` has one direct callsite, at `0x97722F`, inside the higher-level
function `0x976EF0-0x977387`. That function:

1. resolves `Widgets/TextInput/Text_0`;
2. resolves `Widgets/TextInput/Input` against the same row root and, when the
   path exists, hides it and retains its underlying Scaleform value;
3. composes three incoming erased-callable roles—the editor factory,
   completion action, and disabled predicate—and constructs a temporary
   `CS::EditProperty` row;
4. calls `0x86BCB0`;
5. writes the enabled byte to the `CS::EditProperty` row at offset `+0x80`
   (the controller pointer is stored separately at row offset `+0x78`); and
6. copies/appends the row through `0x928630` into the page fixed vector at
   owner `+0x1268`, whose count is at `+0x1AF0` and capacity is 16, then tears
   down the temporary row through `0x86B500`.

This is the current priority candidate for a native “add TextInput row”
boundary. Its recovered nine-argument contract is:

| # | Recovered role | Observed at both native callsites |
|---:|---|---|
| 1 | page/owner | `CS::MatchingDialog*` |
| 2 | row text references | native `TextReferences*` |
| 3 | bound value | retained native `CS::MenuString*` at owner `+0x1B50` |
| 4 | editor factory | erased factory wrapping target `0x81D700`; source is consumed/emptied |
| 5 | initial text | default-constructed empty native `CS::MenuString*`; copied |
| 6 | placeholder text | default-constructed empty native `CS::MenuString*`; copied |
| 7 | completion action | nonempty erased `std::function<void()>*`, incorporated into the activation closure |
| 8 | disabled predicate | erased `std::function<bool()>*`; cloned |
| 9 | enabled state | byte value `1` |

**Confirmed by direct data flow and both native callsites.** Those callsites
are `0x91B8E2` and `0x91C219`, both inside `0x91B010-0x91CC1E`; the routine
installs `CS::MatchingDialog::vftable` at `0x91B0B2`. The native class is thus
identified, although the human-facing screen and field names still require an
in-game observation. This table records the unchanged vanilla MatchingDialog
callsites; ERNativeUI's guarded experiment deliberately supplies the
ABI-compatible `0x81D610` character-creation factory instead.

### `CS::TextInputController`

RTTI identifies the primary vtable at RVA `0x2B1B128`; the constructor is
`0x977CB0-0x977EA2`, the destructor is `0x977EB0`, and the deleting destructor
at `0x978040` frees exactly `0x290` bytes. **Confirmed.**

| Offset | Confirmed use |
|---:|---|
| `+0x10` | base property/path object |
| `+0x70` | composed activation/editor-launch callable; callable pointer at `+0xA8` |
| `+0xB0` | `Text_0` wrapper; nested Scaleform value at `+0xB8` |
| `+0x110` | `GrayoutItem` wrapper; nested value at `+0x118` |
| `+0x170` | `TextOnEmpty` wrapper; nested value at `+0x178` |
| `+0x1D0` | external bound-value pointer |
| `+0x1D8` | first owned string/value buffer |
| `+0x210` | placeholder string/value buffer |
| `+0x248` | disabled state derived from `!enabled` |
| `+0x250` | cloned disabled predicate; callable pointer at `+0x288` |

The constructor resolves the relative literals `Text_0`, `GrayoutItem`, and
`TextOnEmpty`. It copies both string/value buffers, applies the placeholder to
`TextOnEmpty`, and initializes enabled/grayed presentation from the byte option
or second callable. **Confirmed.** Ownership beyond these copied members is not
yet generalized to a public contract.

The seven observed primary vtable entries are:

| Slot | RVA | Evidence-bounded interpretation |
|---:|---:|---|
| 0 | `0x86BE90` | obtains a manager and passes this controller through manager slot `+0x68`; activation/registration is inferred |
| 1 | `0x978040` | deleting destructor, confirmed |
| 2 | `0x978260` | refreshes availability, bound text, `Text_0`, `GrayoutItem`, and `TextOnEmpty`, confirmed |
| 3 | `0x9781F0` | copies the owned buffer at `+0x1D8` to the destination referenced by `+0x1D0`; write-back mechanics confirmed, commit/cancel dispatch timing unknown |
| 4 | `0x978080` | produces text through an output argument and has a native-message fallback; exact UI role unknown |
| 5 | `0x9781E0` | returns false, confirmed |
| 6 | `0x86BED0` | resolves relative path `Caption` into the output wrapper, confirmed |

Slot numbers are indices in this recovered vtable only; they are not stable API
numbers and should not be hooked independently in production.

### `CS::TextInputDialog`

RTTI gives vtable RVA `0x2B2B908` and the hierarchy
`TextInputDialog -> MenuWindow -> SceneObjModifier -> DLReferenceCountObject ->
MenuJobRunnable -> ComponentStack`. Its constructor is `0x9B9E00`, destructor
`0x9B9FC0`, and deleting destructor `0x9BA050`; the latter frees `0xBC0` bytes.
**Confirmed.** The sole static constructor call found is `0x7F6C8C` inside the
factory `0x7F6BF0-0x7F6CD0`. That factory allocates `0xBC0`, clones a supplied
type-erased callable, forwards three preceding arguments to the constructor,
and returns the dialog. Its callers may be indirect; none should be invented
from the absence of a direct `call rel32` reference.

| Offset | Confirmed use |
|---:|---|
| `+0xA38` | base Scaleform value/path object |
| `+0xA98` | `TextInput/Text_0` control wrapper; nested value at `+0xAA0` |
| `+0xB00` | auxiliary object constructed from one factory argument |
| `+0xB78` | type-erased completion callable; callable pointer at `+0xBB0` |
| `+0xBB8` | one-shot completion guard |

The constructor resolves `TextInput/Text_0` and `TextInput`, applies dimensions
stored near `+0xB70`, sets the displayed text, and activates the control.
**Confirmed.** Specialized vtable entry `0x9BA090` writes native kind `0x22`
to its output. Entry `0x9BA0A0` performs the completion lifecycle: after the
control reports completion it sets the one-shot guard; result state `1`
extracts the entered text and invokes the callable once, while the other path
does not; both construct a native action and call the dialog's `MenuWindow`
Back slot (`+0x60`, target `0x747CD0`) to close. **Confirmed.** Human labels
such as “accepted” and “cancelled” remain inferred until an input trace maps
the numeric states.

### Native `SoftwareKeyboardJob` factory family

RTTI and bounded decompilation recover a separate job family with considerably
more precision than its name alone provides. **Confirmed:**

- `_anon_27CBC08A::SoftwareKeyboardJob` has CompleteObjectLocator
  `0x32FE8B0`, vtable `0x2AC5AD0`, and three recovered slots: `0x745BE0`,
  deleting destructor `0x81CF90`, and update/poll routine `0x7AC700`;
- `CS::MenuMemberJob<_anon_27CBC08A::SoftwareKeyboardJob>` has locator
  `0x32FE940`, vtable `0x2AC5AF0`, and slots `0x745BE0`, deleting destructor
  `0x81CF40`, and adapter routine `0x81D9D0`;
- common constructor `0x81CCB0` initializes a `0x1A8`-byte
  `SoftwareKeyboardJob`, copies its resource/configuration and erased callback,
  allocates a `0x20`-byte member job, and registers that member job with the
  parent; and
- the member-job adapter calls the function pointer stored by the constructor
  (`0x81DBF0`) on the owning software-keyboard job. The job's update slot polls
  and retires an owned child job. The exact numeric poll states remain unknown.

Four paired wrapper/factory routes feed that constructor:

| Higher wrapper | Factory | Resource | Message ID | Configured limit-like value | Bit 1 of flags at `+0x68` |
|---:|---:|---|---:|---:|---|
| `0x81D610` | `0x81CFD0` | `02_990_TextInput` | `0x470AE` | `0x10` | set |
| `0x81D700` | `0x81D160` | `02_991_TextInput2` | `0x7BCDC` | `8` | clear |
| `0x81D7F0` | `0x81D2F0` | `02_991_TextInput2` | `0x1E08D` | `8` | clear |
| `0x81D8E0` | `0x81D480` | `02_991_TextInput2` | `0x1E08C` | `8` | clear |

#### Character-name idle renderer and activation split

The native character-name relationship is direct, not inferred from nearby
strings. The apparent two-argument renderer at `0x83C630` receives a
character-command item/controller state and the row's Scaleform value. Its
first argument contains the active byte at `+0x18`, command-parameter pointer
at `+0x30`, and character-creation state/provider at `+0xA8`. This ABI is
inferred from the decompile and the function pointer stored at `0x2AB4998`;
there is no direct call reference that independently proves the prototype.

For presentation kind 1, selected through `0x83D370`, the renderer hides
`Value/Slider` and `Value/Color`, then shows `Value/Text` and `Cursor/Value`.
When the command parameter's first integer is `0x1A`, it obtains the current
native name through `0x78A260`. A non-empty name is written directly to
`Value/Text/Text_0`. For an empty name, the renderer constructs localized label
ID `0x9CB8`, passes style argument 2 through `0x760390`, resolves the same
`Text_0`, and applies the result through `0x74AC60`. This is the path that
produces the observed localized red `None entered` presentation. The precise
semantic names of `0x760390`, `0x74AC60`, and style value 2 remain unproved;
style 2 must not yet be published as a general "red text" enum. The renderer
hides `Value/Input` and `CommandList` before returning from the active
presentation path.

Activation is a separate path. Routine `0x7E5740` still resolves the hidden
`Value/Input` marker in `04_010`, obtains its world position through
`0xD83770`, and calls `0x81D610` at `0x7E5892`. That wrapper selects
`0x81CFD0` and `02_990_TextInput`. Hiding the marker therefore does not remove
its use as a positioning object. The `02_990` editable field and the inline
`02_040` TextInput value are both 400 pixels wide, which made the complete
`0x81D610` profile the safest geometry-compatible experiment.

The generic producer's fallback to `Widgets/TextInput/Text_0` remains
necessary because `02_040` has no named `Input` child. That fallback is enough
to launch and align the active editor without a GFX modification; it does not
import the character-creation idle frame, selected overlay, localized empty
label, or style-2 presentation. Exact visual parity therefore requires an
explicit `02_040` visual extension or equivalent runtime display composition,
not another editor-factory offset.

An inventory of all eleven extracted `04_*chrmake*` movies found 59
`DefineEditText` fields, all `readOnly=true`; none contains an editable
TextInput. Character creation therefore does not hide another editor in a
`04_000_*` movie. Its `04_010` movie supplies the framed read-only value and a
plain 8-by-35-pixel `Value/Input` positioning marker; `02_990_TextInput` is the
actual editable name overlay. On the tested game
build their SHA-256 values are respectively
`8D8B910A8ED90754D10B9B607F97AD43C09C97C6A5517F1D06C3E3C28F56CB9B`
and `65C5A9DB7595AC7CC1120F4267F0EAF5A91FFD2A1413909C9D3469AD84838234`.

Position flow is now confirmed. `0xD83770` asks the GFx display object for its
world matrix, transforms local `(0,0)`, and converts twips to pixel floats.
`0x976EF0` captures that result from `Widgets/TextInput/Text_0` (or the optional
`Input` override), and activation helper `0x976740` truncates it to an integer
point passed as argument five of the editor factory. `0x81D610 -> 0x81CFD0`
copies the point unchanged into the SoftwareKeyboardJob configuration.

The apparent character-row offset must not be copied blindly. Its read-only
field is 524 pixels wide, so its 400-pixel editor is centered using an anchor
about 70 pixels to the right. The `02_040` read-only and `02_990` editable
fields are both 400 pixels wide. For this equal-width case, only `02_990`'s
resource-local `Text_0` placement of `-8` pixels must be normalized. The
guarded prototype therefore wraps `0x81D610` in a pinned-host adapter that
copies the incoming point to the stack, adds `+8` to X, leaves Y unchanged,
and calls the native factory. It does not write into a game or Scaleform
object and does not require a patched GFX.

Each higher wrapper clones an erased callable, normalizes a string-like input,
calls its corresponding factory, and wraps the returned job through
`0x742720`. Each factory allocates `0x1A8` bytes and calls `0x81CCB0`.
The four configuration helpers resolve the listed message IDs, store the shown
numeric value at configuration offset `+0x6C`, and pass the same value to a
setter. Calling that field a character limit is a strong inference, not yet a
confirmed unit or meaning; the text represented by the message IDs is also
unknown until resolved from a concrete locale.

The inline-row relationship is now confirmed: `0x915D70` builds the exact
erased editor-factory object around target `0x81D700`, and both native
TextInput-row callsites pass that factory to `0x976EF0`. `0x81D700` calls the
`02_991_TextInput2` SoftwareKeyboardJob factory at `0x81D160`. There is still no
proved direct-call edge from this route to `TextInputDialog_Create` at
`0x7F6BF0`, and no call graph in this evidence reaches a Steam/platform API.
Device-specific activation, result encoding, and the relationship to the
separate `CS::TextInputDialog` remain open; the job must not be described as a
Steam keyboard without stronger evidence.

## The three layers we must recover

1. **Presentation**: widget selection, caption binding, displayed value,
   enabled/disabled state, caret, selection, and visual focus.
2. **Native ownership**: row constructor arguments, copied versus retained
   objects, text encoding, capacity, allocator/destructor pairs, and the page
   refresh mechanism.
3. **Editing lifecycle**: activation, keyboard/controller routing, live edits,
   confirm, cancel, validation, modal ownership, page Back behavior, and
   teardown while editing.

Finding only one layer cannot safely support the public feature.

## Discovery plan

### Phase 1: map the GFX contract

- **Completed for `02_040_optionsetting.gfx`:** record the TextInput sprite,
  named descendants, edit-text flags, frame references, and lack of a custom
  ActionScript class.
- **Completed:** compare the original six-row movie with the optional
  thirteen-row movie; TextInput and its shared widget ancestors are unchanged.
- **Completed statically:** the producer resolves `Widgets/TextInput/Input` as
  a Scaleform path against the row root and uses it only when resolution
  succeeds. It is not a static child in `02_040`; whether another resource or
  runtime setup creates it remains unknown.

This phase makes no game-memory writes.

### Phase 2: identify a real vanilla producer

- **Completed statically:** locate executable cross-references, identify
  `0x86BCB0` as the TextInput discriminator, and follow it to higher-level
  producer `0x976EF0` and its two vanilla callsites.
- **Completed statically:** identify both callsites as part of
  `CS::MatchingDialog` construction and recover all nine producer arguments.
- Find the reproducible human-facing screen and field represented by that
  native dialog; do not infer it solely from shared resource names.

### Phase 3: bounded runtime observation

The prioritized probes are deliberately incremental. Each must be opt-in,
rate-limited, and forward the original call unchanged:

1. Probe entry/return of `0x976EF0` only at its two known callsites. Record all
   nine arguments, thread, and collection count before/after. Its return value
   is the owner and is not a row/controller success identity.
2. Probe `0x86BCB0` only when called from `0x97722F`. Record its arguments and
   returned temporary row; obtain the controller only from validated row
   offset `+0x78`. Do not dump or retain either object.
3. After opening one known vanilla field, observe controller slot 0 once to
   identify the manager passed through slot `+0x68`. Do not call it manually.
4. Observe activation/completion and controller slot 3 on confirm and cancel
   in separate runs. Treat values as native `CS::MenuString` objects and use a
   validated accessor or documented bounded structural decoding, not raw
   UTF-16-buffer assumptions. This maps when write-back actually occurs.
5. Probe dialog factory `0x7F6BF0` only when a genuine vanilla TextInputDialog
   is opened. Record argument identities and the cloned callable pointer; then
   observe `0x9BA0A0` once per outcome to map result states.

For only the first few matching calls, record:

- function RVA and return address;
- page pointer and row/text-reference pointers;
- integer/register arguments and the required stack arguments;
- small, guarded snapshots of candidate objects before and after construction;
- validated row/controller identities and the thread ID.

Every probe observes and forwards the original call unchanged. It must not
invoke a guessed target, retain unproved pointers, dump large memory regions,
or log each frame/keystroke.

### Phase 4: recover behavior and lifetime

With one vanilla field open, perform separate runs for activation, one edit,
confirm, cancel, Back, page closure, and repeated reopening. Observe callbacks
and destructor paths independently. A production implementation requires:

- cancel restores the pre-edit value;
- confirm commits exactly once;
- a destroyed or paginated-away row leaves no native callback or focus owner;
- dialogs and page navigation cannot own input simultaneously;
- non-ASCII input survives without lossy conversion;
- all retained objects have a proved lifetime and destruction rule.

### Phase 5: controlled ERNativeUI prototype (retired)

The first implementation was deliberately private: it was compiled only by an
opt-in diagnostics build, restricted to the exact tested PE image, and
injected fixed rows only when Tarnished UI Showcase was registered. It used a
process-lifetime native `CS::MenuString`, supplied a nonempty host-owned
completion callable, and validated the resulting row, controller, and bound
pointer after construction.

That probe-only implementation and its build switch were removed when the
validated path moved into the append-only development API 1.1. The production
adapter in `src/native_text_input.cpp` retains the proven image checks,
postconditions, lifetime rules, editor alignment correction, and coherent
maximum-length patch. It replaces the fixed probe rows and per-limit factory
thunks with one binding per registered logical row and one immutable native
factory lookup. The historical results below are kept because they explain
why those safeguards exist.

#### First live result: matchmaking editor profile

The first in-game run on 1 September 2026 used editor wrapper `0x81D700` and
its `02_991_TextInput2` resource. The row constructed six times without a
warning, failed postcondition, or injection fault. Keyboard input, repeated
confirmation, display refresh, page closure, and reopen worked; the same bound
native value survived all six page lifetimes. Three completion callbacks ran
on the UI thread, while merely closing/reopening did not appear to invoke one.

The run also reproduced the profile's static constraints exactly: input stopped
at eight characters, and the active field appeared about 152 pixels right and
25 pixels above the expected local origin, allowing the previous read-only row
value to remain visible underneath. This validates the recovered factory/GFX
relationship and rejects `0x81D700` as the general ERNativeUI text profile.
The next bounded experiment uses the ABI-compatible character-creation wrapper
`0x81D610`, selecting `02_990_TextInput` and its coherent native 16-character
configuration rather than patching a limit immediate in isolation.

The empty-state hint is also constructed through the game's native borrowed-
literal `CS::MenuString` constructor at `0x6766F0`. This constructor retains
the supplied UTF-16 pointer rather than copying it, so the prototype supplies a
`static constexpr` literal from the process-pinned host DLL and destroys the
native wrapper normally after the row producer has copied it into the
controller. Stack strings and temporary `std::wstring` storage are unsafe for
this path. No native string-layout fields are written by ERNativeUI.

#### Second live result: character-name editor profile

The `0x81D610` experiment displayed the supplied empty-state hint, accepted
exactly the tested 16-character ASCII value `1234567890ABCDEF`, rejected a
seventeenth ASCII character, committed successfully, and preserved the value
after closing and reopening the page. The editable overlay hid the read-only
row value and was vertically aligned. Its initial slight left shift matched
the statically recovered `-8`-pixel placement in `02_990`; the subsequently
tested `+8` X adapter improved that alignment without changing input, commit,
or persistence behavior.

The remaining visual difference shown by the next comparison was entirely in
the idle row: the generic `02_040` field showed its plain muted placeholder,
whereas character creation showed the bracketed `04_010` value and red
localized empty-state label documented above. It was not evidence of a missing
sprite inside `02_990`. These tests still do not establish how the native
limit counts non-ASCII UTF-16 text.

#### Third live result: character-name idle frame

An opt-in GFX prototype then recreated the character-name idle frame inside
the shared `02_040` TextInput widget. The first generated movie patched the
empty-state color correctly but placed its newly generated external-image and
wrapper definitions after sprite 103, which already referenced the wrapper.
On a root row the placeholder became red while the bracket image stayed
absent. This separated deployment and TextInput-selection success from the
failed image path.

After moving the external-image, wrapper-sprite, and scaling-grid definitions
before sprite 103, the same root-row test displayed the native
`MENU_FL_Cursor_EntWaku` brackets. No `DoABC` class or `SymbolClass` entry was
added. The test therefore confirms definition-before-use for this direct
external-image display path and rejects the provisional claim that a bitmap
class binding was required to render it. Tests on an ERNativeUI subpage must
be evaluated separately because that page is hosted by
`02_042_pc_graphicsetting.gfx`, not the patched `02_040` root movie.

The corresponding structure-derived patch was then applied independently to
`02_042`. It allocated external-image character 91 and wrapper character 92,
placed their definitions before TextInput sprite 61, and installed
`CharacterFrame` at the same free depth 2. On 1 September 2026 the ERNativeUI
Text Input Prototype subpage displayed the red empty-state text and native
character-creation brackets in game. Combined with the earlier subpage tests
of activation, editing, confirmation, and persistence, this closes the
idle-versus-active presentation loop for both current host movies. These
results supplied the native basis for the development API 1.1 implementation.
Confirmation-only callback semantics and the compatibility-test foundation
are now implemented; non-ASCII length behavior and the full cancellation/
focus/teardown matrix remain live release gates before that API is frozen.

#### Fourth live result: non-power-of-two maximum lengths

Static analysis of the character-name route identifies two copies of the same
maximum-length setting. The character editor configuration writes 16 through
the clamping setter at `0x24196F0` into configuration offset `+0x60`, and also
writes 16 directly at configuration offset `+0x6C`. The software-keyboard job
builder at `0x7F4D30` copies that configuration block into the returned job, so
the corresponding job fields are `+0xC0` and `+0xCC`. The retained job pointer
is available in the editor-factory result before the caller enqueues it.

The next opt-in build therefore contains four independent, bounded probes:

- root-page maximum 3;
- root-page maximum 7;
- subpage maximum 3; and
- subpage maximum 7.

Three and seven are intentional non-powers of two. They test whether these
fields represent an ordinary numeric character limit rather than a flag,
bitmask, or power-of-two capacity. Each probe calls the already validated
16-character native factory, verifies the exact software-keyboard job vtable
and the original `(16, 16)` field pair, then writes and reads back the coherent
`(3, 3)` or `(7, 7)` pair before native code consumes the job. Any failed
identity, baseline, or read-back check disables the private experiment for the
remainder of the process. The four factories and four bound values are fixed
and separate because native activation occurs after row construction; a
shared mutable current limit would race with later activations.

The in-game run confirmed the expected behavior on both host movies: each
maximum-3 row rejected its fourth ASCII character, each maximum-7 row rejected
its eighth, confirmed values survived reopening, and the four bound values
remained independent. This establishes that the paired fields accept ordinary
non-power-of-two numeric limits; they are not a power-of-two capacity or a
bitmask.

#### Fifth live result: limits above the native 16 preset

The 3/7 result does not establish whether 16 is merely the character-name
profile's chosen value or an upper bound elsewhere in the pipeline. Static
analysis supports a cautious 17/35 follow-up:

- no maximum-dependent allocation occurs while `0x81CCB0` constructs the
  fixed-size `SoftwareKeyboardJob`;
- after ERNativeUI patches the returned job and before activation, the
  configuration is copied again by `0x7F4900`, `0x7F44D0`, and `0x7F4870`;
- the later `TextInputDialog` factory at `0x7F6BF0` allocates and constructs
  the dialog only after those copies;
- constructor `0x9B9E00` reads the copied secondary limit and passes it to
  `0xD85B00`; that helper validates the Scaleform text object and writes the
  requested value to its internal maximum-count field at `+0xC8`, with no
  upper clamp or limit-dependent allocation; and
- `02_990_TextInput` does not declare a static `DefineEditText` maximum.

The native and Scaleform strings on this route are dynamic rather than fixed
16- or 32-character arrays. The exact-build diagnostic therefore kept
the confirmed 3/7 controls and added independent maximum-17 and maximum-35
controls to both root and subpage. Seventeen crosses the known preset by the
smallest possible amount; 35 can expose a hidden 32-character boundary or
power-of-two capacity.

The in-game run passed on both host movies. Each maximum-17 row accepted 17
ASCII characters and rejected character 18; each maximum-35 row accepted 35
and rejected character 36. Confirmation, reopening, independent bound values,
focus release, and navigation behaved normally. Together with the 3/7 run,
this proves that the current editor route accepts per-row runtime limits on
both sides of its native 16-character preset, including a non-power-of-two
value above 32. It does not establish how the same field counts non-ASCII
UTF-16 input or a safe unrestricted upper bound.

These were private, exact-build diagnostics. Their validated 1-through-35
range now bounds the unreleased TextInput API, while the released 1.0 ABI
remains unchanged.

### Confirm, cancel, and programmatic-write ordering

Static follow-up closes an important ownership boundary. The character editor
copies its starting text into `SoftwareKeyboardJob`-owned storage at `+0xE8`;
it does not retain the external bound `CS::MenuString` as its editable buffer.
The row controller retains only that stable object's address at `+0x1D0` and
reads it synchronously when refreshing the idle value.

On Confirm, `0x9775E0 -> 0x976840` first assigns the returned string to the
external bound value, invokes the producer's completion action, and returns.
On Cancel, the dialog submits native action kind 3 without invoking that
completion action. The `SoftwareKeyboardJob` route agrees: its success chain
invokes the retained value callback, while cancel reaches terminal status 3
without doing so.

Production therefore applies `set_text` values to the stable bound object only
at the UI-frame boundary. An already active editor continues with its private
copy: Cancel leaves the programmatic value intact, while a later Confirm writes
the player's result and wins. Confirm callbacks only copy/commit host state;
native borrowed-string normalization is explicitly deferred to the next page
frame, including unchanged confirmation.

## Unreleased public API constraints

The current development headers implement the narrow TextInput surface needed
for validation. It remains unreleased and may be refined before API 1.1 is
frozen. The frozen-header and bidirectional negotiation coverage required
before publication is specified in the
[API version compatibility test plan](API_VERSION_COMPATIBILITY_TESTS.md).

- API 1.0 remains byte-for-byte supported; text input is an append-only 1.1
  capability negotiated explicitly.
- The C boundary exposes fixed-width integers, handles, function pointers, and
  UTF-16 pointers with explicit lengths/capacities—never STL or native game
  objects.
- Initial text is copied into host-owned bounded storage during registration.
- Callbacks receive a borrowed snapshot valid only for the documented call, or
  use explicit copy-in/copy-out functions. Allocations never cross CRTs.
- The changed callback is confirmation-only and fires once only when the
  confirmed value differs from canonical state. Cancel, unchanged confirmation,
  and programmatic writes do not invoke it.
- Initial and programmatic values are measured in UTF-16 code units at the
  public boundary. The current limit defaults to 16, accepts 1 through 35, and
  is fixed for the row at registration time. Native behavior for non-ASCII
  text remains a release-validation item.
- Native input modes and validation flags are not published speculatively.

## Initial test matrix

Before API 1.1 is frozen, the production candidate must still cover:

- keyboard typing, deletion, selection, confirm, and cancel;
- controller activation, game software keyboard if used, confirm, and cancel;
- mouse activation and focus transfer;
- empty, maximum-length, accented French, non-Latin, and surrogate-pair text;
- root page, subpage, next/previous pagination, native alert interaction, and
  repeated reopen;
- original six-row and optional thirteen-row GFX;
- standalone ERNativeUI and the established Solid Uncapper compatibility load.

## Open questions

- Which human-facing screen and field correspond to the confirmed
  `CS::MatchingDialog` callsites?
- How do devices select and interact with the confirmed SoftwareKeyboardJob
  editor route?
- Is the separate native `TextInputDialog` connected indirectly, or is it an
  independent UI path?
- What encoding and length unit apply to native `CS::MenuString` values?
- Does the statically supported minimum limit of 1 pass the empty/one/two
  boundary test on both host movies?
- Which exact destruction/focus sequence follows cancellation and page
  teardown across every supported input device?
- How is focus released when pagination replaces the physical page contents?
- Does the widget exist on built-in tabs other than Controller Settings with a
  compatible contract?

Answers should be added with raw evidence and failed experiments before they
are summarized as production behavior.
