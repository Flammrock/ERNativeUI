#pragma once

#include "gfx/gfx_document.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace erui::gfx {

enum class SettingsGfxHost : std::uint8_t {
    unknown,
    game_options,
    advanced_settings,
};

enum class SettingsGfxFeature : std::uint8_t {
    none,
    document,
    game_options_rows,
    camera_options_rows,
    display_rows,
    sound_rows,
    network_rows,
    keyboard_mouse_rows,
    graphics_rows,
    tab_slots,
    button_label,
    text_input,
    color_picker,
    verification,
};

enum class SettingsGfxErrorCode : std::uint8_t {
    none,
    invalid_argument,
    invalid_document,
    unsupported_document,
    structure_not_found,
    ambiguous_structure,
    incompatible_structure,
    resource_conflict,
    character_id_exhausted,
    mutation_failed,
    verification_failed,
};

enum class SettingsGfxPresentation : std::uint8_t {
    preserve,
    ensure_character_creation,
};

struct SettingsGfxError {
    SettingsGfxErrorCode code{SettingsGfxErrorCode::none};
    SettingsGfxFeature feature{SettingsGfxFeature::none};
    GfxDocumentError document_error{};
    std::uint32_t detail{};

    [[nodiscard]] bool success() const noexcept {
        return code == SettingsGfxErrorCode::none;
    }
};

struct SettingsGfxInspection {
    SettingsGfxHost host{SettingsGfxHost::unknown};
    std::uint8_t gfx_version{};
    std::uint32_t declared_length{};

    std::uint16_t window_list_sprite{};
    std::uint16_t game_options_sprite{};
    std::uint16_t game_options_item_character{};
    std::uint16_t game_options_rows{};
    std::uint16_t camera_options_sprite{};
    std::uint16_t camera_options_item_character{};
    std::uint16_t camera_options_rows{};
    std::uint16_t display_sprite{};
    std::uint16_t display_item_character{};
    std::uint16_t display_rows{};
    std::uint16_t sound_sprite{};
    std::uint16_t sound_item_character{};
    std::uint16_t sound_rows{};
    std::uint16_t network_sprite{};
    std::uint16_t network_item_character{};
    std::uint16_t network_rows{};
    std::uint16_t keyboard_mouse_sprite{};
    std::uint16_t keyboard_mouse_item_character{};
    std::uint16_t keyboard_mouse_rows{};
    std::uint16_t graphics_sprite{};
    std::uint16_t graphics_item_character{};
    std::uint16_t graphics_rows{};

    // The validated Configuration layouts author one more contiguous
    // Item_0_N cell than the requested native records. This is only authored
    // presentation capacity; the live selector's D0 remains authoritative
    // for the number of active/selectable physical tabs.
    std::uint16_t tab_list_sprite{};
    std::uint16_t back_tab_list_sprite{};
    std::uint16_t tab_list_cells{};
    std::uint16_t back_tab_list_cells{};
    std::uint16_t authored_tab_slot_capacity{};

    std::uint16_t text_input_sprite{};
    bool character_name_text_input{};
    std::uint16_t color_picker_fill_sprite{};
    std::uint16_t color_picker_host_count{};
    bool character_creation_color_picker{};
};

struct SettingsGfxInspectionResult {
    SettingsGfxInspection inspection{};
    SettingsGfxError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

struct SettingsGfxRequirements {
    std::optional<std::uint16_t> minimum_game_options_rows{};
    std::optional<std::uint16_t> minimum_camera_options_rows{};
    std::optional<std::uint16_t> minimum_display_rows{};
    std::optional<std::uint16_t> minimum_sound_rows{};
    std::optional<std::uint16_t> minimum_network_rows{};
    std::optional<std::uint16_t> minimum_keyboard_mouse_rows{};
    std::optional<std::uint16_t> minimum_graphics_rows{};
    // Selectable physical slots requested by policy. The transformer ensures
    // both TabList layers author at least minimum_tab_slots + 1 cells; it
    // never removes cells from a larger compatible third-party movie.
    std::optional<std::uint16_t> minimum_tab_slots{};
    bool ensure_button_label{};
    SettingsGfxPresentation text_input{SettingsGfxPresentation::preserve};
    SettingsGfxPresentation color_picker{SettingsGfxPresentation::preserve};
};

enum class SettingsGfxTransformStatus : std::uint8_t {
    failed,
    unchanged,
    modified,
};

struct SettingsGfxTransformResult {
    // Valid only when status is unchanged or modified. On failure the source
    // passed by the caller remains untouched and this document is invalid.
    GfxDocument document{};
    SettingsGfxInspection before{};
    SettingsGfxInspection after{};
    SettingsGfxError error{};
    SettingsGfxTransformStatus status{SettingsGfxTransformStatus::failed};

    [[nodiscard]] bool success() const noexcept {
        return status != SettingsGfxTransformStatus::failed && error.success();
    }

    [[nodiscard]] bool changed() const noexcept {
        return status == SettingsGfxTransformStatus::modified;
    }
};

// Inspects only the Elden Ring settings structures understood by the host.
// Unknown tags and unrelated authored structures remain the responsibility of
// GfxDocument and are never normalized by this semantic layer.
[[nodiscard]] SettingsGfxInspectionResult inspect_settings_gfx(
    const GfxDocument& document) noexcept;

// Inspects only the shared TextInput/widget library. This route deliberately
// does not require the Game Options and Camera Options panel pair, allowing a
// presentation-only consumer to operate on a movie whose settings panels are
// absent or independently authored.
[[nodiscard]] SettingsGfxInspectionResult inspect_settings_widgets_gfx(
    const GfxDocument& document) noexcept;

// The source is consumed by value so all edits are private until semantic
// verification succeeds. Pass an lvalue to retain an untouched caller-owned
// source, or move a disposable document to avoid its initial copy.
[[nodiscard]] SettingsGfxTransformResult transform_settings_gfx(
    GfxDocument source,
    const SettingsGfxRequirements& requirements) noexcept;

// Applies only Button/TextInput/ColorPicker presentation requirements. Row
// requirements are invalid for this entry point. Edits remain private until
// widget-only semantic verification succeeds.
[[nodiscard]] SettingsGfxTransformResult transform_settings_widgets_gfx(
    GfxDocument source,
    const SettingsGfxRequirements& requirements) noexcept;

[[nodiscard]] const char* settings_gfx_error_name(
    SettingsGfxErrorCode code) noexcept;
[[nodiscard]] const char* settings_gfx_feature_name(
    SettingsGfxFeature feature) noexcept;
[[nodiscard]] const char* settings_gfx_host_name(SettingsGfxHost host) noexcept;

} // namespace erui::gfx
