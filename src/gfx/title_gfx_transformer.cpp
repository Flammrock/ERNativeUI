#include "gfx/title_gfx_transformer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace erui::gfx {
namespace {

constexpr std::uint16_t kTagShowFrame = 1;
constexpr std::uint16_t kTagPlaceObject = 4;
constexpr std::uint16_t kTagRemoveObject = 5;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagRemoveObject2 = 28;
constexpr std::uint16_t kTagDefineEditText = 37;
constexpr std::uint16_t kTagPlaceObject3 = 70;

constexpr std::array<std::string_view, 4> kNativeFieldNames{
    "RegulationVersion",
    "AppVersion",
    "OnlineMode",
    "DLCVersion_0",
};
constexpr std::string_view kInfoName = "Info";
constexpr std::string_view kPrivatePlacementPrefix =
    "ERNativeUI_TitleText_";
constexpr std::string_view kPrivateVariablePrefix =
    "ERNativeUI_TitleTextValue_";
constexpr std::size_t kPrivateIndexDigits = 4;
constexpr std::size_t kMaximumTextBytes = 4096;
constexpr std::int32_t kMaximumReasonableSpacing = 4096;

struct BitReader {
    explicit BitReader(
        std::span<const std::uint8_t> bytes,
        std::size_t byte_offset = 0) noexcept
        : bytes(bytes), bit_position(byte_offset * 8U) {}

    [[nodiscard]] bool read(
        std::size_t count,
        std::uint32_t& value) noexcept {
        if (count > 32U || bit_position > bytes.size() * 8U ||
            count > bytes.size() * 8U - bit_position) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < count; ++index) {
            value = (value << 1U) |
                ((bytes[bit_position / 8U] >>
                     (7U - (bit_position % 8U))) &
                    std::uint8_t{1});
            ++bit_position;
        }
        return true;
    }

    [[nodiscard]] bool read_signed(
        std::size_t count,
        std::int32_t& value) noexcept {
        if (count > 32U) return false;
        std::uint32_t raw{};
        if (!read(count, raw)) return false;
        if (count != 0U && count < 32U &&
            (raw & (std::uint32_t{1} << (count - 1U))) != 0U) {
            raw |= ~((std::uint32_t{1} << count) - 1U);
        }
        value = static_cast<std::int32_t>(raw);
        return true;
    }

    void align() noexcept {
        bit_position = (bit_position + 7U) & ~std::size_t{7U};
    }

    [[nodiscard]] std::size_t byte_offset() const noexcept {
        return bit_position / 8U;
    }

