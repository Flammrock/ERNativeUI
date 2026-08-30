#pragma once

#include "addresses.hpp"

#include <cstdint>

namespace erui::native {

enum class HookInstallStatus : std::uint8_t {
    success,
    hook_failed,
};

HookInstallStatus install_native_menu_hooks(const GameAddresses& addresses) noexcept;
void remove_native_menu_hooks() noexcept;

} // namespace erui::native
