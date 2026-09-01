# Game Options native class map

This note maps the native class family behind Elden Ring's Configuration
screen. Its immediate purpose is to keep future Audio, Graphics, and custom-tab
work on the correct object boundary. It is not a public ABI and none of the
addresses below should be called without the normal build validation,
ownership, and lifetime work.

## Evidence boundary

The static results in this note are for Elden Ring `2.7.0.0`:

- PE timestamp: `0x69E9C9B9`
- image size: `0x5E09600`
- SHA-256:
  `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`

The labels use the analysis vocabulary from this directory:

- **Confirmed** means recovered from exact-build RTTI, a vtable, or a bounded
  disassembly/decompilation with the relevant data flow visible.
- **Strong inference** means several independent observations agree, but a
  decisive constructor/callsite or runtime observation is still missing.
- **Unknown** marks a boundary that must not yet become a production
  assumption.

The Ghidra pseudocode is evidence, not original FromSoftware source. This note
retains only offsets, relationships, and short instruction-level facts; the
generated query reports remain in ignored `research-work/`.

## Executive result

**Confirmed:** the Configuration screen is split into two native layers. The
outer `CS::OptionSettingTopDialog` owns the tab selector, a fixed collection of
category records, and a panel manager. The manager treats its current settings
panel through the separate `CS::OptionSettingDialog` selection/editor family.

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

This immediately rejects two tempting designs:

1. `OptionSettingTopDialog` is **not** an `OptionSettingDialog`, so row
   constructors cannot be generalized to the tab host merely because both are
   Configuration UI objects.
2. The nested `_SettingTabControl` is **not** a `MenuWindow`; it is a
   `SceneObjModifier` registered into the top window's component stack.

## Exact recovered RTTI

### Class relationships

**Confirmed:** the complete-object RTTI gives the following primary-base
chains. `MenuJobRunnable` at displacement `0x10` and `ComponentStack` at
displacement `0x50` are the additional `MenuWindow` subobjects inherited by
every class on a `MenuWindow` branch.

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

| Recovered class | Type descriptor | Complete Object Locator | Hierarchy descriptor | Primary vtable | Slots |
|---|---:|---:|---:|---:|---:|
| `CS::GenericListSelectDialog` | `0x3C97590` | `0x32F0E08` | `0x32F0E30` | `0x2AA2148` | 18 |
| `CS::PropertyEditDialog` | `0x3CC79B8` | `0x33181F0` | `0x3318218` | `0x2B04E08` | 21 |
| `CS::OptionSettingDialog` | `0x3CD0C10` | `0x331F8A8` | `0x331F8D0` | `0x2B14888` | 22 |
| `CS::PadSettingDialog` | `0x3CD43D0` | `0x33238F0` | `0x3323918` | `0x2B16DD8` | 22 |
| `CS::OptionSettingTopDialog` | `0x3CD4068` | `0x33234A0` | `0x33234C8` | `0x2B16B48` | 13 |
| `CS::OptionSettingTopDialog::_SettingTabControl` | `0x3CD40A0` | `0x3323540` | `0x3323568` | `0x2B16AB8` | 3 |

### Exact primary-slot comparison

The table is intentionally mechanical. A blank means that the recovered table
ends before that slot. Semantic names are only attached where a separate
analysis established them.

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
| 13 | `0x77FA10` | `0x77FA10` | `0x9580C0` | `0x9580C0` | |
| 14 | `0x77F930` | `0x77F930` | `0x77F930` | `0x77F930` | |
| 15 | `0x77FA40` | `0x77FA40` | `0x77FA40` | `0x77FA40` | |
| 16 | `0x77F970` | `0x77F970` | `0x77F970` | `0x77F970` | |
| 17 | `0x77F980` | `0x77F980` | `0x77F980` | `0x77F980` | |
| 18 | | `0x920E80` | `0x920E80` | `0x920E80` | |
| 19 | | `0x921010` | `0x921010` | `0x921010` | |
| 20 | | `0x9273C0` | `0x966240` | `0x966240` | |
| 21 | | | `0x959080` | `0x969250` | |