    std::span<const std::uint8_t> bytes{};
    std::size_t bit_position{};
};

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& bytes) noexcept
        : bytes_(bytes) {}

    void write(std::uint32_t value, std::uint32_t count) {
        for (std::uint32_t remaining = count;
             remaining != 0U;
             --remaining) {
            if (bit_offset_ == 0U) bytes_.push_back(0);
            bytes_.back() |= static_cast<std::uint8_t>(
                ((value >> (remaining - 1U)) & 1U) <<
                (7U - bit_offset_));
            bit_offset_ = static_cast<std::uint8_t>(
                (bit_offset_ + 1U) & 7U);
        }
    }

    void write_signed(std::int32_t value, std::uint32_t count) {
        const std::uint32_t mask = count == 32U
            ? std::numeric_limits<std::uint32_t>::max()
            : ((std::uint32_t{1} << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() noexcept { bit_offset_ = 0; }

private:
    std::vector<std::uint8_t>& bytes_;
    std::uint8_t bit_offset_{};
};

struct MatrixFields {
    std::optional<std::int32_t> scale_x{};
    std::optional<std::int32_t> scale_y{};
    std::optional<std::int32_t> rotate_skew_0{};
    std::optional<std::int32_t> rotate_skew_1{};
    std::int32_t x{};
    std::int32_t y{};
    std::size_t end_offset{};
};

struct EditTextLayout {
    GfxFrameRect bounds{};
    std::size_t flags1_offset{};
    std::size_t color_offset{};
    std::size_t alignment_offset{};
    std::size_t variable_begin{};
    std::size_t variable_end{};
    std::size_t text_begin{};
    std::size_t text_end{};
    std::array<std::uint8_t, 4> color{};
    std::uint8_t alignment{};
};

struct ExistingLine {
    std::size_t index{};
    TitleGfxCorner corner{};
    const GfxPlacementInfo* placement{};
    GfxRawTag definition{};
    EditTextLayout layout{};
};

struct AxisTransform {
    long double scale_x{1.0L};
    long double scale_y{1.0L};
    long double translate_x{};
    long double translate_y{};
};

struct PlannedLine {
    std::int32_t x{};
    std::int32_t y{};
    std::uint8_t alignment{};
};

struct TitleContext {
    GfxSpriteInfo info{};
    const GfxTagInfo* show_frame{};
    std::array<const GfxPlacementInfo*, 4> native{};
    GfxRawTag donor_definition{};
    EditTextLayout donor_layout{};
    GfxRawTag donor_placement{};
    std::int32_t spacing{};
    std::uint16_t maximum_depth{};
    GfxFrameRect frame{};
    AxisTransform info_to_stage{};
    std::int32_t left_x{};
    std::int32_t right_x{};
    std::int32_t top_y{};
    std::int32_t bottom_y{};
    std::array<std::vector<std::int32_t>, 2> occupied_text_y{};
    std::vector<ExistingLine> existing_lines{};
};

[[nodiscard]] TitleGfxError semantic_error(
    TitleGfxErrorCode code,
    TitleGfxFeature feature,
    std::uint32_t detail = 0,
    std::uint32_t line_index = TitleGfxError::no_line) noexcept {
    return {
        .code = code,
        .feature = feature,
        .detail = detail,
        .line_index = line_index,
    };
}

[[nodiscard]] TitleGfxError document_error(
    TitleGfxFeature feature,
    const GfxDocumentError& error,
    TitleGfxErrorCode code = TitleGfxErrorCode::mutation_failed) noexcept {
    return {
        .code = code,
        .feature = feature,
        .document_error = error,
    };
}

[[nodiscard]] bool read_u16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint16_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 2U) return false;
    value = static_cast<std::uint16_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
    return true;
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

[[nodiscard]] std::uint32_t signed_bits(std::int32_t value) noexcept {
    for (std::uint32_t count = 1U; count < 32U; ++count) {
        const auto minimum = -(std::int64_t{1} << (count - 1U));
        const auto maximum = (std::int64_t{1} << (count - 1U)) - 1;
        if (value >= minimum && value <= maximum) return count;
    }
    return 32U;
}

[[nodiscard]] bool decode_matrix(
    std::span<const std::uint8_t> body,
    std::size_t offset,
    MatrixFields& output) noexcept {
    output = {};
    BitReader reader(body, offset);
    std::uint32_t present{};
    std::uint32_t count{};
    if (!reader.read(1U, present)) return false;
    if (present != 0U) {
        std::int32_t x{};
        std::int32_t y{};
        if (!reader.read(5U, count) || count > 31U ||
            !reader.read_signed(count, x) ||
            !reader.read_signed(count, y)) {
            return false;
        }
        output.scale_x = x;
        output.scale_y = y;
    }
    if (!reader.read(1U, present)) return false;
    if (present != 0U) {
        std::int32_t first{};
        std::int32_t second{};
        if (!reader.read(5U, count) || count > 31U ||
            !reader.read_signed(count, first) ||
            !reader.read_signed(count, second)) {
            return false;
        }
        output.rotate_skew_0 = first;
        output.rotate_skew_1 = second;
    }
    if (!reader.read(5U, count) || count > 31U ||
        !reader.read_signed(count, output.x) ||
        !reader.read_signed(count, output.y)) {
        return false;
    }
    reader.align();
    output.end_offset = reader.byte_offset();
    return output.end_offset <= body.size();
}

void append_matrix(
    std::vector<std::uint8_t>& output,
    const MatrixFields& fields,
    std::int32_t x,
    std::int32_t y) {
    BitWriter writer(output);
    const bool has_scale = fields.scale_x && fields.scale_y;
    writer.write(has_scale ? 1U : 0U, 1U);
    if (has_scale) {
        const std::uint32_t count = std::max(
            signed_bits(*fields.scale_x),
            signed_bits(*fields.scale_y));
        writer.write(count, 5U);
        writer.write_signed(*fields.scale_x, count);
        writer.write_signed(*fields.scale_y, count);
    }
    const bool has_rotate =
        fields.rotate_skew_0 && fields.rotate_skew_1;
    writer.write(has_rotate ? 1U : 0U, 1U);
    if (has_rotate) {
        const std::uint32_t count = std::max(
            signed_bits(*fields.rotate_skew_0),
            signed_bits(*fields.rotate_skew_1));
        writer.write(count, 5U);
        writer.write_signed(*fields.rotate_skew_0, count);
        writer.write_signed(*fields.rotate_skew_1, count);
    }
    const std::uint32_t count = std::max(signed_bits(x), signed_bits(y));
    writer.write(count, 5U);
    writer.write_signed(x, count);
    writer.write_signed(y, count);
    writer.align();
}

[[nodiscard]] bool skip_color_transform(
    std::span<const std::uint8_t> body,
    std::size_t& offset) noexcept {
    BitReader reader(body, offset);
    std::uint32_t has_add{};
    std::uint32_t has_multiply{};
    std::uint32_t count{};
    std::uint32_t ignored{};
    if (!reader.read(1U, has_add) ||
        !reader.read(1U, has_multiply) ||
        !reader.read(4U, count) || count > 31U) {
        return false;
    }
    const std::size_t terms =
        (has_add != 0U ? 4U : 0U) +
        (has_multiply != 0U ? 4U : 0U);
    for (std::size_t index = 0; index < terms; ++index) {
        if (!reader.read(count, ignored)) return false;
    }
    reader.align();
    offset = reader.byte_offset();
    return offset <= body.size();
}

[[nodiscard]] bool read_rect(
    std::span<const std::uint8_t> body,
    std::size_t& offset,
    GfxFrameRect& output) noexcept {
    BitReader reader(body, offset);
    std::uint32_t count{};
    if (!reader.read(5U, count) || count > 31U) return false;
    if (!reader.read_signed(count, output.x_min) ||
        !reader.read_signed(count, output.x_max) ||
        !reader.read_signed(count, output.y_min) ||
        !reader.read_signed(count, output.y_max)) {
        return false;
    }
    reader.align();
    offset = reader.byte_offset();
    return offset <= body.size();
}

[[nodiscard]] bool read_c_string(
    std::span<const std::uint8_t> body,
    std::size_t begin,
    std::size_t maximum,
    std::size_t& end) noexcept {
    if (begin >= body.size()) return false;
    const std::size_t maximum_with_terminator =
        maximum == std::numeric_limits<std::size_t>::max()
        ? maximum
        : maximum + 1U;
    const std::size_t available = std::min(
        maximum_with_terminator, body.size() - begin);
    const auto terminator = std::find(
        body.begin() + static_cast<std::ptrdiff_t>(begin),
        body.begin() + static_cast<std::ptrdiff_t>(begin + available),
        std::uint8_t{0});
    if (terminator ==
        body.begin() + static_cast<std::ptrdiff_t>(begin + available)) {
        return false;
    }
    end = static_cast<std::size_t>(
        std::distance(body.begin(), terminator));
    return true;
}

[[nodiscard]] bool parse_edit_text(
    const GfxRawTag& raw,
    std::size_t maximum_name_bytes,
    EditTextLayout& output) noexcept {
    output = {};
    if (raw.code != kTagDefineEditText || raw.body.size() < 6U) return false;

    std::size_t offset = 2U;
    if (!read_rect(raw.body, offset, output.bounds) ||
        !output.bounds.has_positive_area() ||
        raw.body.size() - offset < 2U) {
        return false;
    }
    output.flags1_offset = offset;
    const std::uint8_t flags1 = raw.body[offset++];
    const std::uint8_t flags2 = raw.body[offset++];
    const bool has_text = (flags1 & 0x80U) != 0U;
    const bool has_text_color = (flags1 & 0x04U) != 0U;
    const bool has_maximum_length = (flags1 & 0x02U) != 0U;
    const bool has_font = (flags1 & 0x01U) != 0U;
    const bool has_font_class = (flags2 & 0x80U) != 0U;
    const bool has_layout = (flags2 & 0x20U) != 0U;
    const bool has_html = (flags2 & 0x02U) != 0U;
    if (!has_text || !has_text_color ||
        (!has_font && !has_font_class) || has_html) {
        return false;
    }

    if (has_font) {
        if (raw.body.size() - offset < 2U) return false;
        offset += 2U;
    }
    if (has_font_class) {
        std::size_t end{};
        if (!read_c_string(raw.body, offset, maximum_name_bytes, end)) {
            return false;
        }
        offset = end + 1U;
    }
    // Scaleform authors FontHeight for both FontId and FontClass forms. The
    // native title fields use FontClass, which is why this is not conditioned
    // on HasFont alone as the original SWF 8 wording might suggest.
    if (raw.body.size() - offset < 2U) return false;
    offset += 2U;

    if (raw.body.size() - offset < 4U) return false;
    output.color_offset = offset;
    std::copy_n(
        raw.body.begin() + static_cast<std::ptrdiff_t>(offset),
        4U,
        output.color.begin());
    offset += 4U;

    if (has_maximum_length) {
        if (raw.body.size() - offset < 2U) return false;
        offset += 2U;
    }
    if (has_layout) {
        constexpr std::size_t layout_bytes = 9U;
        if (raw.body.size() - offset < layout_bytes) return false;
        output.alignment_offset = offset;
        output.alignment = raw.body[offset];
        offset += layout_bytes;
    } else {
        output.alignment = 0U;
    }

    output.variable_begin = offset;
    if (!read_c_string(
            raw.body,
            output.variable_begin,
            maximum_name_bytes,
            output.variable_end)) {
        return false;
    }
    output.text_begin = output.variable_end + 1U;
    if (!read_c_string(
            raw.body,
            output.text_begin,
            std::max(maximum_name_bytes, kMaximumTextBytes),
            output.text_end)) {
        return false;
    }
    return output.text_end + 1U == raw.body.size();
}

[[nodiscard]] std::string_view field_string(
    const GfxRawTag& raw,
    std::size_t begin,
    std::size_t end) noexcept {
    if (begin > end || end > raw.body.size()) return {};
    return {
        reinterpret_cast<const char*>(raw.body.data() + begin),
        end - begin};
}

[[nodiscard]] std::vector<std::uint8_t> normalized_edit_text(
    const GfxRawTag& raw,
    const EditTextLayout& layout) {
    std::vector<std::uint8_t> output(
        raw.body.begin(),
        raw.body.begin() + static_cast<std::ptrdiff_t>(layout.variable_begin));
    output[0] = 0;
    output[1] = 0;
    std::fill_n(
        output.begin() + static_cast<std::ptrdiff_t>(layout.color_offset),
        4U,
        std::uint8_t{0});
    output.push_back(0);
    output.push_back(0);
    return output;
}

[[nodiscard]] bool valid_utf8_text(std::string_view text) noexcept {
    if (text.empty() || text.size() > kMaximumTextBytes) return false;
    std::size_t offset{};
    while (offset < text.size()) {
        const std::uint8_t first =
            static_cast<std::uint8_t>(text[offset]);
        if (first == 0U || first == '\r' || first == '\n') return false;
        if (first < 0x80U) {
            ++offset;
            continue;
        }

        std::size_t trailing{};
        std::uint32_t code_point{};
        std::uint32_t minimum{};
        if (first >= 0xC2U && first <= 0xDFU) {
            trailing = 1U;
            code_point = first & 0x1FU;
            minimum = 0x80U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            trailing = 2U;
            code_point = first & 0x0FU;
            minimum = 0x800U;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            trailing = 3U;
            code_point = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (trailing > text.size() - offset - 1U) return false;
        for (std::size_t index = 1U; index <= trailing; ++index) {
            const std::uint8_t next =
                static_cast<std::uint8_t>(text[offset + index]);
            if ((next & 0xC0U) != 0x80U) return false;
            code_point = (code_point << 6U) | (next & 0x3FU);
        }
        if (code_point < minimum || code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
            code_point == 0x0085U || code_point == 0x2028U ||
            code_point == 0x2029U) {
            return false;
        }
        offset += trailing + 1U;
    }
    return true;
}

[[nodiscard]] std::string_view corner_code(TitleGfxCorner corner) noexcept {
    switch (corner) {
    case TitleGfxCorner::top_left: return "TL";
    case TitleGfxCorner::top_right: return "TR";
    case TitleGfxCorner::bottom_left: return "BL";
    case TitleGfxCorner::bottom_right: return "BR";
    default: return {};
    }
}

[[nodiscard]] bool valid_corner(TitleGfxCorner corner) noexcept {
    return !corner_code(corner).empty();
}

[[nodiscard]] bool valid_dlc_state(TitleGfxDlcState state) noexcept {
    return state == TitleGfxDlcState::unknown ||
        state == TitleGfxDlcState::absent ||
        state == TitleGfxDlcState::installed;
}

[[nodiscard]] bool valid_dlc_field_proof(
    TitleGfxDlcFieldProof proof) noexcept {
    return proof == TitleGfxDlcFieldProof::unproven ||
        proof ==
            TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree;
}

[[nodiscard]] bool valid_text_side(TitleGfxTextSide side) noexcept {
    return side == TitleGfxTextSide::left ||
        side == TitleGfxTextSide::right;
}

[[nodiscard]] bool may_reuse_dlc_row(
    TitleGfxTransformOptions options) noexcept {
    return options.dlc_state == TitleGfxDlcState::absent &&
        options.dlc_field_proof ==
            TitleGfxDlcFieldProof::canonical_shadow_of_the_erdtree;
}

[[nodiscard]] std::string indexed_name(
    std::string_view prefix,
    TitleGfxCorner corner,
    std::size_t index) {
    std::array<char, kPrivateIndexDigits> digits{};
    std::size_t value = index;
    for (std::size_t position = digits.size(); position != 0U; --position) {
        digits[position - 1U] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    }
    std::string result(prefix);
    result.append(corner_code(corner));
    result.push_back('_');
    result.append(digits.data(), digits.size());
    return result;
}

struct PrivateIdentity {
    TitleGfxCorner corner{};
    std::size_t index{};
};

[[nodiscard]] std::optional<PrivateIdentity> private_identity(
    std::string_view value,
    std::string_view prefix) noexcept {
    if (!value.starts_with(prefix) ||
        value.size() != prefix.size() + 3U + kPrivateIndexDigits ||
        value[prefix.size() + 2U] != '_') {
        return std::nullopt;
    }
    TitleGfxCorner corner{};
    const std::string_view code = value.substr(prefix.size(), 2U);
    if (code == "TL") corner = TitleGfxCorner::top_left;
    else if (code == "TR") corner = TitleGfxCorner::top_right;
    else if (code == "BL") corner = TitleGfxCorner::bottom_left;
    else if (code == "BR") corner = TitleGfxCorner::bottom_right;
    else return std::nullopt;
    std::size_t index{};
    const char* begin = value.data() + prefix.size() + 3U;
    const char* end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, index);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        index >= title_gfx_maximum_lines) {
        return std::nullopt;
    }
    return PrivateIdentity{corner, index};
}

