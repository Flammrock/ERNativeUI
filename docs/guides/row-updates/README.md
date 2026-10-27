# Row updates

<table>
  <tr>
    <td align="center" valign="middle">
      <a href="../capabilities.md#what-a-capability-is"><img src="../../assets/images/badges/capabilities.svg" alt="Capabilities" width="112" height="22" align="top"></a>
    </td>
    <td valign="middle">
      <a href="../capabilities.md#updates"><code>updates</code></a> ·
      <a href="../capabilities.md#row-availability"><code>row_availability</code></a> ·
      <a href="../capabilities.md#row-visibility"><code>row_visibility</code></a>
    </td>
  </tr>
</table>

API 1.4 lets a mod retain the typed object returned when it creates a row and
later update that logical row safely. This page explains the shared
`row.edit()...commit()` model used by text, availability, and visibility
updates.

If you have not registered an ERNativeUI menu before, first read
[Build your first client mod](../../getting-started/first-mod.md). The examples
below assume that `erui::connect()` succeeded and that you already prepared an
`erui::Connection` and `erui::ProviderOptions`.

## The edit/commit model

There are four steps:

1. A `Page::add_*` call creates a logical row and returns its typed reference,
   such as `erui::ToggleRow` or `erui::SliderRow`.
2. Your mod retains that typed reference in state that outlives every callback
   which may use it.
3. `row.edit()` creates an inert, move-only draft. Each chained setter stages
   one field on that draft; fields you do not mention keep their current value.
4. `commit()` validates and copies the staged update, then returns an
   `ERUI_Result`. A successful native repaint happens later at a UI-safe
   boundary.

Given a retained `erui::ToggleRow` named `difficulty` (the complete setup is
shown next):

```cpp
const ERUI_Result result = difficulty.edit()
    .label(L"Enemy Damage: Hard")
    .help(L"The Hard balance profile is active.")
    .commit();

if (result != ERUI_OK) {
    // Report or log the numeric result for troubleshooting.
}
```

This edit changes only `label` and `help`. It does not change the Toggle's
value, interaction, appearance, or visibility.

## Complete minimal setup

This example shows where `difficulty` comes from and when it becomes safe to
edit:

```cpp
#include <ernativeui/ERNativeUI.hpp>

namespace {

struct UiState {
    erui::ToggleRow difficulty{};
    erui::Registration registration{};
};

// Keep this state alive for as long as callbacks can use the row.
UiState g_ui;

ERUI_Result set_hard_difficulty(bool hard) noexcept
{
    return g_ui.difficulty.edit()
        .label(hard ? L"Enemy Damage: Hard" : L"Enemy Damage: Normal")
        .help(hard
            ? L"The Hard balance profile is active."
            : L"The Normal balance profile is active.")
        .commit();
}

bool register_rows(
    const erui::Connection& connection,
    const erui::ProviderOptions& provider_options) noexcept
{
    if (!connection.supports(erui::Capability::toggle) ||
        !connection.supports(erui::Capability::updates)) {
        return false;
    }

    const auto result = connection.register_menu(
        provider_options,
        [](erui::Menu& menu) {
            g_ui.difficulty = menu.root().add_toggle(
                L"Enemy Damage",
                L"Choose the active balance profile.",
                0); // Initial Toggle value: Off.
        });

    if (!result) {
        return false;
    }

    g_ui.registration = result.value();

    // The provider is committed now, so the retained row can be edited.
    return set_hard_difficulty(false) == ERUI_OK;
}

} // namespace
```

`g_ui.difficulty` is not a native widget pointer. It is a small typed reference
to ERNativeUI's logical row plus the provider/API context needed to submit an
edit. Pagination, page reconstruction, leaving System, and reopening it do not
invalidate that logical reference.

Do not call `edit().commit()` from inside the `register_menu` builder. Retain
the row there, wait for `register_menu` to succeed, and then submit the initial
runtime presentation as shown above.

## Handle every result

Always check the result at the call site:

```cpp
const ERUI_Result result = difficulty.edit()
    .help(L"The Hard balance profile is active.")
    .commit();

if (result != ERUI_OK) {
    // Follow your mod's logging and fallback policy.
}
```

The central [Error handling](../../reference/error-handling.md) reference
defines the result types, every result code, recovery policy, and the
difference between synchronous acceptance and later native presentation.

## Setters are optional and preserve omitted fields

A new draft does not reset anything. Only a setter you actually call is part
of that commit:

