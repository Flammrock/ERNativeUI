#include "gfx_document.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace erui::gfx {
namespace {

constexpr std::uint16_t kTagEnd = 0;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kTagImportAssets = 57;
constexpr std::uint16_t kTagPlaceObject3 = 70;
constexpr std::uint16_t kTagImportAssets2 = 71;
constexpr std::uint16_t kTagDefineScalingGrid = 78;
constexpr std::uint16_t kMaximumTagCode = 0x03FF;
constexpr std::uint32_t kLongTagMarker = 0x3F;

std::atomic<std::uint64_t> g_next_document_identity{1};

[[nodiscard]] std::uint64_t next_document_identity() noexcept {
    for (;;) {
        const std::uint64_t identity = g_next_document_identity.fetch_add(
            1, std::memory_order_relaxed);
        if (identity != 0) return identity;
    }
}

struct TagNode {
    std::uint64_t token{};
    std::uint16_t code{};
    bool force_long_header{};
    bool parsed_sprite{};
    std::uint16_t sprite_character_id{};
    std::uint16_t sprite_frame_count{};
    std::vector<std::uint8_t> body{};
    std::vector<TagNode> children{};
    std::size_t original_body_size{};
    std::size_t source_tag_offset{};
    std::size_t source_body_offset{};
};

struct ParsedTree {
    GfxDocumentSignature signature{GfxDocumentSignature::unknown};
    std::uint8_t version{};
    std::uint32_t declared_length{};
    std::size_t physical_size{};
    std::vector<std::uint8_t> movie_header{};
    std::vector<TagNode> top_level{};
    std::vector<std::uint8_t> trailing{};
    std::uint64_t next_token{1};
};

[[nodiscard]] GfxDocumentError make_error(
    GfxDocumentErrorCode code,
    GfxDocumentErrorContext context,
    std::size_t offset = 0,
    std::uint16_t tag_code = 0,
    std::uint16_t character_id = 0,
    std::uint32_t detail = 0) noexcept {
    return {
        .code = code,
        .context = context,
        .byte_offset = offset,
        .tag_code = tag_code,
        .character_id = character_id,
        .detail = detail,
    };
}

[[nodiscard]] bool checked_add(
    std::size_t left,
    std::size_t right,
    std::size_t& result) noexcept {
    if (right > std::numeric_limits<std::size_t>::max() - left) return false;
    result = left + right;
    return true;
}

[[nodiscard]] bool read_u16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint16_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 2) return false;
    value = static_cast<std::uint16_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
    return true;
}

[[nodiscard]] bool read_u32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint32_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 4) return false;
    value = static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    return true;
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void write_u32(
    std::vector<std::uint8_t>& output,
    std::size_t offset,
    std::uint32_t value) noexcept {
    output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    output[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    output[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

class BitReader {
public:
    BitReader(std::span<const std::uint8_t> bytes, std::size_t byte_offset) noexcept
        : bytes_(bytes), bit_position_(byte_offset * 8U) {}

    [[nodiscard]] bool read(std::size_t count, std::uint32_t& value) noexcept {
        if (count > 32U || bit_position_ > bytes_.size() * 8U ||
            count > bytes_.size() * 8U - bit_position_) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const std::size_t byte_index = bit_position_ / 8U;
            const std::size_t bit_index = 7U - (bit_position_ % 8U);
            value = (value << 1U) |
                ((bytes_[byte_index] >> bit_index) & std::uint8_t{1});
            ++bit_position_;
        }
        return true;
    }

    [[nodiscard]] bool read_signed(
        std::size_t count,
        std::int32_t& value) noexcept {
        if (count > 31U) return false;
        std::uint32_t raw{};
        if (!read(count, raw)) return false;
        if (count != 0 && (raw & (std::uint32_t{1} << (count - 1U))) != 0) {
            raw |= ~((std::uint32_t{1} << count) - 1U);
        }
        value = static_cast<std::int32_t>(raw);
        return true;
    }

    void align() noexcept {
        bit_position_ = (bit_position_ + 7U) & ~std::size_t{7};
    }

    [[nodiscard]] std::size_t byte_position() const noexcept {
        return bit_position_ / 8U;
    }

private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t bit_position_{};
};

[[nodiscard]] bool movie_header_end(
    std::span<const std::uint8_t> bytes,
    std::size_t& end) noexcept {
    if (bytes.size() < 12) return false;
    BitReader reader(bytes, 8);
    std::uint32_t bit_count{};
    if (!reader.read(5, bit_count) || bit_count > 31U) return false;
    std::int32_t ignored{};
    for (std::size_t field = 0; field < 4; ++field) {
        if (!reader.read_signed(bit_count, ignored)) return false;
    }
    reader.align();
    return checked_add(reader.byte_position(), 4, end) && end <= bytes.size();
}

struct ParseState {
    std::span<const std::uint8_t> bytes{};
    const GfxDocumentLimits* limits{};
    std::size_t tag_count{};
    std::size_t sprite_count{};
    std::uint64_t next_token{1};
};

[[nodiscard]] bool parse_tag_sequence(
    ParseState& state,
    std::size_t begin,
    std::size_t end,
    std::size_t nesting_depth,
    std::vector<TagNode>& output,
    GfxDocumentError& error) {
    if (begin > end || end > state.bytes.size()) {
        error = make_error(
            GfxDocumentErrorCode::tag_body_out_of_bounds,
            GfxDocumentErrorContext::tag_body,
            begin);
        return false;
    }
    if (nesting_depth > state.limits->maximum_nesting_depth) {
        error = make_error(
            GfxDocumentErrorCode::nesting_limit_exceeded,
            GfxDocumentErrorContext::sprite,
            begin,
            kTagDefineSprite,
            0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                nesting_depth, std::numeric_limits<std::uint32_t>::max())));
        return false;
    }

    std::size_t cursor = begin;
    while (cursor < end) {
        const std::size_t tag_start = cursor;
        std::uint16_t packed{};
        if (!read_u16(state.bytes, cursor, packed)) {
            error = make_error(
                GfxDocumentErrorCode::truncated_tag_header,
                GfxDocumentErrorContext::tag_header,
                cursor);
            return false;
        }
        cursor += 2;

        const std::uint16_t code = static_cast<std::uint16_t>(packed >> 6U);
        std::uint32_t body_length = packed & kLongTagMarker;
        bool long_header = false;
        if (body_length == kLongTagMarker) {
            long_header = true;
            if (!read_u32(state.bytes, cursor, body_length)) {
                error = make_error(
                    GfxDocumentErrorCode::truncated_long_tag_length,
                    GfxDocumentErrorContext::tag_header,
                    cursor,
                    code);
                return false;
            }
            cursor += 4;
        }
        if (cursor > end || body_length > end - cursor) {
            error = make_error(
                GfxDocumentErrorCode::tag_body_out_of_bounds,
                GfxDocumentErrorContext::tag_body,
                cursor,
                code,
                0,
                body_length);
            return false;
        }
        if (body_length > state.limits->maximum_tag_body_bytes) {
            error = make_error(
                GfxDocumentErrorCode::tag_body_too_large,
                GfxDocumentErrorContext::tag_body,
                cursor,
                code,
                0,
                body_length);
            return false;
        }
        if (state.tag_count >= state.limits->maximum_tags ||
            state.next_token == 0) {
            error = make_error(
                GfxDocumentErrorCode::tag_limit_exceeded,
                GfxDocumentErrorContext::tag_header,
                tag_start,
                code);
            return false;
        }
        ++state.tag_count;

        const std::size_t body_start = cursor;
        const std::size_t body_end = cursor + body_length;
        TagNode node{};
        node.token = state.next_token++;
        node.code = code;
        node.force_long_header = long_header;
        node.original_body_size = body_length;
        node.source_tag_offset = tag_start;
        node.source_body_offset = body_start;

        if (code == kTagDefineSprite) {
            if (body_length < 4 ||
                !read_u16(state.bytes, body_start, node.sprite_character_id) ||
                !read_u16(state.bytes, body_start + 2, node.sprite_frame_count)) {
                error = make_error(
                    GfxDocumentErrorCode::malformed_define_sprite,
                    GfxDocumentErrorContext::sprite,
                    body_start,
                    code);
                return false;
            }
            if (state.sprite_count >= state.limits->maximum_sprites) {
                error = make_error(
                    GfxDocumentErrorCode::sprite_limit_exceeded,
                    GfxDocumentErrorContext::sprite,
                    tag_start,
                    code,
                    node.sprite_character_id);
                return false;
            }
            ++state.sprite_count;
            node.parsed_sprite = true;
            if (!parse_tag_sequence(
                    state,
                    body_start + 4,
                    body_end,
                    nesting_depth + 1,
                    node.children,
                    error)) {
                return false;
            }
        } else {
            node.body.assign(
                state.bytes.begin() + static_cast<std::ptrdiff_t>(body_start),
                state.bytes.begin() + static_cast<std::ptrdiff_t>(body_end));
        }

        output.push_back(std::move(node));
        cursor = body_end;

        if (code == kTagEnd) {
            if (body_length != 0) {
                error = make_error(
                    GfxDocumentErrorCode::malformed_tag,
                    GfxDocumentErrorContext::tag_body,
                    body_start,
                    code,
                    0,
                    body_length);
                return false;
            }
            if (cursor != end) {
                error = make_error(
                    GfxDocumentErrorCode::data_after_end_tag,
                    GfxDocumentErrorContext::tag_body,
                    cursor,
                    code,
                    0,
                    static_cast<std::uint32_t>(std::min<std::size_t>(
                        end - cursor, std::numeric_limits<std::uint32_t>::max())));
                return false;
            }
            return true;
        }
    }

    error = make_error(
        GfxDocumentErrorCode::missing_end_tag,
        GfxDocumentErrorContext::tag_body,
        cursor);
    return false;
}