[[nodiscard]] const GfxPlacementInfo* unique_named_placement(
    const GfxDocument& document,
    GfxTagHandle owner,
    std::string_view name) noexcept {
    const GfxPlacementInfo* result{};
    for (const auto& placement : document.placements()) {
        if (placement.owner_sprite != owner || !placement.has_name ||
            placement.name != name) {
            continue;
        }
        if (result != nullptr) return nullptr;
        result = &placement;
    }
    return result;
}

[[nodiscard]] std::optional<std::uint16_t> raw_depth(
    const GfxDocument& document,
    const GfxTagInfo& tag) {
    auto copied = document.copy_tag(tag.handle);
    if (!copied.success()) return std::nullopt;
    std::size_t offset{};
    switch (copied.tag.code) {
    case kTagPlaceObject:
    case kTagRemoveObject:
        offset = 2U;
        break;
    case kTagPlaceObject2:
        offset = 1U;
        break;
    case kTagRemoveObject2:
        offset = 0U;
        break;
    case kTagPlaceObject3:
        offset = 2U;
        break;
    default:
        return std::nullopt;
    }
    std::uint16_t depth{};
    if (!read_u16(copied.tag.body, offset, depth)) return std::nullopt;
    return depth;
}

[[nodiscard]] bool placement_can_be_donor(
    const GfxPlacementInfo& placement) noexcept {
    return placement.tag_code == kTagPlaceObject2 && !placement.move &&
        placement.has_character && placement.has_matrix &&
        placement.has_name &&
        (placement.flags == 0x26U || placement.flags == 0x2EU);
}

[[nodiscard]] std::optional<GfxRawTag> clone_placement(
    const GfxRawTag& donor,
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    if (donor.code != kTagPlaceObject2 || donor.body.size() < 6U ||
        (donor.body[0] != 0x26U && donor.body[0] != 0x2EU)) {
        return std::nullopt;
    }
    MatrixFields matrix{};
    if (!decode_matrix(donor.body, 5U, matrix)) return std::nullopt;
    const auto representable = [](std::int32_t value) noexcept {
        return signed_bits(value) <= 31U;
    };
    if (!representable(x) || !representable(y) ||
        (matrix.scale_x && !representable(*matrix.scale_x)) ||
        (matrix.scale_y && !representable(*matrix.scale_y)) ||
        (matrix.rotate_skew_0 &&
            !representable(*matrix.rotate_skew_0)) ||
        (matrix.rotate_skew_1 &&
            !representable(*matrix.rotate_skew_1))) {
        return std::nullopt;
    }

    std::size_t name_offset = matrix.end_offset;
    if ((donor.body[0] & 0x08U) != 0U &&
        !skip_color_transform(donor.body, name_offset)) {
        return std::nullopt;
    }
    if (name_offset >= donor.body.size()) return std::nullopt;
    const auto name_end = std::find(
        donor.body.begin() + static_cast<std::ptrdiff_t>(name_offset),
        donor.body.end(),
        std::uint8_t{0});
    if (name_end == donor.body.end() ||
        std::next(name_end) != donor.body.end()) {
        return std::nullopt;
    }

    GfxRawTag result{
        .code = donor.code,
        // The generated private name can be longer than the native donor.
        // Let GfxDocument promote the tag instead of retaining an invalid
        // short-form constraint copied from that donor.
        .header_form = GfxTagHeaderForm::automatic,
    };
    result.body.reserve(donor.body.size() + name.size() + 16U);
    result.body.push_back(donor.body[0]);
    append_u16(result.body, depth);
    append_u16(result.body, character_id);
    append_matrix(result.body, matrix, x, y);
    result.body.insert(
        result.body.end(),
        donor.body.begin() + static_cast<std::ptrdiff_t>(matrix.end_offset),
        donor.body.begin() + static_cast<std::ptrdiff_t>(name_offset));
    result.body.insert(result.body.end(), name.begin(), name.end());
    result.body.push_back(0);
    return result;
}

[[nodiscard]] GfxRawTag clone_edit_text(
    const GfxRawTag& donor,
    const EditTextLayout& layout,
    std::uint16_t character_id,
    std::string_view variable,
    std::string_view text,
    std::array<std::uint8_t, 4> color,
    std::uint8_t alignment) {
    GfxRawTag result{
        .code = donor.code,
        // Initial text and the private variable name change the body length.
        // Automatic form preserves valid compact output while allowing the
        // required promotion past SWF's 62-byte short-tag ceiling.
        .header_form = GfxTagHeaderForm::automatic,
    };
    result.body.reserve(
        layout.variable_begin + variable.size() + text.size() + 2U);
    result.body.insert(
        result.body.end(),
        donor.body.begin(),
        donor.body.begin() +
            static_cast<std::ptrdiff_t>(layout.variable_begin));
    result.body[0] = static_cast<std::uint8_t>(character_id);
    result.body[1] = static_cast<std::uint8_t>(character_id >> 8U);
    // Generated footer resources are deliberately single-line. Native donor
    // fields carry these layout bits even for short authored values, but
    // retaining them lets long client strings wrap into adjacent lines.
    constexpr std::uint8_t kWordWrapAndMultiline = 0x60U;
    result.body[layout.flags1_offset] &=
        static_cast<std::uint8_t>(~kWordWrapAndMultiline);
    std::copy(
        color.begin(),
        color.end(),
        result.body.begin() +
            static_cast<std::ptrdiff_t>(layout.color_offset));
    if (layout.alignment_offset != 0U) {
        result.body[layout.alignment_offset] = alignment;
    }
    result.body.insert(result.body.end(), variable.begin(), variable.end());
    result.body.push_back(0);
    result.body.insert(result.body.end(), text.begin(), text.end());
    result.body.push_back(0);
    return result;
}

[[nodiscard]] bool private_line_matches(
    const ExistingLine& existing,
    const TitleGfxTextLine& requested,
    const EditTextLayout& donor_layout) noexcept {
    if (existing.corner != requested.corner) return false;
    const std::string expected_variable = indexed_name(
        kPrivateVariablePrefix, existing.corner, existing.index);
    if (field_string(
            existing.definition,
            existing.layout.variable_begin,
            existing.layout.variable_end) != expected_variable ||
        field_string(
            existing.definition,
            existing.layout.text_begin,
            existing.layout.text_end) != requested.text) {
        return false;
    }
    std::array<std::uint8_t, 4> expected_color = donor_layout.color;
    if (requested.color) {
        expected_color = {
            requested.color->red,
            requested.color->green,
            requested.color->blue,
            0xFFU};
    }
    const std::uint8_t expected_alignment =
        existing.corner == TitleGfxCorner::top_left ||
            existing.corner == TitleGfxCorner::bottom_left
        ? 0U
        : 1U;
    return existing.layout.color == expected_color &&
        existing.layout.alignment == expected_alignment;
}

