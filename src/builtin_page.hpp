#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace erui::detail {

// Stable ERNativeUI destinations. These values deliberately mirror the
// public ERUI_BUILTIN_PAGE_* identifiers, not Elden Ring's sparse native
// category IDs.
enum class BuiltinPage : std::uint8_t {
    game_options = 0,
    camera_options = 1,
    display = 2,
    sound = 3,
    network = 4,
    keyboard_mouse = 5,
    graphics = 6,
    count = 7,
};

inline constexpr std::size_t builtin_page_count =
    static_cast<std::size_t>(BuiltinPage::count);
inline constexpr std::size_t native_category_count = 10;
inline constexpr std::uint8_t invalid_native_category = 0xFFu;

inline constexpr std::array<std::uint8_t, builtin_page_count>
    builtin_native_category_ids{
        0u, // Game Options
        1u, // Camera Options
        2u, // Display
        3u, // Sound
        5u, // Network
        7u, // Keyboard/Mouse Settings
        8u, // Graphics
    };

[[nodiscard]] constexpr bool valid_builtin_page(
    BuiltinPage page) noexcept {
    return static_cast<std::size_t>(page) < builtin_page_count;
}

[[nodiscard]] constexpr std::size_t builtin_page_offset(
    BuiltinPage page) noexcept {
    return static_cast<std::size_t>(page);
}

[[nodiscard]] constexpr std::uint8_t native_category_id(
    BuiltinPage page) noexcept {
    return valid_builtin_page(page)
        ? builtin_native_category_ids[builtin_page_offset(page)]
        : invalid_native_category;
}

} // namespace erui::detail
