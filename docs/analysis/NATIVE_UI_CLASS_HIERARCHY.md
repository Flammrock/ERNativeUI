# Native UI class hierarchy

This note curates the bounded MSVC RTTI hierarchy export produced by
`ExportRttiHierarchy.java`. It records class membership and subobject layout,
not guessed constructor signatures. The raw CSV output remains under the
ignored `research-work/queries/ui-hierarchy/` directory.

The analysis applies to the documented Elden Ring 2.7.0.0 executable. No
Ghidra database or executable was opened or modified while preparing this
summary.

## Query validation

The exporter matches a requested base only when a recovered base descriptor's
namespace or decorated type name equals the filter, case-insensitively. It
does not use substring matching for the hierarchy query. Symbol-name
normalization is used only to discover RTTI metadata records.

The completed output is coherent:

| Exact base filter | Matching locators | Unique derived types | Truncated |
|---|---:|---:|---:|
| `CS::MenuWindow` | 111 | 109 | no |
| `CS::MenuJob` | 132 | 132 | no |
| `CS::SceneObjModifier` | 141 | 138 | no |
| `CS::SceneObjProxy` | 3 | 3 | no |
| `CS::CSScaleformSwfPlayer` | 1 | 1 | no |
| `CS::TextInput` | 1 | 1 | no |

For every retained class row, its matching base list contains the exact
requested base. Declared and retained base counts agree, and every reported
vftable is associated with the same CompleteObjectLocator. The two extra
`MenuWindow` locators are legitimate multiple-inheritance subobjects, not
duplicate scanner output.

## Core recovered graphs

MSVC base arrays list the complete ancestry beginning with the most-derived
type. The important core graphs are:

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

These are distinct ownership layers. `SceneObjProxy` is the small value/movie
proxy used by the native Scaleform bridge; it is not a `MenuWindow` and not a
`MenuJob`. `MenuWindow` combines a scene-object modifier with reference
counting and runnable/component-stack behavior. `MenuJob` is separately
reference-counted and is used for asynchronous/menu task composition.

The exact primary anchors are:

| Class | Type descriptor | CompleteObjectLocator | Primary vtable |
|---|---:|---:|---:|
| `CS::MenuWindow` | `0x3C934B0` | `0x32ED4F8` | `0x2A96AE0` |
| `CS::SceneObjModifier` | `0x3C934D8` | `0x32ED1D0` | `0x2A94308` |
| `CS::SceneObjProxy` | `0x3C94568` | `0x32EE1D8` | `0x2A97AF0` |
| `CS::MenuJob` | `0x3C93BF8` | `0x32F4EE0` | `0x2AAB700` |

## Game Options family

The recovered ancestry explains which current ERNativeUI boundaries share a
native family:

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

| Class | CompleteObjectLocator | Vtable |
|---|---:|---:|
| `CS::GenericListSelectDialog` | `0x32F0E08` | `0x2AA2148` |
| `CS::PropertyEditDialog` | `0x33181F0` | `0x2B04E08` |
| `CS::OptionSettingDialog` | `0x331F8A8` | `0x2B14888` |
| `CS::OptionSettingTopDialog` | `0x33234A0` | `0x2B16B48` |
| `CS::PadSettingDialog` | `0x33238F0` | `0x2B16DD8` |

This proves class membership and makes inherited virtual-slot comparison
valid. It does not prove that a constructor for one derived page accepts the
same arguments as another, that their extra fields have the same offsets, or
that an Audio/Graphics page can be materialized through the Controller
handler.

## Dialog families relevant to ERNativeUI

### Message boxes

```text
MessageBoxDialog
`-- CommandSelectDialog
    `-- GenericListSelectDialog
        `-- MenuWindow
```

`CS::MessageBoxDialog` has CompleteObjectLocator `0x331B3E0` and vtable
`0x2B065D0`. This is a high-value anchor for comparing the game's dialog
families with ERNativeUI's currently supported generic blocking-dialog task.
The name and ancestry do not establish its builder arguments, button-result
encoding, task owner, or teardown path.

### Text input

`CS::TextInputDialog` derives directly from `MenuWindow` and has locator
`0x332D208`, vtable `0x2B2B908`. This corroborates the recovered 13-slot table:
the dialog replaces the first four slots and reuses the remaining
`MenuWindow` methods, including native Back. The separate `CS::TextInput`
class is a `SceneObjProxy`, while `CS::TextInputController` belongs to the
property-controller layer; their similar names do not imply layout
interchangeability.

The MenuJob hierarchy adds a separate native editor job family:

```text
anonymous::SoftwareKeyboardJob
`-- MenuSubJob
    `-- MenuJob

MenuMemberJob<anonymous::SoftwareKeyboardJob>
`-- MenuJob
```

