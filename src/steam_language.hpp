#pragma once

#include <ernativeui/erui.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace erui::host {

enum class SteamLanguageFailure : std::uint8_t {
    none,
    module_not_loaded,
    exports_unavailable,
    apps_not_initialized,
    current_identifier_invalid,
    readiness_timeout,
    exception,
};

struct SteamLanguageProbe {
    bool available{};
    std::string current{};
    SteamLanguageFailure failure_code{SteamLanguageFailure::none};
    const char* failure{};
};

enum class SteamLanguageReadiness : std::uint8_t {
    pending,
    ready,
    unavailable,
};

enum class SteamLanguageProbeDecision : std::uint8_t {
    retry,
    settle,
    timeout,
};

// Performs ordinary, read-only calls through the Steamworks flat C API.
// No export or game function is hooked by this probe.
[[nodiscard]] SteamLanguageProbe probe_steam_language() noexcept;
[[nodiscard]] bool is_retryable_steam_language_failure(
    SteamLanguageFailure failure) noexcept;
[[nodiscard]] SteamLanguageProbeDecision decide_steam_language_probe(
    const SteamLanguageProbe& probe,
    std::uint64_t elapsed_ms,
    std::uint64_t wait_limit_ms) noexcept;
[[nodiscard]] ERUI_GameLanguage classify_steam_language(
    std::string_view identifier) noexcept;

/*
 * Publishes exactly one process-wide language result. A successful probe
 * releases API 1.1 clients with a stable language snapshot. A terminal
 * failure releases language callers with ERUI_NOT_SUPPORTED while leaving
 * both menu API versions usable. Retryable failures leave the state pending
 * and return ERUI_HOST_NOT_READY.
 */
ERUI_Result settle_cached_steam_language(
    SteamLanguageProbe discovered) noexcept;
[[nodiscard]] SteamLanguageReadiness steam_language_readiness() noexcept;

/*
 * Waits for the host worker to publish either the cached language or terminal
 * unavailability. This lets frozen API 1.0 clients obtain their table early
 * without racing Steam initialization in get_game_language.
 */
ERUI_Result get_cached_steam_language(
    ERUI_GameLanguageInfo* out_language) noexcept;

} // namespace erui::host
