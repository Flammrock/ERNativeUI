#include "gfx_patch.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace erui::gfx {
namespace {

constexpr std::uint16_t kTagEnd = 0;
constexpr std::uint16_t kTagShowFrame = 1;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kControllerButtonSprite = 97;
constexpr std::uint16_t kControllerCaptionWrapper = 101;
constexpr std::uint16_t kControllerButtonLabelWrapper = 193;
constexpr std::uint16_t kMinimumControllerRows = 6;
constexpr std::uint16_t kMaximumControllerRows = 13;
constexpr std::string_view kControllerPanelName = "ControllSetting";

struct TagView {
    std::uint16_t code{};
    std::size_t tag_start{};
    std::size_t header_size{};
    std::size_t body_start{};
    std::size_t body_end{};
};

struct MatrixTranslation {
    std::int32_t x{};
    std::int32_t y{};
};

struct Placement {
    TagView tag{};
    std::uint8_t flags{};
    std::uint16_t depth{};
    std::uint16_t character_id{};
    std::string name{};
    MatrixTranslation translation{};
    bool has_character{};
    bool has_matrix{};
    bool has_name{};
};

struct Sprite {
    TagView tag{};
    std::uint16_t id{};
    std::uint16_t frame_count{};
    std::vector<TagView> tags{};
    std::vector<Placement> placements{};
};

struct ParsedGfx {
    std::uint8_t version{};
    std::uint32_t declared_length{};
    std::size_t top_level_start{};
    std::vector<TagView> top_level_tags{};
    std::vector<Sprite> sprites{};
};

struct ControllerContext {
    const Sprite* window_list{};
    const Sprite* controller{};
    std::uint16_t item_character_id{};
    std::uint16_t row_count{};
    std::vector<const Placement*> items{};
};

class BitReader {
public:
    BitReader(std::span<const std::uint8_t> bytes, std::size_t byte_offset)
        : bytes_(bytes), bit_position_(byte_offset * 8) {}

    [[nodiscard]] bool read(std::size_t count, std::uint32_t& value) noexcept {
        if (count > 32 || bit_position_ + count > bytes_.size() * 8) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const std::size_t byte_index = bit_position_ / 8;
            const std::size_t bit_index = 7 - (bit_position_ % 8);
            value = (value << 1) | ((bytes_[byte_index] >> bit_index) & 1U);
            ++bit_position_;
        }
        return true;
    }

    [[nodiscard]] bool read_signed(std::size_t count, std::int32_t& value) noexcept {
        std::uint32_t raw{};
        if (!read(count, raw)) {
            return false;
        }
        if (count != 0 && (raw & (1U << (count - 1))) != 0) {
            const std::uint32_t extension = ~((1U << count) - 1U);
            raw |= extension;
        }
        value = static_cast<std::int32_t>(raw);
        return true;
    }

    void align() noexcept { bit_position_ = (bit_position_ + 7) & ~std::size_t{7}; }
    [[nodiscard]] std::size_t byte_position() const noexcept { return bit_position_ / 8; }

private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t bit_position_{};
};

[[nodiscard]] bool read_u16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint16_t& value) noexcept {
    if (offset + 2 > bytes.size()) {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
    return true;
}

[[nodiscard]] bool read_u32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint32_t& value) noexcept {
    if (offset + 4 > bytes.size()) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    return true;
}

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
}

[[nodiscard]] bool parse_rect_end(
    std::span<const std::uint8_t> bytes,
    std::size_t& header_end) noexcept {
    if (bytes.size() < 12) {
        return false;
    }
    BitReader reader(bytes, 8);
    std::uint32_t bit_count{};
    if (!reader.read(5, bit_count) || bit_count > 31) {
        return false;
    }
    std::uint32_t ignored{};
    for (int field = 0; field < 4; ++field) {
        if (!reader.read(bit_count, ignored)) {
            return false;
        }
    }
    reader.align();
    header_end = reader.byte_position() + 4; // FrameRate + FrameCount.
    return header_end <= bytes.size();
}

