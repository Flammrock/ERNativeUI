# ERUI API 1.4 roadmap: reactive native menus

> **Development preview — not released.** The current stable public contract
> is ERUI API 1.3. Names, descriptors, capability bits, behavior, and scope on
> this page may change or be rejected before API 1.4 is frozen.

API 1.4 is planned as the foundation for native menus that can change safely
after provider registration. The target is one coherent update model rather
than unrelated setters: a client stages one or many changes, commits them once,
and ERNativeUI validates and publishes a complete new generation at a safe
game/UI boundary.

Development is intended to take place on the public `api-1.4` branch. Stable
`main`, Release Please, stable tags, and Nexus packages remain independent
until the complete API passes its release gates.

## User-facing goals

### One-control and batched commits

A mod should be able to change one control or several controls in one
operation—for example, change two labels, update their help text, disable one
row, and reveal a conditional group—then commit once. The player must never
observe a partially applied intermediate menu.

The likely C boundary is one array-based, provider-scoped mutation operation.
The C++ wrapper can expose the same operation as either a one-control update
or a batch. In these examples, `difficulty` is a `ToggleRow`, while
`advanced_group` and `expert` are the future contextual group and Collapse
objects returned by their ordinary builder functions. They retain the
provider/API context needed to build an edit and still convert to their legacy
raw handles where one exists:

```cpp
difficulty.edit()
    .label(L"Enemy Damage")
    .help(L"Applied by the active balance profile.")
    .commit();
```

The equivalent multi-control form keeps every edit in one atomic commit:

```cpp
auto updates = registration.updates();

updates.add(difficulty.edit()
    .label(L"Enemy Damage")
    .help(L"Applied by the active balance profile.")
    .label_color(erui::Color{107, 100, 80})
    .interactive(profile_loaded));

updates.add(advanced_group.edit().visible(show_advanced));

updates.add(expert.edit().expanded(show_expert));

updates.commit();
```

Each concrete object's `edit()` creates its corresponding inert draft. For
example, `ToggleRow::edit()` returns `ToggleRowEdit`, `ColorPickerRow::edit()`
returns `ColorPickerRowEdit`, and a future `CollapseRow::edit()` returns
`CollapseRowEdit`. Common presentation operations remain consistent across
draft types, while semantic operations such as `value(...)`, `color(...)`, or
`expanded(...)` exist only on the types for which they make sense.

Calling `commit()` on a draft submits it as a single-entry batch. Passing it to
`updates.add(...)` instead transfers its pending changes into the provider
batch without publishing anything yet. `updates.commit()` then performs one
public submission and atomically publishes one coherent generation containing
every added update. The host may still repaint multiple affected native
surfaces at their respective safe boundaries.

This keeps the two forms consistent: a mod builds an update from the control
that owns the state, then chooses whether to commit it individually or group it
with other updates. The batch is only a collector; it does not expose a second
control-editing syntax. Pending drafts are move-only, one-shot values and
become consumed when they are added or committed. Destroying an uncommitted
draft has no effect; there is no implicit commit.

These edit objects describe the target API, not the first foundation pass.
The typed row references are introduced and compatibility-tested before any
public `edit()` method exists. ERNativeUI must not publish an inert edit object
that cannot yet commit through a complete host operation.

`commit()` means that the complete desired-state update was validated, copied,
and queued. It does not promise that Scaleform has already repainted before the
call returns. ERNativeUI may safely repaint only the affected live page while
an offscreen page consumes the new generation when it is next materialized.
The mod receives one atomic logical result without depending on native object
lifetime or reconstruction details.

Every draft setter must own its supplied strings before returning, and
`updates.add(...)` must consume the complete draft synchronously rather than
retain references to its temporary expression. The live menu changes only
after the complete batch passes validation. A failure keeps the previous
published generation intact. Cross-provider or stale drafts are rejected, and
the duplicate-target policy must be fixed and tested before the API is frozen.

Submitting either a one-row edit or a batch from any ERNativeUI client callback
is a required API 1.4 use case. Callback-originated submission must be safe
regardless of whether that callback arrived through a native UI call or the
host's polling path.

