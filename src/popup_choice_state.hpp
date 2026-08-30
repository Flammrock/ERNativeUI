#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace erui::detail {

inline constexpr std::size_t popup_choice_max_options = 32;
inline constexpr std::size_t popup_choice_selected_offset = 1;

// Elden Ring's popup selector stores its committed item ID at byte +1. Native
// IDs are one-based; ERNativeUI's model and public API remain zero-based.
struct PopupChoiceNativeState {
    std::uint8_t reserved0{};
    volatile std::uint8_t selected{};
    std::array<std::uint8_t, 14> reserved{};

    [[nodiscard]] void* address() noexcept { return this; }
    [[nodiscard]] const void* address() const noexcept { return this; }

    // Elden Ring and ERNativeUI access this byte from different native
    // threads. Aligned byte loads/stores are atomic on Windows x64; volatile
    // prevents either compiler access from being cached or elided.
    [[nodiscard]] volatile std::uint8_t* native_selection_address() noexcept {
        return &selected;
    }

    [[nodiscard]] std::uint8_t native_selection() const noexcept {
        return selected;
    }

    void set_public_selection(std::uint8_t selected_index) noexcept {
        selected =
            static_cast<std::uint8_t>(selected_index + 1u);
    }
};

static_assert(std::is_standard_layout_v<PopupChoiceNativeState>);
static_assert(offsetof(PopupChoiceNativeState, selected) ==
    popup_choice_selected_offset);
static_assert(sizeof(PopupChoiceNativeState) == 16);

inline std::uint8_t synchronize_popup_choice_state(
    volatile std::uint8_t& public_selection,
    PopupChoiceNativeState& native_state,
    std::uint8_t last_observed_selection,
    std::size_t option_count) noexcept {
    if (option_count == 0 || option_count > popup_choice_max_options) {
        public_selection = 0;
        native_state.set_public_selection(0);
        return 0;
    }

    std::uint8_t public_value = public_selection;
    if (public_value >= option_count) {
        public_value = last_observed_selection < option_count
            ? last_observed_selection
            : 0;
        public_selection = public_value;
    }

    // A public write (including set_row_value) wins if it occurred since the
    // last poll. This also repairs the native state without generating a
    // second, synthetic change on the next tick.
    if (public_value != last_observed_selection) {
        native_state.set_public_selection(public_value);
        return public_value;
    }

    const std::uint8_t native_value = native_state.native_selection();
    if (native_value >= 1 && native_value <= option_count) {
        const std::uint8_t selected_index =
            static_cast<std::uint8_t>(native_value - 1u);
        public_selection = selected_index;
        return selected_index;
    }

    // A malformed native byte must not escape through the public API.
    native_state.set_public_selection(public_value);
    return public_value;
}

} // namespace erui::detail
