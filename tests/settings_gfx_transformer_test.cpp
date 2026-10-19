#include "gfx/settings_gfx_transformer.hpp"

#include "support/settings_gfx_fixture.hpp"
#include "test_assertions.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using erui::gfx::GfxDocument;
using erui::gfx::GfxDocumentBytesResult;
using erui::gfx::GfxEdit;
using erui::gfx::GfxRawTag;
using erui::gfx::SettingsGfxErrorCode;
using erui::gfx::SettingsGfxFeature;
using erui::gfx::SettingsGfxHost;
using erui::gfx::SettingsGfxPresentation;
using erui::gfx::SettingsGfxRequirements;
using erui::gfx::SettingsGfxTransformStatus;
namespace fixture = erui::test::settings_gfx_fixture;

[[nodiscard]] GfxDocument parse(
    std::span<const std::uint8_t> bytes) {
    auto result = GfxDocument::parse(bytes);
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.document.valid());
    return std::move(result.document);
}

[[nodiscard]] std::vector<std::uint8_t> serialize(
    const GfxDocument& document) {
    GfxDocumentBytesResult result = document.serialize_and_validate();
    ERUI_TEST_CHECK(result.success());
    return std::move(result.bytes);
}

[[nodiscard]] std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    ERUI_TEST_CHECK(stream.good());
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::vector<std::uint8_t> body_for_unique_tag(
    const GfxDocument& document,
    std::uint16_t code) {
    std::vector<std::uint8_t> body{};
    std::size_t matches{};
    for (const auto& info : document.tags()) {
        if (info.code != code) continue;
        const auto raw = document.copy_tag(info.handle);
        ERUI_TEST_CHECK(raw.success());
        body = raw.tag.body;
        ++matches;
    }
    ERUI_TEST_CHECK(matches == 1);
    return body;
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

[[nodiscard]] const erui::gfx::GfxPlacementInfo* named_placement(
    const GfxDocument& document,
    std::uint16_t owner_character_id,
    std::string_view name) {
    const auto owner = document.sprite_by_character_id(owner_character_id);
    ERUI_TEST_CHECK(owner.has_value());
    const erui::gfx::GfxPlacementInfo* result{};
    for (const auto& placement : document.placements()) {
        if (placement.owner_sprite != owner->definition ||
            !placement.has_name || placement.name != name) {
            continue;
        }
        ERUI_TEST_CHECK(result == nullptr);
        result = &placement;
    }
    return result;
}

[[nodiscard]] const erui::gfx::GfxPlacementInfo* placement_at_depth(
    const GfxDocument& document,
    std::uint16_t owner_character_id,
    std::uint16_t depth) {
    const auto owner = document.sprite_by_character_id(owner_character_id);
    ERUI_TEST_CHECK(owner.has_value());
    const erui::gfx::GfxPlacementInfo* result{};
    for (const auto& placement : document.placements()) {
        if (placement.owner_sprite != owner->definition ||
            placement.depth != depth) {
            continue;
        }
        ERUI_TEST_CHECK(result == nullptr);
        result = &placement;
    }
    return result;
}

[[nodiscard]] GfxRawTag raw_tag(
    const GfxDocument& document,
    erui::gfx::GfxTagHandle handle) {
    auto result = document.copy_tag(handle);
    ERUI_TEST_CHECK(result.success());
    return std::move(result.tag);
}

[[nodiscard]] GfxRawTag decode_fixture_tag(
    std::span<const std::uint8_t> encoded) {
    ERUI_TEST_CHECK(encoded.size() >= 2);
    const std::uint16_t header = static_cast<std::uint16_t>(
        encoded[0] |
        (static_cast<std::uint16_t>(encoded[1]) << 8U));
    const std::uint16_t code = static_cast<std::uint16_t>(header >> 6U);
    const std::uint16_t short_length = header & 0x3FU;
    std::size_t offset = 2;
    std::size_t body_size = short_length;
    erui::gfx::GfxTagHeaderForm form =
        erui::gfx::GfxTagHeaderForm::short_form;
    if (short_length == 0x3FU) {
        ERUI_TEST_CHECK(encoded.size() >= 6);
        body_size = static_cast<std::size_t>(encoded[2]) |
            (static_cast<std::size_t>(encoded[3]) << 8U) |
            (static_cast<std::size_t>(encoded[4]) << 16U) |
            (static_cast<std::size_t>(encoded[5]) << 24U);
        offset = 6;
        form = erui::gfx::GfxTagHeaderForm::long_form;
    }
    ERUI_TEST_CHECK(body_size == encoded.size() - offset);
    return {
        .code = code,
        .body = std::vector<std::uint8_t>(
            encoded.begin() + static_cast<std::ptrdiff_t>(offset),
            encoded.end()),
        .header_form = form,
    };
}

[[nodiscard]] bool raw_equal(
    const GfxRawTag& left,
    const GfxRawTag& right) {
    return left.code == right.code &&
        left.header_form == right.header_form &&
        left.body == right.body;
}

[[nodiscard]] bool raw_body_equal(
    const GfxRawTag& left,
    const GfxRawTag& right) {
    return left.code == right.code && left.body == right.body;
}

[[nodiscard]] std::vector<std::uint8_t> encode_raw_tag(
    const GfxRawTag& raw) {
    return fixture::tag(
        raw.code,
        raw.body,
        raw.header_form == erui::gfx::GfxTagHeaderForm::long_form);
}

[[nodiscard]] GfxRawTag scaling_grid_for(
    const GfxDocument& document,
    std::uint16_t character_id) {
    std::optional<GfxRawTag> result{};
    for (const auto& info : document.tags()) {
        if (info.parent_sprite || info.code != 78) continue;
        const GfxRawTag candidate = raw_tag(document, info.handle);
        if (candidate.body.size() < 2) continue;
        const std::uint16_t id = static_cast<std::uint16_t>(
            candidate.body[0] |
            (static_cast<std::uint16_t>(candidate.body[1]) << 8U));
        if (id != character_id) continue;
        ERUI_TEST_CHECK(!result.has_value());
        result = candidate;
    }
    ERUI_TEST_CHECK(result.has_value());
    return std::move(*result);
}

[[nodiscard]] std::uint16_t wrapper_image_id(
    const GfxDocument& document,
    std::uint16_t wrapper_id) {
    const auto definition = document.character_definition(wrapper_id);
    ERUI_TEST_CHECK(definition.has_value());
    const GfxRawTag wrapper = raw_tag(document, definition->tag);
    ERUI_TEST_CHECK(wrapper.body.size() >= 16);
    return static_cast<std::uint16_t>(
        wrapper.body[14] |
        (static_cast<std::uint16_t>(wrapper.body[15]) << 8U));
}

[[nodiscard]] std::size_t top_level_ordinal(
    const GfxDocument& document,
    std::uint16_t character_id) {
    const auto definition = document.character_definition(character_id);
    ERUI_TEST_CHECK(definition.has_value());
    const auto info = document.tag(definition->tag);
    ERUI_TEST_CHECK(info.has_value());
    ERUI_TEST_CHECK(!info->parent_sprite);
    return info->ordinal;
}

[[nodiscard]] std::size_t top_level_ordinal(
    const GfxDocument& document,
    erui::gfx::GfxTagHandle handle) {
    const auto info = document.tag(handle);
    ERUI_TEST_CHECK(info.has_value());
    ERUI_TEST_CHECK(!info->parent_sprite);
    return info->ordinal;
}

[[nodiscard]] erui::gfx::GfxTagHandle top_level_end(
    const GfxDocument& document) {
    std::optional<erui::gfx::GfxTagHandle> result{};
    for (const auto& info : document.tags()) {
        if (info.parent_sprite || info.code != 0) continue;
        ERUI_TEST_CHECK(!result.has_value());
        result = info.handle;
    }
    ERUI_TEST_CHECK(result.has_value());
    return *result;
}

[[nodiscard]] erui::gfx::GfxTagHandle scaling_grid_handle(
    const GfxDocument& document,
    std::uint16_t character_id) {
    std::optional<erui::gfx::GfxTagHandle> result{};
    for (const auto& info : document.tags()) {
        if (info.parent_sprite || info.code != 78) continue;
        const auto raw = raw_tag(document, info.handle);
        if (raw.body.size() < 2) continue;
        const std::uint16_t id = static_cast<std::uint16_t>(
            raw.body[0] |
            (static_cast<std::uint16_t>(raw.body[1]) << 8U));
        if (id != character_id) continue;
        ERUI_TEST_CHECK(!result.has_value());
        result = info.handle;
    }
    ERUI_TEST_CHECK(result.has_value());
    return *result;
}

[[nodiscard]] erui::gfx::GfxTagHandle character_definition_handle(
    const GfxDocument& document,
    std::uint16_t character_id) {
    const auto definition = document.character_definition(character_id);
    ERUI_TEST_CHECK(definition.has_value());
    return definition->tag;
}

[[nodiscard]] std::size_t count_ascii(
    std::span<const std::uint8_t> bytes,
    std::string_view text) {
    std::size_t count{};
    auto cursor = bytes.begin();
    while (cursor != bytes.end()) {
        const auto match = std::search(
            cursor, bytes.end(), text.begin(), text.end());
        if (match == bytes.end()) break;
        ++count;
        cursor = std::next(
            match, static_cast<std::ptrdiff_t>(text.size()));
    }
    return count;
}

void check_row_depths(
    const GfxDocument& document,
    std::uint16_t owner_character_id,
    std::uint16_t row_count) {
    for (std::uint16_t index = 0; index < row_count; ++index) {
        const auto* placement = named_placement(
            document,
            owner_character_id,
            "Item_" + std::to_string(index) + "_0");
        ERUI_TEST_CHECK(placement != nullptr);
        ERUI_TEST_CHECK(placement->depth == index * 2U + 1U);
    }
}

void check_error(
    const erui::gfx::SettingsGfxError& error,
    SettingsGfxErrorCode code,
    SettingsGfxFeature feature) {
    if (error.code != code || error.feature != feature) {
        std::fprintf(
            stderr,
            "unexpected settings GFX error: code=%s feature=%s detail=%u\n",
            erui::gfx::settings_gfx_error_name(error.code),
            erui::gfx::settings_gfx_feature_name(error.feature),
            error.detail);
    }
    ERUI_TEST_CHECK(error.code == code);
    ERUI_TEST_CHECK(error.feature == feature);
}

void test_structural_inspection_and_remapped_ids() {
    fixture::Options options{};
    options.ids = {
        .game_options = 41'003,
        .camera_options = 41'117,
        .row_donor = 41'209,
        .window_list = 41'303,
        .row_item = 42'011,
        .button = 42'107,
        .widgets = 42'209,
        .text_input = 42'301,
        .text_value = 42'403,
        .text_placeholder = 42'503,
        .caption_wrapper = 42'601,
        .caption_text = 42'707,
        .cursor = 42'809,
        .combo_box = 42'901,
        .slider = 43'003,
        .nested_decoy = 60'101,
    };
    const auto bytes = fixture::make(options);
    const GfxDocument document = parse(bytes);

    const auto result = erui::gfx::inspect_settings_gfx(document);
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.inspection.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(result.inspection.gfx_version == 11);
    ERUI_TEST_CHECK(
        result.inspection.declared_length == bytes.size() - options.trailer.size());
    ERUI_TEST_CHECK(
        result.inspection.window_list_sprite == options.ids.window_list);
    ERUI_TEST_CHECK(
        result.inspection.game_options_sprite == options.ids.game_options);
    ERUI_TEST_CHECK(
        result.inspection.game_options_item_character == options.ids.row_item);
    ERUI_TEST_CHECK(result.inspection.game_options_rows == 6);
    ERUI_TEST_CHECK(
        result.inspection.camera_options_sprite == options.ids.camera_options);
    ERUI_TEST_CHECK(
        result.inspection.camera_options_item_character == options.ids.row_item);
    ERUI_TEST_CHECK(result.inspection.camera_options_rows == 7);
    ERUI_TEST_CHECK(
        result.inspection.text_input_sprite == options.ids.text_input);
    ERUI_TEST_CHECK(!result.inspection.character_name_text_input);
    ERUI_TEST_CHECK(result.inspection.color_picker_host_count == 1);
    ERUI_TEST_CHECK(!result.inspection.character_creation_color_picker);
    ERUI_TEST_CHECK(
        result.inspection.tab_list_sprite == options.ids.tab_list);
    ERUI_TEST_CHECK(
        result.inspection.back_tab_list_sprite ==
            options.ids.back_tab_list);
    ERUI_TEST_CHECK(result.inspection.tab_list_cells == 9);
    ERUI_TEST_CHECK(result.inspection.back_tab_list_cells == 9);
    ERUI_TEST_CHECK(result.inspection.authored_tab_slot_capacity == 8);
}

void test_tab_slot_growth_is_monotonic_atomic_and_idempotent() {
    fixture::Options options{};
    // A third-party-specialized visible cell must not become the donor for
    // ERNativeUI's generic trailing spare cell.
    options.specialized_tab_cell = std::uint16_t{8};
    options.decorated_back_tab_cells = true;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);
    const auto* source_left_arrow = named_placement(
        source, options.ids.tab_list, "LeftArrow");
    const auto* source_right_arrow = named_placement(
        source, options.ids.tab_list, "RightArrow");
    ERUI_TEST_CHECK(source_left_arrow != nullptr);
    ERUI_TEST_CHECK(source_right_arrow != nullptr);
    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.changed());
    ERUI_TEST_CHECK(
        transformed.status == SettingsGfxTransformStatus::modified);
    ERUI_TEST_CHECK(transformed.before.authored_tab_slot_capacity == 8);
    ERUI_TEST_CHECK(transformed.after.authored_tab_slot_capacity == 9);
    ERUI_TEST_CHECK(transformed.after.tab_list_cells == 10);
    ERUI_TEST_CHECK(transformed.after.back_tab_list_cells == 10);

    const auto* front = named_placement(
        transformed.document, options.ids.tab_list, "Item_0_9");
    const auto* back = named_placement(
        transformed.document, options.ids.back_tab_list, "Item_0_9");
    ERUI_TEST_CHECK(front != nullptr);
    ERUI_TEST_CHECK(back != nullptr);
    ERUI_TEST_CHECK(front->character_id == options.ids.tab_item);
    ERUI_TEST_CHECK(back->character_id == options.ids.back_tab_item);
    ERUI_TEST_CHECK(front->depth == 5);
    ERUI_TEST_CHECK(back->depth == 38);
    ERUI_TEST_CHECK(front->translation.x == 6440);
    ERUI_TEST_CHECK(back->translation.x == 6440);
    ERUI_TEST_CHECK(front->translation.y == -6920);
    ERUI_TEST_CHECK(back->translation.y == -6920);
    const auto front_raw = raw_tag(transformed.document, front->tag);
    const auto back_raw = raw_tag(transformed.document, back->tag);
    ERUI_TEST_CHECK(front_raw.body.size() >= 2U);
    ERUI_TEST_CHECK(back_raw.body.size() >= 2U);
    ERUI_TEST_CHECK(front_raw.body[front_raw.body.size() - 2U] == 0x3C);
    ERUI_TEST_CHECK(front_raw.body.back() == 0xC3);
    ERUI_TEST_CHECK(back_raw.body[back_raw.body.size() - 2U] == 0xA5);
    ERUI_TEST_CHECK(back_raw.body.back() == 0x5A);
    const auto expected_front = decode_fixture_tag(
        fixture::place_object_with_opaque_tail(
            5,
            options.ids.tab_item,
            -6920,
            "Item_0_9",
            std::array<std::uint8_t, 2>{0x3C, 0xC3},
            6440));
    ERUI_TEST_CHECK(raw_equal(front_raw, expected_front));
    const auto expected_back = decode_fixture_tag(
        fixture::place_back_tab_object_with_opaque_tail(
            38,
            options.ids.back_tab_item,
            -6920,
            "Item_0_9",
            std::array<std::uint8_t, 2>{0xA5, 0x5A},
            6440));
    ERUI_TEST_CHECK(raw_equal(back_raw, expected_back));

    for (std::uint16_t index = 0; index < 9U; ++index) {
        const auto* existing_front = named_placement(
            transformed.document,
            options.ids.tab_list,
            "Item_0_" + std::to_string(index));
        const auto* existing_back = named_placement(
            transformed.document,
            options.ids.back_tab_list,
            "Item_0_" + std::to_string(index));
        ERUI_TEST_CHECK(existing_front != nullptr);
        ERUI_TEST_CHECK(existing_back != nullptr);
        ERUI_TEST_CHECK(existing_front->depth ==
            static_cast<std::uint16_t>(59U - index * 6U));
        ERUI_TEST_CHECK(existing_back->depth ==
            static_cast<std::uint16_t>(34U - index * 4U));
    }
    const auto* left_arrow = named_placement(
        transformed.document, options.ids.tab_list, "LeftArrow");
    const auto* right_arrow = named_placement(
        transformed.document, options.ids.tab_list, "RightArrow");
    const auto* text = named_placement(
        transformed.document, options.ids.tab_list, "Text_0");
    ERUI_TEST_CHECK(left_arrow != nullptr && left_arrow->depth == 1);
    ERUI_TEST_CHECK(right_arrow != nullptr && right_arrow->depth == 3);
    ERUI_TEST_CHECK(
        left_arrow->translation.x == source_left_arrow->translation.x);
    ERUI_TEST_CHECK(
        left_arrow->translation.y == source_left_arrow->translation.y);
    ERUI_TEST_CHECK(
        right_arrow->translation.x == source_right_arrow->translation.x);
    ERUI_TEST_CHECK(
        right_arrow->translation.y == source_right_arrow->translation.y);
    ERUI_TEST_CHECK(text != nullptr && text->depth == 65);

    const auto transformed_bytes = serialize(transformed.document);
    ERUI_TEST_CHECK(transformed_bytes != source_bytes);
    const auto second = erui::gfx::transform_settings_gfx(
        transformed.document,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(
        second.status == SettingsGfxTransformStatus::unchanged);
    ERUI_TEST_CHECK(serialize(second.document) == transformed_bytes);

    fixture::Options larger_options{};
    larger_options.tab_cells = 10;
    larger_options.specialized_tab_cell = std::uint16_t{8};
    const GfxDocument larger = parse(fixture::make(larger_options));
    const auto larger_bytes = serialize(larger);
    const auto preserved = erui::gfx::transform_settings_gfx(
        larger,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(preserved.success());
    ERUI_TEST_CHECK(!preserved.changed());
    ERUI_TEST_CHECK(preserved.after.authored_tab_slot_capacity == 9);
    ERUI_TEST_CHECK(serialize(preserved.document) == larger_bytes);
}

void test_tab_slot_two_cell_growth_is_atomic_exact_and_upgradeable() {
    fixture::Options options{};
    options.specialized_tab_cell = std::uint16_t{8};
    options.decorated_back_tab_cells = true;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);
    const auto* source_left_arrow = named_placement(
        source, options.ids.tab_list, "LeftArrow");
    const auto* source_right_arrow = named_placement(
        source, options.ids.tab_list, "RightArrow");
    ERUI_TEST_CHECK(source_left_arrow != nullptr);
    ERUI_TEST_CHECK(source_right_arrow != nullptr);
    const auto source_right_arrow_sprite =
        source.sprite_by_character_id(source_right_arrow->character_id);
    ERUI_TEST_CHECK(source_right_arrow_sprite.has_value());
    const auto source_right_arrow_definition = raw_tag(
        source, source_right_arrow_sprite->definition);

    const auto direct = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{10}});
    ERUI_TEST_CHECK(direct.success());
    ERUI_TEST_CHECK(direct.changed());
    ERUI_TEST_CHECK(direct.before.authored_tab_slot_capacity == 8);
    ERUI_TEST_CHECK(direct.after.authored_tab_slot_capacity == 10);
    ERUI_TEST_CHECK(direct.after.tab_list_cells == 11);
    ERUI_TEST_CHECK(direct.after.back_tab_list_cells == 11);
    ERUI_TEST_CHECK(direct.document.revision() == source.revision() + 1U);

    for (std::uint16_t index = 0; index < 11U; ++index) {
        const auto name = "Item_0_" + std::to_string(index);
        const auto* front = named_placement(
            direct.document, options.ids.tab_list, name);
        const auto* back = named_placement(
            direct.document, options.ids.back_tab_list, name);
        ERUI_TEST_CHECK(front != nullptr);
        ERUI_TEST_CHECK(back != nullptr);
        ERUI_TEST_CHECK(front->translation.x ==
            static_cast<std::int32_t>(options.tab_first_x +
                static_cast<std::int32_t>(index) * options.tab_step_x));
        ERUI_TEST_CHECK(back->translation.x == front->translation.x);
        ERUI_TEST_CHECK(front->translation.y == -6920);
        ERUI_TEST_CHECK(back->translation.y == -6920);
        ERUI_TEST_CHECK(front->depth ==
            static_cast<std::uint16_t>(65U - index * 6U));
        const std::uint16_t expected_back = index < 9U
            ? static_cast<std::uint16_t>(34U - index * 4U)
            : static_cast<std::uint16_t>(38U + (index - 9U) * 4U);
        ERUI_TEST_CHECK(back->depth == expected_back);
        if (index >= 9U) {
            ERUI_TEST_CHECK(front->character_id == options.ids.tab_item);
            ERUI_TEST_CHECK(back->character_id == options.ids.back_tab_item);
        }
    }
    const auto* text = named_placement(
        direct.document, options.ids.tab_list, "Text_0");
    const auto* direct_left_arrow = named_placement(
        direct.document, options.ids.tab_list, "LeftArrow");
    const auto* direct_right_arrow = named_placement(
        direct.document, options.ids.tab_list, "RightArrow");
    ERUI_TEST_CHECK(text != nullptr && text->depth == 71U);
    ERUI_TEST_CHECK(direct_left_arrow != nullptr);
    ERUI_TEST_CHECK(direct_right_arrow != nullptr);
    ERUI_TEST_CHECK(
        direct_left_arrow->translation.x == source_left_arrow->translation.x);
    ERUI_TEST_CHECK(
        direct_left_arrow->translation.y == source_left_arrow->translation.y);
    ERUI_TEST_CHECK(
        direct_right_arrow->translation.x ==
            source_right_arrow->translation.x + options.tab_step_x);
    ERUI_TEST_CHECK(
        direct_right_arrow->translation.y == source_right_arrow->translation.y);
    const auto direct_right_arrow_sprite = direct.document.sprite_by_character_id(
        direct_right_arrow->character_id);
    ERUI_TEST_CHECK(direct_right_arrow_sprite.has_value());
    ERUI_TEST_CHECK(raw_equal(
        raw_tag(direct.document, direct_right_arrow_sprite->definition),
        source_right_arrow_definition));
    ERUI_TEST_CHECK(serialize(source) == source_bytes);

    const auto direct_bytes = serialize(direct.document);
    const auto idempotent = erui::gfx::transform_settings_gfx(
        direct.document,
        {.minimum_tab_slots = std::uint16_t{10}});
    ERUI_TEST_CHECK(idempotent.success());
    ERUI_TEST_CHECK(!idempotent.changed());
    ERUI_TEST_CHECK(serialize(idempotent.document) == direct_bytes);

    // A movie emitted by the previous nine-slot request has a canonical
    // prefix followed by ERNativeUI's generated tail. It must be a valid
    // source for a later ten-slot request, not a one-way intermediate format.
    const auto first_stage = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(first_stage.success());
    const auto upgraded = erui::gfx::transform_settings_gfx(
        first_stage.document,
        {.minimum_tab_slots = std::uint16_t{10}});
    ERUI_TEST_CHECK(upgraded.success());
    ERUI_TEST_CHECK(upgraded.changed());
    ERUI_TEST_CHECK(upgraded.after.authored_tab_slot_capacity == 10);
    ERUI_TEST_CHECK(serialize(upgraded.document) == direct_bytes);

    // A canonical third-party ten-cell strip is also accepted. Its existing
    // back-layer mapping is preserved while the eleventh cell extends the
    // proven depth and x lattices.
    fixture::Options canonical_options{};
    canonical_options.tab_cells = 10;
    const GfxDocument canonical = parse(fixture::make(canonical_options));
    const auto canonical_bytes = serialize(canonical);
    const auto* canonical_right_arrow = named_placement(
        canonical, canonical_options.ids.tab_list, "RightArrow");
    ERUI_TEST_CHECK(canonical_right_arrow != nullptr);
    const auto canonical_grown = erui::gfx::transform_settings_gfx(
        canonical,
        {.minimum_tab_slots = std::uint16_t{10}});
    ERUI_TEST_CHECK(canonical_grown.success());
    ERUI_TEST_CHECK(canonical_grown.changed());
    ERUI_TEST_CHECK(
        canonical_grown.document.revision() == canonical.revision() + 1U);
    for (std::uint16_t index = 0; index < 11U; ++index) {
        const auto name = "Item_0_" + std::to_string(index);
        const auto* front = named_placement(
            canonical_grown.document,
            canonical_options.ids.tab_list,
            name);
        const auto* back = named_placement(
            canonical_grown.document,
            canonical_options.ids.back_tab_list,
            name);
        ERUI_TEST_CHECK(front != nullptr && back != nullptr);
        ERUI_TEST_CHECK(front->depth ==
            static_cast<std::uint16_t>(65U - index * 6U));
        ERUI_TEST_CHECK(back->depth == (index < 10U
            ? static_cast<std::uint16_t>(38U - index * 4U)
            : std::uint16_t{42}));
        ERUI_TEST_CHECK(front->translation.x ==
            static_cast<std::int32_t>(canonical_options.tab_first_x +
                static_cast<std::int32_t>(index) *
                    canonical_options.tab_step_x));
        ERUI_TEST_CHECK(back->translation.x == front->translation.x);
    }
    const auto* canonical_grown_right_arrow = named_placement(
        canonical_grown.document,
        canonical_options.ids.tab_list,
        "RightArrow");
    ERUI_TEST_CHECK(canonical_grown_right_arrow != nullptr);
    ERUI_TEST_CHECK(
        canonical_grown_right_arrow->translation.x ==
            canonical_right_arrow->translation.x +
                canonical_options.tab_step_x);
    ERUI_TEST_CHECK(
        canonical_grown_right_arrow->translation.y ==
            canonical_right_arrow->translation.y);
    ERUI_TEST_CHECK(serialize(canonical) == canonical_bytes);

    fixture::Options larger_options{};
    larger_options.tab_cells = 11;
    larger_options.extra_front_tab_decoration = true;
    const GfxDocument larger = parse(fixture::make(larger_options));
    const auto larger_bytes = serialize(larger);
    const auto preserved = erui::gfx::transform_settings_gfx(
        larger,
        {.minimum_tab_slots = std::uint16_t{10}});
    ERUI_TEST_CHECK(preserved.success());
    ERUI_TEST_CHECK(!preserved.changed());
    ERUI_TEST_CHECK(serialize(preserved.document) == larger_bytes);
}

