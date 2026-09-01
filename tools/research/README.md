# ERNativeUI research tooling

These scripts create a local static-analysis database and compact indexes for
ERNativeUI research. They do not modify or launch Elden Ring.

Requirements and the artifact policy are documented in
`docs/analysis/EXECUTABLE_ANALYSIS.md`.

## Whole-program import

`analyze_elden_ring.ps1` verifies the known executable hash by default, invokes
Ghidra's headless analyzer, and runs the scripts in `ghidra/`. Generated files
go to the ignored `research-work/` directory. Ghidra does not permit a project
path component beginning with `.`, so this directory intentionally has a
normal visible name.

The default whole-program run allows an 8 GB Java heap, uses at most 12 logical
processors, and gives the single PE up to six hours. These are upper bounds,
not preallocated resources, and can be changed with `-HeadlessMaxMemory`,
`-MaxCpu`, and `-AnalysisTimeoutSeconds`.

Use `-AllowUnknownExecutable` only after recording and reviewing a new game
build. Never copy the executable into the repository.

The initial exporters deliberately produce structural indexes rather than a
bulk decompiler dump:

- `functions.csv`: discovered entry RVA, analytical name, size, thunk flag,
  and symbol source;
- `string_references.csv`: defined string RVA/value and code/data references.

These raw indexes remain local until a curated subset is suitable for the
public address map.

Before exporting, the initial import applies the exact-build curated symbols
from `docs/analysis/address-map/`. The seed script verifies the imported PE's
SHA-256 and skips documentary entries that are inferred or rejected.

`ghidra/ApplyKnownSymbols.java` applies the reviewed build-specific map under
`docs/analysis/address-map/` after verifying the imported executable SHA-256.
It deliberately ignores documentary rows whose interpretation is incomplete
or rejected.

## Focused queries

After the initial import, use `run_ghidra_query.ps1` to inspect a small set of
anchors or functions without repeating auto-analysis. The wrapper opens the
existing program with `-readOnly -noanalysis`; query scripts cannot persist
changes to the database.

Run only one query process against a project at a time. Ghidra still takes an
exclusive project lock while a headless process has a read-only program open,
so concurrent queries against the same project fail instead of improving
throughput.

By default the wrapper uses `research-work/ghidra-project`, project
`ERNativeUI-EldenRing`, and program `eldenring.exe`. Its optional
`-ProjectDirectory`, `-ProjectName`, and `-ProgramName` parameters support a
separate test database without changing those production defaults.

Every address accepted by these scripts is an image-relative virtual address
(RVA). A bare value such as `950850` is hexadecimal, as are values prefixed by
`0x` or `rva:`. Outputs retain RVAs even though Ghidra internally displays
addresses rebased at the PE image base.

The examples below assume:

```powershell
$ghidra = 'C:\Users\flamm\Tools\ERNativeUI-research\ghidra_12.1.3_PUBLIC'
```

### Find string, symbol, RTTI, and vftable anchors

`Anchors` searches defined strings and symbol names case-insensitively. For
each bounded match it records a bounded number of direct references and their
containing source functions. This is the normal first step for paths such as
`Widgets/TextInput` and for RTTI names or Ghidra-created `vftable` symbols.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Anchors `
  -ScriptArgument @(
    'research-work\queries\text-input\anchors.csv',
    '50',  # maximum matches for each query
    '100', # maximum retained references for each match
    'Widgets/TextInput',
    'TextInput',
    'vftable'
  )
```

Output columns distinguish the anchor address from the referencing instruction
and its containing function. `references_truncated` is set when the per-match
reference cap prevented a larger result from being retained.

### Export a bounded call graph

`CallGraph` walks `callers`, `callees`, or `both` from one or more known
function RVAs. Depth and node limits are mandatory safeguards against utility
or allocator functions expanding into most of the program.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query CallGraph `
  -ScriptArgument @(
    'research-work\queries\open-subpage\callgraph',
    'both',
    '2',   # maximum distance from each seed
    '250', # maximum distinct functions for each seed
    '0x950850'
  )
```

The output directory contains `nodes.csv` and `edges.csv`. An edge marked
`caller` is still oriented in execution order (`caller -> current`); the label
describes how traversal discovered it.

### Inspect assembly, references, and immediate neighbors

`FunctionContext` writes a Markdown report containing Ghidra's current
prototype, direct callers and callees, bounded outgoing references, and the
first instructions of each requested function.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query FunctionContext `
  -ScriptArgument @(
    'research-work\queries\open-subpage\context.md',
    '160', # maximum instructions per function
    '200', # maximum entries in each reference section
    '0x950850',
    '0x959DF0'
  )
```

### Decompile selected functions

`Decompile` emits C-like pseudocode only for explicit function RVAs. The
timeout applies separately to each function. Generated names, parameter types,
and control flow are analytical results, not FromSoftware's original C++.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Decompile `
  -ScriptArgument @(
    'research-work\queries\open-subpage\decompiled.md',
    '120', # seconds per function
    '250000', # maximum output characters per function
    '0x950850'
  )
