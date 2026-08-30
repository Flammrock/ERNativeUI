#pragma once

#include "menu.hpp"

#include <filesystem>
#include <string_view>

namespace erui::host {

struct HostLocale {
    erui::MenuLocalization pagination{};
    bool loaded{};
};

[[nodiscard]] HostLocale load_host_locale(
    const std::filesystem::path& dll_directory,
    std::string_view steam_identifier) noexcept;

} // namespace erui::host
