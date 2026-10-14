#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

struct Settings {
    unsigned numeric_calls{};
    unsigned custom_calls{};
    erui::NumericSliderChange numeric_change{};
    erui::CustomSliderChange custom_change{};
};

void numeric_changed(
    Settings& settings,
    const erui::NumericSliderChange& change) noexcept {
    ++settings.numeric_calls;
    settings.numeric_change = change;
}

void custom_changed(
    Settings& settings,
    const erui::CustomSliderChange& change) noexcept {
    ++settings.custom_calls;
    settings.custom_change = change;
}

bool close_to(double left, double right) noexcept {
    return std::fabs(left - right) < 0.0000001;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    auto connected = erui::connect();
    CHECK(connected.success());
    CHECK(connected.value().api_version() == ERUI_API_VERSION_1_2);
    CHECK(connected.value().supports(erui::Capability::numeric_slider));
    CHECK(connected.value().supports(erui::Capability::custom_slider));

    constexpr std::array<const wchar_t*, 7> crazy_values{
        L"-7", L"3", L"7.8", L"1s", L"hello", L"18", L"x1.3"};
    unsigned formatter_calls{};
    Settings settings{};
    erui::RowHandle numeric_row{ERUI_INVALID_ROW};
    erui::RowHandle custom_row{ERUI_INVALID_ROW};

    erui::ProviderOptions provider{};
    provider.provider_id = "tests.current-v1-2-sliders-cpp";
    provider.display_name = L"Current sliders C++ wrapper";
    provider.owner_module = GetModuleHandleW(nullptr);
    const auto result = connected.value().register_menu(
        provider,
        [&](erui::Menu& menu) noexcept {
            erui::NumericSliderOptions numeric{};
            numeric.minimum = -1.0;
            numeric.maximum = 1.0;
            numeric.step = 0.1;
            numeric.initial_value = -0.55;
            numeric.prefix = L"x";
            numeric.suffix = L"s";
            numeric_row = menu.root().add_slider<&numeric_changed>(
                L"Numeric slider",
                L"Signed decimal wrapper contract",
                numeric,
                settings);

            erui::CustomSliderOptions custom{};
            custom.value_count = static_cast<std::uint32_t>(
                crazy_values.size());
            custom.initial_index = 4;
            custom.format_value = [&](std::uint32_t index, std::uint32_t count) {
                ++formatter_calls;
                if (count != crazy_values.size() || index >= count) {
                    return std::wstring{};
                }
                return std::wstring{crazy_values[index]};
            };
            custom_row = menu.root().add_custom_slider<&custom_changed>(
                L"Crazy custom slider",
                L"Arbitrary index-to-text wrapper contract",
                custom,
                settings);
        });
    CHECK(result.success());
    CHECK(result.value().api_version() == ERUI_API_VERSION_1_2);
    CHECK(result.value().supports(erui::Capability::numeric_slider));
    CHECK(result.value().supports(erui::Capability::custom_slider));
    CHECK(numeric_row != ERUI_INVALID_ROW);
    CHECK(custom_row != ERUI_INVALID_ROW);
    CHECK(numeric_row != custom_row);
    CHECK(formatter_calls == crazy_values.size());

    double numeric_value{};
    CHECK(result.value().get_numeric_slider_value(
        numeric_row, numeric_value) == ERUI_OK);
    CHECK(close_to(numeric_value, -0.6));
    CHECK(result.value().set_numeric_slider_value(
        numeric_row, 0.35) == ERUI_OK);
    CHECK(settings.numeric_calls == 0);
    CHECK(result.value().get_numeric_slider_value(
        numeric_row, numeric_value) == ERUI_OK);
    CHECK(close_to(numeric_value, 0.3));
    CHECK(result.value().set_numeric_slider_value(
        numeric_row, 2.0) == ERUI_INVALID_ARGUMENT);

    std::uint32_t custom_index{UINT32_MAX};
    CHECK(result.value().get_custom_slider_index(
        custom_row, custom_index) == ERUI_OK);
    CHECK(custom_index == 4u);
    CHECK(result.value().set_custom_slider_index(
        custom_row, 6u) == ERUI_OK);
    CHECK(settings.custom_calls == 0);
    CHECK(result.value().get_custom_slider_index(
        custom_row, custom_index) == ERUI_OK);
    CHECK(custom_index == 6u);
    CHECK(result.value().set_custom_slider_index(
        custom_row, 7u) == ERUI_INVALID_ARGUMENT);

    /* Formatter failures become ordinary registration failures; exceptions
       never escape the noexcept public registration boundary. */
    erui::ProviderOptions throwing_provider{};
    throwing_provider.provider_id = "tests.current-v1-2-throwing-slider-cpp";
    throwing_provider.display_name = L"Throwing custom slider formatter";
    throwing_provider.owner_module = GetModuleHandleW(nullptr);
    const auto throwing_result = connected.value().register_menu(
        throwing_provider,
        [](erui::Menu& menu) noexcept {
            erui::CustomSliderOptions custom{};
            custom.value_count = 2;
            custom.format_value = [](std::uint32_t, std::uint32_t)
                -> std::wstring {
                throw std::runtime_error("formatter failed");
            };
            (void)menu.root().add_custom_slider(
                L"Rejected custom slider", L"", custom);
        });
    CHECK(!throwing_result.success());
    CHECK(throwing_result.error().native_result() == ERUI_CALLBACK_REJECTED);

    erui::ProviderOptions invalid_provider{};
    invalid_provider.provider_id = "tests.current-v1-2-invalid-slider-cpp";
    invalid_provider.display_name = L"Invalid custom slider";
    invalid_provider.owner_module = GetModuleHandleW(nullptr);
    const auto invalid_result = connected.value().register_menu(
        invalid_provider,
        [](erui::Menu& menu) noexcept {
            erui::CustomSliderOptions custom{};
            custom.value_count = 1;
            custom.format_value = [](
                std::uint32_t, std::uint32_t) { return std::wstring{L"only"}; };
            (void)menu.root().add_custom_slider(
                L"Rejected custom slider", L"", custom);
        });
    CHECK(!invalid_result.success());
    CHECK(invalid_result.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui_unload_test_host(&host);
    return 0;
}