[[nodiscard]] bool parse_tags(
    std::span<const std::uint8_t> bytes,
    std::size_t begin,
    std::size_t end,
    std::vector<TagView>& tags,
    std::string& error) noexcept {
    if (begin > end || end > bytes.size()) {
        error = "tag range lies outside the GFX";
        return false;
    }

    std::size_t offset = begin;
    while (offset + 2 <= end) {
        const std::size_t tag_start = offset;
        std::uint16_t header{};
        if (!read_u16(bytes, offset, header)) {
            error = "truncated tag header";
            return false;
        }
        offset += 2;

        const std::uint16_t code = static_cast<std::uint16_t>(header >> 6);
        std::uint32_t body_length = header & 0x3FU;
        std::size_t header_size = 2;
        if (body_length == 0x3FU) {
            if (!read_u32(bytes, offset, body_length)) {
                error = "truncated long tag length";
                return false;
            }
            offset += 4;
            header_size = 6;
        }

        if (body_length > end - offset) {
            error = "tag body exceeds its containing range";
            return false;
        }

        TagView tag{
            .code = code,
            .tag_start = tag_start,
            .header_size = header_size,
            .body_start = offset,
            .body_end = offset + body_length,
        };
        tags.push_back(tag);
        offset = tag.body_end;
        if (code == kTagEnd) {
            if (offset != end) {
                error = "data follows an End tag inside its declared tag range";
                return false;
            }
            return true;
        }
    }

    error = "tag sequence does not terminate with an End tag";
    return false;
}

[[nodiscard]] bool skip_matrix(
    std::span<const std::uint8_t> bytes,
    std::size_t& offset,
    std::size_t end,
    MatrixTranslation& translation) noexcept {
    BitReader reader(bytes, offset);
    std::uint32_t present{};
    std::uint32_t bit_count{};
    std::int32_t ignored{};

    if (!reader.read(1, present)) {
        return false;
    }
    if (present != 0) {
        if (!reader.read(5, bit_count) || bit_count > 31 ||
            !reader.read_signed(bit_count, ignored) ||
            !reader.read_signed(bit_count, ignored)) {
            return false;
        }
    }

    if (!reader.read(1, present)) {
        return false;
    }
    if (present != 0) {
        if (!reader.read(5, bit_count) || bit_count > 31 ||
            !reader.read_signed(bit_count, ignored) ||
            !reader.read_signed(bit_count, ignored)) {
            return false;
        }
    }

    if (!reader.read(5, bit_count) || bit_count > 31 ||
        !reader.read_signed(bit_count, translation.x) ||
        !reader.read_signed(bit_count, translation.y)) {
        return false;
    }
    reader.align();
    offset = reader.byte_position();
    return offset <= end;
}

[[nodiscard]] bool skip_color_transform(
    std::span<const std::uint8_t> bytes,
    std::size_t& offset,
    std::size_t end) noexcept {
    BitReader reader(bytes, offset);
    std::uint32_t has_add{};
    std::uint32_t has_multiply{};
    std::uint32_t bit_count{};
    if (!reader.read(1, has_add) || !reader.read(1, has_multiply) ||
        !reader.read(4, bit_count) || bit_count > 31) {
        return false;
    }
    std::int32_t ignored{};
    if (has_multiply != 0) {
        for (int component = 0; component < 4; ++component) {
            if (!reader.read_signed(bit_count, ignored)) {
                return false;
            }
        }
    }
    if (has_add != 0) {
        for (int component = 0; component < 4; ++component) {
            if (!reader.read_signed(bit_count, ignored)) {
                return false;
            }
        }
    }
    reader.align();
    offset = reader.byte_position();
    return offset <= end;
}

[[nodiscard]] bool read_c_string(
    std::span<const std::uint8_t> bytes,
    std::size_t& offset,
    std::size_t end,
    std::string& value) noexcept {
    if (offset >= end) {
        return false;
    }
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto finish = bytes.begin() + static_cast<std::ptrdiff_t>(end);
    const auto terminator = std::find(begin, finish, std::uint8_t{0});
    if (terminator == finish) {
        return false;
    }
    value.assign(begin, terminator);
    offset += static_cast<std::size_t>(std::distance(begin, terminator)) + 1;
    return true;
}

