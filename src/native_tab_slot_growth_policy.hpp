#pragma once

#include <array>
#include <cstddef>

namespace erui::detail {

inline constexpr std::size_t native_tab_stock_slot_count = 8U;
inline constexpr std::size_t native_tab_growth_capacity = 10U;

enum class NativeTabSlotGrowthStatus {
    ready,
    invalid_minimum,
    asymmetric_layers,
    missing_stock_trailing_cell,
    non_contiguous_cells,
    below_minimum,
};

struct NativeTabSlotGrowthPlan final {
    NativeTabSlotGrowthStatus status{
        NativeTabSlotGrowthStatus::invalid_minimum};
    std::size_t discovered_slot_count{native_tab_stock_slot_count};

    [[nodiscard]] constexpr bool structure_exact() const noexcept {
        return status == NativeTabSlotGrowthStatus::ready ||
            status == NativeTabSlotGrowthStatus::below_minimum;
    }

    [[nodiscard]] constexpr bool minimum_satisfied() const noexcept {
        return status == NativeTabSlotGrowthStatus::ready;
    }
};

// Authored capacity and active native records are related but distinct. A
// movie may safely provide ten cells while the merged catalogue contains only
// nine real routes; activating a duplicate tenth record would leave the
// viewport without a logical owner for that physical slot.
[[nodiscard]] constexpr std::size_t native_tab_active_slot_target(
    std::size_t discovered_capacity,
    std::size_t logical_tab_count) noexcept {
    if (discovered_capacity < native_tab_stock_slot_count ||
        discovered_capacity > native_tab_growth_capacity ||
        logical_tab_count < native_tab_stock_slot_count) {
        return 0U;
    }
    return logical_tab_count < discovered_capacity
        ? logical_tab_count
        : discovered_capacity;
}

// The Configuration movie always authors one trailing selector cell beyond
// its active native count. Cells 8, 9, and 10 therefore describe compatible
// active capacities 8, 9, and 10 respectively. Both coincident presentation
// layers must expose the same contiguous prefix before native ownership may be
// changed.
[[nodiscard]] constexpr NativeTabSlotGrowthPlan plan_native_tab_slot_growth(
    const std::array<bool, 3>& front_cells_8_through_10,
    const std::array<bool, 3>& back_cells_8_through_10,
    std::size_t configured_minimum) noexcept {
    NativeTabSlotGrowthPlan result{};
    if (configured_minimum < native_tab_stock_slot_count ||
        configured_minimum > native_tab_growth_capacity) {
        return result;
    }
    if (front_cells_8_through_10 != back_cells_8_through_10) {
        result.status = NativeTabSlotGrowthStatus::asymmetric_layers;
        return result;
    }
    if (!front_cells_8_through_10[0]) {
        result.status =
            NativeTabSlotGrowthStatus::missing_stock_trailing_cell;
        return result;
    }
    if (!front_cells_8_through_10[1] &&
        front_cells_8_through_10[2]) {
        result.status = NativeTabSlotGrowthStatus::non_contiguous_cells;
        return result;
    }

    result.discovered_slot_count = front_cells_8_through_10[2]
        ? 10U
        : (front_cells_8_through_10[1] ? 9U : 8U);
    result.status = result.discovered_slot_count >= configured_minimum
        ? NativeTabSlotGrowthStatus::ready
        : NativeTabSlotGrowthStatus::below_minimum;
    return result;
}

} // namespace erui::detail
