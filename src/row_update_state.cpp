#include "row_update_state.hpp"

#include "native_slider_presentation.hpp"
#include "row_presentation_state.hpp"
#include "row_visibility_state.hpp"
#include "submenu_update_state.hpp"

namespace erui::detail {
namespace {

std::mutex g_publication_mutex{};

} // namespace

RowUpdatePublicationLock lock_row_update_publication() noexcept {
    return RowUpdatePublicationLock{g_publication_mutex};
}

void synchronize_pending_row_updates() noexcept {
    const RowUpdatePublicationLock lock = lock_row_update_publication();
    synchronize_pending_row_presentations();
    synchronize_pending_submenu_updates();
    erui::native::synchronize_pending_native_slider_presentations();
    (void)synchronize_pending_row_visibility();
}

bool synchronize_pending_row_updates_if_visibility_idle() noexcept {
    const RowUpdatePublicationLock lock = lock_row_update_publication();
    if (row_visibility_updates_pending()) return false;
    synchronize_pending_row_presentations();
    synchronize_pending_submenu_updates();
    erui::native::synchronize_pending_native_slider_presentations();
    return true;
}

} // namespace erui::detail
