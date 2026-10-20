#include "tab_viewport.hpp"
#include "tab_initial_presentation_refresh.hpp"

#include "test_assertions.hpp"

#include <ernativeui/erui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using erui::detail::TabViewport;
using erui::detail::TabViewportChange;
using erui::detail::TabOverflowIndicator;
using erui::detail::invalid_tab_index;
using erui::detail::tab_overflow_indicator;
using erui::detail::tab_overflow_indicators_changed;

struct ViewportSnapshot {
    std::size_t logical{};
    std::size_t physical{};
    std::size_t window_start{};
    std::uint64_t mapping_generation{};
};

[[nodiscard]] ViewportSnapshot snapshot(const TabViewport& viewport) {
    return {
        viewport.selected_logical(),
        viewport.selected_physical(),
        viewport.window_start(),
        viewport.mapping_generation(),
    };
}

void assert_invariants(const TabViewport& viewport) {
    if (!viewport.initialized()) {
        ERUI_TEST_CHECK(viewport.logical_count() == 0);
        ERUI_TEST_CHECK(viewport.physical_capacity() == 0);
        ERUI_TEST_CHECK(viewport.visible_slot_count() == 0);
        ERUI_TEST_CHECK(viewport.selected_logical() == invalid_tab_index);
        ERUI_TEST_CHECK(viewport.selected_physical() == invalid_tab_index);
        return;
    }

    ERUI_TEST_CHECK(viewport.logical_count() != 0);
    ERUI_TEST_CHECK(viewport.physical_capacity() != 0);
    const std::size_t visible = std::min(
        viewport.logical_count(), viewport.physical_capacity());
    ERUI_TEST_CHECK(viewport.visible_slot_count() == visible);
    ERUI_TEST_CHECK(viewport.navigation_margin() <= (visible - 1) / 2);
    ERUI_TEST_CHECK(
        viewport.navigation_margin() <=
        viewport.requested_navigation_margin());
    ERUI_TEST_CHECK(
        viewport.window_start() <= viewport.maximum_window_start());
    ERUI_TEST_CHECK(
        viewport.selected_logical() < viewport.logical_count());
    ERUI_TEST_CHECK(viewport.selected_physical() < visible);
    ERUI_TEST_CHECK(viewport.mapping_generation() != 0);

    for (std::size_t physical = 0; physical < visible; ++physical) {
        std::size_t logical = invalid_tab_index;
        ERUI_TEST_CHECK(viewport.logical_for_physical(physical, logical));
        ERUI_TEST_CHECK(logical == viewport.window_start() + physical);
        ERUI_TEST_CHECK(logical < viewport.logical_count());

        std::size_t round_trip = invalid_tab_index;
        ERUI_TEST_CHECK(viewport.physical_for_logical(logical, round_trip));
        ERUI_TEST_CHECK(round_trip == physical);
    }

    std::size_t output = invalid_tab_index;
    ERUI_TEST_CHECK(!viewport.logical_for_physical(visible, output));
    ERUI_TEST_CHECK(
        !viewport.physical_for_logical(viewport.logical_count(), output));
    if (viewport.window_start() != 0) {
        ERUI_TEST_CHECK(!viewport.physical_for_logical(
            viewport.window_start() - 1, output));
    }
    const std::size_t visible_end = viewport.window_start() + visible;
    if (visible_end < viewport.logical_count()) {
        ERUI_TEST_CHECK(
            !viewport.physical_for_logical(visible_end, output));
    }
}

void assert_change(
    const TabViewportChange& change,
    const ViewportSnapshot& before,
    const TabViewport& after,
    bool expected_valid = true) {
    ERUI_TEST_CHECK(change.valid == expected_valid);
    ERUI_TEST_CHECK(change.previous_logical == before.logical);
    ERUI_TEST_CHECK(change.previous_physical == before.physical);
    ERUI_TEST_CHECK(change.previous_window_start == before.window_start);
    ERUI_TEST_CHECK(change.selected_logical == after.selected_logical());
    ERUI_TEST_CHECK(change.selected_physical == after.selected_physical());
    ERUI_TEST_CHECK(change.window_start == after.window_start());
    ERUI_TEST_CHECK(
        change.mapping_generation == after.mapping_generation());
    ERUI_TEST_CHECK(
        change.logical_selection_changed ==
        (before.logical != after.selected_logical()));
    ERUI_TEST_CHECK(
        change.physical_selection_changed ==
        (before.physical != after.selected_physical()));
    ERUI_TEST_CHECK(
        change.mapping_changed ==
        (before.window_start != after.window_start()));
    if (change.mapping_changed) {
        ERUI_TEST_CHECK(
            before.mapping_generation != after.mapping_generation());
    } else {
        ERUI_TEST_CHECK(
            before.mapping_generation == after.mapping_generation());
    }
    assert_invariants(after);
}

