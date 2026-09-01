# Scaleform executable surface and RTTI seeds

Status: static inventory; slot semantics and call flow still require targeted
Ghidra analysis

This note describes the Scaleform and native-menu surface that can be recovered
directly from the PE export table, string table, and MSVC RTTI of the tested
`eldenring.exe`. It deliberately does not reproduce bulk disassembly or claim
that analytical names are FromSoftware's original source names.

Tested executable:

- product version: `2.7.0.0`;
- SHA-256:
  `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`;
- PE timestamp: `0x69E9C9B9`;
- image size: `0x5E09600`.

In this document, **confirmed** means encoded in this exact PE's export table,
section table, or validated RTTI structures. **Inferred** means that class names
and inheritance strongly suggest a role, but the relevant function has not yet
been followed through its callers. Vtable RVAs are navigation seeds, not stable
public addresses.

## The important export-table boundary

**Confirmed:** Scaleform is linked into the main executable. The PE imports no
Scaleform DLL. The executable exports 560 named symbols, 209 of which contain a
Scaleform namespace, but almost all of those belong to memory, file, threading,
reference-counting, render-math, or system support. Only four named exports are
in `Scaleform::GFx` itself:

| RVA | Exported symbol |
|---:|---|
| `0x112CE00` | `Scaleform::GFx::Loader::~Loader` |
| `0x112D180` | `Scaleform::GFx::System::Destroy()` |
| `0x112D170` | `Scaleform::GFx::System::Init(HeapDesc const&, SysAllocBase*)` |
| `0x1B93E0` | `Scaleform::GFx::System::Init(SysAllocBase*)` |

The named export set contains no `GFx::Value` member access, invocation, movie
creation, movie advance, or event-handling function. This is a negative claim
about the export table only: those implementations and their diagnostic strings
are present inside the statically linked runtime, but cannot be discovered with
`GetProcAddress` by those method names.

Consequently, an ERNativeUI Scaleform integration should target validated game
wrappers, vtables, or callsites. Treating the executable like a complete public
Scaleform SDK DLL would expose only a tiny and misleading part of the runtime.

## How the RTTI coordinates were derived

MSVC x64 stores a type-descriptor name after two pointers in `.data`. A valid
complete-object locator in `.rdata` contains the type-descriptor RVA at offset
`+0x0C`, its class-hierarchy descriptor RVA at `+0x10`, and its own RVA at
`+0x14`. A vtable is preceded by an absolute image-base-relative pointer to that
locator. The inventory validated all of those relationships before accepting a
row.

The relevant section mappings are:

| Section | File offset | RVA | Mapping delta |
|---|---:|---:|---:|
| `.rdata` | `0x29B0E00` | `0x29B2000` | `+0x1200` |
| `.data` | `0x3B13600` | `0x3B15000` | `+0x1A00` |

This distinction matters. Applying the `.rdata` delta to RTTI names in `.data`
produces an RVA that is `0x800` too low.

## Native menu and scene-object bridge

The following structures are all **confirmed by validated RTTI**. Base sequences
include the class itself first, as encoded by the MSVC base-class array.

| Class | Type descriptor RVA | COL RVA | Vtable RVA | Relevant confirmed base sequence |
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

This resolves an important ambiguity for text-input work. `CS::TextInput` is a
small scene-object proxy, `CS::TextInputController` belongs to the property
controller layer, and `CS::TextInputDialog` is a complete menu window. They are
three distinct lifecycle layers, not three interchangeable names for one row.

It also shows why proximity is not a safe way to guess behavior. `SliderCtrl`,
`SpinCtrl`, and `TextInput` have materially different base classes even though
their RTTI records occur next to one another.

## Scaleform ownership and loading bridge

| Class | Type descriptor RVA | COL RVA | Vtable RVA | Relevant confirmed base sequence |
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

The RTTI inventory proves that the engine has its own GFX resource repository,
GFx-derived loader, file opener, image creator, movie-definition owner, SWF
player, update task, value wrapper, and render command queue. RTTI alone does
not prove their call order. Focused constructor/caller/ownership analysis now
establishes resource lookup, definition loading, movie instantiation,
per-frame advance, and destruction; complete render submission remains
unresolved. See
[Scaleform movie loading and lifecycle](SCALEFORM_MOVIE_LIFECYCLE.md).

Follow-up analysis confirmed that the loader installs
`CSScaleformFsCommandHandler`, but vtable slot 1 targets RVA `0xD6D790`, whose
exact bytes are `C2 00 00` (`ret 0`). The game override is therefore a no-op in
this build, not a discovered movie-to-native event bridge. See
[Scaleform movie loading and lifecycle](SCALEFORM_MOVIE_LIFECYCLE.md) for the
call and ownership reconstruction.

## Priority vtable seeds

The entries below are executable RVAs read from each vtable, in zero-based slot
order. They are recorded to make targeted Ghidra queries deterministic. Except
where noted, no semantic name has yet been assigned to a slot.

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
| `SliderCtrl` / `0x2A980B0` | the 13 `MenuWindow` entries, then `13:74FA20` |
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

`MenuWindow` slot 12 is the already runtime-validated ERNativeUI native Back
target at RVA `0x747CD0`. Its presence in `SliderCtrl` and `TextInputDialog`
follows directly from their `MenuWindow` inheritance. This establishes the
slot relationship; it does not establish the meaning of every caller or its
complete action-payload contract.

## String anchors for movie, value, and input analysis

These are static cross-reference seeds, not proof that a path is executed by a
specific Elden Ring screen.

