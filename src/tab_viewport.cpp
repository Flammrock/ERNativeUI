#include "tab_viewport.hpp"

#include <algorithm>

namespace erui::detail {

bool TabViewport::initialize(
    std::size_t logical_count,
    std::size_t physical_capacity,
    std::size_t requested_navigation_margin,
    std::size_t initial_logical) noexcept {
    if (logical_count == 0 || physical_capacity == 0 ||
        initial_logical >= logical_count) {
        reset();
        return false;
    }

    logical_count_ = logical_count;
    physical_capacity_ = physical_capacity;
    requested_navigation_margin_ = requested_navigation_margin;
    const std::size_t visible = visible_slot_count();
    navigation_margin_ = std::min(
        requested_navigation_margin_, (visible - 1) / 2);
    window_start_ = 0;
    selected_logical_ = initial_logical;
    window_start_ = aligned_window_start(initial_logical);
    advance_mapping_generation();
    return true;
}

void TabViewport::reset() noexcept {
    const bool had_mapping = initialized();
    logical_count_ = 0;
    physical_capacity_ = 0;
    requested_navigation_margin_ = 0;
    navigation_margin_ = 0;
    window_start_ = 0;
    selected_logical_ = invalid_tab_index;
    if (had_mapping) advance_mapping_generation();
}

TabViewportChange TabViewport::move(
    TabDirection direction,
    TabBoundaryBehavior boundary) noexcept {
    if (!initialized()) return unchanged_change(false);

    std::size_t target = selected_logical_;
    switch (direction) {
    case TabDirection::previous:
        if (selected_logical_ != 0) {
            target = selected_logical_ - 1;
        } else if (boundary == TabBoundaryBehavior::wrap) {
            target = logical_count_ - 1;
        }
        break;
    case TabDirection::next:
        if (selected_logical_ != logical_count_ - 1) {
            target = selected_logical_ + 1;
        } else if (boundary == TabBoundaryBehavior::wrap) {
            target = 0;
        }
        break;
    default:
        return unchanged_change(false);
    }
    return select_logical(target);
}

TabViewportChange TabViewport::select_logical(
    std::size_t logical_index) noexcept {
    if (!initialized() || logical_index >= logical_count_) {
        return unchanged_change(false);
    }

    const std::size_t previous_logical = selected_logical_;
    const std::size_t previous_physical = selected_physical();
    const std::size_t previous_window_start = window_start_;

    selected_logical_ = logical_index;
    window_start_ = aligned_window_start(logical_index);
    if (window_start_ != previous_window_start) {
        advance_mapping_generation();
    }

    TabViewportChange result{};
    result.valid = true;
    result.logical_selection_changed =
        previous_logical != selected_logical_;
    result.previous_logical = previous_logical;
    result.selected_logical = selected_logical_;
    result.previous_physical = previous_physical;
    result.selected_physical = selected_physical();
    result.physical_selection_changed =
        previous_physical != result.selected_physical;
    result.previous_window_start = previous_window_start;
    result.window_start = window_start_;
    result.mapping_changed =
        previous_window_start != window_start_;
    result.mapping_generation = mapping_generation_;
    return result;
}

TabViewportChange TabViewport::select_physical(
    std::size_t physical_slot) noexcept {
    std::size_t logical_index{};
    if (!logical_for_physical(physical_slot, logical_index)) {
        return unchanged_change(false);
    }

    const std::size_t previous_logical = selected_logical_;
    const std::size_t previous_physical = selected_physical();
    selected_logical_ = logical_index;

    TabViewportChange result{};
    result.valid = true;
    result.logical_selection_changed =
        previous_logical != selected_logical_;
    result.previous_logical = previous_logical;
    result.selected_logical = selected_logical_;
    result.previous_physical = previous_physical;
    result.selected_physical = physical_slot;
    result.physical_selection_changed =
        previous_physical != physical_slot;
    result.previous_window_start = window_start_;
    result.window_start = window_start_;
    result.mapping_generation = mapping_generation_;
    return result;
}

TabViewportChange TabViewport::select_physical_aligned(
    std::size_t physical_slot) noexcept {
    std::size_t logical_index{};
    if (!logical_for_physical(physical_slot, logical_index)) {
        return unchanged_change(false);
    }
    return select_logical(logical_index);
}

bool TabViewport::logical_for_physical(
    std::size_t physical_slot,
    std::size_t& output) const noexcept {
    if (!initialized() || physical_slot >= visible_slot_count()) return false;
    output = window_start_ + physical_slot;
    return true;
}

bool TabViewport::physical_for_logical(
    std::size_t logical_index,
    std::size_t& output) const noexcept {
    if (!initialized() || logical_index < window_start_ ||
        logical_index >= logical_count_) {
        return false;
    }
    const std::size_t offset = logical_index - window_start_;
    if (offset >= visible_slot_count()) return false;
    output = offset;
    return true;
}

bool TabViewport::initialized() const noexcept {
    return logical_count_ != 0 && physical_capacity_ != 0 &&
        selected_logical_ < logical_count_;
}

std::size_t TabViewport::logical_count() const noexcept {
    return logical_count_;
}

std::size_t TabViewport::physical_capacity() const noexcept {
    return physical_capacity_;
}

std::size_t TabViewport::visible_slot_count() const noexcept {
    return std::min(logical_count_, physical_capacity_);
}

std::size_t TabViewport::requested_navigation_margin() const noexcept {
    return requested_navigation_margin_;
}

std::size_t TabViewport::navigation_margin() const noexcept {
    return navigation_margin_;
}

std::size_t TabViewport::window_start() const noexcept {
    return window_start_;
}

std::size_t TabViewport::maximum_window_start() const noexcept {
    const std::size_t visible = visible_slot_count();
    return logical_count_ > visible ? logical_count_ - visible : 0;
}

std::size_t TabViewport::selected_logical() const noexcept {
    return selected_logical_;
}

std::size_t TabViewport::selected_physical() const noexcept {
    std::size_t result = invalid_tab_index;
    return physical_for_logical(selected_logical_, result)
        ? result
        : invalid_tab_index;
}

std::uint64_t TabViewport::mapping_generation() const noexcept {
    return mapping_generation_;
}

std::size_t TabViewport::aligned_window_start(
    std::size_t logical_index) const noexcept {
    const std::size_t visible = visible_slot_count();
    if (logical_count_ <= visible) return 0;

    if (logical_index < window_start_ ||
        logical_index - window_start_ < navigation_margin_) {
        return logical_index > navigation_margin_
            ? logical_index - navigation_margin_
            : 0;
    }

    const std::size_t right_anchor =
        visible - 1 - navigation_margin_;
    if (logical_index - window_start_ > right_anchor) {
        return std::min(
            logical_index - right_anchor,
            maximum_window_start());
    }
    return window_start_;
}

TabViewportChange TabViewport::unchanged_change(bool valid) const noexcept {
    TabViewportChange result{};
    result.valid = valid;
    result.previous_logical = selected_logical_;
    result.selected_logical = selected_logical_;
    result.previous_physical = selected_physical();
    result.selected_physical = result.previous_physical;
    result.previous_window_start = window_start_;
    result.window_start = window_start_;
    result.mapping_generation = mapping_generation_;
    return result;
}

void TabViewport::advance_mapping_generation() noexcept {
    ++mapping_generation_;
    // Keep zero as the never-initialized sentinel after unsigned wrap.
    if (mapping_generation_ == 0) ++mapping_generation_;
}

TabOverflowIndicator tab_overflow_indicator(
    const TabViewport& viewport,
    std::size_t physical_slot) noexcept {
    if (!viewport.initialized() ||
        physical_slot >= viewport.visible_slot_count() ||
        physical_slot == viewport.selected_physical()) {
        return TabOverflowIndicator::none;
    }

    const std::size_t visible = viewport.visible_slot_count();
    if (physical_slot == 0 && viewport.window_start() != 0) {
        return TabOverflowIndicator::previous;
    }
    if (physical_slot == visible - 1 &&
        viewport.logical_count() - viewport.window_start() > visible) {
        return TabOverflowIndicator::next;
    }
    return TabOverflowIndicator::none;
}

bool tab_overflow_indicators_changed(
    const TabViewport& before,
    const TabViewport& after) noexcept {
    if (before.initialized() != after.initialized()) return true;
    if (!before.initialized()) return false;
    if (before.visible_slot_count() != after.visible_slot_count()) return true;

    for (std::size_t physical = 0;
         physical < before.visible_slot_count();
         ++physical) {
        if (tab_overflow_indicator(before, physical) !=
            tab_overflow_indicator(after, physical)) {
            return true;
        }
    }
    return false;
}

} // namespace erui::detail
