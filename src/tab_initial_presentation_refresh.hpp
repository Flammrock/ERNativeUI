#pragma once

#include "tab_viewport.hpp"

#include <cstddef>
#include <cstdint>

namespace erui::detail {

// An overflow marker replaces the mapped tab's real icon. Only a custom tab
// whose mapped cell is not an overflow marker needs a follow-up presentation
// pass when its raster resource becomes available after the initial paint.
[[nodiscard]] inline bool tab_viewport_exposes_custom_icon(
    const TabViewport& viewport,
    std::size_t first_custom_logical) noexcept {
    if (!viewport.initialized()) return false;

    for (std::size_t physical = 0;
         physical < viewport.visible_slot_count();
         ++physical) {
        if (tab_overflow_indicator(viewport, physical) !=
            TabOverflowIndicator::none) {
            continue;
        }

        std::size_t logical{invalid_tab_index};
        if (viewport.logical_for_physical(physical, logical) &&
            logical >= first_custom_logical) {
            return true;
        }
    }
    return false;
}

enum class DeferredTabPresentationRefreshPoll : std::uint8_t {
    idle,
    waiting,
    ready,
    stale,
};

// UI-thread-only one-shot token for a post-initialization tab repaint. The
// presentation epoch prevents a request from crossing a Configuration-menu
// lifetime, while the mapping generation prevents it from repainting a strip
// that an earlier input callback has already remapped. A request becomes ready
// only after the composite frame on which it was made has completed.
class DeferredTabPresentationRefresh final {
public:
    [[nodiscard]] bool request_if_needed(
        const TabViewport& viewport,
        std::size_t first_custom_logical,
        std::uint64_t presentation_epoch,
        std::uint64_t current_frame) noexcept {
        reset();
        if (presentation_epoch == 0 ||
            viewport.mapping_generation() == 0 ||
            !tab_viewport_exposes_custom_icon(
                viewport, first_custom_logical)) {
            return false;
        }

        pending_ = true;
        presentation_epoch_ = presentation_epoch;
        mapping_generation_ = viewport.mapping_generation();
        requested_frame_ = current_frame;
        return true;
    }

    [[nodiscard]] DeferredTabPresentationRefreshPoll poll(
        std::uint64_t presentation_epoch,
        std::uint64_t mapping_generation,
        std::uint64_t current_frame) noexcept {
        if (!pending_) {
            return DeferredTabPresentationRefreshPoll::idle;
        }
        if (presentation_epoch != presentation_epoch_ ||
            mapping_generation != mapping_generation_) {
            reset();
            return DeferredTabPresentationRefreshPoll::stale;
        }
        if (current_frame == requested_frame_) {
            return DeferredTabPresentationRefreshPoll::waiting;
        }

        return DeferredTabPresentationRefreshPoll::ready;
    }

    // A ready request remains owned by its caller until the caller has either
    // armed the native presentation transaction or proved that no mutation is
    // necessary. This lets transient movie/selector validation failures retry
    // without losing the one-shot request.
    void consume() noexcept {
        reset();
    }

    void reset() noexcept {
        pending_ = false;
        presentation_epoch_ = 0;
        mapping_generation_ = 0;
        requested_frame_ = 0;
    }

    [[nodiscard]] bool pending() const noexcept {
        return pending_;
    }

private:
    bool pending_{};
    std::uint64_t presentation_epoch_{};
    std::uint64_t mapping_generation_{};
    std::uint64_t requested_frame_{};
};

} // namespace erui::detail
