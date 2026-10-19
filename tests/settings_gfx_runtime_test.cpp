#include "gfx/settings_gfx_runtime.hpp"

#include "support/settings_gfx_fixture.hpp"
#include "test_assertions.hpp"

#include <algorithm>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace fixture = erui::test::settings_gfx_fixture;
using erui::gfx::GfxDocument;
using erui::gfx::GfxRawTag;
using erui::gfx::SettingsGfxHost;
using erui::gfx::SettingsGfxPresentation;
using erui::gfx::SettingsGfxRuntimeCache;
using erui::gfx::SettingsGfxRuntimeErrorCode;
using erui::gfx::SettingsGfxRuntimeOutcome;
using erui::gfx::SettingsGfxRuntimeResult;
using erui::gfx::SettingsGfxRuntimeTarget;

constexpr std::string_view kGameOptionsPath =
    "data0:/menu/win/02_040_optionsetting.gfx";
constexpr std::string_view kAdvancedSettingsPath =
    "data0:/menu/win/02_042_pc_graphicsetting.gfx";

[[nodiscard]] std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    ERUI_TEST_CHECK(stream.good());
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::vector<std::uint8_t> make_complete_game_fixture(
    fixture::Options options = {}) {
    options.include_all_settings_panels = true;
    return fixture::make(options);
}

[[nodiscard]] GfxDocument parse_replacement(
    const SettingsGfxRuntimeResult& result) {
    ERUI_TEST_CHECK(result.has_replacement());
    const auto parsed = GfxDocument::parse(std::span<const std::uint8_t>{
        result.replacement_bytes,
        result.replacement_size});
    ERUI_TEST_CHECK(parsed.success());
    return parsed.document;
}

[[nodiscard]] GfxRawTag definition_tag(
    const GfxDocument& document,
    std::uint16_t character_id) {
    const auto definition = document.character_definition(character_id);
    ERUI_TEST_CHECK(definition.has_value());
    auto raw = document.copy_tag(definition->tag);
    ERUI_TEST_CHECK(raw.success());
    return std::move(raw.tag);
}

[[nodiscard]] GfxRawTag unique_tag(
    const GfxDocument& document,
    std::uint16_t code) {
    std::optional<GfxRawTag> result{};
    for (const auto& tag : document.tags()) {
        if (tag.code != code) continue;
        ERUI_TEST_CHECK(!result.has_value());
        auto raw = document.copy_tag(tag.handle);
        ERUI_TEST_CHECK(raw.success());
        result = std::move(raw.tag);
    }
    ERUI_TEST_CHECK(result.has_value());
    return std::move(*result);
}

[[nodiscard]] bool raw_equal(
    const GfxRawTag& left,
    const GfxRawTag& right) {
    return left.code == right.code &&
        left.header_form == right.header_form &&
        left.body == right.body;
}

[[nodiscard]] bool has_named_placement(
    const GfxDocument& document,
    std::uint16_t owner_character_id,
    std::string_view name) {
    const auto owner = document.sprite_by_character_id(owner_character_id);
    ERUI_TEST_CHECK(owner.has_value());
    return std::any_of(
        document.placements().begin(),
        document.placements().end(),
        [&](const auto& placement) {
            return placement.owner_sprite == owner->definition &&
                placement.has_name && placement.name == name;
        });
}

[[nodiscard]] std::uint16_t unique_named_character(
    const GfxDocument& document,
    std::string_view name) {
    std::optional<std::uint16_t> result{};
    for (const auto& placement : document.placements()) {
        if (!placement.has_name || placement.name != name) continue;
        ERUI_TEST_CHECK(placement.has_character);
        ERUI_TEST_CHECK(!result.has_value());
        result = placement.character_id;
    }
    ERUI_TEST_CHECK(result.has_value());
    return *result;
}

[[nodiscard]] GfxRawTag named_placement_tag(
    const GfxDocument& document,
    std::uint16_t owner_character_id,
    std::string_view name) {
    const auto owner = document.sprite_by_character_id(owner_character_id);
    ERUI_TEST_CHECK(owner.has_value());
    std::optional<GfxRawTag> result{};
    for (const auto& placement : document.placements()) {
        if (placement.owner_sprite != owner->definition ||
            !placement.has_name || placement.name != name) {
            continue;
        }
        ERUI_TEST_CHECK(!result.has_value());
        auto copied = document.copy_tag(placement.tag);
        ERUI_TEST_CHECK(copied.success());
        result = std::move(copied.tag);
    }
    ERUI_TEST_CHECK(result.has_value());
    return std::move(*result);
}

