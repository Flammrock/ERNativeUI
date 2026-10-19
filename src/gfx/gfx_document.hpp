#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace erui::gfx {

// Resource limits are part of parsing, not merely advisory. They keep a
// malformed or hostile replacement movie from turning structural inspection
// into unbounded work on the game's UI thread.
struct GfxDocumentLimits {
    static constexpr std::size_t hard_maximum_nesting_depth = 256;

    std::size_t maximum_physical_bytes{64U * 1024U * 1024U};
    std::size_t maximum_declared_bytes{64U * 1024U * 1024U};
    std::size_t maximum_tag_body_bytes{64U * 1024U * 1024U};
    std::size_t maximum_tags{262144};
    std::size_t maximum_sprites{65535};
    std::size_t maximum_nesting_depth{64};
    std::size_t maximum_name_bytes{4096};
};

enum class GfxDocumentSignature : std::uint8_t {
    unknown,
    gfx,
    swf,
};

enum class GfxDocumentErrorCode : std::uint8_t {
    none,
    invalid_argument,
    allocation_failed,
    input_too_large,
    unsupported_signature,
    invalid_declared_length,
    invalid_movie_header,
    truncated_tag_header,
    truncated_long_tag_length,
    tag_body_out_of_bounds,
    tag_body_too_large,
    missing_end_tag,
    data_after_end_tag,
    tag_limit_exceeded,
    sprite_limit_exceeded,
    nesting_limit_exceeded,
    malformed_tag,
    malformed_define_sprite,
    malformed_placement,
    duplicate_character_id,
    invalid_character_id,
    character_id_exhausted,
    stale_handle,
    tag_not_found,
    invalid_edit,
    output_too_large,
    output_validation_failed,
};

enum class GfxDocumentErrorContext : std::uint8_t {
    none,
    file_header,
    movie_header,
    tag_header,
    tag_body,
    sprite,
    placement,
    character_index,
    edit,
    serialization,
    validation,
};

// Deliberately allocation-free. Diagnostic code may translate these fields to
// text outside the parser; error construction itself remains safe during an
// allocation failure.
struct GfxDocumentError {
    GfxDocumentErrorCode code{GfxDocumentErrorCode::none};
    GfxDocumentErrorContext context{GfxDocumentErrorContext::none};
    std::size_t byte_offset{};
    std::uint16_t tag_code{};
    std::uint16_t character_id{};
    std::uint32_t detail{};

    [[nodiscard]] bool success() const noexcept {
        return code == GfxDocumentErrorCode::none;
    }
};

// Handles never contain byte offsets. A successful edit increments the
// document revision, making every previously issued handle detectably stale.
struct GfxTagHandle {
    std::uint64_t token{};
    std::uint64_t revision{};
    std::uint64_t document_identity{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return token != 0 && revision != 0 && document_identity != 0;
    }

    friend bool operator==(const GfxTagHandle&, const GfxTagHandle&) = default;
};

enum class GfxTagHeaderForm : std::uint8_t {
    automatic,
    short_form,
    long_form,
};

struct GfxTagInfo {
    GfxTagHandle handle{};
    GfxTagHandle parent_sprite{};
    std::uint16_t code{};
    std::size_t body_size{};
    std::size_t ordinal{};
    std::size_t nesting_depth{};
    GfxTagHeaderForm header_form{GfxTagHeaderForm::automatic};
    bool defines_character{};
    std::uint16_t character_id{};
};

struct GfxSpriteInfo {
    GfxTagHandle definition{};
    GfxTagHandle parent_sprite{};
    std::uint16_t character_id{};
    std::uint16_t frame_count{};
    std::size_t child_count{};
    std::size_t nesting_depth{};
};

struct GfxMatrixTranslation {
    std::int32_t x{};
    std::int32_t y{};
};

// PlaceObject2 and PlaceObject3 are indexed far enough to expose their common
// structural identity. Fields after the instance name (filters, clip actions,
// and other renderer data) stay opaque and are preserved byte-for-byte.
struct GfxPlacementInfo {
    GfxTagHandle tag{};
    GfxTagHandle owner_sprite{};
    std::uint16_t tag_code{};
    std::uint16_t depth{};
    std::uint16_t character_id{};
    std::string name{};
    GfxMatrixTranslation translation{};
    // Raw PlaceObject2/3 flag bytes. flags2 is zero for PlaceObject2. These
    // remain available even when a producer appends a PlaceObject3 extension
    // that the structural index deliberately leaves opaque.
    std::uint8_t flags{};
    std::uint8_t flags2{};
    bool has_character{};
    bool has_matrix{};
    bool has_name{};
    bool move{};
    bool is_place_object3{};
    // True only when the optional PlaceObject3 visibility byte was safely
    // decoded. Callers can inspect flags2 for the authored HasVisible bit
    // when preceding extension fields made that byte's offset ambiguous.
    bool has_visible{};
    bool visible{};
};

struct GfxCharacterDefinitionInfo {
    std::uint16_t character_id{};
    std::uint16_t tag_code{};
    GfxTagHandle tag{};
};

