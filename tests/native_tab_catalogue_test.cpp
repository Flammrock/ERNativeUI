#include "native_tab_catalogue.hpp"

#include "menu.hpp"
#include "test_assertions.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using erui::TabIconSource;
using erui::TabMetadata;
using erui::detail::NativeTabCatalogueBuildResult;
using erui::detail::NativeTabCatalogueError;

struct TabSpec final {
    std::string provider_id{};
    std::string tab_id{};
    std::wstring title{};
    std::int32_t preferred_order{};
    std::optional<std::int32_t> player_order{};
    std::int32_t effective_order{};
    bool rgba8_icon{};
};

void pressed(void*) noexcept {}

std::unique_ptr<erui::detail::CompiledMenu> compile_menu(
    const std::vector<TabSpec>& tabs,
    bool add_builtin_pages = false) {
    erui::Menu menu(L"Catalogue host", L"Catalogue help");
    if (add_builtin_pages) {
        menu.builtin_page(erui::detail::BuiltinPage::camera_options)
            .add_button(L"Camera row", L"Camera help", {.callback = &pressed});
        menu.builtin_page(erui::detail::BuiltinPage::graphics)
            .add_button(
                L"Graphics row", L"Graphics help", {.callback = &pressed});
    }
    for (const TabSpec& spec : tabs) {
        TabMetadata metadata{};
        metadata.provider_id = spec.provider_id;
        metadata.tab_id = spec.tab_id;
        metadata.preferred_order = spec.preferred_order;
        metadata.player_order = spec.player_order;
        metadata.effective_order = spec.effective_order;
        if (spec.rgba8_icon) {
            metadata.icon.source = TabIconSource::rgba8;
            metadata.icon.rgba8_pixels.resize(
                ERUI_TAB_ICON_RGBA8_BYTE_COUNT);
            for (std::size_t offset = 0;
                 offset < metadata.icon.rgba8_pixels.size();
                 offset += 4) {
                metadata.icon.rgba8_pixels[offset] = 17;
                metadata.icon.rgba8_pixels[offset + 1] = 34;
                metadata.icon.rgba8_pixels[offset + 2] = 51;
                metadata.icon.rgba8_pixels[offset + 3] = 255;
            }
        }
        menu.add_tab(std::move(metadata), spec.title)
            .add_button(L"Tab row", L"Tab row help", {.callback = &pressed});
    }
    return erui::detail::MenuCompiler::compile(menu);
}

erui::detail::TabConfiguration default_configuration() {
    return erui::detail::parse_tab_configuration({});
}

void expect_error(
    NativeTabCatalogueBuildResult result,
    NativeTabCatalogueError error,
    std::size_t tab_index = erui::detail::invalid_compiled_index) {
    ERUI_TEST_CHECK(!result);
    ERUI_TEST_CHECK(!result.catalogue);
    ERUI_TEST_CHECK(result.error == error);
    ERUI_TEST_CHECK(result.tab_index == tab_index);
    ERUI_TEST_CHECK(std::string(
        erui::detail::native_tab_catalogue_error_name(result.error)) !=
        "unknown");
}