`commit()` validates and copies the complete request before returning, then
queues the accepted generation. It must not recursively destroy, rebuild, or
repaint the native menu while Elden Ring or the host is dispatching the
callback. The current callback finishes against the generation with which it
started; publication and any focus relocation occur later at a proven UI-safe
boundary. A successful return therefore means accepted for publication, not
already visible on screen.

### Mutable row presentation

The planned row-level mutations are:

- label text;
- help text;
- the right-hand control caption where that control owns text;
- structured fragments within label, caption, and help text, including color and
  underline; and
- runtime interactivity.

Plain `label(...)` and `help(...)` values remain literal text. ERNativeUI
escapes characters that have meaning to Scaleform HTML, so a mod does not
need to sanitize ordinary localized or player-derived strings. Raw HTML is not
part of the public API.

For deliberate presentation, `erui::StyledText` composes visible text from
plain fragments and an allowlisted `erui::TextStyle`. A fragment may inherit
the native appearance or request an opaque RGB color, underline, bold, italic,
or a combination of those properties. The host validates the fragment ranges,
escapes every visible fragment, and generates the Scaleform markup itself.
This permits multiple colors and styles in one label or help message without
exposing arbitrary movie markup to client mods.

`label(...)` always means the left-hand row label. `caption(...)` means the
text rendered inside the right-hand control. The first caption-capable C++
drafts are `ButtonRowEdit` and `PopupChoiceRowEdit`; sliders, Toggle,
Inline Choice, TextInput, and ColorPicker do not advertise an operation that
their native control cannot represent safely. Caption values accept the same
plain or structured text as labels and help messages.

A Popup Choice caption is an explicit display override for its closed row. It
does not replace the popup entries or selected index, but it does mask the
ordinary selected-value summary until a future, separately proven reset
operation is introduced. Captions are therefore non-empty and sticky in the
initial API 1.4 implementation. Omitting `caption(...)` from a later edit
preserves the existing override.

```cpp
auto label = erui::StyledText{}
    .append(L"Profile: ")
    .append(
        L"custom",
        erui::TextStyle{}
            .color(erui::Color{107, 100, 80})
            .underline());

difficulty.edit().label(std::move(label)).commit();
```

Color and underline are supported by the effective vanilla settings movies.
Bold and italic requests are represented correctly, but their visible result
depends on matching font faces being available in the effective Scaleform font
configuration. ERNativeUI does not patch or replace font resources, so a
vanilla installation may render those two styles like ordinary text. A
compatible effective font configuration may make them visually distinct, but
that configuration has not yet been validated.

Structured text is separate from a future whole-row presentation color. A
colored fragment controls only the glyphs it contains; it does not replace the
native selected, unselected, highlighted, or disabled timeline behavior of the
row itself.

Before presentation work expands, the development showcase must live-validate
label and help replacement independently on every currently editable row type:
Button, Toggle, byte Slider, Numeric Slider, Custom Slider, Inline Choice,
Popup Choice, TextInput, and ColorPicker. The matrix uses explicit A/B variants
and tests both the Game Options root action layout and the distinct logical-
subpage action layout. A successful API commit alone is insufficient evidence;
the changed label and focused help must both remain visible through frame
updates, focus changes, leaving and reopening the page.

An explicit whole-row color could be replaced later or reset to the inherited
native color. Its exact representation remains provisional until the native
focus and disabled-state behavior is proven.

Whole-row label color requires its own proof gate. A direct RGB write is not
enough if the native timeline overwrites it during focus, selection, disable,
page transition, or reconstruction. The final API may use semantic native
colors instead of arbitrary RGB if that is the only stable behavior.

`interactive` and `visible` are deliberately different concepts:

- a non-interactive row remains in layout and pagination but cannot receive
  focus or activation, and uses a coherent disabled presentation; and
- an invisible row does not participate in layout or pagination.

If the currently focused row becomes non-interactive or invisible, ERNativeUI
must move focus deterministically to a valid row while preserving a working
Back path. A page whose rows are all non-interactive must still be escapable.
No row type will advertise runtime interactivity until both its visual state
and controller, keyboard, and mouse input behavior are proven.

The existing registration-time `enabled` option has different behavior for
different widgets—some rows are disabled while others are omitted—so API 1.4
must not silently reuse it as a universal runtime contract.

The admitted production scope covers Button, Toggle, byte Slider, Numeric
Slider, Custom Slider, Inline Choice, Popup Choice, TextInput, and ColorPicker.
Every typed edit draft separates focus eligibility from presentation:

```cpp
target.edit()
    .interactive(condition)
    .availability_appearance(
        erui::AvailabilityAppearance::follow_interactivity)
    .commit();
```

`follow_interactivity` selects the normal or disabled presentation from the
interaction state. `normal` and `disabled` force only that presentation; they
do not silently re-enable or disable input. Every supported row receives a
reversible disabled label treatment that preserves its authored transform.
Toggle and Inline Choice additionally preserve their construction-time native
movie twins and synchronize localized value text before swapping visibility.
Submenu remains excluded until its own native controller, focus relocation,
and page-stack behavior receive separate live proof.

### Recursive Collapse control and conditional content

A lightweight **Collapse control** is proposed as the convenient native-facing
way to organize optional settings. It owns a static ordered group of children,
uses one selectable header row, and toggles those children between expanded
and collapsed states.

API 1.4 also needs a headless **Group** for conditions driven by another
control. A Group owns children but consumes no native row; a Collapse is a
visible interactive header plus a Group. Both can contain ordinary controls,
Groups, and nested Collapse controls.

A Collapse may contain ordinary controls and other Collapse controls. The
hierarchy is therefore recursive:

```cpp
auto advanced = page.add_collapse(L"Advanced");
advanced.add(frame_limit);

auto expert = advanced.add_collapse(L"Expert");
expert.add(frame_pacing);
expert.add(shader_cache);
```

When introduced, the direct state API should use `collapse()`, `expand()`, and
`set_expanded(bool)`. It should not use `set_value(bool)`, because a bare
boolean does not communicate whether `true` means collapsed or expanded. The
typed edit draft uses `expanded(bool)` consistently with that vocabulary.

The hierarchy is declared during registration and remains acyclic: one child
has at most one parent, and runtime commits change state rather than create or
destroy registrations. A hierarchy belongs to one logical page, although its
flattened visible rows may naturally span multiple pagination slices.

The state rules are independent and composable:

- `visible(false)` on a Collapse hides its header and every descendant;
- `expanded(false)` keeps the Collapse header visible and hides descendants;
- `interactive(false)` preserves the current expanded state but prevents the
  player from toggling it; and
- collapsing a parent preserves every nested Collapse's expanded state, so
  expanding the parent restores the nested layout as it was.

The Collapse header consumes one ordinary pagination slot. Its compact native
expanded/collapsed indicator, focus behavior, mouse target, help text, and
disabled state require live proof before the control becomes public.

Conditional content remains more general than the Collapse control. A Toggle,
Slider, Inline Choice, Popup Choice, or arbitrary callback can update a
Collapse or directly change the visibility of dependent rows and groups in the
same batch.

The intended result includes:

- expanding or collapsing a group of rows;
- nesting collapsible groups without losing descendant state;
- selecting different row sets for different choice values;
- showing a variable number of rows for a slider or numeric range;
- recomputing pagination from the new visible logical content; and
- preserving a valid focus target and Back path after the layout changes.

Effective visibility combines every layer: a row is displayed only when it is
explicitly visible and every Collapse ancestor is visible and expanded. If a
commit hides the focused descendant, focus moves deterministically to its
nearest visible interactive Collapse header. If none exists, ERNativeUI tries
the next interactive row, then the previous row; Back always remains usable.
Expanding a Collapse leaves focus on its header.

If a commit would hide an active TextInput, ColorPicker, Popup Choice, or
another native editor, topology publication is deferred until that editor
closes. The host must not destroy an editor underneath Elden Ring.

A common declarative rule—such as “show this group while Toggle X is on”—may
be added after the explicit mutation model is proven. Arbitrary mod logic must
remain possible through callbacks and an update batch.

### Runtime tab visibility

A committed custom tab should be hideable and showable without unregistering
its provider. This uses the same published-generation model as rows but has
additional lifecycle requirements:

- hiding the active tab selects a deterministic surviving tab;
- the physical viewport and `...` cells are rebuilt from the visible logical
  catalogue;
- tab ordering remains stable while hidden tabs retain their logical identity;
- mouse, controller, wrapping, and dwell navigation remain coherent; and
- icon resources are retained or retired only at proven safe points.

