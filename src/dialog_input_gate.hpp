#pragma once

#include <cstdint>

namespace erui::native {

// Native action 3 is overloaded: menu pages use it for Back, while a
// two-button generic dialog uses it for its right/secondary action. Only the
// latter is dispatched from inside CSPopupMenu's owned update.
[[nodiscard]] constexpr bool should_suppress_native_back(
    bool owns_dialog,
    bool dispatching_popup_input,
    std::uint64_t action) noexcept {
    constexpr std::uint64_t kNativeBackAction = 3;
    return owns_dialog && !dispatching_popup_input &&
        action == kNativeBackAction;
}

} // namespace erui::native
