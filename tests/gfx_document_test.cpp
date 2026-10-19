#include "gfx/gfx_document.hpp"

#include "test_assertions.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uint16_t kTagEnd = 0;
constexpr std::uint16_t kTagShowFrame = 1;
constexpr std::uint16_t kTagDefineShape = 2;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kTagImportAssets = 57;
constexpr std::uint16_t kTagPlaceObject3 = 70;
constexpr std::uint16_t kTagImportAssets2 = 71;
constexpr std::uint16_t kTagDefineScalingGrid = 78;
constexpr std::uint16_t kUnknownTag = 777;

struct ImportedCharacter {
    std::uint16_t id{};
    std::string_view name{};
};

class BitWriter {
public:
    void write(std::uint32_t value, std::size_t count) {
        for (std::size_t remaining = count; remaining != 0; --remaining) {
            if ((bit_position_ % 8U) == 0) bytes_.push_back(0);
            if (((value >> (remaining - 1U)) & 1U) != 0) {
                bytes_.back() |= static_cast<std::uint8_t>(
                    1U << (7U - (bit_position_ % 8U)));
            }
            ++bit_position_;
        }
    }

    void write_signed(std::int32_t value, std::size_t count) {
        const std::uint32_t mask = count == 32
            ? std::numeric_limits<std::uint32_t>::max()
            : ((1U << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() {
        while ((bit_position_ % 8U) != 0) write(0, 1);
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept {
        return bytes_;
    }

private:
    std::vector<std::uint8_t> bytes_{};
    std::size_t bit_position_{};
};

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void write_u32(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t value) {
    ERUI_TEST_CHECK(offset + 4 <= bytes.size());
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] std::size_t signed_bits(std::int32_t value) {
    for (std::size_t bits = 1; bits < 31; ++bits) {
        const auto minimum = -(1 << (bits - 1));
        const auto maximum = (1 << (bits - 1)) - 1;
        if (value >= minimum && value <= maximum) return bits;
    }
    return 31;
}

[[nodiscard]] std::vector<std::uint8_t> matrix(
    std::int32_t x,
    std::int32_t y) {
    BitWriter writer{};
    writer.write(0, 1); // HasScale.
    writer.write(0, 1); // HasRotate.
    const std::size_t bits = std::max(signed_bits(x), signed_bits(y));
    writer.write(static_cast<std::uint32_t>(bits), 5);
    writer.write_signed(x, bits);
    writer.write_signed(y, bits);
    writer.align();
    return writer.bytes();
}

[[nodiscard]] std::vector<std::uint8_t> tag(
    std::uint16_t code,
    std::span<const std::uint8_t> body = {},
    erui::gfx::GfxTagHeaderForm form =
        erui::gfx::GfxTagHeaderForm::automatic) {
    ERUI_TEST_CHECK(code <= 1023);
    ERUI_TEST_CHECK(body.size() <= std::numeric_limits<std::uint32_t>::max());

    const bool long_form =
        form == erui::gfx::GfxTagHeaderForm::long_form ||
        body.size() >= 0x3FU;
    ERUI_TEST_CHECK(
        form != erui::gfx::GfxTagHeaderForm::short_form || !long_form);

    std::vector<std::uint8_t> output{};
    if (long_form) {
        append_u16(output, static_cast<std::uint16_t>((code << 6U) | 0x3FU));
        append_u32(output, static_cast<std::uint32_t>(body.size()));
    } else {
        append_u16(output, static_cast<std::uint16_t>(
            (code << 6U) | static_cast<std::uint16_t>(body.size())));
    }
    output.insert(output.end(), body.begin(), body.end());
    return output;
}

void append(std::vector<std::uint8_t>& output, std::span<const std::uint8_t> bytes) {
    output.insert(output.end(), bytes.begin(), bytes.end());
}

void append_c_string(
    std::vector<std::uint8_t>& output,
    std::string_view value) {
    output.insert(
        output.end(),
        reinterpret_cast<const std::uint8_t*>(value.data()),
        reinterpret_cast<const std::uint8_t*>(value.data()) + value.size());
    output.push_back(0);
}

[[nodiscard]] std::vector<std::uint8_t> character_definition(
    std::uint16_t id,
    std::span<const std::uint8_t> opaque = {}) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    append(body, opaque);
    return tag(kTagDefineShape, body);
}

[[nodiscard]] std::vector<std::uint8_t> import_assets(
    std::uint16_t code,
    std::string_view url,
    std::span<const ImportedCharacter> imports,
    std::uint8_t reserved1 = 1,
    std::uint8_t reserved2 = 0) {
    ERUI_TEST_CHECK(code == kTagImportAssets || code == kTagImportAssets2);
    ERUI_TEST_CHECK(imports.size() <=
        std::numeric_limits<std::uint16_t>::max());

    std::vector<std::uint8_t> body{};
    append_c_string(body, url);
    if (code == kTagImportAssets2) {
        body.push_back(reserved1);
        body.push_back(reserved2);
    }
    append_u16(body, static_cast<std::uint16_t>(imports.size()));
    for (const ImportedCharacter& imported : imports) {
        append_u16(body, imported.id);
        append_c_string(body, imported.name);
    }
    return tag(code, body);
}

[[nodiscard]] std::vector<std::uint8_t> place_object(
    std::uint16_t depth,
    std::uint16_t character_id,
    std::string_view name,
    std::int32_t x = 0,
    std::int32_t y = 0) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x26); // HasName | HasMatrix | HasCharacter.
    append_u16(body, depth);
    append_u16(body, character_id);
    const auto transform = matrix(x, y);
    append(body, transform);
    append(body, std::span{
        reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
    body.push_back(0);
    return tag(kTagPlaceObject2, body);
}

[[nodiscard]] std::vector<std::uint8_t> place_object3(
    std::uint16_t depth,
    std::uint16_t character_id,
    std::string_view name,
    std::int32_t x = 0,
    std::int32_t y = 0,
    bool include_visibility = false,
    bool visible = true) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x26); // HasName | HasMatrix | HasCharacter.
    body.push_back(include_visibility ? 0x20 : 0x00);
    append_u16(body, depth);
    append_u16(body, character_id);
    const auto transform = matrix(x, y);
    append(body, transform);
    append(body, std::span{
        reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
    body.push_back(0);
    if (include_visibility) body.push_back(visible ? 1 : 0);
    return tag(kTagPlaceObject3, body);
}

[[nodiscard]] std::vector<std::uint8_t> place_object3_with_opaque_tail(
    std::uint16_t depth,
    std::uint16_t character_id,
    std::string_view name) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x22); // HasName | HasCharacter.
    // HasFilterList precedes HasVisible and makes the visibility byte's
    // offset unsafe without parsing the complete filter list.
    body.push_back(0x21); // HasFilterList | HasVisible.
    append_u16(body, depth);
    append_u16(body, character_id);
    append(body, std::span{
        reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
    body.push_back(0);
    body.push_back(0xA5); // Opaque producer-specific extension payload.
    body.push_back(0x5A);
    body.push_back(0x00);
    return tag(kTagPlaceObject3, body);
}

[[nodiscard]] std::vector<std::uint8_t> define_scaling_grid(
    std::uint16_t character_id) {
    std::vector<std::uint8_t> body{};
    append_u16(body, character_id);

    // A minimal valid RECT with four zero coordinates.
    BitWriter rectangle{};
    rectangle.write(1, 5);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.align();
    append(body, rectangle.bytes());
    return tag(kTagDefineScalingGrid, body);
}

[[nodiscard]] std::vector<std::uint8_t> define_sprite(
    std::uint16_t id,
    std::span<const std::vector<std::uint8_t>> children,
    std::uint16_t frame_count = 1) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    append_u16(body, frame_count);
    for (const auto& child : children) append(body, child);
    append(body, tag(kTagShowFrame));
    append(body, tag(kTagEnd));
    return tag(kTagDefineSprite, body, erui::gfx::GfxTagHeaderForm::long_form);
}

[[nodiscard]] std::vector<std::uint8_t> movie(
    std::span<const std::vector<std::uint8_t>> root_tags,
    std::span<const std::uint8_t> trailing = {},
    bool append_end = true) {
    std::vector<std::uint8_t> output{
        'G', 'F', 'X', 11,
        0, 0, 0, 0,
    };

    // A minimal valid SWF RECT, followed by frame rate and frame count.
    BitWriter rectangle{};
    rectangle.write(1, 5);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.align();
    append(output, rectangle.bytes());
    append_u16(output, 0);
    append_u16(output, 1);

    for (const auto& root_tag : root_tags) append(output, root_tag);
    if (append_end) append(output, tag(kTagEnd));
    write_u32(output, 4, static_cast<std::uint32_t>(output.size()));
    append(output, trailing);
    return output;
}

[[nodiscard]] std::vector<std::uint8_t> representative_movie() {
    const auto shape = character_definition(1, std::array<std::uint8_t, 2>{0xA5, 0x5A});
    const auto named = place_object(7, 1, "NamedChild", -120, 340);
    const std::array sprite_children{named};
    const auto sprite = define_sprite(3, sprite_children, 2);
    const auto unknown = tag(
        kUnknownTag,
        std::array<std::uint8_t, 4>{0xDE, 0xAD, 0xBE, 0xEF});
    const std::array roots{shape, sprite, unknown, tag(kTagShowFrame)};
    return movie(roots, std::array<std::uint8_t, 4>{0xFA, 0xCE, 0xB0, 0x0C});
}

[[nodiscard]] erui::gfx::GfxDocument parse_document(
    std::span<const std::uint8_t> bytes,
    erui::gfx::GfxDocumentLimits limits = {}) {
    auto result = erui::gfx::GfxDocument::parse(bytes, limits);
    ERUI_TEST_CHECK(result.success());
    ERUI_TEST_CHECK(result.document.valid());
    return std::move(result.document);
}

[[nodiscard]] erui::gfx::GfxTagInfo find_tag(
    const erui::gfx::GfxDocument& document,
    std::uint16_t code,
    bool root_only = false) {
    const auto found = std::find_if(
        document.tags().begin(),
        document.tags().end(),
        [&](const erui::gfx::GfxTagInfo& candidate) {
            return candidate.code == code &&
                (!root_only || !static_cast<bool>(candidate.parent_sprite));
        });
    ERUI_TEST_CHECK(found != document.tags().end());
    return *found;
}

void expect_parse_error(
    std::span<const std::uint8_t> bytes,
    erui::gfx::GfxDocumentErrorCode expected,
    erui::gfx::GfxDocumentLimits limits = {}) {
    const auto result = erui::gfx::GfxDocument::parse(bytes, limits);
    ERUI_TEST_CHECK(!result.success());
    ERUI_TEST_CHECK(!result.document.valid());
    if (result.error.code != expected) {
        std::fprintf(
            stderr,
            "expected GFX parse error %s, received %s (context=%s offset=%zu)\n",
            erui::gfx::gfx_document_error_name(expected),
            erui::gfx::gfx_document_error_name(result.error.code),
            erui::gfx::gfx_document_error_context_name(result.error.context),
            result.error.byte_offset);
    }
    ERUI_TEST_CHECK(result.error.code == expected);
}

#if defined(ERNATIVEUI_TEST_GAME_OPTIONS_GFX) || \
    defined(ERNATIVEUI_TEST_ADVANCED_GFX)
[[nodiscard]] std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    ERUI_TEST_CHECK(input.good());
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}
#endif

void test_parse_and_lossless_round_trip() {
    const erui::gfx::GfxDocument empty{};
    ERUI_TEST_CHECK(!empty.valid());
    ERUI_TEST_CHECK(
        empty.signature() == erui::gfx::GfxDocumentSignature::unknown);

    const auto bytes = representative_movie();
    auto document = parse_document(bytes);

    ERUI_TEST_CHECK(document.version() == 11);
    ERUI_TEST_CHECK(
        document.signature() == erui::gfx::GfxDocumentSignature::gfx);
    ERUI_TEST_CHECK(document.physical_size() == bytes.size());
    ERUI_TEST_CHECK(document.declared_length() == bytes.size() - 4);
    ERUI_TEST_CHECK(document.trailing_size() == 4);
    ERUI_TEST_CHECK(document.revision() != 0);
    ERUI_TEST_CHECK(document.sprites().size() == 1);
    ERUI_TEST_CHECK(document.placements().size() == 1);
    ERUI_TEST_CHECK(document.character_definitions().size() == 2);

    const auto sprite = document.sprite_by_character_id(3);
    ERUI_TEST_CHECK(sprite.has_value());
    ERUI_TEST_CHECK(sprite->frame_count == 2);
    ERUI_TEST_CHECK(sprite->child_count >= 3);
    ERUI_TEST_CHECK(sprite->nesting_depth == 0);
    const auto sprite_by_handle = document.sprite(sprite->definition);
    ERUI_TEST_CHECK(sprite_by_handle.has_value());
    ERUI_TEST_CHECK(sprite_by_handle->character_id == sprite->character_id);
    ERUI_TEST_CHECK(sprite_by_handle->frame_count == sprite->frame_count);

    const auto& placement = document.placements().front();
    ERUI_TEST_CHECK(placement.tag_code == kTagPlaceObject2);
    ERUI_TEST_CHECK(placement.depth == 7);
    ERUI_TEST_CHECK(placement.character_id == 1);
    ERUI_TEST_CHECK(placement.name == "NamedChild");
    ERUI_TEST_CHECK(placement.translation.x == -120);
    ERUI_TEST_CHECK(placement.translation.y == 340);
    ERUI_TEST_CHECK(placement.has_character);
    ERUI_TEST_CHECK(placement.has_matrix);
    ERUI_TEST_CHECK(placement.has_name);
    ERUI_TEST_CHECK(!placement.move);
    ERUI_TEST_CHECK(!placement.is_place_object3);
    ERUI_TEST_CHECK(placement.flags == 0x26);
    ERUI_TEST_CHECK(placement.flags2 == 0);
    ERUI_TEST_CHECK(!placement.has_visible);
    ERUI_TEST_CHECK(placement.owner_sprite == sprite->definition);

    const auto character = document.character_definition(1);
    ERUI_TEST_CHECK(character.has_value());
    ERUI_TEST_CHECK(character->tag_code == kTagDefineShape);
    ERUI_TEST_CHECK(!document.character_definition(2).has_value());

    const auto free = document.first_free_character_id();
    ERUI_TEST_CHECK(free.success());
    ERUI_TEST_CHECK(free.character_id == 2);
    const auto free_from_four = document.first_free_character_id(4);
    ERUI_TEST_CHECK(free_from_four.success());
    ERUI_TEST_CHECK(free_from_four.character_id == 4);

    const auto serialized = document.serialize_and_validate();
    ERUI_TEST_CHECK(serialized.success());
    ERUI_TEST_CHECK(serialized.bytes == bytes);
    const auto serialized_again = document.serialize_and_validate();
    ERUI_TEST_CHECK(serialized_again.success());
    ERUI_TEST_CHECK(serialized_again.bytes == serialized.bytes);

    auto reparsed = parse_document(serialized.bytes);
    const auto reserialized = reparsed.serialize_and_validate();
    ERUI_TEST_CHECK(reserialized.success());
    ERUI_TEST_CHECK(reserialized.bytes == serialized.bytes);
}

void test_uncompressed_swf_round_trip() {
    auto bytes = representative_movie();
    bytes[0] = 'F';
    bytes[1] = 'W';
    bytes[2] = 'S';

    auto document = parse_document(bytes);
    ERUI_TEST_CHECK(
        document.signature() == erui::gfx::GfxDocumentSignature::swf);
    const auto serialized = document.serialize_and_validate();
    ERUI_TEST_CHECK(serialized.success());
    ERUI_TEST_CHECK(serialized.bytes == bytes);
    auto reparsed = parse_document(serialized.bytes);
    ERUI_TEST_CHECK(
        reparsed.signature() == erui::gfx::GfxDocumentSignature::swf);

    auto compressed = bytes;
    compressed[0] = 'C';
    expect_parse_error(
        compressed,
        erui::gfx::GfxDocumentErrorCode::unsupported_signature);
    compressed[0] = 'Z';
    expect_parse_error(
        compressed,
        erui::gfx::GfxDocumentErrorCode::unsupported_signature);
}

void test_nested_sprites_and_limits() {
    const std::array no_children{tag(kUnknownTag, std::array<std::uint8_t, 1>{1})};
    const auto inner = define_sprite(12, no_children);
    const std::array outer_children{inner};
    const auto outer = define_sprite(11, outer_children);
    const std::array roots{outer};
    const auto bytes = movie(roots);

    const auto document = parse_document(bytes);
    ERUI_TEST_CHECK(document.sprites().size() == 2);
    const auto inner_info = document.sprite_by_character_id(12);
    const auto outer_info = document.sprite_by_character_id(11);
    ERUI_TEST_CHECK(inner_info.has_value());
    ERUI_TEST_CHECK(outer_info.has_value());
    ERUI_TEST_CHECK(inner_info->parent_sprite == outer_info->definition);
    ERUI_TEST_CHECK(inner_info->nesting_depth > outer_info->nesting_depth);

    auto limits = erui::gfx::GfxDocumentLimits{};
    limits.maximum_nesting_depth = 1;
    expect_parse_error(
        bytes,
        erui::gfx::GfxDocumentErrorCode::nesting_limit_exceeded,
        limits);

    limits = {};
    limits.maximum_sprites = 1;
    expect_parse_error(
        bytes,
        erui::gfx::GfxDocumentErrorCode::sprite_limit_exceeded,
        limits);

    limits = {};
    limits.maximum_tags = 2;
    expect_parse_error(
        bytes,
        erui::gfx::GfxDocumentErrorCode::tag_limit_exceeded,
        limits);

    limits = {};
    limits.maximum_nesting_depth =
        erui::gfx::GfxDocumentLimits::hard_maximum_nesting_depth + 1;
    expect_parse_error(
        bytes,
        erui::gfx::GfxDocumentErrorCode::invalid_argument,
        limits);
}

void test_recursive_sprite_length_repair() {
    const auto nested_unknown = tag(
        kUnknownTag, std::array<std::uint8_t, 1>{0x21});
    const auto inner = define_sprite(12, std::array{nested_unknown});
    const auto outer = define_sprite(11, std::array{inner});
    const auto original = movie(
        std::array{outer},
        std::array<std::uint8_t, 3>{0xC0, 0xFF, 0xEE});
    auto document = parse_document(original);

    const auto before_outer = document.sprite_by_character_id(11);
    const auto before_inner = document.sprite_by_character_id(12);
    ERUI_TEST_CHECK(before_outer.has_value());
    ERUI_TEST_CHECK(before_inner.has_value());
    ERUI_TEST_CHECK(before_inner->parent_sprite == before_outer->definition);

    const auto unknown = find_tag(document, kUnknownTag);
    ERUI_TEST_CHECK(unknown.parent_sprite == before_inner->definition);
    ERUI_TEST_CHECK(
        unknown.header_form == erui::gfx::GfxTagHeaderForm::short_form);
    const std::vector<std::uint8_t> expanded_body(100, 0x7D);
    const std::array expansion{
        erui::gfx::GfxEdit::replace_body(unknown.handle, expanded_body)};
    ERUI_TEST_CHECK(document.apply(expansion).success());

    const auto serialized = document.serialize_and_validate();
    ERUI_TEST_CHECK(serialized.success());
    // The payload grows by 99 bytes and its tag header grows from two bytes
    // to six. Both enclosing DefineSprite body lengths and the file length
    // are repaired without changing their already-long header widths.
    ERUI_TEST_CHECK(serialized.bytes.size() == original.size() + 103);
    ERUI_TEST_CHECK(std::equal(
        serialized.bytes.end() - 3,
        serialized.bytes.end(),
        std::array<std::uint8_t, 3>{0xC0, 0xFF, 0xEE}.begin()));

    auto reparsed = parse_document(serialized.bytes);
    const auto outer_after = reparsed.sprite_by_character_id(11);
    const auto inner_after = reparsed.sprite_by_character_id(12);
    ERUI_TEST_CHECK(outer_after.has_value());
    ERUI_TEST_CHECK(inner_after.has_value());
    ERUI_TEST_CHECK(inner_after->parent_sprite == outer_after->definition);
    const auto unknown_after = find_tag(reparsed, kUnknownTag);
    ERUI_TEST_CHECK(unknown_after.parent_sprite == inner_after->definition);
    ERUI_TEST_CHECK(
        unknown_after.header_form == erui::gfx::GfxTagHeaderForm::long_form);
    const auto copied = reparsed.copy_tag(unknown_after.handle);
    ERUI_TEST_CHECK(copied.success());
    ERUI_TEST_CHECK(copied.tag.body == expanded_body);

    const auto second = reparsed.serialize_and_validate();
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(second.bytes == serialized.bytes);
}

void test_place_object3_and_name_limit() {
    const auto shape = character_definition(5);
    const auto named = place_object3(
        9, 5, "ThirdPartyChild", 60, -40, true, false);
    const auto sprite = define_sprite(6, std::array{named});
    const auto bytes = movie(std::array{shape, sprite});

    const auto document = parse_document(bytes);
    ERUI_TEST_CHECK(document.placements().size() == 1);
    const auto& placement = document.placements().front();
    ERUI_TEST_CHECK(placement.is_place_object3);
    ERUI_TEST_CHECK(placement.tag_code == kTagPlaceObject3);
    ERUI_TEST_CHECK(placement.depth == 9);
    ERUI_TEST_CHECK(placement.character_id == 5);
    ERUI_TEST_CHECK(placement.name == "ThirdPartyChild");
    ERUI_TEST_CHECK(placement.translation.x == 60);
    ERUI_TEST_CHECK(placement.translation.y == -40);
    ERUI_TEST_CHECK(placement.flags == 0x26);
    ERUI_TEST_CHECK(placement.flags2 == 0x20);
    ERUI_TEST_CHECK(placement.has_visible);
    ERUI_TEST_CHECK(!placement.visible);

    const auto opaque_placement = place_object3_with_opaque_tail(
        10, 5, "OpaqueExtension");
    const auto opaque_sprite = define_sprite(7, std::array{opaque_placement});
    const auto opaque_bytes = movie(std::array{shape, opaque_sprite});
    const auto opaque_document = parse_document(opaque_bytes);
    ERUI_TEST_CHECK(opaque_document.placements().size() == 1);
    const auto& opaque = opaque_document.placements().front();
    ERUI_TEST_CHECK(opaque.depth == 10);
    ERUI_TEST_CHECK(opaque.character_id == 5);
    ERUI_TEST_CHECK(opaque.name == "OpaqueExtension");
    ERUI_TEST_CHECK(opaque.flags == 0x22);
    ERUI_TEST_CHECK(opaque.flags2 == 0x21);
    ERUI_TEST_CHECK(!opaque.has_visible);
    const auto opaque_serialized = opaque_document.serialize_and_validate();
    ERUI_TEST_CHECK(opaque_serialized.success());
    ERUI_TEST_CHECK(opaque_serialized.bytes == opaque_bytes);

    auto limits = erui::gfx::GfxDocumentLimits{};
    limits.maximum_name_bytes = 4;
    auto preserved = erui::gfx::GfxDocument::parse(bytes, limits);
    if (!preserved.success()) {
        std::fprintf(
            stderr,
            "opaque PlaceObject3 parse failed: %s (context=%s offset=%zu)\n",
            erui::gfx::gfx_document_error_name(preserved.error.code),
            erui::gfx::gfx_document_error_context_name(preserved.error.context),
            preserved.error.byte_offset);
    }
    ERUI_TEST_CHECK(preserved.success());
    ERUI_TEST_CHECK(preserved.document.placements().empty());
    const auto preserved_bytes = preserved.document.serialize_and_validate();
    ERUI_TEST_CHECK(preserved_bytes.success());
    ERUI_TEST_CHECK(preserved_bytes.bytes == bytes);

    // PlaceObject3 has extensions used by third-party GFX files. When its
    // common prefix cannot be indexed safely, the document preserves it as
    // opaque data. PlaceObject2 has no such extension ambiguity and malformed
    // names are rejected strictly.
    const auto strict_named = place_object(9, 5, "ThirdPartyChild", 60, -40);
    const auto strict_sprite = define_sprite(6, std::array{strict_named});
    const auto strict_bytes = movie(std::array{shape, strict_sprite});
    expect_parse_error(
        strict_bytes,
        erui::gfx::GfxDocumentErrorCode::malformed_placement,
        limits);
}

void test_malformed_input_and_resource_limits() {
    const auto valid = representative_movie();

    auto bad_signature = valid;
    bad_signature[0] = 'F';
    expect_parse_error(
        bad_signature,
        erui::gfx::GfxDocumentErrorCode::unsupported_signature);

    auto declared_too_large = valid;
    write_u32(
        declared_too_large,
        4,
        static_cast<std::uint32_t>(declared_too_large.size() + 1));
    expect_parse_error(
        declared_too_large,
        erui::gfx::GfxDocumentErrorCode::invalid_declared_length);

    auto limits = erui::gfx::GfxDocumentLimits{};
    limits.maximum_physical_bytes = valid.size() - 1;
    expect_parse_error(
        valid,
        erui::gfx::GfxDocumentErrorCode::input_too_large,
        limits);

    limits = {};
    limits.maximum_declared_bytes = 12;
    expect_parse_error(
        valid,
        erui::gfx::GfxDocumentErrorCode::invalid_declared_length,
        limits);

    // A one-byte tag header cannot carry the required UI16 record header.
    const std::array partial_header{std::vector<std::uint8_t>{0x00}};
    const auto truncated_header = movie(partial_header, {}, false);
    expect_parse_error(
        truncated_header,
        erui::gfx::GfxDocumentErrorCode::truncated_tag_header);

    std::vector<std::uint8_t> incomplete_long{};
    append_u16(incomplete_long, static_cast<std::uint16_t>((kUnknownTag << 6U) | 0x3FU));
    incomplete_long.push_back(0x10);
    incomplete_long.push_back(0x00);
    const std::array partial_long{incomplete_long};
    const auto truncated_long = movie(partial_long, {}, false);
    expect_parse_error(
        truncated_long,
        erui::gfx::GfxDocumentErrorCode::truncated_long_tag_length);

    std::vector<std::uint8_t> oversized_body{};
    append_u16(oversized_body, static_cast<std::uint16_t>((kUnknownTag << 6U) | 10U));
    oversized_body.push_back(0xAB);
    const std::array bad_body{oversized_body};
    const auto out_of_bounds = movie(bad_body, {}, false);
    expect_parse_error(
        out_of_bounds,
        erui::gfx::GfxDocumentErrorCode::tag_body_out_of_bounds);

    const std::array no_end_tags{tag(kTagShowFrame)};
    const auto no_end = movie(no_end_tags, {}, false);
    expect_parse_error(
        no_end,
        erui::gfx::GfxDocumentErrorCode::missing_end_tag);

    const std::array after_end_tags{tag(kTagEnd), tag(kTagShowFrame)};
    const auto after_end = movie(after_end_tags, {}, false);
    expect_parse_error(
        after_end,
        erui::gfx::GfxDocumentErrorCode::data_after_end_tag);

    const auto body_limited = movie(std::array{
        tag(kUnknownTag, std::array<std::uint8_t, 5>{1, 2, 3, 4, 5})});
    limits = {};
    limits.maximum_tag_body_bytes = 4;
    expect_parse_error(
        body_limited,
        erui::gfx::GfxDocumentErrorCode::tag_body_too_large,
        limits);

    std::vector<std::uint8_t> malformed_placement_body{0x26};
    const auto malformed_placement = tag(
        kTagPlaceObject2, malformed_placement_body);
    const std::array malformed_children{malformed_placement};
    const auto malformed_sprite = define_sprite(20, malformed_children);
    const auto malformed_movie = movie(std::array{malformed_sprite});
    expect_parse_error(
        malformed_movie,
        erui::gfx::GfxDocumentErrorCode::malformed_placement);

    // The long length itself is valid, but UINT32_MAX cannot fit inside this
    // declared movie. This also exercises overflow-safe end calculations.
    std::vector<std::uint8_t> maximum_length{};
    append_u16(maximum_length, static_cast<std::uint16_t>(
        (kUnknownTag << 6U) | 0x3FU));
    append_u32(maximum_length, std::numeric_limits<std::uint32_t>::max());
    const auto maximum_length_movie = movie(
        std::array{maximum_length}, {}, false);
    limits = {};
    limits.maximum_tag_body_bytes =
        std::numeric_limits<std::uint32_t>::max();
    expect_parse_error(
        maximum_length_movie,
        erui::gfx::GfxDocumentErrorCode::tag_body_out_of_bounds,
        limits);
}

void test_deterministic_malformed_input_sweep() {
    const auto baseline = representative_movie();
    for (std::uint32_t seed = 1; seed <= 256; ++seed) {
        std::uint32_t state = seed * 0x9E37'79B9U;
        auto next = [&state]() {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            return state;
        };

        auto candidate = baseline;
        const std::size_t mutation_count = 1U + (next() % 4U);
        for (std::size_t mutation = 0; mutation < mutation_count; ++mutation) {
            const std::size_t index = next() % candidate.size();
            const auto delta = static_cast<std::uint8_t>(1U + (next() % 255U));
            candidate[index] ^= delta;
        }
        if ((seed % 4U) == 0) {
            candidate.resize(next() % (candidate.size() + 1U));
        }

        auto parsed = erui::gfx::GfxDocument::parse(candidate);
        if (!parsed.success()) continue;

        const auto first = parsed.document.serialize_and_validate();
        if (!first.success()) {
            std::fprintf(
                stderr,
                "mutation seed %u serialized with %s\n",
                seed,
                erui::gfx::gfx_document_error_name(first.error.code));
        }
        ERUI_TEST_CHECK(first.success());
        auto reparsed = erui::gfx::GfxDocument::parse(first.bytes);
        ERUI_TEST_CHECK(reparsed.success());
        const auto second = reparsed.document.serialize_and_validate();
        ERUI_TEST_CHECK(second.success());
        ERUI_TEST_CHECK(second.bytes == first.bytes);
    }
}

void test_duplicate_and_exhausted_character_ids() {
    const auto zero = character_definition(0);
    expect_parse_error(
        movie(std::array{zero}),
        erui::gfx::GfxDocumentErrorCode::invalid_character_id);

    const auto first = character_definition(42);
    const std::array empty_children{tag(kTagShowFrame)};
    const auto duplicate = define_sprite(42, empty_children);
    const auto duplicate_movie = movie(std::array{first, duplicate});
    expect_parse_error(
        duplicate_movie,
        erui::gfx::GfxDocumentErrorCode::duplicate_character_id);

    const auto highest = character_definition(
        std::numeric_limits<std::uint16_t>::max());
    auto document = parse_document(movie(std::array{highest}));
    const auto exhausted = document.first_free_character_id(
        std::numeric_limits<std::uint16_t>::max());
    ERUI_TEST_CHECK(!exhausted.success());
    ERUI_TEST_CHECK(
        exhausted.error.code ==
        erui::gfx::GfxDocumentErrorCode::character_id_exhausted);
}

void test_multiple_character_id_allocation() {
    const auto local = character_definition(1);
    const std::array imports{ImportedCharacter{3, "Imported"}};
    const auto imported = import_assets(
        kTagImportAssets, "shared.gfx", imports);
    const auto scaleform_definition = tag(
        1009, std::array<std::uint8_t, 5>{5, 0, 0, 0, 0xA5});
    // Typed DefineExternalImage2 resources do not occupy the movie character
    // dictionary, so ID 2 must remain available.
    const auto typed_scaleform_resource = tag(
        1009, std::array<std::uint8_t, 5>{2, 0, 1, 0, 0x5A});
    auto document = parse_document(movie(std::array{
        local,
        imported,
        scaleform_definition,
        typed_scaleform_resource,
    }));

    const auto allocated = document.first_free_character_ids(5);
    ERUI_TEST_CHECK(allocated.success());
    ERUI_TEST_CHECK((allocated.character_ids ==
        std::vector<std::uint16_t>{2, 4, 6, 7, 8}));

    const auto from_four = document.first_free_character_ids(3, 4);
    ERUI_TEST_CHECK(from_four.success());
    ERUI_TEST_CHECK((from_four.character_ids ==
        std::vector<std::uint16_t>{4, 6, 7}));

    const auto empty = document.first_free_character_ids(0);
    ERUI_TEST_CHECK(empty.success());
    ERUI_TEST_CHECK(empty.character_ids.empty());

    const auto highest = character_definition(
        std::numeric_limits<std::uint16_t>::max());
    auto near_end = parse_document(movie(std::array{highest}));
    const auto before = near_end.serialize_and_validate();
    ERUI_TEST_CHECK(before.success());
    const auto exhausted = near_end.first_free_character_ids(2, 65534);
    ERUI_TEST_CHECK(!exhausted.success());
    ERUI_TEST_CHECK(exhausted.character_ids.empty());
    ERUI_TEST_CHECK(
        exhausted.error.code ==
        erui::gfx::GfxDocumentErrorCode::character_id_exhausted);
    // A failed reservation is observational only and cannot consume the one
    // remaining ID or mutate serialized document bytes.
    ERUI_TEST_CHECK(
        near_end.first_free_character_id(65534).character_id == 65534);
    const auto after = near_end.serialize_and_validate();
    ERUI_TEST_CHECK(after.success());
    ERUI_TEST_CHECK(after.bytes == before.bytes);

    const auto impossible = document.first_free_character_ids(65536);
    ERUI_TEST_CHECK(!impossible.success());
    ERUI_TEST_CHECK(impossible.character_ids.empty());
    ERUI_TEST_CHECK(
        impossible.error.code ==
        erui::gfx::GfxDocumentErrorCode::character_id_exhausted);
}

void test_character_references_reserve_allocator_ids() {
    const auto root_placement = place_object(1, 2, "OrphanPlaceObject2");
    const auto root_grid = define_scaling_grid(3);
    const auto nested_placement =
        place_object3(1, 4, "OrphanPlaceObject3");
    const auto nested_grid = define_scaling_grid(5);
    const std::array nested_tags{nested_placement, nested_grid};
    const auto owner = define_sprite(10, nested_tags);

    auto document = parse_document(movie(std::array{
        root_placement,
        root_grid,
        owner,
    }));

    // Orphan references reserve IDs for future edits, but remain references:
    // the only character definition exposed by this movie is the owner sprite.
    ERUI_TEST_CHECK(document.placements().size() == 2);
    ERUI_TEST_CHECK(document.character_definitions().size() == 1);
    ERUI_TEST_CHECK(document.character_definitions().front().character_id == 10);

    const auto allocated = document.first_free_character_ids(6);
    ERUI_TEST_CHECK(allocated.success());
    ERUI_TEST_CHECK((allocated.character_ids ==
        std::vector<std::uint16_t>{1, 6, 7, 8, 9, 11}));
    ERUI_TEST_CHECK(document.first_free_character_id(2).character_id == 6);

    // A later real definition of a referenced ID remains legal. References
    // must not be mistaken for dictionary definitions by duplicate checking.
    const auto referenced_placement = place_object(1, 21, "DefinedLater");
    const auto referenced_grid = define_scaling_grid(22);
    const auto definition21 = character_definition(21);
    const auto definition22 = character_definition(22);
    auto with_definitions = parse_document(movie(std::array{
        referenced_placement,
        referenced_grid,
        definition21,
        definition22,
    }));
    ERUI_TEST_CHECK(with_definitions.character_definitions().size() == 2);
    ERUI_TEST_CHECK(
        with_definitions.first_free_character_id(21).character_id == 23);
}

void test_scaleform_character_definitions() {
    std::vector<std::vector<std::uint8_t>> extensions{};
    // Metadata is not a movie-local resource even when its payload happens
    // to begin with a UI16 that resembles an occupied character ID.
    extensions.push_back(tag(
        1000, std::array<std::uint8_t, 3>{1, 0, 0xA0}));
    extensions.push_back(tag(
        1001, std::array<std::uint8_t, 3>{1, 0, 0xA1}));
    extensions.push_back(tag(
        1002, std::array<std::uint8_t, 3>{2, 0, 0xA2}));
    extensions.push_back(tag(
        1003, std::array<std::uint8_t, 3>{3, 0, 0xA3}));
    extensions.push_back(tag(
        1004, std::array<std::uint8_t, 3>{4, 0, 0xA4}));
    extensions.push_back(tag(
        1005, std::array<std::uint8_t, 3>{5, 0, 0xA5}));
    extensions.push_back(tag(
        1006, std::array<std::uint8_t, 3>{6, 0, 0xA6}));
    extensions.push_back(tag(
        1007, std::array<std::uint8_t, 3>{7, 0, 0xA7}));
    extensions.push_back(tag(
        1008, std::array<std::uint8_t, 3>{8, 0, 0xA8}));
    // DefineExternalImage2 only enters the movie dictionary when idType is
    // IDTYPE_NONE (zero). Typed resources use a different namespace.
    extensions.push_back(tag(
        1009, std::array<std::uint8_t, 5>{9, 0, 0, 0, 0xA9}));
    extensions.push_back(tag(
        1009, std::array<std::uint8_t, 5>{10, 0, 1, 0, 0xAA}));

    auto document = parse_document(movie(extensions));
    ERUI_TEST_CHECK(document.character_definitions().size() == 5);
    ERUI_TEST_CHECK(!erui::gfx::GfxDocument::can_define_character(1000));
    for (const auto [code, character_id] :
         std::array<std::pair<std::uint16_t, std::uint16_t>, 5>{
             {{std::uint16_t{1001}, std::uint16_t{1}},
              {std::uint16_t{1005}, std::uint16_t{5}},
              {std::uint16_t{1006}, std::uint16_t{6}},
              {std::uint16_t{1008}, std::uint16_t{8}},
              {std::uint16_t{1009}, std::uint16_t{9}}}}) {
        const auto definition = document.character_definition(character_id);
        ERUI_TEST_CHECK(definition.has_value());
        ERUI_TEST_CHECK(definition->tag_code == code);
        ERUI_TEST_CHECK(erui::gfx::GfxDocument::can_define_character(code));
    }
    for (const std::uint16_t code : std::array<std::uint16_t, 4>{
             1002, 1003, 1004, 1007}) {
        ERUI_TEST_CHECK(!erui::gfx::GfxDocument::can_define_character(code));
    }
    ERUI_TEST_CHECK(!document.character_definition(2).has_value());
    ERUI_TEST_CHECK(!document.character_definition(10).has_value());
    ERUI_TEST_CHECK(document.first_free_character_id().character_id == 2);

    std::size_t external_image2_count{};
    std::size_t external_image2_definition_count{};
    for (const auto& info : document.tags()) {
        if (info.code != 1009) continue;
        ++external_image2_count;
        if (info.defines_character) {
            ++external_image2_definition_count;
            ERUI_TEST_CHECK(info.character_id == 9);
        } else {
            ERUI_TEST_CHECK(info.character_id == 0);
        }
    }
    ERUI_TEST_CHECK(external_image2_count == 2);
    ERUI_TEST_CHECK(external_image2_definition_count == 1);

    const auto external_image = tag(
        1001, std::array<std::uint8_t, 3>{1, 0, 0xA1});
    const auto duplicate = tag(
        1009, std::array<std::uint8_t, 5>{1, 0, 0, 0, 0xD4});
    expect_parse_error(
        movie(std::array{external_image, duplicate}),
        erui::gfx::GfxDocumentErrorCode::duplicate_character_id);

    // A typed DefineExternalImage2 with the same numeric resource ID is not
    // a duplicate movie character definition.
    const auto typed_resource = tag(
        1009, std::array<std::uint8_t, 5>{1, 0, 2, 0, 0xD5});
    const auto typed_document = parse_document(
        movie(std::array{external_image, typed_resource}));
    ERUI_TEST_CHECK(typed_document.character_definitions().size() == 1);

    const auto truncated_external_image2 = tag(
        1009, std::array<std::uint8_t, 3>{11, 0, 0});
    expect_parse_error(
        movie(std::array{truncated_external_image2}),
        erui::gfx::GfxDocumentErrorCode::malformed_tag);
}

void test_imported_character_ids() {
    const auto local = character_definition(1);
    const std::array first_imports{
        ImportedCharacter{2, "ImportedShape"},
        ImportedCharacter{4, "ImportedSprite"},
    };
    const std::array second_imports{
        ImportedCharacter{3, "ImportedFont"},
        ImportedCharacter{5, "ImportedImage"},
    };
    const auto imports = import_assets(
        kTagImportAssets, "shared.gfx", first_imports);
    // Non-standard producers sometimes vary the reserved bytes. They remain
    // opaque and byte-preserved; their positions still delimit Count.
    const auto imports2 = import_assets(
        kTagImportAssets2,
        "theme.gfx",
        second_imports,
        0xFE,
        0xED);
    const auto bytes = movie(std::array{local, imports, imports2});
    auto document = parse_document(bytes);

    // Imported IDs occupy the local dictionary but are not definitions owned
    // by a tag in this movie.
    ERUI_TEST_CHECK(document.character_definitions().size() == 1);
    ERUI_TEST_CHECK(document.character_definition(1).has_value());
    ERUI_TEST_CHECK(!document.character_definition(2).has_value());
    ERUI_TEST_CHECK(document.first_free_character_id().character_id == 6);
    const auto serialized = document.serialize_and_validate();
    ERUI_TEST_CHECK(serialized.success());
    ERUI_TEST_CHECK(serialized.bytes == bytes);

    const std::array colliding_import{
        ImportedCharacter{1, "ConflictsWithLocal"},
    };
    expect_parse_error(
        movie(std::array{
            local,
            import_assets(kTagImportAssets, "duplicate.gfx", colliding_import)}),
        erui::gfx::GfxDocumentErrorCode::duplicate_character_id);

    const std::array duplicate_imports{
        ImportedCharacter{7, "First"},
        ImportedCharacter{7, "Second"},
    };
    expect_parse_error(
        movie(std::array{
            import_assets(kTagImportAssets2, "duplicate.gfx", duplicate_imports)}),
        erui::gfx::GfxDocumentErrorCode::duplicate_character_id);

    const std::array zero_import{
        ImportedCharacter{0, "Invalid"},
    };
    expect_parse_error(
        movie(std::array{
            import_assets(kTagImportAssets, "invalid.gfx", zero_import)}),
        erui::gfx::GfxDocumentErrorCode::invalid_character_id);

    const auto malformed_import2 = tag(
        kTagImportAssets2,
        std::array<std::uint8_t, 3>{'x', 0, 1});
    expect_parse_error(
        movie(std::array{malformed_import2}),
        erui::gfx::GfxDocumentErrorCode::malformed_tag);
}

void test_transactional_edits_and_handle_revisions() {
    auto document = parse_document(representative_movie());
    const auto original = document.serialize_and_validate();
    ERUI_TEST_CHECK(original.success());

    const auto sprite = document.sprite_by_character_id(3);
    ERUI_TEST_CHECK(sprite.has_value());
    const auto stale_sprite_handle = sprite->definition;
    const std::uint64_t initial_revision = document.revision();

    auto invalid_kind = erui::gfx::GfxEdit::erase(sprite->definition);
    invalid_kind.kind = static_cast<erui::gfx::GfxEditKind>(0xFF);
    ERUI_TEST_CHECK(
        document.apply(std::array{invalid_kind}).code ==
        erui::gfx::GfxDocumentErrorCode::invalid_edit);
    ERUI_TEST_CHECK(document.revision() == initial_revision);

    auto invalid_header = erui::gfx::GfxEdit::insert_before(
        sprite->definition,
        {
            .code = kUnknownTag,
            .body = {1, 2, 3},
            .header_form = static_cast<erui::gfx::GfxTagHeaderForm>(0xFF),
        });
    ERUI_TEST_CHECK(
        document.apply(std::array{invalid_header}).code ==
        erui::gfx::GfxDocumentErrorCode::invalid_edit);
    ERUI_TEST_CHECK(document.revision() == initial_revision);

    const std::array frame_edit{
        erui::gfx::GfxEdit::set_sprite_frame_count(sprite->definition, 9)};
    const auto frame_error = document.apply(frame_edit);
    ERUI_TEST_CHECK(frame_error.success());
    ERUI_TEST_CHECK(document.revision() == initial_revision + 1);
    ERUI_TEST_CHECK(
        document.sprite_by_character_id(3)->frame_count == 9);
    ERUI_TEST_CHECK(!document.tag(stale_sprite_handle).has_value());
    const auto stale_copy = document.copy_tag(stale_sprite_handle);
    ERUI_TEST_CHECK(!stale_copy.success());
    ERUI_TEST_CHECK(
        stale_copy.error.code == erui::gfx::GfxDocumentErrorCode::stale_handle);

    const auto before_failed_batch = document.serialize_and_validate();
    ERUI_TEST_CHECK(before_failed_batch.success());
    const auto unknown = find_tag(document, kUnknownTag, true);
    auto nonexistent = unknown.handle;
    nonexistent.token = std::numeric_limits<std::uint64_t>::max();
    const std::array failed_batch{
        erui::gfx::GfxEdit::replace_body(unknown.handle, {1, 2, 3}),
        erui::gfx::GfxEdit::erase(nonexistent),
    };
    const auto failed_error = document.apply(failed_batch);
    ERUI_TEST_CHECK(!failed_error.success());
    ERUI_TEST_CHECK(
        failed_error.code == erui::gfx::GfxDocumentErrorCode::tag_not_found);
    ERUI_TEST_CHECK(document.revision() == initial_revision + 1);
    const auto after_failed_batch = document.serialize_and_validate();
    ERUI_TEST_CHECK(after_failed_batch.success());
    ERUI_TEST_CHECK(after_failed_batch.bytes == before_failed_batch.bytes);

    // A duplicate character definition is structurally valid as a raw tag,
    // but must be rejected by full output reparsing without a partial commit.
    const auto end = find_tag(document, kTagEnd, true);
    const std::vector<std::uint8_t> duplicate_body{1, 0};
    const std::array duplicate_edit{
        erui::gfx::GfxEdit::insert_before(
            end.handle,
            {
                .code = kTagDefineShape,
                .body = duplicate_body,
                .header_form = erui::gfx::GfxTagHeaderForm::automatic,
            })};
    const auto duplicate_error = document.apply(duplicate_edit);
    ERUI_TEST_CHECK(!duplicate_error.success());
    ERUI_TEST_CHECK(
        duplicate_error.code ==
            erui::gfx::GfxDocumentErrorCode::duplicate_character_id);
    const auto after_duplicate = document.serialize_and_validate();
    ERUI_TEST_CHECK(after_duplicate.success());
    ERUI_TEST_CHECK(after_duplicate.bytes == before_failed_batch.bytes);

    const auto current_sprite = document.sprite_by_character_id(3);
    ERUI_TEST_CHECK(current_sprite.has_value());
    const std::array idempotent_frame_edit{
        erui::gfx::GfxEdit::set_sprite_frame_count(
            current_sprite->definition, 9)};
    const auto idempotent_error = document.apply(idempotent_frame_edit);
    ERUI_TEST_CHECK(idempotent_error.success());
    const auto idempotent_bytes = document.serialize_and_validate();
    ERUI_TEST_CHECK(idempotent_bytes.success());
    ERUI_TEST_CHECK(idempotent_bytes.bytes == before_failed_batch.bytes);
}

void test_handles_are_document_scoped() {
    auto first = parse_document(representative_movie());
    auto second = parse_document(representative_movie());
    const auto first_unknown = find_tag(first, kUnknownTag, true);
    const auto second_before = second.serialize_and_validate();
    ERUI_TEST_CHECK(second_before.success());

    ERUI_TEST_CHECK(!second.tag(first_unknown.handle).has_value());
    const auto foreign_copy = second.copy_tag(first_unknown.handle);
    ERUI_TEST_CHECK(!foreign_copy.success());
    ERUI_TEST_CHECK(
        foreign_copy.error.code == erui::gfx::GfxDocumentErrorCode::stale_handle);
    const std::array foreign_edit{
        erui::gfx::GfxEdit::erase(first_unknown.handle)};
    const auto foreign_error = second.apply(foreign_edit);
    ERUI_TEST_CHECK(!foreign_error.success());
    ERUI_TEST_CHECK(
        foreign_error.code == erui::gfx::GfxDocumentErrorCode::stale_handle);
    const auto second_after = second.serialize_and_validate();
    ERUI_TEST_CHECK(second_after.success());
    ERUI_TEST_CHECK(second_after.bytes == second_before.bytes);

    erui::gfx::GfxDocument copied = first;
    ERUI_TEST_CHECK(copied.valid());
    ERUI_TEST_CHECK(!copied.tag(first_unknown.handle).has_value());
    const auto copied_unknown = find_tag(copied, kUnknownTag, true);
    ERUI_TEST_CHECK(!first.tag(copied_unknown.handle).has_value());
    ERUI_TEST_CHECK(
        copied_unknown.handle.document_identity !=
        first_unknown.handle.document_identity);

    // Moving transfers ownership, so handles issued before the move remain
    // valid in the destination and cannot be used on the moved-from object.
    erui::gfx::GfxDocument moved = std::move(first);
    ERUI_TEST_CHECK(moved.valid());
    ERUI_TEST_CHECK(!first.valid());
    ERUI_TEST_CHECK(moved.tag(first_unknown.handle).has_value());
    ERUI_TEST_CHECK(!first.tag(first_unknown.handle).has_value());
}

void test_dense_handle_indexes_survive_copy_move_and_edit() {
    constexpr std::size_t kSpriteCount = 8192;
    const std::span<const std::vector<std::uint8_t>> no_children{};
    std::vector<std::vector<std::uint8_t>> roots{};
    roots.reserve(kSpriteCount);
    for (std::size_t index = 0; index < kSpriteCount; ++index) {
        roots.push_back(define_sprite(
            static_cast<std::uint16_t>(index + 1U), no_children));
    }

    auto document = parse_document(movie(roots));
    ERUI_TEST_CHECK(document.sprites().size() == kSpriteCount);
    ERUI_TEST_CHECK(document.tags().size() == kSpriteCount * 3U + 1U);

    const auto verify_handle = [](
                                   const erui::gfx::GfxDocument& subject,
                                   std::size_t index) {
        const erui::gfx::GfxSpriteInfo& indexed = subject.sprites()[index];
        const std::uint16_t expected_id =
            static_cast<std::uint16_t>(index + 1U);
        ERUI_TEST_CHECK(indexed.character_id == expected_id);

        const auto tag_info = subject.tag(indexed.definition);
        ERUI_TEST_CHECK(tag_info.has_value());
        ERUI_TEST_CHECK(tag_info->handle == indexed.definition);
        ERUI_TEST_CHECK(tag_info->code == kTagDefineSprite);
        ERUI_TEST_CHECK(tag_info->character_id == expected_id);

        const auto sprite_info = subject.sprite(indexed.definition);
        ERUI_TEST_CHECK(sprite_info.has_value());
        ERUI_TEST_CHECK(sprite_info->definition == indexed.definition);
        ERUI_TEST_CHECK(sprite_info->character_id == expected_id);

        const auto raw = subject.copy_tag(indexed.definition);
        ERUI_TEST_CHECK(raw.success());
        ERUI_TEST_CHECK(raw.tag.code == kTagDefineSprite);
        ERUI_TEST_CHECK(raw.tag.body.size() >= 4U);
        const std::uint16_t copied_id = static_cast<std::uint16_t>(
            raw.tag.body[0] |
            (static_cast<std::uint16_t>(raw.tag.body[1]) << 8U));
        const std::uint16_t copied_frame_count = static_cast<std::uint16_t>(
            raw.tag.body[2] |
            (static_cast<std::uint16_t>(raw.tag.body[3]) << 8U));
        ERUI_TEST_CHECK(copied_id == expected_id);
        ERUI_TEST_CHECK(copied_frame_count == indexed.frame_count);
    };
    const auto verify_all_handles = [&](const erui::gfx::GfxDocument& subject) {
        ERUI_TEST_CHECK(subject.sprites().size() == kSpriteCount);
        // Reverse order prevents a future accidentally cached forward scan
        // from hiding per-handle linear lookup behind favorable locality.
        for (std::size_t remaining = kSpriteCount; remaining != 0; --remaining) {
            verify_handle(subject, remaining - 1U);
        }
    };

    verify_all_handles(document);

    const auto source_handle = document.sprites().front().definition;
    erui::gfx::GfxDocument copied = document;
    ERUI_TEST_CHECK(!copied.tag(source_handle).has_value());
    verify_all_handles(copied);

    erui::gfx::GfxDocument assigned{};
    assigned = copied;
    const auto copied_handle = copied.sprites()[kSpriteCount / 2U].definition;
    ERUI_TEST_CHECK(!assigned.tag(copied_handle).has_value());
    verify_handle(assigned, 0);
    verify_handle(assigned, kSpriteCount / 2U);
    verify_handle(assigned, kSpriteCount - 1U);

    const auto moved_handle = assigned.sprites()[kSpriteCount / 3U].definition;
    erui::gfx::GfxDocument moved = std::move(assigned);
    ERUI_TEST_CHECK(!assigned.valid());
    ERUI_TEST_CHECK(moved.tag(moved_handle).has_value());
    ERUI_TEST_CHECK(moved.copy_tag(moved_handle).success());

    const std::size_t edited_index = kSpriteCount / 2U;
    const auto stale_handle = moved.sprites()[edited_index].definition;
    const std::uint16_t edited_id = moved.sprites()[edited_index].character_id;
    const std::array edit{erui::gfx::GfxEdit::set_sprite_frame_count(
        stale_handle, 7)};
    ERUI_TEST_CHECK(moved.apply(edit).success());
    ERUI_TEST_CHECK(!moved.tag(stale_handle).has_value());
    ERUI_TEST_CHECK(!moved.sprite(stale_handle).has_value());
    const auto stale_copy = moved.copy_tag(stale_handle);
    ERUI_TEST_CHECK(!stale_copy.success());
    ERUI_TEST_CHECK(
        stale_copy.error.code == erui::gfx::GfxDocumentErrorCode::stale_handle);
    const auto edited = moved.sprite_by_character_id(edited_id);
    ERUI_TEST_CHECK(edited.has_value());
    ERUI_TEST_CHECK(edited->frame_count == 7);
    verify_all_handles(moved);
}

void test_unknown_tag_edits_and_header_promotion() {
    auto document = parse_document(representative_movie());
    auto unknown = find_tag(document, kUnknownTag, true);
    auto copied = document.copy_tag(unknown.handle);
    ERUI_TEST_CHECK(copied.success());
    ERUI_TEST_CHECK(copied.tag.code == kUnknownTag);
    ERUI_TEST_CHECK(copied.tag.header_form == erui::gfx::GfxTagHeaderForm::short_form);
    ERUI_TEST_CHECK((copied.tag.body ==
        std::vector<std::uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));

    const std::vector<std::uint8_t> long_body(80, 0x6B);
    const std::array promote{
        erui::gfx::GfxEdit::replace_body(unknown.handle, long_body)};
    ERUI_TEST_CHECK(document.apply(promote).success());
    unknown = find_tag(document, kUnknownTag, true);
    copied = document.copy_tag(unknown.handle);
    ERUI_TEST_CHECK(copied.success());
    ERUI_TEST_CHECK(copied.tag.header_form == erui::gfx::GfxTagHeaderForm::long_form);
    ERUI_TEST_CHECK(copied.tag.body == long_body);

    const std::array insert_after{
        erui::gfx::GfxEdit::insert_after(
            unknown.handle,
            {
                .code = 780,
                .body = {6, 5, 4},
                .header_form = erui::gfx::GfxTagHeaderForm::automatic,
            })};
    ERUI_TEST_CHECK(document.apply(insert_after).success());
    const auto after_unknown = find_tag(document, kUnknownTag, true);
    const auto inserted_after = find_tag(document, 780, true);
    ERUI_TEST_CHECK(inserted_after.ordinal == after_unknown.ordinal + 1);

    auto end = find_tag(document, kTagEnd, true);
    const std::array insert{
        erui::gfx::GfxEdit::insert_before(
            end.handle,
            {
                .code = 778,
                .body = {9, 8, 7},
                .header_form = erui::gfx::GfxTagHeaderForm::automatic,
            })};
    ERUI_TEST_CHECK(document.apply(insert).success());
    const auto inserted = find_tag(document, 778, true);
    const auto inserted_copy = document.copy_tag(inserted.handle);
    ERUI_TEST_CHECK(inserted_copy.success());
    ERUI_TEST_CHECK((inserted_copy.tag.body == std::vector<std::uint8_t>{9, 8, 7}));

    const std::array replace{
        erui::gfx::GfxEdit::replace(
            inserted.handle,
            {
                .code = 779,
                .body = {4, 5},
                .header_form = erui::gfx::GfxTagHeaderForm::long_form,
            })};
    ERUI_TEST_CHECK(document.apply(replace).success());
    const auto replaced = find_tag(document, 779, true);
    const auto replaced_copy = document.copy_tag(replaced.handle);
    ERUI_TEST_CHECK(replaced_copy.success());
    ERUI_TEST_CHECK(replaced_copy.tag.header_form == erui::gfx::GfxTagHeaderForm::long_form);

    const std::array erase{erui::gfx::GfxEdit::erase(replaced.handle)};
    ERUI_TEST_CHECK(document.apply(erase).success());
    ERUI_TEST_CHECK(std::none_of(
        document.tags().begin(),
        document.tags().end(),
        [](const erui::gfx::GfxTagInfo& candidate) {
            return candidate.code == 779;
        }));

    const auto serialized = document.serialize_and_validate();
    ERUI_TEST_CHECK(serialized.success());
    ERUI_TEST_CHECK(serialized.bytes.size() >= document.declared_length());
    ERUI_TEST_CHECK(std::equal(
        serialized.bytes.end() - 4,
        serialized.bytes.end(),
        std::array<std::uint8_t, 4>{0xFA, 0xCE, 0xB0, 0x0C}.begin()));

    auto reparsed = parse_document(serialized.bytes);
    const auto promoted_reparsed = find_tag(reparsed, kUnknownTag, true);
    const auto promoted_copy = reparsed.copy_tag(promoted_reparsed.handle);
    ERUI_TEST_CHECK(promoted_copy.success());
    ERUI_TEST_CHECK(promoted_copy.tag.body == long_body);
    ERUI_TEST_CHECK(
        promoted_copy.tag.header_form == erui::gfx::GfxTagHeaderForm::long_form);
}

#if defined(ERNATIVEUI_TEST_GAME_OPTIONS_GFX) || \
    defined(ERNATIVEUI_TEST_ADVANCED_GFX)
void test_real_asset_round_trip(const char* path) {
    const auto bytes = read_file(path);
    auto document = parse_document(bytes);
    ERUI_TEST_CHECK(!document.tags().empty());
    ERUI_TEST_CHECK(!document.sprites().empty());
    ERUI_TEST_CHECK(!document.character_definitions().empty());

    const auto first = document.serialize_and_validate();
    ERUI_TEST_CHECK(first.success());
    ERUI_TEST_CHECK(first.bytes == bytes);
    auto reparsed = parse_document(first.bytes);
    const auto second = reparsed.serialize_and_validate();
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(second.bytes == first.bytes);
}
#endif

} // namespace