void test_tab_slot_two_cell_growth_fails_before_publication() {
    fixture::Options options{};
    options.tab_first_x = 0;
    options.tab_step_x = 107'374'183;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);

    // Item_0_9 remains representable by SWF MATRIX.NTranslateBits, while
    // Item_0_10 would require 32 signed bits. Planning must reject the second
    // cell before the single document transaction publishes the first one.
    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{10}});
    ERUI_TEST_CHECK(!transformed.success());
    ERUI_TEST_CHECK(!transformed.document.valid());
    check_error(
        transformed.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::tab_slots);
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
    ERUI_TEST_CHECK(!has_named_placement(
        source, options.ids.tab_list, "Item_0_9"));
    ERUI_TEST_CHECK(!has_named_placement(
        source, options.ids.tab_list, "Item_0_10"));
}

void test_tab_slot_growth_rejects_unrepresentable_matrix_translation() {
    fixture::Options options{};
    // Every authored translation fits a signed 31-bit SWF MATRIX field. The
    // next point in this arithmetic progression still fits int32_t, but needs
    // 32 signed bits and therefore cannot be encoded by MATRIX.NTranslateBits.
    options.tab_first_x = -1'073'741'824;
    options.tab_step_x = 268'435'455;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);

    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(!transformed.success());
    ERUI_TEST_CHECK(!transformed.document.valid());
    check_error(
        transformed.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::tab_slots);
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
}