[[nodiscard]] bool same_linear_matrix(
    const GfxMatrix& left,
    const GfxMatrix& right) noexcept {
    return left.scale_x == right.scale_x &&
        left.scale_y == right.scale_y &&
        left.rotate_skew_0 == right.rotate_skew_0 &&
        left.rotate_skew_1 == right.rotate_skew_1;
}

[[nodiscard]] bool positive_axis_aligned(
    const GfxMatrix& matrix) noexcept {
    return matrix.scale_x > 0 && matrix.scale_y > 0 &&
        matrix.rotate_skew_0 == 0 && matrix.rotate_skew_1 == 0;
}

[[nodiscard]] AxisTransform axis_transform(const GfxMatrix& matrix) noexcept {
    constexpr long double fixed = 65536.0L;
    return {
        static_cast<long double>(matrix.scale_x) / fixed,
        static_cast<long double>(matrix.scale_y) / fixed,
        static_cast<long double>(matrix.translate_x),
        static_cast<long double>(matrix.translate_y),
    };
}

[[nodiscard]] AxisTransform compose(
    const AxisTransform& parent,
    const AxisTransform& child) noexcept {
    return {
        parent.scale_x * child.scale_x,
        parent.scale_y * child.scale_y,
        parent.scale_x * child.translate_x + parent.translate_x,
        parent.scale_y * child.translate_y + parent.translate_y,
    };
}

struct StageBox {
    long double x_min{};
    long double x_max{};
    long double y_min{};
    long double y_max{};
};

[[nodiscard]] StageBox stage_box(
    const AxisTransform& info_to_stage,
    const GfxMatrix& placement,
    const GfxFrameRect& bounds,
    std::optional<std::int32_t> x = std::nullopt,
    std::optional<std::int32_t> y = std::nullopt) noexcept {
    const AxisTransform local = axis_transform(placement);
    const long double tx = static_cast<long double>(
        x.value_or(placement.translate_x));
    const long double ty = static_cast<long double>(
        y.value_or(placement.translate_y));
    return {
        info_to_stage.scale_x *
                (local.scale_x * bounds.x_min + tx) +
            info_to_stage.translate_x,
        info_to_stage.scale_x *
                (local.scale_x * bounds.x_max + tx) +
            info_to_stage.translate_x,
        info_to_stage.scale_y *
                (local.scale_y * bounds.y_min + ty) +
            info_to_stage.translate_y,
        info_to_stage.scale_y *
                (local.scale_y * bounds.y_max + ty) +
            info_to_stage.translate_y,
    };
}

[[nodiscard]] bool inside_frame(
    const StageBox& box,
    const GfxFrameRect& frame) noexcept {
    constexpr long double tolerance = 0.51L;
    return box.x_min >= static_cast<long double>(frame.x_min) - tolerance &&
        box.x_max <= static_cast<long double>(frame.x_max) + tolerance &&
        box.y_min >= static_cast<long double>(frame.y_min) - tolerance &&
        box.y_max <= static_cast<long double>(frame.y_max) + tolerance;
}

[[nodiscard]] std::optional<std::int32_t> solve_translation(
    long double stage_edge,
    long double info_scale,
    long double info_translation,
    long double local_scale,
    std::int32_t local_edge) noexcept {
    if (!(info_scale > 0.0L) || !(local_scale > 0.0L)) return std::nullopt;
    const long double exact =
        (stage_edge - info_translation) / info_scale -
        local_scale * static_cast<long double>(local_edge);
    if (!std::isfinite(exact) ||
        exact < static_cast<long double>(std::numeric_limits<std::int32_t>::min()) ||
        exact > static_cast<long double>(std::numeric_limits<std::int32_t>::max())) {
        return std::nullopt;
    }
    const auto rounded = static_cast<std::int64_t>(std::llround(exact));
    if (rounded < std::numeric_limits<std::int32_t>::min() ||
        rounded > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    const long double reconstructed = info_scale *
            (local_scale * static_cast<long double>(local_edge) +
             static_cast<long double>(rounded)) +
        info_translation;
    if (std::fabs(reconstructed - stage_edge) > 0.51L) return std::nullopt;
    return static_cast<std::int32_t>(rounded);
}

[[nodiscard]] TitleGfxError resolve_info_transform(
    const GfxDocument& document,
    const GfxSpriteInfo& info,
    AxisTransform& output) {
    output = {};
    std::uint16_t character = info.character_id;
    std::unordered_set<std::uint16_t> visited{};
    for (std::size_t level = 0;
         level <= document.limits().maximum_nesting_depth;
         ++level) {
        if (!visited.insert(character).second) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::info_sprite,
                40U);
        }
        const GfxPlacementInfo* owner_placement{};
        for (const auto& placement : document.placements()) {
            if (!placement.move && placement.has_character &&
                placement.character_id == character) {
                if (owner_placement != nullptr) {
                    return semantic_error(
                        TitleGfxErrorCode::ambiguous_structure,
                        TitleGfxFeature::info_sprite,
                        41U);
                }
                owner_placement = &placement;
            }
        }
        if (owner_placement == nullptr || !owner_placement->has_matrix ||
            !positive_axis_aligned(owner_placement->matrix)) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::info_sprite,
                42U);
        }

        // A native late-frame nudge changes Info by one twip. Accept only
        // that harmless translation variance; a larger or linear animation
        // would make fixed corner anchors structurally ambiguous.
        for (const auto& movement : document.placements()) {
            if (!movement.move ||
                movement.owner_sprite != owner_placement->owner_sprite ||
                movement.depth != owner_placement->depth ||
                !movement.has_matrix) {
                continue;
            }
            if (!positive_axis_aligned(movement.matrix) ||
                !same_linear_matrix(movement.matrix, owner_placement->matrix) ||
                std::llabs(
                    static_cast<long long>(movement.translation.x) -
                    owner_placement->translation.x) > 1LL ||
                std::llabs(
                    static_cast<long long>(movement.translation.y) -
                    owner_placement->translation.y) > 1LL) {
                return semantic_error(
                    TitleGfxErrorCode::incompatible_structure,
                    TitleGfxFeature::info_sprite,
                    43U);
            }
        }

        output = compose(axis_transform(owner_placement->matrix), output);
        if (!owner_placement->owner_sprite) return {};
        const auto parent = document.sprite(owner_placement->owner_sprite);
        if (!parent) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::info_sprite,
                44U);
        }
        character = parent->character_id;
    }
    return semantic_error(
        TitleGfxErrorCode::incompatible_structure,
        TitleGfxFeature::info_sprite,
        45U);
}

[[nodiscard]] std::optional<std::vector<PlannedLine>> plan_lines(
    const TitleContext& context,
    std::span<const TitleGfxTextLine> lines);

