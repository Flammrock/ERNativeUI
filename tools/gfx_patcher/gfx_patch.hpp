#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace erui::gfx {

enum class TextInputPresentation : std::uint8_t {
    native,
    character_name,
};

enum class ColorPickerPresentation : std::uint8_t {
    native,
    character_creation,
};

enum class GfxHost : std::uint8_t {
    unknown,
    game_options,
    advanced_settings,
};

enum class ErrorCode : std::uint8_t {
    none,
    invalid_argument,
    invalid_gfx,
    unsupported_gfx,
    game_options_panel_not_found,
    game_options_items_invalid,
    camera_options_panel_not_found,
    camera_options_items_invalid,
    donor_item_not_found,
    text_input_not_found,
    text_input_invalid,
    output_verification_failed,
};

struct Inspection {
    ErrorCode error{ErrorCode::none};
    std::string message{};
    std::uint8_t gfx_version{};
    std::uint32_t declared_length{};
    std::uint16_t window_list_sprite{};
    std::uint16_t game_options_sprite{};
    std::uint16_t game_options_item_character{};
    std::uint16_t game_options_rows{};
    std::uint16_t camera_options_sprite{};
    std::uint16_t camera_options_item_character{};
    std::uint16_t camera_options_rows{};
    GfxHost host{GfxHost::unknown};
    std::uint16_t text_input_sprite{};
    bool character_name_text_input{};
    std::uint16_t color_picker_fill_sprite{};
    std::uint16_t color_picker_host_count{};
    bool character_creation_color_picker{};

    [[nodiscard]] bool success() const noexcept { return error == ErrorCode::none; }
};

struct PatchOptions {
    std::uint16_t game_options_rows{7};
    std::uint16_t camera_options_rows{7};
    TextInputPresentation text_input_presentation{TextInputPresentation::native};
    ColorPickerPresentation color_picker_presentation{
        ColorPickerPresentation::native};
};

struct PatchReport {
    Inspection before{};
    Inspection after{};
    std::size_t bytes_added{};
    bool already_satisfied{};
};

struct PatchResult {
    ErrorCode error{ErrorCode::none};
    std::string message{};
    std::vector<std::uint8_t> output{};
    PatchReport report{};

    [[nodiscard]] bool success() const noexcept { return error == ErrorCode::none; }
};

// Inspects the user's own uncompressed Scaleform GFX file. The parser only
// needs the standard SWF/GFX tag structures used by 02_040_optionsetting.gfx.
[[nodiscard]] Inspection inspect_game_options_panel(
    std::span<const std::uint8_t> input) noexcept;

// Inspects either supported host movie by locating its TextInput sprite from
// the native named-child/depth/transform contract. Unlike
// inspect_game_options_panel(), this also succeeds for
// 02_042_pc_graphicsetting.gfx, which has no ControllSetting panel.
[[nodiscard]] Inspection inspect_text_input_host(
    std::span<const std::uint8_t> input) noexcept;

// Expands WindowList.ControllSetting and WindowList.CameraSetting, adds the
// shared Button widget's missing Text_1/Text label binding and, when
// requested, adds the character-name presentation to the shared TextInput
// widget. The transformation is validated and byte-idempotent.
[[nodiscard]] PatchResult patch_game_options_panel(
    std::span<const std::uint8_t> input,
    PatchOptions options = {}) noexcept;

// Applies only the shared TextInput presentation. This is the route used for
// 02_042_pc_graphicsetting.gfx; it never changes Game Options row capacity or
// the Game Options button-label repair.
[[nodiscard]] PatchResult patch_text_input_presentation(
    std::span<const std::uint8_t> input,
    TextInputPresentation presentation) noexcept;

// Applies the widget presentations shared by both supported settings movies.
// The native values leave the corresponding widget unchanged. The character-
// creation presentation adds a hidden standalone Widgets/ColorPicker sibling
// with direct Color and Cursor children. Ordinary rows remain byte-for-byte
// native until the host selects that widget for a color row.
[[nodiscard]] PatchResult patch_widget_presentations(
    std::span<const std::uint8_t> input,
    TextInputPresentation text_input_presentation,
    ColorPickerPresentation color_picker_presentation) noexcept;

[[nodiscard]] const char* error_name(ErrorCode error) noexcept;
[[nodiscard]] const char* host_name(GfxHost host) noexcept;

} // namespace erui::gfx
