# Native popup-choice rows

This case study records how ERNativeUI identified and reproduced Elden Ring's
action-style selector: a row that opens a native list and commits one choice.
It is an implementation/evidence record, not a client tutorial. Mod authors
should use the [Popup Choice guide](../../guides/controls/popup-choice.md).

Popup choice was introduced in the frozen API 1.0 prefix and remains part of
the current API 1.1 contract. Native RVAs, layouts, vtables, and the analytical
names below are private implementation evidence, not public ABI. Evidence
labels follow the [research methodology](../methodology.md).

## Question

> Is the Graphics quality selector an ordinary button whose callback opens a
> list, an inline choice with a different presentation, or a distinct native
> row type—and what minimum native contract lets an ERNativeUI-owned row use
> that same list safely without changing vanilla selectors?

**Confirmed:** it is a distinct action-style selector. Its constructor takes
three erased callable objects: one reads the selected value, one commits a
selection and refreshes the page, and one builds the popup presentation. The
presentation path creates a fixed-capacity native option container. Two
strictly scoped hooks replace that container only for a compiled ERNativeUI
row whose private state pointer matches exactly.

## Exact build identity and provenance

The native conclusions and RVAs on this page describe this Windows Steam
image:

| Property | Value |
|---|---|
| Product version | `2.7.0.0` |
| File size | `87024720` bytes |
| SHA-256 | `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134` |
| PE timestamp | `0x69E9C9B9` |
| Image size | `0x5E09600` |
| Investigation period | August 2026 |

The runtime image base is omitted because ASLR changes it. All addresses are
RVAs relative to this exact image. The historical probe notes did not preserve
a hash for every intermediate ERNativeUI DLL; a reproduction must additionally
record its ERNativeUI commit and built-DLL hash.

## Distinguishing the row types

Three visually related rows have different native contracts:

| Row | Player behavior | Recovered construction model |
|---|---|---|
| button | activation invokes an action | display text plus primary/secondary actions |
| inline choice | left/right changes the visible value in place | selected byte, prebuilt option container, input context |
| popup choice | activation opens a native list; confirm commits, Back cancels | selected-value provider, commit action, presentation provider |

The investigation began from the vanilla Graphics quality row. On the tested
French locale, opening the row at `Personnalisé` and committing `Moyenne`
showed a child selection lifecycle rather than inline left/right mutation.

**Rejected:** cloning only the visual/action-button shape. Early controlled
interventions could render a button, but activation either did nothing or
followed the unrelated Advanced Settings action captured with that button.
The list behavior lived in additional callable state, not in the row artwork.

**Rejected:** treating it as the already recovered inline choice constructor.
That constructor successfully supports arbitrary labels, but it cycles them
inside the row and never enters the native popup-list lifecycle.

Following the quality row's activation and constructor call established a
separate five-argument boundary at RVA `0x95D010`:

```cpp
void* (__fastcall*)(
    void* page,
    void* text_references,
    void* value_provider,
    void* commit_action,
    void* presentation_provider);
```

The temporary label/help references are cloned during construction and can be
destroyed immediately afterward. The constructed row retains the semantic
content of the three callable arguments, including their captured pointers.

## Recovered callable contract

On the tested MSVC x64 build, each erased callable is `0x40` bytes:

| Offset | Recovered field |
|---:|---|
| `+0x00` | callable vtable |
| `+0x08` | captured state pointer |
| `+0x10` | captured page pointer in the commit action |
| `+0x38` | inline target/self pointer |

The callable vtable has three executable entries. Its invoke target is entry
2, at byte offset `+0x10`.

| Role | Vtable RVA | Invoke RVA | Confirmed behavior |
|---|---:|---:|---|
| read selected value | `0x2B15A78` | `0x962520` | returns byte `state + 1` |
| commit selected value | `0x2B15AB0` | `0x962330` | copies the event byte to `state + 1`, then calls page vtable `+0xA0` |
| build presentation list | `0x2B15AE8` | `0x9623A0` | initializes the native popup option container |

