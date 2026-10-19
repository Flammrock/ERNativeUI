#include "tab_pointer_dwell.hpp"
#include "tab_viewport.hpp"

#include "test_assertions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using erui::detail::TabBoundaryBehavior;
using erui::detail::TabDirection;
using erui::detail::TabPointerDwell;
using erui::detail::TabPointerDwellEdge;
using erui::detail::TabPointerDwellPhase;
using erui::detail::TabPointerDwellTiming;
using erui::detail::TabPointerDwellUpdate;
using erui::detail::TabViewport;

constexpr std::size_t left_slot = 0;
constexpr std::size_t right_slot = 7;
constexpr std::uint32_t initial_x = 0x44800000u;
constexpr std::uint32_t initial_y = 0x43800000u;

void assert_idle(const TabPointerDwell& dwell) {
    ERUI_TEST_CHECK(!dwell.active());
    ERUI_TEST_CHECK(!dwell.repeat_due());
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::idle);
    ERUI_TEST_CHECK(dwell.edge() == TabPointerDwellEdge::none);
    ERUI_TEST_CHECK(
        dwell.physical_slot() == TabPointerDwell::invalid_physical_slot);
    ERUI_TEST_CHECK(dwell.x_bits() == 0);
    ERUI_TEST_CHECK(dwell.y_bits() == 0);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 0);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 0);
    ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == 0);
}

void arm(
    TabPointerDwell& dwell,
    TabPointerDwellEdge edge,
    std::size_t physical_slot,
    std::uint64_t now_ms,
    bool has_more = true,
    std::uint32_t x_bits = initial_x,
    std::uint32_t y_bits = initial_y) {
    dwell.begin_transition(edge, physical_slot, x_bits, y_bits);
    ERUI_TEST_CHECK(
        dwell.phase() == TabPointerDwellPhase::awaiting_commit);
    ERUI_TEST_CHECK(dwell.edge() == edge);
    ERUI_TEST_CHECK(dwell.physical_slot() == physical_slot);
    dwell.transition_committed(now_ms, has_more);
}

[[nodiscard]] bool has_more(
    const TabViewport& viewport,
    TabPointerDwellEdge edge) {
    if (edge == TabPointerDwellEdge::previous) {
        return viewport.selected_logical() != 0;
    }
    if (edge == TabPointerDwellEdge::next) {
        return viewport.selected_logical() + 1 < viewport.logical_count();
    }
    return false;
}

void test_idle_and_first_hover() {
    TabPointerDwell dwell;
    assert_idle(dwell);

    TabPointerDwellUpdate update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        10);
    ERUI_TEST_CHECK(!update.suppress_natural_selection);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(!update.cancelled);
    ERUI_TEST_CHECK(!update.position_changed);
    ERUI_TEST_CHECK(!update.deadline_restarted);
    assert_idle(dwell);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        20);
    ERUI_TEST_CHECK(!update.repeat_due);
    assert_idle(dwell);

    dwell.begin_transition(
        TabPointerDwellEdge::none,
        right_slot,
        initial_x,
        initial_y);
    assert_idle(dwell);
    dwell.begin_transition(
        TabPointerDwellEdge::next,
        TabPointerDwell::invalid_physical_slot,
        initial_x,
        initial_y);
    assert_idle(dwell);

    dwell.begin_transition(
        TabPointerDwellEdge::next,
        right_slot,
        initial_x,
        initial_y);
    ERUI_TEST_CHECK(dwell.active());
    ERUI_TEST_CHECK(
        dwell.phase() == TabPointerDwellPhase::awaiting_commit);

    // An unexpected second begin cannot overwrite the transaction that is
    // already waiting for the native viewport commit.
    dwell.begin_transition(
        TabPointerDwellEdge::previous,
        left_slot,
        initial_x + 1,
        initial_y + 1);
    ERUI_TEST_CHECK(dwell.edge() == TabPointerDwellEdge::next);
    ERUI_TEST_CHECK(dwell.physical_slot() == right_slot);
    ERUI_TEST_CHECK(dwell.x_bits() == initial_x);
    ERUI_TEST_CHECK(dwell.y_bits() == initial_y);

    dwell.reset();
    assert_idle(dwell);
}

