# Elden Ring native UI research

This directory explains the native Elden Ring UI boundaries behind
ERNativeUI. It is for readers who want to reproduce a finding, review why a
hook is trusted, or continue the research after a game update.

The research is not a public ABI. Native addresses, layouts, vtables, and
analytical names remain private implementation details. Mod clients should
use the [documented ERNativeUI features](../features.md) through the public
[C ABI](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/erui.h) or
[C++17 wrapper](https://github.com/Flammrock/ERNativeUI/blob/main/include/ernativeui/ERNativeUI.hpp).

ERUI API 1.1 is the current, finished public contract. A research item tagged
for API 1.2 is exploratory: it is neither a release promise nor permission to
depend on an unfinished native boundary.

## Evidence labels

Every durable claim uses one of these labels:

- **Confirmed** - reproduced from a static artifact, a controlled live test,
  or both, with the tested build recorded.
- **Inferred** - several observations agree, but a type, semantic meaning,
  owner, thread, or transition has not been observed directly.
- **Hypothesis** - a specific explanation or test target that still needs
  evidence.
- **Rejected** - a controlled result disproved the candidate. Rejected paths
  stay recorded so another investigator does not repeat them.

Confirmed means confirmed on the named build; it does not mean stable across
game versions. A result becomes a production hook only after the additional
validation gates in [Research methodology](methodology.md).

## Reference build

The completed baseline analysis and the historical symbol snapshot describe
this Windows executable:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| File size | `87024720` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |

The runtime image base is deliberately absent. Windows ASLR can change it on
every launch; a runtime virtual address is useful only after subtracting that
launch's module base to obtain an RVA.

## Start here

| Question | Document |
|---|---|
| How should an experiment be designed and promoted? | [Research methodology](methodology.md) |
| How do GFX, Scaleform movies, native pages, input, and jobs fit together? | [Native UI system](architecture/native-ui-system.md) |
| How is the Configuration tab host laid out, and what still blocks custom tabs? | [Configuration and OptionSetting class map](architecture/configuration-class-map.md) |
| How are GFX resources, MovieDefs, players, frame advance, and destruction connected? | [Scaleform movie lifecycle](architecture/scaleform-movie-lifecycle.md) |
| How do ordinary rows, text editing, Back tasks, and popup jobs receive or suppress input? | [Native input and event dispatch](architecture/input-and-event-dispatch.md) |
| Which native UI RTTI families and multiple-inheritance subobjects were recovered? | [Native UI class hierarchy](reference/native-ui-class-hierarchy.md) |
| What does the narrow SceneObjProxy/Scaleform value bridge own and support? | [Scene-object and Scaleform bridge](reference/scene-object-bridge.md) |
| Which existing GFX resources are useful leads, and when is reuse actually safe? | [GFX resource catalog](reference/gfx-resource-catalog.md) |
| Which linked Scaleform exports, RTTI records, vtables, and public SDK concepts are useful navigation seeds? | [Scaleform runtime seeds](reference/scaleform-runtime-seeds.md) |
| How can the local executable database and bounded exporters be reproduced? | [Ghidra workflow](tools/ghidra-workflow.md) |
| Which analytical symbols were captured in the baseline? | [Historical build-locked address map](address-map/README.md) |
| Which hooks and native call boundaries does the current production host actually install? | [Current native hook and call inventory](reference/native-hook-inventory.md) |
| How were built-in settings destinations, live capacities, and pagination recovered? | [Settings pages and pagination case study](case-studies/settings-pages-and-pagination.md) |
| How were native generic dialogs and modal input recovered? | [Native dialogs case study](case-studies/native-dialogs.md) |
| How was the native action-style choice list recovered? | [Popup-choice case study](case-studies/popup-choice.md) |
| How was editable native text recovered and integrated? | [Text-input case study](case-studies/text-input.md) |
| How was the character-creation color editor reused safely? | [Color-picker case study](case-studies/color-picker.md) |
| How were native binding rows, input codes, and activation dispatch recovered? | [Input-bindings case study](case-studies/input-bindings.md) |
| Why do TextInput and ColorPicker need optional patched GFX assets? | [GFX presentation case study](case-studies/gfx-presentation.md) |
| How is Steam language discovered without a game hook? | [Steam language case study](case-studies/steam-language.md) |

## Addresses are evidence, not interfaces

The [CSV symbol map](address-map/eldenring_2.7.0.0_known_symbols.csv) is a
historical snapshot of observed RVAs for exactly one executable. Its source
references point to the current research pages, but its coordinates,
confidence labels, and production-role field still describe that recorded
analysis snapshot. It can make the matching Ghidra project easier to navigate;
it must never be treated as the live production inventory or applied to
another hash.

Production discovery is a separate layer and the current source is
authoritative. The host generally searches the
loaded executable for a semantic byte pattern, requires a unique result,
derives call targets where appropriate, and validates any fallback RVA before
use. Some especially coupled features are also locked to the complete PE
identity. See [Build-locked observations and portable derivations](methodology.md#build-locked-observations-and-portable-derivations).

An AOB is not automatically portable. It is only a candidate derivation that
must be revalidated against the function's callers, arguments, object
relationships, and live behavior on every new build.

## Artifact policy

Keep these local under the ignored `research-work/` directory:

- `eldenring.exe` and every copy of it;
- Ghidra projects and databases;
- bulk disassembly or decompiler output;
- extracted game assets and raw game strings; and
- temporary probe logs, dumps, and caches.

The repository may contain original scripts, concise pseudocode, derived
signatures, build identities, analytical symbol maps, controlled experiment
records, and conclusions about ownership or behavior.

For new work, cite the most specific page in this directory, the current
implementation source, and the public headers where a client-facing contract
is involved. The [native hook inventory](reference/native-hook-inventory.md)
is the current production overview; the build-locked CSV remains a historical
navigation aid.
