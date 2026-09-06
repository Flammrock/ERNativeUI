# Reproducible native UI research methodology

This workflow turns a visible Elden Ring behavior into a reviewable native
boundary without treating a plausible address as proof. It applies to static
analysis, GFX inspection, temporary runtime observation, and game-update
maintenance.

The evidence vocabulary and reference executable are defined in the
[research index](README.md). The
[build-locked address map](address-map/README.md) is a historical baseline
snapshot; current production resolvers remain authoritative.

## 1. Write the question before probing

Begin with one observable question, for example:

> Which native action is submitted when the player presses Back on an
> Advanced Settings page?

Record a small experiment card before changing code:

```text
question:
game identity: product version, SHA-256, PE timestamp, image size
ERNativeUI commit/build:
entry state:
exact player actions:
control case:
expected observation:
time limit:
maximum captured calls/log lines:
cleanup:
result and evidence label:
```

This prevents an open-ended hook from becoming the experiment. A negative
result is useful when the armed interval, action, and capture limits are
recorded.

## 2. Lock the input build

At minimum, record the product version and SHA-256 before importing or
running a probe:

```powershell
$gameExe = 'C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game\eldenring.exe'
(Get-Item -LiteralPath $gameExe).VersionInfo.ProductVersion
Get-FileHash -Algorithm SHA256 -LiteralPath $gameExe
```

Also retain the PE timestamp, image size, and file size in the research note.
The import helper checks the known SHA-256 by default; see the
[Ghidra workflow](tools/ghidra-workflow.md).

Do not put the executable, a modified copy, extracted game assets, or a
Ghidra database in the repository.

## 3. Establish static anchors

Prefer anchors that describe behavior rather than proximity:

1. a distinctive GFX object path, resource name, localized-message use, RTTI
   type, or already confirmed call site;
2. the containing function and its PE unwind/function bounds;
3. direct callers, callees, and relevant data references;
4. constructor vtable writes and MSVC RTTI relationships where a class is
   involved;
5. argument flow and ownership operations around the candidate; and
6. a second independent anchor that reaches the same boundary.

Ghidra names such as `FUN_140950850`, inferred parameters, and decompiled
types are analysis output. Rename a function with an `ERUI_` analytical name
only after its narrow role is supported; never present that name as an
original FromSoftware symbol.

Public Scaleform documentation can explain a call shape, but it cannot prove
that Elden Ring uses a particular wrapper, owner, thread, or lifetime. The
same rule applies to visually similar pages and nearby native functions.

## Build-locked observations and portable derivations

Keep these address concepts separate:

| Record | Meaning | Valid use |
|---|---|---|
| Runtime virtual address | Module base plus an offset for one launch. ASLR changes it. | A temporary trace only. Normalize it before recording. |
| RVA | `runtime address - module base`, observed in one PE image. | Navigate the exact build and correlate evidence. Never copy it blindly to another build. |
| Direct AOB derivation | Search executable sections for stable instructions at the function boundary. | Candidate cross-build resolver when the match is unique and semantics are revalidated. |
| Call-site AOB derivation | Find a semantic caller sequence, wildcard the `rel32`, then decode the target. | Preferable when a callee prologue is generic but its use is distinctive. |
| Validated fallback RVA | A known location accepted only when it is executable and its bytes or related structure match a validation rule. | Recovery for a known layout, never an unchecked bypass. |
| Complete-build lock | Require timestamp/image size or the full hash in addition to local bytes. | Features whose retained state, callbacks, or object layout are too coupled for a short signature alone. |

