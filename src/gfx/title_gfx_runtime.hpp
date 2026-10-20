#pragma once

#include "gfx/title_gfx_transformer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace erui::gfx {

// The supported title movie is small. Four MiB leaves ample room for a
// compatible third-party replacement while bounding synchronous parsing and
// copying on Scaleform's file-opening thread.
inline constexpr std::size_t kTitleGfxRuntimeMaximumBytes =
    4U * 1024U * 1024U;

// Runtime publication mirrors API 1.3 and the transformer’s process-wide
// production line budget.
inline constexpr std::size_t kTitleGfxRuntimeMaximumLines = 16U;
inline constexpr std::size_t kTitleGfxRuntimeMaximumExternalOccupancy = 16U;

// Publication takes ownership of these strings. provider_id is retained for
// diagnostics; the transformer consumes text, color, and corner. Callers must
// supply validated UTF-8 single-line text.
struct TitleGfxRuntimeLine {
    std::string provider_id{};
    std::string text{};
    std::optional<TitleGfxRgb> color{};
    TitleGfxCorner corner{};
};

enum class TitleGfxRuntimePlanState : std::uint8_t {
    pending,
    ready_empty,
    ready_lines,
};

enum class TitleGfxRuntimePublishStatus : std::uint8_t {
    published,
    already_published,
    too_many_lines,
    allocation_failed,
};

struct TitleGfxRuntimePlanSummary {
    TitleGfxRuntimePlanState state{TitleGfxRuntimePlanState::pending};
    TitleGfxDlcState dlc_state{TitleGfxDlcState::unknown};
    std::size_t line_count{};
    std::size_t provider_count{};
    std::size_t custom_color_count{};
    std::size_t external_occupancy_count{};
};

enum class TitleGfxRuntimeOutcome : std::uint8_t {
    pending,
    empty,
    failed,
    unchanged,
    transformed,
};

// An exact identity for a title movie whose DLCVersion_0 field is known to
// retain Elden Ring's native Shadow of the Erdtree status semantics. Unknown
// or modified movies remain structurally transformable, but their DLC row is
// conservatively treated as occupied.
enum class TitleGfxRuntimeSourceProfile : std::uint8_t {
    unrecognized,
    elden_ring_2_7_1_app_1_17_1,
};

enum class TitleGfxRuntimeErrorCode : std::uint8_t {
    none,
    source_changed,
    input_too_large,
    parse_failed,
    transform_failed,
    serialization_failed,
    output_too_large,
    allocation_failed,
};

struct TitleGfxRuntimeError {
    TitleGfxRuntimeErrorCode code{TitleGfxRuntimeErrorCode::none};
    GfxDocumentError document_error{};
    TitleGfxError title_error{};

    [[nodiscard]] bool success() const noexcept {
        return code == TitleGfxRuntimeErrorCode::none;
    }
};

struct TitleGfxRuntimeResult {
    TitleGfxRuntimeOutcome outcome{TitleGfxRuntimeOutcome::pending};
    TitleGfxRuntimeSourceProfile source_profile{
        TitleGfxRuntimeSourceProfile::unrecognized};
    TitleGfxInspection before{};
    TitleGfxInspection after{};
    TitleGfxRuntimeError error{};
    const std::uint8_t* replacement_bytes{};
    std::size_t replacement_size{};
    bool dlc_row_reuse_permitted{};
    bool cache_hit{};

    [[nodiscard]] bool success() const noexcept {
        return outcome != TitleGfxRuntimeOutcome::failed && error.success();
    }

    [[nodiscard]] bool has_replacement() const noexcept {
        return outcome == TitleGfxRuntimeOutcome::transformed &&
            replacement_bytes != nullptr && replacement_size != 0U;
    }
};

// Exact matching is intentional. A basename, case-insensitive, or normalized
// match could redirect an unrelated Scaleform resource.
[[nodiscard]] bool is_title_gfx_runtime_path(
    std::string_view path) noexcept;

// Classification is byte-exact. Structural similarity is deliberately not
// sufficient to authorize reuse of a possibly repurposed DLCVersion_0 row.
[[nodiscard]] TitleGfxRuntimeSourceProfile classify_title_gfx_runtime_source(
    std::span<const std::uint8_t> source) noexcept;

// Owns one publish-once immutable line plan and at most one immutable
// source/result pair. Returned replacement pointers remain stable until this
// object is destroyed. Production retains its instance for the process
// lifetime; scoped destruction is intended only for tests with no live users.
class TitleGfxRuntimeCache final {
public:
    TitleGfxRuntimeCache() noexcept;
    ~TitleGfxRuntimeCache();

    TitleGfxRuntimeCache(const TitleGfxRuntimeCache&) = delete;
    TitleGfxRuntimeCache& operator=(const TitleGfxRuntimeCache&) = delete;
    TitleGfxRuntimeCache(TitleGfxRuntimeCache&&) = delete;
    TitleGfxRuntimeCache& operator=(TitleGfxRuntimeCache&&) = delete;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] TitleGfxRuntimePlanState plan_state() const noexcept;
    // Waits only while publication is pending. The returned state is still
    // `pending` when the finite deadline expires or the cache is unavailable.
    // Publication is immutable, so every non-pending result is terminal.
    [[nodiscard]] TitleGfxRuntimePlanState wait_for_plan(
        std::uint32_t maximum_wait_ms) noexcept;
    [[nodiscard]] TitleGfxRuntimePlanSummary plan_summary() const noexcept;
    // Empty means pending, out of range, or a ready-empty plan. A successful
    // view remains valid for this cache's process-lifetime ownership.
    [[nodiscard]] std::string_view provider_id_for_line(
        std::size_t line_index) const noexcept;

    // A rejected oversized publication leaves the state pending so the owner
    // can correct it. Once published, including an empty plan, the plan can
    // never be replaced.
    [[nodiscard]] TitleGfxRuntimePublishStatus publish(
        std::vector<TitleGfxRuntimeLine> lines,
        TitleGfxDlcState dlc_state,
        std::vector<TitleGfxTextOccupancy> external_occupancy = {}) noexcept;

    // Pending and empty plans do not inspect or retain source. A populated
    // cache recognizes both its exact original and its exact replacement.
    // Any other subsequent source fails open as source_changed.
    [[nodiscard]] TitleGfxRuntimeResult prepare(
        std::span<const std::uint8_t> source) noexcept;

private:
    struct Storage;
    Storage* storage_{};
};

[[nodiscard]] const char* title_gfx_runtime_plan_state_name(
    TitleGfxRuntimePlanState state) noexcept;
[[nodiscard]] const char* title_gfx_runtime_publish_status_name(
    TitleGfxRuntimePublishStatus status) noexcept;
[[nodiscard]] const char* title_gfx_runtime_outcome_name(
    TitleGfxRuntimeOutcome outcome) noexcept;
[[nodiscard]] const char* title_gfx_runtime_source_profile_name(
    TitleGfxRuntimeSourceProfile profile) noexcept;
[[nodiscard]] const char* title_gfx_runtime_error_name(
    TitleGfxRuntimeErrorCode error) noexcept;

} // namespace erui::gfx
