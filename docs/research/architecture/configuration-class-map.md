# Configuration and OptionSetting class map

This page maps the native class family behind Elden Ring's Configuration
screen. It preserves the object, factory, and tab-dispatch evidence needed to
maintain the existing built-in destinations and investigate custom top-level
tabs without treating nearby objects as interchangeable.

In this documentation, **Game Options** means the first visible Configuration
tab and the original ERNativeUI root. `ControllSetting` is the spelling used
by that GFX panel; it does not mean the separate Controller Settings screen.
The latter uses the specialized `PadSettingDialog` and is extended through API
1.1 Input Bindings.

## Reference build and evidence boundary

All addresses and layouts belong to this exact Windows executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The pseudocode and names are analytical, not original FromSoftware source.
They came from exact-build RTTI, vtables, bounded decompilation, call sites,
and controlled live observations.

- **Confirmed** means the relevant field access, construction path, call, or
  live object relationship was observed directly.
- **Inferred** means independent evidence agrees but a decisive field or
  call-site relation is still missing.
- **Unresolved** means a boundary is useful only for navigation.

Every RVA and native offset is private and build-locked. The public API uses
logical pages and handles instead.

## Recovered ownership model

**Confirmed:** Configuration has two native layers. The outer
`CS::OptionSettingTopDialog` owns category selection and a fixed collection of
category records. Its composite manager owns or caches the currently selected
`CS::OptionSettingDialog` panel.

```text
CS::OptionSettingTopDialog (direct CS::MenuWindow subclass)
|
|-- +0x0A38  native TabList selection object
|-- +0x1200  BasicViewItemList<MenuOptionCategory, 10>
|   |-- +0x1208  inline category-record storage
|   `-- +0x1760  current record count
|-- +0x1768  CS::CompositeOptionSettingDialog
|   |-- +0x0068  ten cached panel-pointer slots
|   `-- +0x00B8  current CS::OptionSettingDialog pointer
`-- +0x1870  CS::OptionSettingTopDialog::_SettingTabControl
    |-- +0x0010  pointer to top +0x0A38
    `-- +0x0018  pointer to top +0x1768
```

This rejects two unsafe shortcuts:

1. `OptionSettingTopDialog` is not an `OptionSettingDialog`; row constructors
   cannot be generalized to the tab host because both are Configuration UI.
2. `_SettingTabControl` is not a `MenuWindow`; it is a `SceneObjModifier`
   registered in the top window's component stack.

## RTTI and virtual surfaces

The full primary-base chains are:

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

OptionSettingTopDialog::_SettingTabControl
`-- SceneObjModifier
    `-- DLReferenceCountObject
