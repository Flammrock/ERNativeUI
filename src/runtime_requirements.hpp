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

// Ordinary API 1.0-1.3 rows do not require the API 1.4 presentation pump.
// Gate it with the backend capability frozen before registration opens so an
// older supported executable can keep serving earlier clients without being
// forced through a newer build-locked presenter.
[[nodiscard]] constexpr bool runtime_requires_row_presentations(
    bool enable_row_injection,
    bool updates_backend_available,
    std::size_t editable_row_count) noexcept {
    return enable_row_injection && updates_backend_available &&
        editable_row_count != 0;
}

// The host freezes capability publication before client registration opens.
// A withheld visibility capability is an intentional reduced-mode install,
// not an error. Once advertised, however, both the exact-build identity and
// every native visibility interface must still validate immediately before
// hook installation.
[[nodiscard]] constexpr bool runtime_row_visibility_backend_valid(
    bool row_visibility_backend_available,
    bool expected_build,
    bool native_interfaces_supported) noexcept {
    return !row_visibility_backend_available ||
        (expected_build && native_interfaces_supported);
}

} // namespace erui::detail