[[nodiscard]] bool parse_placement(
    std::span<const std::uint8_t> bytes,
    const TagView& tag,
    Placement& placement) noexcept {
    if (tag.code != kTagPlaceObject2 || tag.body_start + 3 > tag.body_end) {
        return false;
    }

    std::size_t offset = tag.body_start;
    placement = {};
    placement.tag = tag;
    placement.flags = bytes[offset++];
    if (!read_u16(bytes, offset, placement.depth)) {
        return false;
    }
    offset += 2;

    if ((placement.flags & 0x02U) != 0) {
        placement.has_character = true;
        if (!read_u16(bytes, offset, placement.character_id)) {
            return false;
        }
        offset += 2;
    }
    if ((placement.flags & 0x04U) != 0) {
        placement.has_matrix = true;
        if (!skip_matrix(bytes, offset, tag.body_end, placement.translation)) {
            return false;
        }
    }
    if ((placement.flags & 0x08U) != 0 &&
        !skip_color_transform(bytes, offset, tag.body_end)) {
        return false;
    }
    if ((placement.flags & 0x10U) != 0) {
        if (offset + 2 > tag.body_end) {
            return false;
        }
        offset += 2;
    }
    if ((placement.flags & 0x20U) != 0) {
        placement.has_name = true;
        if (!read_c_string(bytes, offset, tag.body_end, placement.name)) {
            return false;
        }
    }
    if ((placement.flags & 0x40U) != 0) {
        if (offset + 2 > tag.body_end) {
            return false;
        }
        offset += 2;
    }
    // ClipActions, when present, are intentionally left opaque. The item
    // placements used by the Game Options panels do not carry them.
    return offset <= tag.body_end;
}

[[nodiscard]] bool parse_gfx(
    std::span<const std::uint8_t> input,
    ParsedGfx& parsed,
    ErrorCode& code,
    std::string& error) noexcept {
    parsed = {};
    if (input.size() < 12 || input[0] != 'G' || input[1] != 'F' || input[2] != 'X') {
        code = ErrorCode::invalid_gfx;
        error = "input is not an uncompressed GFX stream";
        return false;
    }
    parsed.version = input[3];
    if (!read_u32(input, 4, parsed.declared_length) ||
        parsed.declared_length < 12 || parsed.declared_length > input.size()) {
        code = ErrorCode::invalid_gfx;
        error = "declared GFX length is invalid";
        return false;
    }
    for (std::size_t offset = parsed.declared_length; offset < input.size(); ++offset) {
        if (input[offset] != 0) {
            code = ErrorCode::unsupported_gfx;
            error = "non-zero data follows the declared GFX length";
            return false;
        }
    }
    if (!parse_rect_end(input.first(parsed.declared_length), parsed.top_level_start)) {
        code = ErrorCode::invalid_gfx;
        error = "invalid GFX movie header";
        return false;
    }
    if (!parse_tags(
            input.first(parsed.declared_length),
            parsed.top_level_start,
            parsed.declared_length,
            parsed.top_level_tags,
            error)) {
        code = ErrorCode::invalid_gfx;
        return false;
    }

    for (const TagView& tag : parsed.top_level_tags) {
        if (tag.code != kTagDefineSprite) {
            continue;
        }
        if (tag.body_start + 4 > tag.body_end) {
            code = ErrorCode::invalid_gfx;
            error = "truncated DefineSprite tag";
            return false;
        }

        Sprite sprite{};
        sprite.tag = tag;
        if (!read_u16(input, tag.body_start, sprite.id) ||
            !read_u16(input, tag.body_start + 2, sprite.frame_count)) {
            code = ErrorCode::invalid_gfx;
            error = "truncated DefineSprite header";
            return false;
        }
        if (!parse_tags(
                input.first(parsed.declared_length),
                tag.body_start + 4,
                tag.body_end,
                sprite.tags,
                error)) {
            code = ErrorCode::invalid_gfx;
            return false;
        }
        for (const TagView& child : sprite.tags) {
            if (child.code != kTagPlaceObject2) {
                continue;
            }
            Placement placement{};
            if (!parse_placement(input, child, placement)) {
                code = ErrorCode::invalid_gfx;
                error = "invalid PlaceObject2 tag inside a sprite";
                return false;
            }
            sprite.placements.push_back(std::move(placement));
        }
        parsed.sprites.push_back(std::move(sprite));
    }
    return true;
}

[[nodiscard]] const Sprite* find_sprite(const ParsedGfx& parsed, std::uint16_t id) noexcept {
    const auto iterator = std::find_if(
        parsed.sprites.begin(),
        parsed.sprites.end(),
        [id](const Sprite& sprite) { return sprite.id == id; });
    return iterator == parsed.sprites.end() ? nullptr : &*iterator;
}

