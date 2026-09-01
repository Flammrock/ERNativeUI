# Known native UI symbol map

This directory contains curated analytical symbols for the verified Windows
`eldenring.exe` build. These names are ERNativeUI research labels, not original
FromSoftware identifiers and not stable client-facing APIs.

## Build identity

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |
| File size | `87024720` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |

The exhaustive machine-readable map is
[`eldenring_2.7.0.0_known_symbols.csv`](eldenring_2.7.0.0_known_symbols.csv).
Its repeated `image_sha256` field makes accidental application to another game
build fail closed. `rva` always means `runtime address - module image base`.

## What is included

The map seeds the boundaries that ERNativeUI has already established through
validated signatures, static structure, and controlled live tests:

- Game Options root/subpage construction, native page push/pop, and frame input;
- toggle, slider, inline-choice, popup-choice, and action-row construction;
- text-reference construction, custom text resolution, and cleanup;
- Scaleform path/value operations plus the confirmed GFX-resource,
  MovieDef/player, frame-step, and teardown lifecycle boundaries;
- popup-choice callable functions and vtables;
- the Configuration top dialog, fixed category list, nested tab dispatcher,
  composite panel manager, and erased option-panel factory;
- generic-dialog construction, task ownership, polling, input, and retirement;
- the statically recovered TextInput producer/controller/dialog, input-gate,
  Win32-message translation, and movie-event boundaries;
- the known Back/intrusive-task path, its base-slot-2 queue consumer, and
  decisive instruction-level call-site anchors.

`production_role` distinguishes functions ERNativeUI calls or hooks from
context retained only to explain the path. `apply_name=false` intentionally
prevents incomplete or rejected interpretations from becoming authoritative
Ghidra symbols. Confidence uses the vocabulary defined by the parent
[analysis index](../README.md).

Several `src/addresses.cpp` constants are validated fallback RVAs from the
previous supported executable. The `rva` column is the observed address in the
specific build above; `legacy_fallback_rva` preserves the older value without
letting the Ghidra importer confuse it with the current entry.

The dialog descriptor helpers are not hard-coded in production. Their current
entries were re-derived, read-only, from the exact AOBs and rel32 offsets in
`src/native_dialog.cpp` against the executable hash above. This independently
reproduced the logged transport entries (`0x7AA2E0`, `0x7EF6D0`, `0x7AE040`)
and resolved the descriptor initializer/setters, task builder, reference-count
release function, and frontend-manager global recorded in the CSV. These are
still build-specific observations, not permission to replace semantic scans
with raw RVAs.

## Apply to Ghidra

After importing and analyzing the exact executable, run:

```powershell
& 'C:\path\to\ghidra\support\analyzeHeadless.bat' `
  'C:\path\to\research-work\ghidra-project' `
  'ERNativeUI-EldenRing' `
  -process 'eldenring.exe' `
  -noanalysis `
  -scriptPath '.\tools\research\ghidra' `
  -postScript ApplyKnownSymbols.java `
    '.\docs\analysis\address-map\eldenring_2.7.0.0_known_symbols.csv'
```

The script verifies the imported executable SHA-256, renames or creates known
function entries, labels confirmed data/vtables/instruction anchors, and adds
an evidence plate comment. Reapplying an updated map refreshes the ERNativeUI
evidence block while preserving unrelated analyst comments. Rows marked
`apply_name=false` remain documentary only.

## Maintenance rule

Do not copy these RVAs to a new game build. First resolve the production AOBs,
diff callers/callees and object offsets, repeat the bounded behavioral test,
and then add a new build-specific CSV. Preserve this file so older analysis
databases remain reproducible.
