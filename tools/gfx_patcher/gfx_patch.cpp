#include "gfx_patch.hpp"

#include "gfx/gfx_document.hpp"
#include "gfx/settings_gfx_transformer.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace erui::gfx {
namespace {

constexpr std::uint16_t kMinimumGameOptionsRows = 6;
constexpr std::uint16_t kMaximumGameOptionsRows = 13;
constexpr std::uint16_t kMinimumCameraOptionsRows = 7;
constexpr std::uint16_t kMaximumCameraOptionsRows = 13;

[[nodiscard]] GfxHost legacy_host(SettingsGfxHost host) noexcept {
    switch (host) {
    case SettingsGfxHost::game_options:
        return GfxHost::game_options;
    case SettingsGfxHost::advanced_settings:
        return GfxHost::advanced_settings;
    case SettingsGfxHost::unknown:
        return GfxHost::unknown;
    }
    return GfxHost::unknown;
}

[[nodiscard]] Inspection legacy_inspection(
    const SettingsGfxInspection& source) {
    Inspection result{};
    result.gfx_version = source.gfx_version;
    result.declared_length = source.declared_length;
    result.window_list_sprite = source.window_list_sprite;
    result.game_options_sprite = source.game_options_sprite;
    result.game_options_item_character = source.game_options_item_character;
    result.game_options_rows = source.game_options_rows;
    result.camera_options_sprite = source.camera_options_sprite;
    result.camera_options_item_character = source.camera_options_item_character;
    result.camera_options_rows = source.camera_options_rows;
    result.host = legacy_host(source.host);
    result.text_input_sprite = source.text_input_sprite;
    result.character_name_text_input = source.character_name_text_input;
    result.color_picker_fill_sprite = source.color_picker_fill_sprite;
    result.color_picker_host_count = source.color_picker_host_count;
    result.character_creation_color_picker =
        source.character_creation_color_picker;
    return result;
}

[[nodiscard]] ErrorCode legacy_error(const SettingsGfxError& error) noexcept {
    if (error.code == SettingsGfxErrorCode::none) return ErrorCode::none;
    if (error.code == SettingsGfxErrorCode::invalid_argument) {
        return ErrorCode::invalid_argument;
    }
    if (error.code == SettingsGfxErrorCode::invalid_document) {
        return ErrorCode::invalid_gfx;
    }
    if (error.code == SettingsGfxErrorCode::verification_failed ||
        error.feature == SettingsGfxFeature::verification) {
        return ErrorCode::output_verification_failed;
    }
    if (error.feature == SettingsGfxFeature::game_options_rows) {
        const bool anchor_not_found =
            error.code == SettingsGfxErrorCode::structure_not_found ||
            error.code == SettingsGfxErrorCode::ambiguous_structure ||
            (error.code == SettingsGfxErrorCode::incompatible_structure &&
                error.detail == 1);
        return anchor_not_found
            ? ErrorCode::game_options_panel_not_found
            : ErrorCode::game_options_items_invalid;
    }
    if (error.feature == SettingsGfxFeature::camera_options_rows) {
        const bool anchor_not_found =
            error.code == SettingsGfxErrorCode::structure_not_found ||
            error.code == SettingsGfxErrorCode::ambiguous_structure ||
            (error.code == SettingsGfxErrorCode::incompatible_structure &&
                error.detail == 1);
        return anchor_not_found
            ? ErrorCode::camera_options_panel_not_found
            : ErrorCode::camera_options_items_invalid;
    }
    if (error.feature == SettingsGfxFeature::text_input) {
        return error.code == SettingsGfxErrorCode::structure_not_found
            ? ErrorCode::text_input_not_found
            : ErrorCode::text_input_invalid;
    }
    return ErrorCode::unsupported_gfx;
}

// The legacy API distinguished malformed structures found by its inspection
// entry points from a structurally recognized movie that cannot accept a
// requested mutation. In particular, an incompatible TextInput discovered
// while adding CharacterFrame was reported as unsupported_gfx rather than
// text_input_invalid. Preserve that distinction in the compatibility facade.
[[nodiscard]] ErrorCode legacy_transform_error(
    const SettingsGfxError& error) noexcept {
    if (error.feature == SettingsGfxFeature::button_label ||
        error.feature == SettingsGfxFeature::color_picker ||
        (error.feature == SettingsGfxFeature::text_input &&
            error.code != SettingsGfxErrorCode::structure_not_found)) {
        return ErrorCode::unsupported_gfx;
    }
    return legacy_error(error);
}

[[nodiscard]] std::string error_message(const SettingsGfxError& error) {
    std::string message{settings_gfx_feature_name(error.feature)};
    message += ": ";
    message += settings_gfx_error_name(error.code);
    if (!error.document_error.success()) {
        message += " (";
        message += gfx_document_error_name(error.document_error.code);
        message += ")";
    }
    if (error.detail != 0) {
        message += " [detail=";
        message += std::to_string(error.detail);
        message += "]";
    }
    return message;
}

[[nodiscard]] Inspection inspection_failure(const SettingsGfxError& error) {
    Inspection result{};
    result.error = legacy_error(error);
    result.message = error_message(error);
    return result;
}

[[nodiscard]] PatchResult patch_failure(
    ErrorCode code,
    std::string message) {
    PatchResult result{};
    result.error = code;
    result.message = std::move(message);
    return result;
}

[[nodiscard]] PatchResult patch_failure(const SettingsGfxError& error) {
    return patch_failure(
        legacy_transform_error(error), error_message(error));
}

[[nodiscard]] GfxDocumentParseResult parse_input(
    std::span<const std::uint8_t> input) {
    return GfxDocument::parse(input);
}

[[nodiscard]] SettingsGfxPresentation presentation(
    TextInputPresentation value) noexcept {
    return value == TextInputPresentation::character_name
        ? SettingsGfxPresentation::ensure_character_creation
        : SettingsGfxPresentation::preserve;
}

[[nodiscard]] SettingsGfxPresentation presentation(
    ColorPickerPresentation value) noexcept {
    return value == ColorPickerPresentation::character_creation
        ? SettingsGfxPresentation::ensure_character_creation
        : SettingsGfxPresentation::preserve;
}

[[nodiscard]] bool valid(TextInputPresentation value) noexcept {
    return value == TextInputPresentation::native ||
        value == TextInputPresentation::character_name;
}

[[nodiscard]] bool valid(ColorPickerPresentation value) noexcept {
    return value == ColorPickerPresentation::native ||
        value == ColorPickerPresentation::character_creation;
}

[[nodiscard]] PatchResult publish_transform(
    std::span<const std::uint8_t> input,
    SettingsGfxTransformResult transformed,
    bool require_game_options,
    std::optional<std::uint16_t> expected_game_rows,
    std::optional<std::uint16_t> expected_camera_rows,
    bool require_text_input,
    bool require_color_picker) {
    if (!transformed.success()) return patch_failure(transformed.error);

    PatchResult result{};
    result.report.before = legacy_inspection(transformed.before);
    result.report.after = legacy_inspection(transformed.after);
    if ((require_game_options &&
            result.report.after.host != GfxHost::game_options) ||
        (expected_game_rows &&
            result.report.after.game_options_rows != *expected_game_rows) ||
        (expected_camera_rows &&
            result.report.after.camera_options_rows != *expected_camera_rows) ||
        (require_text_input &&
            !result.report.after.character_name_text_input) ||
        (require_color_picker &&
            !result.report.after.character_creation_color_picker)) {
        return patch_failure(
            ErrorCode::output_verification_failed,
            "transformed settings movie did not satisfy the requested postconditions");
    }

    GfxDocumentBytesResult bytes =
        transformed.document.serialize_and_validate();
    if (!bytes.success()) {
        return patch_failure(
            ErrorCode::output_verification_failed,
            std::string{"could not serialize transformed settings movie: "} +
                gfx_document_error_name(bytes.error.code));
    }
    result.output = std::move(bytes.bytes);
    result.report.bytes_added = result.output.size() >= input.size()
        ? result.output.size() - input.size()
        : 0;
    result.report.already_satisfied = !transformed.changed();
    result.message = transformed.changed()
        ? "Settings movie transformed successfully"
        : "Settings movie already satisfies the requested transformation";
    return result;
}

} // namespace