[[nodiscard]] std::optional<std::uint16_t> parse_item_index(std::string_view name) noexcept {
    constexpr std::string_view prefix = "Item_";
    constexpr std::string_view suffix = "_0";
    if (!name.starts_with(prefix) || !name.ends_with(suffix) ||
        name.size() <= prefix.size() + suffix.size()) {
        return std::nullopt;
    }
    const std::string_view digits = name.substr(
        prefix.size(),
        name.size() - prefix.size() - suffix.size());
    std::uint16_t value{};
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool locate_controller(
    const ParsedGfx& parsed,
    ControllerContext& context,
    ErrorCode& code,
    std::string& error) noexcept {
    context = {};
    std::uint16_t controller_id{};
    std::size_t panel_references{};

    for (const Sprite& sprite : parsed.sprites) {
        for (const Placement& placement : sprite.placements) {
            if (placement.has_name && placement.name == kControllerPanelName) {
                if (!placement.has_character) {
                    code = ErrorCode::controller_panel_not_found;
                    error = "ControllSetting placement has no character ID";
                    return false;
                }
                ++panel_references;
                controller_id = placement.character_id;
                context.window_list = &sprite;
            }
        }
    }
    if (panel_references != 1 || context.window_list == nullptr) {
        code = ErrorCode::controller_panel_not_found;
        error = panel_references == 0
            ? "WindowList.ControllSetting was not found"
            : "multiple ControllSetting placements were found";
        return false;
    }

    context.controller = find_sprite(parsed, controller_id);
    if (context.controller == nullptr) {
        code = ErrorCode::controller_panel_not_found;
        error = "ControllSetting references a missing sprite";
        return false;
    }

    std::unordered_map<std::uint16_t, const Placement*> indexed_items{};
    std::optional<std::uint16_t> item_character{};
    for (const Placement& placement : context.controller->placements) {
        if (!placement.has_name) {
            continue;
        }
        const std::optional<std::uint16_t> index = parse_item_index(placement.name);
        if (!index) {
            continue;
        }
        if (!placement.has_character || !placement.has_matrix ||
            placement.translation.x != 0) {
            code = ErrorCode::controller_items_invalid;
            error = "ControllSetting contains an unsupported Item_N_0 placement";
            return false;
        }
        if (!indexed_items.emplace(*index, &placement).second) {
            code = ErrorCode::controller_items_invalid;
            error = "ControllSetting contains duplicate item indices";
            return false;
        }
        if (!item_character) {
            item_character = placement.character_id;
        } else if (*item_character != placement.character_id) {
            code = ErrorCode::controller_items_invalid;
            error = "ControllSetting item placements use different character IDs";
            return false;
        }
    }

    if (!item_character || indexed_items.empty() || indexed_items.size() > kMaximumControllerRows) {
        code = ErrorCode::controller_items_invalid;
        error = "ControllSetting has no supported generic item placements";
        return false;
    }

    const auto maximum = std::max_element(
        indexed_items.begin(),
        indexed_items.end(),
        [](const auto& left, const auto& right) { return left.first < right.first; });
    const std::uint16_t row_count = static_cast<std::uint16_t>(maximum->first + 1);
    if (row_count != indexed_items.size()) {
        code = ErrorCode::controller_items_invalid;
        error = "ControllSetting Item_N_0 indices are not contiguous from zero";
        return false;
    }

    context.item_character_id = *item_character;
    context.row_count = row_count;
    context.items.reserve(row_count);
    for (std::uint16_t index = 0; index < row_count; ++index) {
        const auto iterator = indexed_items.find(index);
        if (iterator == indexed_items.end()) {
            code = ErrorCode::controller_items_invalid;
            error = "ControllSetting item sequence contains a gap";
            return false;
        }
        const std::int32_t expected_y = -4800 + static_cast<std::int32_t>(index) * 1000;
        if (iterator->second->translation.y != expected_y) {
            code = ErrorCode::controller_items_invalid;
            error = "ControllSetting item placement spacing is not recognized";
            return false;
        }
        context.items.push_back(iterator->second);
    }
    return true;
}

[[nodiscard]] Inspection make_inspection(
    const ParsedGfx& parsed,
    const ControllerContext& context) {
    Inspection result{};
    result.gfx_version = parsed.version;
    result.declared_length = parsed.declared_length;
    result.window_list_sprite = context.window_list->id;
    result.controller_sprite = context.controller->id;
    result.controller_item_character = context.item_character_id;
    result.controller_rows = context.row_count;
    return result;
}

[[nodiscard]] const Placement* find_donor(
    const ParsedGfx& parsed,
    const ControllerContext& context,
    std::uint16_t index) noexcept {
    const std::string name = "Item_" + std::to_string(index) + "_0";
    const std::int32_t expected_y = -4800 + static_cast<std::int32_t>(index) * 1000;
    for (const Sprite& sprite : parsed.sprites) {
        if (&sprite == context.controller) {
            continue;
        }
        for (const Placement& placement : sprite.placements) {
            if (placement.has_name && placement.name == name &&
                placement.has_character &&
                placement.character_id == context.item_character_id &&
                placement.has_matrix &&
                placement.translation.x == 0 &&
                placement.translation.y == expected_y &&
                placement.flags == 0x26U) {
                return &placement;
            }
        }
    }
    return nullptr;
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& output) : output_(output) {}

    void write(std::uint32_t value, std::uint32_t count) {
        for (std::uint32_t bit = count; bit != 0; --bit) {
            if (bit_offset_ == 0) output_.push_back(0);
            output_.back() |= static_cast<std::uint8_t>(
                ((value >> (bit - 1)) & 1U) << (7U - bit_offset_));
            bit_offset_ = static_cast<std::uint8_t>((bit_offset_ + 1U) & 7U);
        }
    }

private:
    std::vector<std::uint8_t>& output_;
    std::uint8_t bit_offset_{};
};