void test_tab_slot_contract_rejects_ambiguous_or_mismatched_layers() {
    for (const auto& options : {
             [] {
                 fixture::Options value{};
                 value.gap_tab_cells = true;
                 return value;
             }(),
             [] {
                 fixture::Options value{};
                 value.mismatch_tab_layer_geometry = true;
                 return value;
             }(),
             [] {
                 fixture::Options value{};
                 value.duplicate_tab_list = true;
                 return value;
             }()}) {
        const GfxDocument malformed = parse(fixture::make(options));
        const auto source_bytes = serialize(malformed);
        const auto inspection = erui::gfx::inspect_settings_gfx(malformed);
        ERUI_TEST_CHECK(!inspection.success());
        ERUI_TEST_CHECK(
            inspection.error.feature == SettingsGfxFeature::tab_slots);
        const auto transformed = erui::gfx::transform_settings_gfx(
            malformed,
            {.minimum_tab_slots = std::uint16_t{9}});
        ERUI_TEST_CHECK(!transformed.success());
        ERUI_TEST_CHECK(!transformed.document.valid());
        ERUI_TEST_CHECK(serialize(malformed) == source_bytes);
    }

    const GfxDocument source = parse(fixture::make());
    for (const std::uint16_t requested : {
             std::uint16_t{7}, std::uint16_t{11}}) {
        const auto invalid = erui::gfx::transform_settings_gfx(
            source,
            {.minimum_tab_slots = requested});
        ERUI_TEST_CHECK(!invalid.success());
        check_error(
            invalid.error,
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::tab_slots);
    }
    const auto widget_only = erui::gfx::transform_settings_widgets_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{8}});
    ERUI_TEST_CHECK(!widget_only.success());
    check_error(
        widget_only.error,
        SettingsGfxErrorCode::invalid_argument,
        SettingsGfxFeature::tab_slots);
}

void test_tab_slot_growth_requires_the_proven_timeline_contract() {
    for (const auto& options : {
             [] {
                 fixture::Options value{};
                 value.nonuniform_tab_spacing = true;
                 return value;
             }(),
             [] {
                 fixture::Options value{};
                 value.noncanonical_front_tab_depth_stride = true;
                 return value;
             }(),
             [] {
                 fixture::Options value{};
                 value.omit_tab_text = true;
                 return value;
             }(),
             [] {
                 fixture::Options value{};
                 value.extra_front_tab_decoration = true;
                 return value;
             }()}) {
        const GfxDocument source = parse(fixture::make(options));
        const auto source_bytes = serialize(source);
        const auto inspection = erui::gfx::inspect_settings_gfx(source);
        ERUI_TEST_CHECK(inspection.success());
        ERUI_TEST_CHECK(inspection.inspection.authored_tab_slot_capacity == 8);

        const auto transformed = erui::gfx::transform_settings_gfx(
            source,
            {.minimum_tab_slots = std::uint16_t{9}});
        ERUI_TEST_CHECK(!transformed.success());
        check_error(
            transformed.error,
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::tab_slots);
        ERUI_TEST_CHECK(serialize(source) == source_bytes);
    }

    // Inspection remains permissive for an already-capacious third-party
    // movie. The growth-only contract must not normalize or reject a layout
    // that ERNativeUI does not need to edit.
    fixture::Options larger{};
    larger.tab_cells = 10;
    larger.extra_front_tab_decoration = true;
    larger.nonuniform_tab_spacing = true;
    const GfxDocument source = parse(fixture::make(larger));
    const auto source_bytes = serialize(source);
    const auto preserved = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(preserved.success());
    ERUI_TEST_CHECK(!preserved.changed());
    ERUI_TEST_CHECK(serialize(preserved.document) == source_bytes);

}

void test_nested_settings_decoy_is_not_a_host() {
    fixture::Options options{};
    options.include_nested_settings_decoy = true;
    const GfxDocument source = parse(fixture::make(options));

    const auto inspection = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(inspection.inspection.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(
        inspection.inspection.window_list_sprite == options.ids.window_list);
    ERUI_TEST_CHECK(
        inspection.inspection.game_options_sprite == options.ids.game_options);
    ERUI_TEST_CHECK(
        inspection.inspection.camera_options_sprite == options.ids.camera_options);
    ERUI_TEST_CHECK(
        inspection.inspection.text_input_sprite == options.ids.text_input);

    // Exercise post-edit rediscovery too: the nested decoy must remain
    // harmless after document revisions invalidate and rebuild the index.
    const auto expanded = erui::gfx::transform_settings_gfx(
        source,
        {
            .minimum_game_options_rows = std::uint16_t{7},
            .minimum_camera_options_rows = std::uint16_t{8},
        });
    ERUI_TEST_CHECK(expanded.success());
    ERUI_TEST_CHECK(expanded.changed());
    ERUI_TEST_CHECK(expanded.after.game_options_rows == 7);
    ERUI_TEST_CHECK(expanded.after.camera_options_rows == 8);
    ERUI_TEST_CHECK(
        expanded.document.sprite_by_character_id(options.ids.nested_decoy)
            .has_value());
}

void test_row_expansion_idempotence_and_preservation() {
    const fixture::Options options{};
    const auto bytes = fixture::make(options);
    GfxDocument source = parse(bytes);
    const std::uint64_t source_revision = source.revision();
    const auto source_before = serialize(source);
    const auto root_unknown_before = body_for_unique_tag(
        source, fixture::unknown_root_tag_code);
    const auto nested_unknown_before = body_for_unique_tag(
        source, fixture::unknown_nested_tag_code);

    const auto expanded = erui::gfx::transform_settings_gfx(
        source,
        {
            .minimum_game_options_rows = std::uint16_t{11},
            .minimum_camera_options_rows = std::uint16_t{12},
        });
    ERUI_TEST_CHECK(expanded.success());
    ERUI_TEST_CHECK(expanded.changed());
    ERUI_TEST_CHECK(expanded.status == SettingsGfxTransformStatus::modified);
    ERUI_TEST_CHECK(expanded.before.game_options_rows == 6);
    ERUI_TEST_CHECK(expanded.before.camera_options_rows == 7);
    ERUI_TEST_CHECK(expanded.after.game_options_rows == 11);
    ERUI_TEST_CHECK(expanded.after.camera_options_rows == 12);
    check_row_depths(
        expanded.document, options.ids.game_options, 11);
    check_row_depths(
        expanded.document, options.ids.camera_options, 12);

    // Passing an lvalue copies the source. Even a successful transformation
    // must not mutate or invalidate the caller's handles/revision.
    ERUI_TEST_CHECK(source.revision() == source_revision);
    ERUI_TEST_CHECK(serialize(source) == source_before);

    ERUI_TEST_CHECK(
        body_for_unique_tag(
            expanded.document, fixture::unknown_root_tag_code) ==
        root_unknown_before);
    ERUI_TEST_CHECK(
        body_for_unique_tag(
            expanded.document, fixture::unknown_nested_tag_code) ==
        nested_unknown_before);
    const auto expanded_bytes = serialize(expanded.document);
    ERUI_TEST_CHECK(expanded.document.trailing_size() == options.trailer.size());
    ERUI_TEST_CHECK(std::equal(
        options.trailer.begin(),
        options.trailer.end(),
        expanded_bytes.end() - static_cast<std::ptrdiff_t>(options.trailer.size())));

    const auto second = erui::gfx::transform_settings_gfx(
        expanded.document,
        {
            .minimum_game_options_rows = std::uint16_t{11},
            .minimum_camera_options_rows = std::uint16_t{12},
        });
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(second.status == SettingsGfxTransformStatus::unchanged);
    ERUI_TEST_CHECK(serialize(second.document) == expanded_bytes);

    // Requirements are minima. Asking for fewer rows must never shrink or
    // rewrite a compatible third-party movie.
    const auto no_shrink = erui::gfx::transform_settings_gfx(
        expanded.document,
        {
            .minimum_game_options_rows = std::uint16_t{3},
            .minimum_camera_options_rows = std::uint16_t{4},
        });
    ERUI_TEST_CHECK(no_shrink.success());
    ERUI_TEST_CHECK(!no_shrink.changed());
    ERUI_TEST_CHECK(no_shrink.after.game_options_rows == 11);
    ERUI_TEST_CHECK(no_shrink.after.camera_options_rows == 12);
    ERUI_TEST_CHECK(serialize(no_shrink.document) == expanded_bytes);
}

void test_button_label_and_atomic_late_failure() {
    fixture::Options options{};
    options.omit_button_label = true;
    const auto bytes = fixture::make(options);
    const GfxDocument source = parse(bytes);

    const auto label = erui::gfx::transform_settings_gfx(
        source,
        {.ensure_button_label = true});
    ERUI_TEST_CHECK(label.success());
    ERUI_TEST_CHECK(label.changed());
    ERUI_TEST_CHECK(has_named_placement(
        label.document, options.ids.button, "Text_1"));

    const auto label_second = erui::gfx::transform_settings_gfx(
        label.document,
        {.ensure_button_label = true});
    ERUI_TEST_CHECK(label_second.success());
    ERUI_TEST_CHECK(!label_second.changed());
    ERUI_TEST_CHECK(
        serialize(label_second.document) == serialize(label.document));

    fixture::Options conflict_options{};
    conflict_options.occupy_button_label_depth = true;
    const auto conflict_bytes = fixture::make(conflict_options);
    GfxDocument conflict_source = parse(conflict_bytes);
    const auto conflict_before = serialize(conflict_source);
    const std::uint64_t conflict_revision = conflict_source.revision();

    // Rows are requested together with a deliberately conflicting later
    // presentation edit. Failure must publish no partially expanded document.
    const auto failed = erui::gfx::transform_settings_gfx(
        conflict_source,
        {
            .minimum_game_options_rows = std::uint16_t{10},
            .minimum_camera_options_rows = std::uint16_t{11},
            .ensure_button_label = true,
        });
    ERUI_TEST_CHECK(!failed.success());
    ERUI_TEST_CHECK(failed.status == SettingsGfxTransformStatus::failed);
    ERUI_TEST_CHECK(!failed.document.valid());
    check_error(
        failed.error,
        SettingsGfxErrorCode::resource_conflict,
        SettingsGfxFeature::button_label);
    ERUI_TEST_CHECK(conflict_source.revision() == conflict_revision);
    ERUI_TEST_CHECK(serialize(conflict_source) == conflict_before);
}

void test_row_growth_allocates_after_unrelated_depth() {
    fixture::Options options{};
    options.occupy_next_game_row_depth = true;
    GfxDocument source = parse(fixture::make(options));
    const auto before = serialize(source);
    const std::uint64_t revision = source.revision();

    const auto result = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_game_options_rows = std::uint16_t{7}});
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.changed());
    ERUI_TEST_CHECK(result.after.game_options_rows == 7);
    const auto* unrelated = named_placement(
        result.document,
        options.ids.game_options,
        "ThirdPartyAtNextRowDepth");
    const auto* inserted = named_placement(
        result.document,
        options.ids.game_options,
        "Item_6_0");
    ERUI_TEST_CHECK(unrelated != nullptr);
    ERUI_TEST_CHECK(unrelated->depth == 13);
    ERUI_TEST_CHECK(inserted != nullptr);
    ERUI_TEST_CHECK(inserted->depth == 15);
    ERUI_TEST_CHECK(source.revision() == revision);
    ERUI_TEST_CHECK(serialize(source) == before);
}