void test_commit_suppression_and_exact_repeat() {
    TabPointerDwell dwell;
    dwell.begin_transition(
        TabPointerDwellEdge::next,
        right_slot,
        initial_x,
        initial_y);

    TabPointerDwellUpdate update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        50);
    ERUI_TEST_CHECK(update.suppress_natural_selection);
    ERUI_TEST_CHECK(!update.repeat_due);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        100000);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(
        dwell.phase() == TabPointerDwellPhase::awaiting_commit);

    dwell.transition_committed(100, true);
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::cooldown);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 250);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 350);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        349);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::cooldown);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        350);
    ERUI_TEST_CHECK(update.repeat_due);
    ERUI_TEST_CHECK(dwell.repeat_due());
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::repeat_due);

    // The due state is level-triggered until the adapter accepts exactly one
    // request. It never starts another cooldown by itself.
    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        5000);
    ERUI_TEST_CHECK(update.repeat_due);
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::repeat_due);
    update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        5000);
    ERUI_TEST_CHECK(update.suppress_natural_selection);
    ERUI_TEST_CHECK(update.repeat_due);

    dwell.repeat_dispatched();
    ERUI_TEST_CHECK(!dwell.repeat_due());
    ERUI_TEST_CHECK(
        dwell.phase() == TabPointerDwellPhase::awaiting_commit);
    dwell.transition_committed(5000, true);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 200);
    ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == 1);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 5200);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        5199);
    ERUI_TEST_CHECK(!update.repeat_due);
    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        5200);
    ERUI_TEST_CHECK(update.repeat_due);

    dwell.repeat_dispatched();
    dwell.transition_committed(5200, true);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 180);
    ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == 2);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 5380);
}

void test_motion_restarts_before_expiry() {
    TabPointerDwell dwell;
    arm(dwell, TabPointerDwellEdge::next, right_slot, 0);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 250);

    // Natural motion at the exact deadline takes precedence over expiry.
    TabPointerDwellUpdate update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x + 1,
        initial_y,
        250);
    ERUI_TEST_CHECK(update.suppress_natural_selection);
    ERUI_TEST_CHECK(update.position_changed);
    ERUI_TEST_CHECK(update.deadline_restarted);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::cooldown);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 500);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x + 1,
        initial_y,
        499);
    ERUI_TEST_CHECK(!update.repeat_due);

    // Poll-only motion is equally authoritative and restarts the full delay.
    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x + 1,
        initial_y + 1,
        500);
    ERUI_TEST_CHECK(update.position_changed);
    ERUI_TEST_CHECK(update.deadline_restarted);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 750);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x + 1,
        initial_y + 1,
        750);
    ERUI_TEST_CHECK(update.repeat_due);

    // Motion can retract a not-yet-dispatched repeat.
    update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x + 2,
        initial_y + 1,
        750);
    ERUI_TEST_CHECK(update.suppress_natural_selection);
    ERUI_TEST_CHECK(update.position_changed);
    ERUI_TEST_CHECK(update.deadline_restarted);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::cooldown);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 1000);

    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x + 2,
        initial_y + 1,
        1000);
    ERUI_TEST_CHECK(update.repeat_due);
    dwell.repeat_dispatched();

    // Motion while a transition is pending is remembered, but the next
    // countdown starts only when that transition really commits.
    update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x + 2,
        initial_y + 2,
        1050);
    ERUI_TEST_CHECK(update.suppress_natural_selection);
    ERUI_TEST_CHECK(update.position_changed);
    ERUI_TEST_CHECK(!update.deadline_restarted);
    dwell.transition_committed(1100, true);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 250);
    ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == 0);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 1350);
}