[[nodiscard]] std::vector<std::uint8_t> make_controller_placement(
    std::uint16_t character_id,
    std::uint16_t index,
    std::uint16_t depth) {
    constexpr std::uint8_t flags = 0x26U; // character, matrix, name
    const std::int32_t y = -4800 + static_cast<std::int32_t>(index) * 1000;
    constexpr std::uint32_t translation_bits = 15;

    std::vector<std::uint8_t> body{};
    body.push_back(flags);
    append_u16(body, depth);
    append_u16(body, character_id);
    BitWriter matrix(body);
    matrix.write(0, 1); // no scale
    matrix.write(0, 1); // no rotate/skew
    matrix.write(translation_bits, 5);
    matrix.write(0, translation_bits);
    matrix.write(static_cast<std::uint32_t>(y) &
        ((1U << translation_bits) - 1U), translation_bits);

    const std::string name = "Item_" + std::to_string(index) + "_0";
    body.insert(body.end(), name.begin(), name.end());
    body.push_back(0);

    std::vector<std::uint8_t> tag{};
    const std::uint32_t body_size = static_cast<std::uint32_t>(body.size());
    if (body_size < 0x3FU) {
        append_u16(tag, static_cast<std::uint16_t>(
            (kTagPlaceObject2 << 6U) | body_size));
    } else {
        append_u16(tag, static_cast<std::uint16_t>(
            (kTagPlaceObject2 << 6U) | 0x3FU));
        tag.push_back(static_cast<std::uint8_t>(body_size));
        tag.push_back(static_cast<std::uint8_t>(body_size >> 8U));
        tag.push_back(static_cast<std::uint8_t>(body_size >> 16U));
        tag.push_back(static_cast<std::uint8_t>(body_size >> 24U));
    }
    tag.insert(tag.end(), body.begin(), body.end());
    return tag;
}

[[nodiscard]] std::vector<std::uint8_t> make_named_placement(
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    constexpr std::uint8_t flags = 0x26U;
    constexpr std::uint32_t translation_bits = 15;
    std::vector<std::uint8_t> body{};
    body.push_back(flags);
    append_u16(body, depth);
    append_u16(body, character_id);
    BitWriter matrix(body);
    matrix.write(0, 1);
    matrix.write(0, 1);
    matrix.write(translation_bits, 5);
    matrix.write(static_cast<std::uint32_t>(x) & 0x7FFFU, translation_bits);
    matrix.write(static_cast<std::uint32_t>(y) & 0x7FFFU, translation_bits);
    body.insert(body.end(), name.begin(), name.end());
    body.push_back(0);

    std::vector<std::uint8_t> output{};
    append_u16(output, static_cast<std::uint16_t>(
        (kTagPlaceObject2 << 6U) | body.size()));
    output.insert(output.end(), body.begin(), body.end());
    return output;
}

[[nodiscard]] std::optional<std::size_t> show_frame_offset(const Sprite& sprite) noexcept {
    const auto iterator = std::find_if(
        sprite.tags.begin(),
        sprite.tags.end(),
        [](const TagView& tag) { return tag.code == kTagShowFrame; });
    if (iterator == sprite.tags.end()) {
        return std::nullopt;
    }
    return iterator->tag_start;
}