**Confirmed interpretation:**

- `0x9217C0` is a direct thunk to the common `MenuWindow` frame dispatcher
  `0x7463C0`.
- slot 11 is the selected-controller/page-frame pass in the editor family;
  the Option and Pad classes share `0x958FF0`.
- slot 12 is the common native Back path at `0x747CD0`.
- Option and Pad differ only at slots 1 and 21. Pad is therefore a narrow
  specialization of the same editor machinery, not a separate tab host.
- the Option-top slot-2 implementation `0x9680B0` calls the common frame
  dispatcher and then `0x968BB0` for top-specific presentation refresh.
- Option-top slot 7 (`0x968170`) is an empty return. Slot 10 (`0x967FF0`)
  returns whether the 64-bit field at `this + 0x98` exceeds one. Their higher
  semantic names remain **unknown**.
- Option-top slot 3 (`0x967FE0`) writes value `0x0C` through its second
  argument. The meaning of that value remains **unknown**.

The nested tab controller has this exact three-entry table:

| Slot | Target | Established behavior |
|---:|---:|---|
| 0 | `0x735100` | shared reference-count/object entry; exact semantic name unknown |
| 1 | `0x967F60` | destruction wrapper, strongly inferred from the standard wrapper pattern |
| 2 | `0x9680D0` | confirmed per-frame tab/panel input dispatcher |

## Construction and teardown evidence

This section deliberately assigns construction roles only where the function
does more than happen to sit near a vtable.

### Option-setting top

**Confirmed:** `0x807430` is a top-dialog allocation/factory path. It allocates
exactly `0x18A0` bytes with alignment eight, obtains a scene-object value
through `0x7460D0`, and passes the allocation to `0x9672C0`. It returns the
constructed pointer and destroys the temporary Scaleform value on the same
path.

**Confirmed:** `0x9672C0` is the corresponding complete construction path:

1. It calls the common `MenuWindow` constructor path `0x7427B0`.
2. It installs `OptionSettingTopDialog::vftable`.
3. It resolves `TabList` and constructs the selector at `this + 0x0A38`.
4. It constructs
   `BasicViewItemList<CS::MenuOptionCategory, 10>` at `this + 0x1200`.
5. It constructs `CompositeOptionSettingDialog` at `this + 0x1768`.
6. It constructs `_SettingTabControl` at `this + 0x1870`, storing pointers to
   the selector and composite manager.
7. It registers that controller with the inherited `ComponentStack` through
   `0x734D40`.
8. It resolves `TabList/RightArrow` and `BackTabList`, then binds the latter
   with the format `Item_0_%d`.

The only other functions that write the exact top-dialog vtable are
`0x967B20` and the construction path. `0x967B20` retires the owned tab list,
composite manager, selector, controller, and base window, and is called by
slot-1 wrapper `0x967F20`. This is confirmed teardown behavior, not a name
assigned from address proximity.

### Generic option panel and Pad specialization

**Confirmed:** `0x94D710` calls the Property-edit base construction path
`0x91CC20`, installs `OptionSettingDialog::vftable`, initializes the additional
option fields, and returns `this`. `0x94E790` performs the reverse cleanup and
then enters the Property-edit teardown path `0x91E010`.

**Confirmed:** `0x968E30` calls `0x94D710`, installs
`PadSettingDialog::vftable`, and initializes a Pad-specific object at
`this + 0x1DA8`. Its path resolution includes platform-specific names such as
`XboxOne`, `Scarlette`, and `Win64`. `0x968FF0` retires that extra object and
then calls the Option-setting teardown path.

The following vtable writers remain useful leads, but this investigation did
not assign their complete signatures:

| Vtable | Referencing functions | Safe conclusion |
|---:|---|---|
| Generic list `0x2AA2148` | `0x77F750`, `0x78D2F0` | construction/teardown pair must be separated by bounded context before use |
| Property edit `0x2B04E08` | `0x91CC20`, `0x91E010`, `0x929EA0` | `0x91CC20` and `0x91E010` are reached as Option base construction/teardown; the third role is unknown |
| Option setting `0x2B14888` | `0x94D710`, `0x94E790` | confirmed construction and teardown paths above |
| Pad setting `0x2B16DD8` | `0x968E30`, `0x968FF0` | confirmed construction and teardown paths above |

## Category storage and the native tab limit

**Confirmed:** the top dialog uses an inline Dantelion fixed vector with a
compile-time capacity of ten. `0x968CF0` is the append helper used only by the
top construction path in this bounded graph. It:

- reads and increments the count at `top + 0x1760`;
- enforces a maximum of ten;
- uses an element stride of `0x88` bytes; and
- copy-constructs each record through `0x967230`.

Construction writes the list's base tables in order at `top + 0x1200`:

```text
0x2AA1618  CS::MenuViewItemListBase
0x2B16AD8  CS::MenuViewItemList<CS::MenuOptionCategory>
0x2B16B10  CS::BasicViewItemList<CS::MenuOptionCategory, 10>
```

The final list's slot-1 target `0x968DA0` returns the count at list offset
`+0x560`, which is top offset `+0x1760`.

The construction path always creates two records, then adds several records
behind platform/feature predicates, and finally adds another record when its
incoming mode flag is set. The exact category represented by each builder is
still **unknown**; assigning Audio, Graphics, Controller, and so on from call
order alone would be unsafe.

The GFX side has a different proven bound: stock `TabList` and `BackTabList`
each contain nine statically placed item instances, `Item_0_0` through
`Item_0_8`. Therefore:

- ten is the native category-storage capacity;
- nine is the visible stock-movie placement count; and
- neither number means "arbitrarily many custom tabs."

Supporting an unbounded public custom-tab API will require either a recycled
or scrolling tab presentation, or an ERNativeUI-owned tab layer. Appending to
the fixed native vector cannot provide that contract. Even a tenth native
record requires a compatible tenth GFX placement before it can be considered
presentable.

## Where tab selection actually lives

**Confirmed:** `_SettingTabControl::slot2` at `0x9680D0` is the clearest
native tab-dispatch boundary found so far.

On each call it:

1. reads the selected index from the object referenced at `this + 0x10`;
2. asks the composite panel manager at `this + 0x18` whether tab input is
   currently allowed;
3. calls selector slot 2 with the normal input byte, or a zero byte when the
   active panel blocks tab input;
4. reads the selected index again; and
5. calls the active panel's frame method with a zero input byte when the index
   changed, otherwise with the original input byte.

This explains the transition discipline: a shoulder-button or mouse tab
change cannot also activate a control on the newly selected panel in the same
frame.

**Strong inference:** the object at top offset `0x0A38` is the authoritative
tab selection model/controller. It is constructed from the resolved `TabList`
proxy, its getter `0x73AC70` supplies both before/after indices, and its virtual
slot 2 mutates the selection. The exact recovered class name and full layout
are still unknown.

## Composite panel ownership and dispatch

`CS::CompositeOptionSettingDialog` is a small but important middle layer. Its
recovered RTTI is:

| Type descriptor | Complete Object Locator | Vtable | Recovered slots |
|---:|---:|---:|---:|
| `0x3CCDDE0` | `0x331E6C8` | `0x2B0C950` | one (`0x93C7C0`) |

**Confirmed layout facts from constructor `0x93C090` and teardown
`0x93C1A0`:**

- an embedded scene-object proxy begins at `+0x08`;
- ten owned/cached panel pointers occupy `+0x68` through `+0xB7`;
- the current `OptionSettingDialog*` is at `+0xB8`;
- an erased callback is cloned into the storage beginning at `+0xC0`; and
- a mode byte is stored at `+0x100`.