```

`MenuJobRunnable` at displacement `0x10` and `ComponentStack` at displacement
`0x50` are additional `MenuWindow` subobjects inherited by every class on a
`MenuWindow` branch.

| Recovered class | Type descriptor | Complete Object Locator | Hierarchy descriptor | Primary vtable | Slots |
|---|---:|---:|---:|---:|---:|
| `CS::GenericListSelectDialog` | `0x3C97590` | `0x32F0E08` | `0x32F0E30` | `0x2AA2148` | 18 |
| `CS::PropertyEditDialog` | `0x3CC79B8` | `0x33181F0` | `0x3318218` | `0x2B04E08` | 21 |
| `CS::OptionSettingDialog` | `0x3CD0C10` | `0x331F8A8` | `0x331F8D0` | `0x2B14888` | 22 |
| `CS::PadSettingDialog` | `0x3CD43D0` | `0x33238F0` | `0x3323918` | `0x2B16DD8` | 22 |
| `CS::OptionSettingTopDialog` | `0x3CD4068` | `0x33234A0` | `0x33234C8` | `0x2B16B48` | 13 |
| `CS::OptionSettingTopDialog::_SettingTabControl` | `0x3CD40A0` | `0x3323540` | `0x3323568` | `0x2B16AB8` | 3 |

### Exact primary-slot comparison

A blank means that the recovered table ends before that slot. Semantic names
are attached only where another analysis established them.

| Slot | Generic list | Property edit | Option setting | Pad setting | Option top |
|---:|---:|---:|---:|---:|---:|
| 0 | `0x735100` | `0x735100` | `0x735100` | `0x735100` | `0x735100` |
| 1 | `0x77F8C0` | `0x91F830` | `0x953520` | `0x969060` | `0x967F20` |
| 2 | `0x7463C0` | `0x9217C0` | `0x9217C0` | `0x9217C0` | `0x9680B0` |
| 3 | `0x746000` | `0x746000` | `0x957CC0` | `0x957CC0` | `0x967FE0` |
| 4 | `0x906B50` | `0x976AA0` | `0x9536F0` | `0x9536F0` | `0x745150` |
| 5 | `0x7455E0` | `0x7455E0` | `0x7455E0` | `0x7455E0` | `0x7455E0` |
| 6 | `0x745D80` | `0x745D80` | `0x745D80` | `0x745D80` | `0x745D80` |
| 7 | `0x746A20` | `0x746A20` | `0x746A20` | `0x746A20` | `0x968170` |
| 8 | `0x7469C0` | `0x7469C0` | `0x7469C0` | `0x7469C0` | `0x7469C0` |
| 9 | `0x746880` | `0x746880` | `0x746880` | `0x746880` | `0x746880` |
| 10 | `0x77F9C0` | `0x77F9C0` | `0x77F9C0` | `0x77F9C0` | `0x967FF0` |
| 11 | `0x746820` | `0x976D40` | `0x958FF0` | `0x958FF0` | `0x746820` |
| 12 | `0x747CD0` | `0x747CD0` | `0x747CD0` | `0x747CD0` | `0x747CD0` |
| 13 | | `0x77FA10` | `0x9580C0` | `0x9580C0` | |
| 14 | | `0x77F930` | `0x77F930` | `0x77F930` | |
| 15 | | `0x77FA40` | `0x77FA40` | `0x77FA40` | |
| 16 | | `0x77F970` | `0x77F970` | `0x77F970` | |
| 17 | | `0x77F980` | `0x77F980` | `0x77F980` | |
| 18 | | `0x920E80` | `0x920E80` | `0x920E80` | |
| 19 | | `0x921010` | `0x921010` | `0x921010` | |
| 20 | | `0x9273C0` | `0x966240` | `0x966240` | |
| 21 | | | `0x959080` | `0x969250` | |

Confirmed interpretations:

- `0x9217C0` is a direct thunk to common `MenuWindow` frame dispatcher
  `0x7463C0`.
- Slot 11 is the selected-controller/page-frame pass; Option and Pad share
  `0x958FF0`.
- Slot 12 is the common native Back path at `0x747CD0`.
- Option and Pad differ only in slots 1 and 21, making Pad a narrow
  specialization rather than a separate tab host.
- Option-top slot 2, `0x9680B0`, calls the common frame dispatcher and then
  top-specific refresh `0x968BB0`.
- Option-top slot 7, `0x968170`, returns immediately. Slot 10, `0x967FF0`,
  tests whether the 64-bit field at `this + 0x98` exceeds one. Their semantic
  names remain unresolved.
- Option-top slot 3, `0x967FE0`, writes `0x0C` through its second argument;
  that value's meaning is unresolved.

The nested tab controller has exactly three entries:

| Slot | Target | Established behavior |
|---:|---:|---|
| 0 | `0x735100` | Shared reference-count/object entry; exact name unresolved. |
| 1 | `0x967F60` | Destruction wrapper, inferred from the standard wrapper pattern. |
| 2 | `0x9680D0` | Confirmed per-frame tab/panel input dispatcher. |

## Construction and teardown

### Top dialog

**Confirmed:** `0x807430` allocates exactly `0x18A0` bytes with alignment
eight, obtains a scene-object value through `0x7460D0`, and calls `0x9672C0`.
It returns the constructed pointer and destroys the temporary Scaleform value.

**Confirmed:** `0x9672C0` then:

1. calls the common `MenuWindow` constructor path `0x7427B0`;
2. installs the OptionSetting-top vtable;
3. resolves `TabList` and constructs the selector at `+0x0A38`;
4. constructs `BasicViewItemList<CS::MenuOptionCategory, 10>` at `+0x1200`;
5. constructs `CompositeOptionSettingDialog` at `+0x1768`;
6. constructs `_SettingTabControl` at `+0x1870` and stores selector/manager
   pointers;
7. registers it with the inherited `ComponentStack` through `0x734D40`; and
8. resolves `TabList/RightArrow` and `BackTabList`, binding the latter with
   `Item_0_%d`.

RVA `0x967B20` retires the owned tab list, composite manager, selector,
controller, and base window. Slot-1 wrapper `0x967F20` calls it. The vtable is
otherwise written only by the constructor, confirming the paired teardown.

### Ordinary and Pad panels

**Confirmed:** `0x94D710` calls Property-edit construction at `0x91CC20`,
installs the `OptionSettingDialog` vtable, and initializes added fields.
`0x94E790` performs the reverse cleanup before `0x91E010`.

**Confirmed:** `0x968E30` calls `0x94D710`, installs the `PadSettingDialog`
vtable, and initializes a Pad-specific object at `+0x1DA8`. Its resolved paths
include `XboxOne`, `Scarlette`, and `Win64`. `0x968FF0` retires that object and
then enters the Option-setting teardown.

Other vtable writers remain navigation leads unless their role is independently
established:

| Vtable | Referencing functions | Established boundary |
|---:|---|---|
| `0x2AA2148` Generic list | `0x77F750`, `0x78D2F0` | Construction/teardown pair still needs separation by call context. |
| `0x2B04E08` Property edit | `0x91CC20`, `0x91E010`, `0x929EA0` | First two are reached as Option base construction/teardown; third role unresolved. |
| `0x2B14888` Option setting | `0x94D710`, `0x94E790` | Confirmed construction and teardown. |
| `0x2B16DD8` Pad setting | `0x968E30`, `0x968FF0` | Confirmed construction and teardown. |

## Category storage and tab limits

**Confirmed:** the top dialog owns an inline Dantelion fixed vector with
capacity ten. Append helper `0x968CF0`, used by the top construction path:

- reads and increments the count at top `+0x1760`;
- rejects more than ten records;
- uses `0x88`-byte elements; and
- copy-constructs each record through `0x967230`.

Construction installs these list tables at top `+0x1200`:

```text
0x2AA1618  CS::MenuViewItemListBase
0x2B16AD8  CS::MenuViewItemList<CS::MenuOptionCategory>
0x2B16B10  CS::BasicViewItemList<CS::MenuOptionCategory, 10>
```

The final list's slot 1 at `0x968DA0` returns the count at list `+0x560`, which
is top `+0x1760`.

The stock GFX has a different bound: `TabList` and `BackTabList` each contain
nine instances, `Item_0_0` through `Item_0_8`. Thus ten is native record
capacity and nine is stock visible placement capacity. Neither is evidence
for an unbounded custom-tab contract. A tenth native record still requires a
compatible presentation, while arbitrary counts require virtualization,
scrolling, or an ERNativeUI-owned tab layer.

## Tab selection and composite-panel dispatch

**Confirmed:** `_SettingTabControl::slot2` at `0x9680D0`:

1. reads the selected index from the object referenced at `this + 0x10`;
2. asks the composite manager at `this + 0x18` whether tab input is allowed;
3. calls selector slot 2 with the real input byte, or zero while blocked;
4. reads the selected index again; and
5. advances the active panel with zero input when the index changed, otherwise
   with the real input byte.

This prevents a shoulder-button or mouse tab change from activating a control
on the newly selected panel in the same frame. The object at top `+0x0A38` is
therefore strongly inferred to be the authoritative tab selector; getter
`0x73AC70` supplies its before/after indices.

`CS::CompositeOptionSettingDialog` has Complete Object Locator `0x331E6C8`,
vtable `0x2B0C950`, and one recovered slot at `0x93C7C0`.

Constructor `0x93C090` and teardown `0x93C1A0` confirm:

- an embedded `SceneObjProxy` at `+0x08`;
- ten cached panel pointers at `+0x68..+0xB7`;
- current `OptionSettingDialog*` at `+0xB8`;
- erased callback storage beginning at `+0xC0`; and
- a mode byte at `+0x100`.

Teardown walks all ten cached slots, retires non-null panels, then destroys
the callback and scene proxy. `0x93D5E0` forwards a frame call to slot 2 of the
current panel. `0x93C8B0` withholds selector input when current-panel slot 10
reports a blocking state or a job/state rooted at panel `+0x10` is not ready.
Top refresh `0x968BB0` passes a temporary manager value to top slot 8; that
presentation value remains unresolved.

## Erased panel factory

The RTTI inventory contains this exact MSVC callable family:

```text
std::_Func_base<
    CS::OptionSettingDialog*,
    CS::SceneObjProxy const&,
    CS::MENU_OPTION_DATA const&,
    bool>
