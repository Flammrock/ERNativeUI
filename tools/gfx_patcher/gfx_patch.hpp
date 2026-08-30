#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace erui::gfx {

enum class ErrorCode : std::uint8_t {
    none,
    invalid_argument,
    invalid_gfx,
    unsupported_gfx,
    controller_panel_not_found,
    controller_items_invalid,
    donor_item_not_found,
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

    [[nodiscard]] bool success() const noexcept { return error == ErrorCode::none; }
};

struct PatchOptions {
    std::uint16_t controller_rows{7};
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

// Expands WindowList.ControllSetting and adds its missing button Text_1/Text
// label binding. The transformation is validated and byte-idempotent.
[[nodiscard]] PatchResult patch_controller_panel(
    std::span<const std::uint8_t> input,
    PatchOptions options = {}) noexcept;

[[nodiscard]] const char* error_name(ErrorCode error) noexcept;

} // namespace erui::gfx
