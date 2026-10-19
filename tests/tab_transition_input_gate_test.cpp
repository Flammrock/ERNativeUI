#include "tab_transition_input_gate.hpp"

#include "test_assertions.hpp"

namespace {

using erui::detail::TabTransitionCommitResult;
using erui::detail::TabTransitionInputAction;
using erui::detail::TabTransitionSelectorAnchor;
using erui::detail::should_cancel_pointer_dwell_for_composite_input;
using erui::detail::tab_transition_input_action;
using erui::detail::tab_transition_selector_anchor;

void test_transition_input_gate() {
    ERUI_TEST_CHECK(tab_transition_input_action(
        TabTransitionCommitResult::failed) ==
        TabTransitionInputAction::continue_normal_path);
    ERUI_TEST_CHECK(tab_transition_input_action(
        TabTransitionCommitResult::waiting) ==
        TabTransitionInputAction::invoke_with_blocked_input);
    ERUI_TEST_CHECK(tab_transition_input_action(
        TabTransitionCommitResult::committed) ==
        TabTransitionInputAction::invoke_with_live_input);
}

void test_overlapping_selection_anchor() {
    constexpr std::uint32_t source_physical = 7;
    constexpr std::uint32_t target_physical = 6;

    ERUI_TEST_CHECK(tab_transition_selector_anchor(
        TabTransitionSelectorAnchor::source,
        source_physical,
        target_physical) == source_physical);
    ERUI_TEST_CHECK(tab_transition_selector_anchor(
        TabTransitionSelectorAnchor::target,
        source_physical,
        target_physical) == target_physical);
}

void test_pointer_dwell_input_gate_policy() {
    constexpr auto ordinary =
        TabTransitionInputAction::continue_normal_path;
    constexpr auto waiting =
        TabTransitionInputAction::invoke_with_blocked_input;
    constexpr auto committed =
        TabTransitionInputAction::invoke_with_live_input;

    ERUI_TEST_CHECK(!should_cancel_pointer_dwell_for_composite_input(
        ordinary, true, true));
    ERUI_TEST_CHECK(should_cancel_pointer_dwell_for_composite_input(
        ordinary, true, false));
    ERUI_TEST_CHECK(should_cancel_pointer_dwell_for_composite_input(
        ordinary, false, false));
    ERUI_TEST_CHECK(should_cancel_pointer_dwell_for_composite_input(
        ordinary, false, true));

    // Waiting and coherent commit frames own their temporary input gate.
    // A persistent disabled gate is observed and cancelled on the following
    // ordinary frame instead.
    ERUI_TEST_CHECK(!should_cancel_pointer_dwell_for_composite_input(
        waiting, true, false));
    ERUI_TEST_CHECK(!should_cancel_pointer_dwell_for_composite_input(
        waiting, false, false));
    ERUI_TEST_CHECK(!should_cancel_pointer_dwell_for_composite_input(
        committed, true, false));
    ERUI_TEST_CHECK(!should_cancel_pointer_dwell_for_composite_input(
        committed, false, false));
    ERUI_TEST_CHECK(!should_cancel_pointer_dwell_for_composite_input(
        committed, true, true));
}

} // namespace

int main() {
    test_transition_input_gate();
    test_overlapping_selection_anchor();
    test_pointer_dwell_input_gate_policy();
    return 0;
}