This is visibility, not provider unload. Callback code, provider state, page
content, and the client DLL remain process-lifetime objects.
An ordinary row callback—including a Toggle callback—may request the tab
visibility change. Stock game tabs are outside the client visibility API.

## Required development sequence

The order is part of the compatibility design, not merely a scheduling
preference:

1. Freeze the exact published API 1.3 headers and prove frozen 1.3 clients
   against the current host.
2. Introduce the typed C++ row-reference foundation over the unchanged API 1.3
   C table, then rebuild and live-test the examples. No `edit()` object is
   exposed in this pass.
3. Add immutable model generations and the first complete mutation operation.
   Only this step needs a new API 1.4 C table and type-specific edit drafts.
4. Build presentation changes, conditional topology, repagination, Collapse,
   and tab visibility on that proven mutation foundation.

Step 1 is complete: the headers published by `ERNativeUI-v1.3.0` are frozen
with provenance, hashes, exact layout and contract fixtures, and public-only
C/C++ smoke clients against the production-sourced current host. Typed-row
work may now begin without rewriting the API 1.3 baseline. Runtime behavior in
step 2 must be indistinguishable from API 1.3; any menu, input, callback,
close/reopen, or navigation difference is a regression rather than an expected
consequence of the wrapper improvement.

### Compatibility constraints

The strict-C boundary remains append-only. Existing API 1.0–1.3 function
tables, descriptors, and raw row/page handles cannot change shape or meaning.
When the first new host operation is ready, Group, Collapse, and edit records
must be added through an API 1.4 table using sized structures, reserved fields,
and explicit field-presence flags so a complete batch can be validated before
publication.

The typed-row foundation does not itself require a new C operation. During
that milestone, `erui.h`, `ERUI_API_VERSION_CURRENT`, the exported host table,
and runtime negotiation remain at API 1.3. The `api-1.4` branch and alpha name
identify development toward 1.4; they do not justify advertising a C API 1.4
table before one exists. The C boundary moves to 1.4 only when the first
complete new host capability is implemented and tested.

The API 1.4 C++ wrapper keeps the existing alias unchanged:

```cpp
using RowHandle = ERUI_RowHandle;
```

It adds a small non-virtual `RowRef` value base and concrete final row types.
`Page::add_button`, `Page::add_toggle`, and the other row-building functions
return `ButtonRow`, `ToggleRow`, `SliderRow`, `NumericSliderRow`,
`CustomSliderRow`, `InlineChoiceRow`, `PopupChoiceRow`, `TextInputRow`, or
`ColorPickerRow` as appropriate. The base holds the raw ID plus the
provider/API context needed by typed operations; it is not an abstract runtime
interface and it owns no native game object.

```cpp
class RowRef {
public:
    [[nodiscard]] RowHandle handle() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
    operator RowHandle() const noexcept;
};

class ToggleRow final : public RowRef {
public:
    ERUI_Result set_value(bool value) const noexcept;
    ERUI_Result get_value(bool& value) const noexcept;
};

class ColorPickerRow final : public RowRef {
public:
    ERUI_Result set_color(Color value) const noexcept;
    ERUI_Result get_color(Color& value) const noexcept;
};
```

The foundation maps row types to the semantic operations already supported by
the API 1.3 host:

| Returned type | Typed operation |
|---|---|
| `ButtonRow` | No programmatic activation; common identity only |
| `ToggleRow` | `set_value(bool)` and `get_value(...)` |
| `SliderRow` | Byte-valued `set_value(...)` and `get_value(...)` |
| `NumericSliderRow` | Numeric `set_value(...)` and `get_value(...)` |
| `CustomSliderRow` | `set_index(...)` and `get_index(...)` |
| `InlineChoiceRow` | `set_index(...)` and `get_index(...)` |
| `PopupChoiceRow` | `set_index(...)` and `get_index(...)` |
| `TextInputRow` | `set_text(...)` and `get_text(...)` |
| `ColorPickerRow` | `set_color(...)` and `get_color(...)` |

Every concrete row is default-constructible to an invalid value, cheap to copy
and move, and safe to retain for the provider's process lifetime. It must keep
stable provider/API context rather than point at a temporary `Page`, draft, or
`Registration` wrapper. Type-specific getters and setters forward to existing
API 1.3 operations; they add C++ type safety and convenience without changing
host behavior. `ButtonRow` exposes only common identity/presentation behavior
and must not invent a method that programmatically invokes its callback.