Teardown walks exactly ten cached slots and retires every non-null panel before
destroying the callback and scene-object proxy. This ownership is a major
constraint for custom categories: a panel cannot be attached without also
matching the manager's caching and retirement rules.

**Confirmed dispatch facts:**

- `0x93D5E0` forwards the frame call to slot 2 of the panel at `+0xB8`.
- `0x93C8B0` withholds tab-selector input when current-panel slot 10 reports a
  blocking state, or when a job/state check rooted at `panel + 0x10` is not
  ready.
- top-specific refresh `0x968BB0` asks the composite manager for a temporary
  value and passes it to top-dialog slot 8. The exact presentation meaning of
  that value is still unknown.

## The erased option-panel factory

The RTTI inventory contains this exact MSVC callable family:

```text
std::_Func_base<
    CS::OptionSettingDialog*,
    CS::SceneObjProxy const&,
    CS::MENU_OPTION_DATA const&,
    bool>
```

Its base vtable is `0x2AC13D8`; the concrete plain-function-pointer wrapper is
at `0x2AC1628`.

**Confirmed:** wrapper slot 2, `0x809D30`, loads the raw function pointer from
wrapper offset `+0x08` and invokes it as:

```text
OptionSettingDialog*(SceneObjProxy const&, MENU_OPTION_DATA const&, bool)
```

The other five vtable entries are callable-wrapper management functions, not
five additional panel factories. In particular, `0x809030` copies a wrapper,
`0x8097D0` destroys it, and `0x80A060` exposes its stored target address.

**Strong inference:** this callable is the panel-construction boundary carried
by, or immediately associated with, a `MenuOptionCategory` record. Resource
builder `0x808860`, already tied to `02_042_PC_GraphicSetting`, is among the
functions that use and retire this wrapper family. The exact record field
holding the callable and the activation callsite have not yet been proven, so
production must not synthesize a category from this signature alone.

Also note that the generic `0x94D710` Option constructor takes more native
arguments than the three-argument factory contract. A category-specific raw
factory would have to supply those additional constants and resources.
Calling `0x94D710` directly is therefore not a substitute for recovering the
proper factory.

## What is common and what is panel-specific

| Concern | Common native layer | Panel-specific evidence |
|---|---|---|
| Tab selection | top dialog selector plus `_SettingTabControl` | category availability predicates and record data |
| Category capacity | fixed vector of ten `0x88`-byte records | stock build chooses a subset by platform/mode |
| Active-panel ownership | `CompositeOptionSettingDialog` cache/current pointer | raw factory and concrete returned vtable |
| Row navigation | `MenuWindow`/Generic-list/Property-edit frame machinery | concrete panel handlers and controller collections |
| Back | common slot 12 `0x747CD0` | Option/Pad share the slot-13 Back-action builder `0x9580C0` |
| Option page frame | Option and Pad share slot 11 `0x958FF0` | concrete row materialization functions differ by panel |
| Extra behavior | Option-setting base fields and callbacks | Pad adds the object at `+0x1DA8`; Graphics uses an additional on-demand GFX resource |

The resident `02_040_OptionSetting` movie contains named panels such as
`AudioSetting`, `ControllSetting`, `PCGraphic`, and `PadSetting`, while
`02_042_PC_GraphicSetting` is an additional on-demand Graphics resource. Those
resource names are presentation anchors, not proof of a one-to-one native
category order. The contiguous executable string table containing names such
as `LanguageSetting`, `CameraSetting`, `AudioSetting`, and `PCGraphic` is also
not enough by itself to establish record semantics or a safe factory.

## Required work before other native tabs

Adding rows to Audio, Graphics, or another existing tab requires all of the
following evidence:

1. Map each `MenuOptionCategory` builder to its visible tab and window/movie
   resource.
2. Recover the category record's `0x88`-byte field layout, including owned
   strings/message IDs, icon/presentation data, the erased panel factory, and
   every destructor obligation.
