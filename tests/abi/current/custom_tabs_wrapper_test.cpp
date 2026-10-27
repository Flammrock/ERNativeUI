#include "host_loader.h"
#include "connection_control.h"

#include <ernativeui/ERNativeUI.hpp>

#include <array>
#include <cstdio>
#include <utility>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

void button_pressed() noexcept {}

void accept_page(erui::Page) noexcept {}

// These statements are intentionally never executed. They keep the source
// compatibility promised by the richer API 1.4 Tab return type under direct
// compilation: explicit Page storage, passing a Tab to Page-taking code, and
// building ordinary inherited rows through Tab all remain valid.
[[maybe_unused]] void compile_tab_source_compatibility(
    erui::Menu& menu,
    const erui::TabOptions& options) noexcept {
    erui::Page page = menu.add_tab(options);
    auto tab = menu.add_tab(options);
    accept_page(tab);
    (void)tab.add_button<&button_pressed>(
        L"Inherited row", L"Built directly through Tab");
    (void)page;
}

std::wstring observed_text(
    const ERUI_TestTitleScreenTextObservation& observation) {
    std::wstring result;
    result.reserve(observation.text_length);
    for (std::uint32_t index = 0; index < observation.text_length; ++index) {
        result.push_back(static_cast<wchar_t>(observation.text[index]));
    }
    return result;
}

