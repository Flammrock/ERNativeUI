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

[[nodiscard]] bool contains_ascii(
    std::span<const std::uint8_t> bytes,
    std::string_view text) {
    return std::search(
        bytes.begin(), bytes.end(), text.begin(), text.end()) != bytes.end();
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
        cursor = std::next(match, static_cast<std::ptrdiff_t>(text.size()));
    }
    return count;
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

[[nodiscard]] std::vector<std::uint8_t> place_object_unnamed(
    std::uint16_t depth,
    std::uint16_t character) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x06); // Character + Matrix.
    append_u16(body, depth);
    append_u16(body, character);
    const std::vector<std::uint8_t> transform = matrix(0, 0);
    body.insert(body.end(), transform.begin(), transform.end());
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
    bool occupy_text_input_depth_two = false,
    bool occupy_button_depth_seven = false,
    bool conflicting_button_frame_name = false,
    bool omit_camera_panel = false) {
    constexpr std::uint16_t game_options_id = 100;
    constexpr std::uint16_t camera_options_id = 110;
    constexpr std::uint16_t donor_id = 101;
    constexpr std::uint16_t window_list_id = 102;
    constexpr std::uint16_t item_character = 200;
    constexpr std::uint16_t button_id = 104;
    constexpr std::uint16_t widgets_id = 105;

    std::vector<std::vector<std::uint8_t>> game_options_items{};
    for (std::uint16_t index = 0; index < 6; ++index) {
        game_options_items.push_back(place_object(
            static_cast<std::uint16_t>(index * 2 + 1),
            item_character,
            -4800 + static_cast<std::int32_t>(index) * 1000,
            "Item_" + std::to_string(index) + "_0"));
    }

    std::vector<std::vector<std::uint8_t>> camera_options_items{};
    for (std::uint16_t index = 0; index < 7; ++index) {
        camera_options_items.push_back(place_object(
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
    window_items.push_back(place_object(45, game_options_id, 0, "ControllSetting"));
    if (!omit_camera_panel) {
        window_items.push_back(place_object(
            47, camera_options_id, 0, "CameraSetting"));
    }

    std::vector<std::vector<std::uint8_t>> text_input_items{};
    if (occupy_text_input_depth_two) {
        text_input_items.push_back(place_object(2, 98, 0, "ExistingFrame"));
    }
    text_input_items.push_back(place_object(3, 98, -640, "Text_0", -4360));
    text_input_items.push_back(place_object(4, 99, -640, "TextOnEmpty", -4360));
    text_input_items.push_back(place_object(5, 101, 0, "Caption", -9520));
    text_input_items.push_back(place_object(7, 102, 0, "Cursor"));

    std::vector<std::vector<std::uint8_t>> button_items{};
    button_items.push_back(place_object_unnamed(1, 89));
    button_items.push_back(conflicting_button_frame_name
        ? place_object(3, 91, 0, "ExistingFrame")
        : place_object_unnamed(3, 91));
    if (occupy_button_depth_seven) {
        button_items.push_back(place_object(7, 92, 0, "ExistingPreview"));
    }
    button_items.push_back(place_object(6, 94, 0, "Cursor"));
    button_items.push_back(place_object(10, 96, 0, "Text_0"));
    button_items.push_back(place_object(12, 97, 0, "Text_1"));

    std::vector<std::vector<std::uint8_t>> widget_items{};
    widget_items.push_back(place_object(1, button_id, 0, "Button"));
    widget_items.push_back(place_object(12, 103, 0, "TextInput"));
    widget_items.push_back(place_object(20, 106, 0, "ComboBox"));
    widget_items.push_back(place_object(34, 107, 0, "Slider"));

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

    // Button precedes TextInput so the combined presentation test also proves
    // that an existing CharacterFrame closure is relocated before its new
    // earliest standalone ColorPicker reference.
    append_tag(movie, define_sprite(button_id, button_items));
    append_tag(movie, define_sprite(widgets_id, widget_items));
    append_tag(movie, define_text_stub(98, false));
    append_tag(movie, define_text_stub(99, true));
    append_tag(movie, define_sprite(103, text_input_items));
    append_tag(movie, define_sprite(game_options_id, game_options_items));
    if (!omit_camera_panel) {
        append_tag(movie, define_sprite(camera_options_id, camera_options_items));
    }
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
    const erui::gfx::Inspection initial = erui::gfx::inspect_game_options_panel(original);
    ERUI_TEST_CHECK(initial.success());
    ERUI_TEST_CHECK(initial.game_options_rows == 6);
    ERUI_TEST_CHECK(initial.game_options_sprite == 100);
    ERUI_TEST_CHECK(initial.window_list_sprite == 102);
    ERUI_TEST_CHECK(initial.game_options_item_character == 200);
    ERUI_TEST_CHECK(initial.camera_options_rows == 7);
    ERUI_TEST_CHECK(initial.camera_options_sprite == 110);
    ERUI_TEST_CHECK(initial.camera_options_item_character == 200);
    ERUI_TEST_CHECK(initial.host == erui::gfx::GfxHost::game_options);
    ERUI_TEST_CHECK(initial.text_input_sprite == 103);
    ERUI_TEST_CHECK(!initial.character_name_text_input);
    ERUI_TEST_CHECK(initial.color_picker_host_count == 1);
    ERUI_TEST_CHECK(!initial.character_creation_color_picker);

    const erui::gfx::Inspection initial_text_input =
        erui::gfx::inspect_text_input_host(original);
    ERUI_TEST_CHECK(initial_text_input.success());
    ERUI_TEST_CHECK(
        initial_text_input.host == erui::gfx::GfxHost::game_options);
    ERUI_TEST_CHECK(initial_text_input.text_input_sprite == 103);

    const erui::gfx::PatchResult character_name =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(character_name.success());
    ERUI_TEST_CHECK(character_name.report.after.character_name_text_input);
    ERUI_TEST_CHECK(!character_name.report.already_satisfied);
    ERUI_TEST_CHECK(character_name.output.size() > original.size());

    const erui::gfx::PatchResult character_name_second =
        erui::gfx::patch_game_options_panel(
            character_name.output,
            {
                .game_options_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(character_name_second.success());
    ERUI_TEST_CHECK(character_name_second.report.already_satisfied);
    ERUI_TEST_CHECK(character_name_second.output == character_name.output);

    const erui::gfx::PatchResult color_picker =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 6,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(color_picker.success());
    ERUI_TEST_CHECK(
        color_picker.report.after.character_creation_color_picker);
    ERUI_TEST_CHECK(color_picker.report.after.color_picker_host_count == 1);
    ERUI_TEST_CHECK(!color_picker.report.already_satisfied);
    ERUI_TEST_CHECK(color_picker.output.size() > original.size());
    ERUI_TEST_CHECK(!contains_ascii(color_picker.output, "ButtonValueText"));
    ERUI_TEST_CHECK(count_ascii(color_picker.output, "ColorPicker") == 1);
    ERUI_TEST_CHECK(count_ascii(color_picker.output, "ColorPreview") == 0);
    ERUI_TEST_CHECK(
        count_ascii(color_picker.output, "Text_0") ==
        count_ascii(original, "Text_0") + 1);
    ERUI_TEST_CHECK(
        count_ascii(color_picker.output, "Text_1") ==
        count_ascii(original, "Text_1") + 1);

    const erui::gfx::PatchResult color_picker_second =
        erui::gfx::patch_game_options_panel(
            color_picker.output,
            {
                .game_options_rows = 6,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(color_picker_second.success());
    ERUI_TEST_CHECK(color_picker_second.report.already_satisfied);
    ERUI_TEST_CHECK(color_picker_second.output == color_picker.output);

    const erui::gfx::PatchResult text_then_color =
        erui::gfx::patch_game_options_panel(
            character_name.output,
            {
                .game_options_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(text_then_color.success());
    ERUI_TEST_CHECK(text_then_color.report.after.character_name_text_input);
    ERUI_TEST_CHECK(
        text_then_color.report.after.character_creation_color_picker);

    const erui::gfx::PatchResult color_then_text =
        erui::gfx::patch_game_options_panel(
            color_picker.output,
            {
                .game_options_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(color_then_text.success());
    ERUI_TEST_CHECK(color_then_text.report.after.character_name_text_input);
    ERUI_TEST_CHECK(
        color_then_text.report.after.character_creation_color_picker);
    ERUI_TEST_CHECK(color_then_text.output == text_then_color.output);

    // A standalone ColorPicker must not consume or rewrite an unrelated child
    // already present in the ordinary Button definition.
    const std::vector<std::uint8_t> button_with_depth_seven =
        make_fixture(false, true);
    const erui::gfx::PatchResult preserve_button_depth_seven =
        erui::gfx::patch_game_options_panel(
            button_with_depth_seven,
            {
                .game_options_rows = 6,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(preserve_button_depth_seven.success());
    ERUI_TEST_CHECK(contains_ascii(
        preserve_button_depth_seven.output, "ExistingPreview"));

    const std::vector<std::uint8_t> conflicting_color =
        make_fixture(false, false, true);
    const erui::gfx::PatchResult reject_conflicting_color =
        erui::gfx::patch_game_options_panel(
            conflicting_color,
            {
                .game_options_rows = 6,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(!reject_conflicting_color.success());
    ERUI_TEST_CHECK(
        reject_conflicting_color.error ==
            erui::gfx::ErrorCode::unsupported_gfx);

    const erui::gfx::PatchResult character_name_thirteen =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 13,
                .camera_options_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(character_name_thirteen.success());
    ERUI_TEST_CHECK(character_name_thirteen.report.after.game_options_rows == 13);
    ERUI_TEST_CHECK(character_name_thirteen.report.after.camera_options_rows == 13);
    ERUI_TEST_CHECK(character_name_thirteen.report.after.character_name_text_input);
    ERUI_TEST_CHECK(std::equal(
        original.end() - 9,
        original.end(),
        character_name_thirteen.output.end() - 9));

    const std::vector<std::uint8_t> occupied_text_input = make_fixture(true);
    const erui::gfx::PatchResult reject_occupied_text_input =
        erui::gfx::patch_game_options_panel(
            occupied_text_input,
            {
                .game_options_rows = 6,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(!reject_occupied_text_input.success());
    ERUI_TEST_CHECK(
        reject_occupied_text_input.error == erui::gfx::ErrorCode::unsupported_gfx);

    const erui::gfx::PatchResult seven = erui::gfx::patch_game_options_panel(
        original,
        {.game_options_rows = 7});
    ERUI_TEST_CHECK(seven.success());
    ERUI_TEST_CHECK(!seven.report.already_satisfied);
    ERUI_TEST_CHECK(seven.report.before.game_options_rows == 6);
    ERUI_TEST_CHECK(seven.report.after.game_options_rows == 7);
    ERUI_TEST_CHECK(seven.report.bytes_added != 0);
    ERUI_TEST_CHECK(seven.output.size() == original.size() + seven.report.bytes_added);
    ERUI_TEST_CHECK(std::equal(original.end() - 9, original.end(), seven.output.end() - 9));

    const erui::gfx::PatchResult twelve = erui::gfx::patch_game_options_panel(
        original,
        {.game_options_rows = 12});
    ERUI_TEST_CHECK(twelve.success());
    ERUI_TEST_CHECK(twelve.report.after.game_options_rows == 12);
    ERUI_TEST_CHECK(twelve.report.bytes_added > seven.report.bytes_added);

    const erui::gfx::PatchResult thirteen = erui::gfx::patch_game_options_panel(
        original,
        {
            .game_options_rows = 13,
            .camera_options_rows = 13,
        });
    ERUI_TEST_CHECK(thirteen.success());
    ERUI_TEST_CHECK(thirteen.report.after.game_options_rows == 13);
    ERUI_TEST_CHECK(thirteen.report.bytes_added > twelve.report.bytes_added);

    const erui::gfx::PatchResult reject_fourteen =
        erui::gfx::patch_game_options_panel(original, {.game_options_rows = 14});
    ERUI_TEST_CHECK(!reject_fourteen.success());
    ERUI_TEST_CHECK(reject_fourteen.error == erui::gfx::ErrorCode::invalid_argument);

    const erui::gfx::PatchResult camera_eight =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 6,
                .camera_options_rows = 8,
            });
    ERUI_TEST_CHECK(camera_eight.success());
    ERUI_TEST_CHECK(camera_eight.report.before.camera_options_rows == 7);
    ERUI_TEST_CHECK(camera_eight.report.after.camera_options_rows == 8);
    ERUI_TEST_CHECK(camera_eight.report.after.game_options_rows == 6);
    ERUI_TEST_CHECK(camera_eight.report.bytes_added != 0);
    ERUI_TEST_CHECK(std::equal(
        original.end() - 9,
        original.end(),
        camera_eight.output.end() - 9));

    const erui::gfx::PatchResult camera_thirteen =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 6,
                .camera_options_rows = 13,
            });
    ERUI_TEST_CHECK(camera_thirteen.success());
    ERUI_TEST_CHECK(camera_thirteen.report.after.camera_options_rows == 13);
    ERUI_TEST_CHECK(
        camera_thirteen.report.bytes_added > camera_eight.report.bytes_added);

    const erui::gfx::PatchResult reject_camera_six =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 6,
                .camera_options_rows = 6,
            });
    ERUI_TEST_CHECK(!reject_camera_six.success());
    ERUI_TEST_CHECK(
        reject_camera_six.error == erui::gfx::ErrorCode::invalid_argument);

    const erui::gfx::PatchResult reject_camera_fourteen =
        erui::gfx::patch_game_options_panel(
            original,
            {
                .game_options_rows = 6,
                .camera_options_rows = 14,
            });
    ERUI_TEST_CHECK(!reject_camera_fourteen.success());
    ERUI_TEST_CHECK(
        reject_camera_fourteen.error == erui::gfx::ErrorCode::invalid_argument);

    const erui::gfx::PatchResult camera_idempotent =
        erui::gfx::patch_game_options_panel(
            camera_thirteen.output,
            {
                .game_options_rows = 6,
                .camera_options_rows = 13,
            });
    ERUI_TEST_CHECK(camera_idempotent.success());
    ERUI_TEST_CHECK(camera_idempotent.report.already_satisfied);
    ERUI_TEST_CHECK(camera_idempotent.output == camera_thirteen.output);

    const erui::gfx::PatchResult refuse_camera_shrink =
        erui::gfx::patch_game_options_panel(
            camera_thirteen.output,
            {
                .game_options_rows = 6,
                .camera_options_rows = 8,
            });
    ERUI_TEST_CHECK(!refuse_camera_shrink.success());
    ERUI_TEST_CHECK(
        refuse_camera_shrink.error == erui::gfx::ErrorCode::invalid_argument);

    const erui::gfx::PatchResult idempotent = erui::gfx::patch_game_options_panel(
        twelve.output,
        {.game_options_rows = 12});
    ERUI_TEST_CHECK(idempotent.success());
    ERUI_TEST_CHECK(idempotent.report.already_satisfied);
    ERUI_TEST_CHECK(idempotent.output == twelve.output);

    const erui::gfx::PatchResult refuse_shrink = erui::gfx::patch_game_options_panel(
        twelve.output,
        {.game_options_rows = 7});
    ERUI_TEST_CHECK(!refuse_shrink.success());
    ERUI_TEST_CHECK(refuse_shrink.error == erui::gfx::ErrorCode::invalid_argument);

    std::vector<std::uint8_t> corrupt = original;
    corrupt[0] = 'X';
    const erui::gfx::Inspection invalid = erui::gfx::inspect_game_options_panel(corrupt);
    ERUI_TEST_CHECK(!invalid.success());
    ERUI_TEST_CHECK(invalid.error == erui::gfx::ErrorCode::invalid_gfx);

    const std::vector<std::uint8_t> no_camera =
        make_fixture(false, false, false, true);
    const erui::gfx::Inspection missing_camera =
        erui::gfx::inspect_game_options_panel(no_camera);
    ERUI_TEST_CHECK(!missing_camera.success());
    ERUI_TEST_CHECK(
        missing_camera.error ==
            erui::gfx::ErrorCode::camera_options_panel_not_found);

#if defined(ERNATIVEUI_TEST_GAME_OPTIONS_GFX)
    std::ifstream real_file(
        ERNATIVEUI_TEST_GAME_OPTIONS_GFX,
        std::ios::binary);
    ERUI_TEST_CHECK(real_file.good());
    const std::vector<std::uint8_t> real_input{
        std::istreambuf_iterator<char>(real_file),
        std::istreambuf_iterator<char>()};
    const erui::gfx::Inspection real_text_input =
        erui::gfx::inspect_game_options_panel(real_input);
    ERUI_TEST_CHECK(real_text_input.success());
    ERUI_TEST_CHECK(
        real_text_input.host == erui::gfx::GfxHost::game_options);
    ERUI_TEST_CHECK(real_text_input.text_input_sprite == 103);
    // Release packaging promises the complete optional presentation: the
    // thirteen-row layout and the character-name idle frame. Test the checked-
    // in asset itself so a locally patched deploy cannot hide a stale file.
    ERUI_TEST_CHECK(real_text_input.game_options_rows == 13);
    ERUI_TEST_CHECK(real_text_input.camera_options_rows == 13);
    ERUI_TEST_CHECK(real_text_input.character_name_text_input);
    ERUI_TEST_CHECK(real_text_input.character_creation_color_picker);
    ERUI_TEST_CHECK(real_text_input.color_picker_host_count == 2);
    const erui::gfx::PatchResult real_patch =
        erui::gfx::patch_game_options_panel(
            real_input,
            {
                .game_options_rows = 13,
                .camera_options_rows = 13,
            });
    ERUI_TEST_CHECK(real_patch.success());
    ERUI_TEST_CHECK(real_patch.report.after.game_options_rows == 13);
    const erui::gfx::PatchResult real_second =
        erui::gfx::patch_game_options_panel(
            real_patch.output,
            {
                .game_options_rows = 13,
                .camera_options_rows = 13,
            });
    ERUI_TEST_CHECK(real_second.success());
    ERUI_TEST_CHECK(real_second.report.already_satisfied);
    ERUI_TEST_CHECK(real_second.output == real_patch.output);

    const erui::gfx::PatchResult real_character_name =
        erui::gfx::patch_game_options_panel(
            real_input,
            {
                .game_options_rows = 13,
                .camera_options_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(real_character_name.success());
    ERUI_TEST_CHECK(real_character_name.report.after.character_name_text_input);
    ERUI_TEST_CHECK(real_character_name.report.already_satisfied);
    ERUI_TEST_CHECK(real_character_name.output == real_input);
    const erui::gfx::PatchResult real_character_name_second =
        erui::gfx::patch_game_options_panel(
            real_character_name.output,
            {
                .game_options_rows = 13,
                .camera_options_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
            });
    ERUI_TEST_CHECK(real_character_name_second.success());
    ERUI_TEST_CHECK(real_character_name_second.report.already_satisfied);
    ERUI_TEST_CHECK(
        real_character_name_second.output == real_character_name.output);

    const erui::gfx::PatchResult real_color_picker =
        erui::gfx::patch_game_options_panel(
            real_input,
            {
                .game_options_rows = 13,
                .camera_options_rows = 13,
                .text_input_presentation =
                    erui::gfx::TextInputPresentation::character_name,
                .color_picker_presentation =
                    erui::gfx::ColorPickerPresentation::character_creation,
            });
    ERUI_TEST_CHECK(real_color_picker.success());
    ERUI_TEST_CHECK(real_color_picker.report.already_satisfied);
    ERUI_TEST_CHECK(real_color_picker.output == real_input);
    ERUI_TEST_CHECK(!contains_ascii(real_input, "ButtonValueText"));
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
    ERUI_TEST_CHECK(
        advanced_inspection.character_creation_color_picker);
    ERUI_TEST_CHECK(advanced_inspection.color_picker_host_count == 1);

    const erui::gfx::Inspection no_game_options =
        erui::gfx::inspect_game_options_panel(advanced_input);
    ERUI_TEST_CHECK(!no_game_options.success());
    ERUI_TEST_CHECK(
        no_game_options.error == erui::gfx::ErrorCode::game_options_panel_not_found);

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

    const erui::gfx::PatchResult advanced_color_picker =
        erui::gfx::patch_widget_presentations(
            advanced_input,
            erui::gfx::TextInputPresentation::character_name,
            erui::gfx::ColorPickerPresentation::character_creation);
    ERUI_TEST_CHECK(advanced_color_picker.success());
    ERUI_TEST_CHECK(advanced_color_picker.report.already_satisfied);
    ERUI_TEST_CHECK(advanced_color_picker.output == advanced_input);
    ERUI_TEST_CHECK(!contains_ascii(advanced_input, "ButtonValueText"));
#endif
    return 0;
}