void test_invalid_and_reset_states() {
    using namespace erui::detail;

    TabViewport viewport;
    assert_invariants(viewport);

    std::size_t output = 123;
    ERUI_TEST_CHECK(!viewport.logical_for_physical(0, output));
    ERUI_TEST_CHECK(!viewport.physical_for_logical(0, output));

    ViewportSnapshot before = snapshot(viewport);
    assert_change(
        viewport.move(TabDirection::next), before, viewport, false);
    before = snapshot(viewport);
    assert_change(viewport.select_logical(0), before, viewport, false);
    before = snapshot(viewport);
    assert_change(viewport.select_physical(0), before, viewport, false);

    ERUI_TEST_CHECK(!viewport.initialize(0, 9));
    assert_invariants(viewport);
    ERUI_TEST_CHECK(!viewport.initialize(8, 0));
    assert_invariants(viewport);
    ERUI_TEST_CHECK(!viewport.initialize(8, 9, 1, 8));
    assert_invariants(viewport);

    ERUI_TEST_CHECK(viewport.initialize(12, 9));
    assert_invariants(viewport);
    const std::uint64_t initialized_generation =
        viewport.mapping_generation();

    ERUI_TEST_CHECK(viewport.initialize(12, 9));
    ERUI_TEST_CHECK(
        viewport.mapping_generation() != initialized_generation);
    const std::uint64_t reinitialized_generation =
        viewport.mapping_generation();

    ERUI_TEST_CHECK(viewport.initialize(5, 2, 0, 4));
    ERUI_TEST_CHECK(
        viewport.mapping_generation() != reinitialized_generation);
    const std::uint64_t reshaped_generation =
        viewport.mapping_generation();

    ERUI_TEST_CHECK(!viewport.initialize(0, 2));
    assert_invariants(viewport);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() != reshaped_generation);
    const std::uint64_t invalidated_generation =
        viewport.mapping_generation();

    ERUI_TEST_CHECK(viewport.initialize(12, 9));
    ERUI_TEST_CHECK(
        viewport.mapping_generation() != invalidated_generation);
    const std::uint64_t restored_generation =
        viewport.mapping_generation();
    viewport.reset();
    assert_invariants(viewport);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() != restored_generation);
    const std::uint64_t reset_generation = viewport.mapping_generation();
    viewport.reset();
    ERUI_TEST_CHECK(viewport.mapping_generation() == reset_generation);
}

void test_tabs_that_fit() {
    using namespace erui::detail;

    for (const std::size_t logical_count : {
             std::size_t{1}, std::size_t{8}, std::size_t{9}}) {
        TabViewport viewport;
        ERUI_TEST_CHECK(viewport.initialize(logical_count, 9, 1));
        const std::uint64_t generation = viewport.mapping_generation();
        assert_invariants(viewport);

        for (std::size_t logical = 1; logical < logical_count; ++logical) {
            const ViewportSnapshot before = snapshot(viewport);
            const TabViewportChange change =
                viewport.move(TabDirection::next);
            assert_change(change, before, viewport);
            ERUI_TEST_CHECK(viewport.selected_logical() == logical);
            ERUI_TEST_CHECK(viewport.selected_physical() == logical);
            ERUI_TEST_CHECK(!change.mapping_changed);
        }
        ERUI_TEST_CHECK(viewport.mapping_generation() == generation);

        ViewportSnapshot before = snapshot(viewport);
        TabViewportChange change = viewport.move(TabDirection::next);
        assert_change(change, before, viewport);
        ERUI_TEST_CHECK(!change.logical_selection_changed);

        before = snapshot(viewport);
        change = viewport.move(
            TabDirection::next, TabBoundaryBehavior::wrap);
        assert_change(change, before, viewport);
        ERUI_TEST_CHECK(viewport.selected_logical() == 0);
        ERUI_TEST_CHECK(viewport.selected_physical() == 0);
        ERUI_TEST_CHECK(!change.mapping_changed);

        if (logical_count == 1) {
            before = snapshot(viewport);
            change = viewport.move(TabDirection::previous);
            assert_change(change, before, viewport);
            ERUI_TEST_CHECK(!change.logical_selection_changed);

            before = snapshot(viewport);
            change = viewport.move(
                TabDirection::previous, TabBoundaryBehavior::wrap);
            assert_change(change, before, viewport);
            ERUI_TEST_CHECK(!change.logical_selection_changed);
        }
    }
}

void test_canonical_overflow_sequence() {
    using namespace erui::detail;

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(12, 9, 1));

    TabViewportChange change = viewport.select_logical(7);
    ERUI_TEST_CHECK(change.valid);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(!change.mapping_changed);

    ViewportSnapshot before = snapshot(viewport);
    change = viewport.move(TabDirection::next);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 8);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(change.logical_selection_changed);
    ERUI_TEST_CHECK(!change.physical_selection_changed);
    ERUI_TEST_CHECK(change.mapping_changed);

    for (const std::size_t logical : {std::size_t{9}, std::size_t{10}}) {
        before = snapshot(viewport);
        change = viewport.move(TabDirection::next);
        assert_change(change, before, viewport);
        ERUI_TEST_CHECK(viewport.selected_logical() == logical);
        ERUI_TEST_CHECK(viewport.window_start() == logical - 7);
        ERUI_TEST_CHECK(viewport.selected_physical() == 7);
        ERUI_TEST_CHECK(change.mapping_changed);
        ERUI_TEST_CHECK(!change.physical_selection_changed);
    }

    before = snapshot(viewport);
    change = viewport.move(TabDirection::next);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 11);
    ERUI_TEST_CHECK(viewport.window_start() == 3);
    ERUI_TEST_CHECK(viewport.selected_physical() == 8);
    ERUI_TEST_CHECK(!change.mapping_changed);
    ERUI_TEST_CHECK(change.physical_selection_changed);

    // Stop at the real right boundary.
    before = snapshot(viewport);
    change = viewport.move(TabDirection::next);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(!change.logical_selection_changed);

    // Reverse without shifting until the selection moves left of slot 1.
    while (viewport.selected_logical() > 4) {
        before = snapshot(viewport);
        change = viewport.move(TabDirection::previous);
        assert_change(change, before, viewport);
        ERUI_TEST_CHECK(viewport.window_start() == 3);
        ERUI_TEST_CHECK(!change.mapping_changed);
    }
    ERUI_TEST_CHECK(viewport.selected_physical() == 1);

    before = snapshot(viewport);
    change = viewport.move(TabDirection::previous);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 3);
    ERUI_TEST_CHECK(viewport.window_start() == 2);
    ERUI_TEST_CHECK(viewport.selected_physical() == 1);
    ERUI_TEST_CHECK(change.mapping_changed);
    ERUI_TEST_CHECK(!change.physical_selection_changed);
}