void test_acceleration_decay_floor_and_motion_reset() {
    TabPointerDwell dwell{TabPointerDwellTiming{250, 200, 20, 50}};
    arm(dwell, TabPointerDwellEdge::next, right_slot, 100);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 250);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 350);

    std::uint64_t now = 350;
    constexpr std::array<std::uint64_t, 12> expected_delays{
        200, 180, 160, 140, 120, 100, 80, 60, 50, 50, 50, 50,
    };
    for (std::size_t index = 0; index < expected_delays.size(); ++index) {
        const TabPointerDwellUpdate due = dwell.observe_poll(
            static_cast<std::int32_t>(right_slot),
            initial_x,
            initial_y,
            now);
        ERUI_TEST_CHECK(due.repeat_due);
        dwell.repeat_dispatched();
        dwell.transition_committed(now, true);
        ERUI_TEST_CHECK(dwell.current_delay_ms() == expected_delays[index]);
        ERUI_TEST_CHECK(
            dwell.automatic_repeats_committed() == index + 1);
        ERUI_TEST_CHECK(
            dwell.deadline_ms() == now + expected_delays[index]);
        now = dwell.deadline_ms();
    }

    // Once at the floor, one accepted repeat still requires a complete
    // dispatch/commit transaction. The policy never emits catch-up
    // transitions by itself.
    TabPointerDwellUpdate due = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        now);
    ERUI_TEST_CHECK(due.repeat_due);
    due = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        now + 1000);
    ERUI_TEST_CHECK(due.repeat_due);

    // Genuine motion retracts the due repeat, clears acceleration, and waits
    // the full initial delay from the motion timestamp.
    const TabPointerDwellUpdate moved = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x + 1,
        initial_y,
        now + 1000);
    ERUI_TEST_CHECK(moved.position_changed);
    ERUI_TEST_CHECK(moved.deadline_restarted);
    ERUI_TEST_CHECK(!moved.repeat_due);
    ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == 0);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 250);
    ERUI_TEST_CHECK(dwell.deadline_ms() == now + 1250);
}

void test_motion_during_automatic_commit_resets_acceleration() {
    TabPointerDwell dwell{TabPointerDwellTiming{250, 200, 20, 50}};
    arm(dwell, TabPointerDwellEdge::next, right_slot, 0);

    TabPointerDwellUpdate update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        250);
    ERUI_TEST_CHECK(update.repeat_due);
    dwell.repeat_dispatched();

    // The native selection has been requested but has not reached the commit
    // seam yet. Motion here must survive that commit and restart at 250 ms.
    update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x + 1,
        initial_y,
        251);
    ERUI_TEST_CHECK(update.position_changed);
    ERUI_TEST_CHECK(!update.deadline_restarted);
    dwell.transition_committed(252, true);
    ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == 0);
    ERUI_TEST_CHECK(dwell.current_delay_ms() == 250);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 502);
}

void test_zero_decay_is_fixed_repeat_cadence() {
    TabPointerDwell dwell{TabPointerDwellTiming{250, 200, 0, 50}};
    arm(dwell, TabPointerDwellEdge::previous, left_slot, 0);
    ERUI_TEST_CHECK(dwell.deadline_ms() == 250);

    std::uint64_t now = 250;
    for (std::size_t repeat = 1; repeat <= 32; ++repeat) {
        const TabPointerDwellUpdate due = dwell.observe_poll(
            static_cast<std::int32_t>(left_slot),
            initial_x,
            initial_y,
            now);
        ERUI_TEST_CHECK(due.repeat_due);
        dwell.repeat_dispatched();
        dwell.transition_committed(now, true);
        ERUI_TEST_CHECK(dwell.current_delay_ms() == 200);
        ERUI_TEST_CHECK(dwell.automatic_repeats_committed() == repeat);
        now += 200;
        ERUI_TEST_CHECK(dwell.deadline_ms() == now);
    }
}