Inspection inspect_text_input_host(
    std::span<const std::uint8_t> input) noexcept {
    try {
        GfxDocumentParseResult parsed = parse_input(input);
        if (!parsed.success()) {
            Inspection result{};
            result.error = ErrorCode::invalid_gfx;
            result.message = gfx_document_error_name(parsed.error.code);
            return result;
        }
        const SettingsGfxInspectionResult inspected =
            inspect_settings_widgets_gfx(parsed.document);
        if (!inspected.success()) return inspection_failure(inspected.error);
        Inspection result = legacy_inspection(inspected.inspection);
        if (result.text_input_sprite == 0) {
            result.error = ErrorCode::text_input_not_found;
            result.message = "native TextInput sprite was not found";
        }
        return result;
    } catch (...) {
        Inspection result{};
        result.error = ErrorCode::invalid_gfx;
        result.message = "unexpected exception while inspecting the GFX";
        return result;
    }
}

Inspection inspect_game_options_panel(
    std::span<const std::uint8_t> input) noexcept {
    try {
        GfxDocumentParseResult parsed = parse_input(input);
        if (!parsed.success()) {
            Inspection result{};
            result.error = ErrorCode::invalid_gfx;
            result.message = gfx_document_error_name(parsed.error.code);
            return result;
        }
        const SettingsGfxInspectionResult inspected =
            inspect_settings_gfx(parsed.document);
        if (!inspected.success()) return inspection_failure(inspected.error);

        Inspection result = legacy_inspection(inspected.inspection);
        if (result.host != GfxHost::game_options ||
            result.game_options_sprite == 0) {
            result.error = ErrorCode::game_options_panel_not_found;
            result.message = "WindowList.ControllSetting was not found";
        } else if (result.camera_options_sprite == 0) {
            result.error = ErrorCode::camera_options_panel_not_found;
            result.message = "WindowList.CameraSetting was not found";
        }
        return result;
    } catch (...) {
        Inspection result{};
        result.error = ErrorCode::invalid_gfx;
        result.message = "unexpected exception while inspecting the GFX";
        return result;
    }
}

