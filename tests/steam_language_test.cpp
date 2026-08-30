#include "steam_language.hpp"

#include "test_assertions.hpp"

#include <array>
#include <string_view>

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
    return 0;
}
