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

enum class GfxHost : std::uint8_t {
    unknown,
    controller_settings,
    advanced_settings,
};

enum class ErrorCode : std::uint8_t {
    none,
    invalid_argument,
    invalid_gfx,
    unsupported_gfx,
    controller_panel_not_found,
    controller_items_invalid,
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
    std::uint16_t controller_sprite{};
    std::uint16_t controller_item_character{};
    std::uint16_t controller_rows{};
    GfxHost host{GfxHost::unknown};
    std::uint16_t text_input_sprite{};
    bool character_name_text_input{};

    [[nodiscard]] bool success() const noexcept { return error == ErrorCode::none; }
};

struct PatchOptions {
    std::uint16_t controller_rows{7};
    TextInputPresentation text_input_presentation{TextInputPresentation::native};
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
[[nodiscard]] Inspection inspect_controller_panel(
    std::span<const std::uint8_t> input) noexcept;

// Inspects either supported host movie by locating its TextInput sprite from
// the native named-child/depth/transform contract. Unlike
// inspect_controller_panel(), this also succeeds for
// 02_042_pc_graphicsetting.gfx, which has no ControllSetting panel.
[[nodiscard]] Inspection inspect_text_input_host(
    std::span<const std::uint8_t> input) noexcept;

// Expands WindowList.ControllSetting, adds its missing button Text_1/Text
// label binding and, when requested, adds the character-name presentation to
// the shared TextInput widget. The transformation is validated and
// byte-idempotent.
[[nodiscard]] PatchResult patch_controller_panel(
    std::span<const std::uint8_t> input,
    PatchOptions options = {}) noexcept;

// Applies only the shared TextInput presentation. This is the route used for
// 02_042_pc_graphicsetting.gfx; it never changes controller row capacity or
// the Controller Settings button-label repair.
[[nodiscard]] PatchResult patch_text_input_presentation(
    std::span<const std::uint8_t> input,
    TextInputPresentation presentation) noexcept;

[[nodiscard]] const char* error_name(ErrorCode error) noexcept;
[[nodiscard]] const char* host_name(GfxHost host) noexcept;

} // namespace erui::gfx