```

The base vtable is `0x2AC13D8`; the plain-function-pointer wrapper is
`0x2AC1628`. **Confirmed:** wrapper slot 2 at `0x809D30` loads its raw function
pointer from `+0x08` and invokes it as:

```text
OptionSettingDialog*(SceneObjProxy const&, MENU_OPTION_DATA const&, bool)
```

The remaining entries manage the erased callable: `0x809030` copies it,
`0x8097D0` destroys it, and `0x80A060` exposes the stored target.

**Inferred:** this is the panel-construction boundary carried by or associated
with a `MenuOptionCategory`. The exact record field and activation call site
are not yet proven. Generic Option constructor `0x94D710` also takes more
native arguments than this factory contract, so calling it directly is not a
substitute for reconstructing the category-specific wrapper.

## Confirmed ordinary-page materializers

The shared category dispatcher at `0x93D730` maps category `1` through wrapper
`0x93CB70`, which supplies two-argument materializer `0x959320` to common panel
constructor `0x95FD30`. Bounded live observation confirmed the same model for
the other ordinary destinations:

| Native category | Destination | Materializer RVA |
|---:|---|---:|
| `0` | Game Options | `0x959DF0` |
| `1` | Camera Options | `0x959320` |
| `2` | Display | `0x95D540` |
| `3` | Sound | `0x959090` |
| `5` | Network | `0x95AD00` |
| `7` | Keyboard/Mouse Settings | `0x95CB30` |
| `8` | Graphics | `0x95C050` |

Each callback receives a live `OptionSettingDialog*` in `RCX` and
`MENU_OPTION_DATA*` in `RDX`. Production detours the concrete target, calls
vanilla once, validates the ordinary vtable and capacity/count fields, then
adds provider rows. The sparse native categories are explicitly translated
from the contiguous public enum.

This evidence is sufficient to append rows to an existing panel. It does not
complete the `MenuOptionCategory` record layout, justify replacing panels, or
prove safe custom-tab ownership. The controlled live evidence is documented
in [Settings pages and pagination](../case-studies/settings-pages-and-pagination.md).

## What remains before custom tabs

A production custom-tab system still needs:

1. a complete `MenuOptionCategory` builder and paired destruction path;
2. exact title, help, icon, and selection-field encoding;
3. mapping from each record to `TabList`, `BackTabList`, and `WindowList`;
4. L1/R1, mouse, wraparound, focus, and transition behavior for added entries;
5. a policy for native capacity ten versus stock visual capacity nine;
6. virtualization or scrolling if the public contract promises arbitrary
   counts;
7. a panel factory that participates safely in composite caching, frame
   dispatch, Back, and teardown; and
8. coexistence rules for mods that detour the constructor, category list, or
   factory chain.

Unchecked fixed-vector writes and fabricated `OptionSettingDialog` objects
are rejected approaches.

## Reproduce and close the remaining gaps

Import the exact build using the [Ghidra workflow](../tools/ghidra-workflow.md).
Useful bounded seeds are:

```text
0x807430  top allocation/factory
0x9672C0  top construction
0x967B20  top teardown
0x9680B0  top frame override
0x9680D0  nested tab/panel dispatcher
0x968CF0  fixed category-vector append
0x93C090  composite manager construction
0x93C1A0  composite manager teardown
0x93C8B0  tab-input readiness gate
0x93D5E0  active-panel frame forwarding
0x94D710  Option-setting construction
0x94E790  Option-setting teardown
0x968E30  Pad-setting construction
0x968FF0  Pad-setting teardown
0x809D30  erased panel-factory invoke
```

For a bounded live continuation:

1. validate the top vtable and record count, selected index, current panel,
   vtable, live row count, and capacity once per Configuration open;
2. record only one transition per tab and compare controller, keyboard, and
   mouse routes;
3. observe `0x968CF0` only during `0x9672C0`, logging a small reviewed field
   set from each `0x88`-byte record;
4. observe `0x809D30` once per native category, including raw target,
   arguments, result pointer, and result vtable;
5. pair construction with composite teardown to distinguish owned and
   borrowed values; and
6. make no mutation probe until both record construction and retirement are
   understood.
