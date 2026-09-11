# Elden Ring 2.7.1.0 native-profile update

Status: **statically revalidated and live-validated in game**.

This record explains how ERNativeUI support was ported from Elden Ring
`2.7.0.0` to `2.7.1.0` without changing the public ERUI API. It is intended
both as provenance for the 1.1.1 host patch and as a reproducible example for
the next game update.

## Compared executables

| Property | Elden Ring 2.7.0.0 | Elden Ring 2.7.1.0 |
|---|---:|---:|
| File size | `87024720` bytes | `87042128` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` | `1A3547101327F65D0C76DA2F9190AC0AA66871EA42BAE2AECC61E11A8B597891` |
| PE timestamp | `0x69E9C9B9` | `0x6A96B418` |
| Image size | `0x5E09600` | `0x5E0DA00` |

The current image is `0x4400` bytes larger. Ordinary PE section RVAs remain
stable; the final protected executable section accounts for the growth.
Addresses below are RVAs and therefore do not include the ASLR image base.

## What failed first

The 2.7.1.0 diagnostic log showed that all ordinary settings/menu AOBs still
resolved. The installation stopped at:

```text
Address resolution: no compatible TextInput path for this game image
Pre-resolved native interfaces do not satisfy the compiled menu
```

This was a deliberate fail-closed result: TextInput, ColorPicker, and Input
Bindings were admitted only for the complete 2.7.0.0 PE identity. The first
task was therefore to determine whether their native contracts changed, not
to remove the identity guard.

## Comparison method

1. Record both complete file hashes and in-memory PE identities.
2. Scan both executable images with every production signature.
3. Compare each exact-RVA function entry, its complete recovered function
   body where available, relative call targets, vtables, and data anchors.
4. Classify a boundary as unchanged, relocated with equivalent semantics, or
   unresolved.
5. Add an explicit profile for the new identity; never silently reuse the old
   one.
6. Keep local byte, vtable, executable-memory, and data validation after the
   profile is selected.
7. Run the feature-level in-game matrix before calling the build supported in
   a release.

The current implementation centralizes the exact identity list in
`src/game_build.hpp` and the build-specific address maps in
`src/game_build_profiles.hpp`. A profile is selected only when both timestamp
and image size match. A hybrid or unknown identity is rejected.

## Results

### Generic settings and dialog layer

The ordinary settings-page, row, navigation, text, popup-choice, and dialog
semantic signatures still resolve. The shared Scaleform-result destructor
moved from `0xD81590` to `0xD81600`. Its generic AOB derives the new address,
and its validated fallback is now selected from the exact core-UI build
profile rather than carrying a single build's RVA.

### TextInput

All six exact TextInput boundaries remain at the same RVAs:

| Boundary | 2.7.0.0 | 2.7.1.0 |
|---|---:|---:|
| row producer | `0x976EF0` | `0x976EF0` |
| native menu-string constructor | `0x5EE0F0` | `0x5EE0F0` |
| borrowed menu-string constructor | `0x6766F0` | `0x6766F0` |
| native menu-string destructor | `0x1BCC60` | `0x1BCC60` |
| editor-factory builder | `0x915D70` | `0x915D70` |
| character-name editor factory | `0x81D610` | `0x81D610` |

The helper constructors, destructor, builder, and factory are byte-identical.
Static control-flow, bounds-access, call-shape, entry-validator, and vtable
comparison found no contract change in the row producer; only relative
displacements to helpers that moved by `0x70` changed. The five required
TextInput-related vtables remain at the same RVAs and their inspected entries
are byte-identical. The new profile therefore deliberately repeats the six
RVAs while preserving every existing entry-pattern and live-object check.
No TextInput address change was required for this build profile. The complete
host reached readiness during the live 2.7.1.0 compatibility regression.

### ColorPicker

Three called boundaries moved; the rest of the exact backend remained stable:

| Boundary | 2.7.0.0 | 2.7.1.0 |
|---|---:|---:|
| game allocation dispatch | `0x1EBBCD0` | `0x1EBBD40` |
| temporary SceneObjProxy destructor | `0xD81590` | `0xD81600` |
| Scaleform color-transform setter | `0xD85610` | `0xD85680` |

The modal job builder, queue submission, heap provider, palette lifecycle,
scene-proxy bridge, color controller, widget producer, visibility/value
helpers, path resolver, and movie-name data retain their 2.7.0.0 RVAs. Every
entry is still checked against its expected bytes after profile selection.

### Input Bindings

Two called/hooked boundaries moved:

| Boundary | 2.7.0.0 | 2.7.1.0 |
|---|---:|---:|
| query-input-states forwarding thunk | `0x2667AD0` | `0x2667B40` |
| input-manager update | `0x266A480` | `0x266A4F0` |

The forwarding thunk still loads the owner from `+0x10` and jumps to an
implementation that also moved by `0x70`. The old pattern accidentally
included six bytes belonging to the neighboring function. The replacement
signature covers the thunk, wildcards those six unstable adjacent bytes, and
then includes a longer stable prefix from the immediately following function
as uniqueness context:

```text
48 8B 49 10 E9 ?? ?? ?? ?? 90 ?? ?? ?? ?? ?? ??
40 53 48 81 EC 80 00 00 00 8B 84 24 B0 00 00 00
48 8B 59 38
```

It has one match in each compared executable. The input-manager singleton
slot, binding functions, editor-lifetime functions, and all five required
vtables remain at their prior RVAs and retain their validators.

### First in-game candidate with Solid Uncapper

The first 2.7.1.0 in-game candidate resolved the new game profile and compiled
all three client providers successfully. It also recognized and chained all
three previously supported Solid Uncapper menu detours. ERNativeUI rows still
did not appear because installation then stopped while preparing Input
Bindings.

Comparison against the pristine 2.7.1.0 executable confirmed that every Input
Bindings pattern and profile RVA was correct. Solid Uncapper's own log then
identified the runtime difference: its asynchronous installer also detours
the following entries before ERNativeUI's late Input Bindings preparation:

| Input boundary | RVA in both compared builds | Runtime owner |
|---|---:|---|
| `write_binding_value` | `0x242960` | Solid Uncapper detour |
| `clear_key_setting` | `0x8687D0` | Solid Uncapper detour |

The failure was therefore not another Elden Ring relocation. It was a
third-party hook-order race: validating those two live entries as pristine
after Solid Uncapper had installed correctly rejected them and caused the
host's atomic installation to fail closed.

The 1.1.1 host patch captures the complete native Input Bindings interface
set before Solid Uncapper's worker can modify it. After the existing Solid
Uncapper stabilization wait, ERNativeUI validates the current entries again:

- every interface other than `write_binding_value` and
  `clear_key_setting` must still match its pristine entry pattern;
- either known overlap may remain pristine;
- a changed known overlap is accepted only as an `FF 25`
  absolute-indirect detour whose resolved target is executable and owned by
  `Solid Uncapper.dll`; and
- any foreign target, unsupported detour encoding, or change to another Input
  Bindings entry rejects the compatibility path.

With those conditions established, ERNativeUI deliberately installs through
the captured entries. The existing Solid Uncapper detours remain reachable in
the call chain instead of being overwritten or bypassed. This design is
specific to the two observed Input Bindings overlaps; it is not a general
permission to hook arbitrarily modified native functions.

### Second in-game candidate with Solid Uncapper

The input-chain candidate subsequently reached and prepared the complete
native Input Bindings backend. Atomic installation still failed one stage
later because the showcase declares ColorPicker rows and one exact ColorPicker
entry no longer matched pristine game bytes:

| ColorPicker boundary | RVA in both compared builds | Runtime owner |
|---|---:|---|
| Scaleform visibility setter | `0x734190` | Solid Uncapper detour |

Solid Uncapper's log independently identified its property-set hook at the
same RVA. Static inspection of that detour established the relevant call
contract: it preserves the incoming object/value arguments, calls its stored
vanilla trampoline, and performs its post-processing afterwards. The failure
was therefore another recognized hook-order overlap, not a missed Elden Ring
relocation or a changed ColorPicker ABI.

The host patch captures this one pristine visibility entry and its first
16 bytes before Solid Uncapper's asynchronous installer can modify it. After
the normal stabilization wait, the current entry is accepted only when it is:

- byte-for-byte equal to the early snapshot and still matches the pristine
  visibility-setter pattern; or
- an `FF 25` absolute-indirect detour whose resolved destination is executable
  memory owned by `Solid Uncapper.dll`.

The approved entry is validated again immediately before ColorPicker publishes
its native call table. ERNativeUI keeps calling the game entry address so the
runtime chain remains ERNativeUI caller -> Solid Uncapper detour -> original
Elden Ring setter. It neither calls Solid Uncapper's destination directly nor
replays copied game bytes. Every other exact ColorPicker function and data
anchor must remain pristine. A foreign target, unsupported encoding, stale
snapshot, or new ColorPicker overlap fails closed. The resulting chain was
then live-validated in the matrix below.

### Root-capacity correction after the shared rows appeared

Once both mods' rows were visible, live testing exposed a separate pagination
accounting error on Game Options: the root planner selected a slice from the
movie's visual capacity but still assumed that only Elden Ring's canonical
four rows occupied it. Solid Uncapper had already appended another row through
the validated chained materializer, so ERNativeUI could place its navigation
row one slot too late.

The corrected implementation samples the live materialized row count after the complete
original chain returns. It selects a precompiled root plan from the actual
number of free placements:

```text
free placements = visual capacity - post-chain materialized rows
```

On the vanilla six-row movie, a fifth existing row leaves one placement. If
provider content overflows, that placement contains Next only and all provider
content starts on the continuation. On the optional 13-row movie, five
existing rows leave eight placements, so an overflowing first slice contains
seven provider rows plus Next. Counts that are unreadable, below the proven
four-row baseline, or leave no free placement fail closed. Both layouts were
subsequently live-validated.

## Why this remains ERUI API 1.1

No public structure, function-table entry, enum, callback, ownership rule, or
client header changed. Only the host's private mapping from a recognized game
image to validated native addresses changed. ERNativeUI `1.1.1` can therefore
continue to negotiate ERUI APIs 1.0 and 1.1 exactly as `1.1.0` did.

## Recorded live validation

The final 1.1.1 compatibility build was live-tested with the showcase client
on both supported executables:

| Game executable | Solid Uncapper 2.3 loaded first | No Solid Uncapper | Original six-row GFX | Optional 13-row GFX |
|---|---:|---:|---:|---:|
| `2.7.0.0` | Passed | Passed | Passed | Passed |
| `2.7.1.0` | Passed | Passed | Passed | Passed |

Both mods' rows appeared together when Solid Uncapper was loaded, and
ERNativeUI initialized normally through the pristine path when it was absent.
Game Options pagination used the post-chain materialized count: Next occupied
the final available placement without overflowing the authored layout, and
Previous restored the root in both GFX configurations. This live result does
not weaken the exact-build guards or imply compatibility with future game or
Solid Uncapper versions.