void test_stock_catalogue_and_settings() {
    auto compiled = compile_menu({}, true);
    auto configuration = erui::detail::parse_tab_configuration(
        "[Tabs]\n"
        "NavigationMargin = 3\n"
        "MouseDwellEnabled = 0\n"
        "MouseDwellInitialDelayMs = 125\n"
        "MouseDwellRepeatDelayMs = 100\n"
        "MouseDwellDecayMs = 5\n"
        "MouseDwellMinimumDelayMs = 40\n");

    const auto result = erui::detail::build_native_tab_catalogue(
        *compiled, configuration);
    ERUI_TEST_CHECK(result);
    ERUI_TEST_CHECK(result.error == NativeTabCatalogueError::none);
    ERUI_TEST_CHECK(result.catalogue->entries().size() == 8);
    ERUI_TEST_CHECK(result.catalogue->stock_entries().size() == 8);
    ERUI_TEST_CHECK(result.catalogue->custom_entries().empty());
    ERUI_TEST_CHECK(result.catalogue->custom_count() == 0);
    ERUI_TEST_CHECK(
        result.catalogue->navigation_settings() == configuration.settings);

    for (std::size_t index = 0;
         index < erui::detail::native_stock_tab_count;
         ++index) {
        const auto& entry = result.catalogue->entries()[index];
        ERUI_TEST_CHECK(entry.is_stock());
        ERUI_TEST_CHECK(!entry.is_custom());
        ERUI_TEST_CHECK(entry.native_category ==
            erui::detail::native_stock_tab_categories[index]);
        ERUI_TEST_CHECK(entry.title_id ==
            erui::detail::native_owned_tab_title_id);
        ERUI_TEST_CHECK(entry.provider_id.empty());
        ERUI_TEST_CHECK(entry.tab_id.empty());
        ERUI_TEST_CHECK(entry.icon.source == TabIconSource::fallback);
        ERUI_TEST_CHECK(entry.icon.rgba8_pixels.empty());
    }

    ERUI_TEST_CHECK(result.catalogue->entries()[0].page_index ==
        compiled->root_page_index);
    ERUI_TEST_CHECK(result.catalogue->entries()[1].page_index ==
        compiled->builtin_page_index(1u));
    ERUI_TEST_CHECK(result.catalogue->entries()[5].native_category == 6u);
    ERUI_TEST_CHECK(result.catalogue->entries()[5].page_index ==
        erui::detail::invalid_compiled_index);
    ERUI_TEST_CHECK(result.catalogue->entries()[7].page_index ==
        compiled->builtin_page_index(8u));
}

void test_custom_order_identity_title_and_owned_icon() {
    auto configuration = erui::detail::parse_tab_configuration(
        "[TabOrder]\n"
        "provider.z/main = -5\n");
    auto compiled = compile_menu({
        {"provider.z", "main", L"Zed", 30, -5, -5, true},
        {"provider.a", "settings", L"Alpha", 10, {}, 10, false},
        {"provider.b", "debug", L"Beta", 20, {}, 20, false},
    });
    compiled->tabs.front().metadata->icon.source =
        TabIconSource::rgba8;
    compiled->tabs.front().metadata->icon.rgba8_pixels[3] = 0;
    compiled->tabs.front().metadata->icon.rgba8_pixels[7] = 127;
    compiled->tabs.front().metadata->icon.rgba8_pixels[11] = 255;

    auto result = erui::detail::build_native_tab_catalogue(
        *compiled, configuration);
    ERUI_TEST_CHECK(result);
    ERUI_TEST_CHECK(result.catalogue->entries().size() == 11);
    ERUI_TEST_CHECK(result.catalogue->custom_count() == 3);

    const auto custom = result.catalogue->custom_entries();
    constexpr std::array expected_providers{
        "provider.z", "provider.a", "provider.b"};
    constexpr std::array expected_tabs{"main", "settings", "debug"};
    constexpr std::array expected_titles{L"Zed", L"Alpha", L"Beta"};
    constexpr std::array<std::int32_t, 3> expected_orders{-5, 10, 20};
    for (std::size_t index = 0; index < custom.size(); ++index) {
        ERUI_TEST_CHECK(custom[index].is_custom());
        ERUI_TEST_CHECK(custom[index].native_category ==
            erui::detail::invalid_native_category);
        ERUI_TEST_CHECK(custom[index].provider_id == expected_providers[index]);
        ERUI_TEST_CHECK(custom[index].tab_id == expected_tabs[index]);
        ERUI_TEST_CHECK(custom[index].effective_order == expected_orders[index]);
        ERUI_TEST_CHECK(custom[index].page_index ==
            compiled->tabs[index].page_index);
        ERUI_TEST_CHECK(custom[index].title_id ==
            compiled->pages[custom[index].page_index].title_id);
        ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
            static_cast<int>(custom[index].title_id))) ==
            expected_titles[index]);
    }
    ERUI_TEST_CHECK(custom.front().player_order == -5);
    ERUI_TEST_CHECK(custom.front().preferred_order == 30);
    ERUI_TEST_CHECK(custom.front().icon.source ==
        TabIconSource::rgba8);
    ERUI_TEST_CHECK(custom.front().icon.rgba8_pixels.size() ==
        ERUI_TAB_ICON_RGBA8_BYTE_COUNT);
    ERUI_TEST_CHECK(custom.front().icon.rgba8_pixels[3] == 0);
    ERUI_TEST_CHECK(custom.front().icon.rgba8_pixels[7] == 127);
    ERUI_TEST_CHECK(custom.front().icon.rgba8_pixels[11] == 255);

    // The runtime catalogue owns its icon bytes independently of the compiled
    // provider model it was built from.
    compiled->tabs.front().metadata->icon.rgba8_pixels[0] = 99;
    ERUI_TEST_CHECK(custom.front().icon.rgba8_pixels[0] == 17);
}

