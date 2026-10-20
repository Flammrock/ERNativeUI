#include "gfx/title_gfx_transformer.hpp"

#include "test_assertions.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using erui::gfx::GfxDocument;
using erui::gfx::GfxEdit;
using erui::gfx::GfxRawTag;
using erui::gfx::TitleGfxErrorCode;
using erui::gfx::TitleGfxFeature;
using erui::gfx::TitleGfxCorner;
using erui::gfx::TitleGfxDlcFieldProof;
using erui::gfx::TitleGfxDlcState;
using erui::gfx::TitleGfxRgb;
using erui::gfx::TitleGfxTextLine;
using erui::gfx::TitleGfxTextOccupancy;
using erui::gfx::TitleGfxTextSide;
using erui::gfx::TitleGfxTransformStatus;

constexpr std::uint16_t kTagEnd = 0;
constexpr std::uint16_t kTagShowFrame = 1;
constexpr std::uint16_t kTagPlaceObject = 4;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagDefineEditText = 37;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kTagPlaceObject3 = 70;
constexpr std::uint16_t kUnknownTag = 777;

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& output) noexcept
        : output_(output) {}

    void write(std::uint32_t value, std::size_t count) {
        for (std::size_t remaining = count; remaining != 0U; --remaining) {
            if (bit_offset_ == 0U) output_.push_back(0);
            output_.back() |= static_cast<std::uint8_t>(
                ((value >> (remaining - 1U)) & 1U) <<
                (7U - bit_offset_));
            bit_offset_ = (bit_offset_ + 1U) & 7U;
        }
    }

    void write_signed(std::int32_t value, std::size_t count) {
        const std::uint32_t mask = count == 32U
            ? std::numeric_limits<std::uint32_t>::max()
            : ((std::uint32_t{1} << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() noexcept { bit_offset_ = 0; }

private:
    std::vector<std::uint8_t>& output_;
    std::uint8_t bit_offset_{};
};

[[nodiscard]] std::size_t signed_bits(std::int32_t value) noexcept {
    for (std::size_t count = 1U; count < 32U; ++count) {
        const auto minimum = -(std::int64_t{1} << (count - 1U));
        const auto maximum = (std::int64_t{1} << (count - 1U)) - 1;
        if (value >= minimum && value <= maximum) return count;
    }
    return 32U;
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void write_u32(
    std::vector<std::uint8_t>& output,
    std::size_t offset,
    std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value);
    output[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    output[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
    output[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

void append_rect(
    std::vector<std::uint8_t>& output,
    std::int32_t x_min,
    std::int32_t x_max,
    std::int32_t y_min,
    std::int32_t y_max) {
    const std::size_t count = std::max({
        signed_bits(x_min),
        signed_bits(x_max),
        signed_bits(y_min),
        signed_bits(y_max)});
    BitWriter writer(output);
    writer.write(static_cast<std::uint32_t>(count), 5U);
    writer.write_signed(x_min, count);
    writer.write_signed(x_max, count);
    writer.write_signed(y_min, count);
    writer.write_signed(y_max, count);
    writer.align();
}

void append_matrix(
    std::vector<std::uint8_t>& output,
    std::int32_t x,
    std::int32_t y) {
    const std::size_t count = std::max(signed_bits(x), signed_bits(y));
    BitWriter writer(output);
    writer.write(0U, 1U);
    writer.write(0U, 1U);
    writer.write(static_cast<std::uint32_t>(count), 5U);
    writer.write_signed(x, count);
    writer.write_signed(y, count);
    writer.align();
}

void append_matrix_with_components(
    std::vector<std::uint8_t>& output,
    std::int32_t x,
    std::int32_t y,
    std::optional<std::array<std::int32_t, 2>> scale,
    std::optional<std::array<std::int32_t, 2>> skew,
    bool zero_bit_components = false) {
    BitWriter writer(output);
    writer.write(scale ? 1U : 0U, 1U);
    if (scale) {
        const std::size_t count = zero_bit_components
            ? 0U
            : std::max(signed_bits((*scale)[0]), signed_bits((*scale)[1]));
        writer.write(static_cast<std::uint32_t>(count), 5U);
        writer.write_signed((*scale)[0], count);
        writer.write_signed((*scale)[1], count);
    }
    writer.write(skew ? 1U : 0U, 1U);
    if (skew) {
        const std::size_t count = zero_bit_components
            ? 0U
            : std::max(signed_bits((*skew)[0]), signed_bits((*skew)[1]));
        writer.write(static_cast<std::uint32_t>(count), 5U);
        writer.write_signed((*skew)[0], count);
        writer.write_signed((*skew)[1], count);
    }
    const std::size_t translation_bits =
        std::max(signed_bits(x), signed_bits(y));
    writer.write(static_cast<std::uint32_t>(translation_bits), 5U);
    writer.write_signed(x, translation_bits);
    writer.write_signed(y, translation_bits);
    writer.align();
}

void append_color_transform(
    std::vector<std::uint8_t>& output,
    std::int32_t red_add) {
    BitWriter writer(output);
    writer.write(1U, 1U); // HasAddTerms
    writer.write(0U, 1U); // HasMultTerms
    const std::size_t count = signed_bits(red_add);
    writer.write(static_cast<std::uint32_t>(count), 4U);
    writer.write_signed(red_add, count);
    writer.write_signed(0, count);
    writer.write_signed(0, count);
    writer.write_signed(0, count);
    writer.align();
}

[[nodiscard]] std::vector<std::uint8_t> encoded_tag(
    std::uint16_t code,
    std::span<const std::uint8_t> body) {
    std::vector<std::uint8_t> output{};
    output.reserve(body.size() + 6U);
    if (body.size() < 0x3FU) {
        append_u16(
            output,
            static_cast<std::uint16_t>((code << 6U) | body.size()));
    } else {
        append_u16(
            output,
            static_cast<std::uint16_t>((code << 6U) | 0x3FU));
        append_u32(output, static_cast<std::uint32_t>(body.size()));
    }
    for (const std::uint8_t byte : body) output.push_back(byte);
    return output;
}

void append_tag(
    std::vector<std::uint8_t>& output,
    std::uint16_t code,
    std::span<const std::uint8_t> body = {}) {
    const auto encoded = encoded_tag(code, body);
    output.insert(output.end(), encoded.begin(), encoded.end());
}

[[nodiscard]] GfxRawTag edit_text(
    std::uint16_t character_id,
    std::string_view initial_text = "ONLINEEE",
    std::array<std::uint8_t, 4> color = {0xFFU, 0xFFU, 0xFFU, 0xFFU},
    std::uint16_t font_height = 320U,
    std::string_view variable = {},
    bool html = false,
    std::int32_t x_min = -40,
    std::int32_t x_max = 9340) {
    GfxRawTag tag{.code = kTagDefineEditText};
    append_u16(tag.body, character_id);
    append_rect(tag.body, x_min, x_max, -40, 520);
    tag.body.push_back(0xECU);
    tag.body.push_back(html ? 0xA3U : 0xA1U);
    constexpr std::string_view font_class = "MenuFont_01";
    tag.body.insert(tag.body.end(), font_class.begin(), font_class.end());
    tag.body.push_back(0);
    append_u16(tag.body, font_height);
    tag.body.insert(tag.body.end(), color.begin(), color.end());
    tag.body.push_back(1U); // right-aligned
    tag.body.insert(tag.body.end(), 8U, 0U);
    tag.body.insert(tag.body.end(), variable.begin(), variable.end());
    tag.body.push_back(0);
    tag.body.insert(tag.body.end(), initial_text.begin(), initial_text.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag named_placement(
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x26U);
    append_u16(tag.body, depth);
    append_u16(tag.body, character_id);
    append_matrix(tag.body, x, y);
    tag.body.insert(tag.body.end(), name.begin(), name.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag legacy_placement(
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y) {
    GfxRawTag tag{.code = kTagPlaceObject};
    append_u16(tag.body, character_id);
    append_u16(tag.body, depth);
    append_matrix(tag.body, x, y);
    return tag;
}

[[nodiscard]] GfxRawTag named_scaled_placement(
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name,
    std::array<std::int32_t, 2> scale,
    std::optional<std::array<std::int32_t, 2>> skew = std::nullopt,
    bool zero_bit_components = false) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x26U);
    append_u16(tag.body, depth);
    append_u16(tag.body, character_id);
    append_matrix_with_components(
        tag.body, x, y, scale, skew, zero_bit_components);
    tag.body.insert(tag.body.end(), name.begin(), name.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag named_color_transform_placement(
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name,
    std::int32_t red_add) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x2EU);
    append_u16(tag.body, depth);
    append_u16(tag.body, character_id);
    append_matrix(tag.body, x, y);
    append_color_transform(tag.body, red_add);
    tag.body.insert(tag.body.end(), name.begin(), name.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag named_move_placement(
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    GfxRawTag tag{.code = kTagPlaceObject2};
    tag.body.push_back(0x25U); // Move | HasName | HasMatrix.
    append_u16(tag.body, depth);
    append_matrix(tag.body, x, y);
    tag.body.insert(tag.body.end(), name.begin(), name.end());
    tag.body.push_back(0);
    return tag;
}

[[nodiscard]] GfxRawTag sprite(
    std::uint16_t character_id,
    std::span<const GfxRawTag> children) {
    GfxRawTag tag{.code = kTagDefineSprite};
    append_u16(tag.body, character_id);
    append_u16(tag.body, 1U);
    for (const auto& child : children) {
        append_tag(tag.body, child.code, child.body);
    }
    append_tag(tag.body, kTagShowFrame);
    append_tag(tag.body, kTagEnd);
    return tag;
}

struct FixtureOptions {
    bool third_party_line{};
    bool third_party_above{};
    bool third_party_extreme_top{};
    bool legacy_info_placement{};
    bool opaque_info_place_object3{};
    bool duplicate_info{};
    bool partial_private{};
    bool incompatible_native_style{};
    bool html_native_field{};
    bool maximum_depth{};
    bool third_party_move_only{};
    bool native_color_transform{};
    bool native_zero_bit_matrix_components{};
    bool overlapping_corner_boxes{};
    bool repurposed_dlc_field{};
};

[[nodiscard]] std::vector<std::uint8_t> title_fixture(
    const FixtureOptions& options = {}) {
    constexpr std::array<std::string_view, 4> names{
        "RegulationVersion", "AppVersion", "OnlineMode", "DLCVersion_0"};
    constexpr std::array<std::int32_t, 4> y{1236, 757, 278, -680};
    const std::int32_t native_x = options.overlapping_corner_boxes
        ? -15260
        : -4640;
    const std::int32_t native_x_max = options.overlapping_corner_boxes
        ? 19960
        : 9340;

    std::vector<GfxRawTag> top_level{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        top_level.push_back(edit_text(
            static_cast<std::uint16_t>(100U + index),
            options.repurposed_dlc_field && index == 3U
                ? "Mod-owned footer"
                : "ONLINEEE",
            {0xFFU, 0xFFU, 0xFFU, 0xFFU},
            options.incompatible_native_style && index == 3U ? 321U : 320U,
            {},
            options.html_native_field && index == 0U,
            -40,
            native_x_max));
    }
    if (options.third_party_line) {
        top_level.push_back(edit_text(
            200U,
            "Third-party footer",
            {0xD8U, 0xA7U, 0x67U, 0xFFU},
            320U,
            "OtherModVersion"));
    }
    if (options.partial_private) {
        top_level.push_back(edit_text(
            201U,
            "Orphan",
            {0xFFU, 0xFFU, 0xFFU, 0xFFU},
            320U,
            "ERNativeUI_TitleTextValue_0000"));
    }

    std::vector<GfxRawTag> info_children{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (options.native_color_transform) {
            info_children.push_back(named_color_transform_placement(
                static_cast<std::uint16_t>(100U + index),
                static_cast<std::uint16_t>(index + 1U),
                native_x,
                y[index],
                names[index],
                1));
        } else if (options.native_zero_bit_matrix_components) {
            info_children.push_back(named_scaled_placement(
                static_cast<std::uint16_t>(100U + index),
                static_cast<std::uint16_t>(index + 1U),
                native_x,
                y[index],
                names[index],
                {0, 0},
                std::array<std::int32_t, 2>{0, 0},
                true));
        } else {
            info_children.push_back(named_placement(
                static_cast<std::uint16_t>(100U + index),
                static_cast<std::uint16_t>(index + 1U),
                native_x,
                y[index],
                names[index]));
        }
    }
    if (options.third_party_line) {
        const std::int32_t third_party_y = options.third_party_extreme_top
            ? -(std::int32_t{1} << 30)
            : (options.third_party_above ? -1159 : 1710);
        info_children.push_back(named_placement(
            200U, 120U, -4640, third_party_y, "OtherModFooter"));
    }
    if (options.maximum_depth) {
        info_children.push_back(named_placement(
            100U,
            std::numeric_limits<std::uint16_t>::max(),
            -4640,
            1236,
            "MaximumDepthOccupant"));
    }
    if (options.third_party_move_only) {
        info_children.push_back(named_move_placement(
            121U, -4640, -1159, "OtherModMove"));
    }
    if (options.legacy_info_placement) {
        info_children.push_back(legacy_placement(
            100U, 122U, -4640, -2000));
    }
    if (options.opaque_info_place_object3) {
        info_children.push_back({
            .code = kTagPlaceObject3,
            .body = {0x00U, 0x00U, 0x7AU},
        });
    }
    top_level.push_back(sprite(300U, info_children));

    std::array<GfxRawTag, 1> root_child{
        named_placement(300U, 20U, 12582, 8138, "Info")};
    top_level.push_back(sprite(301U, root_child));
    top_level.push_back(named_placement(
        301U, 130U, 19200, 10800, "Top"));
    if (options.duplicate_info) {
        std::vector<GfxRawTag> duplicate_children{};
        for (std::size_t index = 0; index < names.size(); ++index) {
            duplicate_children.push_back(named_placement(
                static_cast<std::uint16_t>(100U + index),
                static_cast<std::uint16_t>(index + 1U),
                -4640,
                y[index],
                names[index]));
        }
        top_level.push_back(sprite(302U, duplicate_children));
        std::array<GfxRawTag, 1> duplicate_root{
            named_placement(302U, 21U, 12582, 8138, "Info")};
        top_level.push_back(sprite(303U, duplicate_root));
    }
    top_level.push_back({.code = kUnknownTag, .body = {0xA5U, 0x5AU}});

    std::vector<std::uint8_t> movie{
        static_cast<std::uint8_t>('G'),
        static_cast<std::uint8_t>('F'),
        static_cast<std::uint8_t>('X'),
        10U,
        0U,
        0U,
        0U,
        0U};
    append_rect(movie, 0, 38400, 0, 21600);
    append_u16(movie, 0x1E00U);
    append_u16(movie, 1U);
    for (const auto& tag : top_level) append_tag(movie, tag.code, tag.body);
    append_tag(movie, kTagShowFrame);
    append_tag(movie, kTagEnd);
    write_u32(movie, 4U, static_cast<std::uint32_t>(movie.size()));
    return movie;
}

[[nodiscard]] GfxDocument parse(std::span<const std::uint8_t> bytes) {
    auto result = GfxDocument::parse(bytes);
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.document.valid());
    return std::move(result.document);
}

[[nodiscard]] std::vector<std::uint8_t> serialize(
    const GfxDocument& document) {
    auto result = document.serialize_and_validate();
    ERUI_TEST_CHECK(result.success());
    return std::move(result.bytes);
}

[[nodiscard]] const erui::gfx::GfxPlacementInfo* named_placement_in(
    const GfxDocument& document,
    std::uint16_t owner_id,
    std::string_view name) {
    const auto owner = document.sprite_by_character_id(owner_id);
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

[[nodiscard]] std::vector<std::uint8_t> raw_body_for_character(
    const GfxDocument& document,
    std::uint16_t character_id) {
    const auto definition = document.character_definition(character_id);
    ERUI_TEST_CHECK(definition.has_value());
    auto copied = document.copy_tag(definition->tag);
    ERUI_TEST_CHECK(copied.success());
    return std::move(copied.tag.body);
}

[[nodiscard]] GfxRawTag raw_tag(
    const GfxDocument& document,
    erui::gfx::GfxTagHandle handle) {
    auto copied = document.copy_tag(handle);
    ERUI_TEST_CHECK(copied.success());
    return std::move(copied.tag);
}

[[nodiscard]] bool body_contains(
    std::span<const std::uint8_t> body,
    std::string_view text) {
    return std::search(
               body.begin(),
               body.end(),
               text.begin(),
               text.end(),
               [](std::uint8_t left, char right) {
                   return left == static_cast<std::uint8_t>(right);
               }) != body.end();
}

[[nodiscard]] std::array<std::uint8_t, 4> color_after_font_class(
    std::span<const std::uint8_t> body) {
    constexpr std::string_view font = "MenuFont_01";
    const auto found = std::search(
        body.begin(),
        body.end(),
        font.begin(),
        font.end(),
        [](std::uint8_t left, char right) {
            return left == static_cast<std::uint8_t>(right);
        });
    ERUI_TEST_CHECK(found != body.end());
    const auto offset = static_cast<std::size_t>(
        std::distance(body.begin(), found)) + font.size() + 1U + 2U;
    ERUI_TEST_CHECK(body.size() - offset >= 4U);
    return {body[offset], body[offset + 1U], body[offset + 2U], body[offset + 3U]};
}

[[nodiscard]] std::uint8_t flags_before_font_class(
    std::span<const std::uint8_t> body) {
    constexpr std::string_view font = "MenuFont_01";
    const auto found = std::search(
        body.begin(),
        body.end(),
        font.begin(),
        font.end(),
        [](std::uint8_t left, char right) {
            return left == static_cast<std::uint8_t>(right);
        });
    ERUI_TEST_CHECK(found != body.end());
    ERUI_TEST_CHECK(std::distance(body.begin(), found) >= 2);
    return *std::prev(found, 2);
}

[[nodiscard]] std::uint8_t alignment_after_font_class(
    std::span<const std::uint8_t> body) {
    constexpr std::string_view font = "MenuFont_01";
    const auto found = std::search(
        body.begin(),
        body.end(),
        font.begin(),
        font.end(),
        [](std::uint8_t left, char right) {
            return left == static_cast<std::uint8_t>(right);
        });
    ERUI_TEST_CHECK(found != body.end());
    const auto offset = static_cast<std::size_t>(
        std::distance(body.begin(), found)) +
        font.size() + 1U + 2U + 4U;
    ERUI_TEST_CHECK(offset < body.size());
    return body[offset];
}

void test_inspection_is_structural() {
    const auto document = parse(title_fixture());
    const auto result = erui::gfx::inspect_title_gfx(document);
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.inspection.info_sprite == 300U);
    ERUI_TEST_CHECK(result.inspection.native_text_character == 100U);
    ERUI_TEST_CHECK(result.inspection.native_maximum_depth == 4U);
    ERUI_TEST_CHECK(result.inspection.text_x == -4640);
    ERUI_TEST_CHECK(result.inspection.first_injected_y == -1159);
    ERUI_TEST_CHECK(result.inspection.line_spacing == 479);
    ERUI_TEST_CHECK(
        (result.inspection.native_color == TitleGfxRgb{255U, 255U, 255U}));
    ERUI_TEST_CHECK(result.inspection.native_alpha == 255U);
    ERUI_TEST_CHECK(result.inspection.existing_private_lines == 0U);
}

void test_all_four_corners_and_dlc_occupancy() {
    const std::array<TitleGfxTextLine, 8> lines{{
        {"TL first", std::nullopt, TitleGfxCorner::top_left},
        {"BR first", std::nullopt, TitleGfxCorner::bottom_right},
        {"TR first", std::nullopt, TitleGfxCorner::top_right},
        {"BL first", std::nullopt, TitleGfxCorner::bottom_left},
        {"TL second", std::nullopt, TitleGfxCorner::top_left},
        {"BR second", std::nullopt, TitleGfxCorner::bottom_right},
        {"TR second", std::nullopt, TitleGfxCorner::top_right},
        {"BL second", std::nullopt, TitleGfxCorner::bottom_left},
    }};
    auto transformed = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        lines,
        {.dlc_state = TitleGfxDlcState::installed});
    ERUI_TEST_CHECK(transformed.success());

    struct Expected {
        std::string_view code;
        std::size_t index;
        std::int32_t x;
        std::int32_t y;
        std::uint8_t alignment;
    };
    const std::array expected{
        Expected{"TL", 0U, -29824, -17992, 0U},
        Expected{"BR", 1U, -4640, -1638, 1U},
        Expected{"TR", 2U, -4640, -17992, 1U},
        Expected{"BL", 3U, -29824, 757, 0U},
        Expected{"TL", 4U, -29824, -17513, 0U},
        Expected{"BR", 5U, -4640, -1159, 1U},
        Expected{"TR", 6U, -4640, -17513, 1U},
        Expected{"BL", 7U, -29824, 1236, 0U},
    };
    for (const auto& value : expected) {
        const std::string name = "ERNativeUI_TitleText_" +
            std::string(value.code) + "_000" +
            std::to_string(value.index);
        const auto* placement = named_placement_in(
            transformed.document, 300U, name);
        ERUI_TEST_CHECK(placement != nullptr);
        ERUI_TEST_CHECK(placement->translation.x == value.x);
        ERUI_TEST_CHECK(placement->translation.y == value.y);
        ERUI_TEST_CHECK(alignment_after_font_class(
            raw_body_for_character(
                transformed.document, placement->character_id)) ==
            value.alignment);
    }

    const std::array<TitleGfxTextLine, 2> absent_lines{{
        {"First", std::nullopt, TitleGfxCorner::bottom_right},
        {"Second", std::nullopt, TitleGfxCorner::bottom_right},
    }};
    auto absent = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        absent_lines,
        {
            .dlc_state = TitleGfxDlcState::absent,
            .dlc_field_proof =
                TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
        });
    ERUI_TEST_CHECK(absent.success());
    const auto* first = named_placement_in(
        absent.document, 300U, "ERNativeUI_TitleText_BR_0000");
    const auto* second = named_placement_in(
        absent.document, 300U, "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(first != nullptr && second != nullptr);
    ERUI_TEST_CHECK(first->translation.y == -680);
    ERUI_TEST_CHECK(second->translation.y == -201);

    auto repeated = erui::gfx::transform_title_gfx(
        std::move(absent.document),
        absent_lines,
        {
            .dlc_state = TitleGfxDlcState::absent,
            .dlc_field_proof =
                TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
        });
    ERUI_TEST_CHECK(repeated.success());
    ERUI_TEST_CHECK(repeated.status == TitleGfxTransformStatus::unchanged);
}

void test_dlc_row_reuse_requires_canonical_field_proof() {
    const std::array<TitleGfxTextLine, 2> lines{{
        {"First", std::nullopt, TitleGfxCorner::bottom_right},
        {"Second", std::nullopt, TitleGfxCorner::bottom_right},
    }};

    // A structurally compatible modified movie may retain the native field
    // name while assigning it another runtime purpose. DLC absence alone must
    // not let the transformer overlap content it cannot semantically identify.
    auto modified = erui::gfx::transform_title_gfx(
        parse(title_fixture({.repurposed_dlc_field = true})),
        lines,
        {.dlc_state = TitleGfxDlcState::absent});
    ERUI_TEST_CHECK(modified.success());
    const auto* modified_first = named_placement_in(
        modified.document, 300U, "ERNativeUI_TitleText_BR_0000");
    const auto* modified_second = named_placement_in(
        modified.document, 300U, "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(modified_first != nullptr && modified_second != nullptr);
    ERUI_TEST_CHECK(modified_first->translation.y == -1638);
    ERUI_TEST_CHECK(modified_second->translation.y == -1159);
    ERUI_TEST_CHECK(body_contains(
        raw_body_for_character(modified.document, 103U),
        "Mod-owned footer"));

    // Even a canonical-field proof does not free anything while installation
    // state is unknown. Unknown always follows the conservative layout.
    auto unknown = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        lines,
        {
            .dlc_state = TitleGfxDlcState::unknown,
            .dlc_field_proof =
                TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
        });
    ERUI_TEST_CHECK(unknown.success());
    const auto* unknown_first = named_placement_in(
        unknown.document, 300U, "ERNativeUI_TitleText_BR_0000");
    const auto* unknown_second = named_placement_in(
        unknown.document, 300U, "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(unknown_first != nullptr && unknown_second != nullptr);
    ERUI_TEST_CHECK(unknown_first->translation.y == -1638);
    ERUI_TEST_CHECK(unknown_second->translation.y == -1159);

    // Both independent facts are required: the DLC is absent and this exact
    // field is proven to retain the native DLC-status semantics.
    auto proven = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        lines,
        {
            .dlc_state = TitleGfxDlcState::absent,
            .dlc_field_proof =
                TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
        });
    ERUI_TEST_CHECK(proven.success());
    const auto* proven_first = named_placement_in(
        proven.document, 300U, "ERNativeUI_TitleText_BR_0000");
    const auto* proven_second = named_placement_in(
        proven.document, 300U, "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(proven_first != nullptr && proven_second != nullptr);
    ERUI_TEST_CHECK(proven_first->translation.y == -680);
    ERUI_TEST_CHECK(proven_second->translation.y == -201);
}

void test_external_text_occupancy_reserves_only_its_observed_row() {
    const std::array<TitleGfxTextLine, 2> lines{{
        {"First", std::nullopt, TitleGfxCorner::bottom_right},
        {"Second", std::nullopt, TitleGfxCorner::bottom_right},
    }};
    const std::array occupancy{
        TitleGfxTextOccupancy{TitleGfxTextSide::right, 0}};
    const erui::gfx::TitleGfxTransformOptions options{
        .dlc_state = TitleGfxDlcState::absent,
        .dlc_field_proof =
            TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
        .external_text_occupancy = occupancy,
    };

    auto transformed = erui::gfx::transform_title_gfx(
        parse(title_fixture()), lines, options);
    ERUI_TEST_CHECK(transformed.success());
    const auto* first = named_placement_in(
        transformed.document, 300U, "ERNativeUI_TitleText_BR_0000");
    const auto* second = named_placement_in(
        transformed.document, 300U, "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(first != nullptr && second != nullptr);
    ERUI_TEST_CHECK(first->translation.y == -1159);
    ERUI_TEST_CHECK(second->translation.y == -680);

    // The same external row is already covered by the conservative native
    // DLC reservation. Installed and unknown layouts therefore retain their
    // existing two-line placement instead of gaining an unnecessary gap.
    for (const TitleGfxDlcState dlc_state : {
             TitleGfxDlcState::installed,
             TitleGfxDlcState::unknown}) {
        auto conservative = erui::gfx::transform_title_gfx(
            parse(title_fixture()),
            lines,
            {
                .dlc_state = dlc_state,
                .dlc_field_proof =
                    TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
                .external_text_occupancy = occupancy,
            });
        ERUI_TEST_CHECK(conservative.success());
        const auto* conservative_first = named_placement_in(
            conservative.document, 300U, "ERNativeUI_TitleText_BR_0000");
        const auto* conservative_second = named_placement_in(
            conservative.document, 300U, "ERNativeUI_TitleText_BR_0001");
        ERUI_TEST_CHECK(
            conservative_first != nullptr && conservative_second != nullptr);
        ERUI_TEST_CHECK(conservative_first->translation.y == -1638);
        ERUI_TEST_CHECK(conservative_second->translation.y == -1159);
    }

    auto repeated = erui::gfx::transform_title_gfx(
        std::move(transformed.document), lines, options);
    ERUI_TEST_CHECK(repeated.success());
    ERUI_TEST_CHECK(repeated.status == TitleGfxTransformStatus::unchanged);

    const std::array<TitleGfxTextLine, 1> left_line{{
        {"Left", std::nullopt, TitleGfxCorner::bottom_left},
    }};
    auto left = erui::gfx::transform_title_gfx(
        parse(title_fixture()), left_line, options);
    ERUI_TEST_CHECK(left.success());
    const auto* left_placement = named_placement_in(
        left.document, 300U, "ERNativeUI_TitleText_BL_0000");
    ERUI_TEST_CHECK(left_placement != nullptr);
    ERUI_TEST_CHECK(left_placement->translation.y == 1236);

    const std::array duplicate_occupancy{
        occupancy[0], occupancy[0]};
    auto duplicate = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        lines,
        {
            .dlc_state = TitleGfxDlcState::absent,
            .dlc_field_proof =
                TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree,
            .external_text_occupancy = duplicate_occupancy,
        });
    ERUI_TEST_CHECK(duplicate.success());
    const auto* duplicate_first = named_placement_in(
        duplicate.document, 300U, "ERNativeUI_TitleText_BR_0000");
    ERUI_TEST_CHECK(duplicate_first != nullptr);
    ERUI_TEST_CHECK(duplicate_first->translation.y == -1159);
}

void test_external_text_occupancy_is_validated_transactionally() {
    const std::array<TitleGfxTextLine, 1> lines{{
        {"Line", std::nullopt, TitleGfxCorner::bottom_right},
    }};
    const std::array invalid_side{
        TitleGfxTextOccupancy{
            static_cast<TitleGfxTextSide>(0xFFU), 0}};
    auto invalid = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        lines,
        {.external_text_occupancy = invalid_side});
    ERUI_TEST_CHECK(!invalid.success());
    ERUI_TEST_CHECK(invalid.error.code ==
        TitleGfxErrorCode::invalid_argument);
    ERUI_TEST_CHECK(invalid.error.feature == TitleGfxFeature::text_line);
    ERUI_TEST_CHECK(invalid.error.detail == 10U);
    ERUI_TEST_CHECK(invalid.error.line_index == 0U);

    const std::array off_frame{
        TitleGfxTextOccupancy{
            TitleGfxTextSide::right,
            (std::numeric_limits<std::int32_t>::max)()}};
    auto outside = erui::gfx::transform_title_gfx(
        parse(title_fixture()),
        lines,
        {.external_text_occupancy = off_frame});
    ERUI_TEST_CHECK(!outside.success());
    ERUI_TEST_CHECK(outside.error.code ==
        TitleGfxErrorCode::invalid_argument);
    ERUI_TEST_CHECK(outside.error.feature == TitleGfxFeature::text_line);
    ERUI_TEST_CHECK(outside.error.detail == 11U);
    ERUI_TEST_CHECK(outside.error.line_index == 0U);
}

void test_multiple_lines_color_order_and_idempotence() {
    auto original = parse(title_fixture());
    const auto original_bytes = serialize(original);
    const auto* original_native = named_placement_in(
        original, 300U, "OnlineMode");
    ERUI_TEST_CHECK(original_native != nullptr);
    std::int32_t original_top = std::numeric_limits<std::int32_t>::max();
    for (const auto& placement : original.placements()) {
        if (placement.owner_sprite == original_native->owner_sprite &&
            placement.has_matrix) {
            original_top = std::min(original_top, placement.translation.y);
        }
    }
    ERUI_TEST_CHECK(original_top != std::numeric_limits<std::int32_t>::max());
    const std::array<TitleGfxTextLine, 3> lines{{
        {"First <footer> & literal", std::nullopt, TitleGfxCorner::bottom_right},
        {"Solid-style yellow", TitleGfxRgb{216U, 167U, 103U},
         TitleGfxCorner::bottom_right},
        {"UTF-8: Caf\xC3\xA9", TitleGfxRgb{12U, 34U, 56U},
         TitleGfxCorner::bottom_right},
    }};
    auto result = erui::gfx::transform_title_gfx(original, lines);
    if (!result.success()) {
        std::fprintf(
            stderr,
            "transform failed: %s/%s detail=%u line=%u document=%s\n",
            erui::gfx::title_gfx_error_name(result.error.code),
            erui::gfx::title_gfx_feature_name(result.error.feature),
            result.error.detail,
            result.error.line_index,
            erui::gfx::gfx_document_error_name(
                result.error.document_error.code));
    }
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.status == TitleGfxTransformStatus::modified);
    ERUI_TEST_CHECK(result.after.existing_private_lines == lines.size());
    ERUI_TEST_CHECK(serialize(original) == original_bytes);

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string name =
            "ERNativeUI_TitleText_BR_000" + std::to_string(index);
        const auto* placement = named_placement_in(
            result.document, result.after.info_sprite, name);
        ERUI_TEST_CHECK(placement != nullptr);
        ERUI_TEST_CHECK(placement->depth == 5U + index);
        ERUI_TEST_CHECK(placement->translation.x == -4640);
        ERUI_TEST_CHECK(
            placement->translation.y == -2117 + static_cast<int>(index) * 479);
        ERUI_TEST_CHECK(placement->translation.y < original_top);
        const auto body = raw_body_for_character(
            result.document, placement->character_id);
        ERUI_TEST_CHECK(body_contains(body, lines[index].text));
        ERUI_TEST_CHECK(body_contains(
            body, "ERNativeUI_TitleTextValue_BR_000" + std::to_string(index)));
        ERUI_TEST_CHECK(!body_contains(body, "DLCVersion_"));
    }
    const auto* first = named_placement_in(
        result.document, result.after.info_sprite,
        "ERNativeUI_TitleText_BR_0000");
    const auto* second = named_placement_in(
        result.document, result.after.info_sprite,
        "ERNativeUI_TitleText_BR_0001");
    const auto* third = named_placement_in(
        result.document, result.after.info_sprite,
        "ERNativeUI_TitleText_BR_0002");
    ERUI_TEST_CHECK(color_after_font_class(raw_body_for_character(
        result.document, first->character_id)) ==
        (std::array<std::uint8_t, 4>{255U, 255U, 255U, 255U}));
    ERUI_TEST_CHECK(color_after_font_class(raw_body_for_character(
        result.document, second->character_id)) ==
        (std::array<std::uint8_t, 4>{216U, 167U, 103U, 255U}));
    ERUI_TEST_CHECK(color_after_font_class(raw_body_for_character(
        result.document, third->character_id)) ==
        (std::array<std::uint8_t, 4>{12U, 34U, 56U, 255U}));
    ERUI_TEST_CHECK((flags_before_font_class(raw_body_for_character(
        result.document, 100U)) & 0x60U) == 0x60U);
    ERUI_TEST_CHECK((flags_before_font_class(raw_body_for_character(
        result.document, first->character_id)) & 0x60U) == 0U);

    const auto once = serialize(result.document);
    auto second_pass = erui::gfx::transform_title_gfx(
        std::move(result.document), lines);
    ERUI_TEST_CHECK(second_pass.success());
    ERUI_TEST_CHECK(second_pass.status == TitleGfxTransformStatus::unchanged);
    ERUI_TEST_CHECK(serialize(second_pass.document) == once);
}

void test_third_party_content_is_preserved_and_avoided() {
    auto document = parse(title_fixture({
        .third_party_line = true,
        .third_party_above = true,
    }));
    const auto third_party_before = raw_body_for_character(document, 200U);
    const std::array<TitleGfxTextLine, 2> lines{{
        {"Our first", std::nullopt, TitleGfxCorner::bottom_right},
        {"Our second", std::nullopt, TitleGfxCorner::bottom_right},
    }};
    auto result = erui::gfx::transform_title_gfx(
        std::move(document), lines);
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.after.existing_private_lines == 2U);
    const auto* third_party = named_placement_in(
        result.document, 300U, "OtherModFooter");
    ERUI_TEST_CHECK(third_party != nullptr);
    ERUI_TEST_CHECK(third_party->depth == 120U);
    ERUI_TEST_CHECK(third_party->translation.y == -1159);
    ERUI_TEST_CHECK(raw_body_for_character(result.document, 200U) ==
        third_party_before);

    const auto* first = named_placement_in(
        result.document, 300U, "ERNativeUI_TitleText_BR_0000");
    const auto* second = named_placement_in(
        result.document, 300U, "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(first != nullptr && second != nullptr);
    ERUI_TEST_CHECK(first->depth == 121U);
    ERUI_TEST_CHECK(second->depth == 122U);
    ERUI_TEST_CHECK(first->translation.y == -2117);
    ERUI_TEST_CHECK(second->translation.y == -1638);

    std::size_t unknown_count{};
    for (const auto& tag : result.document.tags()) {
        if (tag.code != kUnknownTag) continue;
        auto raw = result.document.copy_tag(tag.handle);
        ERUI_TEST_CHECK(raw.success());
        ERUI_TEST_CHECK(raw.tag.body ==
            (std::vector<std::uint8_t>{0xA5U, 0x5AU}));
        ++unknown_count;
    }
    ERUI_TEST_CHECK(unknown_count == 1U);
}

void test_existing_private_lines_must_remain_canonical() {
    const std::array<TitleGfxTextLine, 1> lines{{
        {"Canonical line", std::nullopt, TitleGfxCorner::bottom_right},
    }};
    auto transformed = erui::gfx::transform_title_gfx(
        parse(title_fixture()), lines);
    ERUI_TEST_CHECK(transformed.success());
    const auto* placement = named_placement_in(
        transformed.document,
        transformed.after.info_sprite,
        "ERNativeUI_TitleText_BR_0000");
    ERUI_TEST_CHECK(placement != nullptr);
    const GfxRawTag moved = named_placement(
        placement->character_id,
        placement->depth,
        placement->translation.x,
        placement->translation.y + 17,
        placement->name);
    const std::array edits{
        GfxEdit::replace(placement->tag, moved),
    };
    ERUI_TEST_CHECK(transformed.document.apply(edits).success());

    const auto rejected = erui::gfx::transform_title_gfx(
        std::move(transformed.document), lines);
    ERUI_TEST_CHECK(!rejected.success());
    ERUI_TEST_CHECK(
        rejected.error.code == TitleGfxErrorCode::resource_conflict);
    ERUI_TEST_CHECK(rejected.error.feature == TitleGfxFeature::text_line);
}

void test_private_resource_bytes_must_remain_canonical() {
    const std::array<TitleGfxTextLine, 1> lines{{
        {"Canonical line", std::nullopt, TitleGfxCorner::bottom_right},
    }};

    {
        auto transformed = erui::gfx::transform_title_gfx(
            parse(title_fixture()), lines);
        ERUI_TEST_CHECK(transformed.success());
        const auto* placement = named_placement_in(
            transformed.document,
            transformed.after.info_sprite,
            "ERNativeUI_TitleText_BR_0000");
        ERUI_TEST_CHECK(placement != nullptr);
        const GfxRawTag scaled = named_scaled_placement(
            placement->character_id,
            placement->depth,
            placement->translation.x,
            placement->translation.y,
            placement->name,
            {65536, 65536});
        const std::array edits{GfxEdit::replace(placement->tag, scaled)};
        ERUI_TEST_CHECK(transformed.document.apply(edits).success());

        const auto rejected = erui::gfx::transform_title_gfx(
            std::move(transformed.document), lines);
        ERUI_TEST_CHECK(!rejected.success());
        ERUI_TEST_CHECK(
            rejected.error.code == TitleGfxErrorCode::resource_conflict);
    }

    {
        auto transformed = erui::gfx::transform_title_gfx(
            parse(title_fixture({.native_color_transform = true})), lines);
        ERUI_TEST_CHECK(transformed.success());
        const auto* placement = named_placement_in(
            transformed.document,
            transformed.after.info_sprite,
            "ERNativeUI_TitleText_BR_0000");
        ERUI_TEST_CHECK(placement != nullptr);
        const GfxRawTag changed_color_transform =
            named_color_transform_placement(
                placement->character_id,
                placement->depth,
                placement->translation.x,
                placement->translation.y,
                placement->name,
                2);
        const std::array edits{
            GfxEdit::replace(placement->tag, changed_color_transform)};
        ERUI_TEST_CHECK(transformed.document.apply(edits).success());

        const auto rejected = erui::gfx::transform_title_gfx(
            std::move(transformed.document), lines);
        ERUI_TEST_CHECK(!rejected.success());
        ERUI_TEST_CHECK(
            rejected.error.code == TitleGfxErrorCode::resource_conflict);
    }

    {
        auto transformed = erui::gfx::transform_title_gfx(
            parse(title_fixture()), lines);
        ERUI_TEST_CHECK(transformed.success());
        const auto* placement = named_placement_in(
            transformed.document,
            transformed.after.info_sprite,
            "ERNativeUI_TitleText_BR_0000");
        ERUI_TEST_CHECK(placement != nullptr);
        const auto definition = transformed.document.character_definition(
            placement->character_id);
        ERUI_TEST_CHECK(definition.has_value());
        GfxRawTag changed_definition = raw_tag(
            transformed.document, definition->tag);
        constexpr std::string_view font = "MenuFont_01";
        const auto found = std::search(
            changed_definition.body.begin(),
            changed_definition.body.end(),
            font.begin(),
            font.end(),
            [](std::uint8_t left, char right) {
                return left == static_cast<std::uint8_t>(right);
            });
        ERUI_TEST_CHECK(found != changed_definition.body.end());
        ERUI_TEST_CHECK(
            std::distance(changed_definition.body.begin(), found) >= 2);
        *std::prev(found, 2) |= 0x60U;
        const std::array edits{
            GfxEdit::replace(definition->tag, std::move(changed_definition))};
        ERUI_TEST_CHECK(transformed.document.apply(edits).success());

        const auto rejected = erui::gfx::transform_title_gfx(
            std::move(transformed.document), lines);
        ERUI_TEST_CHECK(!rejected.success());
        ERUI_TEST_CHECK(
            rejected.error.code == TitleGfxErrorCode::resource_conflict);
    }
}

void test_move_only_placements_fail_open() {
    const std::array<TitleGfxTextLine, 1> lines{{
        {"After move-only content", std::nullopt,
         TitleGfxCorner::bottom_right},
    }};
    auto transformed = erui::gfx::transform_title_gfx(
        parse(title_fixture({.third_party_move_only = true})), lines);
    ERUI_TEST_CHECK(!transformed.success());
    ERUI_TEST_CHECK(
        transformed.error.code == TitleGfxErrorCode::incompatible_structure);
}

void test_zero_scale_matrix_components_fail_open() {
    const std::array<TitleGfxTextLine, 1> lines{{
        {"Zero-bit matrix", std::nullopt, TitleGfxCorner::bottom_right},
    }};
    auto transformed = erui::gfx::transform_title_gfx(
        parse(title_fixture({.native_zero_bit_matrix_components = true})),
        lines);
    ERUI_TEST_CHECK(!transformed.success());
    ERUI_TEST_CHECK(
        transformed.error.code == TitleGfxErrorCode::incompatible_structure);
}

void test_exact_sixteen_line_boundary() {
    std::vector<std::string> storage{};
    storage.reserve(16U);
    for (std::size_t index = 0; index < 16U; ++index) {
        storage.push_back("Line " + std::to_string(index));
    }
    std::vector<TitleGfxTextLine> lines{};
    lines.reserve(storage.size());
    for (const auto& text : storage) {
        lines.push_back({
            text, std::nullopt, TitleGfxCorner::bottom_right});
    }

    auto transformed = erui::gfx::transform_title_gfx(
        parse(title_fixture()), lines);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.after.existing_private_lines == 16U);
    const auto* first = named_placement_in(
        transformed.document,
        transformed.after.info_sprite,
        "ERNativeUI_TitleText_BR_0000");
    const auto* last = named_placement_in(
        transformed.document,
        transformed.after.info_sprite,
        "ERNativeUI_TitleText_BR_0015");
    ERUI_TEST_CHECK(first != nullptr && last != nullptr);
    ERUI_TEST_CHECK(first->depth == 5U);
    ERUI_TEST_CHECK(last->depth == 20U);
    ERUI_TEST_CHECK(first->translation.y == -8344);
    ERUI_TEST_CHECK(last->translation.y == -1159);

    std::vector<TitleGfxTextLine> too_many(
        17U,
        {"one too many", std::nullopt, TitleGfxCorner::bottom_right});
    const auto rejected = erui::gfx::transform_title_gfx(
        parse(title_fixture()), too_many);
    ERUI_TEST_CHECK(!rejected.success());
    ERUI_TEST_CHECK(
        rejected.error.code == TitleGfxErrorCode::invalid_argument);
}

void test_off_frame_incoming_content_fails_transactionally() {
    auto original = parse(title_fixture({
        .third_party_line = true,
        .third_party_extreme_top = true,
    }));
    const auto original_bytes = serialize(original);
    const std::array lines{TitleGfxTextLine{
        "Cannot fit above", std::nullopt, TitleGfxCorner::bottom_right}};

    const auto transformed = erui::gfx::transform_title_gfx(original, lines);
    ERUI_TEST_CHECK(!transformed.success());
    ERUI_TEST_CHECK(
        transformed.error.code ==
        TitleGfxErrorCode::incompatible_structure);
    ERUI_TEST_CHECK(serialize(original) == original_bytes);
}

void test_exact_text_byte_boundary() {
    const std::string maximum_text(4096U, 'x');
    const std::array maximum_line{
        TitleGfxTextLine{
            maximum_text, std::nullopt, TitleGfxCorner::bottom_right}};
    auto accepted = erui::gfx::transform_title_gfx(
        parse(title_fixture()), maximum_line);
    ERUI_TEST_CHECK(accepted.success());
    const auto* placement = named_placement_in(
        accepted.document,
        accepted.after.info_sprite,
        "ERNativeUI_TitleText_BR_0000");
    ERUI_TEST_CHECK(placement != nullptr);
    ERUI_TEST_CHECK(body_contains(
        raw_body_for_character(accepted.document, placement->character_id),
        maximum_text));

    const std::string too_long(4097U, 'x');
    const std::array excessive_line{
        TitleGfxTextLine{
            too_long, std::nullopt, TitleGfxCorner::bottom_right}};
    const auto rejected = erui::gfx::transform_title_gfx(
        parse(title_fixture()), excessive_line);
    ERUI_TEST_CHECK(!rejected.success());
    ERUI_TEST_CHECK(
        rejected.error.code == TitleGfxErrorCode::invalid_argument);
}

void test_unicode_line_separators_are_rejected() {
    const std::array<std::string, 3> separators{{
        std::string{"before\xC2\x85" "after"},
        std::string{"before\xE2\x80\xA8" "after"},
        std::string{"before\xE2\x80\xA9" "after"},
    }};
    for (const auto& text : separators) {
        auto document = parse(title_fixture());
        const std::array line{TitleGfxTextLine{
            text, std::nullopt, TitleGfxCorner::bottom_right}};
        const auto rejected = erui::gfx::transform_title_gfx(document, line);
        ERUI_TEST_CHECK(!rejected.success());
        ERUI_TEST_CHECK(
            rejected.error.code == TitleGfxErrorCode::invalid_argument);
    }
}

void test_requested_text_can_alias_source_index_storage() {
    auto document = parse(title_fixture());
    const auto* native = named_placement_in(document, 300U, "OnlineMode");
    ERUI_TEST_CHECK(native != nullptr);
    const std::string_view aliased_text = native->name;
    const std::array lines{
        TitleGfxTextLine{
            aliased_text,
            TitleGfxRgb{1U, 2U, 3U},
            TitleGfxCorner::bottom_right}};

    auto transformed = erui::gfx::transform_title_gfx(
        std::move(document), lines);
    ERUI_TEST_CHECK(transformed.success());
    const auto* private_line = named_placement_in(
        transformed.document,
        transformed.after.info_sprite,
        "ERNativeUI_TitleText_BR_0000");
    ERUI_TEST_CHECK(private_line != nullptr);
    ERUI_TEST_CHECK(body_contains(
        raw_body_for_character(
            transformed.document, private_line->character_id),
        "OnlineMode"));
}

void test_invalid_input_and_conflicts_fail_without_touching_source() {
    const auto base_bytes = title_fixture();
    const auto check_unchanged = [&](const GfxDocument& document) {
        ERUI_TEST_CHECK(serialize(document) == base_bytes);
    };

    {
        auto document = parse(base_bytes);
        const std::array lines{TitleGfxTextLine{
            "", std::nullopt, TitleGfxCorner::bottom_right}};
        const auto result = erui::gfx::transform_title_gfx(document, lines);
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(result.error.code == TitleGfxErrorCode::invalid_argument);
        ERUI_TEST_CHECK(result.error.feature == TitleGfxFeature::text_line);
        ERUI_TEST_CHECK(result.error.line_index == 0U);
        check_unchanged(document);
    }
    {
        auto document = parse(base_bytes);
        const std::array lines{
            TitleGfxTextLine{
                "two\nlines", std::nullopt, TitleGfxCorner::bottom_right}};
        const auto result = erui::gfx::transform_title_gfx(document, lines);
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(result.error.code == TitleGfxErrorCode::invalid_argument);
        check_unchanged(document);
    }
    {
        auto document = parse(base_bytes);
        const std::string invalid_utf8{"\xC0\xAF", 2U};
        const std::array lines{
            TitleGfxTextLine{
                invalid_utf8, std::nullopt, TitleGfxCorner::bottom_right}};
        const auto result = erui::gfx::transform_title_gfx(document, lines);
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(result.error.code == TitleGfxErrorCode::invalid_argument);
        check_unchanged(document);
    }
    {
        auto document = parse(base_bytes);
        std::vector<TitleGfxTextLine> lines(
            17U, {"x", std::nullopt, TitleGfxCorner::bottom_right});
        const auto result = erui::gfx::transform_title_gfx(document, lines);
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(result.error.code == TitleGfxErrorCode::invalid_argument);
        check_unchanged(document);
    }
    {
        auto document = parse(base_bytes);
        const std::array lines{TitleGfxTextLine{
            "x", std::nullopt, TitleGfxCorner::bottom_right}};
        const auto result = erui::gfx::transform_title_gfx(
            document,
            lines,
            {
                .dlc_state = TitleGfxDlcState::absent,
                .dlc_field_proof =
                    static_cast<TitleGfxDlcFieldProof>(0xFFU),
            });
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(result.error.code == TitleGfxErrorCode::invalid_argument);
        ERUI_TEST_CHECK(result.error.feature == TitleGfxFeature::document);
        ERUI_TEST_CHECK(result.error.detail == 3U);
        check_unchanged(document);
    }
    {
        auto document = parse(title_fixture({.partial_private = true}));
        const std::array lines{TitleGfxTextLine{
            "x", std::nullopt, TitleGfxCorner::bottom_right}};
        const auto result = erui::gfx::transform_title_gfx(document, lines);
        ERUI_TEST_CHECK(!result.success());
        ERUI_TEST_CHECK(result.error.code == TitleGfxErrorCode::resource_conflict);
    }
}

void test_opaque_info_geometry_is_rejected_transactionally() {
    const std::array fixtures{
        FixtureOptions{.legacy_info_placement = true},
        FixtureOptions{.opaque_info_place_object3 = true},
    };
    const std::array lines{TitleGfxTextLine{
        "Safe prepend", std::nullopt, TitleGfxCorner::bottom_right}};

    for (const auto& options : fixtures) {
        auto original = parse(title_fixture(options));
        const auto original_bytes = serialize(original);
        const auto inspected = erui::gfx::inspect_title_gfx(original);
        ERUI_TEST_CHECK(!inspected.success());
        ERUI_TEST_CHECK(
            inspected.error.code == TitleGfxErrorCode::incompatible_structure);
        ERUI_TEST_CHECK(inspected.error.feature == TitleGfxFeature::info_sprite);

        const auto transformed = erui::gfx::transform_title_gfx(
            original, lines);
        ERUI_TEST_CHECK(!transformed.success());
        ERUI_TEST_CHECK(
            transformed.error.code ==
            TitleGfxErrorCode::incompatible_structure);
        ERUI_TEST_CHECK(serialize(original) == original_bytes);
    }
}

void test_overlapping_corner_boxes_fail_open() {
    auto original = parse(title_fixture({.overlapping_corner_boxes = true}));
    const auto original_bytes = serialize(original);

    const auto inspected = erui::gfx::inspect_title_gfx(original);
    ERUI_TEST_CHECK(!inspected.success());
    ERUI_TEST_CHECK(
        inspected.error.code == TitleGfxErrorCode::incompatible_structure);
    ERUI_TEST_CHECK(
        inspected.error.feature == TitleGfxFeature::native_text_fields);
    ERUI_TEST_CHECK(inspected.error.detail == 54U);

    const std::array lines{TitleGfxTextLine{
        "Must not be injected",
        std::nullopt,
        TitleGfxCorner::top_left}};
    const auto transformed = erui::gfx::transform_title_gfx(original, lines);
    ERUI_TEST_CHECK(!transformed.success());
    ERUI_TEST_CHECK(
        transformed.error.code == TitleGfxErrorCode::incompatible_structure);
    ERUI_TEST_CHECK(transformed.error.detail == 54U);
    ERUI_TEST_CHECK(serialize(original) == original_bytes);
}

void test_incompatible_and_ambiguous_movies_are_rejected() {
    {
        auto document = parse(title_fixture({.duplicate_info = true}));
        const auto inspected = erui::gfx::inspect_title_gfx(document);
        ERUI_TEST_CHECK(!inspected.success());
        ERUI_TEST_CHECK(
            inspected.error.code == TitleGfxErrorCode::ambiguous_structure);
    }
    {
        auto document = parse(title_fixture({
            .incompatible_native_style = true}));
        const auto inspected = erui::gfx::inspect_title_gfx(document);
        ERUI_TEST_CHECK(!inspected.success());
        ERUI_TEST_CHECK(
            inspected.error.code == TitleGfxErrorCode::incompatible_structure);
        ERUI_TEST_CHECK(
            inspected.error.feature == TitleGfxFeature::native_text_fields);
    }
    {
        auto document = parse(title_fixture({.html_native_field = true}));
        const auto inspected = erui::gfx::inspect_title_gfx(document);
        ERUI_TEST_CHECK(!inspected.success());
        ERUI_TEST_CHECK(
            inspected.error.code == TitleGfxErrorCode::incompatible_structure);
        ERUI_TEST_CHECK(
            inspected.error.feature == TitleGfxFeature::native_text_fields);
    }
    {
        auto document = parse(title_fixture({.maximum_depth = true}));
        const std::array lines{TitleGfxTextLine{
            "x", std::nullopt, TitleGfxCorner::bottom_right}};
        const auto transformed = erui::gfx::transform_title_gfx(
            std::move(document), lines);
        ERUI_TEST_CHECK(!transformed.success());
        ERUI_TEST_CHECK(
            transformed.error.code == TitleGfxErrorCode::depth_exhausted);
    }
}

[[nodiscard]] std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    ERUI_TEST_CHECK(stream.good());
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

void test_real_title_movie(const char* path) {
    auto document = parse(read_file(path));
    const auto inspected = erui::gfx::inspect_title_gfx(document);
    ERUI_TEST_CHECK(inspected.success());
    ERUI_TEST_CHECK(inspected.inspection.line_spacing > 0);

    const std::array<TitleGfxTextLine, 2> lines{{
        {"ERNativeUI test", std::nullopt, TitleGfxCorner::bottom_right},
        {"Colored test", TitleGfxRgb{216U, 167U, 103U},
         TitleGfxCorner::bottom_right},
    }};
    auto transformed = erui::gfx::transform_title_gfx(
        std::move(document), lines);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.changed());
    ERUI_TEST_CHECK(transformed.after.existing_private_lines == lines.size());
    const auto* first = named_placement_in(
        transformed.document,
        transformed.after.info_sprite,
        "ERNativeUI_TitleText_BR_0000");
    const auto* second = named_placement_in(
        transformed.document,
        transformed.after.info_sprite,
        "ERNativeUI_TitleText_BR_0001");
    ERUI_TEST_CHECK(first != nullptr && second != nullptr);
    ERUI_TEST_CHECK(
        first->translation.y + transformed.before.line_spacing ==
        second->translation.y);
    ERUI_TEST_CHECK(
        second->translation.y == transformed.before.first_injected_y);
    ERUI_TEST_CHECK(
        transformed.after.first_injected_y == first->translation.y);
    ERUI_TEST_CHECK(serialize(transformed.document).size() > 0U);
}

} // namespace

int main(int argc, char** argv) {
    test_inspection_is_structural();
    test_all_four_corners_and_dlc_occupancy();
    test_dlc_row_reuse_requires_canonical_field_proof();
    test_external_text_occupancy_reserves_only_its_observed_row();
    test_external_text_occupancy_is_validated_transactionally();
    test_multiple_lines_color_order_and_idempotence();
    test_third_party_content_is_preserved_and_avoided();
    test_existing_private_lines_must_remain_canonical();
    test_private_resource_bytes_must_remain_canonical();
    test_move_only_placements_fail_open();
    test_zero_scale_matrix_components_fail_open();
    test_exact_sixteen_line_boundary();
    test_off_frame_incoming_content_fails_transactionally();
    test_exact_text_byte_boundary();
    test_unicode_line_separators_are_rejected();
    test_requested_text_can_alias_source_index_storage();
    test_invalid_input_and_conflicts_fail_without_touching_source();
    test_opaque_info_geometry_is_rejected_transactionally();
    test_overlapping_corner_boxes_fail_open();
    test_incompatible_and_ambiguous_movies_are_rejected();
    if (argc > 1) test_real_title_movie(argv[1]);
    std::puts("title GFX transformer tests passed");
    return 0;
}
