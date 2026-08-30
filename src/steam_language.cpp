#include "steam_language.hpp"

#include <Windows.h>

#include <cstring>
#include <mutex>
namespace erui::host {
namespace {

using SteamAppsAccessor = void* (__cdecl*)();
using SteamAppsStringQuery = const char* (__cdecl*)(void* steam_apps);

constexpr std::size_t kMaximumSteamLanguageText = 4096;
std::mutex g_language_mutex{};
SteamLanguageProbe g_cached_language{};

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
            result.failure = "steam_api64.dll is not loaded";
            return result;
        }

        const auto steam_apps = load_export<SteamAppsAccessor>(
            steam, "SteamAPI_SteamApps_v008");
        const auto current_language = load_export<SteamAppsStringQuery>(
            steam, "SteamAPI_ISteamApps_GetCurrentGameLanguage");
        const auto supported_languages = load_export<SteamAppsStringQuery>(
            steam, "SteamAPI_ISteamApps_GetAvailableGameLanguages");
        if (!steam_apps || !current_language || !supported_languages) {
            result.failure = "required Steamworks language exports are unavailable";
            return result;
        }

        void* const apps = steam_apps();
        if (!apps) {
            result.failure = "SteamApps008 is not initialized";
            return result;
        }

        if (!copy_bounded_text(current_language(apps), result.current)) {
            result.failure = "Steam returned no valid current-language identifier";
            return result;
        }
        if (!copy_bounded_text(supported_languages(apps), result.supported)) {
            result.failure = "Steam returned no valid available-language list";
            return result;
        }

        result.available = true;
        return result;
    } catch (...) {
        result.available = false;
        result.current.clear();
        result.supported.clear();
        result.failure = "exception while copying Steam language information";
        return result;
    }
}

ERUI_Result get_cached_steam_language(
    ERUI_GameLanguageInfo* out_language) noexcept {
    if (!out_language || out_language->size < sizeof(*out_language)) {
        return ERUI_INVALID_ARGUMENT;
    }

    std::lock_guard lock(g_language_mutex);
    if (!g_cached_language.available) {
        SteamLanguageProbe discovered = probe_steam_language();
        if (!discovered.available) return ERUI_NOT_SUPPORTED;
        g_cached_language = std::move(discovered);
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
