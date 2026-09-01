#include "gfx_patch.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
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
constexpr std::uint16_t kTagDefineEditText = 37;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kTagPlaceObject3 = 70;
constexpr std::uint16_t kTagDefineScalingGrid = 78;
constexpr std::uint16_t kTagDefineExternalImage2 = 1009;
constexpr std::uint16_t kControllerButtonSprite = 97;
constexpr std::uint16_t kControllerCaptionWrapper = 101;
constexpr std::uint16_t kControllerButtonLabelWrapper = 193;
constexpr std::uint16_t kMinimumControllerRows = 6;
constexpr std::uint16_t kMaximumControllerRows = 13;
constexpr std::string_view kControllerPanelName = "ControllSetting";
constexpr std::string_view kCharacterFrameName = "CharacterFrame";
constexpr std::string_view kValueTextName = "Text_0";
constexpr std::string_view kEmptyTextName = "TextOnEmpty";
constexpr std::string_view kCaptionName = "Caption";
constexpr std::string_view kCursorName = "Cursor";
constexpr std::string_view kCharacterFrameExport = "MENU_FL_Cursor_EntWaku";
constexpr std::string_view kCharacterFrameFile = "MENU_FL_Cursor_EntWaku.tga";

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

struct TextInputContext {
    const Sprite* sprite{};
    const Placement* value{};
    const Placement* empty{};
    const Placement* caption{};
    const Placement* cursor{};
    GfxHost host{GfxHost::unknown};
};

[[nodiscard]] bool has_character_name_text_input(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed) noexcept;

