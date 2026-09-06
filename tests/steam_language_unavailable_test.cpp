#include "steam_language.hpp"

#include "test_assertions.hpp"

#include <utility>

int main() {
    using erui::host::SteamLanguageFailure;
    using erui::host::SteamLanguageProbe;
    using erui::host::SteamLanguageReadiness;

    ERUI_GameLanguageInfo language{};
    language.size = sizeof(language);

    ERUI_TEST_CHECK(erui::host::steam_language_readiness() ==
        SteamLanguageReadiness::pending);

    SteamLanguageProbe pending{};
    pending.failure_code = SteamLanguageFailure::apps_not_initialized;
    pending.failure = "SteamApps is not initialized";
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(pending)) == ERUI_HOST_NOT_READY);
    ERUI_TEST_CHECK(erui::host::steam_language_readiness() ==
        SteamLanguageReadiness::pending);

    SteamLanguageProbe unavailable{};
    unavailable.failure_code = SteamLanguageFailure::readiness_timeout;
    unavailable.failure = "Steam language readiness deadline expired";
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(unavailable)) == ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(erui::host::steam_language_readiness() ==
        SteamLanguageReadiness::unavailable);
    ERUI_TEST_CHECK(erui::host::get_cached_steam_language(&language) ==
        ERUI_NOT_SUPPORTED);

    // Settlement is immutable in both directions. A late valid observation
    // cannot make different clients see a different process-wide result.
    SteamLanguageProbe too_late{};
    too_late.available = true;
    too_late.current = "french";
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(too_late)) == ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(erui::host::get_cached_steam_language(&language) ==
        ERUI_NOT_SUPPORTED);
    return 0;
}
