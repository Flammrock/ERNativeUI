#pragma once

#include <ernativeui/erui.h>

#include <string>
#include <string_view>

namespace erui::host {

struct SteamLanguageProbe {
    bool available{};
    std::string current{};
    std::string supported{};
    const char* failure{};
};

// Performs ordinary, read-only calls through the Steamworks flat C API.
// No export or game function is hooked by this probe.
[[nodiscard]] SteamLanguageProbe probe_steam_language() noexcept;
[[nodiscard]] ERUI_GameLanguage classify_steam_language(
    std::string_view identifier) noexcept;
ERUI_Result get_cached_steam_language(
    ERUI_GameLanguageInfo* out_language) noexcept;

} // namespace erui::host
