#pragma once

#include <cstdint>

namespace erui::detail {
class NativeTabCatalogue;
}

namespace erui::native {

// API 1.3 custom tabs are executable-build locked. Preflight is read-only and
// is intentionally performed before provider registration opens, so the
// advertised capability cannot change during a registration epoch.
enum class NativeTabsPreflightStatus : std::uint8_t {
    available,
    unsupported_game,
    interface_mismatch,
};

[[nodiscard]] NativeTabsPreflightStatus preflight_native_tabs() noexcept;

// Installs the viewport, Graphics carrier coordinator, mouse dwell adapter,
// and retained RGBA8 icon bridge for one immutable catalogue. The ordinary
// menu hooks (including the Graphics materializer detour) must already be
// installed before this function is called.
[[nodiscard]] bool install_native_tabs(
    const detail::NativeTabCatalogue& catalogue) noexcept;

// A successfully installed tab backend is process-lifetime because callbacks
// can begin on the game thread without a teardown/drain handshake. The host
// DLL is pinned and hot unloading is unsupported. Returns true only when no
// tab backend was installed; otherwise leaves the complete runtime intact and
// returns false.
[[nodiscard]] bool remove_native_tabs() noexcept;

[[nodiscard]] bool native_tabs_installed() noexcept;

} // namespace erui::native