[[nodiscard]] bool patch_tag_body_length(
    std::vector<std::uint8_t>& output,
    const TagView& tag,
    std::size_t added,
    std::string& error) {
    const std::size_t old_length = tag.body_end - tag.body_start;
    if (added > std::numeric_limits<std::uint32_t>::max() - old_length) {
        error = "patched sprite length overflows the GFX tag format";
        return false;
    }
    const std::size_t new_length = old_length + added;

    if (tag.header_size == 6) {
        write_u32(output, tag.tag_start + 2, static_cast<std::uint32_t>(new_length));
        return true;
    }
    if (tag.header_size == 2 && new_length < 0x3F) {
        std::uint16_t header{};
        if (!read_u16(output, tag.tag_start, header)) {
            error = "could not update the DefineSprite tag header";
            return false;
        }
        header = static_cast<std::uint16_t>((header & 0xFFC0U) | new_length);
        write_u16(output, tag.tag_start, header);
        return true;
    }

    error = "ControllSetting uses an unsupported short DefineSprite header";
    return false;
}

[[nodiscard]] PatchResult fail(ErrorCode code, std::string message) {
    PatchResult result{};
    result.error = code;
    result.message = std::move(message);
    return result;
}

[[nodiscard]] bool ensure_controller_button_label(
    std::vector<std::uint8_t>& output,
    std::string& error,
    bool& added) {
    added = false;
    ParsedGfx parsed{};
    ErrorCode code{};
    if (!parse_gfx(output, parsed, code, error)) return false;

    const Sprite* button = find_sprite(parsed, kControllerButtonSprite);
    const Sprite* caption = find_sprite(parsed, kControllerCaptionWrapper);
    if (!button || !caption) {
        // Synthetic fixtures and older supported assets do not necessarily
        // contain the PC button widget. Row expansion remains useful there.
        return true;
    }

    // Clone the exact Caption wrapper used by Slider, ComboBox and TextInput.
    // It contains a PlaceObject3 child with the native -4360 twip transform
    // and filter/shadow list. Only its ActionScript binding name differs:
    // Caption exposes Text_0, while an injected Button requires Text_1.Text.
    std::vector<std::uint8_t> cloned(
        output.begin() + static_cast<std::ptrdiff_t>(caption->tag.tag_start),
        output.begin() + static_cast<std::ptrdiff_t>(caption->tag.body_end));
    write_u16(cloned, caption->tag.header_size, kControllerButtonLabelWrapper);

    constexpr std::array<std::uint8_t, 7> source_name{
        'T', 'e', 'x', 't', '_', '0', 0};
    const auto name = std::search(
        cloned.begin(), cloned.end(), source_name.begin(), source_name.end());
    if (name == cloned.end() ||
        std::search(std::next(name), cloned.end(),
            source_name.begin(), source_name.end()) != cloned.end()) {
        error = "Controller Caption wrapper does not contain one Text_0 binding";
        return false;
    }
    cloned.erase(std::next(name, 4), std::next(name, 6));

    const std::size_t cloned_body_size =
        caption->tag.body_end - caption->tag.body_start - 2;
    if (caption->tag.header_size == 2 && cloned_body_size < 0x3FU) {
        std::uint16_t header{};
        if (!read_u16(cloned, 0, header)) {
            error = "could not update cloned Caption header";
            return false;
        }
        header = static_cast<std::uint16_t>(
            (header & 0xFFC0U) | cloned_body_size);
        write_u16(cloned, 0, header);
    } else if (caption->tag.header_size == 6) {
        write_u32(cloned, 2, static_cast<std::uint32_t>(cloned_body_size));
    } else {
        error = "Controller Caption wrapper has an unsupported tag header";
        return false;
    }

    const Sprite* existing_wrapper =
        find_sprite(parsed, kControllerButtonLabelWrapper);
    if (existing_wrapper) {
        const auto old_begin = output.begin() +
            static_cast<std::ptrdiff_t>(existing_wrapper->tag.tag_start);
        const auto old_end = output.begin() +
            static_cast<std::ptrdiff_t>(existing_wrapper->tag.body_end);
        if (static_cast<std::size_t>(std::distance(old_begin, old_end)) !=
                cloned.size() ||
            !std::equal(old_begin, old_end, cloned.begin())) {
            const std::ptrdiff_t delta = static_cast<std::ptrdiff_t>(cloned.size()) -
                std::distance(old_begin, old_end);
            output.erase(old_begin, old_end);
            output.insert(
                output.begin() + static_cast<std::ptrdiff_t>(
                    existing_wrapper->tag.tag_start),
                cloned.begin(), cloned.end());
            write_u32(output, 4, static_cast<std::uint32_t>(
                static_cast<std::ptrdiff_t>(parsed.declared_length) + delta));
            added = true;
        }
    } else {
        const auto top_end = std::find_if(
            parsed.top_level_tags.begin(), parsed.top_level_tags.end(),
            [](const TagView& tag) { return tag.code == kTagEnd; });
        if (top_end == parsed.top_level_tags.end()) {
            error = "top-level GFX stream has no End tag";
            return false;
        }
        output.insert(
            output.begin() + static_cast<std::ptrdiff_t>(top_end->tag_start),
            cloned.begin(), cloned.end());
        write_u32(output, 4, parsed.declared_length +
            static_cast<std::uint32_t>(cloned.size()));
        added = true;
    }

    // Reparse because inserting or upgrading the wrapper shifts later tags.
    if (!parse_gfx(output, parsed, code, error)) return false;
    button = find_sprite(parsed, kControllerButtonSprite);
    if (!button) {
        error = "Controller button sprite disappeared after wrapper insertion";
        return false;
    }
    const bool has_text_1 = std::any_of(
        button->placements.begin(), button->placements.end(),
        [](const Placement& placement) {
            return placement.has_name && placement.name == "Text_1";
        });
    if (has_text_1) return true;

    const std::optional<std::size_t> insertion = show_frame_offset(*button);
    if (!insertion) {
        error = "Controller button sprite has no ShowFrame tag";
        return false;
    }
    if (std::any_of(button->placements.begin(), button->placements.end(),
            [](const Placement& placement) { return placement.depth == 12; })) {
        error = "Controller button depth 12 is already occupied";
        return false;
    }

    const std::vector<std::uint8_t> label = make_named_placement(
        kControllerButtonLabelWrapper, 12, -9520, 0, "Text_1");
    if (!patch_tag_body_length(output, button->tag, label.size(), error)) {
        return false;
    }
    output.insert(
        output.begin() + static_cast<std::ptrdiff_t>(*insertion),
        label.begin(), label.end());
    write_u32(output, 4, parsed.declared_length +
        static_cast<std::uint32_t>(label.size()));
    added = true;
    return true;
}

} // namespace

