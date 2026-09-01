# Scene-object proxy and native Scaleform bridge

This note maps the small Elden Ring wrapper family that turns a named scene
object into a reference-counted GFx value and performs common text-field
operations on it. The observations are build-specific to Elden Ring 2.7.0.0
(`PE timestamp 0x69E9C9B9`, image size `0x5E09600`). Names beginning with
`ERUI_` are analytical names, not recovered FromSoftware symbols.

The work was performed read-only from the executable bytes and PE unwind
ranges. The active Ghidra project was not opened or modified.

## Confirmed object layout

`0x733F00` initializes the `ComponentProxy` base prefix: it installs the
`CS::ComponentProxy` vtable at `0x2A92B88` and writes the object address to
`+0x08`, `+0x10`, and `+0x18`. The derived constructors in the `0x74B140`
family then replace offset `+0x00` with the `CS::SceneObjProxy` vtable at
`0x2A97AF0`, copy the owner at `+0x20`, and construct an embedded
`CSScaleformValue` at `+0x28`. Arrays of the completed objects advance by
`0x60`. The resulting layout is therefore:

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

Both `0x74C930` and `0x74C940` are one-instruction accessors returning
`this + 0x28`. The operation wrappers first follow the caller's self-link,
dispatch proxy vtable slot `+0x08`, and receive this embedded value. This
explains why ERNativeUI must retain and destroy the complete proxy result
rather than copying only its GFx payload.

## Path/member resolution

| RVA | Confirmed behavior |
|---|---|
| `0x74B140` | Public, `printf`-style variadic path-formatting and resolution entry used by the game and ERNativeUI. It formats a non-empty third argument, converts that path representation, and delegates construction to `0x74BA10`. A null or empty format copies the source proxy. |
| `0x74BA10` | Thin argument-order adapter for `0x74B610`. |
| `0x74B610` | Constructs a `0x60`-byte result, copies the source owner, obtains the path character pointer, and calls `0xD81710` for the embedded value. |
| `0xD81710` | Splits a path at `/`, resolves each component through `0xD81680`, and releases intermediate GFx values. |
| `0xD81680` | Direct object-member lookup. It accepts GFx kinds `8..11`, passes source object payload, member name, destination GFx storage, and the display-object boolean to object-interface virtual slot `+0x20`. |
| `0xD81590` | `CSScaleformValue` destructor: restores its wrapper vtable and releases owned GFx storage through the object interface when the ownership flag is set. |

The `+0x20` object-interface slot is thus a GetMember-equivalent boundary.
It is not a vtable slot on `SceneObjProxy`; it belongs to the object interface
stored inside the embedded GFx value.

The variadic contract at `0x74B140` is visible in its entry code: it saves
`R9`, builds a `va_list` beginning there, and passes the format plus that list
to two formatting calls. ERNativeUI's current three-register call has worked
on the tested Microsoft x64 build because its paths contain no conversion
directives, but the production function-pointer typedef is narrower than the
recovered native signature. A future generic wrapper must expose the variadic
contract or reject format directives rather than treating this as an
arbitrary plain-string function.

`0xD81710` also has a confirmed fixed-buffer constraint. Every path component
that precedes a slash is copied into a local buffer and must be at most
`0x20` bytes; a longer non-final component reaches
`__report_rangecheckfailure`. The final component is passed directly. This is
a native precondition, not a suggested public ERNativeUI limit.

## Direct operation wrappers

The complete `0x74AE50..0x74B0F3` cluster was inspected. Each wrapper validates
the embedded GFx kind and then delegates to one low-level implementation.

| Proxy wrapper | Low-level target | Status | Operation |
|---|---|---|---|
| `0x74AE50` | `0xD85FE0` | Confirmed | Assign UTF-16 text. The low-level function converts the string and dispatches object-interface slot `+0x128`. |
| `0x74AEA0` | `0xD81EF0` | Confirmed | Test whether horizontal text overflow is positive. |
| `0x74AED0` | `0xD81F10` | Confirmed | Test whether current vertical scroll is below maximum vertical scroll. |
| `0x74AF00` | `0xD85950` | Inferred | Toggle a TextField-specific flag. Language-text call sites use it before assigning text, but the exact SDK property is not yet proven. |
| `0x74AF50` | `0xD850F0` | Inferred | Return a TextField line/viewport quantity (default `1`). The exact public-SDK name is not yet proven. |
| `0x74AF90` | `0xD843A0` | Confirmed | Return maximum horizontal text scroll. Geometry is converted from twips with `0.05f`. |
| `0x74AFC0` | `0xD84450` | Confirmed | Return maximum vertical text scroll. |
| `0x74AFF0` | `0xD85150` | Confirmed | Return current vertical text scroll. |
| `0x74B020` | `0xD85610` | Confirmed | Assign packed text color after byte reordering; the generic path reads/writes four normalized color channels. |
| `0x74B080` | `0xD85F60` | Confirmed | Assign horizontal text scroll. The value is multiplied by `20.0f` before entering the twip-based TextField routine. |
| `0x74B0C0` | `0xD86080` | Confirmed | Assign vertical text scroll and invalidate the TextField. |

The vertical-scroll interpretation is independently supported by a native
controller call site that reads `0x74AFC0` and `0x74AFF0`, clamps a new value,
writes it through `0x74B0C0`, and reads the result again. A separate call site
uses `0x74AF90` as the bound for changes made through `0x74B080`.

## Invoke, SetMember, and CreateObject boundary

No Invoke, SetMember, CreateObject, CreateArray, or AttachMovie equivalent is
present in the fully enumerated `0x74AE50..0x74B100` wrapper cluster. The only
generic object operation proved here is GetMember through object-interface
slot `+0x20`; the remaining calls are specialized TextField text, color, and
scroll operations.

That negative result is deliberately bounded. It does **not** imply the game
lacks those Scaleform operations. They must be sought in other wrapper
families or at the movie/player interface, using call-site shapes and value
lifetime behavior rather than assuming adjacent RVAs have adjacent semantics.

## Practical consequences for ERNativeUI

- A resolved scene object is an owning `0x60`-byte proxy, not a raw
  `GFx::Value`.
- Member paths may be resolved component-by-component with `/`; every
  intermediate value has explicit reference management.
- Direct text assignment is already a proven, narrow operation suitable for
  production use.
- Arbitrary ActionScript invocation and runtime display-object creation remain
  unsupported until their actual movie/object interfaces and ownership rules
  are recovered.