void test_public_custom_tab_limit() {
    std::vector<TabSpec> at_limit{};
    at_limit.reserve(ERUI_CUSTOM_TAB_MAX_COUNT);
    for (std::size_t index = 0;
         index < ERUI_CUSTOM_TAB_MAX_COUNT;
         ++index) {
        at_limit.push_back({
            "provider.limit",
            "tab" + std::to_string(index),
            L"Limit tab " + std::to_wstring(index),
            static_cast<std::int32_t>(index),
            {},
            static_cast<std::int32_t>(index),
            false,
        });
    }
    auto compiled = compile_menu(at_limit);
    auto accepted = erui::detail::build_native_tab_catalogue(
        *compiled, default_configuration());
    ERUI_TEST_CHECK(accepted);
    ERUI_TEST_CHECK(accepted.catalogue->custom_count() ==
        ERUI_CUSTOM_TAB_MAX_COUNT);
    ERUI_TEST_CHECK(accepted.catalogue->custom_entries()[255].tab_id ==
        "tab255");
    ERUI_TEST_CHECK(accepted.catalogue->custom_entries()[256].tab_id ==
        "tab256");
    ERUI_TEST_CHECK(accepted.catalogue->custom_entries().back().tab_id ==
        "tab1023");

    at_limit.push_back({
        "provider.limit", "overflow", L"Overflow", 100, {}, 100, false});
    compiled = compile_menu(at_limit);
    expect_error(
        erui::detail::build_native_tab_catalogue(
            *compiled, default_configuration()),
        NativeTabCatalogueError::custom_tab_limit_exceeded);
}

std::unique_ptr<erui::detail::CompiledMenu> one_tab_menu() {
    return compile_menu({
        {"provider.valid", "main", L"Valid", 10, {}, 10, false},
    });
}

void test_structural_rejections() {
    erui::detail::CompiledMenu unfinished{};
    expect_error(
        erui::detail::build_native_tab_catalogue(
            unfinished, default_configuration()),
        NativeTabCatalogueError::menu_not_frozen);

    {
        auto compiled = one_tab_menu();
        compiled->root_page_index = compiled->pages.size();
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::invalid_root_page);
    }
    {
        auto compiled = one_tab_menu();
        compiled->builtin_page_indices[4] = compiled->root_page_index;
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::invalid_builtin_page);
    }
    {
        auto compiled = one_tab_menu();
        compiled->tab_page_indices.clear();
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::tab_index_list_mismatch);
    }
    {
        auto compiled = one_tab_menu();
        compiled->tabs[0].page_index = compiled->pages.size();
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::invalid_tab_page_index,
            0);
    }
    {
        auto compiled = compile_menu({
            {"provider.valid", "a", L"A", 1, {}, 1, false},
            {"provider.valid", "b", L"B", 2, {}, 2, false},
        });
        compiled->tabs[1].page_index = compiled->tabs[0].page_index;
        compiled->tab_page_indices[1] = compiled->tabs[0].page_index;
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::duplicate_tab_page_index,
            1);
    }
    {
        auto compiled = one_tab_menu();
        compiled->pages[compiled->tabs[0].page_index].kind =
            erui::PageKind::submenu;
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::tab_page_kind_mismatch,
            0);
    }
    {
        auto compiled = one_tab_menu();
        compiled->reachable_pages[compiled->tabs[0].page_index] = false;
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::tab_page_unreachable,
            0);
    }
    {
        auto compiled = one_tab_menu();
        compiled->tabs[0].metadata.reset();
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::missing_tab_metadata,
            0);
    }
}