[[nodiscard]] bool parse_tree(
    std::span<const std::uint8_t> input,
    const GfxDocumentLimits& limits,
    ParsedTree& tree,
    GfxDocumentError& error) {
    tree = {};
    if (limits.maximum_physical_bytes < 12 ||
        limits.maximum_declared_bytes < 12 ||
        limits.maximum_tags == 0 ||
        limits.maximum_nesting_depth >
            GfxDocumentLimits::hard_maximum_nesting_depth) {
        error = make_error(
            GfxDocumentErrorCode::invalid_argument,
            GfxDocumentErrorContext::file_header);
        return false;
    }
    if (input.size() > limits.maximum_physical_bytes) {
        error = make_error(
            GfxDocumentErrorCode::input_too_large,
            GfxDocumentErrorContext::file_header,
            0,
            0,
            0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                input.size(), std::numeric_limits<std::uint32_t>::max())));
        return false;
    }
    const bool gfx_signature = input.size() >= 3 &&
        input[0] == 'G' && input[1] == 'F' && input[2] == 'X';
    const bool swf_signature = input.size() >= 3 &&
        input[0] == 'F' && input[1] == 'W' && input[2] == 'S';
    if (input.size() < 12 || (!gfx_signature && !swf_signature)) {
        error = make_error(
            GfxDocumentErrorCode::unsupported_signature,
            GfxDocumentErrorContext::file_header);
        return false;
    }

    std::uint32_t declared{};
    if (!read_u32(input, 4, declared) || declared < 12 ||
        declared > input.size() || declared > limits.maximum_declared_bytes) {
        error = make_error(
            GfxDocumentErrorCode::invalid_declared_length,
            GfxDocumentErrorContext::file_header,
            4,
            0,
            0,
            declared);
        return false;
    }

    std::size_t tags_begin{};
    if (!movie_header_end(input.first(declared), tags_begin) ||
        tags_begin > declared) {
        error = make_error(
            GfxDocumentErrorCode::invalid_movie_header,
            GfxDocumentErrorContext::movie_header,
            8);
        return false;
    }

    tree.signature = gfx_signature
        ? GfxDocumentSignature::gfx
        : GfxDocumentSignature::swf;
    tree.version = input[3];
    tree.declared_length = declared;
    tree.physical_size = input.size();
    tree.movie_header.assign(
        input.begin(), input.begin() + static_cast<std::ptrdiff_t>(tags_begin));
    tree.trailing.assign(
        input.begin() + static_cast<std::ptrdiff_t>(declared), input.end());

    ParseState state{
        .bytes = input.first(declared),
        .limits = &limits,
    };
    if (!parse_tag_sequence(
            state, tags_begin, declared, 0, tree.top_level, error)) {
        return false;
    }
    tree.next_token = state.next_token;
    return true;
}

[[nodiscard]] bool is_definition_code(std::uint16_t code) noexcept {
    switch (code) {
    case 2:   // DefineShape
    case 6:   // DefineBits
    case 7:   // DefineButton
    case 10:  // DefineFont
    case 11:  // DefineText
    case 14:  // DefineSound
    case 20:  // DefineBitsLossless
    case 21:  // DefineBitsJPEG2
    case 22:  // DefineShape2
    case 32:  // DefineShape3
    case 33:  // DefineText2
    case 34:  // DefineButton2
    case 35:  // DefineBitsJPEG3
    case 36:  // DefineBitsLossless2
    case 37:  // DefineEditText
    case 39:  // DefineSprite
    case 46:  // DefineMorphShape
    case 48:  // DefineFont2
    case 60:  // DefineVideoStream
    case 75:  // DefineFont3
    case 83:  // DefineShape4
    case 84:  // DefineMorphShape2
    case 87:  // DefineBinaryData
    case 90:  // DefineBitsJPEG4
    case 91:  // DefineFont4
    case 1001: // Scaleform DefineExternalImage
    case 1005: // Scaleform DefineCompactedFont
    case 1006: // Scaleform DefineExternalSound
    case 1008: // Scaleform DefineSubImage
    case 1009: // Scaleform DefineExternalImage2
        return true;
    default:
        return false;
    }
}

enum class CharacterDefinitionState : std::uint8_t {
    not_definition,
    definition,
    malformed,
};

[[nodiscard]] CharacterDefinitionState character_id_of(
    const TagNode& node,
    std::uint16_t& character_id) noexcept {
    if (!is_definition_code(node.code)) {
        return CharacterDefinitionState::not_definition;
    }
    if (node.parsed_sprite) {
        character_id = node.sprite_character_id;
        return CharacterDefinitionState::definition;
    }
    if (!read_u16(node.body, 0, character_id)) {
        return CharacterDefinitionState::malformed;
    }
    if (node.code == 1009) {
        std::uint16_t id_type{};
        if (!read_u16(node.body, 2, id_type)) {
            return CharacterDefinitionState::malformed;
        }
        if (id_type != 0) return CharacterDefinitionState::not_definition;
    }
    return CharacterDefinitionState::definition;
}

[[nodiscard]] bool scaling_grid_target_of(
    const TagNode& node,
    std::uint16_t& character_id) noexcept {
    if (node.code != kTagDefineScalingGrid ||
        !read_u16(node.body, 0, character_id)) {
        return false;
    }

    // DefineScalingGrid contains a UI16 character reference followed by one
    // RECT. Only reserve the reference when the complete standard body is
    // structurally valid; malformed opaque tags remain byte-preserved without
    // influencing character allocation.
    BitReader rectangle(node.body, 2);
    std::uint32_t bit_count{};
    if (!rectangle.read(5, bit_count) || bit_count > 31U) return false;
    std::int32_t ignored{};
    for (std::size_t field = 0; field < 4; ++field) {
        if (!rectangle.read_signed(bit_count, ignored)) return false;
    }
    rectangle.align();
    return rectangle.byte_position() == node.body.size();
}

[[nodiscard]] bool skip_matrix(
    std::span<const std::uint8_t> body,
    std::size_t& cursor,
    GfxMatrixTranslation& translation) noexcept {
    BitReader reader(body, cursor);
    std::uint32_t present{};
    std::uint32_t bit_count{};
    std::int32_t ignored{};
    if (!reader.read(1, present)) return false;
    if (present != 0 &&
        (!reader.read(5, bit_count) || bit_count > 31 ||
            !reader.read_signed(bit_count, ignored) ||
            !reader.read_signed(bit_count, ignored))) {
        return false;
    }
    if (!reader.read(1, present)) return false;
    if (present != 0 &&
        (!reader.read(5, bit_count) || bit_count > 31 ||
            !reader.read_signed(bit_count, ignored) ||
            !reader.read_signed(bit_count, ignored))) {
        return false;
    }
    if (!reader.read(5, bit_count) || bit_count > 31 ||
        !reader.read_signed(bit_count, translation.x) ||
        !reader.read_signed(bit_count, translation.y)) {
        return false;
    }
    reader.align();
    cursor = reader.byte_position();
    return cursor <= body.size();
}