The implicit conversion operator—not a raw-handle-only constructor—is what
preserves assignments to existing `RowHandle` variables and calls to existing
functions that accept them. Comparisons with `ERUI_INVALID_ROW` must also
remain unambiguous; dedicated row-reference/`RowHandle` comparisons should be
provided rather than left to incidental compiler conversion behavior.

This makes the ordinary API 1.3 source patterns continue to compile while
letting code that used `auto` acquire the typed API 1.4 wrapper naturally:

```cpp
auto difficulty = page.add_toggle(/* ... */);      // ToggleRow
RowHandle raw = page.add_toggle(/* ... */);        // Still valid; remains raw
registration.set_value(difficulty, 1);             // Converts to RowHandle
difficulty.set_value(true);                        // Typed convenience
```

The reverse conversion is deliberately contextual. A bare `RowHandle` does
not identify the provider or negotiated API table required to submit an edit,
so it must not silently construct a usable typed row by itself. Contextual
reacquisition from a retained raw handle may be designed later if a real use
case requires it; any such API must require the owning registration and the
expected row type.

`Page::add_submenu` remains unchanged during this foundation pass because it
returns a child `Page` as well as creating a row. A richer submenu/container
object belongs with the Group and recursive Collapse design, not in a nominal
row-return-type conversion.

The later mutation layer adds a type-specific edit draft to each concrete row:
`ButtonRowEdit`, `ToggleRowEdit`, `SliderRowEdit`, `NumericSliderRowEdit`,
`CustomSliderRowEdit`, `InlineChoiceRowEdit`, `PopupChoiceRowEdit`,
`TextInputRowEdit`, `ColorPickerRowEdit`, and `CollapseRowEdit`. Common draft
behavior may be factored internally without exposing one untyped public edit
object. No `edit()` member is published in the typed-row foundation milestone.

Already-compiled API 1.3 clients remain binary-compatible and continue using
the frozen API 1.3 table. Most API 1.3 C++ source also recompiles unchanged.
Only code that depends on the exact return type—for example `decltype`, a
pointer-to-member signature, template deduction, `auto` followed by raw-handle
reassignment, or a container keyed specifically by `RowHandle`—needs
attention. These cases require focused compatibility tests and a permanent,
concise `1.3-to-1.4` migration guide before the API is frozen.

## Workstream status

| Workstream | Current status | Required exit evidence |
|---|---|---|
| Freeze the shipped API 1.3 ABI fixture | Complete | Headers recovered from `ERNativeUI-v1.3.0`, immutable provenance and hashes, exact layout and bidirectional contract fixtures, and public-only frozen-client/current-host smoke tests |
| Typed C++ row foundation | Complete | Concrete typed rows returned by row builders, preserved raw `RowHandle`, one-way implicit raw conversion, type-safe forwarding, MSVC/MinGW tests, rebuilt examples, and live regression validation |
| Update batch and generation model | Initial row-update foundation complete; richer batching remains proposed | Unit-tested atomic validation, copy semantics, failure rollback, callback-originated commits, then multi-control coalescing without reentrancy |
| Mutable label and help text | Complete for the current presentation scope | Live callback updates passed on all editable row types, including focus changes, tab changes, editor transitions, and close/reopen reconstruction |
| Structured label and help text | Complete for the current presentation scope | Escaping and span-validation tests plus live color, underline, mixed-fragment, callback-update, tab-change, and close/reopen validation |
| Button and Popup Choice captions | Complete for the current presentation scope | Typed availability, atomic label/caption/help commits, Family A/B target mapping, styled captions, Popup selection independence, frame reapplication, and reconstruction all passed automated and live validation |
| Whole-row presentation color | Proposed; separate research gate | Proven behavior through focus, disable, transitions, and native timeline updates |
| Runtime interactivity | Complete for the admitted nine-family production scope | Every supported row skips focus and activation, labels and slider values dim and restore without discarding authored style, Toggle/Inline native value twins stay localized, callback self-disable relocates focus safely, and reopen/mouse/controller/tab-switch regression passes; Submenu remains excluded |
| Recursive Collapse control | Proposed; blocked by update foundation | Acyclic nested groups, preserved descendant state, native header presentation, focus relocation, and pagination flattening |
| Conditional groups and row visibility | Proposed; blocked by update foundation | Correct visible model, active-editor policy, focus relocation, and atomic publication |
| Dynamic repagination | Proposed; blocked by row visibility | First/middle/last slice changes, navigation rows, provider coexistence, and no stranded pages |
| Runtime custom-tab visibility | Proposed; blocked by catalogue lifecycle proof | Active-tab fallback, viewport shifts, wrap/dwell/mouse, reopen stress, and icon lifetime |
| Alpha build pipeline | Implemented; initial manual publication succeeded, deletion/reuse rehearsal pending | Manual milestone selection, exact-SHA package provenance, tests, prerelease publication, rollback instructions, and clean host/client mismatch failure |