[[nodiscard]] TitleGfxError locate_title(
    const GfxDocument& document,
    TitleContext& output,
    TitleGfxTransformOptions options) {
    output = {};
    if (!document.valid()) {
        return semantic_error(
            TitleGfxErrorCode::invalid_document,
            TitleGfxFeature::document);
    }

    std::vector<std::pair<GfxSpriteInfo, std::array<const GfxPlacementInfo*, 4>>>
        candidates{};
    for (const auto& sprite : document.sprites()) {
        std::array<const GfxPlacementInfo*, 4> fields{};
        bool matches = true;
        for (std::size_t index = 0; index < fields.size(); ++index) {
            fields[index] = unique_named_placement(
                document, sprite.definition, kNativeFieldNames[index]);
            if (fields[index] == nullptr) {
                matches = false;
                break;
            }
        }
        if (matches) candidates.emplace_back(sprite, fields);
    }
    if (candidates.empty()) {
        return semantic_error(
            TitleGfxErrorCode::structure_not_found,
            TitleGfxFeature::info_sprite);
    }
    if (candidates.size() != 1U) {
        return semantic_error(
            TitleGfxErrorCode::ambiguous_structure,
            TitleGfxFeature::info_sprite,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                candidates.size(),
                std::numeric_limits<std::uint32_t>::max())));
    }
    output.info = candidates.front().first;
    output.native = candidates.front().second;

    std::size_t info_roots{};
    for (const auto& placement : document.placements()) {
        if (placement.has_name && placement.name == kInfoName &&
            placement.has_character &&
            placement.character_id == output.info.character_id) {
            ++info_roots;
        }
    }
    if (info_roots != 1U || output.info.frame_count != 1U) {
        return semantic_error(
            info_roots > 1U
                ? TitleGfxErrorCode::ambiguous_structure
                : TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::info_sprite,
            static_cast<std::uint32_t>(info_roots));
    }

    const GfxTagInfo* show_frame{};
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite != output.info.definition ||
            tag.code != kTagShowFrame) {
            continue;
        }
        if (show_frame != nullptr) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::info_sprite,
                2U);
        }
        show_frame = &tag;
    }
    if (show_frame == nullptr) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::info_sprite,
            3U);
    }
    output.show_frame = show_frame;

    if (!placement_can_be_donor(*output.native[0]) ||
        !placement_can_be_donor(*output.native[1]) ||
        !placement_can_be_donor(*output.native[2]) ||
        !placement_can_be_donor(*output.native[3])) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            1U);
    }
    for (std::size_t index = 1U; index < output.native.size(); ++index) {
        if (!positive_axis_aligned(output.native[index]->matrix) ||
            !same_linear_matrix(
                output.native[0]->matrix, output.native[index]->matrix)) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::native_text_fields,
                3U);
        }
    }
    if (!positive_axis_aligned(output.native[0]->matrix)) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            4U);
    }
    const std::int64_t first_spacing =
        static_cast<std::int64_t>(output.native[0]->translation.y) -
        output.native[1]->translation.y;
    const std::int64_t second_spacing =
        static_cast<std::int64_t>(output.native[1]->translation.y) -
        output.native[2]->translation.y;
    const std::int64_t dlc_spacing =
        static_cast<std::int64_t>(output.native[2]->translation.y) -
        output.native[3]->translation.y;
    if (first_spacing != second_spacing ||
        dlc_spacing != first_spacing * 2 || first_spacing <= 0 ||
        first_spacing > kMaximumReasonableSpacing ||
        output.native[0]->translation.x != output.native[1]->translation.x ||
        output.native[0]->translation.x != output.native[2]->translation.x ||
        output.native[0]->translation.x != output.native[3]->translation.x) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            2U);
    }
    output.spacing = static_cast<std::int32_t>(first_spacing);

    std::vector<std::uint8_t> normalized_donor{};
    for (std::size_t index = 0; index < output.native.size(); ++index) {
        const auto definition = document.character_definition(
            output.native[index]->character_id);
        if (!definition || definition->tag_code != kTagDefineEditText) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::native_text_fields,
                static_cast<std::uint32_t>(10U + index));
        }
        auto copied = document.copy_tag(definition->tag);
        if (!copied.success()) {
            return document_error(
                TitleGfxFeature::native_text_fields,
                copied.error);
        }
        EditTextLayout layout{};
        if (!parse_edit_text(
                copied.tag,
                document.limits().maximum_name_bytes,
                layout)) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::native_text_fields,
                static_cast<std::uint32_t>(20U + index));
        }
        if (layout.alignment != 1U || layout.alignment_offset == 0U) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::native_text_fields,
                static_cast<std::uint32_t>(24U + index));
        }
        auto normalized = normalized_edit_text(copied.tag, layout);
        if (index == 0U) {
            normalized_donor = std::move(normalized);
            output.donor_definition = std::move(copied.tag);
            output.donor_layout = layout;
        } else if (normalized != normalized_donor) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::native_text_fields,
                static_cast<std::uint32_t>(30U + index));
        }
    }
    auto donor_placement = document.copy_tag(output.native[0]->tag);
    if (!donor_placement.success()) {
        return document_error(
            TitleGfxFeature::native_text_fields,
            donor_placement.error);
    }
    output.donor_placement = std::move(donor_placement.tag);

    const auto frame = document.frame_rect();
    if (!frame || !frame->has_positive_area()) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::document,
            1U);
    }
    output.frame = *frame;
    if (auto error = resolve_info_transform(
            document, output.info, output.info_to_stage);
        !error.success()) {
        return error;
    }

    std::array<StageBox, 4> native_boxes{};
    for (std::size_t index = 0; index < output.native.size(); ++index) {
        native_boxes[index] = stage_box(
            output.info_to_stage,
            output.native[index]->matrix,
            output.donor_layout.bounds);
        if (!inside_frame(native_boxes[index], output.frame)) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::native_text_fields,
                static_cast<std::uint32_t>(40U + index));
        }
    }
    const long double native_right = std::max_element(
        native_boxes.begin(), native_boxes.end(),
        [](const StageBox& left, const StageBox& right) {
            return left.x_max < right.x_max;
        })->x_max;
    const long double native_bottom = std::max_element(
        native_boxes.begin(), native_boxes.end(),
        [](const StageBox& left, const StageBox& right) {
            return left.y_max < right.y_max;
        })->y_max;
    const long double horizontal_inset =
        static_cast<long double>(output.frame.x_max) - native_right;
    const long double vertical_inset =
        static_cast<long double>(output.frame.y_max) - native_bottom;
    const long double frame_width =
        static_cast<long double>(output.frame.x_max) - output.frame.x_min;
    const long double frame_height =
        static_cast<long double>(output.frame.y_max) - output.frame.y_min;
    if (horizontal_inset < 0.0L || vertical_inset < 0.0L ||
        horizontal_inset * 2.0L >= frame_width ||
        vertical_inset * 2.0L >= frame_height) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            50U);
    }
    const AxisTransform donor_matrix = axis_transform(
        output.native[0]->matrix);
    const auto left_x = solve_translation(
        static_cast<long double>(output.frame.x_min) + horizontal_inset,
        output.info_to_stage.scale_x,
        output.info_to_stage.translate_x,
        donor_matrix.scale_x,
        output.donor_layout.bounds.x_min);
    const auto right_x = solve_translation(
        static_cast<long double>(output.frame.x_max) - horizontal_inset,
        output.info_to_stage.scale_x,
        output.info_to_stage.translate_x,
        donor_matrix.scale_x,
        output.donor_layout.bounds.x_max);
    const auto top_y = solve_translation(
        static_cast<long double>(output.frame.y_min) + vertical_inset,
        output.info_to_stage.scale_y,
        output.info_to_stage.translate_y,
        donor_matrix.scale_y,
        output.donor_layout.bounds.y_min);
    const auto bottom_y = solve_translation(
        static_cast<long double>(output.frame.y_max) - vertical_inset,
        output.info_to_stage.scale_y,
        output.info_to_stage.translate_y,
        donor_matrix.scale_y,
        output.donor_layout.bounds.y_max);
    if (!left_x || !right_x || !top_y || !bottom_y ||
        *right_x != output.native[0]->translation.x ||
        *bottom_y != output.native[0]->translation.y) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            51U);
    }
    const StageBox left_box = stage_box(
        output.info_to_stage,
        output.native[0]->matrix,
        output.donor_layout.bounds,
        *left_x,
        *top_y);
    const StageBox right_box = stage_box(
        output.info_to_stage,
        output.native[0]->matrix,
        output.donor_layout.bounds,
        *right_x,
        *top_y);
    const long double horizontal_middle =
        (static_cast<long double>(output.frame.x_min) +
         output.frame.x_max) /
        2.0L;
    if (!inside_frame(left_box, output.frame) ||
        !inside_frame(right_box, output.frame) ||
        left_box.x_max > horizontal_middle ||
        right_box.x_min < horizontal_middle ||
        left_box.x_max > right_box.x_min) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            54U);
    }
    output.left_x = *left_x;
    output.right_x = *right_x;
    output.top_y = *top_y;
    output.bottom_y = *bottom_y;

    std::unordered_set<std::uint16_t> private_characters{};
    std::unordered_set<std::uint64_t> private_placement_tokens{};
    std::unordered_set<std::uint64_t> positioned_placement_tokens{};
    std::vector<std::uint8_t> normalized_geometry = normalized_donor;
    if (output.donor_layout.alignment_offset >= normalized_geometry.size()) {
        return semantic_error(
            TitleGfxErrorCode::incompatible_structure,
            TitleGfxFeature::native_text_fields,
            52U);
    }
    normalized_geometry[output.donor_layout.alignment_offset] = 0U;
    for (const auto& placement : document.placements()) {
        if (placement.owner_sprite != output.info.definition) continue;
        if (placement.move || !placement.has_character ||
            !placement.has_matrix ||
            !positive_axis_aligned(placement.matrix)) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::info_sprite,
                4U);
        }
        positioned_placement_tokens.insert(placement.tag.token);
        const bool is_private = placement.has_name &&
            placement.name.starts_with(kPrivatePlacementPrefix);
        if (!is_private) {
            const auto definition = document.character_definition(
                placement.character_id);
            if (!definition || definition->tag_code != kTagDefineEditText ||
                !same_linear_matrix(
                    placement.matrix, output.native[0]->matrix)) {
                return semantic_error(
                    TitleGfxErrorCode::incompatible_structure,
                    TitleGfxFeature::info_sprite,
                    6U);
            }
            auto copied = document.copy_tag(definition->tag);
            if (!copied.success()) {
                return document_error(
                    TitleGfxFeature::native_text_fields, copied.error);
            }
            EditTextLayout layout{};
            if (!parse_edit_text(
                    copied.tag,
                    document.limits().maximum_name_bytes,
                    layout) ||
                layout.alignment_offset == 0U ||
                (layout.alignment != 0U && layout.alignment != 1U) ||
                layout.bounds != output.donor_layout.bounds) {
                return semantic_error(
                    TitleGfxErrorCode::incompatible_structure,
                    TitleGfxFeature::info_sprite,
                    7U);
            }
            auto normalized = normalized_edit_text(copied.tag, layout);
            if (layout.alignment_offset >= normalized.size()) {
                return semantic_error(
                    TitleGfxErrorCode::incompatible_structure,
                    TitleGfxFeature::info_sprite,
                    8U);
            }
            normalized[layout.alignment_offset] = 0U;
            if (normalized != normalized_geometry) {
                return semantic_error(
                    TitleGfxErrorCode::incompatible_structure,
                    TitleGfxFeature::info_sprite,
                    9U);
            }

            // DLC absence proves only the user's installation state. It does
            // not prove that a modified movie or another mod has not
            // repurposed DLCVersion_0. Free this exact placement only when the
            // caller separately proved the field's canonical semantics.
            if (may_reuse_dlc_row(options) &&
                placement.tag == output.native[3]->tag) {
                continue;
            }
            const StageBox box = stage_box(
                output.info_to_stage, placement.matrix, layout.bounds);
            if (!inside_frame(box, output.frame)) {
                return semantic_error(
                    TitleGfxErrorCode::incompatible_structure,
                    TitleGfxFeature::info_sprite,
                    10U);
            }
            const long double middle =
                (static_cast<long double>(output.frame.x_min) +
                 output.frame.x_max) /
                2.0L;
            if (box.x_min < middle) {
                output.occupied_text_y[0].push_back(
                    placement.translation.y);
            }
            if (box.x_max > middle) {
                output.occupied_text_y[1].push_back(
                    placement.translation.y);
            }
            if (placement.tag == output.native[3]->tag) {
                const std::int64_t authored_gap =
                    static_cast<std::int64_t>(placement.translation.y) +
                    output.spacing;
                if (authored_gap > std::numeric_limits<std::int32_t>::max()) {
                    return semantic_error(
                        TitleGfxErrorCode::incompatible_structure,
                        TitleGfxFeature::native_text_fields,
                        53U);
                }
                output.occupied_text_y[1].push_back(
                    static_cast<std::int32_t>(authored_gap));
            }
            continue;
        }
        const auto identity = private_identity(
            placement.name, kPrivatePlacementPrefix);
        if (!identity || !placement_can_be_donor(placement) ||
            !same_linear_matrix(
                placement.matrix, output.native[0]->matrix) ||
            !private_characters.insert(placement.character_id).second ||
            !private_placement_tokens.insert(placement.tag.token).second) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                1U);
        }
        const auto definition = document.character_definition(
            placement.character_id);
        if (!definition || definition->tag_code != kTagDefineEditText) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                2U,
                static_cast<std::uint32_t>(identity->index));
        }
        auto copied = document.copy_tag(definition->tag);
        if (!copied.success()) {
            return document_error(TitleGfxFeature::text_line, copied.error);
        }
        EditTextLayout layout{};
        if (!parse_edit_text(
                copied.tag,
                document.limits().maximum_name_bytes,
                layout) ||
            field_string(copied.tag, layout.variable_begin, layout.variable_end) !=
                indexed_name(
                    kPrivateVariablePrefix,
                    identity->corner,
                    identity->index)) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                3U,
                static_cast<std::uint32_t>(identity->index));
        }
        const std::string expected_variable = indexed_name(
            kPrivateVariablePrefix, identity->corner, identity->index);
        const std::uint8_t alignment =
            identity->corner == TitleGfxCorner::top_left ||
                identity->corner == TitleGfxCorner::bottom_left
            ? 0U
            : 1U;
        const GfxRawTag expected_definition = clone_edit_text(
            output.donor_definition,
            output.donor_layout,
            placement.character_id,
            expected_variable,
            field_string(copied.tag, layout.text_begin, layout.text_end),
            layout.color,
            alignment);
        if (copied.tag.code != expected_definition.code ||
            copied.tag.body != expected_definition.body) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                9U,
                static_cast<std::uint32_t>(identity->index));
        }
        output.existing_lines.push_back({
            .index = identity->index,
            .corner = identity->corner,
            .placement = &placement,
            .definition = std::move(copied.tag),
            .layout = layout,
        });
    }

    // Some native mods add a title-footer field in a later Scaleform parser
    // hook, after ERNativeUI has already returned its transformed bytes. Keep
    // those independently observed rows in the same collision model as
    // fields that were physically present in the document. Validate every
    // hint against the actual incoming frame and donor geometry so a stale or
    // malformed compatibility profile fails open instead of displacing text
    // to an unbounded coordinate.
    for (std::size_t index = 0;
         index < options.external_text_occupancy.size();
         ++index) {
        const TitleGfxTextOccupancy occupancy =
            options.external_text_occupancy[index];
        if (!valid_text_side(occupancy.side)) {
            return semantic_error(
                TitleGfxErrorCode::invalid_argument,
                TitleGfxFeature::text_line,
                10U,
                static_cast<std::uint32_t>(index));
        }
        const std::size_t side =
            occupancy.side == TitleGfxTextSide::left ? 0U : 1U;
        if (std::find(
                output.occupied_text_y[side].begin(),
                output.occupied_text_y[side].end(),
                occupancy.y) != output.occupied_text_y[side].end()) {
            continue;
        }
        const std::int32_t x = side == 0U
            ? output.left_x
            : output.right_x;
        const StageBox box = stage_box(
            output.info_to_stage,
            output.native[0]->matrix,
            output.donor_layout.bounds,
            x,
            occupancy.y);
        if (!inside_frame(box, output.frame)) {
            return semantic_error(
                TitleGfxErrorCode::invalid_argument,
                TitleGfxFeature::text_line,
                11U,
                static_cast<std::uint32_t>(index));
        }
        output.occupied_text_y[side].push_back(occupancy.y);
    }

    // Prepending is safe only when every visual child of Info contributes a
    // decoded translation. GfxDocument intentionally preserves legacy
    // PlaceObject and an unrecognized PlaceObject3 as opaque tags; accepting
    // either here could place our lines on top of geometry we cannot see.
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite != output.info.definition ||
            (tag.code != kTagPlaceObject &&
             tag.code != kTagPlaceObject2 &&
             tag.code != kTagPlaceObject3)) {
            continue;
        }
        if (tag.code == kTagPlaceObject ||
            !positioned_placement_tokens.contains(tag.handle.token)) {
            return semantic_error(
                TitleGfxErrorCode::incompatible_structure,
                TitleGfxFeature::info_sprite,
                5U);
        }
    }
    std::sort(
        output.existing_lines.begin(),
        output.existing_lines.end(),
        [](const ExistingLine& left, const ExistingLine& right) {
            return left.index < right.index;
        });
    for (std::size_t index = 0; index < output.existing_lines.size(); ++index) {
        if (output.existing_lines[index].index != index) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                4U,
                static_cast<std::uint32_t>(index));
        }
    }

    // Reserve every depth used by incoming content, including remove tags,
    // but exclude the private placements whose canonical depths are verified
    // immediately below. This makes the base depth stable across a second
    // transformation pass.
    for (const auto& tag : document.tags()) {
        if (tag.parent_sprite != output.info.definition ||
            private_placement_tokens.contains(tag.handle.token)) {
            continue;
        }
        if (const auto depth = raw_depth(document, tag); depth && *depth != 0U) {
            output.maximum_depth = std::max(output.maximum_depth, *depth);
        }
    }

    if (!output.existing_lines.empty()) {
        const auto info_definition = document.tag(output.info.definition);
        if (!info_definition) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                6U);
        }
        std::size_t previous_placement_ordinal{};
        std::size_t previous_definition_ordinal{};
        std::vector<TitleGfxTextLine> existing_plan{};
        existing_plan.reserve(output.existing_lines.size());
        for (const ExistingLine& line : output.existing_lines) {
            existing_plan.push_back({{}, std::nullopt, line.corner});
        }
        const auto expected_positions = plan_lines(output, existing_plan);
        if (!expected_positions ||
            output.maximum_depth ==
                std::numeric_limits<std::uint16_t>::max() ||
            output.existing_lines.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint16_t>::max() -
                    output.maximum_depth)) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                7U);
        }
        for (std::size_t index = 0;
             index < output.existing_lines.size();
             ++index) {
            const ExistingLine& line = output.existing_lines[index];
            const auto placement_tag = document.tag(line.placement->tag);
            const auto definition = document.character_definition(
                line.placement->character_id);
            const auto definition_tag = definition
                ? document.tag(definition->tag)
                : std::nullopt;
            const std::uint16_t expected_depth =
                static_cast<std::uint16_t>(
                    output.maximum_depth + index + 1U);
            const bool ordered = placement_tag && definition_tag &&
                placement_tag->parent_sprite == output.info.definition &&
                !definition_tag->parent_sprite &&
                placement_tag->ordinal < output.show_frame->ordinal &&
                definition_tag->ordinal < info_definition->ordinal &&
                (index == 0U ||
                    (previous_placement_ordinal < placement_tag->ordinal &&
                     previous_definition_ordinal < definition_tag->ordinal));
            auto actual_placement = document.copy_tag(line.placement->tag);
            const auto expected_placement = clone_placement(
                output.donor_placement,
                line.placement->character_id,
                expected_depth,
                (*expected_positions)[index].x,
                (*expected_positions)[index].y,
                indexed_name(
                    kPrivatePlacementPrefix, line.corner, index));
            const bool canonical_placement = actual_placement.success() &&
                expected_placement &&
                actual_placement.tag.code == expected_placement->code &&
                actual_placement.tag.body == expected_placement->body;
            if (!ordered || !canonical_placement) {
                return semantic_error(
                    TitleGfxErrorCode::resource_conflict,
                    TitleGfxFeature::text_line,
                    8U,
                    static_cast<std::uint32_t>(index));
            }
            previous_placement_ordinal = placement_tag->ordinal;
            previous_definition_ordinal = definition_tag->ordinal;
        }
    }

    // A private definition without its corresponding placement indicates a
    // partial earlier transformation. Treat it as a conflict instead of
    // silently allocating another resource beside it.
    for (const auto& definition : document.character_definitions()) {
        if (definition.tag_code != kTagDefineEditText ||
            private_characters.contains(definition.character_id)) {
            continue;
        }
        auto copied = document.copy_tag(definition.tag);
        if (!copied.success()) {
            return document_error(TitleGfxFeature::text_line, copied.error);
        }
        EditTextLayout layout{};
        if (!parse_edit_text(
                copied.tag,
                document.limits().maximum_name_bytes,
                layout)) {
            continue;
        }
        const auto variable = field_string(
            copied.tag, layout.variable_begin, layout.variable_end);
        if (variable.starts_with(kPrivateVariablePrefix)) {
            return semantic_error(
                TitleGfxErrorCode::resource_conflict,
                TitleGfxFeature::text_line,
                5U);
        }
    }

    return {};
}