The first two invoke bodies are distinctive and were confirmed statically.
The presentation-provider invoke was confirmed by the nested live call into
the list initializer. `page + vtable[0xA0]` is described by its observed
refresh effect; the original game type/member name remains **Inferred**.

### Address derivation

The current resolver finds each invoke/constructor with a unique executable
AOB. A fallback RVA is accepted only when it remains executable and matches
the same local validation bytes. For each callable, the resolver then scans
non-executable image sections for a vtable whose invoke entry points to the
resolved function and whose three entries are executable. It requires one
such match, or validates the exact-build vtable fallback by the same rule.

| Analytical purpose | Reference-build RVA |
|---|---:|
| popup-choice row constructor | `0x95D010` |
| presentation-provider invoke | `0x9623A0` |
| popup-list template initializer | `0x86A610` |
| value-provider invoke | `0x962520` |
| commit-action invoke | `0x962330` |
| value-provider vtable | `0x2B15A78` |
| commit-action vtable | `0x2B15AB0` |
| presentation-provider vtable | `0x2B15AE8` |

**Hypothesis:** an additional builder observed at `0x94ED90` while tracing the
vanilla path performs narrower preparation. Its role is not proven, and
production deliberately does not call or hook it.

## Native value and list model

### Selected value

The game stores popup item values as `1..N`; ERNativeUI exposes zero-based
indices `0..N-1`. Each compiled popup row owns an independent 16-byte native
state object, with the committed byte at `+1`.

At each host-worker poll:

1. A newly applied public value wins, is converted to `index + 1`, and repairs
   the native state.
2. Otherwise, a valid native byte `1..N` becomes public index `value - 1`.
3. Native zero or a value greater than `N` is not exposed; the last valid
   public selection is written back.
4. A changed zero-based observation invokes the value callback once. Cancel
   or confirming the existing value leaves it silent.

**Confirmed:** separate compiled rows have separate state addresses. The
research prototype initially appeared to persist visually because this native
state survived page refreshes; that alone did not prove public
`get`/`set`/callback synchronization. The production poll and focused state
tests close that gap.

### Option container

Static stride analysis and the bounded list trace recovered this layout:

| Field | Offset/size |
|---|---:|
| container | `0x920` bytes |
| first element | `+0x08` |
| element stride | `0x48` bytes |
| element native value | element `+0x08` |
| element text-reference object | element `+0x10` |
| element count | `+0x910` (`uint64_t`) |
| structural capacity | 32 elements |

The initializer at `0x86A610` creates a valid template, including element
vtable/destructor behavior. ERNativeUI calls the original initializer first.
For an owned row, it then:

1. requires a template count in `1..32`;
2. validates every live element's destructor entry before mutation;
3. shrinks the published count before destroying each element in reverse;
4. clears the complete 32-element storage;
5. restores the validated element vtable and constructs each registered text
   reference with native value `index + 1`; and
6. grows the published count only after each element is fully constructed.

This preserves native cleanup invariants even if reconstruction stops partway
through. The 32-option API limit comes from this recovered container, not from
the public byte's theoretical range.

## Why two scoped hooks are required

The list-template initializer receives only `(list, selected_byte)`. Neither
argument identifies the provider, page, or registered row, so hooking it by
itself would risk rewriting every vanilla selector.

The presentation provider does retain the row's state at `provider + 0x08`.
The production bridge therefore uses a nested scope:

```text
presentation-provider detour
    read captured state at +0x08
    find exact compiled popup row by state address
    set thread-local active row
    call original provider exactly once
        -> list-template detour
           call original initializer exactly once
           rebuild only when thread-local row is owned
    restore previous thread-local row
```

The scope restores its predecessor, so nested calls do not erase an outer
identity. It is thread-local, so unrelated UI activity on another thread
cannot inherit the active row. Vanilla Graphics selectors never match a
compiled ERNativeUI state address and retain the original list unchanged.

