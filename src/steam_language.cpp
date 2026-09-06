#include "steam_language.hpp"

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <utility>

namespace erui::host {
namespace {

using SteamAppsAccessor = void* (__cdecl*)();
using SteamAppsStringQuery = const char* (__cdecl*)(void* steam_apps);
using SteamHandleQuery = std::int32_t (__cdecl*)();

constexpr std::size_t kMaximumSteamLanguageText = 4096;
std::mutex g_language_mutex{};
std::condition_variable g_language_changed{};
SteamLanguageProbe g_cached_language{};
std::atomic<SteamLanguageReadiness> g_language_readiness{
    SteamLanguageReadiness::pending};

bool copy_bounded_text(const char* source, std::string& destination) {
    if (!source) return false;
    const std::size_t length = strnlen_s(source, kMaximumSteamLanguageText);
    if (length == 0 || length == kMaximumSteamLanguageText) return false;
    destination.assign(source, length);
    return true;
}

template <typename Function>
Function load_export(HMODULE module, const char* name) noexcept {
    static_assert(sizeof(Function) == sizeof(FARPROC));
    const FARPROC address = GetProcAddress(module, name);
    Function function{};
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

} // namespace

bool is_retryable_steam_language_failure(
    SteamLanguageFailure failure) noexcept {
    switch (failure) {
    case SteamLanguageFailure::module_not_loaded:
    case SteamLanguageFailure::apps_not_initialized:
    case SteamLanguageFailure::current_identifier_invalid:
        return true;
    case SteamLanguageFailure::none:
    case SteamLanguageFailure::exports_unavailable:
    case SteamLanguageFailure::readiness_timeout:
    case SteamLanguageFailure::exception:
        return false;
    }
    return false;
}

SteamLanguageProbeDecision decide_steam_language_probe(
    const SteamLanguageProbe& probe,
    std::uint64_t elapsed_ms,
    std::uint64_t wait_limit_ms) noexcept {
    if (probe.available ||
        !is_retryable_steam_language_failure(probe.failure_code)) {
        return SteamLanguageProbeDecision::settle;
    }
    return elapsed_ms >= wait_limit_ms
        ? SteamLanguageProbeDecision::timeout
        : SteamLanguageProbeDecision::retry;
}

ERUI_GameLanguage classify_steam_language(
    std::string_view identifier) noexcept {
    if (identifier == "english") return ERUI_GAME_LANGUAGE_ENGLISH;
    if (identifier == "german") return ERUI_GAME_LANGUAGE_GERMAN;
    if (identifier == "french") return ERUI_GAME_LANGUAGE_FRENCH;
    if (identifier == "italian") return ERUI_GAME_LANGUAGE_ITALIAN;
    if (identifier == "koreana") return ERUI_GAME_LANGUAGE_KOREAN;
    if (identifier == "spanish") return ERUI_GAME_LANGUAGE_SPANISH;
    if (identifier == "schinese") return ERUI_GAME_LANGUAGE_CHINESE_SIMPLIFIED;
    if (identifier == "tchinese") return ERUI_GAME_LANGUAGE_CHINESE_TRADITIONAL;
    if (identifier == "russian") return ERUI_GAME_LANGUAGE_RUSSIAN;
    if (identifier == "thai") return ERUI_GAME_LANGUAGE_THAI;
    if (identifier == "japanese") return ERUI_GAME_LANGUAGE_JAPANESE;
    if (identifier == "polish") return ERUI_GAME_LANGUAGE_POLISH;
    if (identifier == "arabic") return ERUI_GAME_LANGUAGE_ARABIC;
    if (identifier == "brazilian") return ERUI_GAME_LANGUAGE_PORTUGUESE_BRAZIL;
    if (identifier == "latam") {
        return ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA;
    }
    return ERUI_GAME_LANGUAGE_UNKNOWN;
}

SteamLanguageProbe probe_steam_language() noexcept {
    SteamLanguageProbe result{};
    try {
        const HMODULE steam = GetModuleHandleW(L"steam_api64.dll");
        if (!steam) {
            result.failure_code = SteamLanguageFailure::module_not_loaded;
            result.failure = "steam_api64.dll is not loaded";
            return result;
        }

        const auto steam_apps = load_export<SteamAppsAccessor>(
            steam, "SteamAPI_SteamApps_v008");
        const auto steam_user = load_export<SteamHandleQuery>(
            steam, "SteamAPI_GetHSteamUser");
        const auto steam_pipe = load_export<SteamHandleQuery>(
            steam, "SteamAPI_GetHSteamPipe");
        const auto current_language = load_export<SteamAppsStringQuery>(
            steam, "SteamAPI_ISteamApps_GetCurrentGameLanguage");
        if (!steam_apps || !steam_user || !steam_pipe || !current_language) {
            result.failure_code = SteamLanguageFailure::exports_unavailable;
            result.failure = "required Steamworks language exports are unavailable";
            return result;
        }

        // SteamAPI_Init owns these process-wide handles. Accessing an
        // interface before both are valid is outside Steamworks' contract and
        // can enter its synchronous context factory during partial startup.
        // Elden Ring owns initialization; ERNativeUI only observes it.
        if (steam_user() <= 0 || steam_pipe() <= 0) {
            result.failure_code = SteamLanguageFailure::apps_not_initialized;
            result.failure = "Steam API user/pipe handles are not initialized";
            return result;
        }

        void* const apps = steam_apps();
        if (!apps) {
            result.failure_code = SteamLanguageFailure::apps_not_initialized;
            result.failure = "SteamApps008 is not initialized";
            return result;
        }

        if (!copy_bounded_text(current_language(apps), result.current)) {
            result.failure_code = SteamLanguageFailure::current_identifier_invalid;
            result.failure = "Steam returned no valid current-language identifier";
            return result;
        }

        result.available = true;
        return result;
    } catch (...) {
        result.available = false;
        result.current.clear();
        result.failure_code = SteamLanguageFailure::exception;
        result.failure = "exception while copying Steam language information";
        return result;
    }
}

ERUI_Result settle_cached_steam_language(
    SteamLanguageProbe discovered) noexcept {
    const SteamLanguageReadiness settlement = discovered.available
        ? SteamLanguageReadiness::ready
        : SteamLanguageReadiness::unavailable;

    {
        std::lock_guard lock(g_language_mutex);
        const SteamLanguageReadiness existing =
            g_language_readiness.load(std::memory_order_relaxed);
        if (existing != SteamLanguageReadiness::pending) {
            return existing == SteamLanguageReadiness::ready
                ? static_cast<ERUI_Result>(ERUI_OK)
                : static_cast<ERUI_Result>(ERUI_NOT_SUPPORTED);
        }
        if (!discovered.available &&
            is_retryable_steam_language_failure(discovered.failure_code)) {
            return ERUI_HOST_NOT_READY;
        }
        if (discovered.available && discovered.current.empty()) {
            return ERUI_INVALID_ARGUMENT;
        }
        g_cached_language = std::move(discovered);
        g_language_readiness.store(settlement, std::memory_order_release);
    }
    g_language_changed.notify_all();
    return settlement == SteamLanguageReadiness::ready
        ? static_cast<ERUI_Result>(ERUI_OK)
        : static_cast<ERUI_Result>(ERUI_NOT_SUPPORTED);
}

SteamLanguageReadiness steam_language_readiness() noexcept {
    return g_language_readiness.load(std::memory_order_acquire);
}

ERUI_Result get_cached_steam_language(
    ERUI_GameLanguageInfo* out_language) noexcept {
    if (!out_language || out_language->size < sizeof(*out_language)) {
        return ERUI_INVALID_ARGUMENT;
    }

    std::unique_lock lock(g_language_mutex);
    g_language_changed.wait(lock, [] {
        return g_language_readiness.load(std::memory_order_acquire) !=
            SteamLanguageReadiness::pending;
    });
    if (g_language_readiness.load(std::memory_order_relaxed) ==
        SteamLanguageReadiness::unavailable) {
        return ERUI_NOT_SUPPORTED;
    }

    ERUI_GameLanguageInfo result{};
    result.size = sizeof(result);
    result.known_language = classify_steam_language(g_cached_language.current);
    result.identifier.data = g_cached_language.current.c_str();
    result.identifier.length = static_cast<std::uint32_t>(
        g_cached_language.current.size());
    *out_language = result;
    return ERUI_OK;
}

} // namespace erui::host
