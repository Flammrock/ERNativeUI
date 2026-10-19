#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <array>
#include <cstdio>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

void button_pressed() noexcept {}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    auto connected = erui::connect();
    CHECK(connected.success());
    CHECK(connected.value().api_version() == ERUI_API_VERSION_1_3);
    CHECK(connected.value().supports(erui::Capability::custom_tabs));

    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT> pixels{};
    for (std::size_t offset = 3; offset < pixels.size(); offset += 4) {
        pixels[offset] = 255u;
    }
    erui::ProviderOptions provider{};
    provider.provider_id = "tests.current-v1-3-custom-tabs-cpp";
    provider.display_name = L"Current custom-tabs C++ wrapper";
    provider.owner_module = GetModuleHandleW(nullptr);
    erui::RowHandle row{ERUI_INVALID_ROW};
    const auto result = connected.value().register_menu(
        provider,
        [&](erui::Menu& menu) noexcept {
            erui::TabOptions tab{};
            tab.tab_id = "settings";
            tab.title = L"Settings";
            tab.preferred_order = 25;
            tab.icon = {pixels.data(), pixels.size()};
            erui::Page page = menu.add_tab(tab);
            row = page.add_button<&button_pressed>(
                L"Apply", L"Ordinary rows work on custom-tab pages");

            pixels[3] = 0u;
            pixels[7] = 127u;
            pixels[11] = 255u;
            erui::TabOptions alpha{};
            alpha.tab_id = "transparent";
            alpha.title = L"Transparent";
            alpha.icon = {
                pixels.data(),
                pixels.size()};
            (void)menu.add_tab(alpha);

            erui::TabOptions fallback{};
            fallback.tab_id = "advanced";
            fallback.title = L"Advanced";
            (void)menu.add_tab(fallback);
        });
    CHECK(result.success());
    CHECK(result.value().supports(erui::Capability::custom_tabs));
    CHECK(row != ERUI_INVALID_ROW);

    erui::ProviderOptions invalid_provider{};
    invalid_provider.provider_id = "tests.current-v1-3-invalid-tab-cpp";
    invalid_provider.display_name = L"Invalid custom tab";
    invalid_provider.owner_module = GetModuleHandleW(nullptr);
    pixels[3] = 0u;
    const auto invalid = connected.value().register_menu(
        invalid_provider,
        [&](erui::Menu& menu) noexcept {
            erui::TabOptions tab{};
            tab.tab_id = "invalid";
            tab.title = L"Invalid";
            tab.icon = {
                pixels.data(),
                pixels.size(),
                static_cast<erui::TabIconFormat>(2u)};
            (void)menu.add_tab(tab);
        });
    CHECK(!invalid.success());
    CHECK(invalid.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui::ProviderOptions oversized_provider{};
    oversized_provider.provider_id =
        "tests.current-v1-3-oversized-tab-title-cpp";
    oversized_provider.display_name = L"Oversized custom tab title";
    oversized_provider.owner_module = GetModuleHandleW(nullptr);
    const std::wstring oversized_title(
        ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS + 1u, L'X');
    const auto oversized = connected.value().register_menu(
        oversized_provider,
        [&](erui::Menu& menu) noexcept {
            erui::TabOptions tab{};
            tab.tab_id = "oversized";
            tab.title = oversized_title;
            (void)menu.add_tab(tab);
        });
    CHECK(!oversized.success());
    CHECK(oversized.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui_unload_test_host(&host);
    return 0;
}
