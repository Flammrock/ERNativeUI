#include "host_locale.hpp"

#include "test_assertions.hpp"

#include <filesystem>

int main() {
    const std::filesystem::path assets = ERNATIVEUI_TEST_ASSETS;

    const erui::host::HostLocale french =
        erui::host::load_host_locale(assets, "french");
    ERUI_TEST_CHECK(french.loaded);
    ERUI_TEST_CHECK(french.pagination.previous_label == L"Page précédente");
    ERUI_TEST_CHECK(french.pagination.next_label == L"Page suivante");

    const erui::host::HostLocale missing =
        erui::host::load_host_locale(assets, "community-locale");
    ERUI_TEST_CHECK(!missing.loaded);
    ERUI_TEST_CHECK(missing.pagination.previous_label == L"Previous Page");
    ERUI_TEST_CHECK(missing.pagination.next_label == L"Next Page");

    const erui::host::HostLocale unsafe =
        erui::host::load_host_locale(assets, "../french");
    ERUI_TEST_CHECK(!unsafe.loaded);
    return 0;
}