std::wstring observed_tab_title(
    const ERUI_TestTabUpdateObservation& observation) {
    std::wstring result;
    result.reserve(observation.title_length);
    for (std::uint32_t index = 0; index < observation.title_length; ++index) {
        result.push_back(static_cast<wchar_t>(observation.title[index]));
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    auto connected = erui::connect();
    CHECK(connected.success());
    CHECK(connected.value().api_version() == ERUI_API_VERSION_CURRENT);
    CHECK(connected.value().supports(erui::Capability::custom_tabs));
    CHECK(connected.value().supports(erui::Capability::title_screen_text));
    const auto get_title_observation =
        reinterpret_cast<ERUI_TestGetTitleScreenTextObservationFn>(
            GetProcAddress(
                host.module, "ERUI_TestGetTitleScreenTextObservation"));
    const auto get_tab_update_observation =
        reinterpret_cast<ERUI_TestGetTabUpdateObservationFn>(
            GetProcAddress(
                host.module, "ERUI_TestGetTabUpdateObservation"));

    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT> pixels{};
    for (std::size_t offset = 3; offset < pixels.size(); offset += 4) {
        pixels[offset] = 255u;
    }
    erui::ProviderOptions provider{};
    provider.provider_id = "tests.current-v1-3-custom-tabs-cpp";
    provider.display_name = L"Current custom-tabs C++ wrapper";
    provider.owner_module = GetModuleHandleW(nullptr);
    erui::RowHandle row{ERUI_INVALID_ROW};
    erui::Tab editable_tab{};
    const auto result = connected.value().register_menu(
        provider,
        [&](erui::Menu& menu) noexcept {
            menu.add_title_screen_text(
                erui::TitleScreenCorner::top_left,
                {L"Current wrapper test"});
            menu.add_title_screen_text(
                erui::TitleScreenCorner::bottom_right,
                {
                    L"Colored wrapper test",
                    erui::Color{216u, 167u, 103u},
                });
            erui::TabOptions tab{};
            tab.tab_id = "settings";
            tab.title = L"Settings";
            tab.preferred_order = 25;
            tab.icon = {pixels.data(), pixels.size()};
            editable_tab = menu.add_tab(tab);
            erui::Page page = editable_tab;
            accept_page(editable_tab);
            row = editable_tab.add_button<&button_pressed>(
                L"Apply", L"Ordinary rows work on custom-tab pages");
            (void)page;

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
    CHECK(result.value().supports(erui::Capability::title_screen_text));
    CHECK(result.value().supports(erui::Capability::tab_updates));
    CHECK(row != ERUI_INVALID_ROW);
    CHECK(editable_tab.valid());
    CHECK(editable_tab.page_handle() != ERUI_INVALID_PAGE);

    erui::Tab invalid_tab{};
    CHECK(!invalid_tab.valid());
    CHECK(invalid_tab.edit().title(L"Invalid").commit() ==
        ERUI_INVALID_HANDLE);

    // The edit draft owns its icon snapshot. Mutating the source after icon()
    // must not affect the bytes submitted by commit().
    const auto expected_runtime_pixels = pixels;
    auto tab_edit = editable_tab.edit()
        .title(L"Runtime Settings")
        .icon({pixels.data(), pixels.size()})
        .visible(true)
        .preferred_order(-50);
    pixels.fill(0u);
    CHECK(tab_edit.commit() == ERUI_OK);
    CHECK(tab_edit.commit() == ERUI_ALREADY_COMMITTED);
    CHECK(editable_tab.edit().default_icon().commit() == ERUI_OK);
    CHECK(editable_tab.edit().title(L"").commit() ==
        ERUI_INVALID_ARGUMENT);
    CHECK(editable_tab.edit().commit() == ERUI_INVALID_ARGUMENT);

    erui::TabIconView invalid_runtime_icon{};
    invalid_runtime_icon.pixels = pixels.data();
    invalid_runtime_icon.byte_count = pixels.size() - 1u;
    CHECK(editable_tab.edit().icon(invalid_runtime_icon).commit() ==
        ERUI_INVALID_ARGUMENT);

    if (get_tab_update_observation) {
        ERUI_TestTabUpdateObservation observed{};
        observed.size = sizeof(observed);
        CHECK(get_tab_update_observation(0, &observed) == ERUI_OK);
        CHECK(observed.descriptor_size == sizeof(ERUI_TabUpdateDesc));
        CHECK(observed.page == editable_tab.page_handle());
        CHECK(observed.fields ==
            (ERUI_TAB_UPDATE_FIELD_TITLE |
                ERUI_TAB_UPDATE_FIELD_ICON |
                ERUI_TAB_UPDATE_FIELD_VISIBLE |
                ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER));
        CHECK(observed.visible == 1u);
        CHECK(observed.preferred_order == -50);
        CHECK(observed_tab_title(observed) == L"Runtime Settings");
        CHECK(observed.icon_present == 1u);
        CHECK(observed.icon_descriptor_size == sizeof(ERUI_TabIconDesc));
        CHECK(observed.icon_format == ERUI_TAB_ICON_FORMAT_RGBA8);
        CHECK(observed.icon_byte_count == ERUI_TAB_ICON_RGBA8_BYTE_COUNT);
        for (std::size_t index = 0;
             index < expected_runtime_pixels.size();
             ++index) {
            CHECK(observed.icon_pixels[index] ==
                expected_runtime_pixels[index]);
        }
        CHECK(observed.reserved[0] == 0u);
        CHECK(observed.reserved[1] == 0u);

        observed = {};
        observed.size = sizeof(observed);
        CHECK(get_tab_update_observation(1, &observed) == ERUI_OK);
        CHECK(observed.descriptor_size == sizeof(ERUI_TabUpdateDesc));
        CHECK(observed.page == editable_tab.page_handle());
        CHECK(observed.fields == ERUI_TAB_UPDATE_FIELD_ICON);
        CHECK(observed.visible == 0u);
        CHECK(observed.preferred_order == 0);
        CHECK(observed.title_length == 0u);
        for (const auto unit : observed.title) CHECK(unit == 0u);
        CHECK(observed.icon_present == 0u);
        CHECK(observed.icon_descriptor_size == 0u);
        CHECK(observed.icon_format == 0u);
        CHECK(observed.icon_byte_count == 0u);
        for (const auto byte : observed.icon_pixels) CHECK(byte == 0u);
        CHECK(observed.reserved[0] == 0u);
        CHECK(observed.reserved[1] == 0u);

        observed = {};
        observed.size = sizeof(observed);
        CHECK(get_tab_update_observation(2, &observed) == ERUI_NOT_FOUND);
    }

    auto moved_from_edit = editable_tab.edit().visible(true);
    auto moved_edit = std::move(moved_from_edit);
    CHECK(moved_from_edit.commit() == ERUI_INVALID_HANDLE);
    CHECK(moved_edit.commit() == ERUI_OK);

    // The explicit connection fixture exposes a test-only descriptor capture
    // seam.  The production host deliberately does not export it; the shared
    // production-contract run continues below with public registration,
    // capability, validation, and process-wide budget assertions.
    if (get_title_observation) {
        ERUI_TestTitleScreenTextObservation observed{};
        observed.size = sizeof(observed);
        CHECK(get_title_observation(0, &observed) == ERUI_OK);
        CHECK(observed.descriptor_size == sizeof(ERUI_TitleScreenTextDesc));
        CHECK(observed.flags == 0);
        CHECK(observed.color.red == 0);
        CHECK(observed.color.green == 0);
        CHECK(observed.color.blue == 0);
        CHECK(observed.color.reserved == 0);
        CHECK(observed.corner == ERUI_TITLE_SCREEN_CORNER_TOP_LEFT);
        CHECK(observed_text(observed) == L"Current wrapper test");

        observed = {};
        observed.size = sizeof(observed);
        CHECK(get_title_observation(1, &observed) == ERUI_OK);
        CHECK(observed.descriptor_size == sizeof(ERUI_TitleScreenTextDesc));
        CHECK(observed.flags == ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR);
        CHECK(observed.color.red == 216u);
        CHECK(observed.color.green == 167u);
        CHECK(observed.color.blue == 103u);
        CHECK(observed.color.reserved == 0);
        CHECK(observed.corner == ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT);
        CHECK(observed_text(observed) == L"Colored wrapper test");

        observed = {};
        observed.size = sizeof(observed);
        CHECK(get_title_observation(2, &observed) == ERUI_NOT_FOUND);
    }

    // Two committed lines leave fourteen process-wide slots. A poisoned
    // draft may stage all fourteen, but aborting it must release the draft so
    // a replacement provider can commit the same fourteen lines.
    erui::ProviderOptions abandoned_title_provider{};
    abandoned_title_provider.provider_id =
        "tests.current-v1-3-abandoned-title-budget-cpp";
    abandoned_title_provider.display_name = L"Abandoned title budget";
    abandoned_title_provider.owner_module = GetModuleHandleW(nullptr);
    const auto abandoned_title = connected.value().register_menu(
        abandoned_title_provider,
        [&](erui::Menu& menu) noexcept {
            for (std::uint32_t index = 0;
                 index < ERUI_TITLE_SCREEN_TEXT_MAX_LINES - 2u;
                 ++index) {
                menu.add_title_screen_text(
                    erui::TitleScreenCorner::bottom_left,
                    {L"Abandoned title line"});
            }
            menu.add_title_screen_text(
                erui::TitleScreenCorner::bottom_left,
                {L"Invalid\ntitle line"});
        });
    CHECK(!abandoned_title.success());
    CHECK(abandoned_title.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui::ProviderOptions replacement_title_provider{};
    replacement_title_provider.provider_id =
        "tests.current-v1-3-replacement-title-budget-cpp";
    replacement_title_provider.display_name = L"Replacement title budget";
    replacement_title_provider.owner_module = GetModuleHandleW(nullptr);
    const auto replacement_title = connected.value().register_menu(
        replacement_title_provider,
        [&](erui::Menu& menu) noexcept {
            for (std::uint32_t index = 0;
                 index < ERUI_TITLE_SCREEN_TEXT_MAX_LINES - 2u;
                 ++index) {
                menu.add_title_screen_text(
                    erui::TitleScreenCorner::bottom_left,
                    {L"Replacement title line"});
            }
        });
    CHECK(replacement_title.success());

    erui::ProviderOptions exhausted_title_provider{};
    exhausted_title_provider.provider_id =
        "tests.current-v1-3-exhausted-title-budget-cpp";
    exhausted_title_provider.display_name = L"Exhausted title budget";
    exhausted_title_provider.owner_module = GetModuleHandleW(nullptr);
    const auto exhausted_title = connected.value().register_menu(
        exhausted_title_provider,
        [&](erui::Menu& menu) noexcept {
            menu.add_title_screen_text(
                erui::TitleScreenCorner::top_right,
                {L"No committed capacity remains"});
        });
    CHECK(!exhausted_title.success());
    CHECK(exhausted_title.error().native_result() ==
        ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED);

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

    erui::ProviderOptions invalid_title_provider{};
    invalid_title_provider.provider_id =
        "tests.current-v1-3-invalid-title-screen-text-cpp";
    invalid_title_provider.display_name = L"Invalid title-screen text";
    invalid_title_provider.owner_module = GetModuleHandleW(nullptr);
    const auto invalid_title = connected.value().register_menu(
        invalid_title_provider,
        [&](erui::Menu& menu) noexcept {
            menu.add_title_screen_text(
                erui::TitleScreenCorner::top_right,
                {L"First line\nSecond line"});
        });
    CHECK(!invalid_title.success());
    CHECK(invalid_title.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui::ProviderOptions invalid_corner_provider{};
    invalid_corner_provider.provider_id =
        "tests.current-v1-3-invalid-title-screen-corner-cpp";
    invalid_corner_provider.display_name = L"Invalid title-screen corner";
    invalid_corner_provider.owner_module = GetModuleHandleW(nullptr);
    const auto invalid_corner = connected.value().register_menu(
        invalid_corner_provider,
        [&](erui::Menu& menu) noexcept {
            menu.add_title_screen_text(
                static_cast<erui::TitleScreenCorner>(0u),
                {L"Invalid corner"});
        });
    CHECK(!invalid_corner.success());
    CHECK(invalid_corner.error().native_result() == ERUI_INVALID_ARGUMENT);

    erui_unload_test_host(&host);
    return 0;
}
