# Historical build-locked native UI address map

Status: historical exact-build snapshot with current documentation provenance.

This directory preserves analytical symbols from one completed analysis of a
verified Windows `eldenring.exe`. Its analytical coordinates and evidence are
preserved from that snapshot; its `sources` entries have been redirected to
the current research tree. It is a navigation and provenance artifact, not a
public ABI, a current hook inventory, or a replacement for production pattern
resolution.

## Covered executable

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| File size | `87024720` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The machine-readable snapshot is
[`eldenring_2.7.0.0_known_symbols.csv`](eldenring_2.7.0.0_known_symbols.csv).
Its repeated `image_sha256` field lets the importer reject accidental use on
another program. `rva` always means `address - PE image base`.

## Reading the CSV

| Column | Meaning |
|---|---|
| `rva` | Observed address in the exact executable above. |
| `analytical_name` | ERNativeUI research label, not an original FromSoftware symbol. |
| `symbol_kind` and `category` | Navigation grouping for functions, data, vtables, and instruction anchors. |
| `confidence` | `confirmed`, `inferred`, `hypothesis`, or `rejected` under the [research vocabulary](../README.md#evidence-labels). |
| `production_role` | Whether current code hooks/calls the boundary or retains it only as explanatory context. |
| `apply_name` | Whether the exact-build Ghidra importer may create or rename the symbol. Documentary rows remain in the CSV but are not promoted. |
| `legacy_fallback_rva` | An older observed location retained for comparison; never the current-build RVA by implication. |
| `evidence` and `sources` | Concise reason for the label and its provenance. |

The recorded confidence describes the evidence available when each row was
written; it does not imply that `production_role` still matches the current
tree. For example, the snapshot predates parts of the complete production
inventories for ColorPicker and Input Bindings, and it describes the
TextInput row producer as research context. The `sources` field now points to
the current case study, architecture page, reference page, or source file
that preserves the supporting evidence. For current hook roles, use the
[native hook inventory](../reference/native-hook-inventory.md).

## RVA versus production derivation

- **Confirmed:** Every CSV RVA is build-locked to the hash above.
- **Confirmed:** Current production patterns and validators live beside the
  implementation, primarily in
  the current online [`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp) and feature-local native
  backends.
- **Confirmed:** The main resolver scans executable sections, rejects zero or
  multiple matches, can decode a relative call from a distinctive call-site
  pattern, and validates any fallback before accepting it.
- **Inferred:** A semantic AOB that survives several builds is a stronger
  derivation than a raw RVA, but it still needs new-build caller, argument,
  ownership, and live-behavior validation.
- **Rejected:** An RVA is safe because nearby functions kept similar
  addresses, or an AOB is portable merely because it matches once.

Some native features intentionally add a complete-build gate when short bytes
cannot prove retained object layout or callback ownership. That is a valid
fail-closed design, not a resolver failure.

Several constants in `src/addresses.cpp` deliberately name fallback RVAs from
an earlier executable revision. For those core rows, a difference between the
constant and this CSV's `rva` is not automatically a contradiction: the CSV's
`legacy_fallback_rva` records that relationship, while the runtime AOB chooses
and logs the observed address. The unresolved documentation problem is
coverage and role drift in newer feature-local backends, not evidence that
every differing number is wrong.

## Apply the map to Ghidra

The normal [whole-program import](../tools/ghidra-workflow.md#first-import)
applies this historical seed set automatically. This is safe only because the
importer verifies the exact executable hash; it does not make the snapshot a
current production inventory. To apply it manually to an already analyzed
copy of that exact executable:

```powershell
& 'C:\Tools\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat' `
  'C:\path\to\research-work\ghidra-project' `
  'ERNativeUI-EldenRing' `
  -process 'eldenring.exe' `
  -noanalysis `
  -scriptPath '.\tools\research\ghidra' `
  -postScript ApplyKnownSymbols.java `
    '.\docs\research\address-map\eldenring_2.7.0.0_known_symbols.csv'
```

`ApplyKnownSymbols.java` verifies the imported executable SHA-256, applies
only eligible rows, and adds an evidence plate comment. Reapplying a revised
map refreshes that plate without claiming documentary rows as functions.

## Add support for another game build

Do not rewrite this CSV and do not copy its RVAs into a new build map.

1. Record the complete new PE identity.
2. Resolve the production AOB derivations and inspect every missing or
   ambiguous result.
3. Compare function bounds, instructions, callers/callees, call targets,
   vtables, object offsets, and cleanup behavior.
4. Repeat the bounded in-game procedure for each changed native boundary.
5. Add a new CSV named for that product version with its own repeated hash.
6. Keep inferred/rejected rows documentary (`apply_name=false`).
7. Update the importer selection without weakening either hash guard.

The full safety and promotion criteria are in the
[research methodology](../methodology.md). The CSV's current source links and
the [native hook inventory](../reference/native-hook-inventory.md) provide the
corresponding evidence and production context.