[[nodiscard]] bool skip_color_transform(
    std::span<const std::uint8_t> body,
    std::size_t& cursor) noexcept {
    BitReader reader(body, cursor);
    std::uint32_t has_add{};
    std::uint32_t has_multiply{};
    std::uint32_t bit_count{};
    if (!reader.read(1, has_add) || !reader.read(1, has_multiply) ||
        !reader.read(4, bit_count)) {
        return false;
    }
    std::int32_t ignored{};
    const std::size_t components =
        (has_multiply != 0 ? 4U : 0U) + (has_add != 0 ? 4U : 0U);
    for (std::size_t component = 0; component < components; ++component) {
        if (!reader.read_signed(bit_count, ignored)) return false;
    }
    reader.align();
    cursor = reader.byte_position();
    return cursor <= body.size();
}

[[nodiscard]] bool read_c_string(
    std::span<const std::uint8_t> body,
    std::size_t& cursor,
    std::size_t maximum,
    std::string* value) {
    if (cursor >= body.size()) return false;
    const std::size_t begin = cursor;
    while (cursor < body.size() && body[cursor] != 0) {
        if (cursor - begin >= maximum) return false;
        ++cursor;
    }
    if (cursor >= body.size()) return false;
    if (value != nullptr) {
        value->assign(
            reinterpret_cast<const char*>(body.data() + begin), cursor - begin);
    }
    ++cursor;
    return true;
}

[[nodiscard]] bool parse_import_character_ids(
    const TagNode& node,
    std::size_t maximum_name,
    std::vector<std::uint16_t>& ids) {
    ids.clear();
    if (node.code != kTagImportAssets && node.code != kTagImportAssets2) {
        return false;
    }
    const std::span<const std::uint8_t> body = node.body;
    std::size_t cursor{};
    if (!read_c_string(body, cursor, maximum_name, nullptr)) return false;
    if (node.code == kTagImportAssets2) {
        // SWF defines two reserved bytes between URL and Count. Preserve and
        // accept their values, but never reinterpret them as variable-length
        // payload metadata when locating the following dictionary table.
        if (cursor > body.size() || body.size() - cursor < 2) return false;
        cursor += 2;
    }
    std::uint16_t count{};
    if (!read_u16(body, cursor, count)) return false;
    cursor += 2;
    if (cursor > body.size() ||
        static_cast<std::size_t>(count) > (body.size() - cursor) / 3U) {
        return false;
    }
    ids.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        std::uint16_t character_id{};
        if (!read_u16(body, cursor, character_id)) return false;
        cursor += 2;
        if (!read_c_string(body, cursor, maximum_name, nullptr)) return false;
        ids.push_back(character_id);
    }
    return cursor == body.size();
}

[[nodiscard]] bool parse_place_object2(
    const TagNode& node,
    GfxTagHandle owner,
    std::uint64_t revision,
    std::uint64_t document_identity,
    std::size_t maximum_name,
    GfxPlacementInfo& placement) {
    const std::span<const std::uint8_t> body = node.body;
    if (body.size() < 3) return false;
    std::size_t cursor{};
    const std::uint8_t flags = body[cursor++];
    placement = {};
    placement.tag = {node.token, revision, document_identity};
    placement.owner_sprite = owner;
    placement.tag_code = node.code;
    placement.flags = flags;
    placement.move = (flags & 0x01U) != 0;
    if (!read_u16(body, cursor, placement.depth)) return false;
    cursor += 2;
    if ((flags & 0x02U) != 0) {
        placement.has_character = true;
        if (!read_u16(body, cursor, placement.character_id)) return false;
        cursor += 2;
    }
    if ((flags & 0x04U) != 0) {
        placement.has_matrix = true;
        if (!skip_matrix(body, cursor, placement.translation)) return false;
    }
    if ((flags & 0x08U) != 0 && !skip_color_transform(body, cursor)) return false;
    if ((flags & 0x10U) != 0) {
        if (cursor > body.size() || body.size() - cursor < 2) return false;
        cursor += 2;
    }
    if ((flags & 0x20U) != 0) {
        placement.has_name = true;
        if (!read_c_string(body, cursor, maximum_name, &placement.name)) {
            return false;
        }
    }
    if ((flags & 0x40U) != 0) {
        if (cursor > body.size() || body.size() - cursor < 2) return false;
        cursor += 2;
    }
    return cursor <= body.size();
}

[[nodiscard]] bool parse_place_object3(
    const TagNode& node,
    GfxTagHandle owner,
    std::uint64_t revision,
    std::uint64_t document_identity,
    std::size_t maximum_name,
    GfxPlacementInfo& placement) {
    const std::span<const std::uint8_t> body = node.body;
    if (body.size() < 4) return false;
    std::size_t cursor{};
    const std::uint8_t flags = body[cursor++];
    const std::uint8_t flags2 = body[cursor++];
    placement = {};
    placement.tag = {node.token, revision, document_identity};
    placement.owner_sprite = owner;
    placement.tag_code = node.code;
    placement.flags = flags;
    placement.flags2 = flags2;
    placement.move = (flags & 0x01U) != 0;
    placement.is_place_object3 = true;
    if (!read_u16(body, cursor, placement.depth)) return false;
    cursor += 2;

    const bool has_character = (flags & 0x02U) != 0;
    const bool has_class_name = (flags2 & 0x08U) != 0;
    const bool has_image = (flags2 & 0x10U) != 0;
    if ((has_class_name || (has_image && has_character)) &&
        !read_c_string(body, cursor, maximum_name, nullptr)) {
        return false;
    }
    if (has_character) {
        placement.has_character = true;
        if (!read_u16(body, cursor, placement.character_id)) return false;
        cursor += 2;
    }
    if ((flags & 0x04U) != 0) {
        placement.has_matrix = true;
        if (!skip_matrix(body, cursor, placement.translation)) return false;
    }
    if ((flags & 0x08U) != 0 && !skip_color_transform(body, cursor)) return false;
    if ((flags & 0x10U) != 0) {
        if (cursor > body.size() || body.size() - cursor < 2) return false;
        cursor += 2;
    }
    if ((flags & 0x20U) != 0) {
        placement.has_name = true;
        if (!read_c_string(body, cursor, maximum_name, &placement.name)) {
            return false;
        }
    }
    if ((flags & 0x40U) != 0) {
        if (cursor > body.size() || body.size() - cursor < 2) return false;
        cursor += 2;
    }

    // FilterList, BlendMode, and BitmapCache precede Visibility. FilterList
    // is variable-length and intentionally opaque to this structural index;
    // the other two fields are fixed UI8 values and can be skipped safely.
    // The complete tag body is retained regardless.
    constexpr std::uint8_t kHasFilterList = 0x01U;
    constexpr std::uint8_t kHasBlendMode = 0x02U;
    constexpr std::uint8_t kHasCacheAsBitmap = 0x04U;
    constexpr std::uint8_t kHasVisible = 0x20U;
    if ((flags2 & kHasFilterList) != 0) return true;
    if ((flags2 & kHasBlendMode) != 0) {
        if (cursor >= body.size()) return false;
        ++cursor;
    }
    if ((flags2 & kHasCacheAsBitmap) != 0) {
        if (cursor >= body.size()) return false;
        ++cursor;
    }
    if ((flags2 & kHasVisible) != 0) {
        if (cursor >= body.size()) return false;
        placement.has_visible = true;
        placement.visible = body[cursor++] != 0;
    }
    return cursor <= body.size();
}

[[nodiscard]] bool node_body_size(
    const TagNode& node,
    const GfxDocumentLimits& limits,
    std::size_t& size,
    GfxDocumentError& error);

[[nodiscard]] bool append_node(
    const TagNode& node,
    const GfxDocumentLimits& limits,
    std::vector<std::uint8_t>& output,
    GfxDocumentError& error);

struct MutableNodeLocation {
    std::vector<TagNode>* sequence{};
    std::size_t index{};

    [[nodiscard]] TagNode* node() const noexcept {
        return sequence == nullptr || index >= sequence->size()
            ? nullptr
            : &(*sequence)[index];
    }
};

