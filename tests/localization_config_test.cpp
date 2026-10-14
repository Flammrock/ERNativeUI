#include "localization_config.hpp"

#include "test_assertions.hpp"

#include <string>

int main() {
    using erui::host::LocalizationLanguageMode;
    using erui::host::parse_localization_language;

    const auto automatic = parse_localization_language(L"auto");
    ERUI_TEST_CHECK(automatic.mode == LocalizationLanguageMode::automatic);
    ERUI_TEST_CHECK(automatic.identifier.empty());

    const auto padded_automatic =
        parse_localization_language(L" \t auto\t ");
    ERUI_TEST_CHECK(
        padded_automatic.mode == LocalizationLanguageMode::automatic);

    const auto known = parse_localization_language(L"french");
    ERUI_TEST_CHECK(
        known.mode == LocalizationLanguageMode::explicit_identifier);
    ERUI_TEST_CHECK(known.identifier == "french");

    const auto forward_compatible =
        parse_localization_language(L"vietnamese");
    ERUI_TEST_CHECK(
        forward_compatible.mode ==
        LocalizationLanguageMode::explicit_identifier);
    ERUI_TEST_CHECK(forward_compatible.identifier == "vietnamese");

    const auto community =
        parse_localization_language(L"community-locale_2");
    ERUI_TEST_CHECK(
        community.mode == LocalizationLanguageMode::explicit_identifier);
    ERUI_TEST_CHECK(community.identifier == "community-locale_2");

    const auto maximum_length =
        parse_localization_language(std::wstring(64, L'a'));
    ERUI_TEST_CHECK(
        maximum_length.mode ==
        LocalizationLanguageMode::explicit_identifier);
    ERUI_TEST_CHECK(maximum_length.identifier.size() == 64);

    ERUI_TEST_CHECK(parse_localization_language(L"").mode ==
        LocalizationLanguageMode::invalid);
    ERUI_TEST_CHECK(parse_localization_language(L"AUTO").mode ==
        LocalizationLanguageMode::invalid);
    ERUI_TEST_CHECK(parse_localization_language(L"../french").mode ==
        LocalizationLanguageMode::invalid);
    ERUI_TEST_CHECK(parse_localization_language(L"french.ini").mode ==
        LocalizationLanguageMode::invalid);
    ERUI_TEST_CHECK(parse_localization_language(L"fr fran\u00e7ais").mode ==
        LocalizationLanguageMode::invalid);

    const std::wstring oversized(65, L'a');
    ERUI_TEST_CHECK(parse_localization_language(oversized).mode ==
        LocalizationLanguageMode::invalid);
    return 0;
}
