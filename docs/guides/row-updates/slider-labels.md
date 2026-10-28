# Replace slider display labels at runtime

<table>
  <tr>
    <td align="center" valign="middle">
      <a href="../capabilities.md#what-a-capability-is"><img src="../../assets/images/badges/capabilities.svg" alt="Capabilities" width="112" height="22" align="top"></a>
    </td>
    <td valign="middle">
      <a href="../capabilities.md#updates"><code>updates</code></a> ·
      <a href="../capabilities.md#row-configuration"><code>row_configuration</code></a> ·
      <a href="../capabilities.md#numeric-slider"><code>numeric_slider</code></a> ·
      <a href="../capabilities.md#custom-slider"><code>custom_slider</code></a>
    </td>
  </tr>
</table>

API 1.4 can replace the complete display-label table of a retained Numeric
Slider or Custom Slider. This first runtime-configuration slice is deliberately
presentation-only:

- the replacement must contain exactly as many labels as the row already has;
- the current numeric value or custom index is preserved;
- the range, step, precision, prefix, suffix, callback, and row identity do
  not change; and
- changing labels does not invoke the slider's player-change callback.

Use this when the same logical positions need different text at runtime. For
example, a Numeric Slider with logical values `0`, `1`, and `2` can display
`LOW`, `MEDIUM`, and `HIGH` without changing the value reported to the mod.

## Complete C++ example

The following subpage contains two three-position sliders and a Button. The
Button callback replaces both display tables while keeping both middle
selections:

```cpp
#include <ernativeui/ERNativeUI.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

erui::NumericSliderRow g_numeric;
erui::CustomSliderRow g_custom;
std::atomic_bool g_variant_b{false};

constexpr std::array<std::wstring_view, 3> kNumericA{
    L"ZERO", L"ONE", L"TWO"};
constexpr std::array<std::wstring_view, 3> kNumericB{
    L"LOW", L"MEDIUM", L"HIGH"};
constexpr std::array<std::wstring_view, 3> kCustomA{
    L"North", L"Center", L"South"};
constexpr std::array<std::wstring_view, 3> kCustomB{
    L"Alpha", L"Beta", L"Gamma"};

void switch_labels() noexcept
{
    const bool next = !g_variant_b.load(std::memory_order_acquire);
    const std::array<std::wstring_view, 3>& numeric =
        next ? kNumericB : kNumericA;
    const std::array<std::wstring_view, 3>& custom =
        next ? kCustomB : kCustomA;

    // One row commit atomically publishes its help and all three labels.
    const ERUI_Result numeric_result = g_numeric.edit()
        .help(next
            ? L"LOW / MEDIUM / HIGH; logical value is unchanged."
            : L"ZERO / ONE / TWO; logical value is unchanged.")
        .value_labels(numeric.data(), numeric.size())
        .commit();

    const ERUI_Result custom_result = g_custom.edit()
        .help(next
            ? L"Alpha / Beta / Gamma; selected index is unchanged."
            : L"North / Center / South; selected index is unchanged.")
        .values(custom.data(), custom.size())
        .commit();

    if (numeric_result == ERUI_OK && custom_result == ERUI_OK) {
        g_variant_b.store(next, std::memory_order_release);
    } else {
        // Log both result codes according to the mod's error policy.
    }
}

void add_slider_label_example(erui::Menu& menu) noexcept
{
    if (!menu.supports(erui::Capability::button) ||
        !menu.supports(erui::Capability::numeric_slider) ||
        !menu.supports(erui::Capability::custom_slider) ||
        !menu.supports(erui::Capability::updates) ||
        !menu.supports(erui::Capability::row_configuration)) {
        return;
    }

    erui::Submenu page = menu.root().add_submenu(
        L"Runtime Slider Labels",
        L"Replace display labels without changing either selection.");

    erui::NumericSliderOptions numeric_options{};
    numeric_options.minimum = 0.0;
    numeric_options.maximum = 2.0;
    numeric_options.step = 1.0;
    numeric_options.initial_value = 1.0;
    numeric_options.decimal_places = 0;
    g_numeric = page.add_slider(
        L"Numeric display labels",
        L"Initially displays 0 / 1 / 2.",
        numeric_options);

    erui::CustomSliderOptions custom_options{};
    custom_options.value_count =
        static_cast<std::uint32_t>(kCustomA.size());
    custom_options.initial_index = 1;
    custom_options.format_value =
        [](std::uint32_t index, std::uint32_t) -> std::wstring {
            return std::wstring(kCustomA[index]);
        };
    g_custom = page.add_custom_slider(
        L"Custom display labels",
        L"Initially displays North / Center / South.",
        custom_options);

    page.add_button<&switch_labels>(
        L"Switch both label sets",
        L"Runs both typed edits from this callback.");
}

} // namespace
```