void test_minimum_delay_normalization_and_zero_floor() {
    // A malformed floor above the base repeat delay is normalized down. It
    // must never make an accelerating repeat slower than its configured base.
    TabPointerDwell clamped{TabPointerDwellTiming{250, 100, 10, 500}};
    ERUI_TEST_CHECK(clamped.timing().initial_delay_ms == 250);
    ERUI_TEST_CHECK(clamped.timing().repeat_delay_ms == 100);
    ERUI_TEST_CHECK(clamped.timing().repeat_decay_ms == 10);
    ERUI_TEST_CHECK(clamped.timing().minimum_repeat_delay_ms == 100);
    arm(clamped, TabPointerDwellEdge::next, right_slot, 0);
    auto update = clamped.observe_poll(
        static_cast<std::int32_t>(right_slot), initial_x, initial_y, 250);
    ERUI_TEST_CHECK(update.repeat_due);
    clamped.repeat_dispatched();
    clamped.transition_committed(250, true);
    ERUI_TEST_CHECK(clamped.current_delay_ms() == 100);
    update = clamped.observe_poll(
        static_cast<std::int32_t>(right_slot), initial_x, initial_y, 350);
    ERUI_TEST_CHECK(update.repeat_due);
    clamped.repeat_dispatched();
    clamped.transition_committed(350, true);
    ERUI_TEST_CHECK(clamped.current_delay_ms() == 100);

    // A zero minimum explicitly retains the old eventually-zero behavior.
    // A decay larger than the whole range must saturate without underflow.
    TabPointerDwell zero_floor{TabPointerDwellTiming{
        250,
        200,
        std::numeric_limits<std::uint64_t>::max(),
        0,
    }};
    arm(zero_floor, TabPointerDwellEdge::next, right_slot, 0);
    update = zero_floor.observe_poll(
        static_cast<std::int32_t>(right_slot), initial_x, initial_y, 250);
    ERUI_TEST_CHECK(update.repeat_due);
    zero_floor.repeat_dispatched();
    zero_floor.transition_committed(250, true);
    ERUI_TEST_CHECK(zero_floor.current_delay_ms() == 200);
    update = zero_floor.observe_poll(
        static_cast<std::int32_t>(right_slot), initial_x, initial_y, 450);
    ERUI_TEST_CHECK(update.repeat_due);
    zero_floor.repeat_dispatched();
    zero_floor.transition_committed(450, true);
    ERUI_TEST_CHECK(zero_floor.current_delay_ms() == 0);
}

void test_leave_cancel_and_immediate_reentry() {
    TabPointerDwell dwell;
    arm(dwell, TabPointerDwellEdge::next, right_slot, 0);

    TabPointerDwellUpdate update = dwell.observe_natural(
        6, initial_x - 1, initial_y, 100);
    ERUI_TEST_CHECK(update.cancelled);
    ERUI_TEST_CHECK(!update.suppress_natural_selection);
    assert_idle(dwell);

    // Once cancelled, re-entering the edge is the first native hover again.
    update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        101);
    ERUI_TEST_CHECK(!update.suppress_natural_selection);
    assert_idle(dwell);

    arm(dwell, TabPointerDwellEdge::next, right_slot, 200);
    update = dwell.observe_poll(-1, 0, 0, 250);
    ERUI_TEST_CHECK(update.cancelled);
    ERUI_TEST_CHECK(!update.repeat_due);
    assert_idle(dwell);

    arm(dwell, TabPointerDwellEdge::previous, left_slot, 300);
    update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        350);
    ERUI_TEST_CHECK(update.cancelled);
    ERUI_TEST_CHECK(!update.suppress_natural_selection);
    assert_idle(dwell);
}

void test_controller_failure_and_terminal_commit() {
    TabPointerDwell dwell;
    arm(dwell, TabPointerDwellEdge::next, right_slot, 0);
    dwell.controller_navigation();
    assert_idle(dwell);

    // Stationary polling can validate an active dwell, but can never create
    // one after controller navigation.
    TabPointerDwellUpdate update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        5000);
    ERUI_TEST_CHECK(!update.repeat_due);
    assert_idle(dwell);

    dwell.begin_transition(
        TabPointerDwellEdge::next,
        right_slot,
        initial_x,
        initial_y);
    dwell.controller_navigation();
    dwell.transition_committed(10, true);
    assert_idle(dwell);

    dwell.begin_transition(
        TabPointerDwellEdge::next,
        right_slot,
        initial_x,
        initial_y);
    dwell.transition_failed();
    assert_idle(dwell);

    dwell.begin_transition(
        TabPointerDwellEdge::next,
        right_slot,
        initial_x,
        initial_y);
    dwell.transition_committed(20, false);
    assert_idle(dwell);
}