The current resolver implementation in
The current online [`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp) illustrates direct searches,
call-target derivation, ambiguity rejection, executable-section checks, and
validated fallbacks. The byte patterns there and in feature-local backends are
the production source of truth. The unreconciled CSV records the earlier
exact-build evidence snapshot and may be incomplete or stale as a production
inventory.

Wildcard relocation-sensitive values such as `call rel32`, RIP-relative data
displacements, and absolute addresses. Do not wildcard meaningful stack
layout, field offsets, branch structure, or constants merely to force a match.
A pattern that matches twice is unresolved, not "close enough."

## 4. Inspect static GFX separately

A GFX placement proves presentation structure, not native behavior. Record:

- source movie and its exact hash or extraction provenance;
- the full named display path;
- whether the character definition is shared while placements are per row;
- character IDs, depths, matrices, scaling grids, imported resources, and
  definition order relevant to the change; and
- a structural before/after comparison and an idempotence check.

Adding `Item_N_0`, a widget, or a tab sprite does not create a native row,
focus target, callback, input route, or owner. A runtime feature needs both the
presentation and native-control halves unless the existing game already owns
one of them.

The standalone GFX patcher is an offline, optional transformation. It writes
a separate output and is not evidence that the host can create arbitrary
Scaleform objects at runtime.

## 5. Use bounded observation before intervention

Prefer temporary DLL instrumentation to patching `eldenring.exe` on disk.
Run offline with Easy Anti-Cheat disabled and keep the probe feature-local.

A safe observation probe should:

- be opt-in and inactive at startup;
- arm only after a precise player action or diagnostic command;
- expire after a short fixed interval;
- cap total captures and log lines;
- filter by page, owner, object, thread, or call-site identity where possible;
- record RVAs instead of only absolute addresses;
- call the original function exactly once unless the experiment explicitly
  tests suppression;
- avoid dereferencing unproved pointers or invoking unknown functions; and
- remove its hooks and temporary build artifacts after the result is captured.

Capture at least two repetitions and a control action. Stable caller RVAs and
stable object relationships across changing heap addresses are stronger than
one register snapshot. Human input delay does not invalidate a trace when the
armed window is bounded and the repeated call relationship is unique.

Structured exception handling is a final diagnostic boundary. It does not
make a wrong signature, stale pointer, or invalid owner safe.

## 6. Recover the whole contract

Before calling or detouring a candidate, answer all applicable questions:

- What is the real function boundary and calling convention?
- Which arguments are inputs, outputs, owners, borrowed views, or packed
  actions?
- Which fields are read before and after the call?
- Does the callee copy, retain, consume, or borrow each object?
- Which constructor, destructor, retain, or release operation closes the
  lifetime?
- On which observed construction/update path does it run?
- Can it be re-entered, and does it invoke client code synchronously?
- What happens on Back, Cancel, page replacement, repeated opening, and game
  shutdown?
- What other mod may already detour the same entry?

An oversized scratch buffer that happens to work is not a recovered native
`sizeof`. A vtable member at the expected offset is not enough without object
identity. A native function that creates pixels is not enough without focus,
input, update, render, and teardown ownership.

## 7. Promote evidence deliberately

Use this progression:

1. **Hypothesis** - document the predicted relationship and falsifying test.
2. **Confirmed** or **Rejected** static result - record exact bytes, callers,
   data, or GFX structure.
3. **Confirmed** bounded live observation - establish arguments, object
   identity, and behavior without changing state where possible.
4. **Confirmed** positive and negative intervention - exercise success,
   cancel/failure, reopen, navigation, and teardown paths.
5. Production candidate - add a semantic resolver, typed boundary, dependency
   gating, cleanup, and feature-local failure.
6. Production feature - pass automated tests plus an explicit in-game matrix
   on the recorded build and any declared compatibility combination.

A production candidate must fail closed. A missing or ambiguous optional
address should disable that destination or feature, not encourage an
unchecked RVA or leave a partially installed hook set.

Keep **Inferred** wording when only the behavior is known but the original
class name or field meaning is not. Preserve **Rejected** candidates with the
probe shape and result.

## 8. Maintain the map after a game update

When Elden Ring changes:

1. preserve the failing log and record the new complete PE identity;
2. classify every resolver as missing, ambiguous, unchanged, or moved;
3. create a separate Ghidra project/research root for the new hash;
4. resolve current production AOBs before consulting old RVAs;
5. diff candidate instructions, function bounds, callers/callees, vtables,
   object offsets, and cleanup paths;
6. repeat the bounded behavioral test, including failure and teardown;
7. update patterns only when their semantic anchor remains true;
8. add a new build-specific CSV instead of rewriting the previous map; and
9. run unit/ABI tests and the in-game compatibility matrix before release.

Nearby RVAs are search hints only. An unchanged RVA is not proof of unchanged
semantics, and a moved RVA is not proof that only the address changed.

## Minimum research-note template

```markdown
# Feature or boundary

Status: Hypothesis | Inferred | Confirmed | Rejected

## Build identity

Product version, SHA-256, timestamp, image size, ERNativeUI commit.

## Question

One observable question and why the answer is needed.

## Static derivation

Anchors, function bounds, callers/callees, AOB derivation, object fields.

## Bounded live procedure

Exact player actions, control, timeout, capture cap, and safety gates.

## Result

Observed facts, rejected alternatives, and raw artifact locations.

## Ownership and failure behavior

Retain/release, callback/thread observations, teardown, fail-closed scope.

## Production decision

What was promoted, what remains inferred, and what must be retested next build.
```

Worked examples of this method are the
[settings-page investigation](case-studies/settings-pages-and-pagination.md),
[native-dialog investigation](case-studies/native-dialogs.md), and
[TextInput investigation](case-studies/text-input.md). The
[research index](README.md) separates current production evidence from future
research targets.
