# Scene-object proxy and native Scaleform bridge

This reference maps the narrow Elden Ring wrapper family that resolves a
named scene object into a referenced Scaleform value and performs common
TextField operations. It explains the ownership contract behind ERNativeUI's
existing title and ColorPicker presentation paths. It is not a generic
Scaleform API.

## Reference build and evidence boundary

All addresses and offsets on this page belong to this exact Windows image:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

Names beginning with `ERUI_` in the symbol map are analytical names. The work
was reconstructed from executable bytes, PE unwind ranges, RTTI, bounded call
graphs, and controlled live results. RVAs and native layouts are private,
build-locked evidence rather than public ABI.

- **Confirmed** means an exact field access, call shape, vtable relation, or
  controlled live result establishes the behavior.
- **Inferred** means the behavior is strongly supported but its exact
  Scaleform SDK property name is not proven.
- **Rejected** records a bounded negative result.

## Confirmed object layout

RVA `0x733F00` initializes the `ComponentProxy` base prefix. It installs the
`CS::ComponentProxy` vtable at `0x2A92B88` and writes the object address to
`+0x08`, `+0x10`, and `+0x18`.

Derived constructors in the `0x74B140` family replace `+0x00` with the
`CS::SceneObjProxy` vtable at `0x2A97AF0`, copy the owner at `+0x20`, and
construct an embedded `CSScaleformValue` at `+0x28`. Arrays of completed
objects advance by `0x60` bytes:

```text
SceneObjProxy (0x60 bytes)
+0x00  SceneObjProxy vtable after derived construction
+0x08  self link 0
+0x10  self link 1
+0x18  self link 2
+0x20  owner/context pointer
+0x28  CSScaleformValue (0x38 bytes)
       +0x00 wrapper vtable
       +0x08 GFx::Value storage begins
             +0x10 object interface / reference owner
             +0x18 type and ownership flags
             +0x20 object payload
```

RVA `0x74C930` and `0x74C940` are one-instruction accessors that return
`this + 0x28`. Operation wrappers follow a self-link, dispatch proxy virtual
slot `+0x08`, and receive that embedded value. ERNativeUI must therefore keep
and destroy the complete proxy result; copying only the GFx payload does not
retain its owner safely.

## Path and member resolution

| RVA | Confirmed behavior |
|---:|---|
| `0x74B140` | Public variadic path-formatting and resolution entry used by the game and ERNativeUI. A null or empty format copies the source proxy. |
| `0x74BA10` | Thin argument-order adapter for `0x74B610`. |
| `0x74B610` | Constructs a `0x60`-byte result, copies owner `+0x20`, obtains the path characters, and calls `0xD81710` for the embedded value. |
| `0xD81710` | Splits a path at `/`, resolves each component through `0xD81680`, and releases intermediate values. |
| `0xD81680` | Direct object-member lookup for GFx kinds `8..11`; calls object-interface virtual slot `+0x20`. |
| `0xD81590` | `CSScaleformValue` destructor; restores the wrapper vtable and releases owned GFx storage when its ownership flag is set. |

The `+0x20` object-interface call is a GetMember-equivalent operation. It is
not a `SceneObjProxy` virtual slot; it belongs to the object interface stored
inside the embedded GFx value.

### Variadic contract

The entry code at `0x74B140` saves `R9`, creates a `va_list` beginning there,
and passes the format plus that list into two formatting calls. Calls with
literal paths and no conversion directives happened to work through an
earlier three-register typedef on Microsoft x64, but that typedef was narrower
than the native contract.

A reusable wrapper must either preserve the variadic call shape or accept
only validated literal paths without format directives. It must never expose
this address as an arbitrary plain three-argument function.

### Component-length precondition

`0xD81710` copies every non-final path component into a fixed local buffer. A
component preceding a slash must be at most `0x20` bytes; a longer component
reaches `__report_rangecheckfailure`. The final component is passed directly.
This is a native precondition, not a suggested public ERNativeUI string limit.

