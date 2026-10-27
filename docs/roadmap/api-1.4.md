# ERUI API 1.4 release roadmap: runtime UI updates

> **Status: feature-complete, pre-freeze release preparation.** The API 1.4
> feature scope is complete, but the public contract has not been frozen or
> published. The remaining gates are final compatibility validation, contract
> freeze, migration and release documentation, packaging, and the protected
> release process.

API 1.4 gives a client mod safe runtime control over UI it registered. It adds
typed C++ row, Submenu, and custom-tab references; one atomic provider-scoped C
update operation; one-target C++ edit drafts; styled text; runtime availability;
explicit row visibility and live repagination; rich Submenu text edits; and
custom-tab presentation edits.

Recursive Collapse controls and headless Groups remain outside this release.
Rows inside an API 1.3 custom tab receive the same API 1.4 update behavior as
rows on supported built-in pages and subpages, while API 1.4 can also retain
and edit the custom tab itself.

The task-oriented [row-update guides](../guides/row-updates/README.md),
[Submenu guide](../guides/controls/submenu.md), and
[custom-tab guide](../guides/custom-tabs.md) describe normal use. The public C
and C++ headers remain the normative contract.

## API 1.4 feature scope

| Workstream | Delivered surface | Status |
|---|---|---|
| Compatibility foundation | Frozen API 1.0 through 1.3 prefixes and clients remain supported by the append-only 1.4 host | Complete |
| Typed C++ rows | Concrete row types, retained provider/API context, implicit conversion to raw `RowHandle`, and type-specific value helpers | Complete |
| Atomic update boundary | Provider-scoped `ERUI_UpdateBatchDesc` plus one-row C++ `edit().commit()` drafts | Complete |
| Runtime text | Label, focused help, and supported Button/Popup Choice captions using plain or structured text | Complete |
| Runtime availability | Independent interaction and `AvailabilityAppearance` for the nine validated ordinary row families | Complete |
| Runtime visibility | Explicit `.visible(bool)` with in-place reconstruction, focus repair, pagination, titles, navigation rows, and subpage scrollbar reconciliation | Complete |
| Rich Submenu editing | Retained `Submenu`, atomic opener label/help/caption and child-page title/help edits | Complete |
| Rich custom-tab editing | Retained `Tab`, atomic title/icon/visibility/preferred-order edits, active-tab fallback, and fast-reopen icon recovery | Complete |

### Typed row references and values

Ordinary `Page::add_*` builders return concrete non-polymorphic row values:
`ButtonRow`, `ToggleRow`, `SliderRow`, `NumericSliderRow`, `CustomSliderRow`,
`InlineChoiceRow`, `PopupChoiceRow`, `TextInputRow`, and `ColorPickerRow`.
Each value keeps the opaque row ID and provider/API context required by later
operations; it never points at a game or Scaleform object.

The frozen raw alias remains available:

```cpp
using RowHandle = ERUI_RowHandle;

auto difficulty = page.add_toggle(/* ... */); // ToggleRow
RowHandle raw = difficulty;                   // Source-compatible conversion
```

Typed value methods expose only operations valid for that control family.
They forward to the same canonical host-owned values introduced in earlier
APIs. A raw handle cannot silently reconstruct a typed row because it lacks
the owning provider and negotiated API context. `Page::add_submenu` now returns
a `Submenu` that remains a child-page builder, and `Menu::add_tab` returns a
`Tab` that remains an ordinary page builder; both retain stable edit identity.

### Atomic target edits

The strict-C table appends `commit_updates`. One call validates and copies an
entire `ERUI_UpdateBatchDesc` before publication. Every row or page target must
belong to the submitting provider, duplicate targets are rejected, and any
invalid item rejects the complete batch without changing the published
generation. Tab batches may contain several distinct tabs but do not mix tab
items with row or Submenu items.

The C++ wrapper builds a move-only, one-shot draft from the typed row:

```cpp
difficulty.edit()
    .label(L"Enemy Damage")
    .help(L"Applied by the active balance profile.")
    .interactive(profile_loaded)
    .commit();
```

Destroying a draft has no effect. `commit()` validates and copies the request
synchronously, then applies its native presentation at a proven UI-safe
boundary. It is safe to commit from an ERNativeUI callback; the callback
finishes against the generation with which it started, without recursive
native reconstruction.

The C operation can atomically submit several compatible targets. API 1.4's
C++ convenience surface intentionally commits one row, Submenu, or Tab draft
at a time; it does not include a `registration.updates()` collector.

### Text and structured presentation

`label(...)` changes the left-hand row text, `help(...)` changes the focused
footer, and `caption(...)` changes the right-hand text for Button and Popup
Choice rows. Omitting a field preserves its current value.

