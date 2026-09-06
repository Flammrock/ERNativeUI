#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace erui::detail {

inline constexpr std::size_t invalid_route_index = static_cast<std::size_t>(-1);
inline constexpr std::uint8_t invalid_builtin_category = 0xFF;
inline constexpr std::size_t native_subpage_capacity = 15;
// Built-in OptionSetting panels are accepted only when their live visual
// capacity is in [1, 32]. The remaining first-page capacity can therefore use
// the same closed bound when immutable continuation metadata is compiled.
inline constexpr std::uint8_t builtin_page_max_first_capacity = 32;
inline constexpr std::size_t game_options_vanilla_row_count = 4;
inline constexpr std::uint8_t game_options_vanilla_capacity = 6;
inline constexpr std::uint8_t game_options_max_visual_capacity = 13;
inline constexpr std::size_t game_options_capacity_count =
    game_options_max_visual_capacity - game_options_vanilla_capacity + 1;

enum class PageRouteKind : std::uint8_t {
    submenu,
    root_continuation,
    root_main,
    builtin_continuation,
    builtin_main,
};

// Identifies one physical slice of a logical ERNativeUI page. root_main is a
// navigation-only target representing Elden Ring's existing Game Options
// page; it is never bound to the native subpage handler.
struct PageRoute {
    PageRouteKind kind{PageRouteKind::submenu};
    std::uint8_t root_capacity{};
    std::uint8_t builtin_category{invalid_builtin_category};
    std::uint8_t reserved{};
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

    [[nodiscard]] static constexpr PageRoute builtin_continuation(
        std::size_t logical_page_index,
        std::uint8_t native_category,
        std::uint8_t first_page_capacity,
        std::size_t slice_index) noexcept {
        return {
            .kind = PageRouteKind::builtin_continuation,
            .root_capacity = first_page_capacity,
            .builtin_category = native_category,
            .logical_page_index = logical_page_index,
            .slice_index = slice_index,
        };
    }

    [[nodiscard]] static constexpr PageRoute builtin_main(
        std::size_t logical_page_index,
        std::uint8_t native_category,
        std::uint8_t first_page_capacity) noexcept {
        return {
            .kind = PageRouteKind::builtin_main,
            .root_capacity = first_page_capacity,
            .builtin_category = native_category,
            .logical_page_index = logical_page_index,
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
    std::uint8_t native_capacity{game_options_vanilla_capacity};
    std::size_t vanilla_row_count{game_options_vanilla_row_count};
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
    std::size_t vanilla_row_count = game_options_vanilla_row_count,
    std::size_t continuation_capacity = native_subpage_capacity);

// Resolves one slice of a page whose first physical page has a live runtime
// capacity. This allocation-free form is used by Elden Ring's built-in pages,
// where the available slots are known only after the native materializer has
// constructed its vanilla rows. A first-page capacity of one is valid: when
// content overflows, that sole slot is occupied by Next and slice zero has no
// content rows. Zero capacity fails closed.
[[nodiscard]] bool resolve_paginated_slice(
    std::size_t logical_page_index,
    std::size_t logical_row_count,
    std::size_t first_page_capacity,
    std::size_t continuation_capacity,
    std::size_t slice_index,
    PageSlice& output) noexcept;

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
    if (left.kind == PageRouteKind::builtin_continuation) {
        return left.root_capacity == right.root_capacity &&
            left.builtin_category == right.builtin_category;
    }
    return left.kind == PageRouteKind::submenu;
}

// A Previous row must pop exactly one native page. The first root
// continuation returns to the existing Game Options page; all other
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
    if (current.kind == PageRouteKind::builtin_continuation &&
        current.slice_index == 1) {
        return target.kind == PageRouteKind::builtin_main &&
            target.logical_page_index == current.logical_page_index &&
            target.root_capacity == current.root_capacity &&
            target.builtin_category == current.builtin_category &&
            target.slice_index == 0;
    }
    return same_native_page_domain(current, target) &&
        target.slice_index + 1 == current.slice_index;
}

} // namespace erui::detail
