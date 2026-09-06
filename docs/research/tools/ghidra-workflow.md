# Reproduce the Ghidra analysis

ERNativeUI's research scripts build a local, read-only navigation database for
the Elden Ring executable and export only bounded queries. They do not modify
or launch the game.

Read the [research methodology](../methodology.md) before interpreting a
decompilation or promoting an address. Ghidra output is analytical evidence,
not recovered original C++ source.

## Requirements

- Windows PowerShell
- Ghidra `12.1.3` from the official public release
- a 64-bit JDK accepted by that release (JDK 21 or newer was used)
- a legally installed Windows `eldenring.exe`
- substantial free disk space and time for the first whole-program import

The Ghidra archive used for the recorded baseline had SHA-256:

```text
93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54
```

Verify the game input separately. The import helper accepts the
[reference executable](../README.md#reference-build) by default and rejects a
different SHA-256 unless `-AllowUnknownExecutable` is given deliberately.

## Local artifacts

By default, everything generated goes below the ignored `research-work/`
directory:

```text
research-work/
|-- ghidra-project/
|-- exports/
|   |-- functions.csv
|   `-- string_references.csv
|-- logs/
`-- queries/
```

Do not commit the executable, the Ghidra project, full disassembly,
decompiler dumps, extracted assets, or raw indexes. Commit only curated facts,
original scripts, derived signatures, and bounded evidence summaries.

## First import

From the repository root:

```powershell
.\tools\research\analyze_elden_ring.ps1 `
  -GhidraHome 'C:\Tools\ghidra_12.1.3_PUBLIC' `
  -GameExe 'C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game\eldenring.exe'
```

The defaults allow an 8 GB Java heap, at most 12 logical processors, and six
hours for this PE. They are upper bounds and can be changed:

```powershell
.\tools\research\analyze_elden_ring.ps1 `
  -GhidraHome 'C:\Tools\ghidra_12.1.3_PUBLIC' `
  -GameExe 'C:\Games\ELDEN RING\Game\eldenring.exe' `
  -HeadlessMaxMemory 12G `
  -MaxCpu 8 `
  -AnalysisTimeoutSeconds 28800
```

The import performs four important operations:

1. verifies the executable hash;
2. applies the conservative large-PE analyzer profile;
3. applies the exact-build historical
   [seed symbol map](../address-map/eldenring_2.7.0.0_known_symbols.csv);
4. exports a function index and a string-reference index.

The seed map is useful for navigation on that hash. Its documentation sources
have been reconciled with the current research tree, but its coordinates,
confidence, and production-role fields remain a historical analysis snapshot.
Use the [current native hook inventory](../reference/native-hook-inventory.md)
and current resolver source when production behavior differs.

The recorded baseline completed in 4,152 seconds and produced 503,655
function rows and 123,654 string-reference rows. Those are Ghidra analysis
counts, not original FromSoftware function or symbol counts, and they are not
performance promises for another computer.

Use `-Reimport` only for a deliberately disposable/rebuildable local project;
it passes overwrite to the import. Prefer a separate `-ResearchRoot` for a new
game hash so the old database remains reproducible.

## Conservative large-PE profile

`ConfigureLargePeAnalysis.java` keeps the ordinary PE, x64, RTTI, exception,
demangler, string, reference, function-start, stack, and decompiler analysis
passes. It explicitly disables:

- discovered non-returning-function inference;
- its flow-damage repair cascade;
- aggressive instruction finding; and
- function-start searches inside data blocks.

This trades some localized unreachable tail code for protection against a
false heuristic no-return decision rewriting many optimized callers. The
profile is for a fresh import; it cannot undo damage already persisted in an
older database.

- **Confirmed:** The reference image contains a second executable section
  named `.text` at RVA `0x4C13000`, but all curated UI RVAs are in the first
  executable block.
- **Rejected:** Removing that second block from the default whole-program
  analysis. Its purpose is unresolved, and omitting it could hide indirect
  helpers or cross-section relationships.

A separate UI-only triage database may mark it non-executable only when the
exact hash, section range, and omission are recorded in every result.

## Run focused read-only queries

The query wrapper opens the existing program with `-readOnly -noanalysis`.
Run one query process at a time: Ghidra takes an exclusive project lock even
for this headless read-only use.

```powershell
$ghidra = 'C:\Tools\ghidra_12.1.3_PUBLIC'
```

Every query address is an RVA. Bare values and values beginning with `0x` or
`rva:` are interpreted as hexadecimal.

### Find strings, symbols, and RTTI anchors

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Anchors `
  -ScriptArgument @(
    'research-work\queries\text-input\anchors.csv',
    '50',
    '100',
    'Widgets/TextInput',
    'TextInput',
    'vftable'
  )
```

The two numeric limits cap matches per term and references per match. Inspect
the output's truncation field before treating an absence as meaningful.

### Inspect a small function set

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query FunctionContext `
  -ScriptArgument @(
    'research-work\queries\open-subpage\context.md',
    '160',
    '200',
    '0x950850',
    '0x959DF0'
  )
```

This exports prototypes, direct callers/callees, outgoing references, and a
bounded instruction prefix. The RVAs in this example belong only to the
reference build.

### Decompile only explicit functions

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Decompile `
  -ScriptArgument @(
    'research-work\queries\open-subpage\decompiled.md',
    '120',
    '250000',
    '0x950850'
  )
```

The timeout and output-character limit apply per function. Generated
parameters, types, structure fields, and control flow still need instruction
and call-site validation.

### Expand a bounded call graph

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query CallGraph `
  -ScriptArgument @(
    'research-work\queries\open-subpage\callgraph',
    'both',
    '2',
    '250',
    '0x950850'
  )
```

Depth and node limits are mandatory. Utility, allocator, or logging functions
can otherwise expand into a large part of the program.

### Recover class and virtual-call evidence

Use a tightly filtered RTTI inventory:

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query RttiClasses `
  -ScriptArgument @(
    'research-work\queries\ui-classes\rtti',
    '50', '8', '8', '128',
    'CS::MenuWindow',
    'TextInput',
    'Scaleform'
  )
```

Find descendants of an exact recovered base:

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query RttiHierarchy `
  -ScriptArgument @(
    'research-work\queries\ui-hierarchy',
    '500', '128',
    'CS::MenuWindow',
    'CS::MenuJob'
  )
```

Classify calls inside explicit seeds:

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Callsites `
  -ScriptArgument @(
    'research-work\queries\open-subpage\callsites',
    '5000', '1000', '0x800',
    '0x950850',
    '0x959DF0'
  )
```

A computed call shaped like `[object register + aligned displacement]` yields
only a probable virtual slot. Confirm the register origin, RTTI locator,
candidate vtable, and concrete call sites before naming the method. Multiple
inheritance can produce several locators/vtables; preserve the recorded object
displacement.

The wrapper also supports `Vtable` for a bounded slot inventory. The detailed
argument reference remains in the
[online tooling README](https://github.com/Flammrock/ERNativeUI/blob/main/tools/research/README.md).

## Productive investigation loop

1. Find a distinctive path, diagnostic, RTTI name, or known symbol with
   `Anchors`.
2. Read bounded instructions and references with `FunctionContext`.
3. Decompile only the promising functions and compare the assembly.
4. Expand one or two call-graph levels, not the whole shared runtime.
5. Validate class claims with RTTI, Complete Object Locators, constructor
   writes, and call sites.
6. Compare the candidate with the historical
   [address map](../address-map/README.md) and the current production resolver;
   the resolver wins if they differ.
7. Write a falsifiable live-probe plan before instrumenting the game.
8. Promote only the evidence level established by the result.

Generic Scaleform runtime landmarks such as EventDispatcher, focus-manager,
or timeline internals are poor primary hooks until a game-owned wrapper and
lifetime lead to them. A string match establishes a reference, not the
meaning of the containing function.

## Analyze a new game build

Do not use `-AllowUnknownExecutable` merely to make the script continue.

1. Record the new product version, hash, file size, PE timestamp, and image
   size.
2. Choose a new `-ResearchRoot` so the reference database is untouched.
3. Import the new program without applying the reference-build symbol CSV.
4. Resolve production AOBs, then compare function context and semantics before
   using old RVAs as navigation hints.
5. Create a new build-specific CSV only after entries have evidence labels.
6. Repeat bounded live tests and the release validation matrix.

The current helper always applies the reference symbol map after import. Its
hash guard correctly rejects a different executable, so
`-AllowUnknownExecutable` alone is not a complete new-build workflow. Until a
`-KnownSymbols` or no-seed option is added, use a separate manual Ghidra import
with the same conservative pre-script and bounded exporters, or make a
reviewed local-only copy of the wrapper that omits `ApplyKnownSymbols.java`.
Never remove or weaken the CSV hash guard.

The exact baseline, analyzer-profile rationale, and bounded-query procedure
are recorded on this page. Curated Scaleform and resource navigation anchors
are kept separately in the
[Scaleform runtime seeds](../reference/scaleform-runtime-seeds.md) and
[GFX resource catalog](../reference/gfx-resource-catalog.md).