`SoftwareKeyboardJob` has locator `0x32FE8B0`, vtable `0x2AC5AD0`; its member
job wrapper has locator `0x32FE940`, vtable `0x2AC5AF0`. Both recovered tables
have three entries. Their deleting destructors are `0x81CF90` and `0x81CF40`;
their operational slots are the parent update/poll routine `0x7AC700` and
member adapter `0x81D9D0`.

Common constructor `0x81CCB0` constructs the `0x1A8`-byte parent and a
`0x20`-byte `MenuMemberJob`, installs both validated vtables, stores callback
target `0x81DBF0` in the member, and registers the member with the parent.
Four factories (`0x81CFD0`, `0x81D160`, `0x81D2F0`, `0x81D480`) select either
`02_990_TextInput` or `02_991_TextInput2` and feed the common constructor; the
paired higher wrappers are `0x81D610`, `0x81D700`, `0x81D7F0`, and `0x81D8E0`.

This proves construction and membership relationships that RTTI alone could
not establish. It still does **not** prove that `TextInputDialog` uses the job,
which device or platform backs it, or the semantic meaning of its callback and
poll states. In particular, `SoftwareKeyboardJob` is a recovered native class
name, not evidence of a Steam call.

### Tutorial variants

```text
CSTutorialDialog
`-- MenuWindow

CSTutorialModalDialog
`-- CSTutorialDialog

CSTutorialToastDialog
`-- CSTutorialDialog
```

| Class | CompleteObjectLocator | Vtable |
|---|---:|---:|
| `CS::CSTutorialDialog` | `0x332CF98` | `0x2B2B4A8` |
| `CS::CSTutorialModalDialog` | `0x332D038` | `0x2B2B640` |
| `CS::CSTutorialToastDialog` | `0x332D160` | `0x2B2B7A0` |

This validates three presentation/lifecycle variants in one family. It does
not by itself map the tutorial IDs, modal input policy, or constructor used by
the earlier working tutorial probes.

### Site of Grace

No retained `MenuWindow` or `MenuJob` class name contains `Grace`, `Site of
Grace`, or `Bonfire`. Shop, exchange-shop, world-map, and tutorial classes are
present, but their names do not substantiate a Grace-menu relationship.
Accordingly, this export supplies no Grace-specific construction anchor. A
future query should begin from a live Grace handler/callsite or a verified GFX
resource, not rename a generic dialog based on proximity or appearance.

## Multiple inheritance and `object_offset`

A CompleteObjectLocator describes one polymorphic subobject. Its
`object_offset` is the displacement of that subobject's vptr from the complete
object. A class may therefore have multiple locators and multiple vftables:
one primary table at offset zero and secondary tables for additional bases.

Two exported types demonstrate this directly:

| Complete type | Primary locator/table | Secondary locator/table |
|---|---|---|
| `CS::ChrMakeSliderReferencePanel` | `0x330AC48`, offset `0`, vtable `0x2AE6310` | `0x330AD28`, offset `0x1260`, vtable `0x2AE63A8` |
| `CS::ChrMakeSliderPanel` | `0x330B580`, offset `0`, vtable `0x2AE6D38` | `0x330B650`, offset `0xA38`, vtable `0x2AE6DA8` |

Thus the `111 MenuWindow locators` count represents `109` complete derived
types. Counting locators as classes would overstate the inventory, while
discarding nonzero-offset locators would lose real secondary interfaces.
Calling a secondary vtable entry with the complete-object pointer instead of
the adjusted subobject pointer would also violate the native ABI.

`constructor_displacement` is zero for the retained records in this focused
set. That does not remove the need for `this` adjustment: `object_offset`
already identifies the secondary subobject position.

## Why RTTI membership is not a constructor API

RTTI proves that an object can be viewed through a base and identifies its
polymorphic tables. It does not encode:

- constructor address or parameter types;
- allocation size and alignment;
- required scene/movie/resource owner;
- whether arguments are copied, moved, retained, or borrowed;
- page/job registration and input-focus policy;
- destructor entry point for partially constructed objects; or
- the thread and state in which construction is legal.

Even a named `MessageBoxDialog`, `SoftwareKeyboardJob`, or
`OptionSettingTopDialog` must be followed to a real factory/callsite and a
complete retirement path before ERNativeUI calls it. Vtable proximity and base
compatibility are navigation evidence, not authorization to instantiate a
class.

## High-value follow-up anchors

1. Trace factories and consumers of `MessageBoxDialog` and compare their task
   ownership/result path with the already confirmed generic dialog transport.
2. Trace callback target `0x81DBF0` and the child job created by the
   `SoftwareKeyboardJob` family to determine whether either connects to
   `TextInputDialog`; the current bounded direct-call graph does not prove it.
3. Compare the exact overridden slots of `OptionSettingTopDialog`,
   `OptionSettingDialog`, and `PadSettingDialog` before generalizing panel or
   tab support.
4. Use nonzero `object_offset` locators as explicit multiple-inheritance
   adjustment anchors in future callsite queries.
5. Acquire a Grace-specific runtime handler/resource anchor before issuing a
   narrower hierarchy query for that menu family.
