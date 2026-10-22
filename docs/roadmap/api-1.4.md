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
or a batch. The following names are design illustrations, not a committed API:

```cpp
difficulty.update()
    .label(L"Enemy Damage")
    .help(L"Applied by the active balance profile.")
    .commit();
```

The equivalent multi-control form keeps every edit in one atomic commit:

```cpp
auto updates = registration.updates();

updates.add(difficulty)
    .label(L"Enemy Damage")
    .help(L"Applied by the active balance profile.")
    .label_color(erui::Color{107, 100, 80})
    .interactive(profile_loaded);

updates.add(advanced_group)
    .visible(show_advanced);

updates.add(expert)
    .expanded(show_expert);

updates.commit();
```

A control-level `commit()` submits a single-entry batch. The provider-level
`updates.commit()` submits every added entry as one batch. These are two C++
conveniences over one host operation, not two update mechanisms.

`commit()` means that the complete desired-state update was validated, copied,
and queued. It does not promise that Scaleform has already repainted before the
call returns. ERNativeUI may safely repaint only the affected live page while
an offscreen page consumes the new generation when it is next materialized.
The mod receives one atomic logical result without depending on native object
lifetime or reconstruction details.

Every supplied string must be copied before the client call returns. The live
menu changes only after the complete batch passes validation. A failure keeps
the previous published generation intact.

Callbacks may stage and submit an update, but they must not recursively
destroy or rebuild the native menu while Elden Ring is dispatching that
callback. Publication therefore occurs asynchronously at a proven safe point.

### Mutable row presentation

The planned row-level mutations are:

- label text;
- help text;
- label color, if native selected, unselected, and disabled states can be
  preserved reliably; and
- runtime interactivity.

An explicit color can be replaced later or reset to the inherited native
color. The exact color representation remains provisional until the native
focus and disabled-state behavior is proven.

Label color requires its own proof gate. A direct RGB write is not enough if
the native timeline overwrites it during focus, selection, disable, page
transition, or reconstruction. The final API may use semantic native colors
instead of arbitrary RGB if that is the only stable behavior.

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

### Compatibility constraints

API 1.4 remains append-only. Existing API 1.0–1.3 functions, descriptors, and
raw row/page handles cannot change shape or meaning. New Group, Collapse, and
update handles must be additive, and the C boundary must use sized records,
reserved fields, and explicit field-presence flags so a complete batch can be
validated before publication.

The C++ wrapper may add friendly control references to support `update()` and
`updates.add(...)`, but existing raw-handle client source must continue to
compile. The exact wrapper acquisition syntax therefore remains provisional
until source-compatibility tests exist.

## Workstream status

| Workstream | Current status | Required exit evidence |
|---|---|---|
| Freeze the shipped API 1.3 ABI fixture | Proposed; mandatory first gate | Headers recovered from `ERNativeUI-v1.3.0`, immutable hash/layout fixture, and old-client/current-host tests |
| Update batch and generation model | Proposed | Unit-tested atomic validation, copy semantics, coalescing, failure rollback, and thread ownership |
| Mutable label and help text | Proposed | Live selected/unselected rows, pagination slices, close/reopen, and movie reconstruction |
| Label color | Proposed; separate research gate | Proven behavior through focus, disable, transitions, and native timeline updates |
| Runtime interactivity | Proposed | Every admitted row type skips focus and activation while preserving navigation and help semantics |
| Recursive Collapse control | Proposed; blocked by update foundation | Acyclic nested groups, preserved descendant state, native header presentation, focus relocation, and pagination flattening |
| Conditional groups and row visibility | Proposed; blocked by update foundation | Correct visible model, active-editor policy, focus relocation, and atomic publication |
| Dynamic repagination | Proposed; blocked by row visibility | First/middle/last slice changes, navigation rows, provider coexistence, and no stranded pages |
| Runtime custom-tab visibility | Proposed; blocked by catalogue lifecycle proof | Active-tab fallback, viewport shifts, wrap/dwell/mouse, reopen stress, and icon lifetime |
| Alpha build pipeline | Implementing; first GitHub publication pending | Manual milestone selection, exact-SHA package provenance, tests, prerelease publication, rollback instructions, and clean host/client mismatch failure |

## Milestones

### R0 — compatibility and alpha foundation

- Freeze API 1.3 from the published 1.3.0 tag before changing public headers.
- Preserve frozen API 1.0 through 1.3 clients against every development host.
- Establish the `api-1.4` branch, required Windows x64 CI, alpha provenance,
  and unmistakable unstable-package naming.
- Keep Release Please, the stable changelog, stable version metadata, and
  Nexus packages on `main` only.

### R1 — immutable model generations

- Define provider-scoped mutation records and a complete validation pass.
- Build a new immutable logical generation without touching the live one.
- Publish or reject the generation atomically at a safe boundary.
- Coalesce multiple changes into one committed generation and expose
  completion/failure without synchronous native reentrancy.

### R2 — presentation-only live changes

- Prove label and help replacement across every supported page lifecycle.
- Prove runtime interactivity for each admitted row type.
- Research label color without weakening Elden Ring's focus/disabled states.
- Publish the first runtime-only alpha only after stable API 1.0–1.3 paths
  pass unchanged.

### R3 — conditional topology and pagination

- Add groups and row visibility to the logical model.
- Add the recursive Collapse tree and flatten its effective visible rows.
- Recompute pagination from visible content in one generation.
- Define active-row, active-editor, page-slice, and empty-page policies.
- Stress repeated expand/collapse updates, callbacks, mouse/controller input,
  and multiple providers before exposing an SDK alpha.

### R4 — custom-tab visibility

- Rebuild the visible logical tab catalogue without unregistering providers.
- Define deterministic fallback when the active tab becomes hidden.
- Validate physical viewports of 8, 9, and 10 slots plus overflow, wrapping,
  dwell, mouse selection, fast close/reopen, and icon-resource retirement.

### R5 — stable API candidate

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
| 2026-10-22 | Manual runtime-alpha packaging and GitHub prerelease workflow added; first GitHub publication remains pending. | [Alpha build policy](../contributing/alpha-builds.md) |
| 2026-10-22 | Initial reactive-menu roadmap recorded; all API 1.4 workstreams remain proposed. | This roadmap |

[Roadmap index](README.md) | [Alpha builds](../contributing/alpha-builds.md) |
[Current stable features](../features.md)