PatchResult patch_widget_presentations(
    std::span<const std::uint8_t> input,
    TextInputPresentation text_input_presentation,
    ColorPickerPresentation color_picker_presentation) noexcept {
    try {
        if (!valid(text_input_presentation) ||
            !valid(color_picker_presentation)) {
            return patch_failure(
                ErrorCode::invalid_argument,
                "unknown widget presentation");
        }
        GfxDocumentParseResult parsed = parse_input(input);
        if (!parsed.success()) {
            return patch_failure(
                ErrorCode::invalid_gfx,
                gfx_document_error_name(parsed.error.code));
        }

        // The legacy widget entry point always required a native TextInput
        // host, including when both requested presentations were `native`.
        // Keep that contract without imposing the Game/Camera panel contract
        // on the standalone Advanced Graphics movie.
        const SettingsGfxInspectionResult before =
            inspect_settings_widgets_gfx(parsed.document);
        if (!before.success()) {
            return patch_failure(
                legacy_error(before.error), error_message(before.error));
        }
        if (before.inspection.text_input_sprite == 0) {
            return patch_failure(
                ErrorCode::text_input_not_found,
                "native TextInput sprite was not found");
        }

        SettingsGfxRequirements requirements{};
        requirements.text_input = presentation(text_input_presentation);
        requirements.color_picker = presentation(color_picker_presentation);
        return publish_transform(
            input,
            transform_settings_widgets_gfx(
                std::move(parsed.document), requirements),
            false,
            std::nullopt,
            std::nullopt,
            text_input_presentation == TextInputPresentation::character_name,
            color_picker_presentation ==
                ColorPickerPresentation::character_creation);
    } catch (...) {
        return patch_failure(
            ErrorCode::invalid_gfx,
            "unexpected exception while patching the GFX");
    }
}

PatchResult patch_text_input_presentation(
    std::span<const std::uint8_t> input,
    TextInputPresentation value) noexcept {
    return patch_widget_presentations(
        input, value, ColorPickerPresentation::native);
}