## Milestones

### R0 — compatibility and alpha foundation

- API 1.3 is frozen from the published 1.3.0 tag before any public-header
  change: the headers are copied byte-for-byte and their provenance and hashes
  are pinned.
- The registered release fixture preserves the exact C/C++ layout and contract,
  exercises both compatibility directions, and proves the current-host path
  without requiring a test-only production-host export.
- Preserve frozen API 1.0 through 1.3 clients against every development host.
- Establish the `api-1.4` branch, required Windows x64 CI, alpha provenance,
  and unmistakable unstable-package naming.
- Keep Release Please, the stable changelog, stable version metadata, and
  Nexus packages on `main` only.

### R1 — typed C++ row foundation

- Keep `RowHandle` as the raw `ERUI_RowHandle` alias and keep the host's C API
  version, table, capabilities, and negotiation at API 1.3.
- Add the non-virtual `RowRef` value base and concrete row types for every
  ordinary row builder; keep `Page::add_submenu` unchanged.
- Make typed rows default-invalid, cheap to copy and move, explicit through
  `handle()`, and implicitly convertible to `RowHandle`.
- Forward type-specific getters and setters through the existing API 1.3 table
  while making operations for the wrong row type unavailable at compile time.
- Do not add `edit()`, edit drafts, Group, Collapse, or a placeholder API 1.4 C
  table in this milestone.
- Test exact `add_*` return types, legacy assignment and parameter passing,
  invalid-handle comparisons, default/copy/move/non-polymorphic traits,
  context lifetime, failed registration, forwarding, and wrong-method absence.
- Run the frozen-client/current-host matrix and the wrapper tests with MSVC and
  MinGW, then rebuild all examples against the same header revision.
- Deploy the development host and recompiled examples for live regression of
  callbacks, input, navigation, pagination, custom tabs, and repeated
  close/reopen. Runtime behavior must remain identical to API 1.3.

### R2 — immutable model generations and typed edits

- Define provider-scoped mutation records and a complete validation pass.
- Define move-only C++ edit drafts, rvalue-safe fluent setters, synchronous
  batch ownership, and no implicit commit on destruction.
- Add type-specific edit drafts to the concrete rows and introduce the API 1.4
  C table only with the complete host mutation operation behind them.
- Reject cross-provider and stale drafts, and choose an explicit policy for
  multiple drafts that target the same control.
- Exercise individual and batched commits from native-dispatched and
  host-polled callbacks; neither path may deadlock or publish reentrantly.
- Build a new immutable logical generation without touching the live one.
- Publish or reject the generation atomically at a safe boundary.
- Coalesce multiple changes into one committed generation and expose
  completion/failure without synchronous native reentrancy.

### R3 — presentation-only live changes

- Prove label and help replacement across every supported page lifecycle.
- Add validated `StyledText` fragments while preserving literal semantics for
  every existing plain-string overload.
- Prove runtime interactivity for each admitted row type.
- Research whole-row presentation color without weakening Elden Ring's
  focus/disabled states.
- Publish the first runtime-only alpha only after stable API 1.0–1.3 paths
  pass unchanged.

### R4 — conditional topology and pagination

- Add groups and row visibility to the logical model.
- Add the recursive Collapse tree and flatten its effective visible rows.
- Recompute pagination from visible content in one generation.
- Define active-row, active-editor, page-slice, and empty-page policies.
- Stress repeated expand/collapse updates, callbacks, mouse/controller input,
  and multiple providers before exposing an SDK alpha.

### R5 — custom-tab visibility