**Confirmed production decision:** both hooks are necessary. The outer hook
supplies ownership that the inner function lacks; the inner hook sees the
fully initialized native container and its destructor contract.

## Bounded live procedure and results

The historical investigation used opt-in, removable hooks scoped to the
vanilla quality selector or one ERNativeUI-owned state. It did not patch
`eldenring.exe`. The durable notes preserve the finite target and action
sequence but not every intermediate probe's numeric record cap. That missing
number must not be invented.

For a new reproduction, use this bounded experiment card:

```text
game: exact identity above; offline; Easy Anti-Cheat disabled
target: one vanilla Graphics quality row, then one exact owned state pointer
records: fixed 64-record in-memory ring; at most 64 worker log lines
entry: Graphics page with the quality selector focused
actions: open once, move once, Cancel; open once, move once, confirm
control: repeat the vanilla selector before and after custom selector use
intervention variants: exactly 2, 4, and 8 registered options
cleanup: remove every discovery-only hook and probe row after capture
```

During observation, record normalized caller RVAs, the five constructor
arguments, callable bytes before/after construction, provider state identity,
template count, element stride, value byte, text-reference address, and the
commit refresh. Do not invoke an unknown function or dereference an unproved
pointer merely because it is near the candidate.

The live intervention matrix produced:

| Test | Result |
|---|---|
| native four-item `Minimal`, `Balanced`, `Detailed`, `Maximum` model | **Confirmed** |
| custom two-, four-, and eight-option lists | **Confirmed** |
| choose first and last item, close, and reopen | **Confirmed value retained** |
| close with Back/Cancel | **Confirmed no selection callback** |
| confirm unchanged item | **Confirmed no selection callback** |
| two popup rows in one menu | **Confirmed independent state** |
| controller and mouse activation | **Confirmed normal native behavior** |
| ordinary Back behavior | **Confirmed unchanged** |
| French vanilla Graphics labels before/after a custom list | **Confirmed unchanged** |
| one and 32 options | **Structurally unit-tested; not recorded as live variants** |

All discovery-only constructor, input, quality-row, and widget-method probes
were removed. Production retains only the constructor call and the two scoped
list hooks described above.

## Production ownership and failure behavior

### Registration and retained data

The host validates and synchronously copies the row label, help text, and
between 1 and 32 non-empty option labels. Menu compilation gives those strings
host-owned text IDs and initializes one private native state per row. The
compiled menu and state outlive every native row that captures them.

The row constructor receives three temporary `0x40` callable shells with
validated vtables, each capturing the stable state address. The commit action
also captures the live physical page so native confirmation can request its
ordinary refresh. No native callable or raw state pointer crosses the public
ABI.

### Install and removal ordering

Popup-specific resolution and hooks are required only when the compiled,
reachable menu contains at least one popup choice. If any constructor,
provider, template, invoke, or callable-vtable dependency is unresolved, the
complete native hook installation is rejected and reset rather than exposing
a row with a nonfunctional list.

Both hooks are created disabled. The nested template hook is enabled before
the outer provider hook; on removal the outer provider is stopped before the
nested hook. A partial enable failure clears both hooks and their original
function pointers.

### Runtime fail-closed rules

The bridge accepts only an exact state address found on a compiled row whose
kind is popup choice. Provider-state reads and native reconstruction are
guarded by SEH, but structural validation is still mandatory.

Before mutation, any null pointer, invalid count, missing element vtable, or
non-executable destructor leaves the original template untouched. If native
text-reference construction fails after mutation begins, the count exposes
only the fully constructed prefix, so game cleanup cannot walk destroyed or
partially initialized entries. Fault logs stop after eight errors and one
suppression message.

Native selection is translated and callbacks are delivered later by the
ERNativeUI host worker. Client code never runs inside the constructor or
list-provider hooks. Provider code remains pinned under the normal host
lifecycle; hot unload is unsupported.

## Confirmed, inferred, rejected

