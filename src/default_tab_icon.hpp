#pragma once

#include "menu.hpp"

#include <ernativeui/erui.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace erui::native {

[[nodiscard]] constexpr bool custom_tab_icon_requests_raster(
    TabIconSource source) noexcept {
    return source == TabIconSource::fallback ||
        source == TabIconSource::rgba8;
}

using DefaultTabIconPixels =
    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT>;

// The host's iconless-tab artwork. This is evaluated at compile time and the
// resulting immutable RGBA8 array is shared by every fallback tab; no client
// registration or repaint regenerates or copies it into the menu catalogue.
[[nodiscard]] consteval DefaultTabIconPixels
make_default_custom_tab_icon_rgba8() noexcept {
    constexpr std::uint8_t red = 107;
    constexpr std::uint8_t green = 100;
    constexpr std::uint8_t blue = 80;
    constexpr std::size_t top = 4;
    constexpr std::size_t bottom = 27;

    DefaultTabIconPixels pixels{};
    for (std::size_t y = 0; y < ERUI_TAB_ICON_HEIGHT; ++y) {
        if (y < top || y > bottom) continue;

        const int maximum_distance = static_cast<int>(y - top) + 1;
        for (std::size_t x = 0; x < ERUI_TAB_ICON_WIDTH; ++x) {
            const int from_centre = static_cast<int>(x * 2U + 1U) - 32;
            const int distance =
                from_centre < 0 ? -from_centre : from_centre;
            if (distance > maximum_distance) continue;

            const bool boundary =
                distance + 2 > maximum_distance || y == bottom;
            const std::uint8_t opacity = boundary ? 160 : 255;
            const std::size_t offset =
                (y * ERUI_TAB_ICON_WIDTH + x) * 4U;
            pixels[offset + 0U] = red;
            pixels[offset + 1U] = green;
            pixels[offset + 2U] = blue;
            pixels[offset + 3U] = opacity;
        }
    }
    return pixels;
}

inline constexpr DefaultTabIconPixels default_custom_tab_icon_rgba8 =
    make_default_custom_tab_icon_rgba8();

} // namespace erui::native
