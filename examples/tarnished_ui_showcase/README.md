# Tarnished UI Showcase

`TarnishedUIShowcase.dll` is the shipped integration example. It demonstrates:

- runtime localization for all 15 language identifiers currently reported by
  Elden Ring's Steamworks interface, explicit `vietnamese` identifier support,
  and English fallback for every other future identifier;
- one explicit API 1.4 connection reused for localization and registration;
- host-owned toggle and slider values;
- a dedicated advanced-slider submenu containing `x0.6`, signed `-0.5`, and
  `3.0s` Numeric Sliders plus a deliberately absurd Custom Slider that moves
  through `-7`, `3`, `7.8`, `1s`, `hello`, `18`, and `x1.3`;
- a capability-gated **Runtime Slider Labels** subpage where one Button
  callback uses `NumericSliderRowEdit::value_labels(...)` and
  `CustomSliderRowEdit::values(...)` to replace two three-entry display tables
  while preserving both current selections; each typed edit also changes that
  row's focused help in the same atomic row commit;
- value and button callbacks;
- a dedicated choice submenu comparing one inline row with independent native
  popup lists containing two, four, and eight options;
- a retained `ToggleRow` and programmatic `ToggleRow::set_value` update;
- a retained intensity `SliderRow` whose label and help update from its
  callback, with the live percentage interpolated from red to green;
- an API 1.4 **Label & Help Updates** subpage covering all nine currently
  editable row types; its first row alternates every label and help message,
  plus the supported Button and Popup Choice captions, between unmistakable
  A/B variants;
- a retained API 1.4 **Rich Submenu** whose A/B choice atomically changes its
  opener label, caption, and help together with the currently open child-page
  title and help; the same surface includes a nested retained Submenu and
  enough rows to validate regenerated pagination titles;
- three retained API 1.4 custom tabs (**Rich Alpha**, **Rich Target**, and
  **Rich Omega**) plus a built-in-page **Rich Tab Updates** controller; the
  target changes title, custom/default icon, visibility, and preferred order
  from both the controller and its own active-page callbacks;
- an API 1.4 **Row Interaction Matrix** covering Button, Toggle, byte Slider,
  Numeric Slider, Custom Slider, Inline Choice, Popup Choice, TextInput, and
  ColorPicker, with an additional red-to-green styled Intensity row that
  verifies exact color restoration, plus a focused **Runtime Availability**
  subpage proving that interaction and disabled appearance remain independent;
- API 1.4 **Reactive Layout** matrices on Game Options, a custom tab, and an
  ordinary subpage; each rebuilds mixed row types and pagination immediately
  from a retained Slider callback through `.visible(bool)`, without a separate
  Refresh button;
- a capability-gated TextInput submenu with the default 16-unit limit and an
  explicit 35-UTF-16-unit field;
- a nested logical page with 32 generated action rows;
- automatic Next/Previous pagination through Elden Ring's native page stack;
- the default native localized-OK alert;
- a dedicated popup submenu covering every bottom/center placement and
  dismiss-only, OK, CANCEL, YES, NO, OK+CANCEL, and YES+NO button layout;
- asynchronous popup completion logging with result and response details;
- a custom outer title and per-slice title formatter;
- a native alert action registered through the public BuiltinPage API on Game
  Options, Camera Options, Display, Sound, Network, Keyboard/Mouse Settings,
  and Graphics;
- production input-action sections demonstrating all-device,
  keyboard/mouse-only, and controller-only defaults, with sections appearing
  only in the native binding screens their actions support;
- stable nonlocalized action IDs, process-lifetime callbacks,
  `ActionActivation` device-mask diagnostics, and explicit provider storage.

The validation subpage contains Button, Toggle, byte Slider, Numeric Slider,
Custom Slider, Inline Choice, Popup Choice, TextInput, and ColorPicker rows.
Press its first row to switch all nine between variants A and B, then focus
each row to compare its help message. A separate root Button tests the Game
Options action-widget layout; logical subpages use a different action layout.
The root Button must show `ROOT BUTTON ROW [A/B]` on the left while its boxed
plain caption changes between `ROOT CAPTION A` and `ROOT CAPTION B`. The
subpage Button combines `BUTTON ROW [A/B]` on the left with a styled
`BUTTON CAPTION [A/B]` on the right. Each of those commits changes label,
caption, and help together.

Popup Choice shows a styled `VALUE ZERO/ONE/TWO [A/B]` caption while its
native popup continues to contain the unmodified `VALUE ZERO`, `VALUE ONE`,
and `VALUE TWO` options. Its selection callback recommits the caption after
the selected index changes. This makes the contract visible: `caption(...)`
changes only the closed row's right-hand presentation; it does not change the
selected index or the popup list. ColorPicker keeps its ordinary swatch, and
the seven row edit types without a textual control caption do not expose
`caption(...)`.