void test_hover_selection_preserves_mapping() {
    using namespace erui::detail;

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(12, 9, 1));
    const std::uint64_t initial_generation =
        viewport.mapping_generation();

    // Native tabs select on hover. The hovered slot must remain mapped to the
    // same logical tab while the cursor is stationary.
    ViewportSnapshot before = snapshot(viewport);
    TabViewportChange change = viewport.select_physical(8);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 8);
    ERUI_TEST_CHECK(viewport.selected_physical() == 8);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(!change.mapping_changed);
    ERUI_TEST_CHECK(viewport.mapping_generation() == initial_generation);

    // Repeated hover notifications are stable and cannot advance through
    // hidden tabs without cursor or controller movement.
    before = snapshot(viewport);
    change = viewport.select_physical(8);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(!change.logical_selection_changed);
    ERUI_TEST_CHECK(!change.mapping_changed);

    // Controller movement from the hovered edge may then move to a hidden
    // logical tab and deliberately shift the strip to the configured anchor.
    before = snapshot(viewport);
    change = viewport.move(TabDirection::next);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 9);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(viewport.window_start() == 2);
    ERUI_TEST_CHECK(change.mapping_changed);

    before = snapshot(viewport);
    change = viewport.select_physical(0);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 2);
    ERUI_TEST_CHECK(viewport.selected_physical() == 0);
    ERUI_TEST_CHECK(viewport.window_start() == 2);
    ERUI_TEST_CHECK(!change.mapping_changed);

    before = snapshot(viewport);
    const TabViewportChange invalid = viewport.select_physical(9);
    assert_change(invalid, before, viewport, false);
}

void test_direct_mouse_edge_jumps_align_consistently() {
    using namespace erui::detail;

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(9, 8, 1));

    // An interior-to-last direct hover resolves the tab currently displayed
    // in slot 7, then shifts it to the right navigation anchor.
    ERUI_TEST_CHECK(viewport.select_logical(4).valid);
    ViewportSnapshot before = snapshot(viewport);
    TabViewportChange change = viewport.select_physical_aligned(7);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 7);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    ERUI_TEST_CHECK(viewport.selected_physical() == 6);
    ERUI_TEST_CHECK(change.mapping_changed);

    // The stationary pointer now sees the newly revealed last tab. With one
    // overflowing tab this converges without another mapping change.
    before = snapshot(viewport);
    change = viewport.select_physical_aligned(7);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 8);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(!change.mapping_changed);

    // Exact endpoint transitions remain controller-style cyclic movement.
    before = snapshot(viewport);
    change = viewport.move(
        TabDirection::next, TabBoundaryBehavior::wrap);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(viewport.selected_physical() == 0);
    ERUI_TEST_CHECK(change.mapping_changed);

    before = snapshot(viewport);
    change = viewport.move(
        TabDirection::previous, TabBoundaryBehavior::wrap);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 8);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(change.mapping_changed);

    // A shifted interior-to-first direct hover is the symmetric case: the
    // displayed logical tab 1 moves to the left navigation anchor.
    ERUI_TEST_CHECK(viewport.select_physical(3).valid);
    ERUI_TEST_CHECK(viewport.selected_logical() == 4);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    before = snapshot(viewport);
    change = viewport.select_physical_aligned(0);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 1);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(viewport.selected_physical() == 1);
    ERUI_TEST_CHECK(change.mapping_changed);

    before = snapshot(viewport);
    change = viewport.select_physical_aligned(0);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(viewport.selected_physical() == 0);
    ERUI_TEST_CHECK(!change.mapping_changed);
}

void test_overflow_indicator_policy() {
    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(12, 8, 1));

    // At the beginning only the right edge advertises hidden tabs.
    for (std::size_t physical = 0; physical < 7; ++physical) {
        ERUI_TEST_CHECK(
            tab_overflow_indicator(viewport, physical) ==
            TabOverflowIndicator::none);
    }
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::next);

    // In the middle both edge icons are masked, while the real logical tabs
    // remain mapped underneath them.
    ERUI_TEST_CHECK(viewport.select_logical(7).valid);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 0) ==
        TabOverflowIndicator::previous);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::next);

    // At the real end only the left edge advertises hidden tabs.
    ERUI_TEST_CHECK(viewport.select_logical(11).valid);
    ERUI_TEST_CHECK(viewport.window_start() == 4);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 0) ==
        TabOverflowIndicator::previous);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::none);

    // A selected real tab is never visually replaced by an indicator.
    ERUI_TEST_CHECK(viewport.initialize(12, 8, 1));
    ERUI_TEST_CHECK(viewport.select_physical(7).valid);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::none);

    // Leaving a selected edge can change only presentation: the viewport
    // mapping stays at window zero, but the hidden-tabs marker must reappear.
    const TabViewport selected_edge = viewport;
    const std::size_t edge_window = viewport.window_start();
    const std::uint64_t edge_generation = viewport.mapping_generation();
    ERUI_TEST_CHECK(viewport.select_physical(6).valid);
    ERUI_TEST_CHECK(viewport.window_start() == edge_window);
    ERUI_TEST_CHECK(viewport.mapping_generation() == edge_generation);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::next);
    ERUI_TEST_CHECK(
        tab_overflow_indicators_changed(selected_edge, viewport));

    const TabViewport right_indicator_visible = viewport;
    ERUI_TEST_CHECK(viewport.select_physical(5).valid);
    ERUI_TEST_CHECK(!tab_overflow_indicators_changed(
        right_indicator_visible, viewport));

    ERUI_TEST_CHECK(viewport.initialize(12, 8, 1, 7));
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    ERUI_TEST_CHECK(viewport.select_physical(0).valid);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 0) ==
        TabOverflowIndicator::none);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::next);

    // No overflow means no indicator, and a one-slot viewport cannot replace
    // its only (therefore selected) real icon.
    ERUI_TEST_CHECK(viewport.initialize(8, 8, 1));
    for (std::size_t physical = 0; physical < 8; ++physical) {
        ERUI_TEST_CHECK(
            tab_overflow_indicator(viewport, physical) ==
            TabOverflowIndicator::none);
    }

    ERUI_TEST_CHECK(viewport.initialize(4, 1, 99));
    for (std::size_t logical = 0; logical < 4; ++logical) {
        ERUI_TEST_CHECK(
            tab_overflow_indicator(viewport, 0) ==
            TabOverflowIndicator::none);
        if (logical + 1 < 4) {
            ERUI_TEST_CHECK(
                viewport.move(erui::detail::TabDirection::next).valid);
        }
    }

    viewport.reset();
    const TabViewport reset_viewport = viewport;
    ERUI_TEST_CHECK(!tab_overflow_indicators_changed(
        reset_viewport, viewport));
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 0) ==
        TabOverflowIndicator::none);
    ERUI_TEST_CHECK(viewport.initialize(12, 8, 1));
    ERUI_TEST_CHECK(tab_overflow_indicators_changed(
        reset_viewport, viewport));
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 8) ==
        TabOverflowIndicator::none);
}