void test_row_growth_prefers_and_preserves_authored_donor() {
    fixture::Options options{};
    options.noncanonical_donor_row = true;
    const GfxDocument source = parse(fixture::make(options));
    const auto* donor = named_placement(
        source, options.ids.row_donor, "Item_7_0");
    ERUI_TEST_CHECK(donor != nullptr);
    GfxRawTag expected = raw_tag(source, donor->tag);
    ERUI_TEST_CHECK(expected.code == 26);
    ERUI_TEST_CHECK(
        expected.header_form == erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(expected.body.size() >= 3);
    constexpr std::uint16_t target_depth = 15;
    expected.body[1] = static_cast<std::uint8_t>(target_depth);
    expected.body[2] = static_cast<std::uint8_t>(target_depth >> 8U);

    const auto result = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_camera_options_rows = std::uint16_t{8}});
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.after.camera_options_rows == 8);
    const auto* inserted = named_placement(
        result.document, options.ids.camera_options, "Item_7_0");
    ERUI_TEST_CHECK(inserted != nullptr);
    const GfxRawTag inserted_raw = raw_tag(
        result.document, inserted->tag);
    ERUI_TEST_CHECK(
        inserted_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(raw_equal(inserted_raw, expected));
}

void test_all_ordinary_panel_layouts_expand_without_cross_contamination() {
    fixture::Options options{};
    options.include_all_settings_panels = true;
    const GfxDocument source = parse(fixture::make(options));

    const auto before = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(before.success());
    ERUI_TEST_CHECK(before.inspection.display_rows == 12);
    ERUI_TEST_CHECK(before.inspection.sound_rows == 12);
    ERUI_TEST_CHECK(
        before.inspection.display_sprite == before.inspection.sound_sprite);
    ERUI_TEST_CHECK(before.inspection.network_rows == 7);
    ERUI_TEST_CHECK(before.inspection.keyboard_mouse_rows == 7);
    ERUI_TEST_CHECK(before.inspection.graphics_rows == 7);

    const auto* warning_before = named_placement(
        source, options.ids.network, "UgcRestricted");
    ERUI_TEST_CHECK(warning_before != nullptr);
    const GfxRawTag warning_raw_before = raw_tag(
        source, warning_before->tag);

    SettingsGfxRequirements requirements{};
    requirements.minimum_game_options_rows = std::uint16_t{13};
    requirements.minimum_camera_options_rows = std::uint16_t{13};
    requirements.minimum_display_rows = std::uint16_t{13};
    requirements.minimum_sound_rows = std::uint16_t{13};
    requirements.minimum_network_rows = std::uint16_t{10};
    requirements.minimum_keyboard_mouse_rows = std::uint16_t{13};
    requirements.minimum_graphics_rows = std::uint16_t{13};
    const auto transformed = erui::gfx::transform_settings_gfx(
        source, requirements);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.changed());
    ERUI_TEST_CHECK(transformed.after.game_options_rows == 13);
    ERUI_TEST_CHECK(transformed.after.camera_options_rows == 13);
    ERUI_TEST_CHECK(transformed.after.display_rows == 13);
    ERUI_TEST_CHECK(transformed.after.sound_rows == 13);
    ERUI_TEST_CHECK(transformed.after.network_rows == 10);
    ERUI_TEST_CHECK(transformed.after.keyboard_mouse_rows == 13);
    ERUI_TEST_CHECK(transformed.after.graphics_rows == 13);
    ERUI_TEST_CHECK(
        transformed.after.display_sprite ==
        transformed.after.sound_sprite);

    const auto* display_item = named_placement(
        transformed.document, options.ids.display_sound, "Item_12_0");
    ERUI_TEST_CHECK(display_item != nullptr);
    ERUI_TEST_CHECK(display_item->depth == 25);
    ERUI_TEST_CHECK(display_item->character_id == options.ids.row_item);

    const auto* network_item = named_placement(
        transformed.document, options.ids.network, "Item_9_0");
    ERUI_TEST_CHECK(network_item != nullptr);
    ERUI_TEST_CHECK(network_item->depth == 21);
    ERUI_TEST_CHECK(
        network_item->character_id == options.ids.native_row_item);
    const auto* warning_after = named_placement(
        transformed.document, options.ids.network, "UgcRestricted");
    ERUI_TEST_CHECK(warning_after != nullptr);
    ERUI_TEST_CHECK(warning_after->depth == 1);
    ERUI_TEST_CHECK(warning_after->translation.y == 6000);
    ERUI_TEST_CHECK(raw_equal(
        warning_raw_before,
        raw_tag(transformed.document, warning_after->tag)));

    for (const std::uint16_t owner : {
             options.ids.keyboard_mouse,
             options.ids.graphics}) {
        const auto* native = named_placement(
            transformed.document, owner, "Item_5_0");
        const auto* spare = named_placement(
            transformed.document, owner, "Item_6_0");
        const auto* added = named_placement(
            transformed.document, owner, "Item_12_0");
        ERUI_TEST_CHECK(native != nullptr);
        ERUI_TEST_CHECK(spare != nullptr);
        ERUI_TEST_CHECK(added != nullptr);
        ERUI_TEST_CHECK(native->character_id == options.ids.native_row_item);
        ERUI_TEST_CHECK(spare->character_id == options.ids.row_item);
        ERUI_TEST_CHECK(added->character_id == spare->character_id);
        ERUI_TEST_CHECK(added->depth == 25);
    }

    const auto first_bytes = serialize(transformed.document);
    const auto second = erui::gfx::transform_settings_gfx(
        transformed.document, requirements);
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(serialize(second.document) == first_bytes);
}

void test_text_input_and_color_picker_presentations() {
    const fixture::Options options{};
    const GfxDocument source = parse(fixture::make(options));
    const auto* source_background = placement_at_depth(
        source, options.ids.button, 1);
    ERUI_TEST_CHECK(source_background != nullptr);
    ERUI_TEST_CHECK(!source_background->has_name);
    const GfxRawTag source_background_raw = raw_tag(
        source, source_background->tag);
    const auto result = erui::gfx::transform_settings_gfx(
        source,
        {
            .text_input = SettingsGfxPresentation::ensure_character_creation,
            .color_picker = SettingsGfxPresentation::ensure_character_creation,
        });
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.changed());
    ERUI_TEST_CHECK(result.after.character_name_text_input);
    ERUI_TEST_CHECK(result.after.character_creation_color_picker);
    ERUI_TEST_CHECK(result.after.color_picker_host_count == 1);

    // The native picker is a standalone Button-derived definition. Its
    // authored depth-1 background must be copied exactly, not synthesized or
    // silently omitted when the ordinary Button uses an unnamed placement.
    const auto* picker_placement = named_placement(
        result.document, options.ids.widgets, "ColorPicker");
    ERUI_TEST_CHECK(picker_placement != nullptr);
    ERUI_TEST_CHECK(picker_placement->has_character);
    const auto* picker_background = placement_at_depth(
        result.document, picker_placement->character_id, 1);
    ERUI_TEST_CHECK(picker_background != nullptr);
    ERUI_TEST_CHECK(raw_equal(
        raw_tag(result.document, picker_background->tag),
        source_background_raw));

    const auto second = erui::gfx::transform_settings_gfx(
        result.document,
        {
            .text_input = SettingsGfxPresentation::ensure_character_creation,
            .color_picker = SettingsGfxPresentation::ensure_character_creation,
        });
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(serialize(second.document) == serialize(result.document));
}