[[nodiscard]] bool find_node_mutable(
    std::vector<TagNode>& sequence,
    std::uint64_t token,
    MutableNodeLocation& location) noexcept;

[[nodiscard]] bool serialize_tree(
    const ParsedTree& tree,
    const GfxDocumentLimits& limits,
    std::vector<std::uint8_t>& output,
    GfxDocumentError& error);

[[nodiscard]] TagNode raw_node_from_edit(
    const GfxRawTag& tag,
    std::uint64_t token);

[[nodiscard]] GfxDocumentError validate_raw_tag(
    const GfxRawTag& tag,
    const GfxDocumentLimits& limits) noexcept;

} // namespace

struct GfxDocument::Storage {
    static constexpr std::size_t missing_index =
        std::numeric_limits<std::size_t>::max();

    Storage() = default;
    Storage(const Storage& other)
        : tree(other.tree),
          limits(other.limits),
          document_identity(other.document_identity),
          revision(other.revision),
          tags(other.tags),
          sprites(other.sprites),
          placements(other.placements),
          definitions(other.definitions),
          occupied_character_ids(other.occupied_character_ids),
          tag_index_by_token(other.tag_index_by_token),
          sprite_index_by_token(other.sprite_index_by_token),
          node_by_token(other.node_by_token.size(), nullptr) {
        rebind_node_index();
    }
    Storage& operator=(const Storage&) = delete;

    ParsedTree tree{};
    GfxDocumentLimits limits{};
    std::uint64_t document_identity{};
    std::uint64_t revision{1};
    std::vector<GfxTagInfo> tags{};
    std::vector<GfxSpriteInfo> sprites{};
    std::vector<GfxPlacementInfo> placements{};
    std::vector<GfxCharacterDefinitionInfo> definitions{};
    std::vector<std::uint16_t> occupied_character_ids{};
    // Parser-issued tokens are dense and start at one. Dense lookup tables
    // keep public handle resolution independent of the number of unrelated
    // tags in the movie. The node table contains non-owning pointers into
    // tree and therefore must be rebound whenever Storage is copied.
    std::vector<std::size_t> tag_index_by_token{};
    std::vector<std::size_t> sprite_index_by_token{};
    std::vector<const TagNode*> node_by_token{};

    [[nodiscard]] GfxDocumentError rebuild_index();

    void rebind_node_index() noexcept {
        std::fill(node_by_token.begin(), node_by_token.end(), nullptr);
        const auto visit = [&]<typename Self>(
                               Self&& self,
                               const std::vector<TagNode>& sequence) noexcept
            -> void {
            for (const TagNode& node : sequence) {
                if (node.token < node_by_token.size()) {
                    node_by_token[static_cast<std::size_t>(node.token)] = &node;
                }
                if (node.parsed_sprite) self(self, node.children);
            }
        };
        visit(visit, tree.top_level);
    }

    void rebind_identity(std::uint64_t identity) noexcept {
        document_identity = identity;
        for (GfxTagInfo& info : tags) {
            info.handle.document_identity = identity;
            if (info.parent_sprite) {
                info.parent_sprite.document_identity = identity;
            }
        }
        for (GfxSpriteInfo& info : sprites) {
            info.definition.document_identity = identity;
            if (info.parent_sprite) {
                info.parent_sprite.document_identity = identity;
            }
        }
        for (GfxPlacementInfo& info : placements) {
            info.tag.document_identity = identity;
            if (info.owner_sprite) {
                info.owner_sprite.document_identity = identity;
            }
        }
        for (GfxCharacterDefinitionInfo& info : definitions) {
            info.tag.document_identity = identity;
        }
    }
};

GfxDocumentError GfxDocument::apply(std::span<const GfxEdit> edits) {
    if (storage_ == nullptr) {
        return make_error(
            GfxDocumentErrorCode::invalid_argument,
            GfxDocumentErrorContext::edit);
    }
    if (edits.empty()) return {};
    if (storage_->revision == std::numeric_limits<std::uint64_t>::max()) {
        return make_error(
            GfxDocumentErrorCode::invalid_edit,
            GfxDocumentErrorContext::edit);
    }

    try {
        auto candidate = std::make_unique<Storage>(*storage_);
        for (const GfxEdit& edit : edits) {
            if (edit.target.token == 0 ||
                edit.target.revision != storage_->revision ||
                edit.target.document_identity != storage_->document_identity) {
                return make_error(
                    GfxDocumentErrorCode::stale_handle,
                    GfxDocumentErrorContext::edit);
            }

            MutableNodeLocation location{};
            if (!find_node_mutable(
                    candidate->tree.top_level,
                    edit.target.token,
                    location)) {
                return make_error(
                    GfxDocumentErrorCode::tag_not_found,
                    GfxDocumentErrorContext::edit);
            }
            TagNode* target = location.node();
            if (target == nullptr) {
                return make_error(
                    GfxDocumentErrorCode::tag_not_found,
                    GfxDocumentErrorContext::edit);
            }

            switch (edit.kind) {
            case GfxEditKind::replace: {
                if (const GfxDocumentError error =
                        validate_raw_tag(edit.tag, candidate->limits);
                    !error.success()) {
                    return error;
                }
                TagNode replacement = raw_node_from_edit(
                    edit.tag, target->token);
                replacement.source_tag_offset = target->source_tag_offset;
                replacement.source_body_offset = target->source_body_offset;
                *target = std::move(replacement);
                break;
            }
            case GfxEditKind::replace_body: {
                GfxRawTag replacement{};
                replacement.code = target->code;
                replacement.body = edit.tag.body;
                replacement.header_form = target->force_long_header
                    ? GfxTagHeaderForm::long_form
                    : GfxTagHeaderForm::automatic;
                if (const GfxDocumentError error =
                        validate_raw_tag(replacement, candidate->limits);
                    !error.success()) {
                    return error;
                }
                TagNode node = raw_node_from_edit(replacement, target->token);
                node.source_tag_offset = target->source_tag_offset;
                node.source_body_offset = target->source_body_offset;
                *target = std::move(node);
                break;
            }
            case GfxEditKind::insert_before:
            case GfxEditKind::insert_after: {
                if (const GfxDocumentError error =
                        validate_raw_tag(edit.tag, candidate->limits);
                    !error.success()) {
                    return error;
                }
                if (candidate->tree.next_token == 0) {
                    return make_error(
                        GfxDocumentErrorCode::tag_limit_exceeded,
                        GfxDocumentErrorContext::edit);
                }
                TagNode inserted = raw_node_from_edit(
                    edit.tag, candidate->tree.next_token++);
                const std::size_t insertion = location.index +
                    (edit.kind == GfxEditKind::insert_after ? 1U : 0U);
                location.sequence->insert(
                    location.sequence->begin() +
                        static_cast<std::ptrdiff_t>(insertion),
                    std::move(inserted));
                break;
            }
            case GfxEditKind::erase:
                location.sequence->erase(
                    location.sequence->begin() +
                    static_cast<std::ptrdiff_t>(location.index));
                break;
            case GfxEditKind::set_sprite_frame_count:
                if (!target->parsed_sprite || target->code != kTagDefineSprite) {
                    return make_error(
                        GfxDocumentErrorCode::invalid_edit,
                        GfxDocumentErrorContext::edit,
                        0,
                        target->code);
                }
                target->sprite_frame_count = edit.frame_count;
                break;
            default:
                return make_error(
                    GfxDocumentErrorCode::invalid_edit,
                    GfxDocumentErrorContext::edit);
            }
        }

        std::vector<std::uint8_t> bytes{};
        GfxDocumentError error{};
        if (!serialize_tree(candidate->tree, candidate->limits, bytes, error)) {
            return error;
        }

        auto validated = std::make_unique<Storage>();
        validated->limits = candidate->limits;
        validated->document_identity = storage_->document_identity;
        validated->revision = storage_->revision + 1;
        if (!parse_tree(bytes, validated->limits, validated->tree, error)) {
            if (error.success()) {
                error = make_error(
                    GfxDocumentErrorCode::output_validation_failed,
                    GfxDocumentErrorContext::validation);
            } else {
                error.context = GfxDocumentErrorContext::validation;
            }
            return error;
        }
        error = validated->rebuild_index();
        if (!error.success()) {
            error.context = GfxDocumentErrorContext::validation;
            return error;
        }

        delete storage_;
        storage_ = validated.release();
        return {};
    } catch (const std::bad_alloc&) {
        return make_error(
            GfxDocumentErrorCode::allocation_failed,
            GfxDocumentErrorContext::edit);
    } catch (const std::length_error&) {
        return make_error(
            GfxDocumentErrorCode::output_too_large,
            GfxDocumentErrorContext::edit);
    }
}