void test_margins_and_single_slot() {
    using namespace erui::detail;

    {
        TabViewport viewport;
        ERUI_TEST_CHECK(viewport.initialize(12, 9, 0));
        ERUI_TEST_CHECK(viewport.navigation_margin() == 0);
        ERUI_TEST_CHECK(viewport.select_logical(8).valid);
        ERUI_TEST_CHECK(viewport.window_start() == 0);
        ERUI_TEST_CHECK(viewport.selected_physical() == 8);
        ERUI_TEST_CHECK(viewport.move(TabDirection::next).mapping_changed);
        ERUI_TEST_CHECK(viewport.window_start() == 1);
        ERUI_TEST_CHECK(viewport.selected_physical() == 8);
    }

    {
        TabViewport viewport;
        ERUI_TEST_CHECK(viewport.initialize(12, 9, 2));
        ERUI_TEST_CHECK(viewport.navigation_margin() == 2);
        ERUI_TEST_CHECK(viewport.select_logical(6).valid);
        ERUI_TEST_CHECK(viewport.window_start() == 0);
        ERUI_TEST_CHECK(viewport.selected_physical() == 6);
        ERUI_TEST_CHECK(viewport.move(TabDirection::next).mapping_changed);
        ERUI_TEST_CHECK(viewport.window_start() == 1);
        ERUI_TEST_CHECK(viewport.selected_physical() == 6);
    }

    {
        TabViewport viewport;
        ERUI_TEST_CHECK(viewport.initialize(12, 9, 100));
        ERUI_TEST_CHECK(viewport.requested_navigation_margin() == 100);
        ERUI_TEST_CHECK(viewport.navigation_margin() == 4);
        ERUI_TEST_CHECK(viewport.select_logical(4).valid);
        ERUI_TEST_CHECK(viewport.window_start() == 0);
        ERUI_TEST_CHECK(viewport.selected_physical() == 4);
        ERUI_TEST_CHECK(viewport.move(TabDirection::next).mapping_changed);
        ERUI_TEST_CHECK(viewport.window_start() == 1);
        ERUI_TEST_CHECK(viewport.selected_physical() == 4);
    }

    {
        TabViewport viewport;
        ERUI_TEST_CHECK(viewport.initialize(4, 1, 99));
        ERUI_TEST_CHECK(viewport.navigation_margin() == 0);
        for (std::size_t logical = 1; logical < 4; ++logical) {
            const ViewportSnapshot before = snapshot(viewport);
            const TabViewportChange change =
                viewport.move(TabDirection::next);
            assert_change(change, before, viewport);
            ERUI_TEST_CHECK(viewport.selected_logical() == logical);
            ERUI_TEST_CHECK(viewport.selected_physical() == 0);
            ERUI_TEST_CHECK(change.mapping_changed);
            ERUI_TEST_CHECK(!change.physical_selection_changed);
        }
    }
}

void test_wrap_and_direct_selection() {
    using namespace erui::detail;

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(12, 9, 1));

    ERUI_TEST_CHECK(viewport.select_logical(11).valid);
    ERUI_TEST_CHECK(viewport.window_start() == 3);
    ERUI_TEST_CHECK(viewport.selected_physical() == 8);

    ViewportSnapshot before = snapshot(viewport);
    TabViewportChange change = viewport.move(
        TabDirection::next, TabBoundaryBehavior::wrap);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(change.mapping_changed);

    before = snapshot(viewport);
    change = viewport.move(
        TabDirection::previous, TabBoundaryBehavior::wrap);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 11);
    ERUI_TEST_CHECK(viewport.window_start() == 3);
    ERUI_TEST_CHECK(change.mapping_changed);

    before = snapshot(viewport);
    change = viewport.select_logical(12);
    assert_change(change, before, viewport, false);

    before = snapshot(viewport);
    change = viewport.move(static_cast<TabDirection>(255));
    assert_change(change, before, viewport, false);
}