void check_success(const SettingsGfxRuntimeResult& result) {
    if (!result.success()) {
        std::fprintf(
            stderr,
            "unexpected runtime GFX failure: target=%s outcome=%s error=%s "
            "document=%s settings=%s/%s detail=%u\n",
            erui::gfx::settings_gfx_runtime_target_name(result.target),
            erui::gfx::settings_gfx_runtime_outcome_name(result.outcome),
            erui::gfx::settings_gfx_runtime_error_name(result.error.code),
            erui::gfx::gfx_document_error_name(
                result.error.document_error.code),
            erui::gfx::settings_gfx_error_name(
                result.error.settings_error.code),
            erui::gfx::settings_gfx_feature_name(
                result.error.settings_error.feature),
            result.error.settings_error.detail);
    }
    ERUI_TEST_CHECK(result.success());
}

void test_exact_path_classification_and_policies() {
    using erui::gfx::classify_settings_gfx_runtime_path;
    using erui::gfx::settings_gfx_runtime_requirements;

    ERUI_TEST_CHECK(
        classify_settings_gfx_runtime_path(kGameOptionsPath) ==
        SettingsGfxRuntimeTarget::game_options);
    ERUI_TEST_CHECK(
        classify_settings_gfx_runtime_path(kAdvancedSettingsPath) ==
        SettingsGfxRuntimeTarget::advanced_settings);

    constexpr std::array<std::string_view, 7> near_matches{
        "",
        "menu/win/02_040_optionsetting.gfx",
        "data0:/menu/win/02_040_optionsetting.gfx.bak",
        "data0:/menu/win/02_040_optionsetting.GFX",
        "data0:\\menu\\win\\02_040_optionsetting.gfx",
        "data0:/menu/win/02_042_pc_graphicsetting.gfx/extra",
        "data0:/menu/win/x02_042_pc_graphicsetting.gfx",
    };
    for (const std::string_view path : near_matches) {
        ERUI_TEST_CHECK(
            classify_settings_gfx_runtime_path(path) ==
            SettingsGfxRuntimeTarget::none);
    }

    const auto game = settings_gfx_runtime_requirements(
        SettingsGfxRuntimeTarget::game_options);
    ERUI_TEST_CHECK(game.minimum_game_options_rows == 13);
    ERUI_TEST_CHECK(game.minimum_camera_options_rows == 13);
    ERUI_TEST_CHECK(game.minimum_display_rows == 13);
    ERUI_TEST_CHECK(game.minimum_sound_rows == 13);
    ERUI_TEST_CHECK(game.minimum_network_rows == 10);
    ERUI_TEST_CHECK(game.minimum_keyboard_mouse_rows == 13);
    ERUI_TEST_CHECK(game.minimum_graphics_rows == 13);
    ERUI_TEST_CHECK(game.minimum_tab_slots == 8);
    ERUI_TEST_CHECK(game.ensure_button_label);
    ERUI_TEST_CHECK(
        game.text_input ==
        SettingsGfxPresentation::ensure_character_creation);
    ERUI_TEST_CHECK(
        game.color_picker ==
        SettingsGfxPresentation::ensure_character_creation);

    const auto advanced = settings_gfx_runtime_requirements(
        SettingsGfxRuntimeTarget::advanced_settings);
    ERUI_TEST_CHECK(!advanced.minimum_game_options_rows.has_value());
    ERUI_TEST_CHECK(!advanced.minimum_camera_options_rows.has_value());
    ERUI_TEST_CHECK(!advanced.minimum_tab_slots.has_value());
    ERUI_TEST_CHECK(!advanced.ensure_button_label);
    ERUI_TEST_CHECK(
        advanced.text_input ==
        SettingsGfxPresentation::ensure_character_creation);
    ERUI_TEST_CHECK(
        advanced.color_picker ==
        SettingsGfxPresentation::ensure_character_creation);

    const auto none = settings_gfx_runtime_requirements(
        SettingsGfxRuntimeTarget::none);
    ERUI_TEST_CHECK(!none.minimum_game_options_rows.has_value());
    ERUI_TEST_CHECK(!none.minimum_camera_options_rows.has_value());
    ERUI_TEST_CHECK(!none.minimum_tab_slots.has_value());
    ERUI_TEST_CHECK(!none.ensure_button_label);
    ERUI_TEST_CHECK(none.text_input == SettingsGfxPresentation::preserve);
    ERUI_TEST_CHECK(none.color_picker == SettingsGfxPresentation::preserve);

    SettingsGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.ready());
    const std::array<std::uint8_t, 1> malformed{0xFF};
    const auto ignored = cache.prepare_path(
        "data0:/menu/win/not-a-target.gfx", malformed);
    ERUI_TEST_CHECK(ignored.success());
    ERUI_TEST_CHECK(ignored.target == SettingsGfxRuntimeTarget::none);
    ERUI_TEST_CHECK(ignored.outcome == SettingsGfxRuntimeOutcome::ignored);
    ERUI_TEST_CHECK(!ignored.cache_hit);
    ERUI_TEST_CHECK(!ignored.has_replacement());

    const auto invalid = cache.prepare(
        SettingsGfxRuntimeTarget::none, malformed);
    ERUI_TEST_CHECK(!invalid.success());
    ERUI_TEST_CHECK(invalid.outcome == SettingsGfxRuntimeOutcome::failed);
    ERUI_TEST_CHECK(
        invalid.error.code == SettingsGfxRuntimeErrorCode::invalid_target);
}