GfxDocumentBytesResult GfxDocument::serialize_and_validate() const {
    GfxDocumentBytesResult result{};
    if (storage_ == nullptr) {
        result.error = make_error(
            GfxDocumentErrorCode::invalid_argument,
            GfxDocumentErrorContext::serialization);
        return result;
    }

    try {
        if (!serialize_tree(
                storage_->tree,
                storage_->limits,
                result.bytes,
                result.error)) {
            result.bytes.clear();
            return result;
        }
        ParsedTree reparsed{};
        if (!parse_tree(
                result.bytes, storage_->limits, reparsed, result.error)) {
            result.bytes.clear();
            if (result.error.success()) {
                result.error = make_error(
                    GfxDocumentErrorCode::output_validation_failed,
                    GfxDocumentErrorContext::validation);
            } else {
                result.error.context = GfxDocumentErrorContext::validation;
            }
            return result;
        }
        Storage validation{};
        validation.tree = std::move(reparsed);
        validation.limits = storage_->limits;
        validation.document_identity = storage_->document_identity;
        validation.revision = storage_->revision;
        result.error = validation.rebuild_index();
        if (!result.error.success()) {
            result.bytes.clear();
            result.error.context = GfxDocumentErrorContext::validation;
        }
        return result;
    } catch (const std::bad_alloc&) {
        result.bytes.clear();
        result.error = make_error(
            GfxDocumentErrorCode::allocation_failed,
            GfxDocumentErrorContext::serialization);
        return result;
    } catch (const std::length_error&) {
        result.bytes.clear();
        result.error = make_error(
            GfxDocumentErrorCode::output_too_large,
            GfxDocumentErrorContext::serialization);
        return result;
    }
}

bool GfxDocument::can_define_character(
    std::uint16_t tag_code) noexcept {
    return is_definition_code(tag_code);
}

const char* gfx_document_error_name(GfxDocumentErrorCode code) noexcept {
    switch (code) {
    case GfxDocumentErrorCode::none: return "none";
    case GfxDocumentErrorCode::invalid_argument: return "invalid_argument";
    case GfxDocumentErrorCode::allocation_failed: return "allocation_failed";
    case GfxDocumentErrorCode::input_too_large: return "input_too_large";
    case GfxDocumentErrorCode::unsupported_signature:
        return "unsupported_signature";
    case GfxDocumentErrorCode::invalid_declared_length:
        return "invalid_declared_length";
    case GfxDocumentErrorCode::invalid_movie_header:
        return "invalid_movie_header";
    case GfxDocumentErrorCode::truncated_tag_header:
        return "truncated_tag_header";
    case GfxDocumentErrorCode::truncated_long_tag_length:
        return "truncated_long_tag_length";
    case GfxDocumentErrorCode::tag_body_out_of_bounds:
        return "tag_body_out_of_bounds";
    case GfxDocumentErrorCode::tag_body_too_large:
        return "tag_body_too_large";
    case GfxDocumentErrorCode::missing_end_tag: return "missing_end_tag";
    case GfxDocumentErrorCode::data_after_end_tag:
        return "data_after_end_tag";
    case GfxDocumentErrorCode::tag_limit_exceeded:
        return "tag_limit_exceeded";
    case GfxDocumentErrorCode::sprite_limit_exceeded:
        return "sprite_limit_exceeded";
    case GfxDocumentErrorCode::nesting_limit_exceeded:
        return "nesting_limit_exceeded";
    case GfxDocumentErrorCode::malformed_tag: return "malformed_tag";
    case GfxDocumentErrorCode::malformed_define_sprite:
        return "malformed_define_sprite";
    case GfxDocumentErrorCode::malformed_placement:
        return "malformed_placement";
    case GfxDocumentErrorCode::duplicate_character_id:
        return "duplicate_character_id";
    case GfxDocumentErrorCode::invalid_character_id:
        return "invalid_character_id";
    case GfxDocumentErrorCode::character_id_exhausted:
        return "character_id_exhausted";
    case GfxDocumentErrorCode::stale_handle: return "stale_handle";
    case GfxDocumentErrorCode::tag_not_found: return "tag_not_found";
    case GfxDocumentErrorCode::invalid_edit: return "invalid_edit";
    case GfxDocumentErrorCode::output_too_large: return "output_too_large";
    case GfxDocumentErrorCode::output_validation_failed:
        return "output_validation_failed";
    }
    return "unknown";
}

const char* gfx_document_error_context_name(
    GfxDocumentErrorContext context) noexcept {
    switch (context) {
    case GfxDocumentErrorContext::none: return "none";
    case GfxDocumentErrorContext::file_header: return "file_header";
    case GfxDocumentErrorContext::movie_header: return "movie_header";
    case GfxDocumentErrorContext::tag_header: return "tag_header";
    case GfxDocumentErrorContext::tag_body: return "tag_body";
    case GfxDocumentErrorContext::sprite: return "sprite";
    case GfxDocumentErrorContext::placement: return "placement";
    case GfxDocumentErrorContext::character_index: return "character_index";
    case GfxDocumentErrorContext::edit: return "edit";
    case GfxDocumentErrorContext::serialization: return "serialization";
    case GfxDocumentErrorContext::validation: return "validation";
    }
    return "unknown";
}

GfxEdit GfxEdit::replace(GfxTagHandle target, GfxRawTag tag) {
    GfxEdit edit{};
    edit.kind = GfxEditKind::replace;
    edit.target = target;
    edit.tag = std::move(tag);
    return edit;
}

GfxEdit GfxEdit::replace_body(
    GfxTagHandle target,
    std::vector<std::uint8_t> body) {
    GfxEdit edit{};
    edit.kind = GfxEditKind::replace_body;
    edit.target = target;
    edit.tag.body = std::move(body);
    return edit;
}

GfxEdit GfxEdit::insert_before(GfxTagHandle target, GfxRawTag tag) {
    GfxEdit edit{};
    edit.kind = GfxEditKind::insert_before;
    edit.target = target;
    edit.tag = std::move(tag);
    return edit;
}

GfxEdit GfxEdit::insert_after(GfxTagHandle target, GfxRawTag tag) {
    GfxEdit edit{};
    edit.kind = GfxEditKind::insert_after;
    edit.target = target;
    edit.tag = std::move(tag);
    return edit;
}

GfxEdit GfxEdit::erase(GfxTagHandle target) noexcept {
    GfxEdit edit{};
    edit.kind = GfxEditKind::erase;
    edit.target = target;
    return edit;
}

GfxEdit GfxEdit::set_sprite_frame_count(
    GfxTagHandle sprite,
    std::uint16_t frame_count) noexcept {
    GfxEdit edit{};
    edit.kind = GfxEditKind::set_sprite_frame_count;
    edit.target = sprite;
    edit.frame_count = frame_count;
    return edit;
}

GfxDocument::GfxDocument() = default;

GfxDocument::GfxDocument(GfxDocument&& other) noexcept
    : storage_(std::exchange(other.storage_, nullptr)) {}

GfxDocument& GfxDocument::operator=(GfxDocument&& other) noexcept {
    if (this == &other) return *this;
    delete storage_;
    storage_ = std::exchange(other.storage_, nullptr);
    return *this;
}

GfxDocument::GfxDocument(const GfxDocument& other)
    : storage_(other.storage_ != nullptr ? new Storage(*other.storage_) : nullptr) {
    if (storage_ != nullptr) {
        storage_->rebind_identity(next_document_identity());
    }
}

GfxDocument& GfxDocument::operator=(const GfxDocument& other) {
    if (this == &other) return *this;
    std::unique_ptr<Storage> replacement = other.storage_ != nullptr
        ? std::make_unique<Storage>(*other.storage_)
        : nullptr;
    if (replacement != nullptr) {
        replacement->rebind_identity(next_document_identity());
    }
    delete storage_;
    storage_ = replacement.release();
    return *this;
}