struct GfxRawTag {
    std::uint16_t code{};
    std::vector<std::uint8_t> body{};
    GfxTagHeaderForm header_form{GfxTagHeaderForm::automatic};
};

enum class GfxEditKind : std::uint8_t {
    replace,
    replace_body,
    insert_before,
    insert_after,
    erase,
    set_sprite_frame_count,
};

// A batch is applied to a private copy and committed only after the resulting
// movie serializes and completely reparses. All targets must come from the
// same current document revision.
struct GfxEdit {
    GfxEditKind kind{GfxEditKind::replace_body};
    GfxTagHandle target{};
    GfxRawTag tag{};
    std::uint16_t frame_count{};

    [[nodiscard]] static GfxEdit replace(
        GfxTagHandle target,
        GfxRawTag tag);
    [[nodiscard]] static GfxEdit replace_body(
        GfxTagHandle target,
        std::vector<std::uint8_t> body);
    [[nodiscard]] static GfxEdit insert_before(
        GfxTagHandle target,
        GfxRawTag tag);
    [[nodiscard]] static GfxEdit insert_after(
        GfxTagHandle target,
        GfxRawTag tag);
    [[nodiscard]] static GfxEdit erase(GfxTagHandle target) noexcept;
    [[nodiscard]] static GfxEdit set_sprite_frame_count(
        GfxTagHandle sprite,
        std::uint16_t frame_count) noexcept;
};

class GfxDocument;
struct GfxDocumentParseResult;
struct GfxDocumentBytesResult;
struct GfxRawTagResult;
struct GfxCharacterIdResult;
struct GfxCharacterIdsResult;

class GfxDocument {
public:
    GfxDocument();
    GfxDocument(GfxDocument&&) noexcept;
    GfxDocument& operator=(GfxDocument&&) noexcept;
    GfxDocument(const GfxDocument&);
    GfxDocument& operator=(const GfxDocument&);
    ~GfxDocument();

    [[nodiscard]] static GfxDocumentParseResult parse(
        std::span<const std::uint8_t> input,
        GfxDocumentLimits limits = {});

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] GfxDocumentSignature signature() const noexcept;
    [[nodiscard]] std::uint8_t version() const noexcept;
    [[nodiscard]] std::uint32_t declared_length() const noexcept;
    [[nodiscard]] std::size_t physical_size() const noexcept;
    [[nodiscard]] std::size_t trailing_size() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] const GfxDocumentLimits& limits() const noexcept;

    // Returned spans remain valid until the document is edited or assigned.
    // Their records contain stable logical handles, never backing-buffer
    // offsets or pointers.
    [[nodiscard]] std::span<const GfxTagInfo> tags() const noexcept;
    [[nodiscard]] std::span<const GfxSpriteInfo> sprites() const noexcept;
    [[nodiscard]] std::span<const GfxPlacementInfo> placements() const noexcept;
    [[nodiscard]] std::span<const GfxCharacterDefinitionInfo>
        character_definitions() const noexcept;

    [[nodiscard]] std::optional<GfxTagInfo> tag(GfxTagHandle handle) const noexcept;
    [[nodiscard]] std::optional<GfxSpriteInfo> sprite(
        GfxTagHandle definition) const noexcept;
    [[nodiscard]] std::optional<GfxSpriteInfo> sprite_by_character_id(
        std::uint16_t character_id) const noexcept;
    [[nodiscard]] std::optional<GfxCharacterDefinitionInfo> character_definition(
        std::uint16_t character_id) const noexcept;

    [[nodiscard]] GfxRawTagResult copy_tag(GfxTagHandle handle) const;
    [[nodiscard]] GfxCharacterIdResult first_free_character_id(
        std::uint16_t first = 1) const noexcept;
    [[nodiscard]] GfxCharacterIdsResult first_free_character_ids(
        std::size_t count,
        std::uint16_t first = 1) const;

    [[nodiscard]] GfxDocumentError apply(std::span<const GfxEdit> edits);
    [[nodiscard]] GfxDocumentBytesResult serialize_and_validate() const;

    // Reports tag codes whose supported layouts can define a movie character.
    // Some codes have an additional body-level condition (notably
    // DefineExternalImage2, whose idType must be IDTYPE_NONE).
    [[nodiscard]] static bool can_define_character(
        std::uint16_t tag_code) noexcept;

private:
    struct Storage;

    Storage* storage_{};
};

struct GfxDocumentParseResult {
    GfxDocument document{};
    GfxDocumentError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

struct GfxDocumentBytesResult {
    std::vector<std::uint8_t> bytes{};
    GfxDocumentError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

struct GfxRawTagResult {
    GfxRawTag tag{};
    GfxDocumentError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

struct GfxCharacterIdResult {
    std::uint16_t character_id{};
    GfxDocumentError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

struct GfxCharacterIdsResult {
    std::vector<std::uint16_t> character_ids{};
    GfxDocumentError error{};

    [[nodiscard]] bool success() const noexcept { return error.success(); }
};

[[nodiscard]] const char* gfx_document_error_name(
    GfxDocumentErrorCode code) noexcept;
[[nodiscard]] const char* gfx_document_error_context_name(
    GfxDocumentErrorContext context) noexcept;

} // namespace erui::gfx
