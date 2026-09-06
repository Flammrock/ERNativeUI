# Scaleform runtime model and executable seeds

This reference combines two deliberately separate kinds of evidence:

- the public Scaleform SDK model, used only to form precise questions; and
- validated export, RTTI, string, vtable, and call-graph seeds from one Elden
  Ring executable, used to navigate the game's statically linked runtime.

The public SDK is not Elden Ring's ABI. Similar names do not establish object
layout, calling convention, ownership, thread, or a safe hook. Likewise, a
generic runtime implementation inside the executable is not automatically a
good integration boundary. ERNativeUI prefers the typed game wrappers and
native owners summarized in
[Elden Ring native UI system](../architecture/native-ui-system.md).

## Reference build and evidence vocabulary

Every RVA and section coordinate on this page belongs to this exact Windows
image:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The labels below follow the [research evidence vocabulary](../README.md#evidence-labels):

- **Confirmed** identifies a relationship encoded in the exact PE, validated
  RTTI, bounded static flow, or a controlled live result.
- **Inferred** identifies a strongly supported role whose complete native
  contract is not yet observed.
- **Lead only** identifies a useful navigation seed with no assigned game-side
  behavior.
- **Rejected** records a disproved interpretation.

Vtable entries and native offsets are build-locked research coordinates, not
public addresses. Names quoted from RTTI identify recovered compiled types;
names beginning with `FUN_` are Ghidra defaults. Neither is a reconstruction of
FromSoftware's source code.

## Public Scaleform reference model

Autodesk's archived Scaleform documentation describes this general lifecycle:

```text
GFx::Loader::CreateMovie(path)
        |
        v
GFx::MovieDef                         shared loaded movie data
        |
        | CreateInstance(...)
        v
GFx::Movie                            one playback/ActionScript state
        |
        +-- Advance(delta)            timeline, queued input, ActionScript
        +-- HandleEvent(event)        keyboard/controller-style input
        +-- GetVariable / Invoke      path-based C++ -> ActionScript bridge
        +-- Capture / display handle  render snapshot
        `-- Release                   movie-instance lifetime
```

`MovieDef::CreateInstance` returns a referenced movie instance. Complex
`GFx::Value` objects belong to that movie's ActionScript runtime and must be
released before the movie dies. `Advance` processes queued input and
ActionScript work as well as animation. With multithreaded rendering, movie
mutation and render submission are separated by capture snapshots.

Therefore, locating or loading a `.gfx` file is not a complete custom screen.
A safe game integration also needs resource binding, movie/player ownership,
update and render registration, input/focus routing, layer/viewport placement,
and ordered teardown.

### Values, objects, and display objects

The public API has two related C++ bridges:

- `GFx::Movie` supplies path-based operations such as `GetVariable`,
  `SetVariable`, and `Invoke`.
- `GFx::Value` represents a primitive, object, array, or display object.
  Complex values expose operations such as `GetMember`, `SetMember`, `Invoke`,
  `SetDisplayInfo`, and `SetText`.

Creating an ActionScript object is not the same as placing a display object on
the stage. Public `Movie::CreateObject` can create an ActionScript object or
class instance, but `GFx::Value` alone does not provide a general stage
placement operation. Runtime visual construction still depends on a movie's
timeline/ActionScript or a game-specific binding.

Applied to ERNativeUI:

- resolving `WindowList/.../Text_0` and setting text is value access;
- cloning a placement offline changes the movie definition;
- constructing a focusable row uses Elden Ring's native row/control model; and
- building an arbitrary runtime visual tree requires a proven stage owner, not
  merely `CreateObject`.

### Event directions

```text
game/native code -> Scaleform
    Movie::HandleEvent
    Movie::GetVariable / SetVariable / Invoke
    GFx::Value direct-access operations

Scaleform/ActionScript -> game/native code
    FSCommandHandler
    ExternalInterface
    FunctionHandler-backed function objects
```

Static inspection of `02_040_optionsetting.gfx` found no menu-specific
ActionScript listener, `ExternalInterface`, or `FSCommand` use. Its settings
behavior is driven primarily by Elden Ring's native menu, scene-object, and
control layers. Other movies may use different mechanisms.

The executable and loader setup prove that the game installs a
`CSScaleformFsCommandHandler`, but its concrete handler override at
`0xD6D790` is exactly `C2 00 00` (`ret 0`) on this build. It is therefore a
**rejected** event bridge for Game Options, even though generic Scaleform
`FSCommand:` diagnostics also exist in the executable.

## PE export-table boundary

**Confirmed:** Scaleform is statically linked into the main executable; the PE
does not import a Scaleform DLL. The image exports 560 named symbols, 209 of
which contain a Scaleform namespace. Most are memory, file, threading,
reference-counting, render-math, or system support. Only four named exports
belong to `Scaleform::GFx` itself:

| RVA | Demangled exported symbol |
|---:|---|
| `0x112CE00` | `Scaleform::GFx::Loader::~Loader` |
| `0x112D180` | `Scaleform::GFx::System::Destroy()` |
| `0x112D170` | `Scaleform::GFx::System::Init(HeapDesc const&, SysAllocBase*)` |
| `0x1B93E0` | `Scaleform::GFx::System::Init(SysAllocBase*)` |

No named export provides `GFx::Value` member access, invocation, movie
creation, movie advance, or event handling. This is a negative statement about
the export table only: implementations and diagnostics for many of those
features exist in the linked runtime, but `GetProcAddress` cannot discover
them by SDK method name.

A Scaleform integration must therefore reach a validated game wrapper,
vtable, or callsite. Treating `eldenring.exe` as a complete public Scaleform
SDK DLL would expose a tiny and misleading surface.

## How the RTTI coordinates were validated

On MSVC x64, a type-descriptor name follows two pointers in `.data`. A valid
Complete Object Locator in `.rdata` contains the type-descriptor RVA at
`+0x0C`, class-hierarchy descriptor RVA at `+0x10`, and its own RVA at
`+0x14`. A vtable is preceded by an image-base-relative pointer to that
locator. The inventory accepted an entry only after validating all of those
relationships.

| Section | File offset | RVA | Mapping delta |
|---|---:|---:|---:|
| `.rdata` | `0x29B0E00` | `0x29B2000` | `+0x1200` |
| `.data` | `0x3B13600` | `0x3B15000` | `+0x1A00` |

The section-specific delta matters. Applying the `.rdata` mapping to a type
name in `.data` yields an RVA `0x800` too low.

For the full current inheritance analysis, including multiple-inheritance
subobjects and query validation, see
[Native UI class hierarchy](native-ui-class-hierarchy.md).

## Native menu and scene-object RTTI seeds

The structures in this table are **confirmed** by validated RTTI. Base
sequences are recorded in the order encoded by the MSVC base-class array,
beginning with the class itself.

| Class | Type descriptor RVA | COL RVA | Vtable RVA | Confirmed base sequence |
|---|---:|---:|---:|---|
| `CS::MenuWindow` | `0x3C934B0` | `0x32ED4F8` | `0x2A96AE0` | `MenuWindow -> SceneObjModifier -> DLReferenceCountObject -> MenuJobRunnable -> ComponentStack` |
| `CS::MenuWindowProxy` | `0x3C94460` | `0x32EDF58` | `0x2A97398` | `MenuWindowProxy -> DLNonCopyable` |
| `CS::MenuResource` | `0x3C94490` | `0x32EDFD8` | `0x2A973A8` | `MenuResource -> DLNonCopyable` |
| `CS::MenuJob` | `0x3C93BF8` | `0x32F4EE0` | `0x2AAB700` | `MenuJob -> DLReferenceCountObject` |
| `CS::MenuWindowJob` | `0x3C9C270` | `0x32F5678` | `0x2AAC868` | `MenuWindowJob -> MenuJob -> DLReferenceCountObject` |
| `CS::CSScaleformValue` | `0x3C944B8` | `0x32EE058` | `0x2A97AD8` | `CSScaleformValue -> Scaleform::GFx::Value -> Scaleform::ListNode<Value>` |
| `CS::SceneObjProxy` | `0x3C94568` | `0x32EE1D8` | `0x2A97AF0` | `SceneObjProxy -> ComponentProxy` |
| `CS::SceneObjStateControl` | `0x3C945D8` | `0x32EE2D0` | `0x2A97E58` | `SceneObjStateControl` |
| `CS::SceneObjTab` | `0x3C94608` | `0x32EE348` | `0x2A97EC8` | `SceneObjTab -> SceneObjProxy -> ComponentProxy` |
| `CS::SliderCtrl` | `0x3C94630` | `0x32EE3D0` | `0x2A980B0` | `SliderCtrl -> MenuWindow -> ... -> ComponentStack` |
| `CS::SpinCtrl` | `0x3C94658` | `0x32EE470` | `0x2A98298` | `SpinCtrl -> SceneObjModifier -> DLReferenceCountObject` |
| `CS::TextInput` | `0x3C94680` | `0x32EE4F8` | `0x2A98340` | `TextInput -> SceneObjProxy -> ComponentProxy` |
| `CS::TextInputController` | `0x3CD6F10` | `0x3325A08` | `0x2B1B128` | `TextInputController -> PropertyController -> DLReferenceCountObject` |
| `CS::TextInputDialog` | `0x3CE0D30` | `0x332D208` | `0x2B2B908` | `TextInputDialog -> MenuWindow -> ... -> ComponentStack` |

The similarly named TextInput classes occupy distinct layers:

- `CS::TextInput` is a scene-object proxy;
- `CS::TextInputController` belongs to the property-controller layer; and
- `CS::TextInputDialog` is a complete menu window.

Their proximity does not make their constructors or layouts interchangeable.
The confirmed production relationship is documented in the
[TextInput case study](../case-studies/text-input.md).

## Resource, loader, and player RTTI seeds

| Class | Type descriptor RVA | COL RVA | Vtable RVA | Confirmed base sequence |
|---|---:|---:|---:|---|
| `CS::GfxFileCap` | `0x3C6A1A0` | `0x32B4848` | `0x29DB708` | `GfxFileCap -> FD4FileCap -> FD4ResCap -> FD4ResCapHolderItem` |
| `CS::GfxRepositoryImp` | `0x3D01F40` | `0x335D128` | `0x2BA7870` | `GfxRepositoryImp -> FD4ResRep -> FD4ResCap -> FD4ResCapHolderItem` |
| `CS::GfxResCap` | `0x3D021D0` | `0x335D5C0` | `0x2BA7C60` | `GfxResCap -> FD4ResCap -> FD4ResCapHolderItem` |
| `CS::CSScaleformImp` | `0x3D04888` | `0x3361A90` | `0x2BBC418` | `CSScaleformImp` |
| `CS::CSScaleformFsCommandHandler` | `0x3D04A28` | `0x3361DD0` | `0x2BBC6C8` | `CSScaleformFsCommandHandler -> GFx::FSCommandHandler -> GFx::State -> ...` |
| `CS::CSScaleformStep` | `0x3D04BF0` | `0x3362200` | `0x2BBD870` | `CSScaleformStep -> CSStepTask<...> -> FD4StepTaskBase<...> -> FD4TaskBase -> FD4ComponentBase` |
| `CS::CSScaleformSwfPlayer` | `0x3D05080` | `0x3362BB8` | `0x2BBE480` | `CSScaleformSwfPlayer` |
| `CS::CSScaleformMovieDef` | `0x3D050B0` | `0x3362C30` | `0x2BBE430` | `CSScaleformMovieDef -> DLReferenceCountObject` |
| `CS::CSScaleformLoader` | `0x3D050E0` | `0x3362CB0` | `0x2BBE448` | `CSScaleformLoader -> GFx::Loader -> GFx::StateBag -> GFx::FileTypeConstants` |
| `CS::CSScaleformFileOpener` | `0x3D05140` | `0x3362DA0` | `0x2BBE498` | `CSScaleformFileOpener -> GFx::FileOpener -> GFx::State -> ...` |
| `CS::CSScaleformImageCreator` | `0x3D05170` | `0x3362E50` | `0x2BBE4C0` | `CSScaleformImageCreator -> GFx::ImageCreator -> GFx::State -> ...` |
| `CS::CSScaleformSystem` | `0x3D05320` | `0x3363208` | `0x2BC0130` | `CSScaleformSystem` |
| `CS::CSScaleformThreadCommandQueue` | `0x3D05388` | `0x3363300` | `0x2BC0148` | `CSScaleformThreadCommandQueue -> Render::SingleThreadCommandQueue -> ThreadCommandQueue` |
| `CS::CSScaleformArabicTranslator` | `0x3D053C0` | `0x3363388` | `0x2BC0168` | `CSScaleformArabicTranslator -> GFx::Translator -> GFx::State -> ...` |

This inventory proves that Elden Ring has its own GFX repository, GFx-derived
loader, file opener, image creator, movie-definition owner, SWF player, value
wrapper, startup/resource task, and render command queue. RTTI alone does not
prove their order. Focused analysis has separately established resource
lookup, definition acquisition, movie instantiation, active-player advance,
and destruction; complete capture/render registration remains unresolved.

## Working correspondences

The public model is useful only when paired with game-side evidence:

| Elden Ring evidence | Public analogue | Current classification |
|---|---|---|
| Path resolver `0x74B140` | Formatted path followed by component-wise GetMember-equivalent traversal from a source value | **Confirmed.** It is not a proved `Movie::GetVariable` wrapper. |
| UTF-16 text setter `0x74AE50` | `GFx::Value::SetText(wchar_t const*)` | **Confirmed behavior** through type checks, call shape, and live use; the public name remains an analogue rather than an imported symbol. |
| Wrapper cluster near `0x74AE50` | TextField text, color, and scroll operations | **Confirmed as specialized operations.** Two narrow property names remain inferred; no generic Invoke/SetMember surface was found there. |
| `CSScaleformSwfPlayer` | Owner/wrapper of a `GFx::Movie` playback instance | **Confirmed;** raw movie pointer at `+0x18`. |
| `CSScaleformMovieDef` | Wrapper around shared `GFx::MovieDef` data | **Confirmed;** raw definition pointer at `+0x10`. |
| `GfxRepositoryImp` / `GfxResCap` | Game resource repository and capability layer above the loader | **Confirmed class and resource-layer relationship;** not a public constructor. |
| `CSScaleformStep` | Startup or resource-preload state machine | **Confirmed.** It is not the per-frame player loop. |
| `SceneObjProxy` and native controls | Binding between a menu owner and named display objects | **Confirmed narrow value/owner bridge;** the complete owner at proxy `+0x20` remains unresolved. |

See [Scene-object proxy and native Scaleform bridge](scene-object-bridge.md)
for the validated member traversal, text/color helpers, and value destruction.

## Priority vtable seeds

Entries are executable RVAs in zero-based slot order. They make exact-build
queries deterministic; an entry has no semantic name merely because it
occupies a particular slot.

| Class / vtable RVA | Entry RVAs by slot |
|---|---|
| `MenuWindow` / `0x2A96AE0` | `0:735100`, `1:744F60`, `2:7463C0`, `3:746000`, `4:745150`, `5:7455E0`, `6:745D80`, `7:746A20`, `8:7469C0`, `9:746880`, `10:735150`, `11:746820`, `12:747CD0` |
| `MenuWindowProxy` / `0x2A97398` | `0:749E00` |
| `MenuResource` / `0x2A973A8` | `0:749DC0` |
| `MenuJob` / `0x2AAB700` | `0:745BE0`, `1:7A7F00`, `2:251EC90` |
| `MenuWindowJob` / `0x2AAC868` | `0:745BE0`, `1:7AD6D0`, `2:7AE040` |
| `CSScaleformValue` / `0x2A97AD8` | `0:D87010`, `1:74BA50` |
| `SceneObjProxy` / `0x2A97AF0` | `0:74C940`, `1:74C930` |
| `SceneObjStateControl` / `0x2A97E58` | `0:74E100` |
| `SceneObjTab` / `0x2A97EC8` | `0:74C940`, `1:74C930` |
| `SliderCtrl` / `0x2A980B0` | The 13 `MenuWindow` entries, then `13:74FA20` |
| `SpinCtrl` / `0x2A98298` | `0:735100`, `1:74FEB0`, `2:750210` |
| `TextInput` / `0x2A98340` | `0:74C940`, `1:74C930` |
| `TextInputController` / `0x2B1B128` | `0:86BE90`, `1:978040`, `2:978260`, `3:9781F0`, `4:978080`, `5:9781E0`, `6:86BED0` |
| `TextInputDialog` / `0x2B2B908` | `0:735100`, `1:9BA050`, `2:9BA0A0`, `3:9BA090`, then `MenuWindow` slots `4` through `12` |
| `GfxRepositoryImp` / `0x2BA7870` | `0:CE9170`, `1:CE8BE0`, `2:2656E60`, `3:2656F00` |
| `CSScaleformImp` / `0x2BBC418` | `0:D6D1F0`, `1:D6C480` |
| `CSScaleformFsCommandHandler` / `0x2BBC6C8` | `0:D6D630`, `1:D6D790` |
| `CSScaleformMovieDef` / `0x2BBE430` | `0:D733A0`, `1:D72410` |
| `CSScaleformLoader` / `0x2BBE448` | `0:112CC30`, `1:D723C0`, `2:D73810`, `3:D73620`, `4:D73650`, `5:112CC50` |
| `CSScaleformSwfPlayer` / `0x2BBE480` | `0:D73D70`, `1:D72480` |
| `CSScaleformSystem` / `0x2BC0130` | `0:D7F590`, `1:D77AD0` |
| `CSScaleformThreadCommandQueue` / `0x2BC0148` | `0:D77B10`, `1:1163A10`, `2:1163A30` |

`MenuWindow` slot 12 is the runtime-validated native Back target at
`0x747CD0`. Its presence in `SliderCtrl` and `TextInputDialog` follows from
their inheritance. That establishes the slot relationship, not every caller
or the complete action-payload contract.

## String and implementation anchors

These references prove that a literal or generic implementation exists; they
do not prove that a particular Elden Ring screen executes it.

| Literal | RVA | Use as a navigation seed |
|---|---:|---|
| `Widgets/TextInput` | `0x2AD74E8` | Native selection of the shared settings-row widget |
| `Widgets/TextInput/Text_0` | `0x2B1AF58` | Native displayed-value path |
| `Widgets/TextInput/Input` | `0x2B1AF78` | Runtime-created or variant-dependent input-binding lead |
| `ScaleformTexRepository` | `0x2BBAB30` | Texture/resource repository setup |
| `CSScaleformSwfPlayer` | `0x2BBDAA8` | Runtime-class registration or diagnostics |
| `CSScaleformSystem` | `0x2BBE7F0` | System registration or diagnostics |
| `font.swf` | `0x2BC02D0` | Game font-movie loading |
| `gfxfontlib.swf` | `0x2CC3B28`, `0x2CC41D0` | Generic GFx font-library paths |
| `gotoAndStop` / `gotoAndPlay` | `0x2C6AA10` / `0x2C6AA20` | Generic ActionScript timeline dispatch |
| `FSCommand:` | `0x2C780C0` | Generic GFx command diagnostics |
| `_global.gfx_ime_candidate_list_state` | `0x2C7A700` | Compiled GFx IME support |
| `_global.gfx_ime_candidate_list_path` | `0x2C7A850` | Compiled GFx IME support |
| `flash.events.FocusEvent` | `0x2CBFD48` | Compiled AS3 focus support |
| `flash.events.MouseEvent` | `0x2CBFD10` | Compiled AS3 mouse support |
| `scaleform.gfx.KeyboardEventEx` | `0x2CBFDC0` | Compiled Scaleform keyboard extension |
| `scaleform.gfx.IMEEventEx` | `0x2CC0620` | Compiled Scaleform IME extension |
| `MovieDef  ` / `MovieView ` | `0x2CC4090` / `0x2CC4170` | Loader/player diagnostics and ownership paths |

Generic GFx diagnostics also mention `GetVariable`, `SetVariable`, object
creation, and method invocation. Their presence confirms linked-runtime
capability, not a safe Elden Ring wrapper.

### Dedicated text-editor movie family

| Literal | String RVA | Referencing function | Reference RVAs |
|---|---:|---:|---:|
| `02_990_TextInput` | `0x2AC5A78` | `FUN_14081CFD0` (`0x81CFD0`, size `0x18D`) | `0x81D0BB`, `0x81D0C2` |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081D160` (`0x81D160`, size `0x18D`) | `0x81D24B`, `0x81D252` |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081D2F0` (`0x81D2F0`, size `0x18D`) | `0x81D3DB`, `0x81D3E2` |
| `02_991_TextInput2` | `0x2AC5AA0` | `FUN_14081D480` (`0x81D480`, size `0x18D`) | `0x81D56B`, `0x81D572` |

The parallel, equal-sized functions were a compact comparison set. Subsequent
bounded analysis and live testing promoted the editor/controller/job path into
the production TextInput feature; see the
[TextInput case study](../case-studies/text-input.md). Their references inside
registration function `0xAEEF0` remain name/type material, not loader evidence.

### Generic runtime surfaces

| Runtime surface | Literal/reference evidence | Classification |
|---|---|---|
| Focus-manager API table | `FUN_140F4AFD0` (`0xF4AFD0`, size `0x706`) references `captureFocus` at `0xF4B01F`, `modalClip` at `0xF4B1B6`, `moveFocus` at `0xF4B1E5`, `setModalClip` at `0xF4B2EB`, `setControllerFocusGroup` at `0xF4B3F1`, and related queries through `0xF4B623`. | **Confirmed generic GFx implementation; lead only for game use.** The cluster resembles method registration/dispatch, not proof that Game Options invokes it. |
| EventDispatcher surface | `FUN_140FF9160` (`0xFF9160`, size `0x54D`) references `dispatchEvent` at `0xFF9194`, plus `clone` and `event`. | **Confirmed generic AVM/EventDispatcher implementation; lead only.** No demonstrated edge makes it Elden Ring's native page queue. |
| Timeline methods | `FUN_140F05530` references the `AvmSprite::SpriteGotoAndStop` argument diagnostic at `0xF05584`; `FUN_140F05690` references its GotoAndPlay counterpart at `0xF056E4`. | **Confirmed generic implementations; lead only.** Neither gives a safe game wrapper or direct signature. |

These landmarks become useful only if a typed game-side call graph reaches
them. Hooking them globally would affect shared runtime code across movies
without an established page owner or lifetime.

Names such as `GfxRepository`, `CSScaleformStep`, `CSScaleformSwfPlayer`, and
`CSScaleformValue` also appear in small registration functions around
`0xAD5B0..0xAFDA0`. Those references are provenance/RTTI evidence; they do not
name the small functions as constructors, loaders, or accessors.

## Input and IME boundary

The static import table contains the Windows message pump and keyboard/focus
primitives, DirectInput creation, and XInput ordinal imports. The only imported
IMM32 function is `ImmDisableIME`; usual composition/context functions are not
statically imported.

| Import | IAT RVA |
|---|---:|
| `DirectInput8Create` | `0x4C1132C` |
| `ImmDisableIME` | `0x4C11354` |
| `PeekMessageW` | `0x4C11B1C` |
| `TranslateMessage` | `0x4C11B2C` |
| `DispatchMessageW` | `0x4C11B34` |
| `MapVirtualKeyW` | `0x4C11B6C` |
| `ToAscii` | `0x4C11BBC` |
| `GetKeyboardState` | `0x4C11BC4` |
| `GetKeyState` | `0x4C11BCC` |
| `SetFocus` | `0x4C11BEC` |
| XInput ordinals `3` and `2` | `0x4C11EDC`, `0x4C11EE4` |

Validated RTTI also contains
`SoftwareKeyboard::detail::SoftwareKeyboardManagerImpl` (type descriptor
`0x3D55040`, vtable `0x32006C8`) deriving from
`SoftwareKeyboardManagerBase` (type descriptor `0x3D55090`, vtable
`0x32006F8`). Adjacent callback types name `GamepadTextInputDismissed_t` and
`GameOverlayActivated_t`.

Those facts made a platform software keyboard a credible early lead, but did
not establish a relationship to `CS::TextInputController` or
`CS::TextInputDialog`. The production TextInput route was proved instead from
its native producer, dialog/job ownership, input forwarding, confirmation,
cancellation, and cleanup. Generic IME strings and imported input APIs must
not be used to infer that route.

## Reproduce the seed inventory

First create the exact-build database with the
[Ghidra workflow](../tools/ghidra-workflow.md). All commands below are run from
the repository root and write only beneath ignored `research-work/`.

```powershell
$ghidra = 'C:\Tools\ghidra_12.1.3_PUBLIC'
```

### 1. Recover bounded Scaleform and menu classes

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query RttiClasses `
  -ScriptArgument @(
    'research-work\queries\scaleform-runtime\rtti',
    '80', '8', '8', '128',
    'CS::CSScaleform',
    'CS::Gfx',
    'CS::SceneObj',
    'CS::TextInput',
    'CS::MenuWindow'
  )
```

Check `queries.csv` for truncation, then validate each type descriptor,
Complete Object Locator, hierarchy descriptor, and vtable relation before
retaining it. Use the dedicated hierarchy reference rather than substring
matching when claiming inheritance.

### 2. Recover literal and reference anchors

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Anchors `
  -ScriptArgument @(
    'research-work\queries\scaleform-runtime\anchors.csv',
    '40', '100',
    'CSScaleformSwfPlayer',
    'ScaleformTexRepository',
    'font.swf',
    'Widgets/TextInput',
    'FSCommand:',
    'dispatchEvent',
    'gotoAndStop'
  )
```

The per-term match and reference limits are part of the evidence. Inspect the
truncation columns before treating an absence as meaningful.

### 3. Inspect only selected functions and vtables

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query FunctionContext `
  -ScriptArgument @(
    'research-work\queries\scaleform-runtime\context.md',
    '180', '200',
    '0xD6D790',
    '0xD7D540',
    '0xD7C900',
    '0xD7ADE0',
    '0xD73850',
    '0xF4AFD0',
    '0xFF9160'
  )

.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Vtable `
  -ScriptArgument @(
    'research-work\queries\scaleform-runtime\vtables.csv',
    '32',
    '0x2A96AE0',
    '0x2A97AF0',
    '0x2BBE430',
    '0x2BBE448',
    '0x2BBE480',
    '0x2BC0130'
  )
```

A table slot becomes a semantic method only after its object register,
construction, callers, arguments, and ownership have been checked. Multiple
inheritance may require an adjusted subobject pointer.

### 4. Compare the dedicated editor factories

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query CallGraph `
  -ScriptArgument @(
    'research-work\queries\scaleform-runtime\text-editors',
    'both', '2', '250',
    '0x81CFD0',
    '0x81D160',
    '0x81D2F0',
    '0x81D480'
  )
```

This comparison should preserve callsite, differing argument, returned owner,
and destruction evidence. A common helper does not make its four callers
semantically identical.

### 5. Keep generic and game-owned paths separate

Trace a generic runtime anchor only when a bounded graph reaches it from a
typed game wrapper. In particular:

- `0xF4AFD0`, `0xFF9160`, `0xF05530`, and `0xF05690` remain generic-runtime
  landmarks, not primary hook sites;
- the no-op at `0xD6D790` remains a negative result;
- a raw Scaleform call must not be promoted without its movie/page owner,
  thread, input relationship, and matching release path.

Current production boundaries and failure policies are recorded in the
[Native hook and call inventory](native-hook-inventory.md). Current feature
proofs live in the [TextInput](../case-studies/text-input.md),
[ColorPicker](../case-studies/color-picker.md),
[GFX presentation](../case-studies/gfx-presentation.md), and
[settings pages and pagination](../case-studies/settings-pages-and-pagination.md)
case studies.

## Artifact and legal boundary

The reproducible raw inventories remain local under ignored
`research-work/pe-inventory/`:

- `eldenring-2.7.0.0-exports.csv`;
- `eldenring-2.7.0.0-scaleform-exports.csv`;
- `eldenring-2.7.0.0-ui-rtti.csv`; and
- `eldenring-2.7.0.0-scaleform-anchors.csv`.

They contain names, addresses, inheritance, and bounded navigation data. Keep
the executable, Ghidra database, bulk disassembly/decompilation, raw full
indexes, and extracted game assets outside the repository as required by the
[research artifact policy](../README.md#artifact-policy). Publish derived
facts, small tables, diagrams, signatures, and original tooling; do not
publish proprietary executable or asset dumps.

## Primary public references

- [Autodesk: Scaleform `GFx::Loader`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/00889.html)
- [Autodesk: `MovieDef::CreateInstance`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/01126.html)
- [Autodesk: Scaleform `GFx::Movie`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/00974.html)
- [Autodesk: `Movie::Advance`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/00975.html)
- [Autodesk: C++ to ActionScript communication](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/game_communication/c_actionscript.html)
- [Autodesk: Direct Access API](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/game_communication/direct_access.html)
- [Autodesk: Scaleform `GFx::Value`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/01708.html)
- [Autodesk: processing input events](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/integration_tutorial/integration_game_engine/integration_processing.html)
- [Autodesk: multithreaded rendering concepts](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/renderer_guide/multi_threaded_concepts.html)