For the live regression, leave each variant visible for several seconds,
move focus across every row, leave and reopen the subpage, switch to another
System tab and back, and close/reopen System. Open and close the Popup Choice,
select each of its three values, and open the TextInput and ColorPicker editors
as well. Labels, captions, and focused help must keep the active A/B variant
throughout those frame updates and reconstructions.

The **Runtime Slider Labels** regression is nested under **Advanced Slider
Showcase**. Both sliders have exactly three positions and initially select the
middle one. Press **Switch both label sets** repeatedly:

- Numeric Slider changes between `ZERO / ONE / TWO` and
  `LOW / MEDIUM / HIGH`, but its logical numeric value remains `1`;
- Custom Slider changes between `North / Center / South` and
  `Alpha / Beta / Gamma`, but its selected index remains `1`; and
- focusing each row shows help from the same variant as its current labels,
  proving that the ordinary help field and typed label table were one atomic
  row commit.

After pressing the Button, both visible selected-value labels must repaint
without moving either slider. Then switch pages and close/reopen System; the
latest label table and the preserved middle selection must remain in effect.

The Button callback makes two independent C++ commits, one per retained row.
There must be no mixed help/label generation within either row. A mod that
needs both rows to publish as one cross-row all-or-nothing generation must use
the strict-C update batch described in the row-update documentation.

The **Rich Submenu** row is the focused runtime Submenu test. Open it and
change **Main presentation variant** between A and B. The currently open page
must immediately change its title without closing, flashing, or losing focus.
Navigate through Next and Previous and verify every slice uses the selected
title with a correct `(n/t)` suffix. Back out once and verify the opener's
label, framed caption, and focused help all use the same variant. The same
atomic commit also changes the child page's logical help metadata; the current
settings movie has no independent page-help field to inspect visually, so that
part of the contract is covered by the automated host tests.

Open **Nested Submenu**, change **Nested presentation variant**, and perform
the same title/opener checks one level deeper. Repeat both changes after
switching System tabs and after closing and reopening System. Each A/B change
is one atomic `SubmenuEdit::commit()`; a mixed generation must never be
visible.

The rich-tab regression starts with three adjacent, unmistakable selectors:
red-diamond **Rich Alpha**, gold-plus **Rich Target A**, and blue-ring
**Rich Omega**. Test without a player `TabOrder` override first so the
example's preferred-order changes remain visible.

1. Open **Rich Target A** and press **Rename this active tab live**. The same
   active page must remain open and focused while its selector and page
   heading become **Rich Target Live**. No registered page or row may be
   recreated.
2. Press **Move this tab after Omega**. The active selector must move to the
   other side of Omega without changing the current title/icon or losing the
   focused row.
3. Press **Hide this active tab**. Its selector must disappear immediately and
   focus must transfer once to a valid surviving tab; there must be no flash,
   duplicate selector, stale icon, or dead input.
4. Return to Game Options, open **Rich Tab Updates**, and press **Show target
   tab**. The same stable target must reappear with its latest title, icon,
   order, and rows.
5. Press **Apply Variant B**. The target must become **Rich Target B**, use the
   green plus, and sort before Alpha live as one complete generation. There
   must be no stock-icon, default-icon, or partially updated frame.
6. Press **Use default triangle icon**, then **Apply Variant A**. Each icon must
   change live while the title, order, page contents, and focused row remain
   coherent. Now apply A or B and immediately close and reopen System: the
   latest desired generation must settle automatically in that first reopened
   movie, even when the close interrupts the live handoff. No second reopen is
   required. The single-field controller actions must preserve every omitted
   field.

Repeat the sequence rapidly, wrap through the strip in both directions,
select all three tabs with the mouse, and close/reopen System three times.
Also hide and show the target while it is not selected. If player ordering is
configured, repeat once with an explicit target override and confirm it
remains authoritative over `.preferred_order(...)` while title, icon, and
visibility still update. Every successful edit is one `TabEdit::commit()`
made directly from a client callback; no separate Refresh action is involved.

Title, preferred-order, visibility, and icon commits all support same-movie
live publication. Icon edits use a complete staged resource bank and an atomic
handoff, so ERNativeUI never mutates an image still owned by Scaleform. A close
during that handoff leaves the desired revision queued for automatic adoption
in the first reopened movie.

