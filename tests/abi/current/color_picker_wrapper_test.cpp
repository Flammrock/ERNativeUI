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

struct Settings {
    erui::Color accent{171u, 125u, 99u};
    erui::Color observed{};
    erui::RowHandle observed_row{};
    unsigned calls{};
};

void accent_changed(
    Settings& settings,
    const erui::ColorPickerChange& change) noexcept {
    settings.observed = change.value;
    settings.observed_row = change.row;
    ++settings.calls;
}

bool color_equal(erui::Color left, erui::Color right) noexcept {
    return left.red == right.red && left.green == right.green &&
        left.blue == right.blue;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    Settings settings{};
    erui::RowHandle row{ERUI_INVALID_ROW};
    bool capability_reported{};
    erui::ProviderOptions provider{};
    provider.provider_id = "tests.current-v1-1-color-picker-cpp";
    provider.display_name = L"Current ColorPicker C++ wrapper";
    provider.owner_module = GetModuleHandleW(nullptr);

    auto connected = erui::connect();
    CHECK(connected.success());
    CHECK(connected.value().api_version() == ERUI_API_VERSION_1_1);
    auto result = connected.value().register_menu(
        provider, [&](erui::Menu& menu) {
        capability_reported = menu.supports(erui::Capability::color_picker);
        erui::ColorPickerOptions options{};
        options.initial_value = settings.accent;
        row = menu.root().add_color_picker<&accent_changed>(
            L"Accent Color",
            L"C++17 wrapper contract",
            options,
            settings);
        });
    CHECK(result.success());
    CHECK(result.value().api_version() == ERUI_API_VERSION_1_1);
    CHECK(capability_reported);
    CHECK(result.value().supports(erui::Capability::color_picker));
    CHECK(row != ERUI_INVALID_ROW);

    erui::Color value{};
    CHECK(result.value().get_color(row, value) == ERUI_OK);
    CHECK(color_equal(value, settings.accent));

    const erui::Color programmed{0u, 127u, 255u};
    CHECK(result.value().set_color(row, programmed) == ERUI_OK);
    CHECK(settings.calls == 0);
    CHECK(result.value().get_color(row, value) == ERUI_OK);
    CHECK(color_equal(value, programmed));

    erui_unload_test_host(&host);
    return 0;
}