void test_generated_presentation_matches_independent_golden_closure() {
    const fixture::Options options{};
    const GfxDocument source = parse(fixture::make(options));
    const auto* source_background = placement_at_depth(
        source, options.ids.button, 1);
    const auto* source_label = placement_at_depth(
        source, options.ids.button, 12);
    ERUI_TEST_CHECK(source_background != nullptr);
    ERUI_TEST_CHECK(source_label != nullptr);
    const GfxRawTag background_raw = raw_tag(
        source, source_background->tag);
    const GfxRawTag label_raw = raw_tag(source, source_label->tag);

    const auto result = erui::gfx::transform_settings_gfx(
        source,
        {
            .text_input =
                SettingsGfxPresentation::ensure_character_creation,
            .color_picker =
                SettingsGfxPresentation::ensure_character_creation,
        });
    ERUI_TEST_CHECK(result.success());

    const auto* character_frame = named_placement(
        result.document, options.ids.text_input, "CharacterFrame");
    ERUI_TEST_CHECK(character_frame != nullptr);
    ERUI_TEST_CHECK(character_frame->has_character);
    const std::uint16_t character_wrapper = character_frame->character_id;
    ERUI_TEST_CHECK(raw_body_equal(
        raw_tag(result.document, character_frame->tag),
        decode_fixture_tag(
            fixture::character_frame_placement(character_wrapper))));

    const std::uint16_t character_image = wrapper_image_id(
        result.document, character_wrapper);
    const auto character_image_definition =
        result.document.character_definition(character_image);
    const auto character_wrapper_definition =
        result.document.character_definition(character_wrapper);
    ERUI_TEST_CHECK(character_image_definition.has_value());
    ERUI_TEST_CHECK(character_wrapper_definition.has_value());
    const GfxRawTag character_image_raw = raw_tag(
        result.document, character_image_definition->tag);
    ERUI_TEST_CHECK(
        character_image_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::short_form);
    ERUI_TEST_CHECK(raw_equal(
        character_image_raw,
        decode_fixture_tag(fixture::external_image(
            character_image, 428, 108,
            "MENU_FL_Cursor_EntWaku", "MENU_FL_Cursor_EntWaku.tga"))));
    const GfxRawTag character_wrapper_raw = raw_tag(
        result.document, character_wrapper_definition->tag);
    ERUI_TEST_CHECK(
        character_wrapper_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(raw_equal(
        character_wrapper_raw,
        decode_fixture_tag(fixture::unscaled_image_wrapper(
            character_wrapper, character_image, -4193, -1080))));
    const GfxRawTag character_grid_raw = scaling_grid_for(
        result.document, character_wrapper);
    ERUI_TEST_CHECK(
        character_grid_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::short_form);
    ERUI_TEST_CHECK(raw_equal(
        character_grid_raw,
        decode_fixture_tag(fixture::scaling_grid(
            character_wrapper, 9, {-80, 80, -160, 180}))));

    const auto* picker_placement = named_placement(
        result.document, options.ids.widgets, "ColorPicker");
    ERUI_TEST_CHECK(picker_placement != nullptr);
    ERUI_TEST_CHECK(picker_placement->has_character);
    const GfxRawTag hidden_picker_raw = raw_tag(
        result.document, picker_placement->tag);
    ERUI_TEST_CHECK(
        hidden_picker_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::short_form);
    ERUI_TEST_CHECK(raw_equal(
        hidden_picker_raw,
        decode_fixture_tag(
            fixture::hidden_color_picker(picker_placement->character_id))));
    const auto* bracket = placement_at_depth(
        result.document, picker_placement->character_id, 3);
    const auto* fill = named_placement(
        result.document, picker_placement->character_id, "Color");
    const auto* foreground = placement_at_depth(
        result.document, picker_placement->character_id, 7);
    ERUI_TEST_CHECK(bracket != nullptr && bracket->has_character);
    ERUI_TEST_CHECK(fill != nullptr && fill->has_character);
    ERUI_TEST_CHECK(foreground != nullptr && foreground->has_character);
    ERUI_TEST_CHECK(bracket->character_id == character_wrapper);

    const std::uint16_t fill_id = fill->character_id;
    const std::uint16_t foreground_id = foreground->character_id;
    const std::uint16_t color_image = wrapper_image_id(
        result.document, foreground_id);
    const auto fill_definition = result.document.character_definition(fill_id);
    const auto foreground_definition =
        result.document.character_definition(foreground_id);
    const auto color_image_definition =
        result.document.character_definition(color_image);
    ERUI_TEST_CHECK(fill_definition.has_value());
    ERUI_TEST_CHECK(foreground_definition.has_value());
    ERUI_TEST_CHECK(color_image_definition.has_value());
    const auto* shape = placement_at_depth(result.document, fill_id, 1);
    ERUI_TEST_CHECK(shape != nullptr && shape->has_character);
    const auto shape_definition = result.document.character_definition(
        shape->character_id);
    ERUI_TEST_CHECK(shape_definition.has_value());
    const GfxRawTag color_image_raw = raw_tag(
        result.document, color_image_definition->tag);
    ERUI_TEST_CHECK(
        color_image_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::short_form);
    ERUI_TEST_CHECK(raw_equal(
        color_image_raw,
        decode_fixture_tag(fixture::external_image(
            color_image, 74, 66,
            "MENU_FL_ColorWaku", "MENU_FL_ColorWaku.tga"))));
    ERUI_TEST_CHECK(raw_equal(
        raw_tag(result.document, shape_definition->tag),
        decode_fixture_tag(
            fixture::color_fill_shape(shape->character_id))));
    const GfxRawTag fill_definition_raw = raw_tag(
        result.document, fill_definition->tag);
    ERUI_TEST_CHECK(
        fill_definition_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(raw_equal(
        fill_definition_raw,
        decode_fixture_tag(
            fixture::single_child_sprite(fill_id, shape->character_id))));
    const GfxRawTag foreground_definition_raw = raw_tag(
        result.document, foreground_definition->tag);
    ERUI_TEST_CHECK(
        foreground_definition_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(raw_equal(
        foreground_definition_raw,
        decode_fixture_tag(fixture::image_wrapper(
            foreground_id, color_image, -363, -325, 32768, 32768))));
    const GfxRawTag color_grid_raw = scaling_grid_for(
        result.document, foreground_id);
    ERUI_TEST_CHECK(
        color_grid_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::short_form);
    ERUI_TEST_CHECK(raw_equal(
        color_grid_raw,
        decode_fixture_tag(fixture::scaling_grid(
            foreground_id, 8, {-100, 100, -100, 100}))));

    constexpr std::array<std::int32_t, 4> bracket_multiply{
        225, 225, 225, 256};
    constexpr std::array<std::int32_t, 4> foreground_multiply{
        230, 230, 230, 256};
    constexpr std::array<std::int32_t, 4> foreground_add{
        26, 26, 26, 0};
    std::vector<std::vector<std::uint8_t>> expected_picker_children{
        encode_raw_tag(background_raw),
        fixture::native_named_placement(
            options.ids.cursor, 2, 0, 0, "Cursor"),
        fixture::transformed_placement(
            3, character_wrapper, -487, -320, 65536, 31457,
            bracket_multiply),
        fixture::color_fill_placement(fill_id, 5),
        fixture::transformed_placement(
            7, foreground_id, -487, -330, 692638, 65536,
            foreground_multiply, foreground_add),
        fixture::transparent_placement(
            options.ids.text_value, 10, "Text_0"),
        encode_raw_tag(label_raw),
    };
    const auto picker_definition = result.document.character_definition(
        picker_placement->character_id);
    ERUI_TEST_CHECK(picker_definition.has_value());
    const GfxRawTag picker_definition_raw = raw_tag(
        result.document, picker_definition->tag);
    ERUI_TEST_CHECK(
        picker_definition_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(raw_equal(
        picker_definition_raw,
        decode_fixture_tag(fixture::define_sprite(
            picker_placement->character_id,
            expected_picker_children))));

    const auto* transparent_value = placement_at_depth(
        result.document, picker_placement->character_id, 10);
    ERUI_TEST_CHECK(transparent_value != nullptr);
    const GfxRawTag transparent_value_raw = raw_tag(
        result.document, transparent_value->tag);
    ERUI_TEST_CHECK(
        transparent_value_raw.header_form ==
        erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(raw_equal(
        transparent_value_raw,
        decode_fixture_tag(fixture::transparent_placement(
            options.ids.text_value, 10, "Text_0"))));
}

void test_duplicate_character_frame_closure_fails_closed() {
    const fixture::Options options{};
    auto generated = erui::gfx::transform_settings_gfx(
        parse(fixture::make(options)),
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(generated.success());
    GfxDocument duplicated = generated.document;

    const auto* picker = named_placement(
        duplicated, options.ids.widgets, "ColorPicker");
    ERUI_TEST_CHECK(picker != nullptr);
    const auto* bracket = placement_at_depth(
        duplicated, picker->character_id, 3);
    ERUI_TEST_CHECK(bracket != nullptr && bracket->has_character);
    const std::uint16_t image_id = wrapper_image_id(
        duplicated, bracket->character_id);
    const auto duplicate_id = duplicated.first_free_character_id();
    ERUI_TEST_CHECK(duplicate_id.success());
    const auto button_definition = duplicated.character_definition(
        options.ids.button);
    ERUI_TEST_CHECK(button_definition.has_value());
    std::array duplicate_resources{
        GfxEdit::insert_before(
            button_definition->tag,
            decode_fixture_tag(fixture::unscaled_image_wrapper(
                duplicate_id.character_id, image_id, -4193, -1080))),
        GfxEdit::insert_before(
            button_definition->tag,
            decode_fixture_tag(fixture::scaling_grid(
                duplicate_id.character_id, 9, {-80, 80, -160, 180}))),
    };
    ERUI_TEST_CHECK(duplicated.apply(duplicate_resources).success());

    const auto inspection = erui::gfx::inspect_settings_gfx(duplicated);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(!inspection.inspection.character_creation_color_picker);
    const auto rejected = erui::gfx::transform_settings_gfx(
        duplicated,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(!rejected.success());
    check_error(
        rejected.error,
        SettingsGfxErrorCode::resource_conflict,
        SettingsGfxFeature::color_picker);
}

void test_legacy_color_preview_migration() {
    for (const std::uint16_t depth : {std::uint16_t{5}, std::uint16_t{7}}) {
        fixture::Options options{};
        options.legacy_color_preview_depth = depth;
        const GfxDocument source = parse(fixture::make(options));
        const auto before = erui::gfx::inspect_settings_gfx(source);
        ERUI_TEST_CHECK(before.success());
        ERUI_TEST_CHECK(!before.inspection.character_creation_color_picker);
        ERUI_TEST_CHECK(count_ascii(serialize(source), "ColorPreview") == 1);

        const auto migrated = erui::gfx::transform_settings_gfx(
            source,
            {.color_picker =
                SettingsGfxPresentation::ensure_character_creation});
        ERUI_TEST_CHECK(migrated.success());
        ERUI_TEST_CHECK(migrated.changed());
        ERUI_TEST_CHECK(
            migrated.after.character_creation_color_picker);
        ERUI_TEST_CHECK(migrated.after.color_picker_host_count == 1);
        const auto migrated_bytes = serialize(migrated.document);
        ERUI_TEST_CHECK(count_ascii(migrated_bytes, "ColorPreview") == 0);
        ERUI_TEST_CHECK(count_ascii(migrated_bytes, "ColorPicker") == 1);

        const auto* frame = placement_at_depth(
            migrated.document, options.ids.button, 3);
        ERUI_TEST_CHECK(frame != nullptr);
        ERUI_TEST_CHECK(!frame->has_name);

        const auto second = erui::gfx::transform_settings_gfx(
            migrated.document,
            {.color_picker =
                SettingsGfxPresentation::ensure_character_creation});
        ERUI_TEST_CHECK(second.success());
        ERUI_TEST_CHECK(!second.changed());
        ERUI_TEST_CHECK(serialize(second.document) == migrated_bytes);
    }
}

void test_widget_host_definition_order_beats_numeric_ids() {
    fixture::Options options{};
    options.include_second_widget_host = true;
    const GfxDocument source = parse(fixture::make(options));
    ERUI_TEST_CHECK(options.ids.second_widgets < options.ids.widgets);
    ERUI_TEST_CHECK(
        top_level_ordinal(source, options.ids.widgets) <
        top_level_ordinal(source, options.ids.second_widgets));

    const auto result = erui::gfx::transform_settings_gfx(
        source,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.after.color_picker_host_count == 2);
    ERUI_TEST_CHECK(result.after.character_creation_color_picker);

    for (const auto [container, button] : std::array{
             std::pair{options.ids.widgets, options.ids.button},
             std::pair{options.ids.second_widgets,
                       options.ids.second_button}}) {
        const auto* picker = named_placement(
            result.document, container, "ColorPicker");
        ERUI_TEST_CHECK(picker != nullptr);
        ERUI_TEST_CHECK(picker->has_character);
        ERUI_TEST_CHECK(
            top_level_ordinal(result.document, picker->character_id) <
            top_level_ordinal(result.document, container));

        const auto* original_background = placement_at_depth(
            source, button, 1);
        const auto* picker_background = placement_at_depth(
            result.document, picker->character_id, 1);
        ERUI_TEST_CHECK(original_background != nullptr);
        ERUI_TEST_CHECK(picker_background != nullptr);
        ERUI_TEST_CHECK(raw_equal(
            raw_tag(source, original_background->tag),
            raw_tag(result.document, picker_background->tag)));
    }
}

void test_color_picker_dependency_order_is_repaired_atomically() {
    fixture::Options options{};
    options.additional_shared_widget_hosts = 2;
    const GfxDocument source = parse(fixture::make(options));
    const auto generated = erui::gfx::transform_settings_gfx(
        source,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(generated.success());
    ERUI_TEST_CHECK(generated.after.character_creation_color_picker);
    GfxDocument reordered = generated.document;

    std::vector<std::uint16_t> containers{options.ids.widgets};
    for (std::uint16_t index = 0;
         index < options.additional_shared_widget_hosts;
         ++index) {
        containers.push_back(static_cast<std::uint16_t>(
            options.ids.shared_widget_first + index));
    }

    std::vector<std::uint16_t> picker_ids{};
    for (const std::uint16_t container : containers) {
        const auto* picker = named_placement(
            reordered, container, "ColorPicker");
        ERUI_TEST_CHECK(picker != nullptr);
        ERUI_TEST_CHECK(picker->has_character);
        picker_ids.push_back(picker->character_id);
    }

    const auto* bracket = placement_at_depth(
        reordered, picker_ids.front(), 3);
    const auto* fill = named_placement(
        reordered, picker_ids.front(), "Color");
    const auto* foreground = placement_at_depth(
        reordered, picker_ids.front(), 7);
    ERUI_TEST_CHECK(bracket != nullptr && bracket->has_character);
    ERUI_TEST_CHECK(fill != nullptr && fill->has_character);
    ERUI_TEST_CHECK(foreground != nullptr && foreground->has_character);
    const auto* shape = placement_at_depth(
        reordered, fill->character_id, 1);
    ERUI_TEST_CHECK(shape != nullptr && shape->has_character);

    const std::uint16_t bracket_id = bracket->character_id;
    const std::uint16_t fill_id = fill->character_id;
    const std::uint16_t foreground_id = foreground->character_id;
    const std::uint16_t shape_id = shape->character_id;

    const std::uint16_t character_image = wrapper_image_id(
        reordered, bracket_id);
    const std::uint16_t color_image = wrapper_image_id(
        reordered, foreground_id);

    std::vector<erui::gfx::GfxTagHandle> definitions{};
    definitions.reserve(picker_ids.size() + 8U);
    for (const std::uint16_t picker_id : picker_ids) {
        definitions.push_back(character_definition_handle(
            reordered, picker_id));
    }
    definitions.push_back(scaling_grid_handle(
        reordered, bracket_id));
    definitions.push_back(character_definition_handle(
        reordered, bracket_id));
    definitions.push_back(character_definition_handle(
        reordered, character_image));
    definitions.push_back(scaling_grid_handle(
        reordered, foreground_id));
    definitions.push_back(character_definition_handle(
        reordered, foreground_id));
    definitions.push_back(character_definition_handle(
        reordered, fill_id));
    definitions.push_back(character_definition_handle(
        reordered, shape_id));
    definitions.push_back(character_definition_handle(
        reordered, color_image));

    const auto end = top_level_end(reordered);
    std::vector<GfxEdit> edits{};
    edits.reserve(definitions.size() * 2U);
    for (const auto handle : definitions) {
        edits.push_back(GfxEdit::insert_before(
            end,
            raw_tag(reordered, handle)));
    }
    for (const auto handle : definitions) {
        edits.push_back(GfxEdit::erase(handle));
    }
    ERUI_TEST_CHECK(reordered.apply(edits).success());
    const auto reordered_bytes = serialize(reordered);

    const auto inspection = erui::gfx::inspect_settings_gfx(reordered);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(!inspection.inspection.character_creation_color_picker);

    const auto repaired = erui::gfx::transform_settings_gfx(
        reordered,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(repaired.success());
    ERUI_TEST_CHECK(repaired.changed());
    ERUI_TEST_CHECK(repaired.after.character_creation_color_picker);
    ERUI_TEST_CHECK(serialize(reordered) == reordered_bytes);

    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, character_image) <
        top_level_ordinal(repaired.document, bracket_id));
    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, bracket_id) <
        top_level_ordinal(
            repaired.document,
            scaling_grid_handle(repaired.document, bracket_id)));
    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, shape_id) <
        top_level_ordinal(repaired.document, fill_id));
    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, color_image) <
        top_level_ordinal(repaired.document, foreground_id));
    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, foreground_id) <
        top_level_ordinal(
            repaired.document,
            scaling_grid_handle(repaired.document, foreground_id)));
    for (std::size_t index = 0; index < picker_ids.size(); ++index) {
        ERUI_TEST_CHECK(
            top_level_ordinal(repaired.document, picker_ids[index]) <
            top_level_ordinal(repaired.document, containers[index]));
    }

    const auto repaired_bytes = serialize(repaired.document);
    const auto second = erui::gfx::transform_settings_gfx(
        repaired.document,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(serialize(second.document) == repaired_bytes);
}