[[nodiscard]] TitleGfxInspection make_inspection(
    const GfxDocument& document,
    const TitleContext& context) noexcept {
    std::int64_t first_y = context.top_y;
    if (!context.existing_lines.empty()) {
        first_y = context.existing_lines.front().placement->translation.y;
    } else {
        const std::array probe{
            TitleGfxTextLine{{}, std::nullopt, TitleGfxCorner::bottom_right}};
        if (const auto planned = plan_lines(context, probe); planned) {
            first_y = planned->front().y;
        }
    }
    return {
        .gfx_version = document.version(),
        .declared_length = document.declared_length(),
        .info_sprite = context.info.character_id,
        .native_text_character = context.native[0]->character_id,
        .native_maximum_depth = context.maximum_depth,
        .text_x = context.right_x,
        .first_injected_y = static_cast<std::int32_t>(std::clamp<std::int64_t>(
            first_y,
            std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::max())),
        .line_spacing = context.spacing,
        .native_color = {
            context.donor_layout.color[0],
            context.donor_layout.color[1],
            context.donor_layout.color[2]},
        .native_alpha = context.donor_layout.color[3],
        .existing_private_lines = context.existing_lines.size(),
    };
}

[[nodiscard]] std::optional<std::vector<PlannedLine>> plan_lines(
    const TitleContext& context,
    std::span<const TitleGfxTextLine> lines) {
    if (context.spacing <= 0 || lines.size() > title_gfx_maximum_lines) {
        return std::nullopt;
    }

    std::vector<PlannedLine> result(lines.size());
    auto occupied = context.occupied_text_y;
    constexpr std::array<TitleGfxCorner, 4> order{
        TitleGfxCorner::top_left,
        TitleGfxCorner::top_right,
        TitleGfxCorner::bottom_left,
        TitleGfxCorner::bottom_right,
    };
    for (const TitleGfxCorner corner : order) {
        std::vector<std::size_t> members{};
        for (std::size_t index = 0; index < lines.size(); ++index) {
            if (lines[index].corner == corner) members.push_back(index);
        }
        if (members.empty()) continue;

        const bool left = corner == TitleGfxCorner::top_left ||
            corner == TitleGfxCorner::bottom_left;
        const bool top = corner == TitleGfxCorner::top_left ||
            corner == TitleGfxCorner::top_right;
        const std::size_t side = left ? 0U : 1U;
        const std::int32_t x = left ? context.left_x : context.right_x;
        const std::uint8_t alignment = left ? 0U : 1U;
        const std::int64_t anchor = top
            ? context.top_y
            : context.bottom_y;
        const long double local_height =
            (static_cast<long double>(context.frame.y_max) -
             context.frame.y_min) /
            context.info_to_stage.scale_y;
        const std::size_t maximum_steps = static_cast<std::size_t>(
            std::min<long double>(
                100000.0L,
                std::ceil(local_height / context.spacing) +
                    static_cast<long double>(
                        occupied[side].size() + lines.size() + 4U)));

        bool found{};
        for (std::size_t step = 0; step <= maximum_steps && !found; ++step) {
            std::vector<std::int32_t> candidates{};
            candidates.reserve(members.size());
            bool valid = true;
            for (std::size_t offset = 0; offset < members.size(); ++offset) {
                const std::int64_t group_offset = top
                    ? static_cast<std::int64_t>(offset)
                    : -static_cast<std::int64_t>(
                          members.size() - 1U - offset);
                const std::int64_t inward = top
                    ? static_cast<std::int64_t>(step)
                    : -static_cast<std::int64_t>(step);
                const std::int64_t candidate = anchor +
                    (group_offset + inward) * context.spacing;
                if (candidate < std::numeric_limits<std::int32_t>::min() ||
                    candidate > std::numeric_limits<std::int32_t>::max() ||
                    signed_bits(static_cast<std::int32_t>(candidate)) > 31U) {
                    valid = false;
                    break;
                }
                const auto y = static_cast<std::int32_t>(candidate);
                const StageBox box = stage_box(
                    context.info_to_stage,
                    context.native[0]->matrix,
                    context.donor_layout.bounds,
                    x,
                    y);
                if (!inside_frame(box, context.frame) ||
                    std::any_of(
                        occupied[side].begin(),
                        occupied[side].end(),
                        [&](std::int32_t existing) {
                            return std::llabs(
                                static_cast<long long>(existing) - y) <
                                context.spacing;
                        })) {
                    valid = false;
                    break;
                }
                candidates.push_back(y);
            }
            if (!valid) continue;
            for (std::size_t offset = 0; offset < members.size(); ++offset) {
                result[members[offset]] = {
                    .x = x,
                    .y = candidates[offset],
                    .alignment = alignment,
                };
                occupied[side].push_back(candidates[offset]);
            }
            found = true;
        }
        if (!found) return std::nullopt;
    }
    return result;
}

} // namespace

