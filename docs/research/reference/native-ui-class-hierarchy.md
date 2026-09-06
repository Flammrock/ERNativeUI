# Native UI class hierarchy

This reference records the bounded MSVC RTTI hierarchy recovered from the
Elden Ring executable. It establishes class membership, polymorphic
subobjects, and useful navigation anchors. It does **not** establish safe
constructors or public ERNativeUI types.

For the larger runtime model, read [Elden Ring native UI system](../architecture/native-ui-system.md).
For the exact research process, read the [Ghidra workflow](../tools/ghidra-workflow.md).

## Reference build and evidence boundary

All addresses on this page belong to this exact Windows executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The names are analytical labels derived from MSVC RTTI. They are not original
FromSoftware source declarations. Every RVA and object offset is build-locked
and private; none is part of the ERNativeUI ABI.

- **Confirmed** means RTTI, a Complete Object Locator, vtable association, or
  a bounded construction/call path establishes the relationship directly.
- **Inferred** means several structural observations agree but a complete
  native contract has not been observed.
- **Unresolved** means the item is a navigation lead only.

The raw bounded export remains local under the ignored
`research-work/queries/ui-hierarchy/` directory.

## Query validation

`ExportRttiHierarchy.java` matches a requested base only when a recovered base
descriptor's namespace or decorated type name equals the filter,
case-insensitively. It does not use substring matching for hierarchy results.
Symbol normalization is used only while locating RTTI metadata.

The reference export produced:

| Exact base filter | Matching locators | Unique derived types | Truncated |
|---|---:|---:|---:|
| `CS::MenuWindow` | 111 | 109 | no |
| `CS::MenuJob` | 132 | 132 | no |
| `CS::SceneObjModifier` | 141 | 138 | no |
| `CS::SceneObjProxy` | 3 | 3 | no |
| `CS::CSScaleformSwfPlayer` | 1 | 1 | no |
| `CS::TextInput` | 1 | 1 | no |

For every retained row, the base list contains the exact requested base,
declared and retained base counts agree, and the reported vtable refers to the
same Complete Object Locator. The two additional `MenuWindow` locators are
real secondary subobjects, not duplicate scanner output.

## Core recovered graphs

MSVC base arrays list complete ancestry beginning with the most-derived type:

```text
MenuWindow
`-- SceneObjModifier
    `-- DLUT::DLReferenceCountObject
        `-- MenuJobRunnable
            `-- ComponentStack

SceneObjProxy
`-- ComponentProxy

MenuJob
`-- DLUT::DLReferenceCountObject
```

These are separate ownership layers. `SceneObjProxy` is the small
value/movie proxy used by the native Scaleform bridge. It is neither a
`MenuWindow` nor a `MenuJob`. `MenuWindow` combines scene modification,
reference counting, runnable behavior, and a component stack. `MenuJob` is a
separately reference-counted asynchronous task family.

| Class | Type descriptor | Complete Object Locator | Primary vtable |
|---|---:|---:|---:|
| `CS::MenuWindow` | `0x3C934B0` | `0x32ED4F8` | `0x2A96AE0` |
| `CS::SceneObjModifier` | `0x3C934D8` | `0x32ED1D0` | `0x2A94308` |
| `CS::SceneObjProxy` | `0x3C94568` | `0x32EE1D8` | `0x2A97AF0` |
| `CS::MenuJob` | `0x3C93BF8` | `0x32F4EE0` | `0x2AAB700` |

## Configuration and OptionSetting family

```text
GenericListSelectDialog
`-- MenuWindow

PropertyEditDialog
`-- GenericItemSelectDialog<EditProperty>
    `-- GenericListSelectDialog
        `-- MenuWindow

OptionSettingDialog
`-- PropertyEditDialog

PadSettingDialog
`-- OptionSettingDialog

OptionSettingTopDialog
`-- MenuWindow
```

The visible Game Options page uses the ordinary `OptionSettingDialog` family.
`PadSettingDialog` is the specialized class behind Controller Settings.
ERNativeUI therefore does not treat Controller Settings as an ordinary
built-in destination; API 1.1 extends its dedicated binding model instead.

| Class | Complete Object Locator | Primary vtable |
|---|---:|---:|
| `CS::GenericListSelectDialog` | `0x32F0E08` | `0x2AA2148` |
| `CS::PropertyEditDialog` | `0x33181F0` | `0x2B04E08` |
| `CS::OptionSettingDialog` | `0x331F8A8` | `0x2B14888` |
| `CS::OptionSettingTopDialog` | `0x33234A0` | `0x2B16B48` |
| `CS::PadSettingDialog` | `0x33238F0` | `0x2B16DD8` |

Class membership makes inherited-slot comparison valid. It does not prove
that sibling constructors accept the same parameters or that their extended
fields share offsets. Production reaches each supported ordinary destination
through its independently validated two-argument materializer and then checks
the expected vtable, live capacity, and row count. See
[Configuration class map](../architecture/configuration-class-map.md).

## Dialog families

### Message boxes

```text
MessageBoxDialog
`-- CommandSelectDialog
    `-- GenericListSelectDialog
        `-- MenuWindow
```

`CS::MessageBoxDialog` has Complete Object Locator `0x331B3E0` and vtable
`0x2B065D0`. This is a useful comparison anchor for the game's dialog
families and ERNativeUI's confirmed generic blocking-dialog task. The name and
ancestry alone establish neither builder arguments, button-result encoding,
task ownership, nor teardown.

### Text input