Plain strings remain literal and are escaped before Scaleform receives them.
`StyledText` adds validated spans for opaque RGB color, underline, bold,
italic, or combinations. Color and underline are live-validated with the
effective vanilla movies. Bold and italic are encoded, but their visible
result depends on compatible font faces; ERNativeUI does not patch fonts.

See [Change row text and style](../guides/row-updates/text-and-styles.md).

### Runtime row availability

`.interactive(bool)` controls whether a materialized row can receive focus or
activation. `AvailabilityAppearance` independently chooses whether its native
presentation follows interaction, remains normal, or remains disabled.

The admitted families are Button, Toggle, byte Slider, Numeric Slider, Custom
Slider, Inline Choice, Popup Choice, TextInput, and ColorPicker. Their native
label/value presentation is restored after repeated disable/enable cycles
without discarding authored structured text. Submenu is excluded.

See [Enable or disable rows](../guides/row-updates/availability.md).

### Visibility and repagination

`.visible(false)` removes a row from layout and pagination while preserving
its canonical value. `.visible(true)` reconstructs it from the latest logical
state. The operation supports the same nine ordinary materialized families as
runtime availability; Submenu remains excluded.

An accepted update recomputes visible order, the active physical slice,
Previous/Next rows, page titles, focus, and the native subpage scrollbar. A
live page rebuilds in place without a close/open transition. Destructive work
is deferred while a native editor owns input, and the final projection may not
leave a logical page empty.

The complete lifecycle is live-validated on:

- a supported built-in page;
- the content page of an API 1.3 custom tab; and
- a provider subpage.

Custom-tab visibility is a separate `TabEdit` field and does not participate
in row repagination.

See [Show or hide rows](../guides/row-updates/visibility.md).

### Rich Submenu edits

`Page::add_submenu` returns a `Submenu` that is both the registration-time
child-page builder and the stable reference to its opener/page pair. Its
move-only `SubmenuEdit` atomically changes opener label, help, caption,
child-page title, and child-page help. Omitted fields retain their latest
values, structured text is admitted for the three opener fields, and an open
child page refreshes without replacing its rows or navigation identity.

Submenu availability and visibility remain excluded because those operations
affect the page stack and reachability rather than one ordinary materialized
row.