void test_final_stress_configuration_traversal() {
    using namespace erui::detail;

    constexpr std::size_t logical_count = 58;
    constexpr std::size_t physical_capacity = 8;
    constexpr std::size_t navigation_margin = 1;
    constexpr std::size_t maximum_window_start =
        logical_count - physical_capacity;

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(
        logical_count, physical_capacity, navigation_margin));
    ERUI_TEST_CHECK(viewport.logical_count() == logical_count);
    ERUI_TEST_CHECK(viewport.physical_capacity() == physical_capacity);
    ERUI_TEST_CHECK(viewport.navigation_margin() == navigation_margin);
    ERUI_TEST_CHECK(
        viewport.maximum_window_start() == maximum_window_start);

    const auto assert_position = [&](std::size_t expected_logical,
                                     std::size_t expected_window,
                                     std::size_t expected_physical) {
        ERUI_TEST_CHECK(viewport.selected_logical() == expected_logical);
        ERUI_TEST_CHECK(viewport.window_start() == expected_window);
        ERUI_TEST_CHECK(viewport.selected_physical() == expected_physical);
        assert_invariants(viewport);

        // Verify the complete overflow-marker presentation, not only the
        // selected slot. Selected edge tabs deliberately suppress the marker
        // that would otherwise replace their real icon.
        for (std::size_t physical = 0; physical < physical_capacity;
             ++physical) {
            TabOverflowIndicator expected = TabOverflowIndicator::none;
            if (physical != expected_physical) {
                if (physical == 0 && expected_window != 0) {
                    expected = TabOverflowIndicator::previous;
                } else if (
                    physical == physical_capacity - 1 &&
                    logical_count - expected_window > physical_capacity) {
                    expected = TabOverflowIndicator::next;
                }
            }
            ERUI_TEST_CHECK(
                tab_overflow_indicator(viewport, physical) == expected);
        }
    };

    const auto expect_move = [&](TabDirection direction,
                                 TabBoundaryBehavior boundary,
                                 std::size_t expected_logical,
                                 std::size_t expected_window,
                                 std::size_t expected_physical) {
        const ViewportSnapshot before = snapshot(viewport);
        const std::uint64_t generation_before =
            viewport.mapping_generation();
        const TabViewportChange change = viewport.move(direction, boundary);
        assert_change(change, before, viewport);
        assert_position(
            expected_logical, expected_window, expected_physical);

        const bool expected_mapping_change =
            before.window_start != expected_window;
        ERUI_TEST_CHECK(change.mapping_changed == expected_mapping_change);
        ERUI_TEST_CHECK(
            viewport.mapping_generation() ==
            generation_before + (expected_mapping_change ? 1u : 0u));
    };

    assert_position(0, 0, 0);
    const std::uint64_t initial_generation =
        viewport.mapping_generation();

    // Traverse every logical tab in the 8-native + 50-custom stress
    // catalogue. With margin one, forward motion remains at physical slot 6
    // while the window advances, except at the two real endpoints.
    std::size_t expected_forward_mapping_changes = 0;
    for (std::size_t logical = 1; logical < logical_count; ++logical) {
        const std::size_t expected_window = logical <= 6
            ? 0
            : std::min(logical - 6, maximum_window_start);
        const std::size_t expected_physical = logical - expected_window;
        const std::size_t previous_window = viewport.window_start();
        expect_move(
            TabDirection::next,
            TabBoundaryBehavior::wrap,
            logical,
            expected_window,
            expected_physical);
        if (previous_window != expected_window) {
            ++expected_forward_mapping_changes;
        }
    }
    ERUI_TEST_CHECK(viewport.selected_logical() == 57);
    ERUI_TEST_CHECK(viewport.window_start() == maximum_window_start);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() ==
        initial_generation + expected_forward_mapping_changes);

    // Forward wrap restores the first window and the first logical tab.
    expect_move(
        TabDirection::next,
        TabBoundaryBehavior::wrap,
        0,
        0,
        0);

    // Reverse wrap jumps to the final logical tab, then a complete reverse
    // traversal exercises every intermediate window back to logical zero.
    expect_move(
        TabDirection::previous,
        TabBoundaryBehavior::wrap,
        57,
        maximum_window_start,
        7);
    for (std::size_t logical = 56;; --logical) {
        const std::size_t expected_window = logical >= 51
            ? maximum_window_start
            : (logical == 0 ? 0 : logical - 1);
        const std::size_t expected_physical = logical - expected_window;
        expect_move(
            TabDirection::previous,
            TabBoundaryBehavior::wrap,
            logical,
            expected_window,
            expected_physical);
        if (logical == 0) break;
    }

    // The reverse boundary remains cyclic after the full traversal.
    expect_move(
        TabDirection::previous,
        TabBoundaryBehavior::wrap,
        57,
        maximum_window_start,
        7);

    // A native hover can change only the overflow-marker presentation. At
    // the first window, selecting physical slot 7 hides the right marker;
    // moving off that edge restores it without remapping or advancing the
    // mapping generation.
    ERUI_TEST_CHECK(viewport.initialize(
        logical_count, physical_capacity, navigation_margin));
    const TabViewport right_marker_visible = viewport;
    const std::uint64_t first_window_generation =
        viewport.mapping_generation();
    ViewportSnapshot before = snapshot(viewport);
    TabViewportChange change = viewport.select_physical(7);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(!change.mapping_changed);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() == first_window_generation);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::none);
    ERUI_TEST_CHECK(
        tab_overflow_indicators_changed(right_marker_visible, viewport));

    const TabViewport selected_right_edge = viewport;
    before = snapshot(viewport);
    change = viewport.select_physical(6);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(!change.mapping_changed);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() == first_window_generation);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 7) ==
        TabOverflowIndicator::next);
    ERUI_TEST_CHECK(
        tab_overflow_indicators_changed(selected_right_edge, viewport));

    // The final window has the symmetric presentation-only transition on
    // its left edge.
    ERUI_TEST_CHECK(viewport.initialize(
        logical_count,
        physical_capacity,
        navigation_margin,
        logical_count - 1));
    ERUI_TEST_CHECK(viewport.window_start() == maximum_window_start);
    const TabViewport left_marker_visible = viewport;
    const std::uint64_t final_window_generation =
        viewport.mapping_generation();
    before = snapshot(viewport);
    change = viewport.select_physical(0);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(!change.mapping_changed);
    ERUI_TEST_CHECK(viewport.window_start() == maximum_window_start);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() == final_window_generation);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 0) ==
        TabOverflowIndicator::none);
    ERUI_TEST_CHECK(
        tab_overflow_indicators_changed(left_marker_visible, viewport));

    const TabViewport selected_left_edge = viewport;
    before = snapshot(viewport);
    change = viewport.select_physical(1);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(!change.mapping_changed);
    ERUI_TEST_CHECK(viewport.window_start() == maximum_window_start);
    ERUI_TEST_CHECK(
        viewport.mapping_generation() == final_window_generation);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(viewport, 0) ==
        TabOverflowIndicator::previous);
    ERUI_TEST_CHECK(
        tab_overflow_indicators_changed(selected_left_edge, viewport));
}

