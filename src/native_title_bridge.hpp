#pragma once

#include "pagination.hpp"

#include <cstddef>

namespace erui::native {

// Confirmed against the native Game Options page constructor. Its persistent
// path result occupies page offsets [0x230, 0x290). Native temporary call sites
// use the same layout, pass result+0x08 to the text setter, and destroy the
// nested member at result+0x28 before releasing their stack storage.
inline constexpr std::size_t scaleform_path_result_size = 0x60;
inline constexpr std::size_t scaleform_text_value_offset = 0x08;
inline constexpr std::size_t scaleform_destructor_member_offset = 0x28;

inline constexpr char native_menu_title_source_path[] = "MenuTitle/Text_0";
inline constexpr char custom_outer_title_path[] =
    "MenuTitle/StaticText_101003";
inline constexpr char custom_page_title_path[] =
    "GraphicOption/StaticText_111114";

struct CapturedTitleTarget {
    erui::detail::PageRoute route{
        .logical_page_index = erui::detail::invalid_route_index,
    };
    void* movie_context{};
    void* persistent_result{};

    [[nodiscard]] bool valid() const noexcept {
        return route.valid() && route.kind != erui::detail::PageRouteKind::root_main &&
            movie_context && persistent_result;
    }

    [[nodiscard]] void* text_value() const noexcept {
        return valid()
            ? static_cast<void*>(
                static_cast<std::byte*>(persistent_result) +
                scaleform_text_value_offset)
            : nullptr;
    }
};

// The native submenu construction and handler callbacks run serially on the
// UI thread, just like SubmenuNavigation. Keeping the capture as simple state
// avoids allocation and synchronization from the hot Scaleform resolver hook.
class TitleCaptureState {
public:
    void reset() noexcept { capture_ = {}; }

    [[nodiscard]] const char* observe(
        const char* path,
        erui::detail::PageRoute pending_route,
        void* movie_context,
        void* persistent_result) noexcept {
        if (!path || !pending_route.valid() ||
            pending_route.kind == erui::detail::PageRouteKind::root_main ||
            !movie_context || !persistent_result || !is_source_path(path)) {
            return nullptr;
        }

        capture_ = {
            .route = pending_route,
            .movie_context = movie_context,
            .persistent_result = persistent_result,
        };
        return custom_page_title_path;
    }

    [[nodiscard]] CapturedTitleTarget consume(
        const erui::detail::PageRoute& expected_route) noexcept {
        const CapturedTitleTarget captured = capture_;
        reset();
        return captured.valid() && captured.route == expected_route
            ? captured
            : CapturedTitleTarget{};
    }

private:
    [[nodiscard]] static bool is_source_path(const char* path) noexcept {
        for (std::size_t index = 0;
             index < sizeof(native_menu_title_source_path); ++index) {
            if (path[index] != native_menu_title_source_path[index]) {
                return false;
            }
        }
        return true;
    }

    CapturedTitleTarget capture_{};
};

} // namespace erui::native
