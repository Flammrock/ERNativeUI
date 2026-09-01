# Conservative Ghidra profile for large PE files

`tools/research/ghidra/ConfigureLargePeAnalysis.java` is a headless pre-script
for very large optimized x86-64 Portable Executable files. It is intended for
a fresh import, before automatic analysis:

```powershell
-import $exe `
-preScript ConfigureLargePeAnalysis.java
```

The script validates the program architecture and Ghidra's registered option
names before changing anything. It then pins these options off:

| Option | Ghidra 12.1.3 default | Profile | Reason |
|---|---:|---:|---|
| `Non-Returning Functions - Discovered` | on | off | Avoid heuristic no-return decisions rewriting flow and function bodies when the input already contains invalid or intentionally confusing flows. |
| `Non-Returning Functions - Discovered.Repair Flow Damage` | on | off | Prevent its clear-and-re-disassemble repair cascade even if the analyzer is later enabled manually. |
| `Aggressive Instruction Finder` | off | off | Do not treat arbitrary undefined bytes as candidate code. |
| `Function Start Search.Search Data Blocks` | off | off | Keep normal executable-block function-start discovery without scanning data blocks for code patterns. |

The first two changes are related but intentionally both explicit. In Ghidra
12.1.3, turning off only `Repair Flow Damage` does not make the discovered
no-return analyzer passive. It can still mark heuristic targets no-return,
override calls to `CALL_RETURN`, and recalculate caller function bodies. A
false decision can therefore damage the call graph even without the final
repair pass.

The profile verifies that it leaves the following analyzer settings unchanged:
known no-return functions, PE exception handling, PE RTTI, Microsoft
demangling, ASCII string creation, code/data references, entry-point
disassembly, executable-block function-start search, Function ID, stack
analysis, call-fixup installation, x86 constant references, and decompiler
parameter/switch analysis. Ghidra may choose a size-dependent default for an
expensive pass; the profile neither enables nor disables such a pass.

## Tradeoff

Disabling discovered no-return analysis means a local wrapper around a fatal
routine will not automatically be marked non-returning unless Ghidra already
knows its name or it is marked manually. Decompiled output for such a wrapper
may retain unreachable tail code or an overly broad function body. This is a
localized, reviewable loss. It is preferable to a false no-return inference
that can rewrite many callers and trigger repeated clear/re-disassembly across
an optimized executable. The separate `Non-Returning Functions - Known`
analyzer remains enabled for named CRT and imported fatal routines.

The profile does not repair a database already changed by a partial run. Use
it on a fresh import (or a deliberately discarded/reimported local database).

## Elden Ring's second executable block

The tested Elden Ring image also contains a second section named `.text` at
RVA `0x4C13000`, size `0x11F6600`. All currently curated UI RVAs are below
`0x29A5800` in the first `.text`. As an additional structural clue, none of
the 235,863 non-empty x64 runtime-function entries in this build's `.pdata`
begin in the second `.text`; 235,856 begin in the first `.text`, with seven
outside either range.

Those facts make setting the second block non-executable a reasonable option
for a separate, explicitly UI-only triage database if full-code analysis is
still impractical. It is not part of the conservative default profile:

- the executable flag in the PE is real, even if the bytes are generated,
  protected, interpreted, or otherwise hostile to static analysis;
- direct or indirect helpers in that block would be omitted as functions;
- cross-section call and thunk relationships could be lost;
- conclusions from such a database could not be presented as whole-program
  coverage.

Prefer the full-memory profile first. If a UI-only database is needed, keep it
separate, bind the block change to the exact executable hash and RVA/size, and
record the omission in every exported report.

## Smoke test

The profile was compiled and run as a pre-script with Ghidra 12.1.3 against a
fresh x86-64 MSVC PE containing C++ RTTI and exception metadata. The import and
post-analysis function export completed with exit code 0. The analyzer timing
report omitted discovered no-return analysis and showed the preserved string,
reference, function, demangler, PE exception, PE RTTI, and decompiler passes.
A read-only reopen confirmed that the four pinned settings persisted and the
preserved analyzers remained enabled.