void test_zero_delay_clock_regression_and_saturation() {
    TabPointerDwell immediate{0};
    arm(immediate, TabPointerDwellEdge::next, right_slot, 42);
    TabPointerDwellUpdate update = immediate.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        42);
    ERUI_TEST_CHECK(update.repeat_due);

    constexpr std::uint64_t maximum =
        std::numeric_limits<std::uint64_t>::max();
    TabPointerDwell near_wrap{10};
    arm(
        near_wrap,
        TabPointerDwellEdge::next,
        right_slot,
        maximum - 5);
    ERUI_TEST_CHECK(near_wrap.deadline_ms() == maximum);

    // Saturating the diagnostic deadline must not shorten the real delay.
    update = near_wrap.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        maximum);
    ERUI_TEST_CHECK(!update.repeat_due);

    // A regressed clock is rebased instead of underflowing into an immediate
    // repeat. GetTickCount64 does not regress in practice, but this keeps the
    // pure helper total over every uint64_t input.
    update = near_wrap.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        0);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(update.deadline_restarted);
    ERUI_TEST_CHECK(near_wrap.deadline_ms() == 10);
    update = near_wrap.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        9);
    ERUI_TEST_CHECK(!update.repeat_due);
    update = near_wrap.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        10);
    ERUI_TEST_CHECK(update.repeat_due);
}

void test_both_directions_and_non_wrapping_viewport() {
    TabViewport viewport;
    constexpr std::size_t logical_tab_count = 48;
    ERUI_TEST_CHECK(viewport.initialize(logical_tab_count, 8, 1));
    TabPointerDwell dwell;

    // The first native hover remains immediate. It selects the logical tab
    // under the ellipsis and aligns it to the right navigation anchor.
    TabPointerDwellUpdate update = dwell.observe_natural(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        0);
    ERUI_TEST_CHECK(!update.suppress_natural_selection);
    auto change = viewport.select_physical_aligned(right_slot);
    ERUI_TEST_CHECK(change.valid);
    ERUI_TEST_CHECK(viewport.selected_logical() == 7);
    ERUI_TEST_CHECK(viewport.selected_physical() == 6);
    ERUI_TEST_CHECK(viewport.window_start() == 1);
    dwell.begin_transition(
        TabPointerDwellEdge::next,
        right_slot,
        initial_x,
        initial_y);
    dwell.transition_committed(
        0, has_more(viewport, TabPointerDwellEdge::next));

    std::uint64_t now = 0;
    for (std::size_t expected = 8;
         expected < logical_tab_count;
         ++expected) {
        now = dwell.deadline_ms();
        update = dwell.observe_poll(
            static_cast<std::int32_t>(right_slot),
            initial_x,
            initial_y,
            now);
        ERUI_TEST_CHECK(update.repeat_due);
        dwell.repeat_dispatched();
        change = viewport.move(
            TabDirection::next, TabBoundaryBehavior::stop);
        ERUI_TEST_CHECK(change.valid);
        ERUI_TEST_CHECK(change.logical_selection_changed);
        ERUI_TEST_CHECK(viewport.selected_logical() == expected);
        dwell.transition_committed(
            now, has_more(viewport, TabPointerDwellEdge::next));
        if (expected >= 16 && expected + 1 < logical_tab_count) {
            ERUI_TEST_CHECK(dwell.current_delay_ms() == 50);
        }
    }
    ERUI_TEST_CHECK(
        viewport.selected_logical() == logical_tab_count - 1);
    ERUI_TEST_CHECK(viewport.selected_physical() == right_slot);
    assert_idle(dwell);

    // No stationary mouse event can wrap at the catalogue endpoint.
    update = dwell.observe_poll(
        static_cast<std::int32_t>(right_slot),
        initial_x,
        initial_y,
        now + 100000);
    ERUI_TEST_CHECK(!update.repeat_due);
    ERUI_TEST_CHECK(
        viewport.selected_logical() == logical_tab_count - 1);

    // Mirror the sequence from the left edge. The final visible Game Options
    // icon receives the last non-wrapping step before the dwell disarms.
    update = dwell.observe_natural(
        static_cast<std::int32_t>(left_slot),
        initial_x - 100,
        initial_y,
        now);
    ERUI_TEST_CHECK(!update.suppress_natural_selection);
    change = viewport.select_physical_aligned(left_slot);
    ERUI_TEST_CHECK(change.valid);
    ERUI_TEST_CHECK(viewport.selected_logical() == 40);
    ERUI_TEST_CHECK(viewport.selected_physical() == 1);
    ERUI_TEST_CHECK(viewport.window_start() == 39);
    dwell.begin_transition(
        TabPointerDwellEdge::previous,
        left_slot,
        initial_x - 100,
        initial_y);
    dwell.transition_committed(
        now, has_more(viewport, TabPointerDwellEdge::previous));

    for (std::size_t expected = 39;; --expected) {
        now = dwell.deadline_ms();
        update = dwell.observe_poll(
            static_cast<std::int32_t>(left_slot),
            initial_x - 100,
            initial_y,
            now);
        ERUI_TEST_CHECK(update.repeat_due);
        dwell.repeat_dispatched();
        change = viewport.move(
            TabDirection::previous, TabBoundaryBehavior::stop);
        ERUI_TEST_CHECK(change.valid);
        ERUI_TEST_CHECK(change.logical_selection_changed);
        ERUI_TEST_CHECK(viewport.selected_logical() == expected);
        dwell.transition_committed(
            now, has_more(viewport, TabPointerDwellEdge::previous));
        if (expected == 0) break;
    }
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
    ERUI_TEST_CHECK(viewport.selected_physical() == left_slot);
    assert_idle(dwell);

    // Controller navigation remains a separate, cyclic policy.
    change = viewport.move(
        TabDirection::previous, TabBoundaryBehavior::wrap);
    ERUI_TEST_CHECK(change.valid);
    ERUI_TEST_CHECK(
        viewport.selected_logical() == logical_tab_count - 1);
    change = viewport.move(
        TabDirection::next, TabBoundaryBehavior::wrap);
    ERUI_TEST_CHECK(change.valid);
    ERUI_TEST_CHECK(viewport.selected_logical() == 0);
}