void test_initial_stress_presentation_by_physical_capacity() {
    using namespace erui::detail;

    constexpr std::size_t stock_tab_count = 8;
    constexpr std::size_t logical_count = stock_tab_count + 50;

    TabViewport eight_slots;
    ERUI_TEST_CHECK(eight_slots.initialize(logical_count, 8, 1));
    std::size_t logical{invalid_tab_index};
    ERUI_TEST_CHECK(eight_slots.logical_for_physical(7, logical));
    ERUI_TEST_CHECK(logical == 7);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(eight_slots, 7) ==
        TabOverflowIndicator::next);
    ERUI_TEST_CHECK(!tab_viewport_exposes_custom_icon(
        eight_slots, stock_tab_count));

    TabViewport nine_slots;
    ERUI_TEST_CHECK(nine_slots.initialize(logical_count, 9, 1));
    ERUI_TEST_CHECK(nine_slots.logical_for_physical(8, logical));
    ERUI_TEST_CHECK(logical == stock_tab_count);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(nine_slots, 8) ==
        TabOverflowIndicator::next);
    ERUI_TEST_CHECK(!tab_viewport_exposes_custom_icon(
        nine_slots, stock_tab_count));

    TabViewport ten_slots;
    ERUI_TEST_CHECK(ten_slots.initialize(logical_count, 10, 1));
    ERUI_TEST_CHECK(ten_slots.logical_for_physical(8, logical));
    ERUI_TEST_CHECK(logical == stock_tab_count);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(ten_slots, 8) ==
        TabOverflowIndicator::none);
    ERUI_TEST_CHECK(ten_slots.logical_for_physical(9, logical));
    ERUI_TEST_CHECK(logical == stock_tab_count + 1);
    ERUI_TEST_CHECK(
        tab_overflow_indicator(ten_slots, 9) ==
        TabOverflowIndicator::next);
    ERUI_TEST_CHECK(tab_viewport_exposes_custom_icon(
        ten_slots, stock_tab_count));
}

void test_transparent_initial_raster_placeholder_policy() {
    using namespace erui::detail;

    const InitialRasterPlaceholderEligibility eligible{
        true,
        false,
        true,
        true,
        true,
        true,
    };
    ERUI_TEST_CHECK(
        should_use_transparent_initial_raster_placeholder(eligible));

    InitialRasterPlaceholderEligibility rejected = eligible;
    rejected.initial_mapping_publication = false;
    ERUI_TEST_CHECK(
        !should_use_transparent_initial_raster_placeholder(rejected));

    rejected = eligible;
    rejected.runtime_active = true;
    ERUI_TEST_CHECK(
        !should_use_transparent_initial_raster_placeholder(rejected));

    rejected = eligible;
    rejected.exact_runtime_top = false;
    ERUI_TEST_CHECK(
        !should_use_transparent_initial_raster_placeholder(rejected));

    rejected = eligible;
    rejected.exact_repaint_coordinate = false;
    ERUI_TEST_CHECK(
        !should_use_transparent_initial_raster_placeholder(rejected));

    rejected = eligible;
    rejected.exact_logical_raster = false;
    ERUI_TEST_CHECK(
        !should_use_transparent_initial_raster_placeholder(rejected));

    rejected = eligible;
    rejected.effective_raster = false;
    ERUI_TEST_CHECK(
        !should_use_transparent_initial_raster_placeholder(rejected));
}