| Status | Finding |
|---|---|
| **Confirmed** | Popup choice has a distinct five-argument constructor and three callable roles. |
| **Confirmed** | The selected native byte is one-based at state `+1`; the public value is zero-based. |
| **Confirmed** | The list is a `0x920`-byte, 32-slot container of `0x48`-byte elements. |
| **Confirmed** | Exact state identity plus nested thread-local scope leaves vanilla lists untouched. |
| **Confirmed** | Two-, four-, and eight-item lists, independent rows, commit, cancel, reopen, controller, mouse, and Back work on the reference build. |
| **Inferred** | Analytical class/member names and the semantic name of page vtable `+0xA0`; only its refresh behavior is established. |
| **Rejected** | Popup choice is merely an ordinary button callback. |
| **Rejected** | Popup choice is the inline left/right choice with alternate artwork. |
| **Rejected** | The unscoped list initializer can safely be rewritten for every call. |
| **Rejected** | Visual persistence alone proves public state/callback integration. |

## Current limits and unresolved questions

- The native container supports at most 32 options. One and 32 are covered by
  structural tests, while the preserved live matrix covers two, four, and
  eight.
- Popup choice has no public disabled state; the current registration path
  creates it enabled.
- The purpose and safe ownership contract of observed builder `0x94ED90`
  remain unresolved.
- The unused bytes in the callable shells, native state, and option elements
  are deliberately not assigned semantics.
- Nested scope restoration is implemented, but deliberately re-entrant popup
  creation has not been preserved as a separate live test.
- Mutation-time text-reference failure is structurally fail-safe; inducing a
  controlled native allocation failure has not been a live validation target.
- Updated game builds must revalidate the constructor, invoke bodies, vtables,
  list layout, destructor contract, and commit refresh—not only the AOB match.
- Coexistence with an unknown foreign detour on either scoped hook remains a
  compatibility test, not an assumed guarantee.

## Current source anchors

| Area | Current source or test |
|---|---|
| AOBs, validated fallbacks, callable-vtable scan, and feature dependency gate | [`src/addresses.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.cpp) and [`src/addresses.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/addresses.hpp) |
| callable construction, captured state/page, row constructor, and exact state lookup | [`src/native_menu.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_menu.cpp) |
| nested provider/template hooks, list validation, reconstruction, and cleanup | [`src/native_popup_choice.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_popup_choice.cpp) and [`src/native_popup_choice.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/native_popup_choice.hpp) |
| one-based native state and public synchronization | [`src/popup_choice_state.hpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/popup_choice_state.hpp) |
| copied text IDs, per-row state initialization, and reachable feature count | [`src/menu_compiler.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/menu_compiler.cpp) |
| conditional bridge installation and reset | [`src/hooks.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/hooks.cpp) |
| worker polling and changed-value callback delivery | [`src/runtime.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/runtime.cpp) |
| strict-C validation, copied options, row handles, and programmatic get/set | [`src/host_registry.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/src/host_registry.cpp) |
| one-/32-item boundaries, invalid-native repair, public writes, cancel, and state independence | [`tests/popup_choice_state_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/popup_choice_state_test.cpp) |
| compiled kind, option text IDs, and distinct state | [`tests/menu_model_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/menu_model_test.cpp) |
| descriptor limits, API capability, and get/set validation | [`tests/host_registry_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/host_registry_test.cpp) |
| C++17 wrapper construction | [`tests/client_header_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/client_header_test.cpp) |
| frozen API 1.0 C/C++ client compatibility | [`tests/abi/releases/v1_0/c_client_test.c`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/releases/v1_0/c_client_test.c) and [`tests/abi/releases/v1_0/cpp_client_test.cpp`](https://github.com/Flammrock/ERNativeUI/blob/main/tests/abi/releases/v1_0/cpp_client_test.cpp) |

For how this row fits the wider GFX, native-page, input, and callback model,
see [Elden Ring native UI system](../architecture/native-ui-system.md).