Inspection inspect_controller_panel(std::span<const std::uint8_t> input) noexcept {
    try {
        ParsedGfx parsed{};
        ErrorCode code{};
        std::string error{};
        if (!parse_gfx(input, parsed, code, error)) {
            Inspection inspection{};
            inspection.error = code;
            inspection.message = std::move(error);
            return inspection;
        }
        ControllerContext context{};
        if (!locate_controller(parsed, context, code, error)) {
            Inspection inspection{};
            inspection.error = code;
            inspection.message = std::move(error);
            return inspection;
        }
        return make_inspection(parsed, context);
    } catch (...) {
        Inspection inspection{};
        inspection.error = ErrorCode::invalid_gfx;
        inspection.message = "unexpected exception while parsing the GFX";
        return inspection;
    }
}

PatchResult patch_controller_panel(
    std::span<const std::uint8_t> input,
    PatchOptions options) noexcept {
    try {
        if (options.controller_rows < kMinimumControllerRows ||
            options.controller_rows > kMaximumControllerRows) {
            return fail(
                ErrorCode::invalid_argument,
                "controller row target must be between 6 and 13");
        }

        ParsedGfx parsed{};
        ControllerContext context{};
        ErrorCode code{};
        std::string error{};
        if (!parse_gfx(input, parsed, code, error)) {
            return fail(code, std::move(error));
        }
        if (!locate_controller(parsed, context, code, error)) {
            return fail(code, std::move(error));
        }

        PatchResult result{};
        result.report.before = make_inspection(parsed, context);
        if (context.row_count > options.controller_rows) {
            return fail(
                ErrorCode::invalid_argument,
                "refusing to remove existing ControllSetting rows");
        }
        if (context.row_count == options.controller_rows) {
            result.output.assign(input.begin(), input.end());
            bool label_added = false;
            if (!ensure_controller_button_label(
                    result.output, error, label_added)) {
                return fail(ErrorCode::unsupported_gfx, std::move(error));
            }
            result.report.after = inspect_controller_panel(result.output);
            result.report.bytes_added = result.output.size() - input.size();
            result.report.already_satisfied = !label_added;
            result.message = label_added
                ? "Controller button label field added successfully"
                : "Controller panel already satisfies the requested patch";
            return result;
        }

        const std::optional<std::size_t> insertion = show_frame_offset(*context.controller);
        if (!insertion) {
            return fail(ErrorCode::unsupported_gfx, "ControllSetting has no ShowFrame tag");
        }

        std::vector<std::uint16_t> occupied_depths{};
        occupied_depths.reserve(context.controller->placements.size());
        for (const Placement& placement : context.controller->placements) {
            occupied_depths.push_back(placement.depth);
        }

        std::vector<std::uint8_t> payload{};
        for (std::uint16_t index = context.row_count;
             index < options.controller_rows;
             ++index) {
            const std::uint16_t depth = static_cast<std::uint16_t>(index * 2 + 1);
            if (std::find(occupied_depths.begin(), occupied_depths.end(), depth) !=
                occupied_depths.end()) {
                return fail(
                    ErrorCode::unsupported_gfx,
                    "ControllSetting depth " + std::to_string(depth) +
                        " is already occupied by another placement");
            }

            const Placement* donor = find_donor(parsed, context, index);
            if (donor == nullptr) {
                std::vector<std::uint8_t> generated = make_controller_placement(
                    context.item_character_id, index, depth);
                payload.insert(payload.end(), generated.begin(), generated.end());
                occupied_depths.push_back(depth);
                continue;
            }
            const std::size_t raw_begin = donor->tag.tag_start;
            const std::size_t raw_end = donor->tag.body_end;
            if (raw_end > parsed.declared_length || raw_begin >= raw_end) {
                return fail(ErrorCode::invalid_gfx, "donor placement range is invalid");
            }
            const std::size_t payload_begin = payload.size();
            payload.insert(
                payload.end(),
                input.begin() + static_cast<std::ptrdiff_t>(raw_begin),
                input.begin() + static_cast<std::ptrdiff_t>(raw_end));

            const std::size_t relative_body = donor->tag.body_start - donor->tag.tag_start;
            const std::size_t relative_depth = relative_body + 1;
            if (payload_begin + relative_depth + 2 > payload.size()) {
                return fail(ErrorCode::invalid_gfx, "donor placement depth is invalid");
            }
            write_u16(payload, payload_begin + relative_depth, depth);
            occupied_depths.push_back(depth);
        }

        if (payload.empty() ||
            payload.size() > std::numeric_limits<std::uint32_t>::max() - parsed.declared_length) {
            return fail(ErrorCode::unsupported_gfx, "patched GFX length would overflow");
        }

        result.output.assign(input.begin(), input.end());
        if (!patch_tag_body_length(
                result.output,
                context.controller->tag,
                payload.size(),
                error)) {
            return fail(ErrorCode::unsupported_gfx, std::move(error));
        }
        result.output.insert(
            result.output.begin() + static_cast<std::ptrdiff_t>(*insertion),
            payload.begin(),
            payload.end());
        write_u32(
            result.output,
            4,
            parsed.declared_length + static_cast<std::uint32_t>(payload.size()));

        bool label_added = false;
        if (!ensure_controller_button_label(
                result.output, error, label_added)) {
            return fail(ErrorCode::unsupported_gfx, std::move(error));
        }
        result.report.bytes_added = result.output.size() - input.size();
        result.report.after = inspect_controller_panel(result.output);
        if (!result.report.after.success() ||
            result.report.after.controller_rows != options.controller_rows) {
            return fail(
                ErrorCode::output_verification_failed,
                result.report.after.message.empty()
                    ? "patched output did not expose the requested controller rows"
                    : result.report.after.message);
        }
        result.message = "ControllSetting expanded successfully";
        return result;
    } catch (...) {
        return fail(ErrorCode::invalid_gfx, "unexpected exception while patching the GFX");
    }
}

const char* error_name(ErrorCode error) noexcept {
    switch (error) {
    case ErrorCode::none:
        return "none";
    case ErrorCode::invalid_argument:
        return "invalid_argument";
    case ErrorCode::invalid_gfx:
        return "invalid_gfx";
    case ErrorCode::unsupported_gfx:
        return "unsupported_gfx";
    case ErrorCode::controller_panel_not_found:
        return "controller_panel_not_found";
    case ErrorCode::controller_items_invalid:
        return "controller_items_invalid";
    case ErrorCode::donor_item_not_found:
        return "donor_item_not_found";
    case ErrorCode::output_verification_failed:
        return "output_verification_failed";
    }
    return "unknown";
}

} // namespace erui::gfx