- Rebuild the visible logical tab catalogue without unregistering providers.
- Define deterministic fallback when the active tab becomes hidden.
- Validate physical viewports of 8, 9, and 10 slots plus overflow, wrapping,
  dwell, mouse selection, fast close/reopen, and icon-resource retirement.

### R6 — stable API candidate

- Freeze the final append-only API 1.4 table after the last SDK alpha.
- Complete strict-C/C++ contracts, capability bits, examples, reference,
  migration guide, feature matrix, compatibility matrix, and What's New page.
- Run the full automated and live validation matrix, including older clients.
- Merge through the protected `main` release path and let Release Please create
  the ordinary stable release.

## Alpha policy

Alphas are manually selected, opt-in engineering milestones, not ordinary
player releases or per-commit builds. The current automated alpha package is
runtime-only: it contains the host and same-commit examples, and deliberately
omits draft public headers and CMake SDK files. Third-party SDK alphas begin
only after the draft table is stable enough to reject a mismatched revision
cleanly.

Each alpha identifies its immutable tag and commit and must preserve the
stable API 1.0–1.3 paths. A future SDK alpha must pair its host, headers,
wrapper, examples, and CMake package from that exact commit. Every alpha follows
the complete [alpha build policy](../contributing/alpha-builds.md), and
alpha-to-alpha ABI compatibility is not promised.

## Non-goals

API 1.4 does not propose:

- provider unload or client DLL hot-unload;
- late registration of entirely new providers;
- arbitrary mod-authored GFX execution;
- a generic Scaleform-object API;
- direct exposure of game pointers or addresses; or
- weakening validation to make an unsupported game build appear compatible.

Those operations have different ownership and safety requirements from
rebuilding ERNativeUI's already-owned logical model.

## Progress log

Add short, evidence-based entries here when a workstream changes state. Link
the implementation, automated tests, research note, and first alpha that
contains it. Do not use this log as a substitute for the stable changelog.

| Date | Change | Evidence or build |
|---|---|---|
| 2026-10-25 | Completed runtime availability for Button, Toggle, byte Slider, Numeric Slider, Custom Slider, Inline Choice, Popup Choice, TextInput, and ColorPicker while keeping Submenu gated. Interaction and `AvailabilityAppearance` remain independent. | Exact per-family controller validation, reversible Scaleform presentation ownership, atomic registry/state tests, and live nine-family/ten-target matrix validation across repeated enable/disable, tab switching, styled-color restoration, and System reopen cycles |
| 2026-10-24 | Moved the proven Toggle availability seam into the API 1.4 production candidate with independent interaction and `AvailabilityAppearance`. | Exact-build predicate/Scaleform validation, atomic registry/state tests, and a focused showcase covering forced appearance, initially disabled reversal, and callback self-disable |
| 2026-10-24 | Added and live-validated a typed, sticky right-hand caption override for Button and Popup Choice edits. Captions share the validated structured-text model; unsupported controls do not expose the operation. | ABI/layout, atomic registry, stable-buffer, Family A/B mapping, typed-wrapper, and showcase regression tests plus live root/subpage, styled caption, Popup selection, repaint, and reconstruction validation |
| 2026-10-23 | Structured row label/help text moved from research into the API 1.4 production path. Plain text remains escaped; allowlisted fragments support color and underline, while bold/italic remain dependent on effective font faces. | Scaleform HTML escaping/span regression tests and live label/help style proof across callback updates, tab changes, and System close/reopen |
| 2026-10-22 | Typed C++ row foundation completed without changing the frozen C API 1.3 contract; automated MSVC/MinGW validation and live Tarnished UI Showcase/Custom Tabs Stress regression passed. | `ERNativeUI.ABI.TypedRows.CurrentProductionCpp`, the API 1.0–1.3 compatibility matrix, and live development-build validation |
| 2026-10-22 | Manual runtime-alpha packaging and GitHub prerelease workflow added; initial `api-1.4` publication succeeded, with deletion/reuse rehearsal still pending. | [Alpha build policy](../contributing/alpha-builds.md) |
| 2026-10-22 | Initial reactive-menu roadmap recorded; all API 1.4 workstreams remain proposed. | This roadmap |

[Roadmap index](README.md) | [Alpha builds](../contributing/alpha-builds.md) |
[Current stable features](../features.md)

