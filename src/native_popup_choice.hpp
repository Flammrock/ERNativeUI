#pragma once

#include "addresses.hpp"

#include <cstddef>

namespace erui::native {

inline constexpr std::size_t popup_choice_list_size = 0x920;
inline constexpr std::size_t popup_choice_element_offset = 0x08;
inline constexpr std::size_t popup_choice_element_stride = 0x48;
inline constexpr std::size_t popup_choice_element_text_offset = 0x10;
inline constexpr std::size_t popup_choice_list_count_offset = 0x910;

bool install_native_popup_choice_bridge(
    const GameAddresses& addresses) noexcept;
void remove_native_popup_choice_bridge() noexcept;

} // namespace erui::native