int main() {
    test_parse_and_lossless_round_trip();
    test_uncompressed_swf_round_trip();
    test_nested_sprites_and_limits();
    test_recursive_sprite_length_repair();
    test_place_object3_and_name_limit();
    test_malformed_input_and_resource_limits();
    test_deterministic_malformed_input_sweep();
    test_duplicate_and_exhausted_character_ids();
    test_multiple_character_id_allocation();
    test_character_references_reserve_allocator_ids();
    test_scaleform_character_definitions();
    test_imported_character_ids();
    test_transactional_edits_and_handle_revisions();
    test_handles_are_document_scoped();
    test_dense_handle_indexes_survive_copy_move_and_edit();
    test_unknown_tag_edits_and_header_promotion();

#if defined(ERNATIVEUI_TEST_GAME_OPTIONS_GFX)
    test_real_asset_round_trip(ERNATIVEUI_TEST_GAME_OPTIONS_GFX);
#endif
#if defined(ERNATIVEUI_TEST_ADVANCED_GFX)
    test_real_asset_round_trip(ERNATIVEUI_TEST_ADVANCED_GFX);
#endif

    ERUI_TEST_CHECK(std::string_view{
        erui::gfx::gfx_document_error_name(
            erui::gfx::GfxDocumentErrorCode::duplicate_character_id)} ==
        "duplicate_character_id");
    ERUI_TEST_CHECK(std::string_view{
        erui::gfx::gfx_document_error_context_name(
            erui::gfx::GfxDocumentErrorContext::serialization)} ==
        "serialization");
    return 0;
}
