#pragma once

#include "tab_pointer_dwell.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace erui::detail {

inline constexpr std::size_t kMaximumTabIdentifierBytes = 255;
inline constexpr std::size_t kMaximumTabConfigurationBytes = 1024 * 1024;
inline constexpr std::size_t kMaximumTabOrderOverrides = 4096;
// Diagnostic input is untrusted. Keep one slot for a deterministic
// diagnostics_suppressed record so a malformed near-limit INI cannot amplify
// into an unbounded collection of owned strings.
inline constexpr std::size_t kMaximumRetainedTabDiagnostics = 256;
inline constexpr std::size_t kMaximumTabDiagnosticFieldBytes = 512;
// The player-facing report can preserve all 1,024 ordered-tab records even
// when both provider and tab identifiers use their maximum public lengths.
// Pathological stale-override or diagnostic output is still truncated by
// whole records and terminated with diagnostics_suppressed.
inline constexpr std::size_t kMaximumTabDiagnosticsReportBytes = 1024 * 1024;
// Eight is Elden Ring's stock active System-tab count. The validated native
// record storage and runtime GFX completion path support at most ten physical
// slots. This setting is a floor, not a cap: a compatible incoming layout that
// already exposes more slots is never reduced.
inline constexpr std::uint32_t kDefaultMinimumTabSlots = 8;
inline constexpr std::uint32_t kMaximumConfigurableMinimumTabSlots = 10;

// Host-owned settings. The parser keeps these defaults in one place so the
// runtime adapter, generated default INI, diagnostics, and tests cannot drift.
struct TabNavigationSettings final {
    std::uint32_t navigation_margin{1};
    std::uint32_t minimum_tab_slots{kDefaultMinimumTabSlots};
    bool mouse_dwell_enabled{true};
    TabPointerDwellTiming mouse_dwell_timing{};

    friend constexpr bool operator==(
        const TabNavigationSettings& left,
        const TabNavigationSettings& right) noexcept {
        return left.navigation_margin == right.navigation_margin &&
            left.minimum_tab_slots == right.minimum_tab_slots &&
            left.mouse_dwell_enabled == right.mouse_dwell_enabled &&
            left.mouse_dwell_timing.initial_delay_ms ==
                right.mouse_dwell_timing.initial_delay_ms &&
            left.mouse_dwell_timing.repeat_delay_ms ==
                right.mouse_dwell_timing.repeat_delay_ms &&
            left.mouse_dwell_timing.repeat_decay_ms ==
                right.mouse_dwell_timing.repeat_decay_ms &&
            left.mouse_dwell_timing.minimum_repeat_delay_ms ==
                right.mouse_dwell_timing.minimum_repeat_delay_ms;
    }
};

enum class TabConfigurationSeverity : std::uint8_t {
    warning,
    error,
};

enum class TabConfigurationDiagnosticCode : std::uint8_t {
    file_too_large,
    malformed_section,
    incorrect_section_case,
    malformed_record,
    unknown_tabs_setting,
    invalid_tabs_setting,
    duplicate_tabs_setting,
    minimum_delay_clamped,
    invalid_order_key,
    invalid_order_value,
    duplicate_order_override,
    override_limit_exceeded,
    invalid_registered_tab,
    duplicate_registered_tab,
    diagnostics_suppressed,
};

struct TabConfigurationDiagnostic final {
    TabConfigurationSeverity severity{TabConfigurationSeverity::warning};
    TabConfigurationDiagnosticCode code{
        TabConfigurationDiagnosticCode::malformed_record};
    std::size_t source_line{};
    std::size_t related_source_line{};
    std::string section{};
    std::string key{};
    std::string value{};
};

struct TabOrderOverride final {
    std::string provider_id{};
    std::string tab_id{};
    std::string stable_key{};
    std::int32_t order{};
    std::size_t source_line{};
};

struct TabConfiguration final {
    TabNavigationSettings settings{};
    // std::less<> gives exact, bytewise ASCII ordering and heterogeneous
    // lookup without introducing locale or Windows-profile semantics.
    std::map<std::string, TabOrderOverride, std::less<>> order_overrides{};
    std::vector<TabConfigurationDiagnostic> diagnostics{};
};

// Provider and tab identifiers share the public machine-identifier grammar:
// 1..255 case-sensitive ASCII letters, digits, '.', '_', or '-'. A stable tab
// key is their unambiguous provider_id/tab_id composition.
[[nodiscard]] bool is_valid_tab_identifier(
    std::string_view identifier) noexcept;
[[nodiscard]] bool split_tab_stable_key(
    std::string_view stable_key,
    std::string_view& provider_id,
    std::string_view& tab_id) noexcept;
[[nodiscard]] std::string make_tab_stable_key(
    std::string_view provider_id,
    std::string_view tab_id);

// Parses the complete ERNativeUI.ini byte stream directly. Section and key
// matching is deliberately exact-case; this must not be replaced with the
// case-insensitive Windows profile APIs.
[[nodiscard]] TabConfiguration parse_tab_configuration(
    std::string_view ini_bytes);

struct TabOrderCandidate final {
    std::string_view provider_id{};
    std::string_view tab_id{};
    std::int32_t preferred_order{};
    // Opaque index returned to the integration layer after sorting.
    std::size_t source_index{};
};

struct ResolvedTabOrderEntry final {
    std::size_t position{}; // one-based presentation position
    std::size_t source_index{};
    std::string provider_id{};
    std::string tab_id{};
    std::string stable_key{};
    std::int32_t preferred_order{};
    std::optional<std::int32_t> player_override{};
    std::int32_t effective_order{};
};

struct TabOrderResolution final {
    // False means at least one registration was invalid or duplicated. Valid
    // unique records are still present for complete diagnostics, but runtime
    // integration must reject the catalogue rather than partially publish it.
    bool complete{true};
    std::vector<ResolvedTabOrderEntry> ordered_tabs{};
    std::vector<TabOrderOverride> stale_overrides{};
    std::vector<TabConfigurationDiagnostic> diagnostics{};
};

// Player values replace the corresponding modder preference. Final ties are
// always resolved by exact ASCII provider_id and then tab_id, never DLL load
// or registration timing.
[[nodiscard]] TabOrderResolution resolve_tab_order(
    std::span<const TabOrderCandidate> candidates,
    const TabConfiguration& configuration);

struct TabDiagnosticsReport final {
    TabNavigationSettings settings{};
    bool order_complete{true};
    std::vector<ResolvedTabOrderEntry> ordered_tabs{};
    std::vector<TabOrderOverride> stale_overrides{};
    std::vector<TabConfigurationDiagnostic> diagnostics{};
};

[[nodiscard]] TabDiagnosticsReport make_tab_diagnostics_report(
    const TabConfiguration& configuration,
    const TabOrderResolution& resolution);

// Stable, locale-independent text intended to be written verbatim to
// ERNativeUI-tabs.log. Arbitrary malformed input is escaped onto one line.
[[nodiscard]] std::string serialize_tab_diagnostics_report(
    const TabDiagnosticsReport& report);

[[nodiscard]] const char* tab_configuration_severity_name(
    TabConfigurationSeverity severity) noexcept;
[[nodiscard]] const char* tab_configuration_diagnostic_name(
    TabConfigurationDiagnosticCode code) noexcept;

} // namespace erui::detail