3. Trace category activation through the composite manager to the concrete
   panel pointer at `+0xB8`.
4. Identify each target panel's row-materialization handler, controller
   collection, visual capacity, and frame target. The Controller hook cannot
   simply be reused for Audio or Graphics.
5. Confirm whether the target panel is resident in `02_040`, created from an
   on-demand movie such as `02_042`, or composed from both.
6. Capture and compare real panel vtables at runtime so a stale page, a
   subpage, or another mod's replacement cannot be mistaken for the target.
7. Preserve composite ownership, input gating, and Back behavior during panel
   replacement and teardown.

## Required work before custom tabs

A custom-tab API additionally needs:

1. A proven builder for a complete `MenuOptionCategory` record and a matching
   destruction path.
2. The exact title/help/icon encoding and the mapping from a record to
   `TabList`, `BackTabList`, and `WindowList` instances.
3. L1/R1, mouse, wraparound, focus, transition, and selected-index behavior
   for added entries.
4. A policy for the native-ten versus visible-nine bounds.
5. An ERNativeUI-owned virtualization/scrolling design if the public contract
   promises any number of tabs.
6. A panel factory whose returned object participates safely in
   `CompositeOptionSettingDialog` caching, frame dispatch, Back, and teardown.
7. Compatibility rules for other mods that patch the top constructor,
   category list, or panel factory chain.

Until those are known, a custom tab should not be implemented as an unchecked
write to the fixed vector or as a fabricated `OptionSettingDialog` object.

## Bounded runtime probes to close the gaps

The next probes should be observational and rate-limited:

1. On one Configuration open, validate the top vtable and log the category
   count at `+0x1760`, selected index from `+0x0A38`, and current panel vtable
   from composite `+0xB8`.
2. Log one transition per tab: old/new index, current panel pointer/vtable,
   and the active `WindowList` child name. This maps visible order without
   guessing from string proximity.
3. Observe `0x968CF0` only during `0x9672C0`, recording the destination index
   and a small set of candidate pointer/callable fields from each `0x88`-byte
   record. Do not dump whole records continuously.
4. Observe `0x809D30` once per native category, recording the raw target at
   wrapper `+0x08`, arguments, returned pointer, and returned vtable.
5. Pair every construction observation with composite teardown to establish
   which strings, callbacks, and panels are owned versus borrowed.
6. Repeat with controller, keyboard, and mouse tab changes to verify that all
   devices pass through the same nested controller and transition gate.

No mutation probe should run until the record factory and retirement path are
both understood.

## Reproduction

The exact RTTI inventory was produced with `RttiClasses` filters for the six
named classes in this note. Vtable references were then found with bounded
`Anchors` queries. `FunctionContext` and `Decompile` were limited to the top
construction/teardown pair, top and nested virtual overrides, composite
manager helpers, Option/Pad construction paths, the fixed-vector append
helper, and the erased-callable invoke slot.

Representative read-only commands are documented in
[`tools/research/README.md`](../../tools/research/README.md). The important
query seeds for this map are:

```text
0x807430  top allocation/factory path
0x9672C0  top complete construction
0x967B20  top teardown
0x9680B0  top frame override
0x9680D0  nested tab/panel dispatcher
0x968CF0  fixed category-vector append
0x93C090  composite manager construction
0x93C1A0  composite manager teardown
0x93C8B0  tab-input availability gate
0x93D5E0  active-panel frame forwarding
0x94D710  Option-setting construction
0x94E790  Option-setting teardown
0x968E30  Pad-setting construction
0x968FF0  Pad-setting teardown
0x809D30  erased option-panel factory invoke
```

Related presentation and input findings are in
[`SCALEFORM_GFX_MODEL.md`](SCALEFORM_GFX_MODEL.md),
[`UI_EVENT_DISPATCH.md`](UI_EVENT_DISPATCH.md), and
[`NATIVE_UI_CLASS_HIERARCHY.md`](NATIVE_UI_CLASS_HIERARCHY.md).
