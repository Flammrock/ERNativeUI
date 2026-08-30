# Native popup-choice rows

This document records how ERNativeUI constructs the action-style setting row
used by Elden Ring for options such as Graphics quality. Selecting the row
opens a native list; this is distinct from ERNativeUI's inline choice, which
changes in place with left/right input.

The addresses below are observations from the Elden Ring build current during
the August 2026 investigation. They are maintenance evidence, not a promise
that future builds retain the same RVAs. `src/addresses.cpp` contains the
authoritative signatures and validates every fallback before installation.

## Recovered call path

The production row constructor is currently at RVA `0x95D010` and has this
five-argument shape:

```cpp
void* (__fastcall*)(
    void* page,
    void* text_references,
    void* value_provider,
    void* commit_action,
    void* presentation_provider);
```

The last three arguments are native type-erased callable objects. On the
tested MSVC x64 ABI each object occupies `0x40` bytes, stores its vtable at
`+0x00`, captured native state at `+0x08`, and its inline target/self pointer
at `+0x38`. The commit action also captures the physical page at `+0x10` so
the game can refresh it after a confirmed selection.

Observed callable identities:

| Role | Vtable RVA | Invoke RVA | Behavior |
|---|---:|---:|---|
| Read selected value | `0x2B15A78` | `0x962520` | Returns state byte `+1` |
| Commit selected value | `0x2B15AB0` | `0x962330` | Writes the event byte to state `+1`, then calls page vtable `+0xA0` |
| Build presentation list | `0x2B15AE8` | `0x9623A0` | Builds the popup's native option container |

ERNativeUI resolves the three invoke functions by executable signatures. It
then scans non-executable image sections for a callable vtable whose invoke
slot at `+0x10` points to the resolved function and validates the other slots
as executable. A current-RVA fallback is accepted only when the same
relationship validates.

## Native value conversion

The game uses item values `1..N`; ERNativeUI's C and C++ APIs expose indices
`0..N-1`. Each compiled popup row owns an independent 16-byte native state
buffer for the lifetime of the installed menu. The selected native byte is at
offset `+1`.

At each host poll:

1. A new public/programmatic value is normalized, converted to `value + 1`,
   and written to the native state.
2. Otherwise, a valid committed native value is converted to `value - 1` and
   published through the row's public byte.
3. Invalid native bytes are repaired without exposing an invalid index.
4. The value callback runs once only when the zero-based observed value
   changes. Closing the list without a change produces no callback.

This explicit synchronization is important. The research prototype persisted
visually because the native state survived page refreshes, but it did not yet
connect that state to public callbacks or `get_row_value`/`set_row_value`.

## Popup list layout

The list template initializer is currently RVA `0x86A610`. An additional
builder at RVA `0x94ED90` was observed during tracing; production does not call
that address directly.

The recovered container layout is:

| Field | Layout |
|---|---:|
| Container size | `0x920` bytes |
| First element | `+0x08` |
| Element stride | `0x48` bytes |
| Element native value | element `+0x08` |
| Element text reference | element `+0x10` |
| Element count | `+0x910` |
| Structural capacity | 32 elements |

The template creates valid native elements and their destructor/vtable
contract. ERNativeUI validates the complete template, releases its original
elements, clears the 32-slot storage, and reconstructs only the registered
options with custom text references. The new item values are one-based.

## Why two hooks remain

The native template initializer receives only the destination list and the
selected byte, so it cannot identify which registered row requested it. A
strictly scoped pair of hooks supplies that identity:

1. The presentation-provider hook reads its captured state pointer at `+0x08`
   and resolves it against ERNativeUI-owned compiled popup rows.
2. A thread-local scope records that exact row while the original provider
   executes.
3. The nested template hook calls the original initializer first, then
   substitutes labels only when that scope contains a registered row.
4. The previous thread-local scope is restored on return, including nested
   calls.

Vanilla Graphics selectors never match an ERNativeUI-owned state address, so
their lists remain untouched. All earlier passive constructor, input,
quality-row, and widget-method probes were removed from production.

## How the path was identified

The investigation deliberately advanced through bounded, removable probes:

1. Observe the vanilla Graphics quality row's action construction.
2. Follow its activation to the special selector constructor.
3. Identify the selected-value, commit, and presentation callables from the
   live widget.
4. Trace presentation into the list template and record its element layout.
5. Reproduce the constructor using per-row state and game vtables.
6. Gate list substitution by exact state identity, then verify that vanilla
   Graphics was unchanged.
7. Remove every discovery-only hook and retain only the minimal functional
   bridge described above.

No executable was patched for the production implementation.

## Validation evidence

Manual in-game testing confirmed:

- two-, four-, and eight-option registered lists;
- the original `Minimal`, `Balanced`, `Detailed`, `Maximum` four-item set;
- first/last selection and persistence after reopening;
- independent state across multiple popup rows;
- controller/mouse activation and normal Back behavior;
- unchanged French vanilla Graphics labels before and after custom use.

The 32-entry limit comes from the recovered native container and is enforced
by the API. It is structurally covered by unit tests; the sizes explicitly
listed above are the variants live-tested during discovery.

For nearby menu interfaces and the pagination/back-stack work, see
[Native addresses and pagination](NATIVE_ADDRESSES_AND_PAGINATION.md).
