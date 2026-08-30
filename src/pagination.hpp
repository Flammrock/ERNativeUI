#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace erui::detail {

inline constexpr std::size_t invalid_route_index = static_cast<std::size_t>(-1);
inline constexpr std::size_t native_subpage_capacity = 15;
inline constexpr std::size_t controller_vanilla_row_count = 4;
inline constexpr std::uint8_t controller_vanilla_capacity = 6;
inline constexpr std::uint8_t controller_max_visual_capacity = 13;
inline constexpr std::size_t controller_capacity_count =
    controller_max_visual_capacity - controller_vanilla_capacity + 1;

enum class PageRouteKind : std::uint8_t {
    submenu,
    root_continuation,
    root_main,
};

// Identifies one physical slice of a logical ERNativeUI page. root_main is a
// navigation-only target representing Elden Ring's existing Controller
// Settings page; it is never bound to the native subpage handler.
struct PageRoute {
    PageRouteKind kind{PageRouteKind::submenu};
    std::uint8_t root_capacity{};
    std::uint16_t reserved{};
    std::size_t logical_page_index{invalid_route_index};
    std::size_t slice_index{};

    [[nodiscard]] static constexpr PageRoute submenu(
        std::size_t logical_page_index,
        std::size_t slice_index = 0) noexcept {
        return {
            .kind = PageRouteKind::submenu,
            .logical_page_index = logical_page_index,
            .slice_index = slice_index,
        };
    }

    [[nodiscard]] static constexpr PageRoute root_continuation(
        std::size_t root_page_index,
        std::uint8_t capacity,
        std::size_t slice_index) noexcept {
        return {
            .kind = PageRouteKind::root_continuation,
            .root_capacity = capacity,
            .logical_page_index = root_page_index,
            .slice_index = slice_index,
        };
    }

    [[nodiscard]] static constexpr PageRoute root_main(
        std::size_t root_page_index,
        std::uint8_t capacity) noexcept {
        return {
            .kind = PageRouteKind::root_main,
            .root_capacity = capacity,
            .logical_page_index = root_page_index,
            .slice_index = 0,
        };
    }

    [[nodiscard]] constexpr bool valid() const noexcept {
        return logical_page_index != invalid_route_index;
    }

    friend constexpr bool operator==(
        const PageRoute& left,
        const PageRoute& right) noexcept = default;
};

struct PageSlice {
    std::size_t logical_page_index{invalid_route_index};
    std::size_t slice_index{};
    std::size_t slice_count{};
    std::size_t first_row{};
    std::size_t row_count{};
    bool has_previous{};
    bool has_next{};
};

struct PagePlan {
    std::size_t logical_page_index{invalid_route_index};
    std::size_t physical_capacity{};
    std::vector<PageSlice> slices{};

    [[nodiscard]] const PageSlice* slice(std::size_t index) const noexcept {
        return index < slices.size() ? &slices[index] : nullptr;
    }
};

struct RootPagePlan {
    std::uint8_t native_capacity{controller_vanilla_capacity};
    std::size_t vanilla_row_count{controller_vanilla_row_count};
    std::size_t custom_capacity{};
    PagePlan pages{};

    [[nodiscard]] const PageSlice* slice(std::size_t index) const noexcept {
        return pages.slice(index);
    }
};

[[nodiscard]] PagePlan paginate_subpage(
    std::size_t logical_page_index,
    std::size_t logical_row_count,
    std::size_t physical_capacity = native_subpage_capacity);

[[nodiscard]] RootPagePlan paginate_root_page(
    std::size_t root_page_index,
    std::size_t logical_row_count,
    std::uint8_t native_capacity,
    std::size_t vanilla_row_count = controller_vanilla_row_count,
    std::size_t continuation_capacity = native_subpage_capacity);

[[nodiscard]] constexpr bool same_native_page_domain(
    const PageRoute& left,
    const PageRoute& right) noexcept {
    if (left.kind != right.kind) {
        return false;
    }
    if (left.logical_page_index != right.logical_page_index) {
        return false;
    }
    if (left.kind == PageRouteKind::root_continuation) {
        return left.root_capacity == right.root_capacity;
    }
    return left.kind == PageRouteKind::submenu;
}

// A Previous row must pop exactly one native page. The first root
// continuation returns to the existing Controller Settings page; all other
// routes return to the preceding slice in the same native-page domain.
[[nodiscard]] constexpr bool is_previous_route_target(
    const PageRoute& current,
    const PageRoute& target) noexcept {
    if (!current.valid() || !target.valid() || current.slice_index == 0) {
        return false;
    }
    if (current.kind == PageRouteKind::root_continuation &&
        current.slice_index == 1) {
        return target.kind == PageRouteKind::root_main &&
            target.logical_page_index == current.logical_page_index &&
            target.root_capacity == current.root_capacity &&
            target.slice_index == 0;
    }
    return same_native_page_domain(current, target) &&
        target.slice_index + 1 == current.slice_index;
}

} // namespace erui::detail
