# Reverse-engineering Elden Ring's Configuration tab system

Status: **production implementation reference for the complete custom-tab
path admitted on the exact Elden Ring 2.7.1.0 Windows executable identified
below.** Each result below carries its own evidence label; this status line is
not a substitute for provenance.

This document is an implementation and evidence reference. It is not a client
API tutorial and it is not a development diary. Native class names come from
RTTI where available; all other names are analytical. RVAs, offsets, category
values, selectors, vtables, and resource identifiers are executable-specific
evidence, not ERNativeUI ABI.

Client-mod authors should use the public
[Custom Configuration tabs guide](../../guides/custom-tabs.md) instead.

## Research question

> Can one process-wide host project an extensible logical catalogue onto
> Elden Ring's native Configuration tab strip, retain native controller and
> pointer behavior, attach independently owned RGBA8 icons, and materialize
> independent row pages without fabricating native panels or requiring a
> modified GFX file?

The answer is **yes** for the admitted build. The recovered boundary includes:

- the Configuration top dialog, selector, category records, and composite
  panel cache;
- a logical viewport over eight validated stock cells in both native
  presentation layers;
- L1/R1, held repeat, wrap, direct pointer selection, and stationary edge
  dwell;
- immutable 32x32 RGBA8 images wrapped as native Scaleform resources;
- client rows materialized through a reconstructed category-8 Graphics
  carrier;
- native pagination for pages larger than the live row capacity;
- Configuration close/reopen reconciliation; and
- a reported 50-custom-tab, 20-row-per-tab live stress fixture.

This evidence does not make an unrecognized executable safe. It also does not
establish unlisted Scaleform services, unresolved record fields, or arbitrary
GFX replacements as compatible.

## Contents

