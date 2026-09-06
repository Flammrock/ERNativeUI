#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <cstdio>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

void ERUI_CALL changed(void*, std::uint8_t) noexcept {}

} // namespace

int main(int argc, char** argv) {
    static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_0);
    static_assert(sizeof(ERUI_Api) == 128u);
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    erui::RowHandle toggle{};
    erui::ProviderOptions options{};
    options.provider_id = "tests.v1-0-wrapper-to-current-smoke";
    options.display_name = L"Frozen wrapper to current host";
    options.owner_module = GetModuleHandleW(nullptr);
    const auto result = erui::register_menu(options, [&](erui::Menu& menu) {
        toggle = menu.root().add_toggle(
            L"Frozen toggle", L"ABI wrapper smoke test", 1, &changed);
    });
    CHECK(result.success());
    CHECK(toggle != ERUI_INVALID_ROW);
    CHECK(result.value().set_value(toggle, 0) == ERUI_OK);
    std::uint8_t value{1};
    CHECK(result.value().get_value(toggle, value) == ERUI_OK);
    CHECK(value == 0);

    erui_unload_test_host(&host);
    return 0;
}