void test_color_picker_repair_preserves_earlier_text_input_consumer() {
    const fixture::Options options{};
    const GfxDocument source = parse(fixture::make(options));
    const auto generated = erui::gfx::transform_settings_gfx(
        source,
        {
            .text_input =
                SettingsGfxPresentation::ensure_character_creation,
            .color_picker =
                SettingsGfxPresentation::ensure_character_creation,
        });
    ERUI_TEST_CHECK(generated.success());
    ERUI_TEST_CHECK(generated.after.character_name_text_input);
    ERUI_TEST_CHECK(generated.after.character_creation_color_picker);

    GfxDocument reordered = generated.document;
    const auto* text_frame = named_placement(
        reordered, options.ids.text_input, "CharacterFrame");
    ERUI_TEST_CHECK(text_frame != nullptr);
    ERUI_TEST_CHECK(text_frame->has_character);
    const std::uint16_t character_wrapper = text_frame->character_id;
    const std::uint16_t character_image = wrapper_image_id(
        reordered, character_wrapper);

    const auto* picker = named_placement(
        reordered, options.ids.widgets, "ColorPicker");
    ERUI_TEST_CHECK(picker != nullptr);
    ERUI_TEST_CHECK(picker->has_character);
    const auto* fill = named_placement(
        reordered, picker->character_id, "Color");
    const auto* foreground = placement_at_depth(
        reordered, picker->character_id, 7);
    ERUI_TEST_CHECK(fill != nullptr && fill->has_character);
    ERUI_TEST_CHECK(foreground != nullptr && foreground->has_character);
    const auto* shape = placement_at_depth(
        reordered, fill->character_id, 1);
    ERUI_TEST_CHECK(shape != nullptr && shape->has_character);

    const std::uint16_t fill_id = fill->character_id;
    const std::uint16_t foreground_id = foreground->character_id;
    const std::uint16_t shape_id = shape->character_id;
    const std::uint16_t color_image = wrapper_image_id(
        reordered, foreground_id);

    // Move only the ColorPicker-specific resource chain after its picker
    // consumer. The shared CharacterFrame closure remains valid for the
    // earlier TextInput definition throughout this malformed input.
    const std::array color_chain{
        character_definition_handle(reordered, color_image),
        character_definition_handle(reordered, shape_id),
        character_definition_handle(reordered, fill_id),
        character_definition_handle(reordered, foreground_id),
        scaling_grid_handle(reordered, foreground_id),
    };
    const auto end = top_level_end(reordered);
    std::vector<GfxEdit> edits{};
    edits.reserve(color_chain.size() * 2U);
    for (const auto handle : color_chain) {
        edits.push_back(GfxEdit::insert_before(
            end, raw_tag(reordered, handle)));
    }
    for (const auto handle : color_chain) {
        edits.push_back(GfxEdit::erase(handle));
    }
    ERUI_TEST_CHECK(reordered.apply(edits).success());
    const auto reordered_bytes = serialize(reordered);

    const auto before = erui::gfx::inspect_settings_gfx(reordered);
    ERUI_TEST_CHECK(before.success());
    ERUI_TEST_CHECK(before.inspection.character_name_text_input);
    ERUI_TEST_CHECK(!before.inspection.character_creation_color_picker);

    const auto repaired = erui::gfx::transform_settings_gfx(
        reordered,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(repaired.success());
    ERUI_TEST_CHECK(repaired.changed());
    ERUI_TEST_CHECK(repaired.after.character_name_text_input);
    ERUI_TEST_CHECK(repaired.after.character_creation_color_picker);
    ERUI_TEST_CHECK(serialize(reordered) == reordered_bytes);

    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, character_image) <
        top_level_ordinal(repaired.document, character_wrapper));
    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, character_wrapper) <
        top_level_ordinal(
            repaired.document,
            scaling_grid_handle(
                repaired.document, character_wrapper)));
    ERUI_TEST_CHECK(
        top_level_ordinal(
            repaired.document,
            scaling_grid_handle(
                repaired.document, character_wrapper)) <
        top_level_ordinal(repaired.document, options.ids.text_input));

    const auto repaired_bytes = serialize(repaired.document);
    const auto second = erui::gfx::transform_settings_gfx(
        repaired.document,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(second.after.character_name_text_input);
    ERUI_TEST_CHECK(second.after.character_creation_color_picker);
    ERUI_TEST_CHECK(serialize(second.document) == repaired_bytes);
}

void test_many_widget_hosts_share_button_deterministically() {
    fixture::Options options{};
    options.additional_shared_widget_hosts = 63;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);

    const auto first = erui::gfx::transform_settings_gfx(
        source,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    const auto second = erui::gfx::transform_settings_gfx(
        source,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(first.success());
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(first.after.color_picker_host_count == 64);
    ERUI_TEST_CHECK(first.after.character_creation_color_picker);
    ERUI_TEST_CHECK(serialize(first.document) == serialize(second.document));
    ERUI_TEST_CHECK(serialize(source) == source_bytes);

    const auto* label = named_placement(
        first.document, options.ids.button, "Text_1");
    ERUI_TEST_CHECK(label != nullptr);
    ERUI_TEST_CHECK(label->depth == 12);

    std::unordered_set<std::uint16_t> picker_ids{};
    for (std::uint16_t index = 0; index < 64; ++index) {
        const std::uint16_t container = index == 0
            ? options.ids.widgets
            : static_cast<std::uint16_t>(
                options.ids.shared_widget_first + index - 1U);
        const auto* picker = named_placement(
            first.document, container, "ColorPicker");
        ERUI_TEST_CHECK(picker != nullptr);
        ERUI_TEST_CHECK(picker->has_character);
        ERUI_TEST_CHECK(picker_ids.emplace(picker->character_id).second);
        ERUI_TEST_CHECK(
            top_level_ordinal(first.document, picker->character_id) <
            top_level_ordinal(first.document, container));

        const auto* source_background = placement_at_depth(
            source, options.ids.button, 1);
        const auto* picker_background = placement_at_depth(
            first.document, picker->character_id, 1);
        ERUI_TEST_CHECK(source_background != nullptr);
        ERUI_TEST_CHECK(picker_background != nullptr);
        ERUI_TEST_CHECK(raw_equal(
            raw_tag(source, source_background->tag),
            raw_tag(first.document, picker_background->tag)));
    }
    ERUI_TEST_CHECK(picker_ids.size() == 64);
}

void test_generated_color_picker_body_budget_fails_atomically() {
    fixture::Options options{};
    options.additional_shared_widget_hosts = 31;
    options.button_background_opaque_tail_bytes = 2U * 1024U * 1024U;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);
    const auto inspection = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(inspection.inspection.color_picker_host_count == 32);

    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(!transformed.success());
    check_error(
        transformed.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::color_picker);
    ERUI_TEST_CHECK(transformed.error.detail == 19);
    ERUI_TEST_CHECK(!transformed.document.valid());
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
    ERUI_TEST_CHECK(!has_named_placement(
        source, options.ids.widgets, "ColorPicker"));
}