GfxDocument::~GfxDocument() {
    delete storage_;
}

GfxDocumentParseResult GfxDocument::parse(
    std::span<const std::uint8_t> input,
    GfxDocumentLimits limits) {
    GfxDocumentParseResult result{};
    try {
        auto storage = std::make_unique<Storage>();
        storage->limits = limits;
        storage->document_identity = next_document_identity();
        if (!parse_tree(input, limits, storage->tree, result.error)) {
            return result;
        }
        result.error = storage->rebuild_index();
        if (!result.error.success()) return result;
        result.document.storage_ = storage.release();
        return result;
    } catch (const std::bad_alloc&) {
        result.error = make_error(
            GfxDocumentErrorCode::allocation_failed,
            GfxDocumentErrorContext::file_header);
        return result;
    } catch (const std::length_error&) {
        result.error = make_error(
            GfxDocumentErrorCode::input_too_large,
            GfxDocumentErrorContext::file_header);
        return result;
    }
}

bool GfxDocument::valid() const noexcept {
    return storage_ != nullptr;
}

GfxDocumentSignature GfxDocument::signature() const noexcept {
    return storage_ != nullptr
        ? storage_->tree.signature
        : GfxDocumentSignature::unknown;
}

std::uint8_t GfxDocument::version() const noexcept {
    return storage_ != nullptr ? storage_->tree.version : 0;
}

std::uint32_t GfxDocument::declared_length() const noexcept {
    return storage_ != nullptr ? storage_->tree.declared_length : 0;
}

std::size_t GfxDocument::physical_size() const noexcept {
    return storage_ != nullptr ? storage_->tree.physical_size : 0;
}

std::size_t GfxDocument::trailing_size() const noexcept {
    return storage_ != nullptr ? storage_->tree.trailing.size() : 0;
}

std::uint64_t GfxDocument::revision() const noexcept {
    return storage_ != nullptr ? storage_->revision : 0;
}

const GfxDocumentLimits& GfxDocument::limits() const noexcept {
    static constexpr GfxDocumentLimits empty{};
    return storage_ != nullptr ? storage_->limits : empty;
}

std::span<const GfxTagInfo> GfxDocument::tags() const noexcept {
    return storage_ != nullptr
        ? std::span<const GfxTagInfo>(storage_->tags)
        : std::span<const GfxTagInfo>{};
}

std::span<const GfxSpriteInfo> GfxDocument::sprites() const noexcept {
    return storage_ != nullptr
        ? std::span<const GfxSpriteInfo>(storage_->sprites)
        : std::span<const GfxSpriteInfo>{};
}

std::span<const GfxPlacementInfo> GfxDocument::placements() const noexcept {
    return storage_ != nullptr
        ? std::span<const GfxPlacementInfo>(storage_->placements)
        : std::span<const GfxPlacementInfo>{};
}

std::span<const GfxCharacterDefinitionInfo>
GfxDocument::character_definitions() const noexcept {
    return storage_ != nullptr
        ? std::span<const GfxCharacterDefinitionInfo>(storage_->definitions)
        : std::span<const GfxCharacterDefinitionInfo>{};
}

std::optional<GfxTagInfo> GfxDocument::tag(
    GfxTagHandle handle) const noexcept {
    if (storage_ == nullptr || handle.revision != storage_->revision ||
        handle.document_identity != storage_->document_identity ||
        handle.token == 0) {
        return std::nullopt;
    }
    if (handle.token >= storage_->tag_index_by_token.size()) {
        return std::nullopt;
    }
    const std::size_t index = storage_->tag_index_by_token[
        static_cast<std::size_t>(handle.token)];
    if (index == Storage::missing_index || index >= storage_->tags.size() ||
        storage_->tags[index].handle != handle) {
        return std::nullopt;
    }
    return storage_->tags[index];
}

std::optional<GfxSpriteInfo> GfxDocument::sprite(
    GfxTagHandle definition) const noexcept {
    if (storage_ == nullptr || definition.revision != storage_->revision ||
        definition.document_identity != storage_->document_identity ||
        definition.token == 0) {
        return std::nullopt;
    }
    if (definition.token >= storage_->sprite_index_by_token.size()) {
        return std::nullopt;
    }
    const std::size_t index = storage_->sprite_index_by_token[
        static_cast<std::size_t>(definition.token)];
    if (index == Storage::missing_index || index >= storage_->sprites.size() ||
        storage_->sprites[index].definition != definition) {
        return std::nullopt;
    }
    return storage_->sprites[index];
}

std::optional<GfxSpriteInfo> GfxDocument::sprite_by_character_id(
    std::uint16_t character_id) const noexcept {
    if (storage_ == nullptr) return std::nullopt;
    const auto found = std::find_if(
        storage_->sprites.begin(),
        storage_->sprites.end(),
        [character_id](const GfxSpriteInfo& info) {
            return info.character_id == character_id;
        });
    return found != storage_->sprites.end()
        ? std::optional<GfxSpriteInfo>(*found)
        : std::nullopt;
}

std::optional<GfxCharacterDefinitionInfo>
GfxDocument::character_definition(std::uint16_t character_id) const noexcept {
    if (storage_ == nullptr) return std::nullopt;
    const auto found = std::lower_bound(
        storage_->definitions.begin(),
        storage_->definitions.end(),
        character_id,
        [](const GfxCharacterDefinitionInfo& info, std::uint16_t id) {
            return info.character_id < id;
        });
    return found != storage_->definitions.end() &&
            found->character_id == character_id
        ? std::optional<GfxCharacterDefinitionInfo>(*found)
        : std::nullopt;
}

GfxRawTagResult GfxDocument::copy_tag(GfxTagHandle handle) const {
    GfxRawTagResult result{};
    if (storage_ == nullptr) {
        result.error = make_error(
            GfxDocumentErrorCode::invalid_argument,
            GfxDocumentErrorContext::edit);
        return result;
    }
    if (handle.revision != storage_->revision ||
        handle.document_identity != storage_->document_identity ||
        handle.token == 0) {
        result.error = make_error(
            GfxDocumentErrorCode::stale_handle,
            GfxDocumentErrorContext::edit);
        return result;
    }
    const TagNode* node = handle.token < storage_->node_by_token.size()
        ? storage_->node_by_token[static_cast<std::size_t>(handle.token)]
        : nullptr;
    if (node == nullptr) {
        result.error = make_error(
            GfxDocumentErrorCode::tag_not_found,
            GfxDocumentErrorContext::edit);
        return result;
    }

    try {
        result.tag.code = node->code;
        result.tag.header_form = node->force_long_header
            ? GfxTagHeaderForm::long_form
            : GfxTagHeaderForm::short_form;
        if (!node->parsed_sprite) {
            result.tag.body = node->body;
            return result;
        }

        std::size_t body_size{};
        if (!node_body_size(
                *node, storage_->limits, body_size, result.error)) {
            return result;
        }
        result.tag.body.reserve(body_size);
        append_u16(result.tag.body, node->sprite_character_id);
        append_u16(result.tag.body, node->sprite_frame_count);
        for (const TagNode& child : node->children) {
            if (!append_node(
                    child,
                    storage_->limits,
                    result.tag.body,
                    result.error)) {
                result.tag.body.clear();
                return result;
            }
        }
        return result;
    } catch (const std::bad_alloc&) {
        result.tag.body.clear();
        result.error = make_error(
            GfxDocumentErrorCode::allocation_failed,
            GfxDocumentErrorContext::edit);
        return result;
    } catch (const std::length_error&) {
        result.tag.body.clear();
        result.error = make_error(
            GfxDocumentErrorCode::output_too_large,
            GfxDocumentErrorContext::edit);
        return result;
    }
}

GfxCharacterIdResult GfxDocument::first_free_character_id(
    std::uint16_t first) const noexcept {
    GfxCharacterIdResult result{};
    if (storage_ == nullptr) {
        result.error = make_error(
            GfxDocumentErrorCode::invalid_argument,
            GfxDocumentErrorContext::character_index);
        return result;
    }
    const std::uint32_t begin = std::max<std::uint32_t>(first, 1);
    for (std::uint32_t candidate = begin; candidate <= 0xFFFFU; ++candidate) {
        const auto found = std::lower_bound(
            storage_->occupied_character_ids.begin(),
            storage_->occupied_character_ids.end(),
            static_cast<std::uint16_t>(candidate));
        if (found == storage_->occupied_character_ids.end() ||
            *found != candidate) {
            result.character_id = static_cast<std::uint16_t>(candidate);
            return result;
        }
    }
    result.error = make_error(
        GfxDocumentErrorCode::character_id_exhausted,
        GfxDocumentErrorContext::character_index);
    return result;
}

