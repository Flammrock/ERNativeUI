#pragma once

#include <cstdint>

namespace erui::detail {

enum class TabTransitionCommitResult : std::uint8_t {
    failed,
    waiting,
    committed,
};

enum class TabTransitionInputAction : std::uint8_t {
    continue_normal_path,
    invoke_with_blocked_input,
    invoke_with_live_input,
};

enum class TabTransitionSelectorAnchor : std::uint8_t {
    source,
    target,
};

// A native held-button repeat may already be latched when the composite input
// gate is blocked. If its selection callback arrives while a viewport
// transition is pending, keep the selector on the mapping phase that owns the
// in-flight transaction. Before phase-A relocation that is the provisional
// native/source cell; once relocation has begun it is the desired/target cell.
[[nodiscard]] constexpr std::uint32_t tab_transition_selector_anchor(
    TabTransitionSelectorAnchor anchor,
    std::uint32_t source_physical,
    std::uint32_t target_physical) noexcept {
    return anchor == TabTransitionSelectorAnchor::target
        ? target_physical
        : source_physical;
}

[[nodiscard]] constexpr TabTransitionInputAction
tab_transition_input_action(TabTransitionCommitResult result) noexcept {
    switch (result) {
    case TabTransitionCommitResult::waiting:
        return TabTransitionInputAction::invoke_with_blocked_input;
    case TabTransitionCommitResult::committed:
        return TabTransitionInputAction::invoke_with_live_input;
    case TabTransitionCommitResult::failed:
    default:
        return TabTransitionInputAction::continue_normal_path;
    }
}

// A transition-controlled frame can deliberately carry a disabled native
// input gate. The adapter must preserve an already-armed pointer dwell across
// that frame; the next ordinary frame remains responsible for cancelling the
// dwell if input is still unavailable.
[[nodiscard]] constexpr bool
should_cancel_pointer_dwell_for_composite_input(
    TabTransitionInputAction action,
    bool input_gate_read,
    bool input_enabled) noexcept {
    return action == TabTransitionInputAction::continue_normal_path &&
        (!input_gate_read || !input_enabled);
}

} // namespace erui::detail