void test_deferred_initial_presentation_refresh_is_one_shot() {
    using namespace erui::detail;

    constexpr std::size_t stock_tab_count = 8;
    constexpr std::size_t logical_count = stock_tab_count + 50;
    constexpr std::uint64_t epoch = 41;
    constexpr std::uint64_t frame = 100;

    DeferredTabPresentationRefresh refresh;
    ERUI_TEST_CHECK(!refresh.pending());
    ERUI_TEST_CHECK(
        refresh.poll(epoch, 1, frame) ==
        DeferredTabPresentationRefreshPoll::idle);

    TabViewport nine_slots;
    ERUI_TEST_CHECK(nine_slots.initialize(logical_count, 9, 1));
    ERUI_TEST_CHECK(!refresh.request_if_needed(
        nine_slots, stock_tab_count, epoch, frame));
    ERUI_TEST_CHECK(!refresh.pending());

    TabViewport ten_slots;
    ERUI_TEST_CHECK(ten_slots.initialize(logical_count, 10, 1));
    const std::uint64_t generation = ten_slots.mapping_generation();
    ERUI_TEST_CHECK(refresh.request_if_needed(
        ten_slots, stock_tab_count, epoch, frame));
    ERUI_TEST_CHECK(refresh.pending());
    ERUI_TEST_CHECK(
        refresh.poll(epoch, generation, frame) ==
        DeferredTabPresentationRefreshPoll::waiting);
    ERUI_TEST_CHECK(refresh.pending());
    ERUI_TEST_CHECK(
        refresh.poll(epoch, generation, frame + 1) ==
        DeferredTabPresentationRefreshPoll::ready);
    ERUI_TEST_CHECK(refresh.pending());
    refresh.consume();
    ERUI_TEST_CHECK(!refresh.pending());
    ERUI_TEST_CHECK(
        refresh.poll(epoch, generation, frame + 2) ==
        DeferredTabPresentationRefreshPoll::idle);

    // The retained RGBA8 catalogue can already be ready when System is
    // reopened. A new Configuration presentation epoch must therefore be
    // able to schedule its own repaint even when the viewport happens to
    // begin with the same mapping generation as the preceding epoch.
    constexpr std::uint64_t reopened_epoch = epoch + 1;
    constexpr std::uint64_t reopened_frame = frame + 20;
    ERUI_TEST_CHECK(refresh.request_if_needed(
        ten_slots,
        stock_tab_count,
        reopened_epoch,
        reopened_frame));
    ERUI_TEST_CHECK(
        refresh.poll(reopened_epoch, generation, reopened_frame) ==
        DeferredTabPresentationRefreshPoll::waiting);
    ERUI_TEST_CHECK(
        refresh.poll(reopened_epoch, generation, reopened_frame + 1) ==
        DeferredTabPresentationRefreshPoll::ready);
    refresh.consume();
    ERUI_TEST_CHECK(!refresh.pending());

    ERUI_TEST_CHECK(refresh.request_if_needed(
        ten_slots, stock_tab_count, epoch, frame));
    ERUI_TEST_CHECK(
        refresh.poll(epoch + 1, generation, frame + 1) ==
        DeferredTabPresentationRefreshPoll::stale);
    ERUI_TEST_CHECK(!refresh.pending());

    ERUI_TEST_CHECK(refresh.request_if_needed(
        ten_slots, stock_tab_count, epoch, frame));
    ERUI_TEST_CHECK(
        refresh.poll(epoch, generation + 1, frame + 1) ==
        DeferredTabPresentationRefreshPoll::stale);
    ERUI_TEST_CHECK(!refresh.pending());

    // If input remaps the strip while the retained catalogue is becoming
    // ready, the publication edge rebases the request to the current mapping
    // rather than abandoning the visible vector fallback indefinitely.
    TabViewport remapped = ten_slots;
    const std::uint64_t initial_generation =
        remapped.mapping_generation();
    ERUI_TEST_CHECK(refresh.request_if_needed(
        remapped, stock_tab_count, epoch, frame));
    for (std::size_t step = 0;
         step < remapped.logical_count() &&
             remapped.mapping_generation() == initial_generation;
         ++step) {
        ERUI_TEST_CHECK(remapped.move(
            TabDirection::next,
            TabBoundaryBehavior::wrap).valid);
    }
    ERUI_TEST_CHECK(
        remapped.mapping_generation() != initial_generation);
    ERUI_TEST_CHECK(
        refresh.poll(
            epoch,
            remapped.mapping_generation(),
            frame + 1) == DeferredTabPresentationRefreshPoll::stale);
    ERUI_TEST_CHECK(refresh.request_if_needed(
        remapped,
        stock_tab_count,
        epoch,
        frame + 1));
    ERUI_TEST_CHECK(
        refresh.poll(
            epoch,
            remapped.mapping_generation(),
            frame + 1) == DeferredTabPresentationRefreshPoll::waiting);
    ERUI_TEST_CHECK(
        refresh.poll(
            epoch,
            remapped.mapping_generation(),
            frame + 2) == DeferredTabPresentationRefreshPoll::ready);
    refresh.consume();
    ERUI_TEST_CHECK(!refresh.pending());

    // The frame sequence is allowed to wrap; only the exact scheduling frame
    // is deferred. Epoch and mapping-generation equality carry identity.
    ERUI_TEST_CHECK(refresh.request_if_needed(
        ten_slots,
        stock_tab_count,
        epoch,
        std::numeric_limits<std::uint64_t>::max()));
    ERUI_TEST_CHECK(
        refresh.poll(epoch, generation, 0) ==
        DeferredTabPresentationRefreshPoll::ready);
    ERUI_TEST_CHECK(refresh.pending());
    refresh.consume();
    ERUI_TEST_CHECK(!refresh.pending());

    ERUI_TEST_CHECK(!refresh.request_if_needed(
        ten_slots, stock_tab_count, 0, frame));
    ERUI_TEST_CHECK(!refresh.pending());
}

void test_public_capacity_traversal() {
    using namespace erui::detail;

    constexpr std::size_t stock_tab_count = 8;
    constexpr std::size_t logical_count =
        stock_tab_count + ERUI_CUSTOM_TAB_MAX_COUNT;
    constexpr std::size_t physical_capacity = 8;
    constexpr std::size_t navigation_margin = 1;
    static_assert(ERUI_CUSTOM_TAB_MAX_COUNT == 1024u);

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(
        logical_count, physical_capacity, navigation_margin));
    assert_invariants(viewport);

    // Traverse the complete 1,024-custom-tab public range. In particular,
    // crossing logical custom indices 255/256 proves the viewport has no
    // residual eight-bit identity boundary.
    for (std::size_t logical = 1; logical < logical_count; ++logical) {
        const ViewportSnapshot before = snapshot(viewport);
        const TabViewportChange change = viewport.move(TabDirection::next);
        assert_change(change, before, viewport);
        ERUI_TEST_CHECK(viewport.selected_logical() == logical);
    }
    ERUI_TEST_CHECK(viewport.selected_logical() == logical_count - 1);
    ERUI_TEST_CHECK(
        viewport.window_start() == logical_count - physical_capacity);
    ERUI_TEST_CHECK(viewport.selected_physical() == physical_capacity - 1);

    ViewportSnapshot before = snapshot(viewport);
    TabViewportChange change = viewport.move(
        TabDirection::next, TabBoundaryBehavior::wrap);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
    ERUI_TEST_CHECK(viewport.window_start() == 0);

    before = snapshot(viewport);
    change = viewport.move(
        TabDirection::previous, TabBoundaryBehavior::wrap);
    assert_change(change, before, viewport);
    ERUI_TEST_CHECK(viewport.selected_logical() == logical_count - 1);

    for (std::size_t logical = logical_count - 1; logical != 0; --logical) {
        before = snapshot(viewport);
        change = viewport.move(TabDirection::previous);
        assert_change(change, before, viewport);
        ERUI_TEST_CHECK(viewport.selected_logical() == logical - 1);
    }
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
    ERUI_TEST_CHECK(viewport.window_start() == 0);

    ERUI_TEST_CHECK(viewport.select_logical(stock_tab_count + 255).valid);
    ERUI_TEST_CHECK(
        viewport.selected_logical() == stock_tab_count + 255);
    ERUI_TEST_CHECK(viewport.select_logical(stock_tab_count + 256).valid);
    ERUI_TEST_CHECK(
        viewport.selected_logical() == stock_tab_count + 256);
    ERUI_TEST_CHECK(viewport.select_logical(logical_count - 1).valid);
    ERUI_TEST_CHECK(viewport.selected_logical() == logical_count - 1);
    assert_invariants(viewport);
}