GfxCharacterIdsResult GfxDocument::first_free_character_ids(
    std::size_t count,
    std::uint16_t first) const {
    GfxCharacterIdsResult result{};
    if (storage_ == nullptr) {
        result.error = make_error(
            GfxDocumentErrorCode::invalid_argument,
            GfxDocumentErrorContext::character_index);
        return result;
    }

    const std::uint32_t begin = std::max<std::uint32_t>(first, 1);
    const std::size_t available_range = 0x10000U - begin;
    if (count > available_range) {
        result.error = make_error(
            GfxDocumentErrorCode::character_id_exhausted,
            GfxDocumentErrorContext::character_index,
            0,
            0,
            0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                count, std::numeric_limits<std::uint32_t>::max())));
        return result;
    }

    try {
        result.character_ids.reserve(count);
        auto occupied = std::lower_bound(
            storage_->occupied_character_ids.begin(),
            storage_->occupied_character_ids.end(),
            static_cast<std::uint16_t>(begin));
        for (std::uint32_t candidate = begin;
             candidate <= 0xFFFFU && result.character_ids.size() < count;
             ++candidate) {
            while (occupied != storage_->occupied_character_ids.end() &&
                   *occupied < candidate) {
                ++occupied;
            }
            if (occupied == storage_->occupied_character_ids.end() ||
                *occupied != candidate) {
                result.character_ids.push_back(
                    static_cast<std::uint16_t>(candidate));
            }
        }
    } catch (const std::bad_alloc&) {
        result.character_ids.clear();
        result.error = make_error(
            GfxDocumentErrorCode::allocation_failed,
            GfxDocumentErrorContext::character_index);
        return result;
    } catch (const std::length_error&) {
        result.character_ids.clear();
        result.error = make_error(
            GfxDocumentErrorCode::allocation_failed,
            GfxDocumentErrorContext::character_index);
        return result;
    }

    if (result.character_ids.size() != count) {
        result.character_ids.clear();
        result.error = make_error(
            GfxDocumentErrorCode::character_id_exhausted,
            GfxDocumentErrorContext::character_index,
            0,
            0,
            0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                count, std::numeric_limits<std::uint32_t>::max())));
    }
    return result;
}

GfxDocumentError GfxDocument::Storage::rebuild_index() {
        tags.clear();
        sprites.clear();
        placements.clear();
        definitions.clear();
        occupied_character_ids.clear();
        tag_index_by_token.clear();
        sprite_index_by_token.clear();
        node_by_token.clear();

        const std::size_t token_count =
            static_cast<std::size_t>(tree.next_token);
        if (tree.next_token == 0 ||
            static_cast<std::uint64_t>(token_count) != tree.next_token) {
            return make_error(
                GfxDocumentErrorCode::tag_limit_exceeded,
                GfxDocumentErrorContext::tag_header);
        }
        tag_index_by_token.assign(token_count, missing_index);
        sprite_index_by_token.assign(token_count, missing_index);
        node_by_token.assign(token_count, nullptr);

        std::array<bool, 65536> defined{};
        GfxDocumentError error{};
        const auto visit = [&]<typename Self>(
                               Self&& self,
                               const std::vector<TagNode>& sequence,
                               GfxTagHandle parent_sprite,
                               std::size_t depth) -> bool {
            for (std::size_t ordinal = 0; ordinal < sequence.size(); ++ordinal) {
                const TagNode& node = sequence[ordinal];
                if (node.token == 0 || node.token >= node_by_token.size() ||
                    node_by_token[static_cast<std::size_t>(node.token)] !=
                        nullptr) {
                    error = make_error(
                        GfxDocumentErrorCode::malformed_tag,
                        GfxDocumentErrorContext::tag_header,
                        node.source_tag_offset,
                        node.code);
                    return false;
                }
                const std::size_t token_index =
                    static_cast<std::size_t>(node.token);
                const GfxTagHandle handle{
                    node.token, revision, document_identity};
                std::uint16_t character_id{};
                const CharacterDefinitionState definition_state =
                    character_id_of(node, character_id);
                const bool direct_definition = definition_state ==
                    CharacterDefinitionState::definition;
                if (definition_state == CharacterDefinitionState::malformed) {
                    error = make_error(
                        GfxDocumentErrorCode::malformed_tag,
                        GfxDocumentErrorContext::character_index,
                        node.source_body_offset,
                        node.code);
                    return false;
                }
                if (direct_definition && character_id == 0) {
                    error = make_error(
                        GfxDocumentErrorCode::invalid_character_id,
                        GfxDocumentErrorContext::character_index,
                        node.source_body_offset,
                        node.code,
                        character_id);
                    return false;
                }

                std::vector<std::uint16_t> imported_ids{};
                const bool import_table = node.code == kTagImportAssets ||
                    node.code == kTagImportAssets2;
                if (import_table && !parse_import_character_ids(
                        node, limits.maximum_name_bytes, imported_ids)) {
                    error = make_error(
                        GfxDocumentErrorCode::malformed_tag,
                        GfxDocumentErrorContext::character_index,
                        node.source_body_offset,
                        node.code);
                    return false;
                }
                node_by_token[token_index] = &node;
                tag_index_by_token[token_index] = tags.size();
                tags.push_back({
                    .handle = handle,
                    .parent_sprite = parent_sprite,
                    .code = node.code,
                    .body_size = node.original_body_size,
                    .ordinal = ordinal,
                    .nesting_depth = depth,
                    .header_form = node.force_long_header
                        ? GfxTagHeaderForm::long_form
                        : GfxTagHeaderForm::short_form,
                    .defines_character = direct_definition,
                    .character_id = direct_definition
                        ? character_id
                        : std::uint16_t{0},
                });

                const auto register_definition = [&](
                                                     std::uint16_t id,
                                                     bool expose_definition) -> bool {
                    if (id == 0) {
                        error = make_error(
                            GfxDocumentErrorCode::invalid_character_id,
                            GfxDocumentErrorContext::character_index,
                            node.source_body_offset,
                            node.code,
                            id);
                        return false;
                    }
                    if (defined[id]) {
                        error = make_error(
                            GfxDocumentErrorCode::duplicate_character_id,
                            GfxDocumentErrorContext::character_index,
                            node.source_body_offset,
                            node.code,
                            id);
                        return false;
                    }
                    defined[id] = true;
                    occupied_character_ids.push_back(id);
                    if (expose_definition) {
                        definitions.push_back({
                            .character_id = id,
                            .tag_code = node.code,
                            .tag = handle,
                        });
                    }
                    return true;
                };
                if (direct_definition &&
                    !register_definition(character_id, true)) {
                    return false;
                }
                for (const std::uint16_t imported_id : imported_ids) {
                    if (!register_definition(imported_id, false)) return false;
                }

                std::uint16_t scaling_grid_target{};
                if (scaling_grid_target_of(node, scaling_grid_target) &&
                    scaling_grid_target != 0) {
                    // References reserve allocator IDs but deliberately do not
                    // participate in duplicate-definition validation.
                    occupied_character_ids.push_back(scaling_grid_target);
                }

                if (node.code == kTagPlaceObject2) {
                    GfxPlacementInfo placement{};
                    if (!parse_place_object2(
                            node,
                            parent_sprite,
                            revision,
                            document_identity,
                            limits.maximum_name_bytes,
                            placement)) {
                        error = make_error(
                            GfxDocumentErrorCode::malformed_placement,
                            GfxDocumentErrorContext::placement,
                            node.source_body_offset,
                            node.code);
                        return false;
                    }
                    if (placement.has_character &&
                        placement.character_id != 0) {
                        occupied_character_ids.push_back(
                            placement.character_id);
                    }
                    placements.push_back(std::move(placement));
                } else if (node.code == kTagPlaceObject3) {
                    GfxPlacementInfo placement{};
                    // Scaleform extends PlaceObject3. If the common prefix is
                    // not safely recognizable, retain the complete tag as
                    // opaque data instead of rejecting a compatible movie.
                    if (parse_place_object3(
                            node,
                            parent_sprite,
                            revision,
                            document_identity,
                            limits.maximum_name_bytes,
                            placement)) {
                        if (placement.has_character &&
                            placement.character_id != 0) {
                            occupied_character_ids.push_back(
                                placement.character_id);
                        }
                        placements.push_back(std::move(placement));
                    }
                }

                if (node.parsed_sprite) {
                    sprite_index_by_token[token_index] = sprites.size();
                    sprites.push_back({
                        .definition = handle,
                        .parent_sprite = parent_sprite,
                        .character_id = node.sprite_character_id,
                        .frame_count = node.sprite_frame_count,
                        .child_count = node.children.size(),
                        .nesting_depth = depth,
                    });
                    if (!self(
                            self,
                            node.children,
                            handle,
                            depth + 1)) {
                        return false;
                    }
                }
            }
            return true;
        };

        if (!visit(visit, tree.top_level, {}, 0)) return error;
        std::sort(
            definitions.begin(),
            definitions.end(),
            [](const auto& left, const auto& right) {
                return left.character_id < right.character_id;
            });
        std::sort(
            occupied_character_ids.begin(), occupied_character_ids.end());
        occupied_character_ids.erase(
            std::unique(
                occupied_character_ids.begin(), occupied_character_ids.end()),
            occupied_character_ids.end());
        return {};
}

