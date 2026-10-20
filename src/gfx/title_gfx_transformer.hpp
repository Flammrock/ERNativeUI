#pragma once

#include "gfx/gfx_document.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace erui::gfx {

// Mirrored by the public API's registration limit. Keeping this visible to
// the host lets the integration boundary enforce equality with static_assert
// without coupling this structural layer to erui.h.
inline constexpr std::size_t title_gfx_maximum_lines = 16;

struct TitleGfxRgb {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};

    friend bool operator==(const TitleGfxRgb&, const TitleGfxRgb&) = default;
};

enum class TitleGfxCorner : std::uint8_t {
    top_left = 1,
    top_right = 2,
    bottom_left = 3,
    bottom_right = 4,
};

enum class TitleGfxDlcState : std::uint8_t {
    unknown,
    absent,
    installed,
};

// The structural transformer can recognize a field named DLCVersion_0, but
// it cannot prove what a modified movie or another mod does with that field at
// runtime. Reusing its authored row therefore requires an independent caller
// proof that the field still has Elden Ring's canonical Shadow of the Erdtree
// status-label semantics. The safe default treats the field as occupied.
enum class TitleGfxDlcFieldProof : std::uint8_t {
    unproven,
    canonical_shadow_of_the_erdtree,
};

// Runtime integrations can add footer text after ERNativeUI's OpenFile
// transformation has completed. Such a line is not present in the incoming
// GFX bytes, so the structural pass cannot discover it by itself. An
// occupancy hint reserves only its vertical row on the corresponding half of
// the title screen; no third-party display object is copied or modified.
enum class TitleGfxTextSide : std::uint8_t {
    left,
    right,
};

struct TitleGfxTextOccupancy {
    TitleGfxTextSide side{};
    std::int32_t y{};

    friend bool operator==(
        const TitleGfxTextOccupancy&,
        const TitleGfxTextOccupancy&) = default;
};

struct TitleGfxTransformOptions {
    TitleGfxDlcState dlc_state{TitleGfxDlcState::unknown};
    TitleGfxDlcFieldProof dlc_field_proof{TitleGfxDlcFieldProof::unproven};
    std::span<const TitleGfxTextOccupancy> external_text_occupancy{};
};

// Text is UTF-8 and is consumed only for the duration of transform_title_gfx.
// Omitting color preserves the native title-version text color found in the
// incoming movie; supplying it writes an opaque RGB color for this line.
struct TitleGfxTextLine {
    std::string_view text{};
    std::optional<TitleGfxRgb> color{};
    TitleGfxCorner corner{};
};

enum class TitleGfxFeature : std::uint8_t {
    none,
    document,
    info_sprite,
    native_text_fields,
    text_line,
    character_id,
    depth,
    serialization,
    verification,
};

enum class TitleGfxErrorCode : std::uint8_t {
    none,
    invalid_argument,
    invalid_document,
    unsupported_document,
    structure_not_found,
    ambiguous_structure,
    incompatible_structure,
    resource_conflict,
    character_id_exhausted,
    depth_exhausted,
    mutation_failed,
    verification_failed,
};

struct TitleGfxError {
    static constexpr std::uint32_t no_line =
        std::numeric_limits<std::uint32_t>::max();

    TitleGfxErrorCode code{TitleGfxErrorCode::none};
    TitleGfxFeature feature{TitleGfxFeature::none};
    GfxDocumentError document_error{};
    std::uint32_t detail{};
    std::uint32_t line_index{no_line};

    [[nodiscard]] bool success() const noexcept {
        return code == TitleGfxErrorCode::none;
    }
};

struct TitleGfxInspection {
    std::uint8_t gfx_version{};
    std::uint32_t declared_length{};
    std::uint16_t info_sprite{};
    std::uint16_t native_text_character{};
    std::uint16_t native_maximum_depth{};
    std::int32_t text_x{};
    std::int32_t first_injected_y{};
    std::int32_t line_spacing{};
    TitleGfxRgb native_color{};
    std::uint8_t native_alpha{};
    std::size_t existing_private_lines{};
};

struct TitleGfxInspectionResult {
    TitleGfxInspection inspection{};
    TitleGfxError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

enum class TitleGfxTransformStatus : std::uint8_t {
    failed,
    unchanged,
    modified,
};

struct TitleGfxTransformResult {
    // Valid only for unchanged or modified results. The source is consumed by
    // value, so a failed transformation cannot partially mutate a caller's
    // retained document or the byte buffer supplied to Scaleform.
    GfxDocument document{};
    TitleGfxInspection before{};
    TitleGfxInspection after{};
    TitleGfxError error{};
    TitleGfxTransformStatus status{TitleGfxTransformStatus::failed};

    [[nodiscard]] bool success() const noexcept {
        return status != TitleGfxTransformStatus::failed && error.success();
    }

    [[nodiscard]] bool changed() const noexcept {
        return status == TitleGfxTransformStatus::modified;
    }
};

// Structural inspection is deliberately limited to the native title footer:
// the unique Info sprite containing RegulationVersion, AppVersion,
// OnlineMode, and DLCVersion_0. No fixed character IDs are assumed.
[[nodiscard]] TitleGfxInspectionResult inspect_title_gfx(
    const GfxDocument& document,
    TitleGfxTransformOptions options = {}) noexcept;

// Adds immutable private fields to structurally derived title-screen corners.
// Registration order is preserved within each corner. Existing content is
// retained, including compatible third-party fields, and acts as collision
// occupancy. Definitions and placements are committed as one validated
// GfxDocument edit, then serialized and reparsed before the result is returned.
[[nodiscard]] TitleGfxTransformResult transform_title_gfx(
    GfxDocument source,
    std::span<const TitleGfxTextLine> lines,
    TitleGfxTransformOptions options = {}) noexcept;

[[nodiscard]] const char* title_gfx_error_name(
    TitleGfxErrorCode code) noexcept;
[[nodiscard]] const char* title_gfx_feature_name(
    TitleGfxFeature feature) noexcept;

} // namespace erui::gfx