TitleGfxInspectionResult inspect_title_gfx(
    const GfxDocument& document,
    TitleGfxTransformOptions options) noexcept {
    TitleGfxInspectionResult result{};
    if (!valid_dlc_state(options.dlc_state) ||
        !valid_dlc_field_proof(options.dlc_field_proof)) {
        result.error = semantic_error(
            TitleGfxErrorCode::invalid_argument,
            TitleGfxFeature::document,
            !valid_dlc_state(options.dlc_state) ? 2U : 3U);
        return result;
    }
    try {
        TitleContext context{};
        if (auto error = locate_title(document, context, options);
            !error.success()) {
            result.error = error;
            return result;
        }
        result.inspection = make_inspection(document, context);
        return result;
    } catch (...) {
        result.error = semantic_error(
            TitleGfxErrorCode::invalid_document,
            TitleGfxFeature::document);
        return result;
    }
}

TitleGfxTransformResult transform_title_gfx(
    GfxDocument source,
    std::span<const TitleGfxTextLine> lines,
    TitleGfxTransformOptions options) noexcept {
    TitleGfxTransformResult result{};
    if (!valid_dlc_state(options.dlc_state) ||
        !valid_dlc_field_proof(options.dlc_field_proof)) {
        result.error = semantic_error(
            TitleGfxErrorCode::invalid_argument,
            TitleGfxFeature::document,
            !valid_dlc_state(options.dlc_state) ? 2U : 3U);
        return result;
    }
    if (lines.size() > title_gfx_maximum_lines) {
        result.error = semantic_error(
            TitleGfxErrorCode::invalid_argument,
            TitleGfxFeature::text_line,
            static_cast<std::uint32_t>(std::min<std::size_t>(
                lines.size(),
                std::numeric_limits<std::uint32_t>::max())));
        return result;
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (!valid_utf8_text(lines[index].text) ||
            !valid_corner(lines[index].corner)) {
            result.error = semantic_error(
                TitleGfxErrorCode::invalid_argument,
                TitleGfxFeature::text_line,
                1U,
                static_cast<std::uint32_t>(index));
            return result;
        }
    }

    try {
        // A caller may provide views into the source document's indexed
        // strings. GfxDocument::apply rebuilds that index, so make every view
        // independent before the first operation that can mutate `source`.
        std::vector<std::string> text_storage{};
        text_storage.reserve(lines.size());
        for (const auto& line : lines) text_storage.emplace_back(line.text);
        std::vector<TitleGfxTextLine> stable_lines{};
        stable_lines.reserve(lines.size());
        for (std::size_t index = 0; index < lines.size(); ++index) {
            stable_lines.push_back({
                text_storage[index],
                lines[index].color,
                lines[index].corner});
        }
        const std::span<const TitleGfxTextLine> requested = stable_lines;

        TitleContext before_context{};
        if (auto error = locate_title(source, before_context, options);
            !error.success()) {
            result.error = error;
            return result;
        }
        result.before = make_inspection(source, before_context);

        if (requested.empty()) {
            result.document = std::move(source);
            result.after = result.before;
            result.status = TitleGfxTransformStatus::unchanged;
            return result;
        }
        if (!before_context.existing_lines.empty()) {
            if (before_context.existing_lines.size() != requested.size()) {
                result.error = semantic_error(
                    TitleGfxErrorCode::resource_conflict,
                    TitleGfxFeature::text_line,
                    6U);
                return result;
            }
            for (std::size_t index = 0; index < requested.size(); ++index) {
                if (!private_line_matches(
                        before_context.existing_lines[index],
                        requested[index],
                        before_context.donor_layout)) {
                    result.error = semantic_error(
                        TitleGfxErrorCode::resource_conflict,
                        TitleGfxFeature::text_line,
                        7U,
                        static_cast<std::uint32_t>(index));
                    return result;
                }
            }
            result.document = std::move(source);
            result.after = result.before;
            result.status = TitleGfxTransformStatus::unchanged;
            return result;
        }

        const auto ids = source.first_free_character_ids(requested.size());
        if (!ids.success()) {
            result.error = document_error(
                TitleGfxFeature::character_id,
                ids.error,
                TitleGfxErrorCode::character_id_exhausted);
            return result;
        }
        if (before_context.maximum_depth ==
                std::numeric_limits<std::uint16_t>::max() ||
            requested.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint16_t>::max() -
                    before_context.maximum_depth)) {
            result.error = semantic_error(
                TitleGfxErrorCode::depth_exhausted,
                TitleGfxFeature::depth);
            return result;
        }

        std::vector<GfxEdit> edits{};
        edits.reserve(requested.size() * 2U);
        const auto generated = plan_lines(before_context, requested);
        if (!generated) {
            result.error = semantic_error(
                TitleGfxErrorCode::depth_exhausted,
                TitleGfxFeature::depth,
                1U);
            return result;
        }
        for (std::size_t index = 0; index < requested.size(); ++index) {
            const std::uint16_t depth = static_cast<std::uint16_t>(
                before_context.maximum_depth + index + 1U);
            const std::string placement_name = indexed_name(
                kPrivatePlacementPrefix, requested[index].corner, index);
            const std::string variable_name = indexed_name(
                kPrivateVariablePrefix, requested[index].corner, index);
            std::array<std::uint8_t, 4> color =
                before_context.donor_layout.color;
            if (requested[index].color) {
                color = {
                    requested[index].color->red,
                    requested[index].color->green,
                    requested[index].color->blue,
                    0xFFU};
            }
            auto definition = clone_edit_text(
                before_context.donor_definition,
                before_context.donor_layout,
                ids.character_ids[index],
                variable_name,
                requested[index].text,
                color,
                (*generated)[index].alignment);
            auto placement = clone_placement(
                before_context.donor_placement,
                ids.character_ids[index],
                depth,
                (*generated)[index].x,
                (*generated)[index].y,
                placement_name);
            if (!placement) {
                result.error = semantic_error(
                    TitleGfxErrorCode::mutation_failed,
                    TitleGfxFeature::text_line,
                    8U,
                    static_cast<std::uint32_t>(index));
                return result;
            }
            edits.push_back(GfxEdit::insert_before(
                before_context.info.definition, std::move(definition)));
            edits.push_back(GfxEdit::insert_before(
                before_context.show_frame->handle, std::move(*placement)));
        }

        if (const auto error = source.apply(edits); !error.success()) {
            result.error = document_error(
                TitleGfxFeature::text_line, error);
            return result;
        }

        // GfxDocument::apply is itself transactional. This second boundary is
        // intentional: the exact bytes destined for Scaleform are serialized,
        // parsed into a fresh document, and semantically verified before they
        // become the successful result.
        const GfxDocumentLimits limits = source.limits();
        auto serialized = source.serialize_and_validate();
        if (!serialized.success()) {
            result.error = document_error(
                TitleGfxFeature::serialization,
                serialized.error,
                TitleGfxErrorCode::verification_failed);
            return result;
        }
        auto reparsed = GfxDocument::parse(serialized.bytes, limits);
        if (!reparsed.success()) {
            result.error = document_error(
                TitleGfxFeature::serialization,
                reparsed.error,
                TitleGfxErrorCode::verification_failed);
            return result;
        }
        TitleContext after_context{};
        if (auto error = locate_title(
                reparsed.document, after_context, options);
            !error.success()) {
            result.error = semantic_error(
                TitleGfxErrorCode::verification_failed,
                TitleGfxFeature::verification,
                error.detail,
                error.line_index);
            result.error.document_error = error.document_error;
            return result;
        }
        if (after_context.existing_lines.size() != requested.size()) {
            result.error = semantic_error(
                TitleGfxErrorCode::verification_failed,
                TitleGfxFeature::verification,
                1U);
            return result;
        }
        for (std::size_t index = 0; index < requested.size(); ++index) {
            if (!private_line_matches(
                    after_context.existing_lines[index],
                    requested[index],
                    after_context.donor_layout)) {
                result.error = semantic_error(
                    TitleGfxErrorCode::verification_failed,
                    TitleGfxFeature::verification,
                    2U,
                    static_cast<std::uint32_t>(index));
                return result;
            }
        }

        result.after = make_inspection(reparsed.document, after_context);
        result.document = std::move(reparsed.document);
        result.status = TitleGfxTransformStatus::modified;
        return result;
    } catch (...) {
        result.error = semantic_error(
            TitleGfxErrorCode::mutation_failed,
            TitleGfxFeature::document);
        return result;
    }
}