void test_delay_and_direction_matrix() {
    constexpr std::array<std::uint64_t, 4> delays{0, 1, 17, 1000};
    constexpr std::array<TabPointerDwellEdge, 2> edges{
        TabPointerDwellEdge::previous,
        TabPointerDwellEdge::next,
    };

    for (const std::uint64_t delay : delays) {
        for (const TabPointerDwellEdge edge : edges) {
            const std::size_t slot = edge == TabPointerDwellEdge::previous
                ? left_slot
                : right_slot;
            TabPointerDwell dwell{delay};
            arm(dwell, edge, slot, 100);
            ERUI_TEST_CHECK(dwell.edge() == edge);
            ERUI_TEST_CHECK(dwell.deadline_ms() == 100 + delay);

            if (delay != 0) {
                const TabPointerDwellUpdate before = dwell.observe_poll(
                    static_cast<std::int32_t>(slot),
                    initial_x,
                    initial_y,
                    100 + delay - 1);
                ERUI_TEST_CHECK(!before.repeat_due);
            }
            const TabPointerDwellUpdate due = dwell.observe_poll(
                static_cast<std::int32_t>(slot),
                initial_x,
                initial_y,
                100 + delay);
            ERUI_TEST_CHECK(due.repeat_due);
            ERUI_TEST_CHECK(dwell.phase() == TabPointerDwellPhase::repeat_due);

            dwell.repeat_dispatched();
            ERUI_TEST_CHECK(
                dwell.phase() == TabPointerDwellPhase::awaiting_commit);
            dwell.transition_committed(100 + delay, false);
            assert_idle(dwell);
        }
    }
}

} // namespace

int main() {
    test_idle_and_first_hover();
    test_commit_suppression_and_exact_repeat();
    test_motion_restarts_before_expiry();
    test_acceleration_decay_floor_and_motion_reset();
    test_motion_during_automatic_commit_resets_acceleration();
    test_zero_decay_is_fixed_repeat_cadence();
    test_minimum_delay_normalization_and_zero_floor();
    test_leave_cancel_and_immediate_reentry();
    test_controller_failure_and_terminal_commit();
    test_zero_delay_clock_regression_and_saturation();
    test_both_directions_and_non_wrapping_viewport();
    test_delay_and_direction_matrix();
    return 0;
}
