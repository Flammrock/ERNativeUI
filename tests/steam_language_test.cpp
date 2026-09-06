#include "steam_language.hpp"

#include "test_assertions.hpp"

#include <array>
#include <string_view>
#include <utility>

int main() {
    using Pair = std::pair<std::string_view, ERUI_GameLanguage>;
    constexpr std::array<Pair, 15> cases{{
        {"english", ERUI_GAME_LANGUAGE_ENGLISH},
        {"german", ERUI_GAME_LANGUAGE_GERMAN},
        {"french", ERUI_GAME_LANGUAGE_FRENCH},
        {"italian", ERUI_GAME_LANGUAGE_ITALIAN},
        {"koreana", ERUI_GAME_LANGUAGE_KOREAN},
        {"spanish", ERUI_GAME_LANGUAGE_SPANISH},
        {"schinese", ERUI_GAME_LANGUAGE_CHINESE_SIMPLIFIED},
        {"tchinese", ERUI_GAME_LANGUAGE_CHINESE_TRADITIONAL},
        {"russian", ERUI_GAME_LANGUAGE_RUSSIAN},
        {"thai", ERUI_GAME_LANGUAGE_THAI},
        {"japanese", ERUI_GAME_LANGUAGE_JAPANESE},
        {"polish", ERUI_GAME_LANGUAGE_POLISH},
        {"arabic", ERUI_GAME_LANGUAGE_ARABIC},
        {"brazilian", ERUI_GAME_LANGUAGE_PORTUGUESE_BRAZIL},
        {"latam", ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA},
    }};
    for (const auto& [identifier, expected] : cases) {
        ERUI_TEST_CHECK(erui::host::classify_steam_language(identifier) == expected);
    }
    ERUI_TEST_CHECK(erui::host::classify_steam_language("community-locale") ==
        ERUI_GAME_LANGUAGE_UNKNOWN);
    ERUI_TEST_CHECK(erui::host::classify_steam_language("") ==
        ERUI_GAME_LANGUAGE_UNKNOWN);
    ERUI_TEST_CHECK(erui::host::is_retryable_steam_language_failure(
        erui::host::SteamLanguageFailure::apps_not_initialized));
    ERUI_TEST_CHECK(erui::host::is_retryable_steam_language_failure(
        erui::host::SteamLanguageFailure::current_identifier_invalid));
    ERUI_TEST_CHECK(erui::host::is_retryable_steam_language_failure(
        erui::host::SteamLanguageFailure::module_not_loaded));
    ERUI_TEST_CHECK(!erui::host::is_retryable_steam_language_failure(
        erui::host::SteamLanguageFailure::exports_unavailable));
    ERUI_TEST_CHECK(!erui::host::is_retryable_steam_language_failure(
        erui::host::SteamLanguageFailure::exception));
    ERUI_TEST_CHECK(!erui::host::is_retryable_steam_language_failure(
        erui::host::SteamLanguageFailure::readiness_timeout));

    erui::host::SteamLanguageProbe startup_pending{};
    startup_pending.failure_code =
        erui::host::SteamLanguageFailure::apps_not_initialized;
    ERUI_TEST_CHECK(erui::host::decide_steam_language_probe(
        startup_pending, 4999, 5000) ==
        erui::host::SteamLanguageProbeDecision::retry);
    ERUI_TEST_CHECK(erui::host::decide_steam_language_probe(
        startup_pending, 5000, 5000) ==
        erui::host::SteamLanguageProbeDecision::timeout);
    startup_pending.failure_code =
        erui::host::SteamLanguageFailure::exports_unavailable;
    ERUI_TEST_CHECK(erui::host::decide_steam_language_probe(
        startup_pending, 0, 5000) ==
        erui::host::SteamLanguageProbeDecision::settle);
    startup_pending.available = true;
    startup_pending.failure_code =
        erui::host::SteamLanguageFailure::none;
    ERUI_TEST_CHECK(erui::host::decide_steam_language_probe(
        startup_pending, 0, 5000) ==
        erui::host::SteamLanguageProbeDecision::settle);

    ERUI_TEST_CHECK(erui::host::steam_language_readiness() ==
        erui::host::SteamLanguageReadiness::pending);
    erui::host::SteamLanguageProbe retryable{};
    retryable.failure_code =
        erui::host::SteamLanguageFailure::module_not_loaded;
    retryable.failure = "test module pending";
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(retryable)) == ERUI_HOST_NOT_READY);
    ERUI_TEST_CHECK(erui::host::steam_language_readiness() ==
        erui::host::SteamLanguageReadiness::pending);

    erui::host::SteamLanguageProbe ready{};
    ready.available = true;
    ready.current = "french";
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(ready)) == ERUI_OK);
    ERUI_TEST_CHECK(erui::host::steam_language_readiness() ==
        erui::host::SteamLanguageReadiness::ready);

    ERUI_GameLanguageInfo language{};
    language.size = sizeof(language);
    ERUI_TEST_CHECK(erui::host::get_cached_steam_language(&language) ==
        ERUI_OK);
    ERUI_TEST_CHECK(language.known_language == ERUI_GAME_LANGUAGE_FRENCH);
    ERUI_TEST_CHECK(language.identifier.length == 6);
    ERUI_TEST_CHECK(std::string_view(
        language.identifier.data, language.identifier.length) == "french");

    // Settlement is immutable. A later terminal observation cannot replace
    // the language snapshot already published to clients.
    erui::host::SteamLanguageProbe too_late{};
    too_late.failure_code =
        erui::host::SteamLanguageFailure::exports_unavailable;
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(too_late)) == ERUI_OK);
    ERUI_TEST_CHECK(erui::host::get_cached_steam_language(&language) ==
        ERUI_OK);
    ERUI_TEST_CHECK(language.known_language == ERUI_GAME_LANGUAGE_FRENCH);
    return 0;
}