This matrix also documents the font boundary visible in the effective game
movie. Color and underline work with the vanilla settings presentation. The
API can request bold and italic fragments as well, but their visible result
depends on corresponding faces being present in the effective Scaleform font
configuration. A compatible configuration may make them visually distinct,
but that has not yet been validated and ERNativeUI does not patch font
resources.

The Row Interaction Matrix validates the complete production availability
scope. Turn **Disable all targets** on, then verify controller, keyboard, and
mouse navigation skip all ten target rows and that every left-hand label is
visibly dimmed. Toggle and Inline Choice also use their native disabled
right-hand presentation, while the byte, numeric, and custom sliders dim their
right-hand value captions. Turn it off and verify all targets become selectable
and return to their original presentation. In particular, the Intensity
percentage must recover its exact red-to-green interpolated color after every
cycle. Popup Choice deliberately keeps its row label and selected-value caption
separate. Submenu is not part of this milestone and is intentionally absent
from the matrix.

The Runtime Availability page has three focused lifecycle checks. Turn
**Target interaction** off and verify controller and mouse navigation skip
**Availability target**; switch **Target appearance** among Follow, Normal,
and Disabled to verify appearance does not silently change interaction. Then
enable **Construction-disabled target** and verify it becomes selectable with
the correctly localized value text. Finally, change **Self-disable on change**:
the callback must complete, focus must move to a surviving row, and the target
must remain skipped until **Re-enable self-disabling row** is pressed. Repeat
after leaving/reopening the page and after rapidly closing/reopening System.

The Reactive Layout matrix is repeated on three native surfaces: the normal
Game Options tab, the **Reactive Layout** custom tab, and the **Reactive Layout
subpage** opened from Game Options. Each starts with **Layout configuration =
0** and contains Button, Toggle, byte Slider, Numeric Slider, Custom Slider,
Inline Choice, Popup Choice, TextInput, ColorPicker, and eight pagination
targets. There is no Refresh row: changing the Slider commits every requested
`.visible(...)` edit directly from its callback.

- `0` (**Full**) shows every target and spans two physical slices on the
  isolated custom-tab and subpage matrices.
- `1` (**Compact**) keeps a mixed subset and three pagination targets; it fits
  one physical page, so Next disappears immediately.
- `2` (**Minimal**) leaves only Layout configuration and the always-visible
  sentinel.

On each surface, test `0 -> 1 -> 2 -> 1 -> 0`, including once while focused on
a row that the next layout hides. The page must not close or flash, focus must
move to a surviving row, Next/Previous must track the current row count, and
every value/editor state must survive reconstruction. Repeat after switching
tabs, backing out of/reopening the subpage, and closing/reopening System. The
sentinel must never disappear or duplicate.

On the Reactive Layout subpage, also test the focused title transition
`0 -> 1 -> 0`: its heading must change from the two-slice `Reactive Layout
subpage (1/2)` variant to the one-slice `Reactive Layout subpage` title and
back to `(1/2)` in place, without a page close/open transition or stale title.

The example builds presentation from text and an allowlisted style rather than
embedding markup:

```cpp
auto caption = erui::StyledText{}
    .append(L"Status: ")
    .append(
        L"ready",
        erui::TextStyle{}
            .color(erui::Color{96, 220, 128})
            .underline());

button.edit().caption(std::move(caption)).commit();
```

Existing `label(L"...")`, `caption(L"...")`, and `help(L"...")` calls are
literal-text APIs; the host escapes their contents before sending them to
Scaleform. Caption editing is available on `ButtonRowEdit`,
`PopupChoiceRowEdit`, and the opener portion of `SubmenuEdit` in this
milestone.

For a focused mod-author explanation of the same surface, including
capability checks, commit semantics, interaction versus appearance, visibility
and pagination behavior, read the
[API 1.4 row-updates guides](../../docs/guides/row-updates/README.md).

The showcase declares semantic defaults, then loads optional overrides from
`mods/tarnished-showcase/config.ini` beside
`ERNativeUI.dll`. Its provider-wide callback explicitly applies player
assignment events in memory and saves the document. This persistence is
showcase policy, not automatic host behavior. The callbacks are global
released-to-pressed actions and do not consume the underlying game input, so
use neutral test inputs when exercising them.

Build it with the full project or independently against an installed SDK:

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\sdk\ERNativeUI
cmake --build build --config Release
```

When copying the full staged tree to `mod\ERNativeUI`, load it after the host:

```toml
external_dlls = [
    "mod\\ERNativeUI\\ERNativeUI.dll",
    "mod\\ERNativeUI\\examples\\TarnishedUIShowcase.dll",
]
```

The showcase is intentionally a demonstration rather than a reusable mod.
Use [../template](../template) as the starting point for your own project.