void test_size_max_boundaries() {
    using namespace erui::detail;

    constexpr std::size_t maximum =
        std::numeric_limits<std::size_t>::max();

    TabViewport viewport;
    ERUI_TEST_CHECK(viewport.initialize(maximum, 9, 1, maximum - 1));
    assert_invariants(viewport);
    ERUI_TEST_CHECK(viewport.maximum_window_start() == maximum - 9);
    ERUI_TEST_CHECK(viewport.window_start() == maximum - 9);
    ERUI_TEST_CHECK(viewport.selected_physical() == 8);

    std::size_t logical = invalid_tab_index;
    ERUI_TEST_CHECK(viewport.logical_for_physical(8, logical));
    ERUI_TEST_CHECK(logical == maximum - 1);

    ERUI_TEST_CHECK(viewport.select_logical(maximum - 2).valid);
    ERUI_TEST_CHECK(viewport.selected_physical() == 7);
    ERUI_TEST_CHECK(viewport.move(TabDirection::next).valid);
    ERUI_TEST_CHECK(viewport.selected_logical() == maximum - 1);
    ERUI_TEST_CHECK(!viewport.select_logical(maximum).valid);

    ERUI_TEST_CHECK(viewport.initialize(5, maximum, maximum, 4));
    assert_invariants(viewport);
    ERUI_TEST_CHECK(viewport.visible_slot_count() == 5);
    ERUI_TEST_CHECK(viewport.navigation_margin() == 2);
    ERUI_TEST_CHECK(viewport.window_start() == 0);
    ERUI_TEST_CHECK(viewport.selected_physical() == 4);
}

void test_exhaustive_state_matrix() {
    using namespace erui::detail;

    for (std::size_t logical_count = 1; logical_count <= 64;
         ++logical_count) {
        for (std::size_t physical_capacity = 1; physical_capacity <= 12;
             ++physical_capacity) {
            for (std::size_t requested_margin = 0; requested_margin <= 16;
                 ++requested_margin) {
                TabViewport viewport;
                ERUI_TEST_CHECK(viewport.initialize(
                    logical_count, physical_capacity, requested_margin));
                assert_invariants(viewport);

                const std::size_t visible = std::min(
                    logical_count, physical_capacity);
                ERUI_TEST_CHECK(
                    viewport.navigation_margin() ==
                    std::min(requested_margin, (visible - 1) / 2));

                // Every direct logical selection must produce a valid,
                // visible selection and internally consistent transition.
                for (std::size_t logical = 0; logical < logical_count;
                     ++logical) {
                    const ViewportSnapshot before = snapshot(viewport);
                    const TabViewportChange change =
                        viewport.select_logical(logical);
                    assert_change(change, before, viewport);
                    ERUI_TEST_CHECK(
                        viewport.selected_logical() == logical);
                }

                // Saturating forward traversal reaches the final tab.
                ERUI_TEST_CHECK(viewport.initialize(
                    logical_count, physical_capacity, requested_margin));
                for (std::size_t logical = 1; logical < logical_count;
                     ++logical) {
                    const ViewportSnapshot before = snapshot(viewport);
                    const TabViewportChange change =
                        viewport.move(TabDirection::next);
                    assert_change(change, before, viewport);
                    ERUI_TEST_CHECK(
                        viewport.selected_logical() == logical);
                }
                ViewportSnapshot before = snapshot(viewport);
                TabViewportChange change =
                    viewport.move(TabDirection::next);
                assert_change(change, before, viewport);
                ERUI_TEST_CHECK(!change.logical_selection_changed);

                // Saturating reverse traversal returns to the first tab.
                for (std::size_t logical = logical_count - 1; logical != 0;
                     --logical) {
                    before = snapshot(viewport);
                    change = viewport.move(TabDirection::previous);
                    assert_change(change, before, viewport);
                    ERUI_TEST_CHECK(
                        viewport.selected_logical() == logical - 1);
                }
                before = snapshot(viewport);
                change = viewport.move(TabDirection::previous);
                assert_change(change, before, viewport);
                ERUI_TEST_CHECK(!change.logical_selection_changed);

                // A physical hover selects the logical tab currently mapped
                // to that slot without remapping the strip under the cursor.
                ERUI_TEST_CHECK(viewport.initialize(
                    logical_count,
                    physical_capacity,
                    requested_margin,
                    logical_count / 2));
                for (std::size_t physical = 0;
                     physical < viewport.visible_slot_count(); ++physical) {
                    std::size_t expected_logical = invalid_tab_index;
                    ERUI_TEST_CHECK(viewport.logical_for_physical(
                        physical, expected_logical));
                    before = snapshot(viewport);
                    change = viewport.select_physical(physical);
                    assert_change(change, before, viewport);
                    ERUI_TEST_CHECK(
                        viewport.selected_logical() == expected_logical);
                }
            }
        }
    }
}

} // namespace

int main() {
    test_invalid_and_reset_states();
    test_tabs_that_fit();
    test_canonical_overflow_sequence();
    test_hover_selection_preserves_mapping();
    test_direct_mouse_edge_jumps_align_consistently();
    test_overflow_indicator_policy();
    test_margins_and_single_slot();
    test_wrap_and_direct_selection();
    test_final_stress_configuration_traversal();
    test_initial_stress_presentation_by_physical_capacity();
    test_transparent_initial_raster_placeholder_policy();
    test_deferred_initial_presentation_refresh_is_one_shot();
    test_public_capacity_traversal();
    test_size_max_boundaries();
    test_exhaustive_state_matrix();
    return 0;
}