void test_game_options_policy_transforms_complete_movie() {
    const std::vector<std::uint8_t> source = make_complete_game_fixture();
    const std::vector<std::uint8_t> original = source;
    SettingsGfxRuntimeCache cache{};

    const auto result = cache.prepare_path(kGameOptionsPath, source);
    check_success(result);
    ERUI_TEST_CHECK(
        result.target == SettingsGfxRuntimeTarget::game_options);
    ERUI_TEST_CHECK(
        result.outcome == SettingsGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(!result.cache_hit);
    ERUI_TEST_CHECK(result.has_replacement());
    ERUI_TEST_CHECK(result.before.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(result.before.game_options_rows == 6);
    ERUI_TEST_CHECK(result.before.camera_options_rows == 7);
    ERUI_TEST_CHECK(result.before.display_rows == 12);
    ERUI_TEST_CHECK(result.before.sound_rows == 12);
    ERUI_TEST_CHECK(result.before.network_rows == 7);
    ERUI_TEST_CHECK(result.before.keyboard_mouse_rows == 7);
    ERUI_TEST_CHECK(result.before.graphics_rows == 7);
    ERUI_TEST_CHECK(result.before.authored_tab_slot_capacity == 8);
    ERUI_TEST_CHECK(result.after.game_options_rows == 13);
    ERUI_TEST_CHECK(result.after.camera_options_rows == 13);
    ERUI_TEST_CHECK(result.after.display_rows == 13);
    ERUI_TEST_CHECK(result.after.sound_rows == 13);
    ERUI_TEST_CHECK(result.after.network_rows == 10);
    ERUI_TEST_CHECK(result.after.keyboard_mouse_rows == 13);
    ERUI_TEST_CHECK(result.after.graphics_rows == 13);
    ERUI_TEST_CHECK(result.after.authored_tab_slot_capacity == 8);
    ERUI_TEST_CHECK(result.after.character_name_text_input);
    ERUI_TEST_CHECK(result.after.character_creation_color_picker);
    ERUI_TEST_CHECK(source == original);

    const GfxDocument replacement = parse_replacement(result);
    const auto inspection = erui::gfx::inspect_settings_gfx(replacement);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(
        inspection.inspection.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(inspection.inspection.game_options_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.camera_options_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.display_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.sound_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.network_rows == 10);
    ERUI_TEST_CHECK(inspection.inspection.keyboard_mouse_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.graphics_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.authored_tab_slot_capacity == 8);
    ERUI_TEST_CHECK(inspection.inspection.character_name_text_input);
    ERUI_TEST_CHECK(
        inspection.inspection.character_creation_color_picker);
}

void test_game_options_policy_uses_configured_minimum_tab_slots() {
    const std::vector<std::uint8_t> source = make_complete_game_fixture();
    SettingsGfxRuntimeCache cache{9};

    const auto result = cache.prepare_path(kGameOptionsPath, source);
    check_success(result);
    ERUI_TEST_CHECK(
        result.outcome == SettingsGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(result.before.authored_tab_slot_capacity == 8);
    ERUI_TEST_CHECK(result.after.authored_tab_slot_capacity == 9);
    ERUI_TEST_CHECK(result.after.tab_list_cells == 10);
    ERUI_TEST_CHECK(result.after.back_tab_list_cells == 10);

    const GfxDocument replacement = parse_replacement(result);
    const auto inspection = erui::gfx::inspect_settings_gfx(replacement);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(inspection.inspection.authored_tab_slot_capacity == 9);
}

void test_advanced_policy_transforms_widgets_only() {
    fixture::Options options{};
    options.advanced_widget_bindings = true;
    const std::vector<std::uint8_t> source =
        make_complete_game_fixture(options);
    SettingsGfxRuntimeCache cache{};

    const auto result = cache.prepare_path(kAdvancedSettingsPath, source);
    check_success(result);
    ERUI_TEST_CHECK(
        result.target == SettingsGfxRuntimeTarget::advanced_settings);
    ERUI_TEST_CHECK(
        result.outcome == SettingsGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(!result.cache_hit);
    ERUI_TEST_CHECK(result.has_replacement());
    ERUI_TEST_CHECK(result.before.host == SettingsGfxHost::advanced_settings);
    ERUI_TEST_CHECK(result.after.host == SettingsGfxHost::advanced_settings);
    ERUI_TEST_CHECK(result.after.character_name_text_input);
    ERUI_TEST_CHECK(result.after.character_creation_color_picker);

    const GfxDocument replacement = parse_replacement(result);
    const auto inspection =
        erui::gfx::inspect_settings_widgets_gfx(replacement);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(
        inspection.inspection.host == SettingsGfxHost::advanced_settings);
    ERUI_TEST_CHECK(inspection.inspection.character_name_text_input);
    ERUI_TEST_CHECK(
        inspection.inspection.character_creation_color_picker);
}

void test_advanced_policy_preserves_existing_fifteen_row_structures() {
    fixture::Options options{};
    options.game_rows = 15;
    options.camera_rows = 15;
    options.advanced_widget_bindings = true;
    const std::vector<std::uint8_t> source =
        make_complete_game_fixture(options);
    const auto parsed_source = GfxDocument::parse(source);
    ERUI_TEST_CHECK(parsed_source.success());
    const GfxRawTag game_before = definition_tag(
        parsed_source.document, options.ids.game_options);
    const GfxRawTag camera_before = definition_tag(
        parsed_source.document, options.ids.camera_options);
    const GfxRawTag window_before = definition_tag(
        parsed_source.document, options.ids.window_list);

    SettingsGfxRuntimeCache cache{};
    const auto result = cache.prepare_path(kAdvancedSettingsPath, source);
    check_success(result);
    ERUI_TEST_CHECK(result.has_replacement());
    const GfxDocument replacement = parse_replacement(result);

    ERUI_TEST_CHECK(raw_equal(
        game_before,
        definition_tag(replacement, options.ids.game_options)));
    ERUI_TEST_CHECK(raw_equal(
        camera_before,
        definition_tag(replacement, options.ids.camera_options)));
    ERUI_TEST_CHECK(raw_equal(
        window_before,
        definition_tag(replacement, options.ids.window_list)));
}

void test_runtime_policy_completes_partially_modified_movie() {
    fixture::Options options{};
    options.game_rows = 10;
    options.camera_rows = 12;
    options.noncanonical_donor_row = true;
    options.omit_button_label = true;
    const std::vector<std::uint8_t> source =
        make_complete_game_fixture(options);
    const auto parsed_source = GfxDocument::parse(source);
    ERUI_TEST_CHECK(parsed_source.success());
    const GfxRawTag unrelated_before = unique_tag(
        parsed_source.document, fixture::unknown_root_tag_code);

    SettingsGfxRuntimeCache cache{};
    const auto result = cache.prepare_path(kGameOptionsPath, source);
    check_success(result);
    ERUI_TEST_CHECK(result.has_replacement());
    ERUI_TEST_CHECK(result.before.game_options_rows == 10);
    ERUI_TEST_CHECK(result.before.camera_options_rows == 12);
    ERUI_TEST_CHECK(result.after.game_options_rows == 13);
    ERUI_TEST_CHECK(result.after.camera_options_rows == 13);

    const GfxDocument replacement = parse_replacement(result);
    ERUI_TEST_CHECK(has_named_placement(
        replacement, options.ids.button, "Text_1"));
    ERUI_TEST_CHECK(raw_equal(
        unrelated_before,
        unique_tag(replacement, fixture::unknown_root_tag_code)));
    ERUI_TEST_CHECK(replacement.trailing_size() == options.trailer.size());
    ERUI_TEST_CHECK(std::equal(
        options.trailer.begin(),
        options.trailer.end(),
        result.replacement_bytes +
            (result.replacement_size - options.trailer.size())));
}

void test_cached_replacement_is_stable_and_source_change_fails_open() {
    const std::vector<std::uint8_t> source = make_complete_game_fixture();
    SettingsGfxRuntimeCache cache{};

    const auto first = cache.prepare(
        SettingsGfxRuntimeTarget::game_options, source);
    ERUI_TEST_CHECK(first.success());
    ERUI_TEST_CHECK(first.has_replacement());
    ERUI_TEST_CHECK(!first.cache_hit);
    const std::vector<std::uint8_t> published(
        first.replacement_bytes,
        first.replacement_bytes + first.replacement_size);

    const std::vector<std::uint8_t> equal_source = source;
    const auto second = cache.prepare(
        SettingsGfxRuntimeTarget::game_options, equal_source);
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(second.cache_hit);
    ERUI_TEST_CHECK(second.has_replacement());
    ERUI_TEST_CHECK(second.replacement_bytes == first.replacement_bytes);
    ERUI_TEST_CHECK(second.replacement_size == first.replacement_size);

    // The native repository can return the same MemoryFile after its view was
    // already redirected. Feeding the exact published bytes back into the
    // cache must remain idempotent and retain the same process-lifetime
    // address.
    const auto published_again = cache.prepare(
        SettingsGfxRuntimeTarget::game_options, published);
    ERUI_TEST_CHECK(published_again.success());
    ERUI_TEST_CHECK(published_again.cache_hit);
    ERUI_TEST_CHECK(published_again.has_replacement());
    ERUI_TEST_CHECK(
        published_again.replacement_bytes == first.replacement_bytes);

    std::vector<std::uint8_t> changed_source = source;
    ERUI_TEST_CHECK(!changed_source.empty());
    changed_source.back() ^= 0x5AU;
    const auto changed = cache.prepare(
        SettingsGfxRuntimeTarget::game_options, changed_source);
    ERUI_TEST_CHECK(!changed.success());
    ERUI_TEST_CHECK(
        changed.outcome == SettingsGfxRuntimeOutcome::failed);
    ERUI_TEST_CHECK(
        changed.error.code == SettingsGfxRuntimeErrorCode::source_changed);
    ERUI_TEST_CHECK(!changed.has_replacement());

    const auto after_failure = cache.prepare(
        SettingsGfxRuntimeTarget::game_options, source);
    ERUI_TEST_CHECK(after_failure.success());
    ERUI_TEST_CHECK(after_failure.cache_hit);
    ERUI_TEST_CHECK(
        after_failure.replacement_bytes == first.replacement_bytes);
    ERUI_TEST_CHECK(after_failure.replacement_size == published.size());
    ERUI_TEST_CHECK(std::equal(
        published.begin(),
        published.end(),
        after_failure.replacement_bytes));
}

void test_runtime_input_limit_fails_before_parsing() {
    std::vector<std::uint8_t> oversized(
        erui::gfx::kSettingsGfxRuntimeMaximumBytes + 1U,
        0xA5U);
    SettingsGfxRuntimeCache cache{};
    const auto result = cache.prepare(
        SettingsGfxRuntimeTarget::game_options, oversized);
    ERUI_TEST_CHECK(!result.success());
    ERUI_TEST_CHECK(result.outcome == SettingsGfxRuntimeOutcome::failed);
    ERUI_TEST_CHECK(
        result.error.code == SettingsGfxRuntimeErrorCode::input_too_large);
    ERUI_TEST_CHECK(!result.cache_hit);
    ERUI_TEST_CHECK(!result.has_replacement());
}

void test_malformed_and_wrong_host_fail_without_replacement() {
    {
        SettingsGfxRuntimeCache cache{};
        const std::array<std::uint8_t, 4> malformed{
            0x00, 0x11, 0x22, 0x33};
        const auto first = cache.prepare_path(kGameOptionsPath, malformed);
        ERUI_TEST_CHECK(!first.success());
        ERUI_TEST_CHECK(
            first.outcome == SettingsGfxRuntimeOutcome::failed);
        ERUI_TEST_CHECK(
            first.error.code == SettingsGfxRuntimeErrorCode::parse_failed);
        ERUI_TEST_CHECK(!first.cache_hit);
        ERUI_TEST_CHECK(!first.has_replacement());

        const auto second = cache.prepare_path(kGameOptionsPath, malformed);
        ERUI_TEST_CHECK(!second.success());
        ERUI_TEST_CHECK(second.cache_hit);
        ERUI_TEST_CHECK(
            second.error.code == SettingsGfxRuntimeErrorCode::parse_failed);
        ERUI_TEST_CHECK(!second.has_replacement());
    }

    {
        SettingsGfxRuntimeCache cache{};
        const std::vector<std::uint8_t> game_host = fixture::make();
        const auto result = cache.prepare(
            SettingsGfxRuntimeTarget::advanced_settings, game_host);
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(
            result.outcome == SettingsGfxRuntimeOutcome::failed);
        ERUI_TEST_CHECK(
            result.error.code ==
            SettingsGfxRuntimeErrorCode::unexpected_host);
        ERUI_TEST_CHECK(!result.has_replacement());
    }
}

void test_checked_in_assets_complete_and_preserve_specialized_panels() {
    const auto game = read_file(ERNATIVEUI_TEST_GAME_OPTIONS_GFX);
    const auto advanced = read_file(ERNATIVEUI_TEST_ADVANCED_GFX);
    const auto parsed_game = GfxDocument::parse(game);
    ERUI_TEST_CHECK(parsed_game.success());
    const std::uint16_t brightness = unique_named_character(
        parsed_game.document, "BrightnessSetting");
    const std::uint16_t controller = unique_named_character(
        parsed_game.document, "PadSetting");
    const std::uint16_t network = unique_named_character(
        parsed_game.document, "NetworkSetting");
    const GfxRawTag brightness_before = definition_tag(
        parsed_game.document, brightness);
    const GfxRawTag controller_before = definition_tag(
        parsed_game.document, controller);
    const GfxRawTag ugc_restricted_before = named_placement_tag(
        parsed_game.document, network, "UgcRestricted");
    SettingsGfxRuntimeCache cache{};

    const auto game_result = cache.prepare_path(kGameOptionsPath, game);
    ERUI_TEST_CHECK(game_result.success());
    ERUI_TEST_CHECK(
        game_result.outcome == SettingsGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(!game_result.cache_hit);
    ERUI_TEST_CHECK(game_result.has_replacement());
    ERUI_TEST_CHECK(game_result.after.game_options_rows == 13);
    ERUI_TEST_CHECK(game_result.after.camera_options_rows == 13);
    ERUI_TEST_CHECK(game_result.after.display_rows == 13);
    ERUI_TEST_CHECK(game_result.after.sound_rows == 13);
    ERUI_TEST_CHECK(game_result.after.network_rows == 10);
    ERUI_TEST_CHECK(game_result.after.keyboard_mouse_rows == 13);
    ERUI_TEST_CHECK(game_result.after.graphics_rows == 13);
    ERUI_TEST_CHECK(game_result.after.character_name_text_input);
    ERUI_TEST_CHECK(
        game_result.after.character_creation_color_picker);
    const GfxDocument completed_game = parse_replacement(game_result);
    ERUI_TEST_CHECK(raw_equal(
        brightness_before,
        definition_tag(completed_game, brightness)));
    ERUI_TEST_CHECK(raw_equal(
        controller_before,
        definition_tag(completed_game, controller)));
    ERUI_TEST_CHECK(raw_equal(
        ugc_restricted_before,
        named_placement_tag(
            completed_game, network, "UgcRestricted")));

    const auto advanced_result = cache.prepare_path(
        kAdvancedSettingsPath, advanced);
    ERUI_TEST_CHECK(advanced_result.success());
    ERUI_TEST_CHECK(
        advanced_result.outcome == SettingsGfxRuntimeOutcome::unchanged);
    ERUI_TEST_CHECK(!advanced_result.cache_hit);
    ERUI_TEST_CHECK(!advanced_result.has_replacement());
    ERUI_TEST_CHECK(
        advanced_result.after.host == SettingsGfxHost::advanced_settings);
    ERUI_TEST_CHECK(advanced_result.after.character_name_text_input);
    ERUI_TEST_CHECK(
        advanced_result.after.character_creation_color_picker);

    const auto game_cached = cache.prepare_path(kGameOptionsPath, game);
    const auto advanced_cached = cache.prepare_path(
        kAdvancedSettingsPath, advanced);
    ERUI_TEST_CHECK(game_cached.success());
    ERUI_TEST_CHECK(advanced_cached.success());
    ERUI_TEST_CHECK(game_cached.cache_hit);
    ERUI_TEST_CHECK(advanced_cached.cache_hit);
    ERUI_TEST_CHECK(game_cached.has_replacement());
    ERUI_TEST_CHECK(!advanced_cached.has_replacement());
}

void test_concurrent_same_source_publishes_one_stable_replacement() {
    constexpr std::size_t kThreadCount = 16;
    const std::vector<std::uint8_t> source = make_complete_game_fixture();
    SettingsGfxRuntimeCache cache{};
    std::array<SettingsGfxRuntimeResult, kThreadCount> results{};
    std::barrier start{static_cast<std::ptrdiff_t>(kThreadCount)};
    std::vector<std::thread> threads{};
    threads.reserve(kThreadCount);

    for (std::size_t index = 0; index < kThreadCount; ++index) {
        threads.emplace_back([&, index] {
            start.arrive_and_wait();
            results[index] = cache.prepare(
                SettingsGfxRuntimeTarget::game_options, source);
        });
    }
    for (std::thread& thread : threads) thread.join();

    const std::uint8_t* published{};
    std::size_t published_size{};
    std::size_t first_publications{};
    for (const SettingsGfxRuntimeResult& result : results) {
        ERUI_TEST_CHECK(result.success());
        ERUI_TEST_CHECK(
            result.outcome == SettingsGfxRuntimeOutcome::transformed);
        ERUI_TEST_CHECK(result.has_replacement());
        if (!result.cache_hit) ++first_publications;
        if (!published) {
            published = result.replacement_bytes;
            published_size = result.replacement_size;
        } else {
            ERUI_TEST_CHECK(result.replacement_bytes == published);
            ERUI_TEST_CHECK(result.replacement_size == published_size);
        }
    }
    ERUI_TEST_CHECK(first_publications == 1);

    const auto parsed = GfxDocument::parse(std::span<const std::uint8_t>{
        published,
        published_size});
    ERUI_TEST_CHECK(parsed.success());
    const auto inspection = erui::gfx::inspect_settings_gfx(parsed.document);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(inspection.inspection.game_options_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.camera_options_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.display_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.sound_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.network_rows == 10);
    ERUI_TEST_CHECK(inspection.inspection.keyboard_mouse_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.graphics_rows == 13);
    ERUI_TEST_CHECK(inspection.inspection.character_name_text_input);
    ERUI_TEST_CHECK(
        inspection.inspection.character_creation_color_picker);
}

} // namespace

int main() {
    test_exact_path_classification_and_policies();
    test_game_options_policy_transforms_complete_movie();
    test_game_options_policy_uses_configured_minimum_tab_slots();
    test_advanced_policy_transforms_widgets_only();
    test_advanced_policy_preserves_existing_fifteen_row_structures();
    test_runtime_policy_completes_partially_modified_movie();
    test_cached_replacement_is_stable_and_source_change_fails_open();
    test_runtime_input_limit_fails_before_parsing();
    test_malformed_and_wrong_host_fail_without_replacement();
    test_checked_in_assets_complete_and_preserve_specialized_panels();
    test_concurrent_same_source_publishes_one_stable_replacement();
    std::puts("settings GFX runtime tests passed");
    return 0;
}