`CS::TextInputDialog` derives directly from `MenuWindow` and has locator
`0x332D208` and vtable `0x2B2B908`. Its 13-slot table replaces the first four
slots and reuses the remaining `MenuWindow` methods, including native Back.

The similarly named classes are not interchangeable:

- `CS::TextInput` is a `SceneObjProxy` used by the editor movie;
- `CS::TextInputController` belongs to the property-controller layer; and
- `CS::TextInputDialog` is a complete `MenuWindow` that owns edit focus.

The `MenuJob` hierarchy also contains a native editor job family:

```text
anonymous::SoftwareKeyboardJob
`-- MenuSubJob
    `-- MenuJob

MenuMemberJob<anonymous::SoftwareKeyboardJob>
`-- MenuJob
```

| Object | Complete Object Locator | Vtable | Size |
|---|---:|---:|---:|
| `SoftwareKeyboardJob` | `0x32FE8B0` | `0x2AC5AD0` | `0x1A8` |
| `MenuMemberJob<SoftwareKeyboardJob>` | `0x32FE940` | `0x2AC5AF0` | `0x20` |

Their deleting destructors are `0x81CF90` and `0x81CF40`; operational slots
lead to parent update/poll routine `0x7AC700` and member adapter `0x81D9D0`.
Constructor `0x81CCB0` creates both objects, installs the validated vtables,
stores callback target `0x81DBF0`, and registers the member with the parent.

Four factories (`0x81CFD0`, `0x81D160`, `0x81D2F0`, `0x81D480`) select
`02_990_TextInput` or `02_991_TextInput2`; their higher wrappers are
`0x81D610`, `0x81D700`, `0x81D7F0`, and `0x81D8E0`.

This proves construction and membership relationships. It does not prove
that every `TextInputDialog` route uses this job family, which platform backs
it, or the semantic names of every poll state. `SoftwareKeyboardJob` is a
recovered native class name, not evidence of a Steam API call. The production
route and remaining limits are documented in the
[text-input case study](../case-studies/text-input.md).

### Tutorial variants

```text
CSTutorialDialog
`-- MenuWindow

CSTutorialModalDialog
`-- CSTutorialDialog

CSTutorialToastDialog
`-- CSTutorialDialog
```

| Class | Complete Object Locator | Vtable |
|---|---:|---:|
| `CS::CSTutorialDialog` | `0x332CF98` | `0x2B2B4A8` |
| `CS::CSTutorialModalDialog` | `0x332D038` | `0x2B2B640` |
| `CS::CSTutorialToastDialog` | `0x332D160` | `0x2B2B7A0` |

This confirms three presentation/lifecycle variants in one family. It does
not map tutorial identifiers, modal input policy, or a safe constructor.

### Site of Grace negative result

No retained `MenuWindow` or `MenuJob` class name contains `Grace`, `Site of
Grace`, or `Bonfire`. Shop, exchange-shop, world-map, and tutorial classes are
present, but their names do not substantiate a Grace-menu relationship.

This export therefore supplies no Grace-specific construction anchor. A
future investigation must begin from a live Grace handler/call site or a
verified GFX resource, not rename a generic dialog from proximity or visual
similarity.

## Multiple inheritance and `object_offset`

A Complete Object Locator describes one polymorphic subobject.
`object_offset` is the displacement of that subobject's vptr from the complete
object. One class may consequently have a primary locator/table at offset zero
and additional locators/tables for secondary bases.

| Complete type | Primary locator/table | Secondary locator/table |
|---|---|---|
| `CS::ChrMakeSliderReferencePanel` | `0x330AC48`, offset `0`, vtable `0x2AE6310` | `0x330AD28`, offset `0x1260`, vtable `0x2AE63A8` |
| `CS::ChrMakeSliderPanel` | `0x330B580`, offset `0`, vtable `0x2AE6D38` | `0x330B650`, offset `0xA38`, vtable `0x2AE6DA8` |

The 111 `MenuWindow` locators therefore describe 109 complete derived types.
Counting locators as classes overstates the inventory; discarding nonzero
offsets loses real interfaces. Calling a secondary table with a complete
object pointer rather than the adjusted subobject pointer violates the native
ABI. `constructor_displacement` is zero in this focused export, but that does
not remove the required `object_offset` adjustment.

## Why RTTI is not a constructor API

RTTI does not encode:

- a constructor address or parameter types;
- allocation size and alignment;
- the required scene, movie, or resource owner;
- whether arguments are copied, moved, retained, or borrowed;
- page/job registration and focus policy;
- partial-construction teardown; or
- the legal thread and game state.

Even a named `MessageBoxDialog`, `SoftwareKeyboardJob`, or
`OptionSettingTopDialog` must be followed to a real factory/call site and a
complete retirement path before it is called. Vtable proximity and base
compatibility are navigation evidence, not permission to instantiate.

## Reproduce and continue the map

1. Import the exact reference executable with the
   [Ghidra workflow](../tools/ghidra-workflow.md).
2. Run `RttiClasses` with an exact base filter; do not replace exact matching
   with a substring search.
3. Verify each Complete Object Locator, base array, vtable association, and
   `object_offset` before naming a relationship.
4. Find constructor and destructor writes independently with bounded
   `Anchors`, `FunctionContext`, and direct-caller queries.
5. Pair every proposed constructor with allocation, owner, registration,
   focus, and final release before a live call is considered.
6. Use nonzero `object_offset` records explicitly when following secondary
   virtual calls.

High-value next anchors are the natural `MessageBoxDialog` factories, the
`SoftwareKeyboardJob` callback at `0x81DBF0`, and a Grace-specific live
handler/resource. Every result must retain the exact executable identity and
the evidence level it actually proves.