## Specialized TextField operations

The complete wrapper cluster at `0x74AE50..0x74B0F3` was inspected. Each
function validates the embedded GFx kind and delegates to one lower-level
implementation.

| Proxy wrapper | Low-level target | Evidence | Operation |
|---:|---:|---|---|
| `0x74AE50` | `0xD85FE0` | **Confirmed** | Assign UTF-16 text through object-interface slot `+0x128`. |
| `0x74AEA0` | `0xD81EF0` | **Confirmed** | Test whether horizontal text overflow is positive. |
| `0x74AED0` | `0xD81F10` | **Confirmed** | Test whether current vertical scroll is below its maximum. |
| `0x74AF00` | `0xD85950` | **Inferred** | Toggle a TextField-specific flag used before language-text assignment; exact SDK name unresolved. |
| `0x74AF50` | `0xD850F0` | **Inferred** | Return a line/viewport quantity whose default is `1`; exact SDK name unresolved. |
| `0x74AF90` | `0xD843A0` | **Confirmed** | Return maximum horizontal scroll; geometry converts twips with `0.05f`. |
| `0x74AFC0` | `0xD84450` | **Confirmed** | Return maximum vertical scroll. |
| `0x74AFF0` | `0xD85150` | **Confirmed** | Return current vertical scroll. |
| `0x74B020` | `0xD85610` | **Confirmed** | Assign packed text color after byte reordering into normalized channels. |
| `0x74B080` | `0xD85F60` | **Confirmed** | Assign horizontal scroll after multiplying by `20.0f` for twips. |
| `0x74B0C0` | `0xD86080` | **Confirmed** | Assign vertical scroll and invalidate the TextField. |

The scroll meanings have independent call-site evidence. One native controller
reads `0x74AFC0` and `0x74AFF0`, clamps a new value, writes it through
`0x74B0C0`, and reads it again. Another uses `0x74AF90` as the bound for
changes applied through `0x74B080`.

## Bounded negative result

The enumerated `0x74AE50..0x74B100` cluster contains no general Invoke,
SetMember, CreateObject, CreateArray, or AttachMovie equivalent. The only
generic object operation proved here is GetMember through object-interface
slot `+0x20`; the rest are specialized TextField operations.

This does **not** prove that Elden Ring lacks the other Scaleform operations.
It means they must be found in another wrapper family or on the movie/player
interface, with a real game owner and lifetime. Address proximity is not
evidence of semantic proximity.

## Production consequences

- A resolved scene object is an owning `0x60`-byte proxy, not a raw
  `GFx::Value`.
- Slash-separated member traversal owns and releases every intermediate
  complex value.
- Direct UTF-16 text assignment is a proven narrow operation.
- Resolved values must not be cached across page or movie replacement.
- Arbitrary ActionScript invocation and runtime display-object creation remain
  unsupported until their owner, call shape, thread, and teardown are proven.

Current production resolution is recorded in the
[native hook inventory](native-hook-inventory.md); page-title usage is covered
by [Settings pages and pagination](../case-studies/settings-pages-and-pagination.md),
and the fill-color path by the [ColorPicker case study](../case-studies/color-picker.md).

## Reproduce or extend the finding

1. Import the exact reference executable with the
   [Ghidra workflow](../tools/ghidra-workflow.md).
2. Begin at `0x74B140`, `0x74B610`, `0xD81710`, and `0xD81680`; verify
   function bounds and callers before assigning names.
3. Validate the `SceneObjProxy` vtable and full `0x60`-byte result at each
   call site, including matching destruction through `0xD81590`.
4. For every proposed wrapper, follow its GFx kind checks and object-interface
   virtual target rather than inferring from its neighboring RVA.
5. Use a bounded live test on one known existing TextField. Never retain the
   proxy after the owning page can close.
6. Record exact executable identity, call context, ownership, and failure
   behavior before promoting another operation.
