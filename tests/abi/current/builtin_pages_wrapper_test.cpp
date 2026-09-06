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
    CHECK(connected.value().api_version() == ERUI_API_VERSION_1_1);
    CHECK(connected.value().supports(erui::Capability::builtin_pages));

    constexpr std::array builtin_pages{
        erui::BuiltinPage::game_options,
        erui::BuiltinPage::camera_options,
        erui::BuiltinPage::display,
        erui::BuiltinPage::sound,
        erui::BuiltinPage::network,
        erui::BuiltinPage::keyboard_mouse,
        erui::BuiltinPage::graphics,
    };
    std::array<erui::RowHandle, builtin_pages.size()> rows{};
    erui::ProviderOptions provider{};
    provider.provider_id = "tests.current-v1-1-builtin-pages-cpp";
    provider.display_name = L"Current built-in pages C++ wrapper";
    provider.owner_module = GetModuleHandleW(nullptr);

    const auto result = connected.value().register_menu(
        provider,
        [&](erui::Menu& menu) noexcept {
            for (std::size_t index = 0; index < builtin_pages.size(); ++index) {
                erui::Page page = menu.page(builtin_pages[index]);
                if (!page.valid()) return;
                rows[index] = page.add_button<&button_pressed>(
                    L"Built-in page row",
                    L"C++17 wrapper contract");
            }
        });
    CHECK(result.success());
    CHECK(result.value().supports(erui::Capability::builtin_pages));
    for (const erui::RowHandle row : rows) {
        CHECK(row != ERUI_INVALID_ROW);
    }

    erui::ProviderOptions invalid_provider{};
    invalid_provider.provider_id = "tests.invalid-builtin-page-cpp";
    invalid_provider.display_name = L"Invalid built-in page C++ wrapper";
    invalid_provider.owner_module = GetModuleHandleW(nullptr);
    const auto invalid = connected.value().register_menu(
        invalid_provider,
        [](erui::Menu& menu) noexcept {
            (void)menu.page(static_cast<erui::BuiltinPage>(
                ERUI_BUILTIN_PAGE_COUNT));
        });
    CHECK(!invalid.success());
    CHECK(invalid.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui_unload_test_host(&host);
    return 0;
}
