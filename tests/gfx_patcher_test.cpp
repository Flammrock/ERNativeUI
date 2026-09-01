#include "gfx_patch.hpp"

#include <algorithm>
#include <array>
#include "test_assertions.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

namespace {

class BitWriter {
public:
    void write(std::uint32_t value, std::size_t count) {
        for (std::size_t bit = count; bit != 0; --bit) {
            const bool set = ((value >> (bit - 1)) & 1U) != 0;
            if ((bit_position_ % 8) == 0) {
                bytes_.push_back(0);
            }
            if (set) {
                bytes_.back() |= static_cast<std::uint8_t>(1U << (7 - (bit_position_ % 8)));
            }
            ++bit_position_;
        }
    }

    void write_signed(std::int32_t value, std::size_t count) {
        const std::uint32_t mask = count == 32
            ? 0xFFFF'FFFFU
            : ((1U << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() {
        while ((bit_position_ % 8) != 0) {
            write(0, 1);
        }
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
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
}

[[nodiscard]] std::size_t signed_bits(std::int32_t value) {
    for (std::size_t bits = 1; bits < 31; ++bits) {
        const std::int32_t minimum = -(1 << (bits - 1));
        const std::int32_t maximum = (1 << (bits - 1)) - 1;
        if (value >= minimum && value <= maximum) {
            return bits;
        }
    }
    return 31;
}

[[nodiscard]] std::vector<std::uint8_t> matrix(std::int32_t x, std::int32_t y) {
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
    std::span<const std::uint8_t> body,
    bool force_long = false) {
    std::vector<std::uint8_t> output{};
    if (!force_long && body.size() < 0x3F) {
        append_u16(output, static_cast<std::uint16_t>((code << 6) | body.size()));
    } else {
        append_u16(output, static_cast<std::uint16_t>((code << 6) | 0x3F));
        append_u32(output, static_cast<std::uint32_t>(body.size()));
    }
    output.insert(output.end(), body.begin(), body.end());
    return output;
}

void append_tag(std::vector<std::uint8_t>& output, const std::vector<std::uint8_t>& value) {
    output.insert(output.end(), value.begin(), value.end());
}

[[nodiscard]] std::vector<std::uint8_t> place_object(
    std::uint16_t depth,
    std::uint16_t character,
    std::int32_t y,
    std::string_view name,
    std::int32_t x = 0) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x26); // Character + Matrix + Name.
    append_u16(body, depth);
    append_u16(body, character);
    const std::vector<std::uint8_t> transform = matrix(x, y);
    body.insert(body.end(), transform.begin(), transform.end());
    body.insert(body.end(), name.begin(), name.end());
    body.push_back(0);
    return tag(26, body, true);
}

[[nodiscard]] std::vector<std::uint8_t> define_sprite(
    std::uint16_t id,
    const std::vector<std::vector<std::uint8_t>>& children) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    append_u16(body, 1);
    for (const auto& child : children) {
        append_tag(body, child);
    }
    append_tag(body, tag(1, {}));
    append_tag(body, tag(0, {}));
    return tag(39, body, true);
}

[[nodiscard]] std::vector<std::uint8_t> define_text_stub(
    std::uint16_t id,
    bool empty_placeholder) {
    std::vector<std::uint8_t> body{};
    append_u16(body, id);
    if (empty_placeholder) {
        const std::array<std::uint8_t, 4> gray{0x50, 0x50, 0x50, 0xFF};
        body.insert(body.end(), gray.begin(), gray.end());
        constexpr std::string_view markup = "<font color=\"#505050\">placeholder</font>";
        body.insert(body.end(), markup.begin(), markup.end());
        body.push_back(0);
    } else {
        body.push_back(0);
    }
    return tag(37, body, true);
}

[[nodiscard]] std::vector<std::uint8_t> make_fixture(
    bool occupy_text_input_depth_two = false) {
    constexpr std::uint16_t controller_id = 100;
    constexpr std::uint16_t donor_id = 101;
    constexpr std::uint16_t window_list_id = 102;
    constexpr std::uint16_t item_character = 200;

    std::vector<std::vector<std::uint8_t>> controller_items{};
    for (std::uint16_t index = 0; index < 6; ++index) {
        controller_items.push_back(place_object(
            static_cast<std::uint16_t>(index * 2 + 1),
            item_character,
            -4800 + static_cast<std::int32_t>(index) * 1000,
            "Item_" + std::to_string(index) + "_0"));
    }

    std::vector<std::vector<std::uint8_t>> donor_items{};
    for (std::uint16_t index = 6; index < 12; ++index) {
        donor_items.push_back(place_object(
            static_cast<std::uint16_t>(31 + index),
            item_character,
            -4800 + static_cast<std::int32_t>(index) * 1000,
            "Item_" + std::to_string(index) + "_0"));
    }

    std::vector<std::vector<std::uint8_t>> window_items{};
    window_items.push_back(place_object(45, controller_id, 0, "ControllSetting"));

    std::vector<std::vector<std::uint8_t>> text_input_items{};
    if (occupy_text_input_depth_two) {
        text_input_items.push_back(place_object(2, 98, 0, "ExistingFrame"));
    }
    text_input_items.push_back(place_object(3, 98, -640, "Text_0", -4360));
    text_input_items.push_back(place_object(4, 99, -640, "TextOnEmpty", -4360));
    text_input_items.push_back(place_object(5, 101, 0, "Caption", -9520));
    text_input_items.push_back(place_object(7, 102, 0, "Cursor"));

    std::vector<std::uint8_t> movie{
        'G', 'F', 'X', 11,
        0, 0, 0, 0,
    };
    BitWriter rectangle{};
    rectangle.write(1, 5);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.write(0, 1);
    rectangle.align();
    movie.insert(movie.end(), rectangle.bytes().begin(), rectangle.bytes().end());
    append_u16(movie, 0); // Frame rate.
    append_u16(movie, 1); // Frame count.

    append_tag(movie, define_text_stub(98, false));
    append_tag(movie, define_text_stub(99, true));
    append_tag(movie, define_sprite(103, text_input_items));
    append_tag(movie, define_sprite(controller_id, controller_items));
    append_tag(movie, define_sprite(donor_id, donor_items));
    append_tag(movie, define_sprite(window_list_id, window_items));
    append_tag(movie, tag(1, {}));
    append_tag(movie, tag(0, {}));
    write_u32(movie, 4, static_cast<std::uint32_t>(movie.size()));
    movie.insert(movie.end(), 9, 0); // Preserve a GFX-style zero-padded tail.
    return movie;
}

} // namespace

int main() {
    const std::vector<std::uint8_t> original = make_fixture();
    const erui::gfx::Inspection initial = erui::gfx::inspect_controller_panel(original);
    ERUI_TEST_CHECK(initial.success());
    ERUI_TEST_CHECK(initial.controller_rows == 6);
    ERUI_TEST_CHECK(initial.controller_sprite == 100);
    ERUI_TEST_CHECK(initial.window_list_sprite == 102);
    ERUI_TEST_CHECK(initial.controller_item_character == 200);
    ERUI_TEST_CHECK(initial.host == erui::gfx::GfxHost::controller_settings);
    ERUI_TEST_CHECK(initial.text_input_sprite == 103);
    ERUI_TEST_CHECK(!initial.character_name_text_input);

    const erui::gfx::Inspection initial_text_input =
        erui::gfx::inspect_text_input_host(original);
    ERUI_TEST_CHECK(initial_text_input.success());
    ERUI_TEST_CHECK(
        initial_text_input.host == erui::gfx::GfxHost::controller_settings);
    ERUI_TEST_CHECK(initial_text_input.text_input_sprite == 103);

    const erui::gfx::PatchResult character_name =
        erui::gfx::patch_controller_panel(
            original,
            {
                .controller_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(character_name.success());
    ERUI_TEST_CHECK(character_name.report.after.character_name_text_input);
    ERUI_TEST_CHECK(!character_name.report.already_satisfied);
    ERUI_TEST_CHECK(character_name.output.size() > original.size());

    const erui::gfx::PatchResult character_name_second =
        erui::gfx::patch_controller_panel(
            character_name.output,
            {
                .controller_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(character_name_second.success());
    ERUI_TEST_CHECK(character_name_second.report.already_satisfied);
    ERUI_TEST_CHECK(character_name_second.output == character_name.output);

    const erui::gfx::PatchResult character_name_thirteen =
        erui::gfx::patch_controller_panel(
            original,
            {
                .controller_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(character_name_thirteen.success());
    ERUI_TEST_CHECK(character_name_thirteen.report.after.controller_rows == 13);
    ERUI_TEST_CHECK(character_name_thirteen.report.after.character_name_text_input);
    ERUI_TEST_CHECK(std::equal(
        original.end() - 9,
        original.end(),
        character_name_thirteen.output.end() - 9));

    const std::vector<std::uint8_t> occupied_text_input = make_fixture(true);
    const erui::gfx::PatchResult reject_occupied_text_input =
        erui::gfx::patch_controller_panel(
            occupied_text_input,
            {
                .controller_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(!reject_occupied_text_input.success());
    ERUI_TEST_CHECK(
        reject_occupied_text_input.error == erui::gfx::ErrorCode::unsupported_gfx);

    const erui::gfx::PatchResult seven = erui::gfx::patch_controller_panel(
        original,
        {.controller_rows = 7});
    ERUI_TEST_CHECK(seven.success());
    ERUI_TEST_CHECK(!seven.report.already_satisfied);
    ERUI_TEST_CHECK(seven.report.before.controller_rows == 6);
    ERUI_TEST_CHECK(seven.report.after.controller_rows == 7);
    ERUI_TEST_CHECK(seven.report.bytes_added != 0);
    ERUI_TEST_CHECK(seven.output.size() == original.size() + seven.report.bytes_added);
    ERUI_TEST_CHECK(std::equal(original.end() - 9, original.end(), seven.output.end() - 9));

    const erui::gfx::PatchResult twelve = erui::gfx::patch_controller_panel(
        original,
        {.controller_rows = 12});
    ERUI_TEST_CHECK(twelve.success());
    ERUI_TEST_CHECK(twelve.report.after.controller_rows == 12);
    ERUI_TEST_CHECK(twelve.report.bytes_added > seven.report.bytes_added);

    const erui::gfx::PatchResult thirteen = erui::gfx::patch_controller_panel(
        original,
        {.controller_rows = 13});
    ERUI_TEST_CHECK(thirteen.success());
    ERUI_TEST_CHECK(thirteen.report.after.controller_rows == 13);
    ERUI_TEST_CHECK(thirteen.report.bytes_added > twelve.report.bytes_added);

    const erui::gfx::PatchResult reject_fourteen =
        erui::gfx::patch_controller_panel(original, {.controller_rows = 14});
    ERUI_TEST_CHECK(!reject_fourteen.success());
    ERUI_TEST_CHECK(reject_fourteen.error == erui::gfx::ErrorCode::invalid_argument);

    const erui::gfx::PatchResult idempotent = erui::gfx::patch_controller_panel(
        twelve.output,
        {.controller_rows = 12});
    ERUI_TEST_CHECK(idempotent.success());
    ERUI_TEST_CHECK(idempotent.report.already_satisfied);
    ERUI_TEST_CHECK(idempotent.output == twelve.output);

    const erui::gfx::PatchResult refuse_shrink = erui::gfx::patch_controller_panel(
        twelve.output,
        {.controller_rows = 7});
    ERUI_TEST_CHECK(!refuse_shrink.success());
    ERUI_TEST_CHECK(refuse_shrink.error == erui::gfx::ErrorCode::invalid_argument);

    std::vector<std::uint8_t> corrupt = original;
    corrupt[0] = 'X';
    const erui::gfx::Inspection invalid = erui::gfx::inspect_controller_panel(corrupt);
    ERUI_TEST_CHECK(!invalid.success());
    ERUI_TEST_CHECK(invalid.error == erui::gfx::ErrorCode::invalid_gfx);

#if defined(ERNATIVEUI_TEST_CONTROLLER_GFX)
    std::ifstream real_file(
        ERNATIVEUI_TEST_CONTROLLER_GFX,
        std::ios::binary);
    ERUI_TEST_CHECK(real_file.good());
    const std::vector<std::uint8_t> real_input{
        std::istreambuf_iterator<char>(real_file),
        std::istreambuf_iterator<char>()};
    const erui::gfx::Inspection real_text_input =
        erui::gfx::inspect_controller_panel(real_input);
    ERUI_TEST_CHECK(real_text_input.success());
    ERUI_TEST_CHECK(
        real_text_input.host == erui::gfx::GfxHost::controller_settings);
    ERUI_TEST_CHECK(real_text_input.text_input_sprite == 103);
    // Release packaging promises the complete optional presentation: the
    // thirteen-row layout and the character-name idle frame. Test the checked-
    // in asset itself so a locally patched deploy cannot hide a stale file.
    ERUI_TEST_CHECK(real_text_input.controller_rows == 13);
    ERUI_TEST_CHECK(real_text_input.character_name_text_input);
    const erui::gfx::PatchResult real_patch =
        erui::gfx::patch_controller_panel(
            real_input, {.controller_rows = 13});
    ERUI_TEST_CHECK(real_patch.success());
    ERUI_TEST_CHECK(real_patch.report.after.controller_rows == 13);
    const erui::gfx::PatchResult real_second =
        erui::gfx::patch_controller_panel(
            real_patch.output, {.controller_rows = 13});
    ERUI_TEST_CHECK(real_second.success());
    ERUI_TEST_CHECK(real_second.report.already_satisfied);
    ERUI_TEST_CHECK(real_second.output == real_patch.output);

    const erui::gfx::PatchResult real_character_name =
        erui::gfx::patch_controller_panel(
            real_input,
            {
                .controller_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(real_character_name.success());
    ERUI_TEST_CHECK(real_character_name.report.after.character_name_text_input);
    ERUI_TEST_CHECK(real_character_name.report.already_satisfied);
    ERUI_TEST_CHECK(real_character_name.output == real_input);
    const erui::gfx::PatchResult real_character_name_second =
        erui::gfx::patch_controller_panel(
            real_character_name.output,
            {
                .controller_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(real_character_name_second.success());
    ERUI_TEST_CHECK(real_character_name_second.report.already_satisfied);
    ERUI_TEST_CHECK(
        real_character_name_second.output == real_character_name.output);
#endif

#if defined(ERNATIVEUI_TEST_ADVANCED_GFX)
    std::ifstream advanced_file(
        ERNATIVEUI_TEST_ADVANCED_GFX,
        std::ios::binary);
    ERUI_TEST_CHECK(advanced_file.good());
    const std::vector<std::uint8_t> advanced_input{
        std::istreambuf_iterator<char>(advanced_file),
        std::istreambuf_iterator<char>()};

    const erui::gfx::Inspection advanced_inspection =
        erui::gfx::inspect_text_input_host(advanced_input);
    ERUI_TEST_CHECK(advanced_inspection.success());
    ERUI_TEST_CHECK(
        advanced_inspection.host == erui::gfx::GfxHost::advanced_settings);
    ERUI_TEST_CHECK(advanced_inspection.text_input_sprite == 61);
    ERUI_TEST_CHECK(advanced_inspection.character_name_text_input);

    const erui::gfx::Inspection no_controller =
        erui::gfx::inspect_controller_panel(advanced_input);
    ERUI_TEST_CHECK(!no_controller.success());
    ERUI_TEST_CHECK(
        no_controller.error == erui::gfx::ErrorCode::controller_panel_not_found);

    const erui::gfx::PatchResult advanced_character_name =
        erui::gfx::patch_text_input_presentation(
            advanced_input,
            erui::gfx::TextInputPresentation::character_name);
    ERUI_TEST_CHECK(advanced_character_name.success());
    ERUI_TEST_CHECK(advanced_character_name.report.already_satisfied);
    ERUI_TEST_CHECK(advanced_character_name.report.bytes_added == 0);
    ERUI_TEST_CHECK(
        advanced_character_name.report.after.host ==
            erui::gfx::GfxHost::advanced_settings);
    ERUI_TEST_CHECK(
        advanced_character_name.report.after.text_input_sprite == 61);
    ERUI_TEST_CHECK(
        advanced_character_name.report.after.character_name_text_input);
    ERUI_TEST_CHECK(advanced_character_name.output == advanced_input);

    const erui::gfx::PatchResult advanced_native =
        erui::gfx::patch_text_input_presentation(
            advanced_input,
            erui::gfx::TextInputPresentation::native);
    ERUI_TEST_CHECK(advanced_native.success());
    ERUI_TEST_CHECK(advanced_native.report.already_satisfied);
    ERUI_TEST_CHECK(advanced_native.report.bytes_added == 0);
    ERUI_TEST_CHECK(advanced_native.output == advanced_input);

    const erui::gfx::PatchResult advanced_second =
        erui::gfx::patch_text_input_presentation(
            advanced_character_name.output,
            erui::gfx::TextInputPresentation::character_name);
    ERUI_TEST_CHECK(advanced_second.success());
    ERUI_TEST_CHECK(advanced_second.report.already_satisfied);
    ERUI_TEST_CHECK(advanced_second.report.bytes_added == 0);
    ERUI_TEST_CHECK(
        advanced_second.output == advanced_character_name.output);
#endif
    return 0;
}