| Setter | What it changes | Initial logical state |
|---|---|---|
| `.label(...)` | Left-hand row text | Text passed to `add_*` |
| `.help(...)` | Footer shown while focused | Help passed to `add_*`; empty help is allowed |
| `.caption(...)` | Right-hand text on supported Button and Popup Choice rows | The control's registered/native caption until overridden |
| `.interactive(bool)` | Focus and activation eligibility | The row's registration-time `enabled` state |
| `.availability_appearance(...)` | Normal/disabled visual policy, independently of input | `follow_interactivity` |
| `.visible(bool)` | Participation in layout and pagination | `true` for a materialized row |

Inline Choice, Popup Choice, and TextInput do not expose a registration-time
`enabled` option and initially start interactive. Other supported controls use
their registration argument or option, whose ordinary default is `true`.

For example, this changes input eligibility but preserves the current
appearance policy:

```cpp
const ERUI_Result result = difficulty.edit()
    .interactive(false)
    .commit();

if (result != ERUI_OK) {
    // The previous interaction state remains published.
}
```

The initial appearance policy is `follow_interactivity`, so an ordinary row
that has never overridden the policy will also look disabled. If an earlier
edit forced `normal` or `disabled`, omitting `.availability_appearance(...)`
preserves that forced policy. Set both fields together when you want to state
the complete ordinary enable/disable behavior explicitly.

Destroying an uncommitted draft does nothing. The first `commit()` attempt
consumes the draft even if validation fails; call `row.edit()` again to retry
or submit the next update. Strings and style spans are copied before
`commit()` returns, so temporary `std::wstring` and `erui::StyledText` values
are safe.

## Capabilities

Check capabilities before registering a feature-dependent scene or submitting
its updates:

```cpp
const bool can_edit_text =
    connection.supports(erui::Capability::updates);

const bool can_change_availability =
    can_edit_text &&
    connection.supports(erui::Capability::row_availability);

const bool can_change_visibility =
    can_edit_text &&
    connection.supports(erui::Capability::row_visibility);
```

The same `supports(...)` query is available on `erui::Menu` during
registration and on `erui::Registration` afterward. Prefer capability checks
over API-number comparisons.

## Choose the update you need

ERNativeUI keeps a control's value, text, interaction, appearance, and layout
presence separate:

| Goal | Guide | Result |
|---|---|---|
| Change a row label, focused help, supported caption, color, or underline | [Text and style](text-and-styles.md) | The same logical row receives new presentation text. |
| Remove or restore a row and recompute pagination | [Visibility](visibility.md) | The row stops or starts consuming layout space. |
| Keep a row visible but disable or enable player interaction | [Runtime row availability](availability.md) | The row remains in layout while input and appearance can change independently. |
| Read or change the control's canonical value | [Values](values.md) | The host-owned value changes without pretending it was player input. |
| Publish changes to several rows as one all-or-nothing generation | [Atomic batching](batching.md) | A strict-C client submits several updates in one commit. |

The presentation guide includes the complete
[`erui::TextStyle` reference](text-and-styles.md#eruitextstyle-reference),
including color, underline, bold, italic, style scope, and font limitations.

These states are not aliases:

| State | Shown | Uses a row slot | Can receive focus | Keeps its value |
|---|---:|---:|---:|---:|
| Visible and interactive | Yes | Yes | Yes | Yes |
| Visible and non-interactive | Yes | Yes | No | Yes |
| Invisible | No | No | No | Yes |

## Update one row or several rows

One C++ draft can update several fields of one row atomically:

```cpp
const ERUI_Result result = difficulty.edit()
    .label(L"Enemy Damage: Hard")
    .help(L"The Hard profile is active.")
    .interactive(true)
    .commit();
```

Calling `commit()` on several C++ drafts performs several independent
publications. They are not one atomic group. API 1.4 deliberately does not
expose a C++ multi-row collector.

When several rows must change as one all-or-nothing generation, the strict-C
API can submit 1..4,096 `ERUI_UpdateDesc` records in one
`ERUI_UpdateBatchDesc` through `commit_updates`. That is **one commit
containing multiple updates**, not multiple commits at once. If any item is
invalid, the host publishes none of them.

See [Update several rows atomically](batching.md) for a complete two-row C
example and all ownership, uniqueness, and lifetime rules.

For exact structure ownership and failure rules, see
[Lifecycle, errors, and limits](../../reference/lifecycle-errors-and-limits.md).
For a complete live regression example, see the
[Tarnished UI Showcase](../../../examples/tarnished_ui_showcase/README.md).
