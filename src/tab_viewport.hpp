#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace erui::detail {

inline constexpr std::size_t invalid_tab_index =
    std::numeric_limits<std::size_t>::max();

enum class TabDirection : std::uint8_t {
    previous,
    next,
};

// Keep this explicit until Elden Ring's native boundary behavior has been
// confirmed by the runtime probe. The native adapter will choose the policy.
enum class TabBoundaryBehavior : std::uint8_t {
    stop,
    wrap,
};

// Presentation-only marker for a physical edge slot. The slot keeps mapping
// to its real logical tab; native rendering may temporarily replace only its
// icon so the player can see that more tabs exist beyond the current window.
enum class TabOverflowIndicator : std::uint8_t {
    none,
    previous,
    next,
};

struct TabViewportChange {
    bool valid{};
    bool logical_selection_changed{};
    bool physical_selection_changed{};
    bool mapping_changed{};
    std::size_t previous_logical{invalid_tab_index};
    std::size_t selected_logical{invalid_tab_index};
    std::size_t previous_physical{invalid_tab_index};
    std::size_t selected_physical{invalid_tab_index};
    std::size_t previous_window_start{};
    std::size_t window_start{};
    std::uint64_t mapping_generation{};
};

// Allocation-free logical-tab viewport. The catalogue that owns titles,
// icons, pages, and stable identities is deliberately separate; this class
// only maps a contiguous logical range onto the physical native tab slots.
class TabViewport {
public:
    [[nodiscard]] bool initialize(
        std::size_t logical_count,
        std::size_t physical_capacity,
        std::size_t requested_navigation_margin = 1,
        std::size_t initial_logical = 0) noexcept;

    void reset() noexcept;

    [[nodiscard]] TabViewportChange move(
        TabDirection direction,
        TabBoundaryBehavior boundary =
            TabBoundaryBehavior::stop) noexcept;

    [[nodiscard]] TabViewportChange select_logical(
        std::size_t logical_index) noexcept;

    // Resolve a hovered native slot against the mapping currently on screen.
    // Hover selection deliberately preserves that mapping: remapping beneath
    // a stationary cursor could immediately hover a different logical tab and
    // cascade through the catalogue. L1/R1 movement applies the margin.
    [[nodiscard]] TabViewportChange select_physical(
        std::size_t physical_slot) noexcept;

    // Resolve a native slot against the mapping currently on screen, then
    // align that logical tab to the configured navigation margin. This is
    // useful for a direct pointer jump to an edge slot: unlike
    // select_physical(), it may shift the viewport.
    [[nodiscard]] TabViewportChange select_physical_aligned(
        std::size_t physical_slot) noexcept;

    [[nodiscard]] bool logical_for_physical(
        std::size_t physical_slot,
        std::size_t& output) const noexcept;

    [[nodiscard]] bool physical_for_logical(
        std::size_t logical_index,
        std::size_t& output) const noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] std::size_t logical_count() const noexcept;
    [[nodiscard]] std::size_t physical_capacity() const noexcept;
    [[nodiscard]] std::size_t visible_slot_count() const noexcept;
    [[nodiscard]] std::size_t requested_navigation_margin() const noexcept;
    [[nodiscard]] std::size_t navigation_margin() const noexcept;
    [[nodiscard]] std::size_t window_start() const noexcept;
    [[nodiscard]] std::size_t maximum_window_start() const noexcept;
    [[nodiscard]] std::size_t selected_logical() const noexcept;
    [[nodiscard]] std::size_t selected_physical() const noexcept;
    [[nodiscard]] std::uint64_t mapping_generation() const noexcept;

private:
    [[nodiscard]] std::size_t aligned_window_start(
        std::size_t logical_index) const noexcept;
    [[nodiscard]] TabViewportChange unchanged_change(bool valid) const noexcept;
    void advance_mapping_generation() noexcept;

    std::size_t logical_count_{};
    std::size_t physical_capacity_{};
    std::size_t requested_navigation_margin_{};
    std::size_t navigation_margin_{};
    std::size_t window_start_{};
    std::size_t selected_logical_{invalid_tab_index};
    std::uint64_t mapping_generation_{};
};

[[nodiscard]] TabOverflowIndicator tab_overflow_indicator(
    const TabViewport& viewport,
    std::size_t physical_slot) noexcept;

[[nodiscard]] bool tab_overflow_indicators_changed(
    const TabViewport& before,
    const TabViewport& after) noexcept;

} // namespace erui::detail