void test_resource_after_use_is_repaired_not_accepted() {
    const fixture::Options options{};
    auto generated = erui::gfx::transform_settings_gfx(
        parse(fixture::make(options)),
        {.text_input =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(generated.success());
    ERUI_TEST_CHECK(generated.after.character_name_text_input);

    GfxDocument reordered = generated.document;
    const auto* frame = named_placement(
        reordered, options.ids.text_input, "CharacterFrame");
    ERUI_TEST_CHECK(frame != nullptr);
    ERUI_TEST_CHECK(frame->has_character);
    const std::uint16_t wrapper_id = frame->character_id;
    const auto wrapper_definition = reordered.character_definition(wrapper_id);
    ERUI_TEST_CHECK(wrapper_definition.has_value());
    const GfxRawTag wrapper_body = raw_tag(
        reordered, wrapper_definition->tag);
    // DefineSprite(4 bytes), long tag header(6 bytes), PlaceObject3 flags(2),
    // depth(2), then the referenced external-image character ID.
    ERUI_TEST_CHECK(wrapper_body.body.size() >= 16);
    const std::uint16_t image_id = static_cast<std::uint16_t>(
        wrapper_body.body[14] |
        (static_cast<std::uint16_t>(wrapper_body.body[15]) << 8U));
    const auto image_definition = reordered.character_definition(image_id);
    ERUI_TEST_CHECK(image_definition.has_value());

    std::optional<erui::gfx::GfxTagHandle> grid{};
    std::optional<erui::gfx::GfxTagHandle> top_level_end{};
    for (const auto& info : reordered.tags()) {
        if (info.parent_sprite) continue;
        if (info.code == 0) {
            ERUI_TEST_CHECK(!top_level_end.has_value());
            top_level_end = info.handle;
        }
        if (info.code != 78) continue;
        const auto raw = raw_tag(reordered, info.handle);
        if (raw.body.size() >= 2 &&
            static_cast<std::uint16_t>(raw.body[0] |
                (static_cast<std::uint16_t>(raw.body[1]) << 8U)) ==
                wrapper_id) {
            ERUI_TEST_CHECK(!grid.has_value());
            grid = info.handle;
        }
    }
    ERUI_TEST_CHECK(grid.has_value());
    ERUI_TEST_CHECK(top_level_end.has_value());

    GfxRawTag image_raw = raw_tag(reordered, image_definition->tag);
    GfxRawTag wrapper_raw = raw_tag(reordered, wrapper_definition->tag);
    GfxRawTag grid_raw = raw_tag(reordered, *grid);
    std::vector<GfxEdit> edits{};
    edits.push_back(GfxEdit::erase(image_definition->tag));
    edits.push_back(GfxEdit::erase(wrapper_definition->tag));
    edits.push_back(GfxEdit::erase(*grid));
    edits.push_back(GfxEdit::insert_before(
        *top_level_end, std::move(image_raw)));
    edits.push_back(GfxEdit::insert_before(
        *top_level_end, std::move(wrapper_raw)));
    edits.push_back(GfxEdit::insert_before(
        *top_level_end, std::move(grid_raw)));
    ERUI_TEST_CHECK(reordered.apply(edits).success());

    const auto misplaced = erui::gfx::inspect_settings_gfx(reordered);
    ERUI_TEST_CHECK(misplaced.success());
    ERUI_TEST_CHECK(!misplaced.inspection.character_name_text_input);

    const auto repaired = erui::gfx::transform_settings_gfx(
        reordered,
        {.text_input =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(repaired.success());
    ERUI_TEST_CHECK(repaired.changed());
    ERUI_TEST_CHECK(repaired.after.character_name_text_input);
    ERUI_TEST_CHECK(
        top_level_ordinal(repaired.document, wrapper_id) <
        top_level_ordinal(repaired.document, options.ids.text_input));
}

void test_malformed_existing_picker_is_rejected() {
    const fixture::Options options{};
    auto generated = erui::gfx::transform_settings_gfx(
        parse(fixture::make(options)),
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(generated.success());

    GfxDocument malformed = generated.document;
    const auto* picker = named_placement(
        malformed, options.ids.widgets, "ColorPicker");
    ERUI_TEST_CHECK(picker != nullptr);
    const auto* fill = named_placement(
        malformed, picker->character_id, "Color");
    ERUI_TEST_CHECK(fill != nullptr);
    ERUI_TEST_CHECK(fill->has_character);
    const auto fill_sprite = malformed.sprite_by_character_id(
        fill->character_id);
    ERUI_TEST_CHECK(fill_sprite.has_value());

    std::optional<erui::gfx::GfxTagHandle> show_frame{};
    for (const auto& info : malformed.tags()) {
        if (info.parent_sprite == fill_sprite->definition && info.code == 1) {
            ERUI_TEST_CHECK(!show_frame.has_value());
            show_frame = info.handle;
        }
    }
    ERUI_TEST_CHECK(show_frame.has_value());
    const auto* copied_background = placement_at_depth(
        malformed, picker->character_id, 1);
    ERUI_TEST_CHECK(copied_background != nullptr);
    const std::array extra_edit{GfxEdit::insert_before(
        *show_frame, raw_tag(malformed, copied_background->tag))};
    ERUI_TEST_CHECK(malformed.apply(extra_edit).success());

    const auto inspection = erui::gfx::inspect_settings_gfx(malformed);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(!inspection.inspection.character_creation_color_picker);
    const auto rejected = erui::gfx::transform_settings_gfx(
        malformed,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(!rejected.success());
    check_error(
        rejected.error,
        SettingsGfxErrorCode::resource_conflict,
        SettingsGfxFeature::color_picker);

    // A second instance with the same name and character ID is not a valid
    // ready picker. It must make the host ambiguous rather than being accepted
    // merely because both placements happen to reference an exact definition.
    GfxDocument duplicated = generated.document;
    const auto* original_picker = named_placement(
        duplicated, options.ids.widgets, "ColorPicker");
    ERUI_TEST_CHECK(original_picker != nullptr);
    const auto widget = duplicated.sprite_by_character_id(options.ids.widgets);
    ERUI_TEST_CHECK(widget.has_value());
    std::optional<erui::gfx::GfxTagHandle> widget_show_frame{};
    for (const auto& info : duplicated.tags()) {
        if (info.parent_sprite == widget->definition && info.code == 1) {
            ERUI_TEST_CHECK(!widget_show_frame.has_value());
            widget_show_frame = info.handle;
        }
    }
    ERUI_TEST_CHECK(widget_show_frame.has_value());
    const std::array duplicate_edit{GfxEdit::insert_before(
        *widget_show_frame, raw_tag(duplicated, original_picker->tag))};
    ERUI_TEST_CHECK(duplicated.apply(duplicate_edit).success());
    const auto duplicate_rejected = erui::gfx::transform_settings_gfx(
        duplicated,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(!duplicate_rejected.success());
    check_error(
        duplicate_rejected.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::color_picker);
}

void test_nested_character_frame_decoy_and_imported_ids() {
    fixture::Options options{};
    options.include_nested_character_frame_decoy = true;
    options.imported_character_ids = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const GfxDocument source = parse(fixture::make(options));
    const auto nested = source.sprite_by_character_id(
        options.ids.nested_character_frame_decoy);
    ERUI_TEST_CHECK(nested.has_value());
    const GfxRawTag nested_before = raw_tag(source, nested->definition);
    const auto before = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(before.success());
    ERUI_TEST_CHECK(!before.inspection.character_name_text_input);

    const auto result = erui::gfx::transform_settings_gfx(
        source,
        {
            .text_input =
                SettingsGfxPresentation::ensure_character_creation,
            .color_picker =
                SettingsGfxPresentation::ensure_character_creation,
        });
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.after.character_name_text_input);
    ERUI_TEST_CHECK(result.after.character_creation_color_picker);
    const auto nested_after = result.document.sprite_by_character_id(
        options.ids.nested_character_frame_decoy);
    ERUI_TEST_CHECK(nested_after.has_value());
    ERUI_TEST_CHECK(raw_equal(
        nested_before,
        raw_tag(result.document, nested_after->definition)));

    std::vector<std::uint16_t> source_definitions{};
    for (const auto& definition : source.character_definitions()) {
        source_definitions.push_back(definition.character_id);
    }
    std::size_t generated_count{};
    for (const auto& definition : result.document.character_definitions()) {
        if (std::find(
                source_definitions.begin(), source_definitions.end(),
                definition.character_id) != source_definitions.end()) {
            continue;
        }
        ++generated_count;
        ERUI_TEST_CHECK(definition.character_id > 12);
    }
    ERUI_TEST_CHECK(generated_count >= 7);
}

void test_request_validation_and_text_input_scope() {
    const GfxDocument invalid{};
    {
        SettingsGfxRequirements requirements{};
        requirements.text_input = static_cast<SettingsGfxPresentation>(0xFF);
        const auto result = erui::gfx::transform_settings_gfx(
            invalid, requirements);
        ERUI_TEST_CHECK(!result.success());
        check_error(
            result.error,
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::text_input);
    }
    {
        const auto result = erui::gfx::transform_settings_gfx(
            invalid,
            {.minimum_game_options_rows = std::uint16_t{14}});
        ERUI_TEST_CHECK(!result.success());
        check_error(
            result.error,
            SettingsGfxErrorCode::invalid_argument,
            SettingsGfxFeature::game_options_rows);
        ERUI_TEST_CHECK(result.error.detail == 14);
    }

    // TextInput presentation is independent of the Button label extension.
    // A TextInput-only request must neither require nor silently add Text_1.
    fixture::Options options{};
    options.omit_button_label = true;
    const auto result = erui::gfx::transform_settings_gfx(
        parse(fixture::make(options)),
        {.text_input = SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.after.character_name_text_input);
    ERUI_TEST_CHECK(!has_named_placement(
        result.document, options.ids.button, "Text_1"));
}

void test_panel_pair_must_share_one_top_level_carrier() {
    fixture::Options options{};
    options.split_panel_carriers = true;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);

    const auto inspection = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(!inspection.success());
    check_error(
        inspection.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::camera_options_rows);

    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {
            .minimum_game_options_rows = std::uint16_t{6},
            .minimum_camera_options_rows = std::uint16_t{7},
        });
    ERUI_TEST_CHECK(!transformed.success());
    check_error(
        transformed.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::camera_options_rows);
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
}

void test_panel_timeline_impostors_are_rejected_before_no_op_growth() {
    for (const auto [frame_count, show_frame_count] :
         std::array{
             std::pair<std::uint16_t, std::uint16_t>{
                 std::uint16_t{2}, std::uint16_t{2}},
             std::pair<std::uint16_t, std::uint16_t>{
                 std::uint16_t{1}, std::uint16_t{0}},
             std::pair<std::uint16_t, std::uint16_t>{
                 std::uint16_t{1}, std::uint16_t{2}},
         }) {
        fixture::Options options{};
        options.game_panel_frame_count = frame_count;
        options.game_panel_show_frame_count = show_frame_count;
        const GfxDocument source = parse(fixture::make(options));
        const auto source_bytes = serialize(source);

        const auto inspection = erui::gfx::inspect_settings_gfx(source);
        ERUI_TEST_CHECK(!inspection.success());
        check_error(
            inspection.error,
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::game_options_rows);

        const auto transformed = erui::gfx::transform_settings_gfx(
            source,
            {.minimum_game_options_rows = std::uint16_t{6}});
        ERUI_TEST_CHECK(!transformed.success());
        check_error(
            transformed.error,
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::game_options_rows);
        ERUI_TEST_CHECK(serialize(source) == source_bytes);
    }
}

void test_button_timeline_uses_the_first_frame_boundary() {
    // Native Button symbols are state timelines rather than one-frame
    // containers. A label inserted before the first ShowFrame remains present
    // in later frames until the timeline explicitly changes it.
    fixture::Options supported{};
    supported.omit_button_label = true;
    supported.button_frame_count = 2;
    supported.button_show_frame_count = 2;
    const GfxDocument source = parse(fixture::make(supported));
    const auto source_bytes = serialize(source);

    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.ensure_button_label = true});
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.changed());
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
    ERUI_TEST_CHECK(has_named_placement(
        transformed.document, supported.ids.button, "Text_1"));

    fixture::Options invalid{};
    invalid.omit_button_label = true;
    invalid.button_frame_count = 1;
    invalid.button_show_frame_count = 0;
    const GfxDocument missing_boundary = parse(fixture::make(invalid));
    const auto missing_boundary_bytes = serialize(missing_boundary);
    const auto rejected = erui::gfx::transform_settings_gfx(
        missing_boundary,
        {.ensure_button_label = true});
    ERUI_TEST_CHECK(!rejected.success());
    check_error(
        rejected.error,
        SettingsGfxErrorCode::incompatible_structure,
        SettingsGfxFeature::button_label);
    ERUI_TEST_CHECK(serialize(missing_boundary) == missing_boundary_bytes);
    ERUI_TEST_CHECK(!has_named_placement(
        missing_boundary, invalid.ids.button, "Text_1"));
}

void test_place_object3_legacy_button_frame_is_rejected_atomically() {
    fixture::Options options{};
    options.legacy_color_preview_depth = std::uint16_t{5};
    options.legacy_button_frame_place_object3 = true;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);
    ERUI_TEST_CHECK(count_ascii(source_bytes, "ColorPreview") == 1);

    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.color_picker =
            SettingsGfxPresentation::ensure_character_creation});
    ERUI_TEST_CHECK(!transformed.success());
    check_error(
        transformed.error,
        SettingsGfxErrorCode::resource_conflict,
        SettingsGfxFeature::color_picker);
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
    ERUI_TEST_CHECK(count_ascii(serialize(source), "ColorPreview") == 1);
}

