#pragma once

#include <cstddef>

namespace erui::detail {

inline constexpr std::size_t research_tab_main_capacity = 2;

// Pagination continuations are native subpages regardless of whether their
// logical owner is the Game Options root, a built-in page, or a custom tab.
// Keep this decision independent from address resolution so the custom-tab-
// only case remains directly testable.
[[nodiscard]] constexpr bool pagination_requires_submenus(
    bool enable_row_injection,
    bool active_pagination,
    bool minimum_root_pagination,
    bool runtime_capacity_pagination,
    bool research_tab_capacity_pagination = false) noexcept {
    return enable_row_injection &&
        (active_pagination || minimum_root_pagination ||
            runtime_capacity_pagination || research_tab_capacity_pagination);
}

} // namespace erui::detail
