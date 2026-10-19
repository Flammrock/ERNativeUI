#pragma once

#include <cstdint>

namespace erui::native {

class ModuleView;

// Runtime GFX completion is optional. The host remains usable when the game
// build or native file interface is unknown; bundled/third-party GFX files
// then pass through unchanged and the existing native fallbacks still apply.
enum class NativeGfxLoaderInstallStatus : std::uint8_t {
    installed,
    already_installed,
    unsupported_game,
    interface_mismatch,
    allocation_failed,
    hook_failed,
};

[[nodiscard]] NativeGfxLoaderInstallStatus install_native_gfx_loader(
    const ModuleView& game,
    std::uint16_t minimum_tab_slots = 8) noexcept;

[[nodiscard]] const char* native_gfx_loader_install_status_name(
    NativeGfxLoaderInstallStatus status) noexcept;

} // namespace erui::native
