#pragma once

#include "addresses.hpp"
#include "pagination.hpp"

#include <cstddef>
#include <cstdint>

namespace erui::detail {
struct CompiledRow;
}

namespace erui::native {

struct RowInjectionOutcome {
    erui::detail::PageRoute route{};
    std::size_t slice_index{};
    std::size_t slice_count{};
    std::size_t first_row{};
    std::size_t content_rows{};
    std::uint8_t root_capacity{};
    std::uint32_t attempted{};
    std::uint32_t added{};
    std::uint32_t navigation{};
    std::uint32_t faulted{};
    std::uint32_t skipped{};
};

RowInjectionOutcome inject_registered_rows(
    void* page,
    const GameAddresses& addresses) noexcept;
RowInjectionOutcome inject_page_route(
    void* page,
    erui::detail::PageRoute route,
    const GameAddresses& addresses) noexcept;

// Resolves only ERNativeUI-owned popup-selector state. The native list bridge
// uses this identity check to avoid touching vanilla Graphics selectors.
const erui::detail::CompiledRow* find_popup_choice_row_by_native_state(
    const void* state) noexcept;

} // namespace erui::native