const char* title_gfx_error_name(TitleGfxErrorCode code) noexcept {
    switch (code) {
    case TitleGfxErrorCode::none: return "none";
    case TitleGfxErrorCode::invalid_argument: return "invalid_argument";
    case TitleGfxErrorCode::invalid_document: return "invalid_document";
    case TitleGfxErrorCode::unsupported_document: return "unsupported_document";
    case TitleGfxErrorCode::structure_not_found: return "structure_not_found";
    case TitleGfxErrorCode::ambiguous_structure: return "ambiguous_structure";
    case TitleGfxErrorCode::incompatible_structure:
        return "incompatible_structure";
    case TitleGfxErrorCode::resource_conflict: return "resource_conflict";
    case TitleGfxErrorCode::character_id_exhausted:
        return "character_id_exhausted";
    case TitleGfxErrorCode::depth_exhausted: return "depth_exhausted";
    case TitleGfxErrorCode::mutation_failed: return "mutation_failed";
    case TitleGfxErrorCode::verification_failed: return "verification_failed";
    default: return "unknown";
    }
}

const char* title_gfx_feature_name(TitleGfxFeature feature) noexcept {
    switch (feature) {
    case TitleGfxFeature::none: return "none";
    case TitleGfxFeature::document: return "document";
    case TitleGfxFeature::info_sprite: return "info_sprite";
    case TitleGfxFeature::native_text_fields: return "native_text_fields";
    case TitleGfxFeature::text_line: return "text_line";
    case TitleGfxFeature::character_id: return "character_id";
    case TitleGfxFeature::depth: return "depth";
    case TitleGfxFeature::serialization: return "serialization";
    case TitleGfxFeature::verification: return "verification";
    default: return "unknown";
    }
}

} // namespace erui::gfx