| Literal | RVA | What it can locate |
|---|---:|---|
| `Widgets/TextInput` | `0x2AD74E8` | Native selection of the shared row widget |
| `Widgets/TextInput/Text_0` | `0x2B1AF58` | Native displayed-value path |
| `Widgets/TextInput/Input` | `0x2B1AF78` | Native/runtime-created input binding candidate |
| `ScaleformTexRepository` | `0x2BBAB30` | Texture/resource repository setup |
| `CSScaleformSwfPlayer` | `0x2BBDAA8` | Engine runtime-class registration or diagnostics |
| `CSScaleformSystem` | `0x2BBE7F0` | System registration or diagnostics |
| `font.swf` | `0x2BC02D0` | Engine font movie loading |
| `gfxfontlib.swf` | `0x2CC3B28`, `0x2CC41D0` | GFx font-library loading paths |
| `gotoAndStop` / `gotoAndPlay` | `0x2C6AA10` / `0x2C6AA20` | Internal ActionScript/movie method dispatch |
| `FSCommand:` | `0x2C780C0` | GFx FSCommand execution/logging path |
| `_global.gfx_ime_candidate_list_state` | `0x2C7A700` | Compiled GFx IME support |
| `_global.gfx_ime_candidate_list_path` | `0x2C7A850` | Compiled GFx IME support |
| `flash.events.FocusEvent` | `0x2CBFD48` | Compiled AS3 focus-event support |
| `flash.events.MouseEvent` | `0x2CBFD10` | Compiled AS3 mouse-event support |
| `scaleform.gfx.KeyboardEventEx` | `0x2CBFDC0` | Compiled Scaleform keyboard extension |
| `scaleform.gfx.IMEEventEx` | `0x2CC0620` | Compiled Scaleform IME extension |
| `MovieDef  "` / `MovieView "` | `0x2CC4090` / `0x2CC4170` | Loader/player diagnostics and ownership paths |

The generic GFx runtime also contains diagnostics for `GetVariable`,
`SetVariable`, object creation, and method invocation. Their presence confirms
that the linked runtime implements those capabilities, not that Elden Ring's
native wrappers expose them safely.

## Input and IME boundary

The static import table contains the Windows message pump and keyboard/focus
primitives (`PeekMessageW`, `TranslateMessage`, `DispatchMessageW`,
`MapVirtualKeyW`, `ToAscii`, `GetKeyboardState`, `GetKeyState`, and `SetFocus`),
DirectInput creation, and XInput ordinal imports. The only imported IMM32
function is `ImmDisableIME`; the executable does not statically import the
usual composition/context functions.

Useful IAT-entry RVAs for cross-reference queries are:

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

A separate validated RTTI cluster contains
`SoftwareKeyboard::detail::SoftwareKeyboardManagerImpl` (type descriptor
`0x3D55040`, vtable `0x32006C8`) deriving from
`SoftwareKeyboardManagerBase` (type descriptor `0x3D55090`, vtable
`0x32006F8`). Immediately adjacent callback types name
`GamepadTextInputDismissed_t` and `GameOverlayActivated_t`. This makes a
Steam/platform software-keyboard route a credible lead, but no static
relationship to `CS::TextInputController` or `CS::TextInputDialog` has been
established.

This does **not** prove that text entry is impossible or that every listed API
belongs to Scaleform. Calls may belong to the engine window/input layer, and a
function may be obtained dynamically. It does mean that compiled AS3 IME
strings alone are insufficient evidence for Elden Ring's active text-input
path. The `TextInputController` and `TextInputDialog` graphs should be followed
before choosing between native keyboard events, a platform software keyboard,
or a GFx IME path.

## What to query next in Ghidra

1. Follow references to the vtables above to identify constructors, object
   sizes, member initialization, and matching destruction paths.
2. Start at `GfxRepositoryImp`, `CSScaleformLoader`, `CSScaleformMovieDef`, and
   `CSScaleformSwfPlayer`; correlate their callers with `font.swf` and real menu
   GFX resource names to reconstruct load, instantiate, retain, and release.
3. Treat the confirmed no-op `CSScaleformFsCommandHandler` as a negative
   result, then investigate `ExternalInterface` and function-object callbacks
   as separate ActionScript-to-native candidates.
4. Follow `CSScaleformStep` and `CSScaleformSystem` callers to locate movie
   advance/render scheduling and the owning thread.
5. Use `CSScaleformValue`, `SceneObjProxy`, and the already validated path
   resolver/setter to recover get/set/invoke wrappers and value ownership.
6. Analyze `MenuJob -> MenuWindowJob -> MenuWindow`, then the Back slot, to
   recover page sequencing, focus ownership, event dispatch, and teardown.
7. Analyze all seven `TextInputController` slots and the three specialized
   `TextInputDialog` slots before adding any runtime probe.

Until those relationships are proved, loading a new movie or calling an
untyped vtable slot is not a safe experiment. Extending an existing movie
through validated scene paths and native row constructors remains the narrower
production boundary.

## Local generated inventories

The reproducible raw inventories stay under ignored `research-work/pe-inventory/`:

- `eldenring-2.7.0.0-exports.csv`;
- `eldenring-2.7.0.0-scaleform-exports.csv`;
- `eldenring-2.7.0.0-ui-rtti.csv`;
- `eldenring-2.7.0.0-scaleform-anchors.csv`.

They contain only addresses, names, inheritance, and bounded navigation data;
the executable and bulk disassembly are not copied into the repository.
