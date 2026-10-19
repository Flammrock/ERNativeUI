#pragma once

#include "gfx/settings_gfx_transformer.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace erui::gfx {

enum class SettingsGfxRuntimeTarget : std::uint8_t {
    none,
    game_options,
    advanced_settings,
};

enum class SettingsGfxRuntimeOutcome : std::uint8_t {
    ignored,
    failed,
    unchanged,
    transformed,
};

enum class SettingsGfxRuntimeErrorCode : std::uint8_t {
    none,
    invalid_target,
    source_changed,
    input_too_large,
    parse_failed,
    unexpected_host,
    transform_failed,
    serialization_failed,
    output_too_large,
    allocation_failed,
};

// These movies are tens of KiB in the supported game build. Four MiB leaves
// generous room for third-party additions while bounding synchronous work on
// Scaleform's file-opening thread.
inline constexpr std::size_t kSettingsGfxRuntimeMaximumBytes =
    4U * 1024U * 1024U;

struct SettingsGfxRuntimeError {
    SettingsGfxRuntimeErrorCode code{SettingsGfxRuntimeErrorCode::none};
    GfxDocumentError document_error{};
    SettingsGfxError settings_error{};

    [[nodiscard]] bool success() const noexcept {
        return code == SettingsGfxRuntimeErrorCode::none;
    }
};

struct SettingsGfxRuntimeResult {
    SettingsGfxRuntimeTarget target{SettingsGfxRuntimeTarget::none};
    SettingsGfxRuntimeOutcome outcome{SettingsGfxRuntimeOutcome::ignored};
    SettingsGfxInspection before{};
    SettingsGfxInspection after{};
    SettingsGfxRuntimeError error{};
    const std::uint8_t* replacement_bytes{};
    std::size_t replacement_size{};
    bool cache_hit{};

    [[nodiscard]] bool success() const noexcept {
        return outcome != SettingsGfxRuntimeOutcome::failed &&
            error.success();
    }

    [[nodiscard]] bool has_replacement() const noexcept {
        return outcome == SettingsGfxRuntimeOutcome::transformed &&
            replacement_bytes != nullptr && replacement_size != 0;
    }
};

// Only the two paths observed at CSScaleformFileOpener::OpenFile are
// recognized. Deliberately avoid case, separator, suffix, or basename
// normalization: a near match must never redirect an unrelated Scaleform
// resource.
[[nodiscard]] SettingsGfxRuntimeTarget classify_settings_gfx_runtime_path(
    std::string_view path) noexcept;

[[nodiscard]] SettingsGfxRequirements settings_gfx_runtime_requirements(
    SettingsGfxRuntimeTarget target,
    std::uint16_t minimum_tab_slots = 8) noexcept;

// Owns at most one immutable source/result pair for each target movie.
// Published replacement pointers remain stable until this object is
// destroyed. Production intentionally retains one instance for the complete
// process lifetime; tests may use ordinary scoped instances.
class SettingsGfxRuntimeCache final {
public:
    explicit SettingsGfxRuntimeCache(
        std::uint16_t minimum_tab_slots = 8) noexcept;
    ~SettingsGfxRuntimeCache();

    SettingsGfxRuntimeCache(const SettingsGfxRuntimeCache&) = delete;
    SettingsGfxRuntimeCache& operator=(const SettingsGfxRuntimeCache&) = delete;
    SettingsGfxRuntimeCache(SettingsGfxRuntimeCache&&) = delete;
    SettingsGfxRuntimeCache& operator=(SettingsGfxRuntimeCache&&) = delete;

    [[nodiscard]] bool ready() const noexcept;

    [[nodiscard]] SettingsGfxRuntimeResult prepare(
        SettingsGfxRuntimeTarget target,
        std::span<const std::uint8_t> source) noexcept;

    [[nodiscard]] SettingsGfxRuntimeResult prepare_path(
        std::string_view path,
        std::span<const std::uint8_t> source) noexcept;

private:
    struct Storage;
    Storage* storage_{};
};

[[nodiscard]] const char* settings_gfx_runtime_target_name(
    SettingsGfxRuntimeTarget target) noexcept;
[[nodiscard]] const char* settings_gfx_runtime_outcome_name(
    SettingsGfxRuntimeOutcome outcome) noexcept;
[[nodiscard]] const char* settings_gfx_runtime_error_name(
    SettingsGfxRuntimeErrorCode error) noexcept;

} // namespace erui::gfx