namespace {

[[nodiscard]] bool find_node_mutable(
    std::vector<TagNode>& sequence,
    std::uint64_t token,
    MutableNodeLocation& location) noexcept {
    for (std::size_t index = 0; index < sequence.size(); ++index) {
        TagNode& node = sequence[index];
        if (node.token == token) {
            location = {&sequence, index};
            return true;
        }
        if (node.parsed_sprite &&
            find_node_mutable(node.children, token, location)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool node_body_size(
    const TagNode& node,
    const GfxDocumentLimits& limits,
    std::size_t& size,
    GfxDocumentError& error);

[[nodiscard]] bool encoded_tag_size(
    const TagNode& node,
    const GfxDocumentLimits& limits,
    std::size_t& size,
    GfxDocumentError& error) {
    std::size_t body_size{};
    if (!node_body_size(node, limits, body_size, error)) return false;
    const bool long_form = node.force_long_header || body_size >= kLongTagMarker;
    const std::size_t header_size = long_form ? 6U : 2U;
    if (!checked_add(header_size, body_size, size)) {
        error = make_error(
            GfxDocumentErrorCode::output_too_large,
            GfxDocumentErrorContext::serialization,
            0,
            node.code);
        return false;
    }
    return true;
}

[[nodiscard]] bool node_body_size(
    const TagNode& node,
    const GfxDocumentLimits& limits,
    std::size_t& size,
    GfxDocumentError& error) {
    if (!node.parsed_sprite) {
        size = node.body.size();
    } else {
        size = 4;
        for (const TagNode& child : node.children) {
            std::size_t child_size{};
            if (!encoded_tag_size(child, limits, child_size, error) ||
                !checked_add(size, child_size, size)) {
                if (error.success()) {
                    error = make_error(
                        GfxDocumentErrorCode::output_too_large,
                        GfxDocumentErrorContext::serialization,
                        0,
                        node.code,
                        node.sprite_character_id);
                }
                return false;
            }
        }
    }
    if (size > limits.maximum_tag_body_bytes ||
        size > std::numeric_limits<std::uint32_t>::max()) {
        error = make_error(
            GfxDocumentErrorCode::tag_body_too_large,
            GfxDocumentErrorContext::serialization,
            0,
            node.code,
            node.parsed_sprite ? node.sprite_character_id : 0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                size, std::numeric_limits<std::uint32_t>::max())));
        return false;
    }
    return true;
}

[[nodiscard]] bool append_node(
    const TagNode& node,
    const GfxDocumentLimits& limits,
    std::vector<std::uint8_t>& output,
    GfxDocumentError& error) {
    std::size_t body_size{};
    if (!node_body_size(node, limits, body_size, error)) return false;
    if (node.code > kMaximumTagCode) {
        error = make_error(
            GfxDocumentErrorCode::invalid_edit,
            GfxDocumentErrorContext::serialization,
            0,
            node.code);
        return false;
    }
    const bool long_form = node.force_long_header || body_size >= kLongTagMarker;
    const std::uint16_t packed = static_cast<std::uint16_t>(
        (node.code << 6U) |
        (long_form ? kLongTagMarker : static_cast<std::uint16_t>(body_size)));
    append_u16(output, packed);
    if (long_form) append_u32(output, static_cast<std::uint32_t>(body_size));

    if (!node.parsed_sprite) {
        output.insert(output.end(), node.body.begin(), node.body.end());
        return true;
    }

    append_u16(output, node.sprite_character_id);
    append_u16(output, node.sprite_frame_count);
    for (const TagNode& child : node.children) {
        if (!append_node(child, limits, output, error)) return false;
    }
    return true;
}

[[nodiscard]] bool serialize_tree(
    const ParsedTree& tree,
    const GfxDocumentLimits& limits,
    std::vector<std::uint8_t>& output,
    GfxDocumentError& error) {
    std::size_t declared = tree.movie_header.size();
    for (const TagNode& node : tree.top_level) {
        std::size_t tag_size{};
        if (!encoded_tag_size(node, limits, tag_size, error) ||
            !checked_add(declared, tag_size, declared)) {
            if (error.success()) {
                error = make_error(
                    GfxDocumentErrorCode::output_too_large,
                    GfxDocumentErrorContext::serialization);
            }
            return false;
        }
    }
    if (declared < 12 || declared > limits.maximum_declared_bytes ||
        declared > std::numeric_limits<std::uint32_t>::max()) {
        error = make_error(
            GfxDocumentErrorCode::output_too_large,
            GfxDocumentErrorContext::serialization,
            0,
            0,
            0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                declared, std::numeric_limits<std::uint32_t>::max())));
        return false;
    }
    std::size_t physical{};
    if (!checked_add(declared, tree.trailing.size(), physical) ||
        physical > limits.maximum_physical_bytes) {
        error = make_error(
            GfxDocumentErrorCode::output_too_large,
            GfxDocumentErrorContext::serialization);
        return false;
    }
    if (tree.movie_header.size() < 8) {
        error = make_error(
            GfxDocumentErrorCode::invalid_movie_header,
            GfxDocumentErrorContext::serialization);
        return false;
    }

    output.clear();
    output.reserve(physical);
    output.insert(
        output.end(), tree.movie_header.begin(), tree.movie_header.end());
    write_u32(output, 4, static_cast<std::uint32_t>(declared));
    for (const TagNode& node : tree.top_level) {
        if (!append_node(node, limits, output, error)) return false;
    }
    output.insert(output.end(), tree.trailing.begin(), tree.trailing.end());
    return true;
}

[[nodiscard]] TagNode raw_node_from_edit(
    const GfxRawTag& tag,
    std::uint64_t token) {
    TagNode node{};
    node.token = token;
    node.code = tag.code;
    node.force_long_header =
        tag.header_form == GfxTagHeaderForm::long_form;
    node.body = tag.body;
    node.original_body_size = node.body.size();
    return node;
}

[[nodiscard]] GfxDocumentError validate_raw_tag(
    const GfxRawTag& tag,
    const GfxDocumentLimits& limits) noexcept {
    const bool valid_header_form =
        tag.header_form == GfxTagHeaderForm::automatic ||
        tag.header_form == GfxTagHeaderForm::short_form ||
        tag.header_form == GfxTagHeaderForm::long_form;
    if (!valid_header_form || tag.code > kMaximumTagCode ||
        tag.body.size() > limits.maximum_tag_body_bytes ||
        tag.body.size() > std::numeric_limits<std::uint32_t>::max() ||
        (tag.header_form == GfxTagHeaderForm::short_form &&
            tag.body.size() >= kLongTagMarker)) {
        return make_error(
            GfxDocumentErrorCode::invalid_edit,
            GfxDocumentErrorContext::edit,
            0,
            tag.code,
            0,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                tag.body.size(), std::numeric_limits<std::uint32_t>::max())));
    }
    return {};
}

} // namespace

} // namespace erui::gfx
