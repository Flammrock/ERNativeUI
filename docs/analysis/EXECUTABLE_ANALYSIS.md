# Elden Ring executable analysis

Status: initial whole-program import complete; focused UI reconstruction in
progress

This investigation builds a local whole-program Ghidra database for navigation
and then documents the Game Options and Scaleform subsystems in focused maps.
The database is an analysis aid: decompiled output is not the original C++ and
automatically inferred function boundaries, signatures, and types require
validation.

## Tested executable

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| File size | `87024720` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The executable is read directly for static analysis and is never patched by
this workflow. Its path and the generated Ghidra project remain local.

## Completed baseline import

The exact executable above completed the conservative whole-program analysis
successfully. The saved local project is reusable for bounded read-only
queries, so later investigations do not repeat the multi-hour import.

| Artifact | Result |
|---|---:|
| Ghidra analysis wall time | `4152` seconds |
| Indexed function rows | `503655` |
| Indexed string-reference rows | `123654` |
| Reviewed analytical names applied during import | `66` |
| Documentary-only names skipped during import | `3` |
| Latest curated-map synchronization | `98` applied, `4` documentary, `0` failed |

These counts describe Ghidra's analytical database and exporters, not original
FromSoftware source functions or symbols. The local database and raw indexes
remain ignored under `research-work/`; only curated evidence and reproducible
query scripts belong in the repository.

The focused query wrapper can now export bounded anchors, call graphs,
decompilations, function context, callsites, vtables, RTTI inventories, and
exact-base RTTI hierarchies. See
[`tools/research/README.md`](../../tools/research/README.md) for the commands
and concurrency rule.

## Canonical toolchain

- Ghidra 12.1.3 official public release
- 64-bit Java Development Kit (JDK 21 or newer accepted by that release)
- GNU `objdump` and `strings` as independent section/string checks
- ERNativeUI's own runtime evidence and validated address signatures

The Ghidra release archive must match the upstream SHA-256:

```text
93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54
```

## Local versus tracked artifacts

The following stay under the ignored `research-work/` directory or another local
directory and must not be committed:

- `eldenring.exe` or any copy of it;
- Ghidra project/database files;
- full disassembly or bulk decompiler output;
- raw game strings or extracted game assets;
- temporary logs and caches.

The repository may contain our original automation, names, signatures,
function/RVA maps, call relationships, object-layout conclusions, experiment
records, and concise pseudocode written to explain behavior.

## Analysis order

1. Import the complete PE and run standard Ghidra analysis.
2. Export the initial function and string-reference indexes.
3. Seed names for every interface already proved by ERNativeUI.
4. Follow callers and callees around the Game Options hub/subpage handlers,
   row constructors, Scaleform helpers, page frame, Back, popup choice, and
   native dialog paths.
5. Follow cross-references from the TextInput widget paths toward the same
   graph.
6. Recover RTTI, vtables, constructors, destructors, event dispatch, focus,
   and ownership incrementally.
7. Promote only validated discoveries into production signatures and hooks.

Every renamed function uses an analytical name, not a claim about
FromSoftware's original source identifier. Each published entry records a
confidence level and its evidence.

## Running the import

After extracting Ghidra, run from the repository root:

```powershell
tools/research/analyze_elden_ring.ps1 `
  -GhidraHome 'C:\Users\you\Tools\ghidra_12.1.3_PUBLIC' `
  -GameExe 'C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game\eldenring.exe'
```

The default database and exports are created below `research-work/`, which is
ignored. A first whole-program analysis can take a long time and substantial
disk space. Later runs can process the existing project without reimporting.
