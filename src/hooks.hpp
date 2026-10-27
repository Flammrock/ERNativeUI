#pragma once

#include "addresses.hpp"

#include <cstddef>
#include <cstdint>

namespace erui::detail {
struct PageRoute;
}

namespace erui::native {

struct CapturedTitleTarget;

enum class HookInstallStatus : std::uint8_t {
    success,
    hook_failed,
};

HookInstallStatus install_native_menu_hooks(
    const GameAddresses& addresses,
    const PreservedSharedMenuAddresses* preserved_shared = nullptr,
    bool enable_row_presentations = false) noexcept;
// Read-only validation for the exact native Graphics carrier contract used by
// custom tabs. This never creates hooks or mutates runtime state.
bool preflight_native_tab_carrier(const GameAddresses& addresses) noexcept;

// Post-install health check for the process-owned Graphics carrier hooks.
// Unlike preflight_native_tab_carrier(), this does not compare original game
// bytes: those entry points are expected to have been patched already.
bool native_tab_graphics_carrier_installed() noexcept;

// Read-only identity proof used by the API 1.4 in-place row rebuild. It is
// true only for the currently committed custom-tab category-8 carrier and
// never accepts the native Graphics route or an aliased panel.
bool validate_native_row_visibility_tab_carrier(
    void* page,
    std::size_t logical_page_index) noexcept;

// Reapplies the already-captured, page-owned submenu title target after an
// in-place visibility rebuild. Main root/built-in/tab pages deliberately have
// no captured target: their native heading is unchanged by slice-count edits.
void reapply_native_row_visibility_page_title(
    void* page,
    const CapturedTitleTarget& captured,
    const erui::detail::PageRoute& route,
    std::size_t slice_count) noexcept;

// Exact-target health check for the shared Scaleform path bridge owned by the
// menu-hook layer. The native-tab installer runs after ordinary menu hooks,
// so this one executable entry may legitimately contain our own patch instead
// of the pristine game bytes validated during startup preflight.
bool native_scaleform_path_bridge_installed_at(
    const void* target) noexcept;

// True from the first attempt to expose either carrier hook. Once exposed,
// the menu/runtime storage that owns their trampolines is process-lifetime:
// there is no game-thread quiescence protocol that makes hot removal safe.
bool native_tab_graphics_carrier_exposed() noexcept;

// Monotonically advances only after the native composite teardown proves that
// category 8 was destroyed exactly. A viewport fault may be cleared for a new
// Configuration epoch only after observing this boundary.
std::uint64_t native_tab_graphics_carrier_teardown_generation() noexcept;

// Private in-process bridge between the native-tab viewport and the ordinary
// menu-hook owner. These functions are deliberately C++ symbols: production
// clients negotiate only ERUI_GetApi and must not depend on research exports.
using NativeTabParkingSwitchFn = bool(__cdecl*)(void*, std::int32_t);
std::uint32_t native_tab_graphics_carrier_version() noexcept;
bool prepare_native_tab_graphics_carrier(
    void* composite,
    std::size_t route,
    std::uint64_t completed_frame,
    std::int32_t* category,
    NativeTabParkingSwitchFn park_native) noexcept;
bool verify_native_tab_graphics_carrier(
    void* composite,
    std::size_t route,
    std::int32_t category) noexcept;

void remove_native_menu_hooks() noexcept;

} // namespace erui::native