Call `add_slider_label_example(menu)` inside the normal `register_menu`
builder. The retained rows become editable after registration commits; the
Button cannot invoke its callback before that point.

## The two typed edit methods

| Row | Method | Meaning |
|---|---|---|
| `erui::NumericSliderRow` | `.edit().value_labels(values, count)` | Overrides the text shown for every existing numeric position while preserving the logical `double` value. |
| `erui::CustomSliderRow` | `.edit().values(values, count)` | Replaces the text associated with every existing custom index while preserving the selected index. |

Both methods also accept an initializer list. The pointer/count form is often
clearer when the labels already live in a container:

```cpp
const ERUI_Result result = g_numeric.edit()
    .value_labels({L"Off", L"Normal", L"Strong"})
    .commit();
```

Every string is copied into the C++ draft before the setter returns, and the
host copies the complete draft synchronously during `commit()`. The client
container and temporary strings do not need to outlive the call.

`commit()` publishes the complete replacement table at the next safe UI
update boundary. If the row is currently materialized, ERNativeUI asks its
already-authorized SliderCtrl to repaint once on the next safe UI frame where
that live controller can be validated; the player does not need to move the
slider. The host revalidates the controller identity and row's exact backing
value for that call and does not retain an additional live controller pointer.
Reopening or rematerializing the page also uses the latest committed table.

## Validation rules

For either row family:

- the row must be a retained row of the matching concrete type;
- `updates`, `row_configuration`, `numeric_slider`, and `custom_slider` must
  be available (the single configuration capability covers both admitted
  slider families);
- the replacement count must equal the row's registered position count;
- every label must contain 1 through 64 valid UTF-16 code units; and
- invalid input rejects the entire row update, leaving its previous help,
  labels, value, and selection published.

Numeric Slider labels are a presentation override. The player's movement and
`NumericSliderChange` still use the original exact numeric sequence. Custom
Slider callbacks still report the original zero-based index.

API 1.4 does not expose a separate “clear override” operation in this slice.
To restore earlier presentation, submit the original complete label table
again. For Numeric Slider, that means supplying the strings that its original
numeric formatting produced.

## One row is atomic; two C++ commits are not

The example deliberately combines `.help(...)` with `.value_labels(...)` or
`.values(...)`. Within each `commit()`, help and all labels publish as one row
generation: the player cannot observe new help with old labels for that row.

The Numeric Slider commit and Custom Slider commit are still two independent
publications. If several rows must change as one cross-row all-or-nothing
generation, use the strict-C
[`ERUI_UpdateBatchDesc`](batching.md) path and place both typed control payloads
in the same `commit_updates` call.

## What is still staged, not implemented

This slice does **not** change slider ranges, steps, decimal precision,
prefixes, suffixes, current values, or list cardinality. Runtime choice-option
replacement and TextInput constraint changes are also not public features.
Those broader control-configuration ideas remain staged for separate native
lifecycle proof and API review.

[Row-updates index](README.md) | [Read or change values](values.md) |
[Update several rows atomically](batching.md)