```

### Resolve a candidate vftable

After an anchor query identifies the RVA of a Ghidra `vftable` symbol,
`Vtable` treats that address as the first virtual slot. It reports the possible
MSVC CompleteObjectLocator pointer at `vftable - pointer_size` and resolves a
bounded number of slots to functions. Two consecutive unresolved slots stop a
candidate early.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Vtable `
  -ScriptArgument @(
    'research-work\queries\text-input\vtable.csv',
    '128', # maximum slots per candidate
    '0x1234560'
  )
```

A vftable report is evidence, not automatic proof of a class boundary. Confirm
the CompleteObjectLocator/type descriptor, constructor writes, and virtual
call sites before documenting a class layout as established.

### Inventory matching MSVC RTTI classes

`RttiClasses` follows the MSVC RTTI structures that Ghidra recovered. It groups
matching type descriptors and class namespaces, records their Complete Object
Locators, follows locator metadata references to candidate vftables, and emits
a bounded slot inventory. A vftable found through a locator reference is
distinguished from the weaker case of a `vftable` symbol found only in the same
class namespace.

Every output dimension is explicitly capped. Filters are case-insensitive
substrings matched against the class namespace, type-descriptor symbols and
decorated type name, and locator symbols. Use the literal `*` only when a
small, deliberately bounded cross-section of all recovered classes is wanted.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query RttiClasses `
  -ScriptArgument @(
    'research-work\queries\ui-classes\rtti',
    '50',  # maximum classes retained for each filter
    '8',   # maximum Complete Object Locators per class
    '8',   # maximum vftables per class
    '128', # maximum virtual slots per vftable
    'CS::MenuWindow',
    'TextInput',
    'Scaleform'
  )
```

The output directory contains `classes.csv`,
`complete_object_locators.csv`, `vftables.csv`, `virtual_slots.csv`, and
`queries.csv`. The last file reports whether a class filter was truncated.
`vftable_symbol_recovered=false` means that the table address came from a real
locator reference but Ghidra did not recover a `vftable` label there. Treat
namespace-only associations and unresolved slots as leads, not class-layout
proof.

### Find classes derived from an RTTI base

`RttiHierarchy` parses the bounded MSVC class-hierarchy descriptors associated
with recovered Complete Object Locators. It is useful when a derived class's
name does not contain the base name—for example, enumerating dialogs whose
RTTI lists `CS::MenuWindow` as a base. It also records PMD displacement fields
and the vftable associated with each object locator.

Base filters are exact recovered RTTI namespace names, not substring searches.
This prevents unrelated template arguments such as
`std::_Func_base<CS::MenuWindow*>` from being reported as subclasses.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query RttiHierarchy `
  -ScriptArgument @(
    'research-work\queries\ui-hierarchy',
    '500', # maximum matching object locators for each filter
    '128', # maximum base descriptors retained per class
    'CS::MenuWindow',
    'CS::MenuJob'
  )
```

The output describes recovered RTTI metadata, not safe constructors or public
interfaces. A class can have more than one locator/vftable because of multiple
inheritance; preserve the reported object displacement instead of collapsing
those records blindly.

### Classify native callsites in seed functions

`Callsites` records all direct and computed call instructions in explicit seed
functions. For computed memory calls, it also recognizes the conservative x64
shape `[one object register + aligned displacement]` and reports the
displacement divided by the pointer size as a *probable* virtual slot. This is
not a data-flow proof: confirm the register's origin and a matching vftable
before assigning a class or method name.

```powershell
.\tools\research\run_ghidra_query.ps1 `
  -GhidraHome $ghidra `
  -Query Callsites `
  -ScriptArgument @(
    'research-work\queries\open-subpage\callsites',
    '5000', # maximum decoded instructions per seed function
    '1000', # maximum retained calls per seed function
    '0x800', # largest displacement eligible for probable-slot classification
    '0x950850',
    '0x959DF0'
  )
```

The output directory contains `calls.csv` and `seeds.csv`. Direct targets are
reported from Ghidra's flow and call references. Indirect register and memory
calls remain in the report even when there is no statically resolved target.
Stack-frame and instruction-pointer-relative memory calls are never classified
as probable virtual dispatch.

## Query workflow

A productive investigation normally follows this loop:

1. Find a distinctive Scaleform path, diagnostic string, RTTI name, or known
   symbol with `Anchors`.
2. Inspect the referencing functions with `FunctionContext` and `Decompile`.
3. Expand only promising functions with a depth-one or depth-two `CallGraph`.
4. Resolve candidate class tables with `Vtable` and look for constructor writes
   to those tables.
5. Use `RttiClasses` to inventory a tightly filtered native class family,
   `RttiHierarchy` to find recovered descendants of a known base, and
   `Callsites` to separate direct helpers from possible virtual dispatch.
6. Record confirmed observations and rejected hypotheses in `docs/analysis/`.

Keep generated CSV, Markdown decompilation, logs, and Ghidra project files in
the ignored `research-work/` tree. Only curated facts, original explanations,
signatures, and reproducible scripts belong in a release or source commit.