PatchResult patch_game_options_panel(
    std::span<const std::uint8_t> input,
    PatchOptions options) noexcept {
    try {
        if (options.game_options_rows < kMinimumGameOptionsRows ||
            options.game_options_rows > kMaximumGameOptionsRows) {
            return patch_failure(
                ErrorCode::invalid_argument,
                "Game Options row target must be between 6 and 13");
        }
        if (options.camera_options_rows < kMinimumCameraOptionsRows ||
            options.camera_options_rows > kMaximumCameraOptionsRows) {
            return patch_failure(
                ErrorCode::invalid_argument,
                "Camera Options row target must be between 7 and 13");
        }
        if (!valid(options.text_input_presentation) ||
            !valid(options.color_picker_presentation)) {
            return patch_failure(
                ErrorCode::invalid_argument,
                "unknown widget presentation");
        }

        GfxDocumentParseResult parsed = parse_input(input);
        if (!parsed.success()) {
            return patch_failure(
                ErrorCode::invalid_gfx,
                gfx_document_error_name(parsed.error.code));
        }
        const SettingsGfxInspectionResult before =
            inspect_settings_gfx(parsed.document);
        if (!before.success()) return patch_failure(before.error);
        if (before.inspection.host != SettingsGfxHost::game_options ||
            before.inspection.game_options_sprite == 0) {
            return patch_failure(
                ErrorCode::game_options_panel_not_found,
                "WindowList.ControllSetting was not found");
        }
        if (before.inspection.camera_options_sprite == 0) {
            return patch_failure(
                ErrorCode::camera_options_panel_not_found,
                "WindowList.CameraSetting was not found");
        }
        if (before.inspection.game_options_rows > options.game_options_rows ||
            before.inspection.camera_options_rows > options.camera_options_rows) {
            return patch_failure(
                ErrorCode::invalid_argument,
                "the requested row target would shrink an existing panel");
        }

        SettingsGfxRequirements requirements{};
        requirements.minimum_game_options_rows = options.game_options_rows;
        requirements.minimum_camera_options_rows = options.camera_options_rows;
        requirements.ensure_button_label =
            before.inspection.text_input_sprite != 0;
        requirements.text_input = presentation(options.text_input_presentation);
        requirements.color_picker = presentation(options.color_picker_presentation);
        return publish_transform(
            input,
            transform_settings_gfx(
                std::move(parsed.document), requirements),
            true,
            options.game_options_rows,
            options.camera_options_rows,
            options.text_input_presentation ==
                TextInputPresentation::character_name,
            options.color_picker_presentation ==
                ColorPickerPresentation::character_creation);
    } catch (...) {
        return patch_failure(
            ErrorCode::invalid_gfx,
            "unexpected exception while patching the GFX");
    }
}

const char* error_name(ErrorCode error) noexcept {
    switch (error) {
    case ErrorCode::none: return "none";
    case ErrorCode::invalid_argument: return "invalid_argument";
    case ErrorCode::invalid_gfx: return "invalid_gfx";
    case ErrorCode::unsupported_gfx: return "unsupported_gfx";
    case ErrorCode::game_options_panel_not_found:
        return "game_options_panel_not_found";
    case ErrorCode::game_options_items_invalid:
        return "game_options_items_invalid";
    case ErrorCode::camera_options_panel_not_found:
        return "camera_options_panel_not_found";
    case ErrorCode::camera_options_items_invalid:
        return "camera_options_items_invalid";
    case ErrorCode::donor_item_not_found: return "donor_item_not_found";
    case ErrorCode::text_input_not_found: return "text_input_not_found";
    case ErrorCode::text_input_invalid: return "text_input_invalid";
    case ErrorCode::output_verification_failed:
        return "output_verification_failed";
    }
    return "unknown";
}

const char* host_name(GfxHost host) noexcept {
    switch (host) {
    case GfxHost::unknown: return "unknown";
    case GfxHost::game_options:
        return "Game Options (02_040_optionsetting.gfx)";
    case GfxHost::advanced_settings:
        return "Advanced Settings (02_042_pc_graphicsetting.gfx)";
    }
    return "unknown";
}

} // namespace erui::gfx
