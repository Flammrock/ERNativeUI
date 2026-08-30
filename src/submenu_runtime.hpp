#pragma once

#include "pagination.hpp"

namespace erui::native {

enum class NavigationRequestKind : unsigned char {
    submenu,
    pagination_next,
    pagination_previous,
};

// Called by the std::function retained by Elden Ring's native button row.
// parent_page_slot points inside that retained callback object, matching the
// pointer-to-page contract used by openSubPage.
void request_navigate_page(
    void** parent_page_slot,
    erui::detail::PageRoute target,
    NavigationRequestKind kind) noexcept;

} // namespace erui::native
