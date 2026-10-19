#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace erui::test::settings_gfx_fixture {

// This deliberately small writer is test-only. It does not call the
// production document or transformer encoders, so malformed production
// serialization cannot make these fixtures agree with it by construction.
class BitWriter {
public:
    void write(std::uint32_t value, std::size_t count) {
        for (std::size_t remaining = count; remaining != 0; --remaining) {
            if ((bit_position_ % 8U) == 0U) bytes_.push_back(0);
            if (((value >> (remaining - 1U)) & 1U) != 0U) {
                bytes_.back() |= static_cast<std::uint8_t>(
                    1U << (7U - (bit_position_ % 8U)));
            }
            ++bit_position_;
        }
    }

    void write_signed(std::int32_t value, std::size_t count) {
        const std::uint32_t mask = count == 32U
            ? 0xFFFF'FFFFU
            : ((1U << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() {
        while ((bit_position_ % 8U) != 0U) write(0, 1);
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept {
        return bytes_;
    }

private:
    std::vector<std::uint8_t> bytes_{};
    std::size_t bit_position_{};
};

inline void append_u16(
    std::vector<std::uint8_t>& bytes,
    std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

inline void append_u32(
    std::vector<std::uint8_t>& bytes,
    std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

inline void write_u32(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] inline std::size_t signed_bits(std::int32_t value) {
    for (std::size_t bits = 1; bits < 31; ++bits) {
        const std::int32_t minimum = -(1 << (bits - 1U));
        const std::int32_t maximum = (1 << (bits - 1U)) - 1;
        if (value >= minimum && value <= maximum) return bits;
    }
    return 31;
}

[[nodiscard]] inline std::vector<std::uint8_t> matrix(
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

[[nodiscard]] inline std::vector<std::uint8_t> scaled_matrix(
    std::int32_t x,
    std::int32_t y,
    std::int32_t scale_x,
    std::int32_t scale_y) {
    BitWriter writer{};
    writer.write(1, 1); // HasScale.
    const std::size_t scale_bits = std::max(
        signed_bits(scale_x), signed_bits(scale_y));
    writer.write(static_cast<std::uint32_t>(scale_bits), 5);
    writer.write_signed(scale_x, scale_bits);
    writer.write_signed(scale_y, scale_bits);
    writer.write(0, 1); // HasRotate.
    const std::size_t translate_bits = std::max(signed_bits(x), signed_bits(y));
    writer.write(static_cast<std::uint32_t>(translate_bits), 5);
    writer.write_signed(x, translate_bits);
    writer.write_signed(y, translate_bits);
    writer.align();
    return writer.bytes();
}

[[nodiscard]] inline std::vector<std::uint8_t> scaled_rotated_matrix(
    std::int32_t x,
    std::int32_t y,
    std::int32_t scale_x,
    std::int32_t scale_y,
    std::int32_t rotate_skew_0,
    std::int32_t rotate_skew_1) {
    BitWriter writer{};
    writer.write(1, 1); // HasScale.
    const std::size_t scale_bits = std::max(
        signed_bits(scale_x), signed_bits(scale_y));
    writer.write(static_cast<std::uint32_t>(scale_bits), 5);
    writer.write_signed(scale_x, scale_bits);
    writer.write_signed(scale_y, scale_bits);
    writer.write(1, 1); // HasRotate.
    const std::size_t rotate_bits = std::max(
        signed_bits(rotate_skew_0), signed_bits(rotate_skew_1));
    writer.write(static_cast<std::uint32_t>(rotate_bits), 5);
    writer.write_signed(rotate_skew_0, rotate_bits);
    writer.write_signed(rotate_skew_1, rotate_bits);
    const std::size_t translate_bits = std::max(signed_bits(x), signed_bits(y));
    writer.write(static_cast<std::uint32_t>(translate_bits), 5);
    writer.write_signed(x, translate_bits);
    writer.write_signed(y, translate_bits);
    writer.align();
    return writer.bytes();
}

[[nodiscard]] inline std::vector<std::uint8_t> color_transform_with_alpha() {
    BitWriter writer{};
    writer.write(1, 1); // HasAddTerms.
    writer.write(1, 1); // HasMultTerms.
    writer.write(10, 4);
    for (const std::int32_t value : {
             256, 240, 224, 208,
             31, -29, 17, -13}) {
        writer.write_signed(value, 10);
    }
    writer.align();
    return writer.bytes();
}

inline void append_c_string(
    std::vector<std::uint8_t>& bytes,
    std::string_view value) {
    bytes.insert(bytes.end(), value.begin(), value.end());
    bytes.push_back(0);
}

[[nodiscard]] inline std::vector<std::uint8_t> tag(
    std::uint16_t code,
    std::span<const std::uint8_t> body,
    bool force_long = false) {
    std::vector<std::uint8_t> bytes{};
    if (!force_long && body.size() < 0x3FU) {
        append_u16(bytes, static_cast<std::uint16_t>(
            (code << 6U) | static_cast<std::uint16_t>(body.size())));
    } else {
        append_u16(bytes, static_cast<std::uint16_t>((code << 6U) | 0x3FU));
        append_u32(bytes, static_cast<std::uint32_t>(body.size()));
    }
    bytes.insert(bytes.end(), body.begin(), body.end());
    return bytes;
}

inline void append_tag(
    std::vector<std::uint8_t>& output,
    const std::vector<std::uint8_t>& value) {
    output.insert(output.end(), value.begin(), value.end());
}

[[nodiscard]] inline std::vector<std::uint8_t> place_object(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t y,
    std::string_view name,
    std::int32_t x = 0) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x26); // HasCharacter | HasMatrix | HasName.
    append_u16(body, depth);
    append_u16(body, character);
    const auto transform = matrix(x, y);
    body.insert(body.end(), transform.begin(), transform.end());
    body.insert(body.end(), name.begin(), name.end());
    body.push_back(0);
    return tag(26, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> place_object3(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t y,
    std::string_view name,
    std::int32_t x = 0) {
    std::vector<std::uint8_t> body{0x26, 0x00};
    append_u16(body, depth);
    append_u16(body, character);
    const auto transform = matrix(x, y);
    body.insert(body.end(), transform.begin(), transform.end());
    append_c_string(body, name);
    return tag(70, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> place_object_with_opaque_tail(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t y,
    std::string_view name,
    std::span<const std::uint8_t> opaque_tail,
    std::int32_t x = 0) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x26);
    append_u16(body, depth);
    append_u16(body, character);
    const auto transform = matrix(x, y);
    body.insert(body.end(), transform.begin(), transform.end());
    append_c_string(body, name);
    body.insert(body.end(), opaque_tail.begin(), opaque_tail.end());
    return tag(26, body, true);
}

// Mirrors the authored BackTabList form: PlaceObject2 with a full matrix,
// CXFORMWITHALPHA, a name, and opaque bytes after the standard fields. Keeping
// this writer independent from production makes exact clone comparisons able
// to catch matrix/color/tail loss in the transformer.
[[nodiscard]] inline std::vector<std::uint8_t>
place_back_tab_object_with_opaque_tail(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t y,
    std::string_view name,
    std::span<const std::uint8_t> opaque_tail,
    std::int32_t x = 0) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x2E); // Character | Matrix | ColorTransform | Name.
    append_u16(body, depth);
    append_u16(body, character);
    const auto transform = scaled_rotated_matrix(
        x, y, 65'536, 61'440, 2'048, -1'024);
    body.insert(body.end(), transform.begin(), transform.end());
    const auto color = color_transform_with_alpha();
    body.insert(body.end(), color.begin(), color.end());
    append_c_string(body, name);
    body.insert(body.end(), opaque_tail.begin(), opaque_tail.end());
    return tag(26, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> place_object_unnamed(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t x = 0,
    std::int32_t y = 0) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x06); // HasCharacter | HasMatrix.
    append_u16(body, depth);
    append_u16(body, character);
    const auto transform = matrix(x, y);
    body.insert(body.end(), transform.begin(), transform.end());
    return tag(26, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> move_object(
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y = 0) {
    std::vector<std::uint8_t> body{0x05}; // Move | HasMatrix.
    append_u16(body, depth);
    const auto transform = matrix(x, y);
    body.insert(body.end(), transform.begin(), transform.end());
    return tag(26, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> define_sprite(
    std::uint16_t id,
    const std::vector<std::vector<std::uint8_t>>& children) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    append_u16(body, 1);
    for (const auto& child : children) append_tag(body, child);
    append_tag(body, tag(1, {})); // ShowFrame.
    append_tag(body, tag(0, {})); // End.
    return tag(39, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> define_sprite_with_timeline(
    std::uint16_t id,
    const std::vector<std::vector<std::uint8_t>>& children,
    std::uint16_t frame_count,
    std::uint16_t show_frame_count) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    append_u16(body, frame_count);
    for (const auto& child : children) append_tag(body, child);
    for (std::uint16_t index = 0; index < show_frame_count; ++index) {
        append_tag(body, tag(1, {}));
    }
    append_tag(body, tag(0, {}));
    return tag(39, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> define_text_stub(
    std::uint16_t id,
    bool placeholder) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    if (placeholder) {
        constexpr std::array<std::uint8_t, 4> gray{
            0x50, 0x50, 0x50, 0xFF};
        body.insert(body.end(), gray.begin(), gray.end());
        constexpr std::string_view markup =
            "<font color=\"#505050\">placeholder</font>";
        body.insert(body.end(), markup.begin(), markup.end());
    }
    body.push_back(0);
    return tag(37, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> import_assets(
    std::string_view url,
    std::span<const std::uint16_t> character_ids) {
    std::vector<std::uint8_t> body{};
    append_c_string(body, url);
    append_u16(body, static_cast<std::uint16_t>(character_ids.size()));
    for (const std::uint16_t id : character_ids) {
        append_u16(body, id);
        append_c_string(body, "Imported_" + std::to_string(id));
    }
    return tag(57, body, true); // ImportAssets.
}

[[nodiscard]] inline std::vector<std::uint8_t> external_image(
    std::uint16_t id,
    std::uint16_t width,
    std::uint16_t height,
    std::string_view export_name,
    std::string_view file_name) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    append_u16(body, 0); // IDTYPE_NONE: this resource owns a character ID.
    body.push_back(13);
    body.push_back(0);
    append_u16(body, width);
    append_u16(body, height);
    body.push_back(static_cast<std::uint8_t>(export_name.size()));
    body.insert(body.end(), export_name.begin(), export_name.end());
    body.push_back(static_cast<std::uint8_t>(file_name.size()));
    body.insert(body.end(), file_name.begin(), file_name.end());
    return tag(1009, body);
}

[[nodiscard]] inline std::vector<std::uint8_t> image_wrapper(
    std::uint16_t wrapper_id,
    std::uint16_t image_id,
    std::int32_t x,
    std::int32_t y,
    std::int32_t scale_x,
    std::int32_t scale_y) {
    std::vector<std::uint8_t> placement{0x06, 0x10};
    append_u16(placement, 1);
    append_u16(placement, image_id);
    const auto transform = scaled_matrix(x, y, scale_x, scale_y);
    placement.insert(placement.end(), transform.begin(), transform.end());
    return define_sprite(
        wrapper_id,
        {tag(70, placement, true)});
}

[[nodiscard]] inline std::vector<std::uint8_t> unscaled_image_wrapper(
    std::uint16_t wrapper_id,
    std::uint16_t image_id,
    std::int32_t x,
    std::int32_t y) {
    std::vector<std::uint8_t> placement{0x06, 0x10};
    append_u16(placement, 1);
    append_u16(placement, image_id);
    const auto transform = matrix(x, y);
    placement.insert(placement.end(), transform.begin(), transform.end());
    return define_sprite(
        wrapper_id,
        {tag(70, placement, true)});
}

[[nodiscard]] inline std::vector<std::uint8_t> scaling_grid(
    std::uint16_t wrapper_id,
    std::size_t bits,
    std::array<std::int32_t, 4> rectangle) {
    std::vector<std::uint8_t> body{};
    append_u16(body, wrapper_id);
    BitWriter writer{};
    writer.write(static_cast<std::uint32_t>(bits), 5);
    for (const std::int32_t value : rectangle) {
        writer.write_signed(value, bits);
    }
    writer.align();
    body.insert(body.end(), writer.bytes().begin(), writer.bytes().end());
    return tag(78, body);
}

[[nodiscard]] inline std::vector<std::uint8_t> color_fill_shape(
    std::uint16_t id) {
    constexpr std::array<std::uint8_t, 29> tail{
        0x5D, 0xA8, 0x4B, 0x1B, 0x50, 0x96, 0x00, 0x01,
        0x00, 0x00, 0x99, 0xCC, 0x00, 0x10, 0x15, 0x76,
        0xA3, 0x6A, 0x7A, 0x12, 0xC3, 0x95, 0x2C, 0x74,
        0x5A, 0x87, 0x2D, 0xA8, 0x00};
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    body.insert(body.end(), tail.begin(), tail.end());
    return tag(2, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> single_child_sprite(
    std::uint16_t id,
    std::uint16_t child) {
    std::vector<std::uint8_t> placement{0x06};
    append_u16(placement, 1);
    append_u16(placement, child);
    placement.push_back(0); // Identity matrix.
    return define_sprite(id, {tag(26, placement)});
}

[[nodiscard]] inline std::vector<std::uint8_t> transformed_placement(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t x,
    std::int32_t y,
    std::int32_t scale_x,
    std::int32_t scale_y,
    std::span<const std::int32_t> multiply,
    std::span<const std::int32_t> add = {}) {
    std::vector<std::uint8_t> body{0x0E};
    append_u16(body, depth);
    append_u16(body, character);
    const auto transform = scaled_matrix(x, y, scale_x, scale_y);
    body.insert(body.end(), transform.begin(), transform.end());
    BitWriter color{};
    color.write(add.empty() ? 0U : 1U, 1);
    color.write(multiply.empty() ? 0U : 1U, 1);
    color.write(10, 4);
    for (const std::int32_t value : multiply) color.write_signed(value, 10);
    for (const std::int32_t value : add) color.write_signed(value, 10);
    color.align();
    body.insert(body.end(), color.bytes().begin(), color.bytes().end());
    return tag(26, body);
}

[[nodiscard]] inline std::vector<std::uint8_t> color_fill_placement(
    std::uint16_t fill_id,
    std::uint16_t depth = 3) {
    std::vector<std::uint8_t> body{0x26};
    append_u16(body, depth);
    append_u16(body, fill_id);
    const auto transform = scaled_matrix(-412, -325, 414260, 49807);
    body.insert(body.end(), transform.begin(), transform.end());
    append_c_string(body, "Color");
    return tag(26, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> native_named_placement(
    std::uint16_t character,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    std::vector<std::uint8_t> body{0x26};
    append_u16(body, depth);
    append_u16(body, character);
    BitWriter transform{};
    transform.write(0, 1);
    transform.write(0, 1);
    transform.write(15, 5);
    transform.write(static_cast<std::uint32_t>(x) & 0x7FFFU, 15);
    transform.write(static_cast<std::uint32_t>(y) & 0x7FFFU, 15);
    transform.align();
    body.insert(
        body.end(), transform.bytes().begin(), transform.bytes().end());
    append_c_string(body, name);
    return tag(26, body);
}

[[nodiscard]] inline std::vector<std::uint8_t> transparent_placement(
    std::uint16_t character,
    std::uint16_t depth,
    std::string_view name) {
    std::vector<std::uint8_t> body{0x2E};
    append_u16(body, depth);
    append_u16(body, character);
    body.push_back(0); // Identity matrix.
    BitWriter color{};
    color.write(0, 1);
    color.write(1, 1);
    color.write(10, 4);
    for (const std::int32_t value : {256, 256, 256, 0}) {
        color.write_signed(value, 10);
    }
    color.align();
    body.insert(body.end(), color.bytes().begin(), color.bytes().end());
    append_c_string(body, name);
    return tag(26, body, true);
}

[[nodiscard]] inline std::vector<std::uint8_t> legacy_preview_sprite(
    std::uint16_t id,
    std::uint16_t character_frame,
    std::uint16_t fill,
    std::uint16_t foreground) {
    constexpr std::array<std::int32_t, 4> bracket_multiply{225, 225, 225, 256};
    constexpr std::array<std::int32_t, 4> foreground_multiply{230, 230, 230, 256};
    constexpr std::array<std::int32_t, 4> foreground_add{26, 26, 26, 0};
    return define_sprite(
        id,
        {
            transformed_placement(
                1, character_frame, -487, -320, 65536, 31457,
                bracket_multiply),
            color_fill_placement(fill),
            transformed_placement(
                5, foreground, -487, -330, 692638, 65536,
                foreground_multiply, foreground_add),
        });
}

[[nodiscard]] inline std::vector<std::uint8_t> hidden_legacy_preview(
    std::uint16_t id,
    std::uint16_t depth) {
    std::vector<std::uint8_t> body{0x26, 0x20};
    append_u16(body, depth);
    append_u16(body, id);
    body.push_back(0); // Identity matrix.
    append_c_string(body, "ColorPreview");
    body.push_back(0); // Visible = false.
    return tag(70, body);
}

[[nodiscard]] inline std::vector<std::uint8_t> hidden_color_picker(
    std::uint16_t id) {
    std::vector<std::uint8_t> body{0x26, 0x20};
    append_u16(body, 2);
    append_u16(body, id);
    body.push_back(0); // Identity matrix.
    append_c_string(body, "ColorPicker");
    body.push_back(0); // Visible = false.
    return tag(70, body);
}

[[nodiscard]] inline std::vector<std::uint8_t> character_frame_placement(
    std::uint16_t wrapper_id) {
    constexpr std::array<std::int32_t, 4> multiply{225, 225, 225, 256};
    std::vector<std::uint8_t> body{0x2E};
    append_u16(body, 2);
    append_u16(body, wrapper_id);
    const auto transform = scaled_matrix(-487, -320, 65536, 31457);
    body.insert(body.end(), transform.begin(), transform.end());
    BitWriter color{};
    color.write(0, 1);
    color.write(1, 1);
    color.write(10, 4);
    for (const std::int32_t value : multiply) color.write_signed(value, 10);
    color.align();
    body.insert(body.end(), color.bytes().begin(), color.bytes().end());
    append_c_string(body, "CharacterFrame");
    return tag(26, body);
}

struct CharacterIds {
    std::uint16_t game_options{31'001};
    std::uint16_t camera_options{31'017};
    std::uint16_t display_sound{31'019};
    std::uint16_t network{31'021};
    std::uint16_t keyboard_mouse{31'023};
    std::uint16_t graphics{31'025};
    std::uint16_t row_donor{31'109};
    std::uint16_t window_list{31'211};
    std::uint16_t second_window_list{31'213};
    std::uint16_t row_item{32'003};
    std::uint16_t native_row_item{32'005};
    std::uint16_t ugc_restricted{32'007};
    std::uint16_t button{32'101};
    std::uint16_t widgets{32'209};
    std::uint16_t text_input{32'303};
    std::uint16_t text_value{32'401};
    std::uint16_t text_placeholder{32'503};
    std::uint16_t caption_wrapper{32'609};
    std::uint16_t caption_text{32'707};
    std::uint16_t cursor{32'801};
    std::uint16_t combo_box{32'903};
    std::uint16_t slider{33'001};
    std::uint16_t button_background{33'101};
    std::uint16_t button_frame{33'103};
    // These IDs are intentionally numerically lower than the primary host,
    // while their definitions are emitted later. That lets tests distinguish
    // character-ID ordering from authored top-level ordering.
    std::uint16_t second_button{30'101};
    std::uint16_t second_widgets{30'209};
    std::uint16_t second_button_background{30'301};
    std::uint16_t second_button_frame{30'303};
    std::uint16_t character_frame_image{34'001};
    std::uint16_t character_frame_wrapper{34'003};
    std::uint16_t color_frame_image{34'005};
    std::uint16_t color_fill_shape{34'007};
    std::uint16_t color_fill{34'009};
    std::uint16_t color_foreground{34'011};
    std::uint16_t legacy_preview{34'013};
    std::uint16_t back_tab_item{35'001};
    std::uint16_t back_tab_list{35'003};
    std::uint16_t tab_item{35'005};
    std::uint16_t tab_list{35'007};
    std::uint16_t specialized_back_tab_item{35'009};
    std::uint16_t specialized_tab_item{35'011};
    std::uint16_t right_arrow{35'013};
    std::uint16_t shared_widget_first{44'001};
    std::uint16_t nested_decoy{60'001};
    std::uint16_t nested_character_frame_decoy{60'003};
};

struct Options {
    CharacterIds ids{};
    std::uint16_t game_rows{6};
    std::uint16_t camera_rows{7};
    std::uint16_t donor_last_row{12};
    bool include_all_settings_panels{};
    std::uint16_t game_panel_frame_count{1};
    std::uint16_t game_panel_show_frame_count{1};
    bool omit_camera_panel{};
    bool duplicate_game_panel{};
    bool split_panel_carriers{};
    bool gap_game_rows{};
    bool occupy_next_game_row_depth{};
    bool include_nested_settings_decoy{};
    bool include_nested_character_frame_decoy{};
    bool include_second_widget_host{};
    std::uint16_t additional_shared_widget_hosts{};
    std::uint16_t tab_cells{9};
    std::optional<std::uint16_t> specialized_tab_cell{};
    bool decorated_back_tab_cells{};
    std::int32_t tab_first_x{-9760};
    std::int32_t tab_step_x{1800};
    bool mismatch_tab_layer_geometry{};
    bool nonuniform_tab_spacing{};
    bool noncanonical_front_tab_depth_stride{};
    bool omit_tab_text{};
    bool extra_front_tab_decoration{};
    bool gap_tab_cells{};
    bool duplicate_tab_list{};
    std::size_t button_background_opaque_tail_bytes{};
    bool noncanonical_donor_row{};
    bool omit_button_label{};
    bool occupy_button_label_depth{};
    // Advanced Settings binds Button's value and label names in the opposite
    // order from the Game Options movie. This creates an independently
    // encoded widget-only host for runtime-policy tests without deriving a
    // fixture from production transformer output.
    bool advanced_widget_bindings{};
    std::uint16_t button_frame_count{1};
    std::uint16_t button_show_frame_count{1};
    std::optional<std::uint16_t> legacy_color_preview_depth{};
    bool legacy_button_frame_place_object3{};
    std::vector<std::uint16_t> imported_character_ids{};
    bool include_unknown_tags{true};
    std::vector<std::uint8_t> trailer{0xA7, 0x00, 0x5C, 0xE1, 0x19};
};

inline constexpr std::uint16_t unknown_root_tag_code = 901;
inline constexpr std::uint16_t unknown_nested_tag_code = 902;
inline constexpr std::array<std::uint8_t, 7> unknown_root_body{
    0xD1, 0x5A, 0xF0, 0x0D, 0x7E, 0x31, 0xC4};
inline constexpr std::array<std::uint8_t, 8> unknown_nested_body{
    0xB4, 0x29, 0x88, 0x03, 0x6D, 0xFE, 0x11, 0x72};

[[nodiscard]] inline std::vector<std::uint8_t> make(const Options& options = {}) {
    const CharacterIds& id = options.ids;

    std::vector<std::vector<std::uint8_t>> game_items{};
    for (std::uint16_t index = 0; index < options.game_rows; ++index) {
        const std::uint16_t logical_index =
            options.gap_game_rows && index >= 2
                ? static_cast<std::uint16_t>(index + 1U)
                : index;
        game_items.push_back(place_object(
            static_cast<std::uint16_t>(logical_index * 2U + 1U),
            id.row_item,
            -4800 + static_cast<std::int32_t>(logical_index) * 1000,
            "Item_" + std::to_string(logical_index) + "_0"));
    }
    if (options.occupy_next_game_row_depth) {
        game_items.push_back(place_object(
            static_cast<std::uint16_t>(options.game_rows * 2U + 1U),
            id.cursor,
            0,
            "ThirdPartyAtNextRowDepth"));
    }

    std::vector<std::vector<std::uint8_t>> camera_items{};
    for (std::uint16_t index = 0; index < options.camera_rows; ++index) {
        camera_items.push_back(place_object(
            static_cast<std::uint16_t>(index * 2U + 1U),
            id.row_item,
            -4800 + static_cast<std::int32_t>(index) * 1000,
            "Item_" + std::to_string(index) + "_0"));
    }

    std::vector<std::vector<std::uint8_t>> display_sound_items{};
    std::vector<std::vector<std::uint8_t>> network_items{};
    std::vector<std::vector<std::uint8_t>> keyboard_mouse_items{};
    std::vector<std::vector<std::uint8_t>> graphics_items{};
    if (options.include_all_settings_panels) {
        for (std::uint16_t index = 0; index < 12; ++index) {
            display_sound_items.push_back(place_object(
                static_cast<std::uint16_t>(23U - index * 2U),
                id.row_item,
                -4800 + static_cast<std::int32_t>(index) * 1000,
                "Item_" + std::to_string(index) + "_0"));
        }
        network_items.push_back(place_object(
            1, id.ugc_restricted, 6000, "UgcRestricted"));
        for (std::uint16_t index = 0; index < 7; ++index) {
            network_items.push_back(place_object(
                static_cast<std::uint16_t>(3U + index * 2U),
                id.native_row_item,
                -4800 + static_cast<std::int32_t>(index) * 1000,
                "Item_" + std::to_string(index) + "_0"));
        }
        for (std::uint16_t index = 0; index < 7; ++index) {
            const std::uint16_t character = index == 6
                ? id.row_item
                : id.native_row_item;
            const auto item = place_object(
                static_cast<std::uint16_t>(13U - index * 2U),
                character,
                -4800 + static_cast<std::int32_t>(index) * 1000,
                "Item_" + std::to_string(index) + "_0");
            keyboard_mouse_items.push_back(item);
            graphics_items.push_back(item);
        }
    }

    std::vector<std::vector<std::uint8_t>> donor_items{};
    const std::uint16_t first_donor = std::min(
        options.game_rows, options.camera_rows);
    for (std::uint32_t index = first_donor;
         index <= options.donor_last_row;
         ++index) {
        const auto name = "Item_" + std::to_string(index) + "_0";
        if (options.noncanonical_donor_row && index == 7U) {
            constexpr std::array<std::uint8_t, 5> opaque_tail{
                0xD6, 0x4A, 0x91, 0x00, 0xEF};
            donor_items.push_back(place_object_with_opaque_tail(
                static_cast<std::uint16_t>(100U + index),
                id.row_item,
                -4800 + static_cast<std::int32_t>(index) * 1000,
                name,
                opaque_tail));
        } else {
            donor_items.push_back(place_object(
                static_cast<std::uint16_t>(100U + index),
                id.row_item,
                -4800 + static_cast<std::int32_t>(index) * 1000,
                name));
        }
    }
    if (options.include_unknown_tags) {
        donor_items.insert(
            donor_items.begin() + static_cast<std::ptrdiff_t>(
                donor_items.size() / 2U),
            tag(unknown_nested_tag_code, unknown_nested_body, true));
    }
    if (options.include_nested_settings_decoy) {
        // A recursively indexed DefineSprite whose authored names resemble
        // real settings structures. It is a character nested inside another
        // definition, not a top-level library definition, so the semantic
        // transformer must preserve it without treating it as another host.
        const std::vector<std::vector<std::uint8_t>> decoy_items{
            place_object(20, id.game_options, 0, "ControllSetting"),
            place_object(21, id.camera_options, 0, "CameraSetting"),
            place_object(3, id.text_value, -640, "Text_0", -4360),
            place_object(4, id.text_placeholder, -640, "TextOnEmpty", -4360),
            place_object(5, id.caption_wrapper, 0, "Caption", -9520),
            place_object(7, id.cursor, 0, "Cursor")};
        donor_items.push_back(define_sprite(id.nested_decoy, decoy_items));
    }
    if (options.include_nested_character_frame_decoy) {
        // This is the exact native CharacterFrame placement contract, but it
        // belongs to a nested dictionary sprite rather than a top-level
        // library definition. It must remain invisible to semantic discovery.
        donor_items.push_back(define_sprite(
            id.nested_character_frame_decoy,
            {character_frame_placement(id.character_frame_wrapper)}));
    }

    std::vector<std::vector<std::uint8_t>> window_items{};
    window_items.push_back(place_object(
        45, id.game_options, 0, "ControllSetting"));
    if (options.duplicate_game_panel) {
        window_items.push_back(place_object(
            46, id.row_donor, 0, "ControllSetting"));
    }
    if (!options.omit_camera_panel && !options.split_panel_carriers) {
        window_items.push_back(place_object(
            47, id.camera_options, 0, "CameraSetting"));
    }
    if (options.include_all_settings_panels) {
        window_items.push_back(place_object(
            48, id.display_sound, 0, "EnvironmentSetting"));
        window_items.push_back(place_object(
            49, id.display_sound, 0, "AudioSetting"));
        window_items.push_back(place_object(
            50, id.network, 0, "NetworkSetting"));
        window_items.push_back(place_object(
            51, id.keyboard_mouse, 0, "PCMouseKey"));
        window_items.push_back(place_object(
            52, id.graphics, 0, "PCGraphic"));
    }
    std::vector<std::vector<std::uint8_t>> second_window_items{};
    if (!options.omit_camera_panel && options.split_panel_carriers) {
        second_window_items.push_back(place_object(
            47, id.camera_options, 0, "CameraSetting"));
    }

    std::vector<std::vector<std::uint8_t>> caption_items{
        place_object(1, id.caption_text, 0, "Text_0", -4360)};

    const std::int32_t text_input_caption_x =
        options.advanced_widget_bindings ? -9120 : -9520;
    std::vector<std::vector<std::uint8_t>> text_input_items{
        place_object(3, id.text_value, -640, "Text_0", -4360),
        place_object(4, id.text_placeholder, -640, "TextOnEmpty", -4360),
        place_object(
            5, id.caption_wrapper, 0, "Caption", text_input_caption_x),
        place_object(7, id.cursor, 0, "Cursor")};

    std::vector<std::uint8_t> opaque_background_tail(
        options.button_background_opaque_tail_bytes, 0xA5);
    const auto button_background = opaque_background_tail.empty()
        ? place_object_unnamed(1, id.button_background)
        : place_object_with_opaque_tail(
            1,
            id.button_background,
            0,
            "ThirdPartyBackground",
            opaque_background_tail);
    const auto button_frame = options.legacy_color_preview_depth
        ? (options.legacy_button_frame_place_object3
              ? place_object3(3, id.button_frame, 0, "ButtonFrame")
              : place_object(3, id.button_frame, 0, "ButtonFrame"))
        : place_object_unnamed(3, id.button_frame);
    const std::string_view button_value_name =
        options.advanced_widget_bindings ? "Text_1" : "Text_0";
    const std::string_view button_label_name =
        options.advanced_widget_bindings ? "Text_0" : "Text_1";
    std::vector<std::vector<std::uint8_t>> button_items{
        button_background,
        button_frame,
        place_object(6, id.cursor, 0, "Cursor"),
        place_object(10, id.text_value, 0, button_value_name)};
    if (options.occupy_button_label_depth) {
        button_items.push_back(place_object(
            12, id.text_placeholder, 0, "OccupiedByThirdParty"));
    } else if (!options.omit_button_label) {
        button_items.push_back(place_object(
            12, id.caption_wrapper, 0, button_label_name));
    }
    if (options.legacy_color_preview_depth) {
        button_items.push_back(hidden_legacy_preview(
            id.legacy_preview, *options.legacy_color_preview_depth));
    }

    std::vector<std::vector<std::uint8_t>> widget_items{
        place_object(1, id.button, 0, "Button"),
        place_object(12, id.text_input, 0, "TextInput"),
        place_object(20, id.combo_box, 0, "ComboBox"),
        place_object(34, id.slider, 0, "Slider")};

    std::vector<std::vector<std::uint8_t>> second_button_items{
        place_object_unnamed(1, id.second_button_background),
        place_object_unnamed(3, id.second_button_frame),
        place_object(6, id.cursor, 0, "Cursor"),
        place_object(10, id.text_value, 0, button_value_name),
        place_object(12, id.caption_wrapper, 0, button_label_name)};
    std::vector<std::vector<std::uint8_t>> second_widget_items{
        place_object(1, id.second_button, 0, "Button"),
        place_object(12, id.text_input, 0, "TextInput"),
        place_object(20, id.combo_box, 0, "ComboBox"),
        place_object(34, id.slider, 0, "Slider")};

    std::vector<std::vector<std::uint8_t>> shared_widget_definitions{};
    shared_widget_definitions.reserve(options.additional_shared_widget_hosts);
    for (std::uint32_t index = 0;
         index < options.additional_shared_widget_hosts;
         ++index) {
        const auto character_id = static_cast<std::uint16_t>(
            id.shared_widget_first + index);
        shared_widget_definitions.push_back(define_sprite(
            character_id,
            widget_items));
    }

    std::vector<std::vector<std::uint8_t>> back_tab_items{};
    std::vector<std::vector<std::uint8_t>> tab_items{};
    constexpr std::array<std::uint8_t, 2> back_tab_tail{0xA5, 0x5A};
    constexpr std::array<std::uint8_t, 2> tab_tail{0x3C, 0xC3};
    back_tab_items.reserve(static_cast<std::size_t>(options.tab_cells) + 1U);
    tab_items.reserve(static_cast<std::size_t>(options.tab_cells) + 3U);

    // Mirror the real 02_040_optionsetting.gfx timelines. The BackTabList
    // reserves depth 1 before its 4-depth cell lattice. TabList reserves
    // depths 1 and 3 for the arrows, uses 6-depth cell blocks beginning at
    // depth 5, and keeps Text_0 in the block immediately above Item_0_0.
    // Logical item names and x positions ascend while tag/depth order runs in
    // reverse. This asymmetry is significant to the Scaleform movie and must
    // not be simplified by the test fixture.
    back_tab_items.push_back(place_object_unnamed(
        1, id.back_tab_item));
    tab_items.push_back(place_object(
        1, id.tab_item, -6640, "LeftArrow", -11260));
    tab_items.push_back(place_object(
        3, id.right_arrow, -6640, "RightArrow", -8260));

    for (std::uint16_t remaining = options.tab_cells;
         remaining != 0U;
         --remaining) {
        const std::uint16_t index = static_cast<std::uint16_t>(remaining - 1U);
        const std::uint16_t logical_index =
            options.gap_tab_cells && index >= 2U
                ? static_cast<std::uint16_t>(index + 1U)
                : index;
        const auto spacing_adjustment =
            options.nonuniform_tab_spacing && index == 4U ? 1 : 0;
        const auto x = static_cast<std::int32_t>(
            static_cast<std::int64_t>(options.tab_first_x) +
            static_cast<std::int64_t>(logical_index) *
                options.tab_step_x + spacing_adjustment);
        const auto back_x = options.mismatch_tab_layer_geometry && index == 4U
            ? x + 1
            : x;
        const bool specialized =
            options.specialized_tab_cell == index;
        const auto name = "Item_0_" + std::to_string(logical_index);
        const auto depth_rank = static_cast<std::uint16_t>(
            options.tab_cells - 1U - index);
        back_tab_items.push_back(
            options.decorated_back_tab_cells
                ? place_back_tab_object_with_opaque_tail(
                    static_cast<std::uint16_t>(2U + depth_rank * 4U),
                    specialized
                        ? id.specialized_back_tab_item
                        : id.back_tab_item,
                    -6920,
                    name,
                    back_tab_tail,
                    back_x)
                : place_object_with_opaque_tail(
                    static_cast<std::uint16_t>(2U + depth_rank * 4U),
                    specialized
                        ? id.specialized_back_tab_item
                        : id.back_tab_item,
                    -6920,
                    name,
                    back_tab_tail,
                    back_x));
        const std::uint16_t front_depth_stride =
            options.noncanonical_front_tab_depth_stride ? 5U : 6U;
        tab_items.push_back(place_object_with_opaque_tail(
            static_cast<std::uint16_t>(
                5U + depth_rank * front_depth_stride),
            specialized ? id.specialized_tab_item : id.tab_item,
            -6920,
            name,
            tab_tail,
            x));
    }
    if (!options.omit_tab_text) {
        const std::uint16_t front_depth_stride =
            options.noncanonical_front_tab_depth_stride ? 5U : 6U;
        tab_items.push_back(place_object3(
            static_cast<std::uint16_t>(
                5U + options.tab_cells * front_depth_stride),
            id.tab_item,
            -8520,
            "Text_0",
            -11660));
    }
    if (options.extra_front_tab_decoration) {
        tab_items.push_back(place_object_unnamed(
            static_cast<std::uint16_t>(
                6U + options.tab_cells * 6U),
            id.tab_item));
    }

    std::vector<std::uint8_t> movie{'G', 'F', 'X', 11, 0, 0, 0, 0};
    BitWriter rectangle{};
    rectangle.write(1, 5);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.align();
    movie.insert(
        movie.end(), rectangle.bytes().begin(), rectangle.bytes().end());
    append_u16(movie, 0); // Frame rate.
    append_u16(movie, 1); // Frame count.

    if (options.include_unknown_tags) {
        append_tag(movie, tag(unknown_root_tag_code, unknown_root_body, true));
    }
    if (!options.imported_character_ids.empty()) {
        append_tag(movie, import_assets(
            "shared-fixture.gfx", options.imported_character_ids));
    }
    if (options.legacy_color_preview_depth) {
        append_tag(movie, external_image(
            id.character_frame_image, 428, 108,
            "MENU_FL_Cursor_EntWaku", "MENU_FL_Cursor_EntWaku.tga"));
        append_tag(movie, unscaled_image_wrapper(
            id.character_frame_wrapper, id.character_frame_image,
            -4193, -1080));
        append_tag(movie, scaling_grid(
            id.character_frame_wrapper, 9, {-80, 80, -160, 180}));
        append_tag(movie, external_image(
            id.color_frame_image, 74, 66,
            "MENU_FL_ColorWaku", "MENU_FL_ColorWaku.tga"));
        append_tag(movie, color_fill_shape(id.color_fill_shape));
        append_tag(movie, single_child_sprite(
            id.color_fill, id.color_fill_shape));
        append_tag(movie, image_wrapper(
            id.color_foreground, id.color_frame_image,
            -363, -325, 32768, 32768));
        append_tag(movie, scaling_grid(
            id.color_foreground, 8, {-100, 100, -100, 100}));
        append_tag(movie, legacy_preview_sprite(
            id.legacy_preview,
            id.character_frame_wrapper,
            id.color_fill,
            id.color_foreground));
    }
    append_tag(movie, define_sprite_with_timeline(
        id.button,
        button_items,
        options.button_frame_count,
        options.button_show_frame_count));
    append_tag(movie, define_sprite(id.widgets, widget_items));
    if (options.include_second_widget_host) {
        // These definitions come later despite their smaller character IDs.
        append_tag(movie, define_sprite(
            id.second_button, second_button_items));
        append_tag(movie, define_sprite(
            id.second_widgets, second_widget_items));
    }
    for (const auto& definition : shared_widget_definitions) {
        append_tag(movie, definition);
    }
    append_tag(movie, define_sprite(id.back_tab_item, {}));
    append_tag(movie, define_sprite(id.tab_item, {}));
    append_tag(movie, define_sprite(id.specialized_back_tab_item, {}));
    append_tag(movie, define_sprite(id.specialized_tab_item, {}));
    append_tag(movie, define_sprite(id.right_arrow, {}));
    append_tag(movie, define_sprite(id.back_tab_list, back_tab_items));
    append_tag(movie, define_sprite(id.tab_list, tab_items));
    append_tag(movie, define_text_stub(id.text_value, false));
    append_tag(movie, define_text_stub(id.text_placeholder, true));
    append_tag(movie, define_sprite(id.caption_wrapper, caption_items));
    append_tag(movie, define_sprite(id.text_input, text_input_items));
    append_tag(movie, define_sprite_with_timeline(
        id.game_options,
        game_items,
        options.game_panel_frame_count,
        options.game_panel_show_frame_count));
    if (!options.omit_camera_panel) {
        append_tag(movie, define_sprite(id.camera_options, camera_items));
    }
    if (options.include_all_settings_panels) {
        append_tag(movie, define_sprite(
            id.display_sound, display_sound_items));
        append_tag(movie, define_sprite(id.network, network_items));
        append_tag(movie, define_sprite(
            id.keyboard_mouse, keyboard_mouse_items));
        append_tag(movie, define_sprite(id.graphics, graphics_items));
    }
    append_tag(movie, define_sprite(id.row_donor, donor_items));
    append_tag(movie, define_sprite(id.window_list, window_items));
    if (options.split_panel_carriers) {
        append_tag(movie, define_sprite(
            id.second_window_list,
            second_window_items));
    }
    append_tag(movie, place_object(
        156, id.back_tab_list, 10800, "BackTabList", 19200));
    append_tag(movie, place_object(
        191, id.tab_list, 10800, "TabList", 19200));
    if (options.duplicate_tab_list) {
        append_tag(movie, place_object(
            192, id.tab_list, 10800, "TabList", 19200));
    }
    append_tag(movie, tag(1, {}));
    append_tag(movie, tag(0, {}));
    write_u32(movie, 4, static_cast<std::uint32_t>(movie.size()));
    movie.insert(movie.end(), options.trailer.begin(), options.trailer.end());
    return movie;
}

} // namespace erui::test::settings_gfx_fixture