void test_widget_only_route_survives_missing_camera_panel() {
    fixture::Options options{};
    options.omit_camera_panel = true;
    const GfxDocument source = parse(fixture::make(options));
    const auto source_bytes = serialize(source);

    const auto full_inspection = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(!full_inspection.success());
    check_error(
        full_inspection.error,
        SettingsGfxErrorCode::structure_not_found,
        SettingsGfxFeature::camera_options_rows);

    const auto widget_inspection =
        erui::gfx::inspect_settings_widgets_gfx(source);
    ERUI_TEST_CHECK(widget_inspection.success());
    ERUI_TEST_CHECK(
        widget_inspection.inspection.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(
        widget_inspection.inspection.text_input_sprite ==
        options.ids.text_input);
    ERUI_TEST_CHECK(
        widget_inspection.inspection.color_picker_host_count == 1);

    const SettingsGfxRequirements requirements{
        .text_input =
            SettingsGfxPresentation::ensure_character_creation,
        .color_picker =
            SettingsGfxPresentation::ensure_character_creation,
    };
    const auto full_transform = erui::gfx::transform_settings_gfx(
        source, requirements);
    ERUI_TEST_CHECK(!full_transform.success());
    check_error(
        full_transform.error,
        SettingsGfxErrorCode::structure_not_found,
        SettingsGfxFeature::camera_options_rows);

    const auto widget_transform =
        erui::gfx::transform_settings_widgets_gfx(source, requirements);
    ERUI_TEST_CHECK(widget_transform.success());
    ERUI_TEST_CHECK(widget_transform.changed());
    ERUI_TEST_CHECK(
        widget_transform.after.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(widget_transform.after.character_name_text_input);
    ERUI_TEST_CHECK(widget_transform.after.character_creation_color_picker);
    ERUI_TEST_CHECK(serialize(source) == source_bytes);

    const auto transformed_bytes = serialize(widget_transform.document);
    const auto second = erui::gfx::transform_settings_widgets_gfx(
        widget_transform.document, requirements);
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(!second.changed());
    ERUI_TEST_CHECK(serialize(second.document) == transformed_bytes);
}

void test_widget_only_route_rejects_row_requirements_atomically() {
    const GfxDocument source = parse(fixture::make());
    const auto source_bytes = serialize(source);

    const auto game_rows = erui::gfx::transform_settings_widgets_gfx(
        source,
        {.minimum_game_options_rows = std::uint16_t{6}});
    ERUI_TEST_CHECK(!game_rows.success());
    check_error(
        game_rows.error,
        SettingsGfxErrorCode::invalid_argument,
        SettingsGfxFeature::game_options_rows);
    ERUI_TEST_CHECK(!game_rows.document.valid());
    ERUI_TEST_CHECK(serialize(source) == source_bytes);

    const auto camera_rows = erui::gfx::transform_settings_widgets_gfx(
        source,
        {.minimum_camera_options_rows = std::uint16_t{7}});
    ERUI_TEST_CHECK(!camera_rows.success());
    check_error(
        camera_rows.error,
        SettingsGfxErrorCode::invalid_argument,
        SettingsGfxFeature::camera_options_rows);
    ERUI_TEST_CHECK(!camera_rows.document.valid());
    ERUI_TEST_CHECK(serialize(source) == source_bytes);
}

void test_rejected_structures() {
    {
        fixture::Options options{};
        options.omit_camera_panel = true;
        const auto result = erui::gfx::inspect_settings_gfx(
            parse(fixture::make(options)));
        ERUI_TEST_CHECK(!result.success());
        check_error(
            result.error,
            SettingsGfxErrorCode::structure_not_found,
            SettingsGfxFeature::camera_options_rows);
    }
    {
        fixture::Options options{};
        options.duplicate_game_panel = true;
        const auto result = erui::gfx::inspect_settings_gfx(
            parse(fixture::make(options)));
        ERUI_TEST_CHECK(!result.success());
        check_error(
            result.error,
            SettingsGfxErrorCode::ambiguous_structure,
            SettingsGfxFeature::game_options_rows);
    }
    {
        fixture::Options options{};
        options.gap_game_rows = true;
        const auto result = erui::gfx::inspect_settings_gfx(
            parse(fixture::make(options)));
        ERUI_TEST_CHECK(!result.success());
        check_error(
            result.error,
            SettingsGfxErrorCode::incompatible_structure,
            SettingsGfxFeature::game_options_rows);
    }
    {
        const GfxDocument invalid{};
        const auto result = erui::gfx::inspect_settings_gfx(invalid);
        ERUI_TEST_CHECK(!result.success());
        check_error(
            result.error,
            SettingsGfxErrorCode::invalid_document,
            SettingsGfxFeature::document);
    }
}

[[maybe_unused]] void check_real_asset_no_op(
    const char* path,
    SettingsGfxHost expected_host) {
    const auto bytes = read_file(path);
    const GfxDocument document = parse(bytes);
    const auto inspection = erui::gfx::inspect_settings_gfx(document);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(inspection.inspection.host == expected_host);

    const auto no_op = erui::gfx::transform_settings_gfx(document, {});
    ERUI_TEST_CHECK(no_op.success());
    ERUI_TEST_CHECK(!no_op.changed());
    ERUI_TEST_CHECK(no_op.status == SettingsGfxTransformStatus::unchanged);
    ERUI_TEST_CHECK(serialize(no_op.document) == bytes);
}

#if defined(ERNATIVEUI_TEST_GAME_OPTIONS_GFX)
void test_real_game_options_tab_growth_contract() {
    const auto bytes = read_file(ERNATIVEUI_TEST_GAME_OPTIONS_GFX);
    const GfxDocument source = parse(bytes);
    const auto before = erui::gfx::inspect_settings_gfx(source);
    ERUI_TEST_CHECK(before.success());
    ERUI_TEST_CHECK(before.inspection.host == SettingsGfxHost::game_options);
    ERUI_TEST_CHECK(before.inspection.tab_list_cells == 9);
    ERUI_TEST_CHECK(before.inspection.back_tab_list_cells == 9);
    ERUI_TEST_CHECK(before.inspection.authored_tab_slot_capacity == 8);
    const auto do_abc_before = body_for_unique_tag(source, 82);
    const auto symbol_class_before = body_for_unique_tag(source, 76);

    const auto transformed = erui::gfx::transform_settings_gfx(
        source,
        {.minimum_tab_slots = std::uint16_t{9}});
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.changed());
    ERUI_TEST_CHECK(transformed.after.tab_list_cells == 10);
    ERUI_TEST_CHECK(transformed.after.back_tab_list_cells == 10);
    ERUI_TEST_CHECK(transformed.after.authored_tab_slot_capacity == 9);

    for (std::uint16_t index = 0; index < 9U; ++index) {
        const auto* front = named_placement(
            transformed.document,
            transformed.after.tab_list_sprite,
            "Item_0_" + std::to_string(index));
        const auto* back = named_placement(
            transformed.document,
            transformed.after.back_tab_list_sprite,
            "Item_0_" + std::to_string(index));
        ERUI_TEST_CHECK(front != nullptr);
        ERUI_TEST_CHECK(back != nullptr);
        ERUI_TEST_CHECK(front->depth ==
            static_cast<std::uint16_t>(59U - index * 6U));
        ERUI_TEST_CHECK(back->depth ==
            static_cast<std::uint16_t>(34U - index * 4U));
    }
    const auto* front_new = named_placement(
        transformed.document,
        transformed.after.tab_list_sprite,
        "Item_0_9");
    const auto* back_new = named_placement(
        transformed.document,
        transformed.after.back_tab_list_sprite,
        "Item_0_9");
    const auto* text = named_placement(
        transformed.document,
        transformed.after.tab_list_sprite,
        "Text_0");
    ERUI_TEST_CHECK(front_new != nullptr && front_new->depth == 5);
    ERUI_TEST_CHECK(back_new != nullptr && back_new->depth == 38);
    ERUI_TEST_CHECK(text != nullptr && text->depth == 65);
    ERUI_TEST_CHECK(
        transformed.document.tag(front_new->tag)->ordinal == 12);
    ERUI_TEST_CHECK(
        transformed.document.tag(back_new->tag)->ordinal == 10);
    ERUI_TEST_CHECK(
        body_for_unique_tag(transformed.document, 82) == do_abc_before);
    ERUI_TEST_CHECK(
        body_for_unique_tag(transformed.document, 76) == symbol_class_before);

    const auto serialized = serialize(transformed.document);
    const GfxDocument reparsed = parse(serialized);
    const auto verified = erui::gfx::inspect_settings_gfx(reparsed);
    ERUI_TEST_CHECK(verified.success());
    ERUI_TEST_CHECK(verified.inspection.authored_tab_slot_capacity == 9);
    ERUI_TEST_CHECK(serialize(source) == bytes);
}
#endif

#if defined(ERNATIVEUI_TEST_ADVANCED_GFX)
void test_advanced_camera_row_request_reports_camera_feature() {
    const auto bytes = read_file(ERNATIVEUI_TEST_ADVANCED_GFX);
    const GfxDocument document = parse(bytes);
    const auto inspection = erui::gfx::inspect_settings_gfx(document);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(
        inspection.inspection.host == SettingsGfxHost::advanced_settings);

    const auto transformed = erui::gfx::transform_settings_gfx(
        document,
        {.minimum_camera_options_rows = std::uint16_t{7}});
    ERUI_TEST_CHECK(!transformed.success());
    check_error(
        transformed.error,
        SettingsGfxErrorCode::unsupported_document,
        SettingsGfxFeature::camera_options_rows);
    ERUI_TEST_CHECK(serialize(document) == bytes);
}
#endif

} // namespace

int main() {
    test_structural_inspection_and_remapped_ids();
    test_tab_slot_growth_is_monotonic_atomic_and_idempotent();
    test_tab_slot_two_cell_growth_is_atomic_exact_and_upgradeable();
    test_tab_slot_two_cell_growth_fails_before_publication();
    test_tab_slot_growth_rejects_unrepresentable_matrix_translation();
    test_tab_slot_contract_rejects_ambiguous_or_mismatched_layers();
    test_tab_slot_growth_requires_the_proven_timeline_contract();
    test_nested_settings_decoy_is_not_a_host();
    test_row_expansion_idempotence_and_preservation();
    test_button_label_and_atomic_late_failure();
    test_row_growth_allocates_after_unrelated_depth();
    test_row_growth_prefers_and_preserves_authored_donor();
    test_all_ordinary_panel_layouts_expand_without_cross_contamination();
    test_text_input_and_color_picker_presentations();
    test_generated_presentation_matches_independent_golden_closure();
    test_duplicate_character_frame_closure_fails_closed();
    test_legacy_color_preview_migration();
    test_widget_host_definition_order_beats_numeric_ids();
    test_color_picker_dependency_order_is_repaired_atomically();
    test_color_picker_repair_preserves_earlier_text_input_consumer();
    test_many_widget_hosts_share_button_deterministically();
    test_generated_color_picker_body_budget_fails_atomically();
    test_malformed_existing_picker_is_rejected();
    test_nested_character_frame_decoy_and_imported_ids();
    test_resource_after_use_is_repaired_not_accepted();
    test_request_validation_and_text_input_scope();
    test_panel_pair_must_share_one_top_level_carrier();
    test_panel_timeline_impostors_are_rejected_before_no_op_growth();
    test_button_timeline_uses_the_first_frame_boundary();
    test_place_object3_legacy_button_frame_is_rejected_atomically();
    test_widget_only_route_survives_missing_camera_panel();
    test_widget_only_route_rejects_row_requirements_atomically();
    test_rejected_structures();

#if defined(ERNATIVEUI_TEST_GAME_OPTIONS_GFX)
    check_real_asset_no_op(
        ERNATIVEUI_TEST_GAME_OPTIONS_GFX,
        SettingsGfxHost::game_options);
    test_real_game_options_tab_growth_contract();
#endif

#if defined(ERNATIVEUI_TEST_ADVANCED_GFX)
    test_advanced_camera_row_request_reports_camera_feature();
    check_real_asset_no_op(
        ERNATIVEUI_TEST_ADVANCED_GFX,
        SettingsGfxHost::advanced_settings);
#endif
    return 0;
}