- [Research question](#research-question)
- [Evidence labels and provenance](#evidence-labels-and-provenance)
- [Recovered system](#recovered-system)
- [Native layouts](#native-layouts)
- [Exact-build address inventory](#exact-build-address-inventory)
- [Recovered call contracts](#recovered-call-contracts)
- [Native call flow](#native-call-flow)
- [Experiment ledger](#experiment-ledger)
- [Controlled probe procedures](#controlled-probe-procedures)
- [Logical viewport and input](#logical-viewport)
- [Tab presentation and RGBA8 image path](#tab-presentation-and-rgba8-image-path)
- [Reusable Graphics carrier](#reusable-graphics-carrier)
- [Movie epochs and failure policy](#movie-epochs-and-stock-restoration)
- [Recorded results and provenance limits](#recorded-results)
- [Rejected hypotheses and unknowns](#rejected-hypotheses)
- [Revalidation and reproduction](#revalidation-after-a-game-update)
- [Production source and test map](#production-source-and-test-map)

## Evidence labels and provenance

| Label | Meaning in this document |
|---|---|
| **Confirmed static** | Exact-build instructions, RTTI, vtables, field accesses, or call flow establish the claim. |
| **Confirmed live** | A bounded run on the named executable established the relationship or behavior. |
| **Reported live** | The tester reported the bounded run as successful, but the published evidence is not sufficient for an independent replay. |
| **Confirmed production** | Static and live evidence were promoted behind exact-build and runtime guards. |
| **Inferred** | Independent observations agree, but the complete native contract is not recovered. |
| **Rejected** | A controlled probe falsified the proposed model. |

### Executable identities

| Property | Elden Ring 2.7.0.0 | Elden Ring 2.7.1.0 |
|---|---:|---:|
| File size | `87024720` bytes | `87042128` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` | `1A3547101327F65D0C76DA2F9190AC0AA66871EA42BAE2AECC61E11A8B597891` |
| PE timestamp | `0x69E9C9B9` | `0x6A96B418` |
| PE image size | `0x5E09600` | `0x5E0DA00` |
| Complete custom-image and reusable-page path admitted | No | Yes |

Every address below is an RVA relative to the image base. ASLR runtime
addresses are intentionally omitted.

The earlier 2.7.0.0 work established the tab host, most selector and
presentation seams, and the RawImage layout. The complete native image
resource, AS3 Shape, and reusable Graphics-page path is admitted only on
2.7.1.0.

### Evidence scope

- Static claims come from the exact executable identities, instruction
  windows, object layouts, and derivation chains recorded in this document.
- Live claims come from bounded tests whose intervention, control, expected
  failure conditions, and observed result are summarized below.
- Pure tests establish the host's algorithms and failure policy. They do not,
  by themselves, establish Elden Ring's native behavior.
- The vanilla `02_040_optionsetting.gfx` reference used during analysis is
  44,016 bytes with SHA-256
  `2B09D4E64EAB964F8D4045D25960F4C6CE1806D6596C75A21F28F334D0CD10B3`.
  A modified 45,129-byte test movie has SHA-256
  `5085358397E17271654EDA15A4D0299C57A06772A2937C3FF074197D820FCEFA`.
  Results involving the specialized ninth cell are valid only for the
  explicitly identified movie; the production path therefore excludes that
  cell.

## Recovered system

### Ownership and routing graph

```text
CS::OptionSettingTopDialog                         vtable 0x2B16B48
|
|-- +0x0120  movie context
|-- +0x0A38  native tab selector
|   `-- +0x00D4  selected physical index
|
|-- +0x1200  BasicViewItemList<MenuOptionCategory, 10>
|   |-- +0x1208  inline 0x88-byte record storage
|   `-- +0x1760  record count
|
|-- +0x1768  CS::CompositeOptionSettingDialog      vtable 0x2B0C950
|   |-- +0x0008  embedded SceneObjProxy
|   |-- +0x0068  category-indexed panel cache [10]
|   |-- +0x00B8  current OptionSettingDialog*
|   `-- +0x00C0  erased callback/factory storage
|
`-- +0x1870  _SettingTabControl                    vtable 0x2B16AB8
    |-- +0x0010  selector pointer
    `-- +0x0018  composite pointer

02_040_OptionSetting movie
|-- TabList/Item_0_0 ... Item_0_8
|   `-- Item_0_N/Cursor/ItemIcon        selected/front icon host
`-- BackTabList/Item_0_0 ... Item_0_8
    `-- Item_0_N/ItemIcon               unselected/back icon host
```

The native category list and visual strip have different limits:

- the fixed vector accepts ten `MenuOptionCategory` records;
- the stock movie authors nine physical cells, `Item_0_0` through
  `Item_0_8`; and
- production uses cells `0..7`, the eight homogeneous hosts exercised by the
  stock category sequence.

A third-party ninth cell retained slot-authored artwork after descriptor
replacement. It is not evidence that all nine cells are interchangeable, and
production does not write it.

### Stock category map

The live stock order is:

```text
0, 1, 2, 3, 5, 6, 7, 8
```

| Physical order | Native title | Category/cache slot | Ordinary row materializer | Recovered role |
|---:|---|---:|---:|---|
| 0 | Game Options | `0` | `0x959DF0` | Ordinary OptionSetting page. |
| 1 | Camera Options | `1` | `0x959320` | Ordinary OptionSetting page. |
| 2 | Display | `2` | `0x95D540` | Ordinary OptionSetting page. |
| 3 | Sound | `3` | `0x959090` | Ordinary OptionSetting page. |
| 4 | Network | `5` | `0x95AD00` | Selector 7; synchronous parking target. It is not modified or retired by the host. |
| 5 | Controller Settings | `6` | specialized PadSetting path | Not an ordinary row-materializer destination. |
| 6 | Keyboard/Mouse Settings | `7` | `0x95CB30` | Ordinary page route; distinct from the specialized binding editor internals. |
| 7 | Graphics | `8` | `0x95C050` | Selector 11; validated rows-only reusable carrier. |

A public logical index is not a native category. The logical catalogue owns
stock and client entries; the viewport maps a contiguous catalogue slice onto
the eight physical records.

## Native layouts

Only fields with direct evidence are named. Gaps are deliberately unresolved.

### `CS::OptionSettingTopDialog`

The allocation path at `0x807430` requests `0x18A0` bytes aligned to eight and
calls constructor `0x9672C0`.

| Offset | Recovered value | Evidence |
|---:|---|---|
| `+0x0000` | primary vtable `0x2B16B48` | RTTI, constructor writes, live validation; **confirmed static/live**. |
| `+0x0120` | movie context used for path resolution | ResolvePath call sites; **confirmed production**. |
| `+0x0A38` | native selector object | constructor and nested-controller references; **confirmed static/live**. |
| `+0x1200` | fixed category-list object | constructor and list vtables; **confirmed static**. |
| `+0x1208` | first inline category record | append stride; **confirmed static/live**. |
| `+0x1760` | record count | append and list getter; **confirmed static/live**. |
| `+0x1768` | embedded composite manager | constructor/controller relation; **confirmed static/live**. |
| `+0x1870` | nested tab controller | three-entry vtable and component registration; **confirmed static**. |

Top teardown `0x967B20` retires the owned tab list, composite, selector,
controller, and base window.

### Selector

| Offset | Recovered value | Evidence |
|---:|---|---|
| `+0x00D4` | signed selected physical index | getter, input dispatch, and live selection probes; **confirmed static/live**. |

The complete selector layout is not recovered. Production uses the containing
top identity, exact selector address, selected-index offset, and validated
native setter rather than assigning semantics to nearby fields.

### `CS::MenuOptionCategory`

Append helper `0x968CF0` rejects an append when the existing count is already
ten and copy-constructs `0x88`-byte records through `0x967230`.

| Offset | Size | Recovered value | Evidence |
|---:|---:|---|---|
| `+0x00` | 8 | vtable `0x2AD6F20` | constructor/live record checks; **confirmed**. |
| `+0x08` | 8 | non-owning resolved display-title pointer | presentation-swap probe; **confirmed live**. |
| `+0x10` | `0x30` | owned fallback-string object | copied through helper `0x117910`; must not be raw-relocated. |
| `+0x40` | 4 | native category/cache index | duplicate-category probe and dispatch; **confirmed live**. |
| `+0x44..+0x47` | 4 | unresolved | no semantics assigned. |
| `+0x48` | `0x40` | presentation descriptor | native presenter and descriptor swap; **confirmed live**. |

Within the presentation payload, `+0x38` is the recovered resource-family
byte and `+0x3C` the resource ID. Production still calls the native descriptor
builder instead of fabricating unresolved fields.

### `CS::CompositeOptionSettingDialog`

| Offset | Size | Recovered value | Evidence |
|---:|---:|---|---|
| `+0x00` | 8 | vtable `0x2B0C950` | RTTI/constructor; **confirmed static**. |
| `+0x08` | `0x60` | embedded `SceneObjProxy` | constructor/teardown; **confirmed static**. |
| `+0x68` | `10 * 8` | category-indexed panel cache | switch/teardown and live identity checks; **confirmed static/live**. |
| `+0xB8` | 8 | current `OptionSettingDialog*` | switch/frame forwarding; **confirmed static/live**. |
| `+0xC0` | unresolved | erased callback storage | callable RTTI and construction; exact subfields unresolved. |
| `+0x100` | 1 | mode byte | **confirmed static**; complete semantics unresolved. |

Constructor `0x93C090` initializes these owners. Teardown `0x93C1A0` walks
all ten cache slots, retires non-null panels, destroys the erased callback,
and destroys the embedded proxy.

### Option page and `SceneObjProxy`

| Object | Offset/size | Recovered value | Evidence |
|---|---:|---|---|
| `OptionSettingDialog` | vtable `0x2B14888` | ordinary row page identity | RTTI, constructors, live checks. |
| `OptionSettingDialog` | `+0xB14` | visual row capacity | live capacity validation. |
| `OptionSettingDialog` | `+0x1200` | page `SceneObjProxy` | static and production owner checks. |
| `OptionSettingDialog` | `+0x1AF0` | native materialized row count | static/live row injection. |
| `SceneObjProxy` | vtable `0x2A97AF0` | complete proxy identity | constructor, RTTI, and guarded live checks. |
| `SceneObjProxy` | size `0x60` | owning wrapper value | construction and teardown. |
| `SceneObjProxy` | `+0x20` | wrapper flags | guarded GFx validation. |
| `SceneObjProxy` | `+0x28` | GFx object payload | getter `0x74C930` and live checks. |

The backend never retains only the proxy's internal payload as though it owned
the wrapper. Native construction and teardown retain the complete proxy.

### Raw image and resource layouts

```cpp
struct RawImageSize {                 // sizeof == 0x08
    std::uint32_t width;              // +0x00
    std::uint32_t height;             // +0x04
};

struct RawImagePlane {                // sizeof == 0x20
    std::uint32_t width;              // +0x00
    std::uint32_t height;             // +0x04
    std::uintptr_t pitch;             // +0x08
    std::uintptr_t data_size;         // +0x10
    std::byte* data;                  // +0x18
};

struct RawImageData {                 // sizeof == 0x40
    std::uint32_t format;             // +0x00
    std::uint32_t use;                // +0x04
    std::uint8_t flags;               // +0x08
    std::uint8_t level_count;         // +0x09
    std::uint16_t raw_plane_count;    // +0x0A
    std::uint32_t padding;            // +0x0C
    RawImagePlane* planes;            // +0x10
    void* palette;                    // +0x18
    RawImagePlane plane0;             // +0x20
};
```

`CS::ScaleformImageResource` is `0x98` bytes on the admitted 2.7.1.0 path.

| Offset | Recovered value |
|---:|---|
| `+0x00` | resource vtable `0x2BBAAF8` |
| `+0x08` | resource intrusive reference count |
| `+0x18` | exposed `Render::Image*` |
| `+0x20` | inline delegate vtable `0x2BBA9E8` |
| `+0x28` | delegate reference count |
| `+0x48` | delegated-image pointer, null in the validated direct-image form |
| `+0x60` | resource kind, `1` in the validated form |

The remainder stays unresolved.

## Exact-build address inventory

Unless a split RVA is shown, the boundary is shared by both identified
executables. Installation validates local instruction bytes and object
identities; matching only an RVA is insufficient.

### Configuration host, records, and routing

| Analytical boundary | RVA | Recovered contract |
|---|---:|---|
| top allocation/factory | `0x807430` | Allocate `0x18A0`, obtain scene value, call top constructor. |
| top constructor | `0x9672C0` | Build selector, fixed list, composite, controller, and tab-layer bindings. |
| top teardown | `0x967B20` | Retire the complete top-owned graph. |
| category copy constructor | `0x967230` | Copy one native category record. |
| category append | `0x968CF0` | Enforce capacity ten and append one `0x88` record. |
| nested tab/panel dispatcher | `0x9680D0` | Serialize selector change and newly selected panel input. |
| selection callback | `0x968510` | Receive one physical selected index. |
| BackTab callback | `0x968590` | Receive physical index selected by the back layer. |
| title refresh | `0x968C40` | Refresh title from the current mapped record. |
| composite constructor | `0x93C090` | Initialize proxy, cache, current pointer, callback, and mode. |
| composite teardown | `0x93C1A0` | Retire all populated category banks. |
| panel switch | `0x93C900` | Resolve, cache, activate, and expose one category panel. |
| switch state-copy seam | `0x93C93C` | Exact state copy may be suppressed for armed host transitions involving a custom logical endpoint, including internal parking and corrective transitions. |
| state-copy resume | `0x93C963` | Resume target for narrow suppression. |
| switch visibility decision | `0x93C989` | Validated active/inactive-panel branch. |
| tab-input readiness | `0x93C8B0` | Decide whether selector receives live input. |
| shared category dispatcher | `0x93D730` | Choose category-specific wrapper/factory. |
| composite frame | `0x93D5E0` | Current-page frame and atomic transition boundary. |
| generic Option constructor/teardown | `0x94D710` / `0x94E790` | Ordinary `OptionSettingDialog` lifecycle. |
| Pad constructor/teardown | `0x968E30` / `0x968FF0` | Specialized Controller Settings lifecycle. |
| SceneObjProxy getter | `0x74C930` | Obtain guarded GFx value from complete proxy. |

### Selector and input provenance

| Boundary | RVA | Registers/data at seam |
|---|---:|---|
| selector getter | `0x73AC70` | Return selected physical index. |
| selector select | `0x739B90` | Select physical cell through native selector. |
| GridControl frame begin | `0x73A196` | `RDI` selector; clear per-frame provenance. |
| pointer dispatch result | `0x73A1C7` | `RDI` selector, `R12D` entry index, `AL` change result. |
| horizontal-navigation commit | `0x73A358` | `RDI` selector; signed x/y deltas packed in `RBX`. |
| final selection dispatch | `0x73A394` | `R12D` entry index; `[RDI+0xD4]` final index. |
| pointer-cell sample | `0x73B880` | `RSI` selector, signed `EAX` hit or `-1`, floats at `RSP+0x20/+0x24`. |
| GridControl hit test | `0x737AE0` | Resolve converted point to physical cell. |
| get pointer position | `0x758940` | Produce point used by native hit test. |
| native pointer selection | `0x73CA60` | Apply timed dwell via ordinary pointer setter. |

At `0x73A358`, the low signed 32 bits of `RBX` are horizontal delta and the
high 32 bits vertical delta. An x delta of `-1` or `+1` with y zero
distinguishes L1/R1 navigation before all paths converge.

### Presentation and Scaleform bridge

| Boundary | 2.7.0.0 | 2.7.1.0 | Recovered role |
|---|---:|---:|---|
| resolve path | `0x74B140` | `0x74B140` | Resolve exact tab-cell/icon-parent objects. |
| value exists | `0x733FA0` | `0x733FA0` | Validate resolved wrapper. |
| present tab cell | `0x967DE0` | `0x967DE0` | Present mapped physical record. |
| build icon descriptor | `0x73E230` | `0x73E230` | Construct complete native descriptor. |
| texture lookup call site | `0x74CCDF` | `0x74CCDF` | Correlate resource request. |
| image bind call site | `0x74CDC7` | `0x74CDC7` | Correlate binder ownership/target. |
| texture lookup target | `0xD65B90` | `0xD65C00` | Return one AddRef'd image resource. |
| image bind target | `0xD83380` | `0xD833F0` | Bind resource to Shape. |
| GFx value constructor | not admitted | `0xD814A0` | Initialize native GFx wrapper. |
| GFx GetMember | not admitted | `0xD816F0` | Resolve one member. |
| GFx SetPosition | not admitted | `0xD85BD0` | Position dynamic icon Shape. |
| GFx SetVisible | not admitted | `0xD86280` | Toggle dynamic/authored visibility. |
| GFx value destructor | `0xD81590` | `0xD81600` | Release validated wrapper. |

### Immutable image/resource path

| Boundary | 2.7.0.0 | 2.7.1.0 | Recovered role |
|---|---:|---:|---|
| RawImage create | `0x114A7B0` | `0x114A820` | Allocate system-memory image. |
| RawImage get data | `0x114A9D0` | `0x114AA40` | Expose copied pixel-plane view. |
| ImageData clear | `0x1146A60` | `0x1146AD0` | Clear copied view only. |
| RawImage release | `0x112D5D0` | `0x112D640` | Intrusive release. |
| RawImage vtable | `0x2CC27E0` | `0x2CC27E0` | Exact object identity. |
| RawImage GetType | not admitted | `0x114B9C0` | Confirm image type. |
| image-resource constructor | not admitted | `0xD61C90` | Wrap RawImage under unique name. |
| image-resource AddRef / Release | not admitted | `0x113D9E0` / `0x113DA00` | Resource ownership. |
| resource deleting destructor | not admitted | `0xD62340` | Exact final destruction. |
| resource allocation call | not admitted | `0xD65DA5` | Native Scaleform allocation seam. |
| resource/delegate vtables | not admitted | `0x2BBAAF8` / `0x2BBA9E8` | Exact identities. |
| Scaleform heap slot | not admitted | `0x45972D0` | Heap used for `0x98` resource. |

### AS3 object bridge and Shape identities, 2.7.1.0

| Boundary | RVA |
|---|---:|
| Value object-interface vtable | `0x2CBF988` |
| GetMember / SetMember | `0x10DDD10` / `0x10DE0D0` |
| Invoke | `0x10DE510` |
| GetMovie | `0x10E26D0` |
| Movie vtable / CreateObject | `0x2CBE5C0` / `0x10D4460` |
| primary authored Shape vtable | `0x2CBD4E0` |
| alternate authored icon vtable | `0x2CC6268` |
| dynamic Shape vtable | `0x2CBD158` |

For the recovered GFx wrapper, the object interface is at `+0x18`, flags at
`+0x20`, and object payload at `+0x28`. The predicates are deliberately
different:

| Wrapper kind | Required predicate | Additional dereference |
|---|---|---|
| ordinary AS3 object | `(flags & 0x8F) == 0x08` | none |
| display object | `(flags & 0x8F) == 0x0A` | native display object at `object_payload+0x88`, then its vtable |

Using the ordinary-object predicate before dereferencing `payload+0x88` is
invalid. Production also admits only the exact interface method slots and the
authored/dynamic Shape vtables listed above.

### Graphics carrier boundaries, 2.7.1.0

| Boundary | RVA | Recovered role |
|---|---:|---|
| Graphics wrapper | `0x93CEF0` | Build selector-11 category-8 panel. |
| secondary callback local | `0x93CF20` | Graphics-only callback descriptor. |
| primary row callback local | `0x93CF32` | Graphics materializer descriptor. |
| builder call | `0x93CF58` | Consume selector/callback descriptors. |
| selector immediate | `0x93CF84` | Exact selector value 11. |
| pre-builder hook | `0x93CF89` | Last validated substitution seam. |
| native Graphics materializer | `0x95C050` | Populate stock Graphics rows. |
| Graphics-only callback | `0x95A820` | Install Graphics apply/rollback state. |
| Network wrapper | `0x93CE10` | Build selector-7 parking page. |
| retirement manager | `0xE1C080` | Obtain native owner for old panel. |
| panel deactivate | `0x953520` | Exact category-8 deactivation target. |

At the pre-builder seam:

- `[RSP+0x20]` must be selector `11`;
- `[RSP+0x28]` must equal `RSP+0x60`, and `[RSP+0x30]` must equal
  `RSP+0x50`;
- bytes `[RSP+0x38]` and `[RSP+0x40]` must be `1` and `0`;
- the secondary descriptor begins at `RSP+0x50`, metadata at `RSP+0x58`;
- the primary descriptor begins at `RSP+0x60`, metadata at `RSP+0x68`;
- the primary callback/metadata must be `image+0x95C050`/zero and the
  secondary callback/metadata must be `image+0x95A820`/zero;
- each descriptor occupies 16 bytes, but its last four padding bytes were not
  read or mutated; and
- every admitted route replaces the primary with the host thunk. The native
  Graphics route retains the secondary callback and the thunk delegates to
  `0x95C050(page, page+0x1B38)`. A custom route additionally zeroes the
  secondary callback plus its 32-bit metadata.

The seam returns normally through the native builder after these stack-local
changes; it does not redirect `RIP`.

The native materializer receives the page in `RCX` and `page+0x1B38` in `RDX`.

## Recovered call contracts

```cpp
using SelectionCallbackFn = void(__fastcall*)(
    void* callable, const std::int32_t* physical_index);
using CompositeFrameFn = void(__fastcall*)(
    void* composite, float frame_value, const std::uint8_t* input_enabled);
using SelectorSelectFn = bool(__fastcall*)(
    void* selector, std::uint32_t physical_index,
    std::uint8_t suppress_selection_callbacks);
struct PointerPosition { float x; float y; };
using GetPointerPositionFn = PointerPosition*(__fastcall*)(
    const std::uint8_t* input_gate, PointerPosition* output_position);
using GridHitTestFn = std::int32_t(__fastcall*)(
    void* selector, const PointerPosition* pointer_position);
using NativePointerSelectFn = std::int32_t(__fastcall*)(
    void* selector, std::uint32_t physical_index);
using SwitchPanelFn = void(__fastcall*)(
    void* composite, std::int32_t category);
using PresentTabFn = void(__fastcall*)(
    void* top_reference, void* scene_obj_proxy,
    std::uint32_t physical_index);
using MakeIconDescriptorFn = void*(__fastcall*)(
    void* descriptor, std::int32_t icon_id);
using TextureLookupFn = void**(__fastcall*)(
    void* repository, void** output_resource,
    const wchar_t* resource_name);
using ImageBindFn = bool(__fastcall*)(
    void* scaleform_value, void** resource, const float* rectangle);
using RawImageCreateFn = void*(__fastcall*)(
    std::uint32_t format, std::uint32_t mip_levels,
    const void* size, std::uint32_t use,
    void* heap, void* update_sync);
using RawImageGetDataFn = bool(__fastcall*)(
    void* image, void* image_data);
using ScaleformImageResourceCtorFn = void*(__fastcall*)(
    void* storage, const wchar_t* name,
    void* image, std::uint32_t kind);
using ValueInvokeFn = bool(__fastcall*)(
    void* object_interface, void* object_payload, void* result,
    const char* method, const void* arguments,
    std::size_t argument_count, bool is_display_object);
using ValueSetMemberFn = bool(__fastcall*)(
    void* object_interface, void* object_payload,
    const char* member, const void* value,
    bool is_display_object);
using MovieCreateObjectFn = void(__fastcall*)(
    void* movie_root, void* out_value, const char* class_name,
    const void* arguments, std::uint32_t argument_count);
```

These are recovered Windows x64 contracts, not public types.

## Native call flow

### Top construction and category population

```text
0x807430
    allocate 0x18A0-byte OptionSettingTopDialog
    obtain parent scene value
    -> 0x9672C0 top constructor
       construct selector at +0x0A38
       construct fixed list at +0x1200
       construct composite at +0x1768
       construct/register _SettingTabControl at +0x1870
       append stock MenuOptionCategory records via 0x968CF0
       resolve TabList, RightArrow, and BackTabList
```

The append helper calculates `storage + count * 0x88`, checks count against
ten, and invokes `0x967230`. That copy constructor:

1. installs the `MenuOptionCategory` vtable;
2. copies the borrowed title pointer at `+0x08`;
3. copy-constructs the owned fallback string at `+0x10`;
4. copies category at `+0x40`; and
5. copies four 16-byte blocks covering presentation `+0x48..+0x87`.

This is why a complete record cannot be moved with `memcpy`: the fallback
string is an owning object. The viewport edits only proven scalar/byte fields
in already constructed records and restores from equally live native records.

### Selection and page activation

```text
GridControl changes selector physical index
    -> 0x968510 selection callback
       retrieve MenuOptionCategory[selected]
       read record +0x40 native category
       -> 0x93C900 composite switch
          if cache[category] is null:
              -> 0x93D730 category factory dispatcher
              -> category wrapper/common panel constructor
              cache result at composite +0x68 + category*8
          copy permitted state from old panel
          activate selected panel
          set composite +0xB8 current panel
       -> 0x968C40 refresh top title from record +0x08
```

The nested controller at `0x9680D0` reads selection before and after framing
the selector. When selection changed, it frames the newly selected page with
zero input; otherwise it forwards the real input byte. This native ordering is
the reason the host commits a pending route at the composite boundary before
the new page receives actionable input.

### Back-layer cell presentation

```text
BackTabList Item_0_N callback @ 0x968590
    construct temporary SceneObjProxy
    -> 0x967DE0(top reference, proxy, physical index N)
       index top +0x1208 by N * 0x88
       consume that record's presentation descriptor
       present the cell
    destroy temporary proxy
```

The indexed presenter and selection callback both use the physical index, but
they consume different record fields. This static split is the basis for the
category and presentation mutation probes.

### Admission bytes and hook publication

Production does not accept an address merely because the module timestamp and
image size match. It checks local instruction contracts before preparing any
hook. Representative 2.7.1.0 admission prefixes are:

| Boundary | Required bytes at RVA |
|---|---|
| selection callback `0x968510` | `40 53 48 83 EC 20 8B 12 48 8B D9 48 8B 49 08 48` |
| composite frame `0x93D5E0` | `48 8B 89 B8 00 00 00 48 85 C9 74 07 48 8B 01 48` |
| selector setter `0x739B90` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48` |
| input frame begin `0x73A196` | `44 8B A7 D4 00 00 00 45 33 FF` |
| pointer result `0x73A1C7` | `84 C0 0F 85 C5 01 00 00 4C 8D B7 60 07 00 00` |
| horizontal commit `0x73A358` | `8B 97 D4 00 00 00 4C 8D 45 40 48 8B CF E8 A6 1B 00 00` |
| final dispatch `0x73A394` | `44 39 A7 D4 00 00 00` |
| pointer sample `0x73B880` | `83 F8 FF 74 59 8B D0 48 8B CE E8 D1 11 00 00 B0 01` |
| Graphics secondary local `0x93CF20` | `48 8D 05 F9 D8 01 00` |
| Graphics primary local `0x93CF32` | `48 8D 05 17 F1 01 00` |
| Graphics builder `0x93CF58` | `48 8D 8C 24 80 00 00 00 E8 CB 04 00 00` |
| Graphics selector/call block `0x93CF84` | `C6 44 24 20 0B 44 0F B6 CF 4C 8B C3 48 8B D0 48 8B CE E8 95 2D 02 00` |
| composite teardown `0x93C1A0` | `48 89 4C 24 08 55 56 57 41 56 41 57 48 83 EC 30` |

The full byte arrays, call-target validation, return-site checks, RawImage and
resource signatures, and Shape helper gates are in
[`src/native_tabs.cpp`](../../../src/native_tabs.cpp) near the hook preflight
and in [`src/hooks.cpp`](../../../src/hooks.cpp) near the category-8 carrier
preflight. Those checks are part of the recovered contract; this shorter table
is not an alternative validator.

Hooks are created disabled. After every target and trampoline is prepared, the
group is enabled in an exact order: state-copy seam; frame-begin, pointer,
horizontal, final-dispatch, and pointer-sample seams; category switch; image
bind; texture lookup; selection; then composite frame. Only after every enable
succeeds is the callback group armed. Before
first exposure a failed install can roll back normally. After any native entry
has been exposed, the runtime remains pinned and inert rather than destroying
trampolines or borrowed storage while a callback may be executing.

### Mid-hook execution contracts

SafetyHook mid-hooks enter the callback before the instruction at the named
RVA. On an ordinary return, SafetyHook's generated trampoline executes the
relocated native instruction(s) and resumes native control flow. The callback
must not replay those instructions, adjust `RSP`, or manufacture flags. Only
the two explicitly documented mutations below are permitted.

| RVA | Native instruction at seam | State consumed by callback | Permitted mutation and continuation |
|---:|---|---|---|
| `0x73A196` | `mov r12d,[rdi+0xD4]` | `RDI` must be the admitted selector. Clears host provenance and per-dispatch pointer sample state. | Observation-only. All GPRs, flags, and stack remain native; relocated instruction runs normally. |
| `0x73A1C7` | `test al,al` followed by native conditional branch | `RDI` selector, `AL` pointer-helper result, `R12D` entry selection, and current `[RDI+0xD4]`. Also runs the independent pointer poll. | Observation-only. In particular, do not replace `AL` or the branch flags; native `test` executes after callback return. |
| `0x73A358` | `mov edx,[rdi+0xD4]` | Signed x/y delta packed in `RBX`; only `x=-1/+1,y=0` establishes L1/R1 provenance. | Observation-only; controller provenance may cancel host dwell state, but native registers/flags/stack are unchanged. |
| `0x73A394` | `cmp [rdi+0xD4],r12d` | `RDI` selector and `R12D` frame-entry selection. | If a dwell deadline is due, call native setter `0x73CA60(RDI, edge_slot)` before this comparison. Do not patch context registers: the native comparison and ordinary change dispatch observe the setter's result. |
| `0x73B880` | `cmp eax,-1` | `RSI` selector; signed `EAX` hit; exact hit-test floats at `[RSP+0x20]` and `[RSP+0x24]`. | Normally observation-only. While an admitted stationary dwell is armed, or while a viewport transition is pending and a natural hit must not race it, replace only low 32 bits of `RAX` with `0xFFFFFFFF` while preserving upper `RAX`; native comparison then takes its existing no-hit branch. Preserve stack and all other registers. |
| `0x93C93C` | first `lea` in the old-to-new panel-state copy block | `RSI` must equal active `top+0x1768`; `RBX` is a non-null source; `RAX` is the non-null target/current panel; source and target differ, resolve to valid cached ordinary panels, and match the armed expectation's composite/source/target category. | A mismatch leaves execution untouched. On the exact expected host transition, change only `RIP` to `image+0x93C963`, skipping `[0x93C93C,0x93C963)`. This covers internal parking and corrective transitions involving a custom endpoint. It is the sole manual resume; do not replay the skipped copy. |

The byte window at `0x93C977` is separately validated through the visibility
loop containing the decision at `0x93C989`; the current implementation does
not install an independent mid-hook there. This distinction matters when
porting the implementation: a validated control-flow boundary is not
automatically a mutation seam.

The inline detour at `0x93C900` has a separate responsibility: while a
validated viewport transaction is pending, it suppresses the provisional
switch emitted by the ordinary selection callback so the composite-frame
transaction can publish the final route atomically. That callback suppression
is not the `0x93C93C` state-copy skip and must not be implemented by widening
the mid-hook predicate.

## Experiment ledger

This table is an index, not a substitute for the controls and procedures that
follow.

| Experiment | Question | Intervention and control | Recorded result | Status |
|---|---|---|---|---|
| native-list observation | What is appended, selected, and cached? | Observation-only append/dispatch/BackTab hooks; stock menu is control. | Categories `0,1,2,3,5,6,7,8`; native wrap and physical dispatch. | **Confirmed live** on 2.7.0/2.7.1. |
| duplicate category | Does category route content independently of presentation? | On exact nine-record fixture `0,1,2,3,5,6,7,8,4`, write record 7 category `8 -> 4`; presentation untouched. | Graphics-looking slot opened category-4 page while title/icon remained Graphics. | **Confirmed live**. |
| presentation swap | Can presentation move without route? | Swap record 7/8 borrowed title pointers and 64-byte descriptors; categories unchanged. | Title/icon and page routes crossed; stock descriptor moved, specialized ninth-cell art did not. | **Confirmed live**. |
| eight-cell viewport | Can many logical tabs use stock cells? | Project a logical slice over cells `0..7`, repaint both layers, and relocate the selector at the frame boundary. | Forward/reverse shifts, pointer jumps, and wrap worked. One-layer repaint caused deterministic overlap. | **Confirmed live**. |
| ninth-cell control | Is cell 8 homogeneous? | With the modified GFX control identified above, replace the descriptor and attempt an explicit transparent pass. | Slot-local artwork remained. | **Rejected** as a production cell. |
| input provenance | Can mouse and L1/R1 be separated before convergence? | Observe frame-begin, pointer-result, horizontal-commit, and final-dispatch seams. | Tap, hold, wrap, adjacent hover, and non-adjacent hover were classified without timing inference. | **Confirmed live** on 2.7.1. |
| native pointer-poll equivalence | Can stationary hover reuse native conversion and hit testing? | Poll once per active frame and compare every natural sample. | 1,530 frames; 331 natural matches; 1,199 fallback polls; zero mismatch, fault, or SEH. | **Confirmed live**. |
| pointer dwell | Can stationary edge hover advance while controller repeat stays native? | Suppress only repaint-induced duplicate hover; dispatch the native pointer setter after a wall-clock deadline; verify the same state machine with pure tests. | 250 ms initial delay; 200,180,... to 50 ms; motion reset; controller cancellation; no mouse endpoint wrap. | **Confirmed live/tested**. |
| vector Shape | Can a runtime display object replace authored icon art? | Create a Shape, draw a cross or ellipsis, and attach it to the validated icon parent. | Portable synthetic icons rendered in all eight validated cells. | **Confirmed live**. |
| BitmapData ownership | Can a movie-rooted BitmapData wrapper survive? | Create a transparent 2x2 object, store and read it through a movie property, then reacquire it next frame. | The same ActionScript payload survived. | **Confirmed only for wrapper/property ownership**. |
| BitmapData pixel mutation | Is mutable BitmapData a safe upload path? | Invoke exactly `setPixel32(0,0,0xA17BC3E5)` after identity proof. | The call reported success and the object survived, but the renderer became black. VM image context/provider were null. | **Rejected**. |
| immutable RawImage | Can system-memory pixels bypass missing DrawableImage services? | Create a 32x32 one-level RGBA image, validate its plane, wrap it in a native resource, and use the unchanged binder. | Custom pixels rendered without blackening. | **Confirmed live** on 2.7.1. |
| multiple image identities | Does each logical icon require separate native identity? | Publish visually distinct `+`, `X`, diagonal, and checker images with distinct pointers, names, IDs, and checksums. | Reused identity produced first-image aliasing or a white square; independent identities rendered independently. | **Confirmed/rejected comparison**. |
| alpha convention | Does the binder consume ordinary or premultiplied RGB? | Compare both byte conventions under equal alpha. | Premultiplied input blended correctly; the public API can stay ordinary RGBA because the host converts it privately. | **Confirmed live**. |
| independent native banks | Can two existing cache banks retain independent rows and state? | Use category 4 selector 6 and category 9 selector 14 with one row each; alternate and reopen. | Distinct panel/wrapper/payload identities and values; only a bounded two-bank proof. | **Confirmed intermediate result**. |
| Game Options carrier | Is category 0 reusable as a blank custom page? | Inject custom rows, wrap, and reopen. | Game Options became empty or inherited custom rows. | **Rejected**. |
| authored Brightness-like carrier | Does replacing rows replace the whole template? | Replace rows while retaining the panel and movie. | The instruction and black calibration art remained. | **Rejected**. |
| Graphics carrier | Can native Graphics and custom pages sequentially reuse category 8? | Same-frame Network parking, exact category-8 retirement, route publication, reconstruction, and verification. | Row-only custom pages, native Graphics restoration, Back/O, hold, wrap, dwell, and reopen passed. | **Confirmed live** on 2.7.1. |
| movie epoch | Can reopen avoid stale records when addresses are reused? | Match complete native-owned stock identity and restore all records before initialization; verify the pure epoch model separately. | Populated Game Options, pristine records, and the correct ellipsis state after reopen. | **Confirmed live/tested**. |
| final stress | Does the full path remain coherent under load? | 50 custom tabs, 20 rows each, distinct alpha icons, hold, dwell, wrap, pagination, and reopen. | The tester reported that the complete matrix passed. | **Reported live**. |

## Common live-probe rules

1. Run offline with Easy Anti-Cheat disabled.
2. Record the complete executable identity before loading a probe.
3. Enable only the intervention under test and record its exact build identity
   and configuration.
4. Load one ERNativeUI host. A second host invalidates hook-ownership evidence.
5. Remove loose third-party `02_040_optionsetting.gfx` files unless that
   modified ninth cell is the declared control.
6. Retain the probe's capture/time cap and call the original exactly once
   unless suppression is the explicit experiment.
7. Run the positive path twice and its control once. Normalize addresses to
   RVAs before comparison.
8. Exit normally and record game identity, intervention identity,
   configuration, diagnostics, and screenshots together.
9. A black frame, invisible UI, crash, wrong page, stale or overlapping icon,
   lost Back/O, unexpected alias, identity mismatch, nonzero SEH, or unbounded
   capture fails the run even if an expected final line appears.

### Public production validation

The admitted production path can be exercised with the API 1.3 stress client:

```powershell
cmake -S . -B build -A x64 -DERNATIVEUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target `
    ERNativeUIHost CustomTabsStress
```

Load `ERNativeUI.dll` before `CustomTabsStress.dll`. That example registers
exactly 50 tabs, 20 rows per tab, and 50 deterministic transparent RGBA8 icons.
It is a repeatable production integration test; it does not reproduce the
controlled mutations that established each native contract. Those experiments
are specified as procedures below so their preconditions, controls, and
failure criteria remain reviewable without depending on a particular
instrumentation codebase.

## Controlled probe procedures

### Native list and dispatch observation

**Question.** What constitutes the stock list, and which value reaches page
dispatch?

**Hooks.**

- category append at `0x968CF0`;
- `_SettingTabControl` frame dispatcher at `0x9680D0`; and
- BackTab callback at `0x968590`.

**Capture.** For one Configuration instance, record top pointer/vtable,
append ordinal, record address and category, final count/order, selector
address/index before and after dispatch, composite/current panel, and callback
physical index. Cap at one complete stock construction plus one transition per
tab.

**Control.** The observer performs no record, selector, Scaleform, page, or
input write. Unknown executable identity, modified entry bytes, wrong top
vtable, an attempted append when count is already ten, or unexpected record
stride prevents hook arming.

**Result.** Eight stock records were appended in order
`0,1,2,3,5,6,7,8`. Controller and pointer navigation supplied physical list
indices; the chosen record category then selected the composite cache bank.
R1 from Graphics wrapped to Game Options and L1 from Game Options wrapped to
Graphics.

### Category-route separation

**Question.** Is the record category also the title/icon identity?

**Precondition.** Exactly nine records in order
`0,1,2,3,5,6,7,8,4`, correct top and record vtables, and an exact executable.

**Mutation.** Change only record 7's `+0x40` category from `8` to `4`.
Do not change its title pointer or `0x40`-byte presentation payload.

**Result.** The original Graphics physical slot retained its Graphics title
and icon but displayed the category-4 page. The original category-4 slot still
displayed that page through its own presentation. Thus:

```text
physical record presentation != native category route != materialized rows
```

Any other list shape is a no-op, not a best-effort mutation.

### Presentation-route separation

**Question.** Can title and icon move while categories remain fixed?

**Mutation.** On the same exact nine-record fixture, swap record 7/8 borrowed
display-title pointers and copy their complete `0x40` presentation payloads
across. Leave category values `8` and `4` unchanged. Keep replacement UTF-16
storage process-lifetime.

**Result.** Titles and native page routes crossed. The stock Graphics
descriptor rendered in the other stock-compatible cell. Artwork embedded in
the specialized ninth cell did not follow its small descriptor token.

This proves both independence and a limit: copying a resource ID is not a
general image-registration mechanism.

### Dual-layer and ninth-cell controls

**Question.** Which authored cells and layers are safe repaint targets?

**Procedure.**

1. Resolve `TabList/Item_0_N` and `BackTabList/Item_0_N` separately.
2. Repaint a changed mapping in only `TabList`; capture transition and
   selected/unselected states.
3. Repeat with only `BackTabList`.
4. Repaint both and compare.
5. On the modified ninth cell, present a stock descriptor, then run an
   explicit transparent vector pass.

**Result.** Either one-layer case left stable overlapping or stale art. Both
layers produced one correct icon per cell. Ninth-cell slot artwork survived
descriptor and transparent passes. Therefore the viewport uses two layers
times eight cells, not nine interchangeable records.

### Input provenance and pointer equivalence

**Question.** Can controller and pointer be distinguished without timing or
index heuristics, and can a stationary pointer be observed?

**Procedure.**

1. At `0x73A196`, clear source state for the identified selector.
2. At `0x73A1C7`, mark pointer only when the pointer helper reports the
   selection change.
3. At `0x73A358`, record previous/next only for x `-1/+1`, y zero.
4. At `0x73A394`, compare frame-entry and final physical selection and bind
   the source to the ordinary callback.
5. Independently call `0x758940` then `0x737AE0` once per active input frame.
6. When a native sample exists, compare its exact coordinate bits and hit to
   the independent poll.

**Result.**

| Frames | Natural samples | Stationary fallback polls | Coordinate mismatch | Hit mismatch | Fault/SEH |
|---:|---:|---:|---:|---:|---:|
| 1,530 | 331 | 1,199 | 0 | 0 | 0 |

The native movie emits no stationary heartbeat. Absence of a natural event is
not a pointer leave. The independent poll supplies the dwell clock while
reusing the exact native coordinate conversion and hit test.

### BitmapData rejection

**Question.** Can AS3 `BitmapData` provide a writable pixel surface?

**Ownership gate.** After the native composite frame, construct one
transparent 2x2 `BitmapData`, store it under a private movie property, destroy
temporary wrappers, reacquire it on the next frame, and require the same
payload.

**Single mutation.** On the proven object, invoke only:

```text
setPixel32(0, 0, 0xA17BC3E5)
```

No `Bitmap`, `addChild`, `dispose`, or readback call is mixed into this gate.

**Result.** Invocation returned success, raised no SEH, and the rooted object
survived, but the renderer became black. Passive reads then found:

| Field | Result |
|---|---|
| `MovieRoot + 0x88` | stable VM pointer |
| `VM + 0x5470` | readable null DrawableImage context |
| `VM + 0x5478` | readable null provider |

Object ownership was proven; mutable pixel backing was not. The path is
rejected and production never calls the missing lazy service.

### Immutable RawImage and multiple identities

**Question.** Can client bytes become an immutable image without the missing
DrawableImage provider?

**Procedure.**

1. Validate RawImage create/get-data/clear/release entry bytes and vtable.
2. Call `RawImage::Create` with format `1` (`Image_R8G8B8A8`), one mip,
   size 32x32, use `0`, null heap, and null update synchronizer.
3. Require a `0x40` ImageData view, one plane, 32x32 dimensions, sufficient
   pitch/data size, and the expected RawImage object/refcount.
4. Write pixels row by row, calculate and reread a deterministic checksum.
5. Clear only the copied ImageData view.
6. On 2.7.1.0, validate heap, `0x98` resource allocation, constructor,
   vtables, type, AddRef/Release, and deleting destructor.
7. Construct distinct named resources; require RawImage refcount `1 -> 2`,
   then release the creator reference and require wrapper-owned count `1`.
8. Publish the complete resource set only after every entry succeeds.
9. Correlate texture lookup and unchanged native binder; require correct
   binder-local resource count before consuming its reference.

**Identity control.** First reuse a resource/Shape identity for a second image;
then use independent image pointer, resource pointer, name/ID, and checksum.

**Result.** Immutable images rendered without the black screen. Shared or
reused identity produced first-image aliasing or a white square. Independent
identity quadruples displayed the distinct `X`, `+`, diagonal, and checker
patterns. A fresh dynamic Shape was required when changing raster asset.

### Candidate page carriers

**Question.** Which native page can represent arbitrary rows without authored
template artifacts or stock-page corruption?

| Candidate | Control | Result |
|---|---|---|
| Game Options/category 0 | wrap away/back; close/reopen | page became empty or inherited custom rows; rejected |
| category 4/selector 6 | replace only native rows | authored brightness instruction/calibration image remained; rejected as generic page |
| category 9/selector 14 | pair with category 4 | proved a second independent cache bank, not arbitrary count |
| Graphics/category 8/selector 11 | restore native Graphics and reconstruct custom routes repeatedly | row-only template, correct navigation and native Graphics restoration; accepted |

The two-bank probe established distinct panels, complete SceneObjProxy
wrappers, payloads, values, and native teardown. It did not establish an
unbounded cache. The scalable result is sequential reconstruction of one
validated rows-only carrier while logical page state remains host-owned.

### Movie-epoch control

**Question.** Does top-pointer equality identify one Configuration movie?

**Procedure.** Snapshot all eight native records only after verifying record
count, order, vtables, categories, native-owned title pointers/contents, and
complete presentation payloads. On every initialization, compare:

- top address and vtable;
- movie object interface;
- owner link;
- MovieRoot;
- complete stock record identity; and
- both reserved icon-property banks.

Restore pristine records before applying a viewport. If the movie fingerprint
changes, reconcile Shape properties through the new top before publication.
Arbitrary partial mismatch suspends mutation.

**Result.** Close/reopen could reuse addresses, so pointer inequality alone
was insufficient. Complete record and movie fingerprints restored populated
Game Options and correct overflow presentation across repeated opens.

## Logical viewport

The viewport is a pure state machine. It does not own GFX values, page
pointers, icon resources, or native category IDs.

Let:

```text
L = logical tab count
C = physical capacity (8)
V = min(L, C)
R = requested navigation margin
M = min(R, floor((V - 1) / 2))
S = current window start
Smax = L > V ? L - V : 0
l = selected logical index
p = selected physical index = l - S
left anchor  = M
right anchor = V - 1 - M
```

The mapping is contiguous:

```text
logical_for_physical(p) = S + p,  0 <= p < V
physical_for_logical(l) = l - S,  S <= l < S + V
```

After selecting logical index `l`, alignment is:

```text
if L <= V:                         S' = 0
else if l < S or l - S < M:       S' = max(0, l - M)
else if l - S > V - 1 - M:        S' = min(l - (V - 1 - M), Smax)
else:                              S' = S
```

This produces directional hysteresis. With margin one, right movement starts
shifting beyond physical slot six and left movement starts shifting before
physical slot one; reversing once does not immediately move the complete
strip.

### Wrap and mouse selection

- Controller next from `L - 1` selects logical zero and aligns `S = 0`.
- Controller previous from zero selects `L - 1` and aligns `S = Smax`.
- A pointer event first resolves `logical = S + physical` using the mapping
  that was actually painted. Edge alignment may then shift the window.
- Pointer dwell stops at a catalogue endpoint; it never wraps.
- A nonzero mapping generation changes only when the physical-to-logical
  mapping changes. Zero remains the never-initialized sentinel.
- A queued or correlated native operation is rejected if its captured
  generation no longer matches the presented generation.

### Overflow indicator predicate

Dots are a presentation state on a real mapped cell:

```text
left dots  iff p == 0     and S != 0    and p != selected physical
right dots iff p == V - 1 and L - S > V and p != selected physical
```

The selected real tab is never covered. Dots do not create a route and do not
replace the logical mapping underneath the cell.

```text
logical catalogue
0 ... S-1 | S ... S+V-1 | S+V ... L-1
             ^ eight physical cells ^

TabList      [front icon hosts 0..7]
BackTabList  [back icon hosts 0..7]
                  same mapping generation
```

### Native commit requirement

A logical transition is complete only when all of these agree:

1. candidate `TabViewport` state;
2. eight record titles, categories, and presentation descriptors;
3. both Scaleform layers and their Shape-property banks;
4. native selector physical index;
5. top title;
6. current native panel route; and
7. logical page/row model.

The source mapping remains authoritative until the candidate passes. Held
controller callbacks are serialized against the source or target selector
anchor; they cannot overwrite an in-progress transition with an index from a
different mapping generation.

## Selection-source recovery and pointer dwell

```text
GridControl frame begins @ 0x73A196
|
|-- pointer helper result @ 0x73A1C7 -------- source = pointer
|       `-- native cell sample @ 0x73B880
|
`-- horizontal commit @ 0x73A358 ------------ source = previous/next

both converge at final dispatch @ 0x73A394
    -> ordinary callback @ 0x968510
    -> composite-frame serialization @ 0x93D5E0
```

The stationary poll uses the game's own path:

```text
0x758940 get native pointer position
    -> 0x737AE0 GridControl hit test
    -> physical cell or -1
    -> when due, 0x73CA60 native pointer select
```

In the armed-dwell case, rewriting the sampled `EAX` to `-1` at `0x73B880`
takes the native no-hit branch and suppresses repaint-induced re-selection
caused by leaving a stationary cursor over a cell whose logical contents have
just changed. The same narrow register mutation is also used while a viewport
transition is pending so a concurrent natural hit cannot race the transaction;
the complete predicate is in the mid-hook contract above.

### Dwell state machine

```text
idle
  | immediate natural hover on an overflow edge
  v
awaiting_commit --failed or no more tabs--> idle
  | transition committed and more tabs remain
  v
cooldown --real pointer motion--> cooldown(initial delay, repeat count reset)
  | deadline reached
  v
repeat_due --dispatch native pointer setter--> awaiting_commit

controller previous/next, pointer leave, different cell, or validation fault
    -> idle
```

For committed automatic repeat index `n`:

```text
delay(n) = max(minimum_repeat_delay,
               repeat_delay - n * repeat_decay)
```

Defaults:

| Setting | Value |
|---|---:|
| initial delay | 250 ms |
| repeat delay | 200 ms |
| per-repeat decay | 20 ms |
| minimum repeat delay | 50 ms |

Zero decay means fixed cadence. Genuine pointer movement resets the repeat
count and starts a fresh initial delay. Controller navigation cancels dwell
and retains the game's ordinary immediate/held-repeat path.

## Tab presentation and RGBA8 image path

### Resolved GFX hosts

| Layer | Exact path | Role |
|---|---|---|
| selected/front | `TabList/Item_0_%zu/Cursor/ItemIcon` | selected cursor/highlight presentation |
| background/back | `BackTabList/Item_0_%zu/ItemIcon` | coincident unselected/transition presentation |

Accepted authored icon vtables are `0x2CBD4E0` and `0x2CC6268`; a runtime Shape
must use `0x2CBD158`. The common Graphics-icon canvas recovered from the
forward and reverse scale probes is:

| Property | Value |
|---|---:|
| left | `-27.200` |
| top | `-23.050` |
| width | `54.599` |
| height | `48.100` |

Two parent-owned property banks are used. A remap paints the changed target
into the inactive bank while the active Shape remains attached. Only after the
new property, parent, asset, transform, visibility, and ownership are verified
is the old bank cleared. This prevents a render boundary containing only the
underlying native fallback icon.

### Descriptor, lookup, and Shape attachment

The presentation descriptor is constructed by the game's own builder rather
than fabricated:

```cpp
using MakeIconDescriptorFn = void*(__fastcall*)(
    void* descriptor, std::int32_t icon_id); // RVA 0x73E230
```

The host zeroes a `0x40`-byte destination, calls the builder, and admits the
result only when the return value equals the destination,
`descriptor[0x38] == 0`, and the signed ID at `descriptor+0x3C` equals the
request. Current internal identifiers are:

| ID/name | Use |
|---|---|
| `90001` / `MENU_ItemIcon_90001` | retained historical/reserved vector-cross probe constant; current production does not build a descriptor with this ID |
| `90002` / `MENU_ItemIcon_90002` | overflow ellipsis descriptor |
| `91000+i` / `MENU_ItemIcon_%05d` | custom logical tab `i`; raster when bytes exist, vector cross otherwise |

These numbers and names are current private implementation coordinates, not
API identifiers. Production also assigns each raster a distinct internal name
`ERNativeUI.TabProbe.RawRGBA8.%04u` (`i+1`); the historical word `TabProbe` in
that private string has no ABI meaning.

The texture-lookup detour accepts a per-tab name only if all of these tests
pass:

1. exact `MENU_ItemIcon_` prefix;
2. exactly five ASCII decimal digits followed immediately by NUL;
3. parsed ID in `[91000, 91000 + active_custom_count)`;
4. exact equality with the generated name at the derived index;
5. an active repaint context for the current top, one of the two admitted
   layers, and physical cell `0..7`; and
6. the mapped logical tab equals `8+i`.

An unrelated lookup, recursive alias lookup, malformed name, stale mapping,
or wrong cell delegates unchanged to Elden Ring. For an effective raster, the
detour validates the retained resource vtable, image pointer, delegate
vtable/refcount, kind, resource refcount, RawImage vtable, checksum, and movie
epoch. It then AddRefs exactly once and publishes that one caller-owned
lookup reference. If publication fails after AddRef, it balances only that
unpublished reference and disables later raster publication. A non-raster
custom tab and the overflow marker bind a known native fallback first, then
replace its visible art with a vector Shape.

The attachment transaction uses only validated AS3 bridge slots:

```text
native presenter requests synthetic MENU_ItemIcon name
    -> correlated lookup establishes logical/layer/physical context
    -> native presenter creates its binder-local resource reference
    -> MovieRoot::CreateObject("flash.display.Shape")
    -> require dynamic Shape vtable 0x2CBD158
    -> resolve Shape.graphics as ordinary object (flags == 0x08)
    -> raster: unchanged 0xD833F0 binder consumes the local reference
       vector: invoke beginFill/drawRect/endFill
    -> SetMember(parent, inactive injected-property, Shape)
    -> SetMember(parent, inactive authored-property, authored icon)
    -> GetMember both properties and verify payload/vtable identity
    -> apply recovered position; make Shape visible
    -> Invoke(parent, "addChild", Shape)
    -> hide authored icon only after attachment succeeds
    -> audit host bit, asset identity, refcounts, and movie fingerprint
```

The two property pairs are currently
`__ernativeui_tab_viewport_probe_icon_a/b` and
`__ernativeui_tab_viewport_probe_authored_icon_a/b`. For each of the 16 hosts
(front/back times eight cells), a bit records which bank is active. During a
mapping change, unchanged host/asset pairs remain attached. Every changed
target is built and verified in the inactive bank above the still-visible
source; only then are old children removed and the bank bits/ownership ledgers
published together. The implementation never rebinds a different raster into
an already bound Shape: a changed asset receives a fresh dynamic Shape.

Failure after property publication invokes `removeChild`, restores authored
visibility, writes an Undefined tracked value to both properties, reads the
properties back, destroys all local wrappers, and clears the host bit only
after exact cleanup. If ownership becomes uncertain, the object and ledger are
retained and future raster publication fails closed; guessing a Release would
risk a use-after-free.

### Immutable image pipeline

```text
client 32x32 row-major RGBA8
    -> synchronous host copy
    -> premultiply each RGB channel for the native upload:
       native = (client_channel * alpha + 127) / 255
    -> RawImage::Create(format=1, mip=1, 32x32, use=0,
                        heap=null, update_sync=null)
    -> RawImage::GetImageData
    -> validate format, one plane, dimensions, pitch, and size
    -> write pixels and verify checksum
    -> ImageData::Clear(copied view only)
    -> allocate/construct 0x98-byte ScaleformImageResource
    -> release creator RawImage reference
    -> atomically publish complete resource catalogue
    -> correlated native texture lookup returns an AddRef'd resource
    -> unchanged native image binder paints a fresh dynamic Shape
    -> parent property roots Shape in each front/back host
    -> hide authored icon only after full verification
```

Scaleform consumes premultiplied RGB in this path. The public API accepts one
ordinary RGBA8 representation and converts it privately. Opaque input already
has alpha 255, for which the conversion is an exact identity.

### Alpha discriminator fixture

The alpha result was not inferred from two visually similar opaque icons. The
32x32 test image used opaque white two-digit seven-segment labels over a
deterministic per-icon color. For every unlit pixel,
`alpha = floor(x * 255 / 31)`. Rows `0..15` stored that color as straight RGB;
rows `16..31` stored `(channel * alpha + 127) / 255`. Thus the two halves had
the same intended color/alpha but different byte conventions, including
nonzero RGB at zero alpha in the ordinary half. The premultiplied half blended
correctly through the native binder. Production consequently accepts one
ordinary RGBA8 form for every alpha value and always performs the required
native conversion inside the host before creating RawImage.

### Ownership transitions

| Boundary | Resource refcount | RawImage refcount | Meaning |
|---|---:|---:|---|
| RawImage created | n/a | `1` | creator owns image |
| image resource constructed | `1` | `2` | resource retains image |
| creator image reference released | `1` | `1` | process root is resource-owned |
| texture lookup return | `1 -> 2` | unchanged | caller receives temporary resource reference |
| presenter creates binder local | `2 -> 3` | unchanged | presenter owns an additional local resource reference |
| binder consumes its local | `3 -> 2` | fresh bind increments by `1` | resource local is released; the bound Shape retains the RawImage |
| caller settles lookup reference | `2 -> 1` | unchanged | permanent resource root remains |
| steady active hosts | `1` | `1 + active_host_count + retirement_debt` | one RawImage reference per attached host; debt covers detached Shapes not yet released by renderer |
| no active Shape after debt settles | `1` | `1` | safe detached baseline |

Cleanup resolves `shape.graphics`, calls `graphics.clear()`, removes the child,
restores authored visibility, clears both private properties, reads both back
as Undefined, then releases wrappers and proxies. `removeChild` alone is not a
release proof because the tracing collector may still retain the detached
Shape.

Each raster asset needs an independent image pointer, resource pointer,
resource name/ID, and checksum; vector-fallback tabs deliberately own no
raster resource. IDs `90001`, `90002`, and `91000+i` are
private host implementation values, not public tab identity; production still
uses this internal namespace.

## Reusable Graphics carrier

Native Graphics and every custom logical tab use category 8 sequentially.
Category 5 Network is a genuine synchronous parking page and is never mutated
or retired by ERNativeUI.

### Route transaction

```text
request logical Graphics or custom route R
    |
    | validate mapping generation, selector, cache, current route,
    | panel/wrapper/payload identities, callback bytes
    v
if category 8 is current:
    switch synchronously to real category 5 Network
    suppress the exact armed old-to-new state copy
    verify Network current and distinct from carrier
require completed_frame > carrier_activation_frame
verify carrier panel/wrapper/payload is unique across all ten cache slots
resolve native owner with 0xE1C080
validate panel deactivate == image+0x953520
validate owner vtable and queue method at owner_vtable+0x68
    clear host references that still equal old carrier
    deactivate old carrier
    queue old carrier exactly once through validated native owner
    clear cache[8]; never inspect old carrier again
    require cache[8] == null
    publish R before category-8 factory executes
    switch back to category 8
    wrapper selector 11 constructs:
        R = native Graphics -> host row thunk -> original 0x95C050
                               plus native secondary 0x95A820
        R = custom         -> host row thunk and zero secondary callback
    verify new cache/current/panel/wrapper/payload/route
    commit viewport, title, selector, and input
```

Parking, retirement, reconstruction, and return occur in one composite-frame
callback. There is no intended render boundary while Network is current.
A carrier activated in frame `F` cannot be retired until a composite callback
whose completed-frame counter is greater than `F`; violating that age gate
freezes custom transitions. A content-only change skips unrelated record,
title, and icon repaint.

The owner returned by `0xE1C080(old_panel)` is accepted only when its vtable,
the function at vtable `+0x68`, and the first eight bytes of that function
match one row in this 2.7.1.0 allowlist. The final column is the eight entry
bytes interpreted as a little-endian `uint64_t`:

| Manager vtable RVA | Queue RVA | First 8 bytes |
|---:|---:|---:|
| `0x29D54F0` | `0x2657530` | `0xFF864788E9CA8B48` |
| `0x2B7D418` | `0xB97EC0` | `0x74894808245C8948` |
| `0x2BDE708` | `0xE14B60` | `0x8B4820EC83485340` |
| `0x2BDE8F0` | `0xE156B0` | `0xC74840EC83485740` |
| `0x2BDECA0` | `0xE16260` | `0x28C1834808498B48` |
| `0x2BDEA48` | `0xE15BA0` | `0x28C1834808498B48` |
| `0x2BDED78` | `0xE166D0` | `0xC74840EC83485740` |
| `0x2BDE818` | `0xE150C0` | `0xC74840EC83485740` |
| `0x2BDEB20` | `0xE15F30` | `0x28C1834808498B48` |
| `0x2BDEE50` | `0xE16CA0` | `0xC74840EC83485740` |
| `0x2BE1870` | `0xE1F250` | `0x8B4820EC83485340` |
| `0x2BE1A20` | `0xE1F7B0` | `0x8B4820EC83485340` |
| `0x2F26FA8` | `0x1C59C30` | `0x00262088E9CA8B48` |
| `0x2F273D0` | `0x1AF6110` | `0x5FA5EF08900000C2` |
| `0x30357D8` | `0x1B44400` | `0x28C1834808498B48` |
| `0x3051240` | `0x1C29FE0` | `0xA33CC7EC900000C2` |
| `0x30B0298` | `0x1EBCE90` | `0xEB17900000065BE9` |
| `0x30C0228` | `0x1F38D90` | `0xC74840EC83485740` |
| `0x30CA908` | `0x1F61F50` | `0x48018B4808498B48` |
| `0x30CAE70` | `0x1F64450` | `0xC74830EC83485740` |
| `0x30CFE80` | `0x1F81AA0` | `0x3176CC030E302AE9` |
| `0x30CFF58` | `0x1F81AB0` | `0x35A19002D12495E9` |
| `0x3278EA0` | `0x26398D0` | `0xC74830EC83485740` |
| `0x30C74A0` | `0x1A4900` | `0x28C1834808498B48` |
| `0x2BBFF38` | `0x1538B0` | `0xC74840EC83485740` |
| `0x2BE0F30` | `0x1F1A8D0` | `0x48018B4808498B48` |

After deactivation, queue invocation is an ownership boundary. The queue may
have accepted the panel even if the call faults. Therefore slot 8 is cleared
on both the normal and exceptional paths, the tracked carrier identity is
discarded, and `old_panel` is never dereferenced again. Retrying the queue or
leaving slot 8 populated would permit double retirement during native
teardown.

### Native and custom construction

For native Graphics, the primary descriptor points to the host thunk, which
delegates to the original materializer with its recovered second argument;
the secondary descriptor remains native. Native behavior is preserved, but
the primary descriptor is not byte-for-byte original. For a custom route:

1. publish the requested logical route before the factory runs;
2. replace only the exact primary callback with the same host thunk;
3. zero the Graphics-specific secondary callback and its 32-bit metadata;
4. leave unobserved descriptor padding untouched;
5. require a fresh category-8 panel whose native row count is zero;
6. validate panel vtable, capacity, SceneObjProxy, wrapper, and payload;
7. inject the exact compiled page slice;
8. require attempted rows equal added rows and the final native count; and
9. require panel identity to remain stable across materialization.

The secondary callback is what installs Graphics apply/rollback state. Zeroing
it makes the native constructor take its generic post-initialization branch,
so client rows do not inherit display-settings behavior.

Logical page state is outside the carrier. Client row values, callbacks,
pagination plan, stable key, order, title, and copied icon survive native
panel reconstruction.

## Movie epochs and stock restoration

A new System menu can reuse an old address. Pointer inequality is not an epoch
test. The first fully validated stock catalogue is copied into process-stable
storage. For each of eight records the fingerprint includes:

- source title pointer and complete copied title;
- record vtable;
- native category; and
- complete `0x40` presentation payload.

Before viewport initialization all records are restored from that pristine
snapshot. A null title, host-owned title pointer, incomplete title, category
mismatch, or presentation mismatch refuses the epoch.

The movie fingerprint separately includes the GFx object interface, owner
link, and MovieRoot. If it changes while records persist, the adapter
reconciles reserved Shape properties through the new top and restores stock
records before initializing a new viewport. Arbitrary partial mismatch freezes
mutation while title storage remains pinned.

## Threading, ownership, and failure policy

| Invariant or failure | Check boundary | Response | Native behavior retained |
|---|---|---|---|
| unknown PE or entry-byte mismatch | install preflight | do not publish custom-tab capability | entire native menu |
| incomplete hook group | disabled-hook prepare/enable | keep callbacks inert; do not publish group | host without custom tabs |
| stock record or GFX host mismatch | epoch initialization | do not mutate that System instance | native System |
| stale mapping generation | selection/presenter/binder correlation | discard candidate; never reinterpret old physical cell | last committed mapping |
| one requested raster fails bootstrap | resource catalogue build | publish no partial raster prefix; use validated fallback policy | title/content/navigation |
| Shape bind/property verification fails before commit | icon transaction | restore authored visibility and property state when exact; otherwise freeze | proven prior presentation |
| carrier preflight mismatch | hook installation | reject custom-page backend | ordinary native pages |
| carrier cache, route, allocator, callback, or identity mismatch | composite transaction | sticky-freeze further custom transitions and preserve coherent selector/mapping | live native input and Back/O |
| ownership uncertain after guarded fault | cleanup | retain object instead of guessing at release | avoids double release/use-after-free |
| native System teardown | top/composite teardown | clear epoch-local hosts after exact audit | process roots remain valid for reopen |

All native calls, record writes, panel reconstruction, Shape/property writes,
and input serialization run on the menu/UI path. Client strings, pixels, page
models, and catalogue entries are copied into host-owned storage before native
publication. Once callbacks and native resources are exposed, runtime hot
unload is unsupported; their code and storage remain process-lifetime.

Hook creation is transactional only until first exposure. Hooks are prepared
disabled, callbacks remain pass-through until the complete group is armed,
and an unexpectedly partial exposed group is retained rather than uninstalled
under a callback that can enter without a host-side drain lock.

## Recorded results

### Native pointer poll

| Metric | Result |
|---|---:|
| active input frames | 1,530 |
| natural native samples | 331 |
| natural samples matching independent poll | 331 |
| stationary fallback polls | 1,199 |
| coordinate mismatches | 0 |
| hit mismatches | 0 |
| native/SEH faults | 0 |

This bounded measurement supports pointer-poll equivalence on the exact tested
build.

### Final integration fixture

| Property | Value |
|---|---:|
| stock logical tabs | 8 |
| custom logical tabs | 50 |
| total logical tabs | 58 |
| custom icon dimensions | 32x32 RGBA8 |
| independent raster identities/checksums | 50 |
| logical rows per custom tab | 20 |
| total client rows | 1,000 |
| first-page capacity in the tested carrier | 7 placements |
| first page in fixture | rows 1–6 plus Next |
| continuation | Previous plus remaining rows, paginated as required |

The prescribed live sequence was:

1. Full game restart; keep the pointer below the strip. Open System, wait five
   seconds, and require populated Game Options, one right ellipsis, no black
   frame, and no missing page.
2. Select Custom Tab 1 and inspect alpha before stress. Alpha-zero pixels must
   reveal the native strip; a solid rectangular fallback fails the run.
3. On custom tabs 1, 25, and 50, verify rows 1–6 plus Next on page 1 and
   Previous plus rows 7–20 across continuation pages. Change rows 1 and 20,
   leave, return, and require independent persistence.
4. Tap R1 across all eight stock and all 50 custom tabs. Require each numbered
   icon exactly once, then one more R1 must wrap to populated Game Options.
5. From Game Options, tap L1 once to reverse-wrap to custom tab 50; traverse
   the complete catalogue in reverse and return to Game Options.
6. Hold R1 for at least three wraps, hold L1 for at least two, then rapidly
   reverse direction. Require coherent focus, titles, pages, icons, markers,
   row input, and non-growing transition time.
7. Close/reopen System five times. On the fifth open, repeat a complete
   forward and reverse traversal; require no resource rebuild, alias, white
   square, stale epoch, or empty stock page.
8. Reopen at the first window. Hold a stationary pointer on the right
   ellipsis and require the 250/200/180/.../50 ms cadence to reach tab 50
   without endpoint wrap. Use the left ellipsis to return; then verify pointer
   motion resets the delay and R1 cancels dwell immediately.
9. Exit normally and inspect the viewport and host logs. Any warning/error,
   incomplete/frozen transaction, nonzero SEH, wrong icon/title/page, missing
   wrap, black frame, lost Back/O, or growing latency fails the run.

The tester reported a successful live run against this acceptance matrix:

- single-step traversal in both directions;
- forward and reverse wrap;
- held R1/L1 across multiple complete traversals;
- rapid direction reversal;
- direct pointer jumps;
- stationary dwell at both overflow edges;
- dwell reset after genuine pointer motion;
- controller cancellation of dwell;
- correct icon/title/page alignment;
- pagination and value isolation on custom tabs 1, 25, and 50;
- restoration of native Graphics and populated Game Options;
- Back/O, focus, sound, and row input; and
- five close/reopen cycles followed by another complete traversal.

This outcome is classified **Reported live** under the evidence scheme above.
The acceptance matrix is the reproduction contract; no representative latency
distribution is claimed.

Expected records in a future reproduction include:

```text
STRESS bootstrap complete=1
STRESS catalogue ready count=50 ... unique=1
STRESS viewport initialized ... logical=58 physical=8
STRESS transition ... complete:1 frozen:0
Native-tab Graphics carrier construct ... logicalRows=20 ... committed=1
```

Any warning/error, incomplete transaction, frozen remap, wrong numbered icon,
missing wrap, empty/wrong page, black frame, lost Back/O, or increasing
per-transition delay fails the run.

## Rejected hypotheses

| Candidate | Discriminating evidence | Observation | Durable conclusion |
|---|---|---|---|
| one independent tab hook per mod | observer loaded beside another host | occupying the same native entry prevented the other tab hook from enabling | one process-wide coordinator owns these hooks |
| append until the native limit | fixed-vector and GFX inspection | native capacity ten and visual capacity nine are separate fixed bounds | neither is an extensibility model |
| use the modified ninth GFX cell | descriptor and transparent repaint | slot-local art survived | restrict production to homogeneous cells `0..7` |
| category, title, icon, and content are one identity | category mutation and presentation swap | route and presentation crossed independently | model each boundary explicitly |
| copy a small icon token | move custom descriptor to ordinary cell | artwork did not follow token | descriptor ID is not image ownership |
| repaint one tab layer | remap with one layer unchanged | old/new icons overlapped | repaint `TabList` and `BackTabList` |
| represent dots as a fake tab | navigation prototype | skipped or complicated real destinations | dots are presentation on a real mapped cell |
| infer source from adjacency/timing | direct hover, wrap, held repeat | cases are ambiguous | capture native branch provenance |
| delay all tab input | compare held controller repeat | controller UX degrades | dwell is pointer-only |
| rely on native stationary hover callbacks | stationary pointer capture | native path is motion-driven | poll the same native conversion/hit test |
| mutable `BitmapData` upload | one rooted `setPixel32` | renderer blackened; backing services null | use immutable RawImage |
| reuse one image/resource identity | multiple distinct pixel patterns | alias or white square | independent identity per raster |
| reuse one cleared Shape for another image | sequential bind test | white square/repeated first image | create a fresh Shape for changed asset |
| use Game Options as custom carrier | wrap/reopen | empty stock page or custom rows under it | do not alias a stock content page |
| use category-4 authored page as blank carrier | replace rows | calibration instructions/image remained | rows are not the entire movie template |
| fabricate one native panel per logical tab | cache/factory ownership analysis | no safe arbitrary fabrication/destruction contract | recycle one validated native carrier |
| treat menu address as movie epoch | repeated close/reopen | address reuse retained stale presentation | compare complete record/movie fingerprint |
| publish resources incrementally | multi-resource identity gate | partial prefix creates ambiguous fallback/ownership | publish catalogue atomically |
| uninstall exposed hooks/resources dynamically | callback and owner graph | no drain boundary covers all native entries | pin exposed runtime for process lifetime |

## Known unknowns

- Complete semantics of every `MenuOptionCategory` field outside the
  recovered title, fallback string, category, and 64-byte presentation data.
- Original FromSoftware names for several selector, presentation, and
  resource functions.
- Complete `ScaleformImageResource` field layout beyond the validated
  vtable, counts, direct image, delegate, kind, and lifecycle.
- A safe generic mutable-image provider for this Configuration VM. None is
  initialized in the tested movie.
- Equivalent complete native resource ownership on 2.7.0.0; API 1.3 does not
  admit it.
- Compatibility with arbitrary third-party GFX files or foreign hooks at the
  same entry points.
- Whether a future GFX revision retains the two coincident layers, cell
  transforms, and Shape identities.
- Quantified latency distributions across representative hardware. Diagnostic
  logging materially affects transition timing, so no general distribution is
  claimed.

## Revalidation after a game update

1. Record the full PE identity and import it into a separate analysis project.
2. Re-derive top construction/teardown, fixed list, record stride/ownership,
   selector, composite cache, factory dispatch, switch, and panel teardown.
3. Compare every input, pointer, presentation, resource, Shape, and carrier
   boundary, including local bytes, callers, register/stack contracts, and
   object vtables.
4. Extract and hash the exact `02_040_OptionSetting` GFX used for the run.
   Verify `TabList`, `BackTabList`, all cell paths, matrices, authored Shape
   types, and bounds.
5. Repeat the observation-only list and input-provenance probes before any
   mutation.
6. Repeat dual-layer and ninth-cell controls.
7. Revalidate RawImage format/plane layout, native heap/resource construction,
   AddRef/Release transitions, binder, fresh Shape, detach, and movie teardown.
8. Revalidate Graphics callbacks, selector 11, Network parking, category-8
   retirement/reconstruction, native Graphics restore, and Back/O.
9. Run the pure viewport, dwell, epoch, transition, catalogue, runtime-state,
   configuration, pagination, and current ABI tests.
10. Repeat the complete live fixture and retain hashes, logs, screenshots,
    timings, and configuration.
11. Leave the capability unavailable if any required boundary is missing,
    ambiguous, or semantically changed.

Do not port these RVAs by byte-pattern search alone. Re-derive the graph with
independent anchors:

| Boundary to recover | Derivation chain |
|---|---|
| top dialog | MSVC RTTI type descriptor for `CS::OptionSettingTopDialog` -> COL -> primary vtable -> constructor/destructor xrefs -> allocation site; recover member offsets again from constructor writes |
| selector/controller | RTTI for `CS::OptionSettingTopDialog::_SettingTabControl` -> three-slot vtable -> frame slot -> selected-index getter/xrefs -> native pointer and horizontal paths; verify `top+selector_offset`, do not carry `+0xA38/+0xD4` forward by assumption |
| category records | `BasicViewItemList<CS::MenuOptionCategory,10>` vtables and top-constructor xrefs -> append helper -> copy constructor -> title/category/presentation consumers; recover stride, ownership, count guard, and destructor together |
| composite cache | `CS::CompositeOptionSettingDialog` RTTI/COL/vtable -> constructor and teardown -> ten-slot walk -> switch routine -> factory dispatcher -> current-panel frame call |
| two GFX layers | top-constructor string xrefs for `TabList`, `TabList/RightArrow`, and `BackTabList` -> indexed `Item_0_N` presenter -> record presentation payload; verify paths against the exact extracted movie |
| Graphics carrier | category-8 case in the recovered factory -> Graphics wrapper -> selector-11 immediate -> stack-local primary/secondary callbacks -> original materializers; derive Network parking independently from category 5 |
| pointer provenance | selector selected-index xrefs -> GridControl frame prologue -> pointer-helper result -> horizontal commit -> final selection comparison; prove the native position getter and hit test by coordinate/hit equivalence again |
| image path | indexed presenter -> texture-lookup call -> image-bind call -> resource AddRef/Release; derive RawImage and image-resource identities from vtables/callers, then derive MovieRoot/CreateObject and Shape bridge from the live target wrapper |

For every chain, require at least two independent links (for example RTTI plus
constructor writes, or call-site arguments plus callee field accesses). A
matching prefix at one old RVA is only a candidate.

## Reproduction seeds

For a clean static pass, useful entry points are:

```text
0x807430  top allocation/factory
0x9672C0  top construction
0x967B20  top teardown
0x968CF0  fixed category append
0x9680D0  selector/panel frame dispatcher
0x968510  selection callback
0x968590  BackTab callback
0x967DE0  indexed tab presenter
0x93C090  composite construction
0x93C1A0  composite teardown
0x93C900  category switch
0x93D730  category factory dispatcher
0x93D5E0  current-panel frame
0x93CEF0  Graphics wrapper
0x93CF89  Graphics pre-builder seam
```

Supporting public reference:

- [`configuration-class-map.md`](../architecture/configuration-class-map.md)
  for RTTI, construction, fixed storage, dispatch, cache, and materializers.

The recovered contracts needed to reproduce the analysis are stated directly
in the layouts, address inventory, call-flow, and controlled-procedure sections
of this document.

## Production source and test map

| Evidence area | Native evidence | Production source | Pure tests | What the test does not prove |
|---|---|---|---|---|
| stock list and native layout | observation-only list and dispatch run; class map | validators in `native_tabs.cpp` | record-epoch tests | future executable layout |
| route vs presentation | bounded category and presentation field isolation | `native_tabs.cpp` | catalogue/viewport tests | native page ownership |
| viewport and wrap | live eight-cell viewport run | `tab_viewport.cpp/.hpp`; native adapter | `tab_viewport_test.cpp` | native GFX/hook behavior |
| input serialization | live input-provenance run | `tab_transition_input_gate.hpp`; adapter | `tab_transition_input_gate_test.cpp` | exact future input seams |
| pointer dwell | native poll-equivalence and stationary-hover runs | `tab_pointer_dwell.hpp`; adapter | `tab_pointer_dwell_test.cpp` | live pointer/GFX behavior |
| icon resources | live RawImage and Shape runs | `native_tabs.cpp`; runtime ledger | `native_tab_runtime_state_test.cpp` | renderer behavior without a live run |
| movie epoch | close/reopen identity run | `tab_record_epoch.hpp` | `tab_record_epoch_test.cpp` | future movie/address reuse |
| catalogue/order | live multi-tab integration run | `native_tab_catalogue.cpp/.hpp`; `tab_configuration.cpp/.hpp` | catalogue/configuration tests | native materialization |
| carrier | live category-8 reconstruction run | `hooks.cpp`; `native_tabs.cpp` | transition/pagination tests | native retirement without a live run |
| final integration | reported 50-tab acceptance matrix | host plus `examples/custom_tabs_stress` | current ABI/contract tests | performance on arbitrary systems |

## Relation to API 1.3

The native addresses and layouts above are private implementation coordinates,
but they are public reverse-engineering evidence. API 1.3 exposes a
provider-local stable tab ID, title, preferred order, optional copied 32x32
RGBA8 icon, and provider-owned page handle. Clients do not own native
categories, resource IDs, category records, GFx objects, or the carrier
protocol.

The production logical catalogue is a dynamic vector of complete logical-tab
objects. Fixed masks and arrays are limited to the 16 physical presentation
hosts (eight cells times two layers), not the logical tab count. The current
public registration ceiling is a host policy, not a recovered native capacity.

| Public contract | Current bound/meaning |
|---|---|
| custom tabs per process | 1,024; host policy and memory/work bound, not native fixed-vector capacity |
| tab ID | provider-local stable ASCII, 1–255 bytes; only letters, digits, `.`, `_`, and `-` |
| global identity | `provider_id/tab_id` |
| title | 1–95 UTF-16 code units |
| icon | none/fallback or one RGBA8 image with alpha `0..255` |
| raster shape | exactly 32x32 row-major pixels, 4,096 bytes |
| client pixel lifetime | copied synchronously before registration returns |
| ordering | provider hint, then optional player override and deterministic stable-key tie break |
| page lifetime | provider-owned logical page; native presentation carrier is transient |

PNG/JPEG decoding, alternate dimensions, client stride, and client-owned
native resources are intentionally outside this contract. A client may decode
or generate pixels by any method and submit the canonical RGBA8 form.

## Related research

- [Configuration and OptionSetting class map](../architecture/configuration-class-map.md)
- [Scene-object bridge](../reference/scene-object-bridge.md)
- [Native hook inventory](../reference/native-hook-inventory.md)
- [Settings pages and pagination](settings-pages-and-pagination.md)
- [Research method](../methodology.md)