[[nodiscard]] bool locate_text_input(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    TextInputContext& context,
    ErrorCode& code,
    std::string& error) noexcept;

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
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    const ControllerContext& context) {
    Inspection result{};
    result.gfx_version = parsed.version;
    result.declared_length = parsed.declared_length;
    result.window_list_sprite = context.window_list->id;
    result.controller_sprite = context.controller->id;
    result.controller_item_character = context.item_character_id;
    result.controller_rows = context.row_count;
    TextInputContext text_input{};
    ErrorCode ignored_code{};
    std::string ignored_error{};
    if (locate_text_input(
            bytes, parsed, text_input, ignored_code, ignored_error)) {
        result.host = text_input.host;
        result.text_input_sprite = text_input.sprite->id;
    }
    // Presentation inspection is intentionally exact. A partial or modified
    // CharacterFrame must not be reported as the supported patch.
    result.character_name_text_input = has_character_name_text_input(bytes, parsed);
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

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
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

    void write_signed(std::int32_t value, std::uint32_t count) {
        const std::uint32_t mask = count == 32
            ? std::numeric_limits<std::uint32_t>::max()
            : ((1U << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() noexcept { bit_offset_ = 0; }

private:
    std::vector<std::uint8_t>& output_;
    std::uint8_t bit_offset_{};
};

[[nodiscard]] std::uint32_t signed_bit_count(std::int32_t value) noexcept {
    for (std::uint32_t bits = 1; bits < 32; ++bits) {
        const std::int64_t minimum = -(std::int64_t{1} << (bits - 1));
        const std::int64_t maximum = (std::int64_t{1} << (bits - 1)) - 1;
        if (value >= minimum && value <= maximum) return bits;
    }
    return 32;
}

void append_matrix(
    std::vector<std::uint8_t>& output,
    std::int32_t x,
    std::int32_t y,
    std::optional<std::int32_t> scale_x = std::nullopt,
    std::optional<std::int32_t> scale_y = std::nullopt) {
    BitWriter writer(output);
    const bool has_scale = scale_x.has_value() && scale_y.has_value();
    writer.write(has_scale ? 1U : 0U, 1);
    if (has_scale) {
        const std::uint32_t bits = std::max(
            signed_bit_count(*scale_x), signed_bit_count(*scale_y));
        writer.write(bits, 5);
        writer.write_signed(*scale_x, bits);
        writer.write_signed(*scale_y, bits);
    }
    writer.write(0, 1); // no rotate/skew
    const std::uint32_t translation_bits = std::max(
        signed_bit_count(x), signed_bit_count(y));
    writer.write(translation_bits, 5);
    writer.write_signed(x, translation_bits);
    writer.write_signed(y, translation_bits);
    writer.align();
}

void append_character_frame_color_transform(std::vector<std::uint8_t>& output) {
    BitWriter writer(output);
    constexpr std::uint32_t bits = 10;
    writer.write(0, 1); // no additive terms
    writer.write(1, 1); // multiplicative terms follow
    writer.write(bits, 4);
    writer.write_signed(225, bits); // red
    writer.write_signed(225, bits); // green
    writer.write_signed(225, bits); // blue
    writer.write_signed(256, bits); // alpha
    writer.align();
}

[[nodiscard]] std::vector<std::uint8_t> make_tag(
    std::uint16_t code,
    std::span<const std::uint8_t> body,
    bool force_long = false) {
    std::vector<std::uint8_t> output{};
    if (!force_long && body.size() < 0x3FU) {
        append_u16(output, static_cast<std::uint16_t>((code << 6U) | body.size()));
    } else {
        append_u16(output, static_cast<std::uint16_t>((code << 6U) | 0x3FU));
        append_u32(output, static_cast<std::uint32_t>(body.size()));
    }
    output.insert(output.end(), body.begin(), body.end());
    return output;
}

[[nodiscard]] std::vector<std::uint8_t> make_character_frame_external_image(
    std::uint16_t character_id) {
    std::vector<std::uint8_t> body{};
    append_u16(body, character_id);
    append_u16(body, 0); // native DefineExternalImage2 id type
    body.push_back(13); // native bitmap format
    body.push_back(0);
    append_u16(body, 428);
    append_u16(body, 108);
    body.push_back(static_cast<std::uint8_t>(kCharacterFrameExport.size()));
    body.insert(body.end(), kCharacterFrameExport.begin(), kCharacterFrameExport.end());
    body.push_back(static_cast<std::uint8_t>(kCharacterFrameFile.size()));
    body.insert(body.end(), kCharacterFrameFile.begin(), kCharacterFrameFile.end());
    return make_tag(kTagDefineExternalImage2, body);
}

[[nodiscard]] std::vector<std::uint8_t> make_character_frame_wrapper(
    std::uint16_t wrapper_id,
    std::uint16_t image_id) {
    std::vector<std::uint8_t> image_body{};
    image_body.push_back(0x06U); // character + matrix
    image_body.push_back(0x10U); // PlaceObject3 image flag
    append_u16(image_body, 1);
    append_u16(image_body, image_id);
    append_matrix(image_body, -4193, -1080);

    std::vector<std::uint8_t> body{};
    append_u16(body, wrapper_id);
    append_u16(body, 1);
    const std::vector<std::uint8_t> image = make_tag(
        kTagPlaceObject3, image_body, true);
    body.insert(body.end(), image.begin(), image.end());
    const std::vector<std::uint8_t> show_frame = make_tag(kTagShowFrame, {});
    body.insert(body.end(), show_frame.begin(), show_frame.end());
    const std::vector<std::uint8_t> end = make_tag(kTagEnd, {});
    body.insert(body.end(), end.begin(), end.end());
    return make_tag(kTagDefineSprite, body, true);
}

[[nodiscard]] std::vector<std::uint8_t> make_character_frame_scaling_grid(
    std::uint16_t wrapper_id) {
    std::vector<std::uint8_t> body{};
    append_u16(body, wrapper_id);
    BitWriter rectangle(body);
    constexpr std::uint32_t bits = 9;
    rectangle.write(bits, 5);
    rectangle.write_signed(-80, bits);
    rectangle.write_signed(80, bits);
    rectangle.write_signed(-160, bits);
    rectangle.write_signed(180, bits);
    rectangle.align();
    return make_tag(kTagDefineScalingGrid, body);
}

[[nodiscard]] std::vector<std::uint8_t> make_character_frame_placement(
    std::uint16_t wrapper_id) {
    constexpr std::uint8_t flags = 0x2EU; // character, matrix, color, name
    std::vector<std::uint8_t> body{};
    body.push_back(flags);
    append_u16(body, 2);
    append_u16(body, wrapper_id);
    append_matrix(body, -487, -320, 65536, 31457);
    append_character_frame_color_transform(body);
    body.insert(body.end(), kCharacterFrameName.begin(), kCharacterFrameName.end());
    body.push_back(0);
    return make_tag(kTagPlaceObject2, body);
}

struct ExternalImage {
    TagView tag{};
    std::uint16_t character_id{};
    std::uint16_t id_type{};
    std::uint8_t bitmap_format{};
    std::uint8_t reserved{};
    std::uint16_t width{};
    std::uint16_t height{};
    std::string export_name{};
    std::string file_name{};
};

[[nodiscard]] bool read_length_string(
    std::span<const std::uint8_t> bytes,
    std::size_t& offset,
    std::size_t end,
    std::string& value) {
    if (offset >= end) return false;
    const std::size_t length = bytes[offset++];
    if (length > end - offset) return false;
    value.assign(
        reinterpret_cast<const char*>(bytes.data() + offset), length);
    offset += length;
    return true;
}

[[nodiscard]] bool parse_external_image(
    std::span<const std::uint8_t> bytes,
    const TagView& tag,
    ExternalImage& image) {
    if (tag.code != kTagDefineExternalImage2 ||
        tag.body_start + 10 > tag.body_end) {
        return false;
    }
    image = {};
    image.tag = tag;
    std::size_t offset = tag.body_start;
    if (!read_u16(bytes, offset, image.character_id) ||
        !read_u16(bytes, offset + 2, image.id_type) ||
        !read_u16(bytes, offset + 6, image.width) ||
        !read_u16(bytes, offset + 8, image.height)) {
        return false;
    }
    image.bitmap_format = bytes[offset + 4];
    image.reserved = bytes[offset + 5];
    offset += 10;
    return read_length_string(bytes, offset, tag.body_end, image.export_name) &&
        read_length_string(bytes, offset, tag.body_end, image.file_name) &&
        offset == tag.body_end;
}

[[nodiscard]] bool tag_equals(
    std::span<const std::uint8_t> bytes,
    const TagView& tag,
    std::span<const std::uint8_t> expected) noexcept {
    if (tag.tag_start > tag.body_end || tag.body_end > bytes.size() ||
        tag.body_end - tag.tag_start != expected.size()) {
        return false;
    }
    return std::equal(
        bytes.begin() + static_cast<std::ptrdiff_t>(tag.tag_start),
        bytes.begin() + static_cast<std::ptrdiff_t>(tag.body_end),
        expected.begin());
}

[[nodiscard]] bool is_character_definition(std::uint16_t code) noexcept {
    switch (code) {
    case 2:  // DefineShape
    case 6:  // DefineBits
    case 7:  // DefineButton
    case 10: // DefineFont
    case 11: // DefineText
    case 14: // DefineSound
    case 20: // DefineBitsLossless
    case 21: // DefineBitsJPEG2
    case 22: // DefineShape2
    case 32: // DefineShape3
    case 33: // DefineText2
    case 34: // DefineButton2
    case 35: // DefineBitsJPEG3
    case 36: // DefineBitsLossless2
    case kTagDefineEditText:
    case kTagDefineSprite:
    case 46: // DefineMorphShape
    case 48: // DefineFont2
    case 60: // DefineVideoStream
    case 75: // DefineFont3
    case 83: // DefineShape4
    case 84: // DefineMorphShape2
    case 87: // DefineBinaryData
    case 90: // DefineBitsJPEG4
    case 91: // DefineFont4
    case kTagDefineExternalImage2:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::optional<std::uint16_t> next_character_id(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    std::uint16_t minimum = 1) noexcept {
    std::uint16_t maximum = static_cast<std::uint16_t>(minimum - 1U);
    for (const TagView& tag : parsed.top_level_tags) {
        if (!is_character_definition(tag.code)) continue;
        std::uint16_t character_id{};
        if (!read_u16(bytes, tag.body_start, character_id)) return std::nullopt;
        maximum = std::max(maximum, character_id);
    }
    if (maximum == std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(maximum + 1U);
}

[[nodiscard]] const TagView* find_character_definition(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    std::uint16_t code,
    std::uint16_t character_id) noexcept {
    const TagView* result{};
    for (const TagView& tag : parsed.top_level_tags) {
        if (tag.code != code) continue;
        std::uint16_t candidate{};
        if (!read_u16(bytes, tag.body_start, candidate) ||
            candidate != character_id) {
            continue;
        }
        if (result != nullptr) return nullptr;
        result = &tag;
    }
    return result;
}

[[nodiscard]] bool find_character_frame_image(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    std::optional<std::uint16_t>& character_id,
    std::string* error = nullptr) {
    character_id.reset();
    for (const TagView& tag : parsed.top_level_tags) {
        if (tag.code != kTagDefineExternalImage2) continue;
        ExternalImage image{};
        if (!parse_external_image(bytes, tag, image)) {
            if (error) *error = "malformed DefineExternalImage2 tag";
            return false;
        }
        const bool related = image.export_name == kCharacterFrameExport ||
            image.file_name == kCharacterFrameFile;
        if (!related) continue;
        if (image.export_name != kCharacterFrameExport ||
            image.file_name != kCharacterFrameFile || image.id_type != 0 ||
            image.bitmap_format != 13 || image.reserved != 0 ||
            image.width != 428 || image.height != 108 || character_id) {
            if (error) {
                *error = "conflicting MENU_FL_Cursor_EntWaku external image definition";
            }
            return false;
        }
        character_id = image.character_id;
    }
    return true;
}

struct SequenceMatch {
    std::size_t count{};
    std::size_t offset{};
};

[[nodiscard]] SequenceMatch find_sequence(
    std::span<const std::uint8_t> bytes,
    std::span<const std::uint8_t> sequence) noexcept {
    SequenceMatch result{};
    auto begin = bytes.begin();
    while (begin != bytes.end()) {
        const auto found = std::search(begin, bytes.end(), sequence.begin(), sequence.end());
        if (found == bytes.end()) break;
        ++result.count;
        result.offset = static_cast<std::size_t>(std::distance(bytes.begin(), found));
        begin = std::next(found);
    }
    return result;
}

enum class PlaceholderColor : std::uint8_t {
    invalid,
    gray,
    red,
};

[[nodiscard]] PlaceholderColor placeholder_color(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    std::uint16_t character_id,
    const TagView** definition = nullptr) noexcept {
    const TagView* tag = find_character_definition(
        bytes, parsed, kTagDefineEditText, character_id);
    if (!tag) return PlaceholderColor::invalid;
    if (definition) *definition = tag;

    constexpr std::array<std::uint8_t, 4> gray_rgba{0x50, 0x50, 0x50, 0xFF};
    constexpr std::array<std::uint8_t, 4> red_rgba{0xFF, 0x00, 0x00, 0xFF};
    constexpr std::array<std::uint8_t, 7> gray_html{'#', '5', '0', '5', '0', '5', '0'};
    constexpr std::array<std::uint8_t, 7> red_html{'#', 'F', 'F', '0', '0', '0', '0'};
    const std::span<const std::uint8_t> body = bytes.subspan(
        tag->body_start, tag->body_end - tag->body_start);
    const SequenceMatch gray_color = find_sequence(body, gray_rgba);
    const SequenceMatch red_color = find_sequence(body, red_rgba);
    const SequenceMatch gray_markup = find_sequence(body, gray_html);
    const SequenceMatch red_markup = find_sequence(body, red_html);
    if (gray_color.count == 1 && gray_markup.count == 1 &&
        red_color.count == 0 && red_markup.count == 0) {
        return PlaceholderColor::gray;
    }
    if (red_color.count == 1 && red_markup.count == 1 &&
        gray_color.count == 0 && gray_markup.count == 0) {
        return PlaceholderColor::red;
    }
    return PlaceholderColor::invalid;
}

[[nodiscard]] const Placement* find_unique_placement(
    const Sprite& sprite,
    std::string_view name) noexcept {
    const Placement* result{};
    for (const Placement& placement : sprite.placements) {
        if (!placement.has_name || placement.name != name) continue;
        if (result != nullptr) return nullptr;
        result = &placement;
    }
    return result;
}

[[nodiscard]] std::size_t placement_name_count(
    const Sprite& sprite,
    std::string_view name) noexcept {
    return static_cast<std::size_t>(std::count_if(
        sprite.placements.begin(), sprite.placements.end(),
        [name](const Placement& placement) {
            return placement.has_name && placement.name == name;
        }));
}

[[nodiscard]] bool matches_named_text_input_child(
    const Placement& placement,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y) noexcept {
    return placement.flags == 0x26U && placement.has_character &&
        placement.has_matrix && placement.has_name &&
        placement.depth == depth && placement.translation.x == x &&
        placement.translation.y == y;
}

[[nodiscard]] bool locate_text_input(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    TextInputContext& context,
    ErrorCode& code,
    std::string& error) noexcept {
    context = {};
    std::size_t signature_count{};

    for (const Sprite& sprite : parsed.sprites) {
        const std::size_t value_count = placement_name_count(sprite, kValueTextName);
        const std::size_t empty_count = placement_name_count(sprite, kEmptyTextName);
        const std::size_t caption_count = placement_name_count(sprite, kCaptionName);
        const std::size_t cursor_count = placement_name_count(sprite, kCursorName);
        if (value_count == 0 || empty_count == 0 ||
            caption_count == 0 || cursor_count == 0) {
            continue;
        }
        ++signature_count;
        if (signature_count != 1) {
            code = ErrorCode::text_input_invalid;
            error = "multiple sprites expose the native TextInput child signature";
            return false;
        }
        if (value_count != 1 || empty_count != 1 ||
            caption_count != 1 || cursor_count != 1) {
            code = ErrorCode::text_input_invalid;
            error = "TextInput named child placements are not unique";
            return false;
        }

        const Placement* value = find_unique_placement(sprite, kValueTextName);
        const Placement* empty = find_unique_placement(sprite, kEmptyTextName);
        const Placement* caption = find_unique_placement(sprite, kCaptionName);
        const Placement* cursor = find_unique_placement(sprite, kCursorName);
        if (!value || !empty || !caption || !cursor || sprite.frame_count != 1 ||
            !matches_named_text_input_child(*value, 3, -4360, -640) ||
            !matches_named_text_input_child(*empty, 4, -4360, -640) ||
            !matches_named_text_input_child(*cursor, 7, 0, 0) ||
            caption->flags != 0x26U || !caption->has_character ||
            !caption->has_matrix || !caption->has_name || caption->depth != 5 ||
            caption->translation.y != 0 ||
            (caption->translation.x != -9520 && caption->translation.x != -9120)) {
            code = ErrorCode::text_input_invalid;
            error = "TextInput child depth or transform contract is not recognized";
            return false;
        }
        if (static_cast<std::size_t>(std::count_if(
                sprite.tags.begin(), sprite.tags.end(),
                [](const TagView& tag) { return tag.code == kTagShowFrame; })) != 1) {
            code = ErrorCode::text_input_invalid;
            error = "TextInput sprite does not contain exactly one ShowFrame tag";
            return false;
        }
        const PlaceholderColor color = placeholder_color(
            bytes, parsed, empty->character_id);
        if (color != PlaceholderColor::gray && color != PlaceholderColor::red) {
            code = ErrorCode::text_input_invalid;
            error = "TextInput TextOnEmpty field is not the supported definition";
            return false;
        }

        context.sprite = &sprite;
        context.value = value;
        context.empty = empty;
        context.caption = caption;
        context.cursor = cursor;
        context.host = caption->translation.x == -9520
            ? GfxHost::controller_settings
            : GfxHost::advanced_settings;
    }

    if (signature_count == 0 || context.sprite == nullptr) {
        code = ErrorCode::text_input_not_found;
        error = "native TextInput sprite was not found";
        return false;
    }
    return true;
}

[[nodiscard]] Inspection make_text_input_inspection(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed,
    const TextInputContext& context) {
    Inspection result{};
    result.gfx_version = parsed.version;
    result.declared_length = parsed.declared_length;
    result.host = context.host;
    result.text_input_sprite = context.sprite->id;
    result.character_name_text_input =
        has_character_name_text_input(bytes, parsed);
    return result;
}

[[nodiscard]] bool character_name_structure_matches(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed) noexcept {
    TextInputContext context{};
    ErrorCode code{};
    std::string error{};
    if (!locate_text_input(bytes, parsed, context, code, error)) return false;
    const Sprite* text_input = context.sprite;
    const Placement* frame = find_unique_placement(*text_input, kCharacterFrameName);
    const Placement* empty = find_unique_placement(*text_input, kEmptyTextName);
    if (!frame || !empty || !frame->has_character || !empty->has_character ||
        !tag_equals(bytes, frame->tag,
            make_character_frame_placement(frame->character_id))) {
        return false;
    }

    std::optional<std::uint16_t> image_id{};
    if (!find_character_frame_image(bytes, parsed, image_id) || !image_id) {
        return false;
    }
    const TagView* external = find_character_definition(
        bytes, parsed, kTagDefineExternalImage2, *image_id);
    const TagView* wrapper = find_character_definition(
        bytes, parsed, kTagDefineSprite, frame->character_id);
    const TagView* grid = find_character_definition(
        bytes, parsed, kTagDefineScalingGrid, frame->character_id);
    if (!external || !wrapper || !grid ||
        external->tag_start >= wrapper->tag_start ||
        wrapper->tag_start >= text_input->tag.tag_start ||
        grid->tag_start >= text_input->tag.tag_start ||
        !tag_equals(bytes, *external, make_character_frame_external_image(*image_id)) ||
        !tag_equals(bytes, *wrapper,
            make_character_frame_wrapper(frame->character_id, *image_id)) ||
        !tag_equals(bytes, *grid,
            make_character_frame_scaling_grid(frame->character_id))) {
        return false;
    }
    return placeholder_color(bytes, parsed, empty->character_id) == PlaceholderColor::red;
}

[[nodiscard]] bool has_character_name_text_input(
    std::span<const std::uint8_t> bytes,
    const ParsedGfx& parsed) noexcept {
    return !bytes.empty() && character_name_structure_matches(bytes, parsed);
}

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

[[nodiscard]] bool patch_placeholder_red(
    std::vector<std::uint8_t>& output,
    const ParsedGfx& parsed,
    std::string& error) {
    TextInputContext context{};
    ErrorCode code{};
    if (!locate_text_input(output, parsed, context, code, error)) {
        return false;
    }
    const Placement* empty = context.empty;
    const TagView* definition{};
    const PlaceholderColor color = placeholder_color(
        output, parsed, empty->character_id, &definition);
    if (color == PlaceholderColor::red) return true;
    if (color != PlaceholderColor::gray || !definition) {
        error = "Controller TextOnEmpty color definition is not recognized";
        return false;
    }

    constexpr std::array<std::uint8_t, 4> gray_rgba{0x50, 0x50, 0x50, 0xFF};
    constexpr std::array<std::uint8_t, 4> red_rgba{0xFF, 0x00, 0x00, 0xFF};
    constexpr std::array<std::uint8_t, 7> gray_html{'#', '5', '0', '5', '0', '5', '0'};
    constexpr std::array<std::uint8_t, 7> red_html{'#', 'F', 'F', '0', '0', '0', '0'};
    const std::span<const std::uint8_t> body{
        output.data() + definition->body_start,
        definition->body_end - definition->body_start};
    const SequenceMatch rgba = find_sequence(body, gray_rgba);
    const SequenceMatch html = find_sequence(body, gray_html);
    if (rgba.count != 1 || html.count != 1) {
        error = "TextInput TextOnEmpty gray color is not unique";
        return false;
    }
    std::copy(red_rgba.begin(), red_rgba.end(),
        output.begin() + static_cast<std::ptrdiff_t>(definition->body_start + rgba.offset));
    std::copy(red_html.begin(), red_html.end(),
        output.begin() + static_cast<std::ptrdiff_t>(definition->body_start + html.offset));
    return true;
}

[[nodiscard]] bool ensure_character_name_text_input(
    std::vector<std::uint8_t>& output,
    std::string& error,
    bool& added) {
    added = false;
    ParsedGfx parsed{};
    ErrorCode code{};
    if (!parse_gfx(output, parsed, code, error)) return false;
    if (character_name_structure_matches(output, parsed)) return true;

    TextInputContext context{};
    if (!locate_text_input(output, parsed, context, code, error)) {
        return false;
    }
    const Sprite* text_input = context.sprite;
    if (find_unique_placement(*text_input, kCharacterFrameName) != nullptr) {
        error = "existing CharacterFrame does not match the supported presentation";
        return false;
    }
    if (std::any_of(
            text_input->placements.begin(), text_input->placements.end(),
            [](const Placement& placement) { return placement.depth == 2; })) {
        error = "TextInput depth 2 is already occupied";
        return false;
    }
    const Placement* empty = find_unique_placement(*text_input, kEmptyTextName);
    if (!empty || !empty->has_character ||
        placeholder_color(output, parsed, empty->character_id) != PlaceholderColor::gray) {
        error = "TextInput TextOnEmpty field is not the supported native definition";
        return false;
    }

    std::optional<std::uint16_t> image_id{};
    if (!find_character_frame_image(output, parsed, image_id, &error)) return false;
    const std::optional<std::uint16_t> first_free = next_character_id(output, parsed);
    if (!first_free) {
        error = "no free GFX character ID remains for CharacterFrame";
        return false;
    }
    std::uint16_t wrapper_id{};
    std::vector<std::uint8_t> definitions{};
    if (image_id) {
        wrapper_id = *first_free;
    } else {
        if (*first_free == std::numeric_limits<std::uint16_t>::max()) {
            error = "no two free GFX character IDs remain for CharacterFrame";
            return false;
        }
        image_id = *first_free;
        wrapper_id = static_cast<std::uint16_t>(*first_free + 1U);
        const std::vector<std::uint8_t> external =
            make_character_frame_external_image(*image_id);
        definitions.insert(definitions.end(), external.begin(), external.end());
    }
    const std::vector<std::uint8_t> wrapper =
        make_character_frame_wrapper(wrapper_id, *image_id);
    const std::vector<std::uint8_t> grid =
        make_character_frame_scaling_grid(wrapper_id);
    definitions.insert(definitions.end(), wrapper.begin(), wrapper.end());
    definitions.insert(definitions.end(), grid.begin(), grid.end());

    // SWF/GFX character definitions must precede their first control-tag use.
    // FFDec accepts a forward reference, but the game's Scaleform loader can
    // silently discard it. Insert this dependency closure immediately before
    // the TextInput sprite, whose depth-2 placement references the wrapper.
    const std::size_t definitions_insertion = text_input->tag.tag_start;
    if (definitions_insertion < parsed.top_level_start ||
        definitions_insertion > parsed.declared_length ||
        definitions.size() > std::numeric_limits<std::uint32_t>::max() -
            parsed.declared_length) {
        error = "could not insert CharacterFrame definitions before TextInput";
        return false;
    }
    output.insert(
        output.begin() + static_cast<std::ptrdiff_t>(definitions_insertion),
        definitions.begin(), definitions.end());
    write_u32(output, 4, parsed.declared_length +
        static_cast<std::uint32_t>(definitions.size()));

    if (!parse_gfx(output, parsed, code, error)) return false;
    if (!locate_text_input(output, parsed, context, code, error)) {
        error = "TextInput disappeared after CharacterFrame definitions: " + error;
        return false;
    }
    text_input = context.sprite;
    const std::optional<std::size_t> insertion = show_frame_offset(*text_input);
    if (!insertion) {
        error = "TextInput sprite has no ShowFrame tag";
        return false;
    }
    const std::vector<std::uint8_t> placement =
        make_character_frame_placement(wrapper_id);
    if (!patch_tag_body_length(output, text_input->tag, placement.size(), error)) {
        return false;
    }
    output.insert(
        output.begin() + static_cast<std::ptrdiff_t>(*insertion),
        placement.begin(), placement.end());
    write_u32(output, 4, parsed.declared_length +
        static_cast<std::uint32_t>(placement.size()));

    if (!parse_gfx(output, parsed, code, error) ||
        !patch_placeholder_red(output, parsed, error) ||
        !parse_gfx(output, parsed, code, error)) {
        return false;
    }
    if (!character_name_structure_matches(output, parsed)) {
        error = "completed CharacterFrame presentation did not verify";
        return false;
    }
    added = true;
    return true;
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

Inspection inspect_text_input_host(
    std::span<const std::uint8_t> input) noexcept {
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
        TextInputContext context{};
        if (!locate_text_input(input, parsed, context, code, error)) {
            Inspection inspection{};
            inspection.error = code;
            inspection.message = std::move(error);
            return inspection;
        }
        return make_text_input_inspection(input, parsed, context);
    } catch (...) {
        Inspection inspection{};
        inspection.error = ErrorCode::invalid_gfx;
        inspection.message = "unexpected exception while parsing the GFX";
        return inspection;
    }
}

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
        return make_inspection(input, parsed, context);
    } catch (...) {
        Inspection inspection{};
        inspection.error = ErrorCode::invalid_gfx;
        inspection.message = "unexpected exception while parsing the GFX";
        return inspection;
    }
}

PatchResult patch_text_input_presentation(
    std::span<const std::uint8_t> input,
    TextInputPresentation presentation) noexcept {
    try {
        if (presentation != TextInputPresentation::native &&
            presentation != TextInputPresentation::character_name) {
            return fail(
                ErrorCode::invalid_argument,
                "unknown TextInput presentation");
        }

        PatchResult result{};
        result.report.before = inspect_text_input_host(input);
        if (!result.report.before.success()) {
            return fail(
                result.report.before.error,
                result.report.before.message);
        }
        result.output.assign(input.begin(), input.end());

        bool presentation_added = false;
        std::string error{};
        if (presentation == TextInputPresentation::character_name &&
            !ensure_character_name_text_input(
                result.output, error, presentation_added)) {
            return fail(ErrorCode::unsupported_gfx, std::move(error));
        }

        result.report.after = inspect_text_input_host(result.output);
        if (!result.report.after.success() ||
            result.report.after.host != result.report.before.host ||
            result.report.after.text_input_sprite !=
                result.report.before.text_input_sprite ||
            (presentation == TextInputPresentation::character_name &&
                !result.report.after.character_name_text_input)) {
            return fail(
                ErrorCode::output_verification_failed,
                result.report.after.message.empty()
                    ? "patched TextInput presentation did not verify"
                    : result.report.after.message);
        }
        result.report.bytes_added = result.output.size() - input.size();
        result.report.already_satisfied = !presentation_added;
        result.message = presentation_added
            ? "Character-name TextInput presentation added successfully"
            : "TextInput presentation already satisfies the requested patch";
        return result;
    } catch (...) {
        return fail(
            ErrorCode::invalid_gfx,
            "unexpected exception while patching the GFX");
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
        result.report.before = make_inspection(input, parsed, context);
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
            bool presentation_added = false;
            if (options.text_input_presentation ==
                    TextInputPresentation::character_name &&
                !ensure_character_name_text_input(
                    result.output, error, presentation_added)) {
                return fail(ErrorCode::unsupported_gfx, std::move(error));
            }
            result.report.after = inspect_controller_panel(result.output);
            if (!result.report.after.success() ||
                (options.text_input_presentation ==
                        TextInputPresentation::character_name &&
                    !result.report.after.character_name_text_input)) {
                return fail(
                    ErrorCode::output_verification_failed,
                    result.report.after.message.empty()
                        ? "patched TextInput presentation did not verify"
                        : result.report.after.message);
            }
            result.report.bytes_added = result.output.size() - input.size();
            result.report.already_satisfied = !label_added && !presentation_added;
            result.message = presentation_added
                ? "Character-name TextInput presentation added successfully"
                : label_added
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
        bool presentation_added = false;
        if (options.text_input_presentation ==
                TextInputPresentation::character_name &&
            !ensure_character_name_text_input(
                result.output, error, presentation_added)) {
            return fail(ErrorCode::unsupported_gfx, std::move(error));
        }
        result.report.bytes_added = result.output.size() - input.size();
        result.report.after = inspect_controller_panel(result.output);
        if (!result.report.after.success() ||
            result.report.after.controller_rows != options.controller_rows ||
            (options.text_input_presentation ==
                    TextInputPresentation::character_name &&
                !result.report.after.character_name_text_input)) {
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
    case ErrorCode::text_input_not_found:
        return "text_input_not_found";
    case ErrorCode::text_input_invalid:
        return "text_input_invalid";
    case ErrorCode::output_verification_failed:
        return "output_verification_failed";
    }
    return "unknown";
}

const char* host_name(GfxHost host) noexcept {
    switch (host) {
    case GfxHost::unknown:
        return "unknown";
    case GfxHost::controller_settings:
        return "Controller Settings (02_040_optionsetting.gfx)";
    case GfxHost::advanced_settings:
        return "Advanced Settings (02_042_pc_graphicsetting.gfx)";
    }
    return "unknown";
}

} // namespace erui::gfx