void test_metadata_rejections() {
    {
        auto compiled = one_tab_menu();
        compiled->tabs[0].metadata->provider_id = "invalid/provider";
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::invalid_tab_identifier,
            0);
    }
    {
        auto compiled = compile_menu({
            {"provider.valid", "a", L"A", 1, {}, 1, false},
            {"provider.valid", "b", L"B", 2, {}, 2, false},
        });
        compiled->tabs[1].metadata->tab_id = "a";
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::duplicate_tab_identifier,
            1);
    }
    {
        auto compiled = one_tab_menu();
        compiled->pages[compiled->tabs[0].page_index].title_id = 0;
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::invalid_tab_title,
            0);
    }
    {
        auto compiled = one_tab_menu();
        compiled->tabs[0].metadata->icon.rgba8_pixels.push_back(1);
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::invalid_tab_icon,
            0);
    }
}

void test_order_and_configuration_rejections() {
    {
        auto compiled = compile_menu({
            {"provider.valid", "later", L"Later", 20, {}, 20, false},
            {"provider.valid", "earlier", L"Earlier", 10, {}, 10, false},
        });
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, default_configuration()),
            NativeTabCatalogueError::tab_order_mismatch,
            0);
    }
    {
        auto compiled = one_tab_menu();
        const auto changed_configuration =
            erui::detail::parse_tab_configuration(
                "[TabOrder]\nprovider.valid/main = -100\n");
        expect_error(
            erui::detail::build_native_tab_catalogue(
                *compiled, changed_configuration),
            NativeTabCatalogueError::tab_order_mismatch,
            0);
    }
    for (const std::uint32_t accepted : std::array{8u, 9u, 10u}) {
        auto compiled = one_tab_menu();
        auto configuration = default_configuration();
        configuration.settings.minimum_tab_slots = accepted;
        const auto result = erui::detail::build_native_tab_catalogue(
            *compiled, configuration);
        ERUI_TEST_CHECK(result);
        ERUI_TEST_CHECK(
            result.catalogue->navigation_settings().minimum_tab_slots ==
            accepted);
    }
    for (const std::uint32_t rejected : std::array{7u, 11u, 14u}) {
        auto compiled = one_tab_menu();
        auto invalid = default_configuration();
        invalid.settings.minimum_tab_slots = rejected;
        expect_error(
            erui::detail::build_native_tab_catalogue(*compiled, invalid),
            NativeTabCatalogueError::invalid_configuration);
    }
    {
        auto compiled = one_tab_menu();
        auto invalid = default_configuration();
        invalid.settings.mouse_dwell_timing.minimum_repeat_delay_ms =
            invalid.settings.mouse_dwell_timing.repeat_delay_ms + 1;
        expect_error(
            erui::detail::build_native_tab_catalogue(*compiled, invalid),
            NativeTabCatalogueError::invalid_configuration);
    }
    {
        auto compiled = one_tab_menu();
        auto invalid = default_configuration();
        erui::detail::TabOrderOverride bad{};
        bad.provider_id = "provider.valid";
        bad.tab_id = "main";
        bad.stable_key = "different/key";
        invalid.order_overrides.emplace("provider.valid/main", bad);
        expect_error(
            erui::detail::build_native_tab_catalogue(*compiled, invalid),
            NativeTabCatalogueError::invalid_configuration);
    }
}

} // namespace

static_assert(noexcept(erui::detail::build_native_tab_catalogue(
    std::declval<const erui::detail::CompiledMenu&>(),
    std::declval<const erui::detail::TabConfiguration&>())));
static_assert(noexcept(erui::detail::native_tab_catalogue_error_name(
    erui::detail::NativeTabCatalogueError::none)));

int main() {
    test_stock_catalogue_and_settings();
    test_custom_order_identity_title_and_owned_icon();
    test_public_custom_tab_limit();
    test_structural_rejections();
    test_metadata_rejections();
    test_order_and_configuration_rejections();
    return 0;
}