See [Submenu](../guides/controls/submenu.md#edit-a-committed-submenu).

### Rich custom-tab edits

`Menu::add_tab` returns a `Tab` that is both the registration-time page builder
and a stable runtime reference. `TabEdit` atomically changes title, exact 32x32
RGBA8 icon (or the shared default triangle), visibility, and preferred order.
The registered page, rows, values, and provider/tab identity remain unchanged.
Player `TabOrder` overrides remain authoritative over the preferred-order hint.

Admitted fields normally publish live in an open System movie. Icon generations
use a complete immutable resource bank and an atomic host/property-ledger
handoff, so no stock fallback or partially updated frame is exposed. If System
closes during that handoff, the desired revision remains queued and is adopted
in the first reopened movie without a second reopen. A strip change that cannot
preserve the current physical geometry is deferred to the next movie rather
than risking an unsafe live rebuild. Hiding the active tab repairs selection
deterministically. Icon edits are low-frequency state changes rather than an
animation API because native image roots are retained for process safety.

See [Custom Configuration tabs](../guides/custom-tabs.md#edit-a-committed-tab).

## Compatibility contract

- The C ABI remains append-only. The API 1.0 through 1.3 table prefixes and
  their published meanings do not change.
- Already-compiled earlier clients continue requesting their exact frozen
  table. There is no silent API upgrade or downgrade.
- API 1.4 descriptors use exact sizes, explicit field-presence bits, zeroed
  reserved fields, copied strings/icon bytes, and provider ownership
  validation.
- Clients test capability bits rather than inferring native support from the
  API number alone. Unsupported game builds fail closed.
- Row references, edit drafts, and public descriptors never expose game
  pointers, Scaleform objects, or host allocator ownership.
- Construction-disabled Button and ColorPicker rows may be omitted by their
  native construction path and therefore cannot be made available or visible
  later.

## Validation completed for the feature scope

| Area | Evidence |
|---|---|
| ABI and source compatibility | Frozen API 1.0–1.3 fixtures, current-host negotiation tests, typed-row source compatibility, and MSVC/MinGW compilation coverage |
| Update transactions | Provider ownership, exact descriptor validation, copied storage, duplicate-target rejection, rollback, callback-originated commits, and immutable generation publication tests |
| Text and style | Escaping/span tests plus live callback updates, focus changes, tab changes, native editor transitions, and repeated System close/reopen |
| Availability | Live nine-family matrix covering focus skipping, activation blocking, native dim/restore behavior, authored-color restoration, callback self-disable, mouse/controller navigation, tab changes, and reopen cycles |
| Visibility and pagination | Repeated full/compact/minimal projections on a built-in page, custom-tab content, and a subpage, including navigation-row removal, focus restoration, no transition/flash, and exact scrollbar reconciliation |
| Rich Submenu editing | Atomic A/B generations for opener and child-page text, nested retained Submenus, pagination titles, active child-page refresh, callbacks, tab changes, and reopen cycles |
| Rich custom-tab editing | Live title/order/visibility/icon generations, active-tab fallback, overflow navigation, exact immutable-resource accounting, rapid close/reopen recovery, and repeated controller/mouse selection |
| Legacy behavior | API 1.3 examples and custom-tab stress paths rebuilt and live-regressed against the development host before the 1.4-only surfaces were added |

## Deliberately deferred beyond API 1.4

| Idea | Why it is not in 1.4 |
|---|---|
| Headless conditional Groups and recursive Collapse controls | They require a new ownership tree, nested-state rules, header presentation, and additional focus semantics. Explicit row visibility already provides the proven primitive without freezing that larger design. |
| C++ multi-row update collector | The atomic C batch is available, while the ergonomic ownership and duplicate-target policy for a fluent C++ collector can be designed independently later. |
| Arbitrary whole-row presentation color | Structured text colors are supported. Replacing every native selected, focused, and disabled timeline color needs a separate semantic design. |
| Runtime Submenu availability or visibility | Submenu mutation affects the page stack and controller lifetime, unlike an ordinary row. It remains rejected by the 1.4 runtime availability and visibility operations. |

Deferral is intentional scope control, not an implied API 1.5 promise. Each
idea must pass its own native-lifecycle and compatibility gates before a later
roadmap can adopt it.

## Remaining publication gates

No additional API 1.4 feature work is planned. The release pass still needs
to:

1. freeze the final 1.4 C and C++ headers into a `v1_4` ABI fixture and pin
   their provenance and hashes;
2. add the permanent API 1.3-to-1.4 migration guide and API 1.4 What's New
   page;
3. run the complete Windows x64, frozen-client/current-host, strict-C, C++17,
   MSVC, and MinGW validation matrix;
4. update the final compatibility and validation records with the release
   build identity;
5. build and inspect the complete SDK/runtime package; and
6. merge through the protected `main` path so Release Please can prepare the
   stable release.

## Completed milestones

| Milestone | Outcome |
|---|---|
| R0 — compatibility and alpha foundation | API 1.3 frozen; protected branch, CI, and reproducible manual alpha publication established |
| R1 — typed C++ rows | Concrete typed references and legacy raw-handle source compatibility implemented and validated |
| R2 — atomic row edits | API 1.4 C table, immutable update generations, validation, callback safety, and one-row C++ drafts completed |
| R3 — presentation and availability | Runtime text, structured styles, captions, interaction, and independent availability appearance completed |
| R4 — visibility and pagination | Explicit row visibility and in-place native reconstruction completed across all three target page surfaces |
| R5 — rich container edits | Retained Submenu and Tab references, live atomic presentation updates, exact icon handoff, and fast-reopen recovery completed |
| R6 — stable publication | Current: fixtures, migration/What's New documentation, final validation, packaging, and protected release |

## Progress log

| Date | Change | Evidence or build |
|---|---|---|
| 2026-10-27 | Expanded the pre-freeze scope with rich Submenu and custom-tab editing after completing their additional lifecycle proof. | Live opener/page text generations plus tab title/order/visibility/icon updates, rapid close/reopen icon recovery, and Release/Debug full suites |
| 2026-10-26 | Closed the API 1.4 feature scope. Promoted the proven subpage scrollbar reconciliation into production, finalized the row-update guides and screenshot scenes, and deferred Collapse/custom-tab mutation beyond 1.4. | Repeated live full/compact/minimal rebuilds on built-in, custom-tab, and subpage surfaces; no page transition, flash, duplicate/missing row, stale focus, stale title, or stale scrollbar |
| 2026-10-25 | Completed explicit row visibility, dynamic repagination, and runtime availability for the nine admitted row families. | Atomic registry/state tests and live controller/mouse matrices across repeated updates, tab changes, and System reopen cycles |
| 2026-10-24 | Completed typed captions and independent interaction/availability appearance. | ABI/layout, stable-buffer, native mapping, focus relocation, reconstruction, and callback self-disable tests |
| 2026-10-23 | Completed plain and structured runtime label/help updates. | Scaleform escaping/span tests and live color, underline, callback, focus, tab-change, and close/reopen validation |
| 2026-10-22 | Completed the typed C++ row foundation after freezing API 1.3. | Frozen-client/current-host matrix, typed-wrapper tests, MSVC/MinGW coverage, and live example regression |
| 2026-10-22 | Added the reproducible manual API-alpha workflow. | [Alpha build policy](../contributing/alpha-builds.md) and successful initial publication |

[Roadmap index](README.md) | [Row-update guides](../guides/row-updates/README.md) |
[Alpha builds](../contributing/alpha-builds.md) | [Feature matrix](../features.md)
