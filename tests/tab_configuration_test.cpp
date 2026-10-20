#include "tab_configuration.hpp"

#include "test_assertions.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace {

using erui::detail::TabConfiguration;
using erui::detail::TabConfigurationDiagnosticCode;

std::size_t count_diagnostic(
    const TabConfiguration& configuration,
    TabConfigurationDiagnosticCode code) {
    return static_cast<std::size_t>(std::count_if(
        configuration.diagnostics.begin(),
        configuration.diagnostics.end(),
        [code](const auto& diagnostic) {
            return diagnostic.code == code;
        }));
}

std::size_t count_occurrences(
    std::string_view text,
    std::string_view needle) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) !=
        std::string_view::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

void test_identifier_and_stable_key_contract() {
    using erui::detail::is_valid_tab_identifier;
    using erui::detail::make_tab_stable_key;
    using erui::detail::split_tab_stable_key;

    ERUI_TEST_CHECK(is_valid_tab_identifier("flammrock.native-tweaks"));
    ERUI_TEST_CHECK(is_valid_tab_identifier("Main_Tab-2"));
    ERUI_TEST_CHECK(!is_valid_tab_identifier(""));
    ERUI_TEST_CHECK(!is_valid_tab_identifier("with/slash"));
    ERUI_TEST_CHECK(!is_valid_tab_identifier("with space"));
    ERUI_TEST_CHECK(!is_valid_tab_identifier(std::string(256, 'a')));

    ERUI_TEST_CHECK(
        make_tab_stable_key("Author.Mod", "Settings") ==
        "Author.Mod/Settings");
    ERUI_TEST_CHECK(make_tab_stable_key("bad/provider", "main").empty());

    std::string_view provider{};
    std::string_view tab{};
    ERUI_TEST_CHECK(split_tab_stable_key(
        "Author.Mod/Settings", provider, tab));
    ERUI_TEST_CHECK(provider == "Author.Mod");
    ERUI_TEST_CHECK(tab == "Settings");
    ERUI_TEST_CHECK(!split_tab_stable_key("Author.Mod", provider, tab));
    ERUI_TEST_CHECK(!split_tab_stable_key("Author/Mod/Main", provider, tab));
    ERUI_TEST_CHECK(!split_tab_stable_key("/Main", provider, tab));
}

void test_defaults_and_exact_case_sections() {
    using erui::detail::parse_tab_configuration;

    const auto defaults = parse_tab_configuration("");
    ERUI_TEST_CHECK(defaults.settings.navigation_margin == 1);
    ERUI_TEST_CHECK(defaults.settings.minimum_tab_slots == 8);
    ERUI_TEST_CHECK(defaults.settings.mouse_dwell_enabled);
    ERUI_TEST_CHECK(
        defaults.settings.mouse_dwell_timing.initial_delay_ms == 250);
    ERUI_TEST_CHECK(
        defaults.settings.mouse_dwell_timing.repeat_delay_ms == 200);
    ERUI_TEST_CHECK(
        defaults.settings.mouse_dwell_timing.repeat_decay_ms == 20);
    ERUI_TEST_CHECK(
        defaults.settings.mouse_dwell_timing.minimum_repeat_delay_ms == 50);

    const auto wrong_case = parse_tab_configuration(
        "[tabs]\n"
        "NavigationMargin=3\n"
        "[taborder]\n"
        "Author.Mod/Main=9\n");
    ERUI_TEST_CHECK(wrong_case.settings.navigation_margin == 1);
    ERUI_TEST_CHECK(wrong_case.order_overrides.empty());
    ERUI_TEST_CHECK(count_diagnostic(
        wrong_case,
        TabConfigurationDiagnosticCode::incorrect_section_case) == 2);
}

void test_tabs_settings_validation() {
    using erui::detail::parse_tab_configuration;

    const auto configured = parse_tab_configuration(
        "\xEF\xBB\xBF[Tabs]\r\n"
        "MinimumTabSlots = 8\r\n"
        "NavigationMargin = 2\r\n"
        "MouseDwellEnabled = 0\r\n"
        "MouseDwellInitialDelayMs = 0\r\n"
        "MouseDwellRepeatDelayMs = 200\r\n"
        "MouseDwellDecayMs = 0\r\n"
        "MouseDwellMinimumDelayMs = 20\r\n");
    ERUI_TEST_CHECK(configured.settings.navigation_margin == 2);
    ERUI_TEST_CHECK(configured.settings.minimum_tab_slots == 8);
    ERUI_TEST_CHECK(!configured.settings.mouse_dwell_enabled);
    ERUI_TEST_CHECK(
        configured.settings.mouse_dwell_timing.initial_delay_ms == 0);
    ERUI_TEST_CHECK(
        configured.settings.mouse_dwell_timing.repeat_delay_ms == 200);
    // Zero decay is retained and therefore means fixed repeat cadence.
    ERUI_TEST_CHECK(
        configured.settings.mouse_dwell_timing.repeat_decay_ms == 0);
    ERUI_TEST_CHECK(
        configured.settings.mouse_dwell_timing.minimum_repeat_delay_ms == 20);
    ERUI_TEST_CHECK(configured.diagnostics.empty());

    const auto invalid = parse_tab_configuration(
        "[Tabs]\n"
        "MinimumTabSlots=7\n"
        "NavigationMargin=-1\n"
        "MouseDwellEnabled=2\n"
        "MouseDwellInitialDelayMs=5001\n"
        "MouseDwellRepeatDelayMs=19\n"
        "MouseDwellDecayMs=-1\n"
        "MouseDwellMinimumDelayMs=19\n"
        "UnknownSetting=1\n");
    ERUI_TEST_CHECK(invalid.settings ==
        erui::detail::TabNavigationSettings{});
    ERUI_TEST_CHECK(count_diagnostic(
        invalid, TabConfigurationDiagnosticCode::invalid_tabs_setting) == 7);
    ERUI_TEST_CHECK(invalid.settings.minimum_tab_slots == 8);
    ERUI_TEST_CHECK(count_diagnostic(
        invalid, TabConfigurationDiagnosticCode::unknown_tabs_setting) == 1);

    const auto duplicate_and_clamped = parse_tab_configuration(
        "[Tabs]\n"
        "NavigationMargin=1\n"
        "NavigationMargin=3\n"
        "NavigationMargin=99\n"
        "MouseDwellRepeatDelayMs=40\n"
        "MouseDwellMinimumDelayMs=50\n");
    ERUI_TEST_CHECK(duplicate_and_clamped.settings.navigation_margin == 3);
    ERUI_TEST_CHECK(
        duplicate_and_clamped.settings.mouse_dwell_timing.repeat_delay_ms ==
        40);
    ERUI_TEST_CHECK(
        duplicate_and_clamped.settings.mouse_dwell_timing
            .minimum_repeat_delay_ms == 40);
    ERUI_TEST_CHECK(count_diagnostic(
        duplicate_and_clamped,
        TabConfigurationDiagnosticCode::duplicate_tabs_setting) == 1);
    ERUI_TEST_CHECK(count_diagnostic(
        duplicate_and_clamped,
        TabConfigurationDiagnosticCode::invalid_tabs_setting) == 1);
    ERUI_TEST_CHECK(count_diagnostic(
        duplicate_and_clamped,
        TabConfigurationDiagnosticCode::minimum_delay_clamped) == 1);

    const auto minimum_slots = parse_tab_configuration(
        "[Tabs]\n"
        "MinimumTabSlots=8\n"
        "MinimumTabSlots=9\n"
        "MinimumTabSlots=10\n"
        "MinimumTabSlots=11\n");
    ERUI_TEST_CHECK(minimum_slots.settings.minimum_tab_slots == 10);
    ERUI_TEST_CHECK(count_diagnostic(
        minimum_slots,
        TabConfigurationDiagnosticCode::duplicate_tabs_setting) == 2);
    ERUI_TEST_CHECK(count_diagnostic(
        minimum_slots,
        TabConfigurationDiagnosticCode::invalid_tabs_setting) == 1);
}

void test_minimum_tab_slots_range() {
    using erui::detail::parse_tab_configuration;

    for (const std::uint32_t accepted : std::array{8u, 9u, 10u}) {
        const auto configuration = parse_tab_configuration(
            "[Tabs]\nMinimumTabSlots=" + std::to_string(accepted) + "\n");
        ERUI_TEST_CHECK(
            configuration.settings.minimum_tab_slots == accepted);
        ERUI_TEST_CHECK(count_diagnostic(
            configuration,
            TabConfigurationDiagnosticCode::invalid_tabs_setting) == 0);
    }

    for (const std::uint32_t rejected : std::array{7u, 11u, 14u}) {
        const auto configuration = parse_tab_configuration(
            "[Tabs]\nMinimumTabSlots=" + std::to_string(rejected) + "\n");
        ERUI_TEST_CHECK(configuration.settings.minimum_tab_slots ==
            erui::detail::kDefaultMinimumTabSlots);
        ERUI_TEST_CHECK(count_diagnostic(
            configuration,
            TabConfigurationDiagnosticCode::invalid_tabs_setting) == 1);
    }
}

void test_order_parser_duplicate_and_malformed_policy() {
    using erui::detail::parse_tab_configuration;

    const auto parsed = parse_tab_configuration(
        "[Logging]\n"
        "EnableLog=1\n"
        "[TabOrder]\n"
        "Author.Mod/Main = -2147483648\n"
        "Author.Mod/Other = +2147483647\n"
        "Author.Mod/Main = not-a-number\n"
        "Author.Mod/Main = 7\n"
        "bad key/main = 3\n"
        "Author.Mod/Overflow = 2147483648\n"
        "missing-equals\n");
    ERUI_TEST_CHECK(parsed.order_overrides.size() == 2);
    ERUI_TEST_CHECK(parsed.order_overrides.at("Author.Mod/Main").order == 7);
    ERUI_TEST_CHECK(
        parsed.order_overrides.at("Author.Mod/Main").source_line == 7);
    ERUI_TEST_CHECK(
        parsed.order_overrides.at("Author.Mod/Other").order ==
        (std::numeric_limits<std::int32_t>::max)());
    ERUI_TEST_CHECK(count_diagnostic(
        parsed,
        TabConfigurationDiagnosticCode::duplicate_order_override) == 1);
    ERUI_TEST_CHECK(count_diagnostic(
        parsed, TabConfigurationDiagnosticCode::invalid_order_value) == 2);
    ERUI_TEST_CHECK(count_diagnostic(
        parsed, TabConfigurationDiagnosticCode::invalid_order_key) == 1);
    ERUI_TEST_CHECK(count_diagnostic(
        parsed, TabConfigurationDiagnosticCode::malformed_record) == 1);

    const auto& duplicate = *std::find_if(
        parsed.diagnostics.begin(),
        parsed.diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code ==
                TabConfigurationDiagnosticCode::duplicate_order_override;
        });
    ERUI_TEST_CHECK(duplicate.source_line == 7);
    ERUI_TEST_CHECK(duplicate.related_source_line == 4);
}

void test_deterministic_resolution_and_stale_overrides() {
    using erui::detail::TabOrderCandidate;
    using erui::detail::parse_tab_configuration;
    using erui::detail::resolve_tab_order;

    const auto configuration = parse_tab_configuration(
        "[TabOrder]\n"
        "z.provider/main=-5\n"
        "A.provider/Missing=3\n"
        "a.provider/Other=10\n");
    const std::array candidates{
        TabOrderCandidate{"z.provider", "main", 10, 100},
        TabOrderCandidate{"a.provider", "Main", 10, 101},
        TabOrderCandidate{"A.provider", "main", 10, 102},
        TabOrderCandidate{"first.provider", "main", -1, 103},
        TabOrderCandidate{"a.provider", "Other", 99, 104},
    };
    const auto resolution = resolve_tab_order(candidates, configuration);
    ERUI_TEST_CHECK(resolution.complete);
    ERUI_TEST_CHECK(resolution.ordered_tabs.size() == 5);
    ERUI_TEST_CHECK(resolution.ordered_tabs[0].stable_key ==
        "z.provider/main");
    ERUI_TEST_CHECK(resolution.ordered_tabs[0].effective_order == -5);
    ERUI_TEST_CHECK(resolution.ordered_tabs[0].player_override == -5);
    ERUI_TEST_CHECK(resolution.ordered_tabs[1].stable_key ==
        "first.provider/main");
    // Exact ASCII tie-breaking places uppercase A before lowercase a.
    ERUI_TEST_CHECK(resolution.ordered_tabs[2].stable_key ==
        "A.provider/main");
    ERUI_TEST_CHECK(resolution.ordered_tabs[3].stable_key ==
        "a.provider/Main");
    ERUI_TEST_CHECK(resolution.ordered_tabs[4].stable_key ==
        "a.provider/Other");
    ERUI_TEST_CHECK(resolution.ordered_tabs[4].effective_order == 10);
    ERUI_TEST_CHECK(resolution.ordered_tabs[4].preferred_order == 99);
    ERUI_TEST_CHECK(resolution.stale_overrides.size() == 1);
    ERUI_TEST_CHECK(resolution.stale_overrides[0].stable_key ==
        "A.provider/Missing");
    for (std::size_t index = 0;
         index < resolution.ordered_tabs.size(); ++index) {
        ERUI_TEST_CHECK(resolution.ordered_tabs[index].position == index + 1);
    }

    // Case is identity, not presentation: this override does not match the
    // differently cased registration and is therefore reported stale.
    const auto exact_case = parse_tab_configuration(
        "[TabOrder]\nAuthor.Provider/Main=1\n");
    const std::array exact_candidates{
        TabOrderCandidate{"author.provider", "Main", 9, 0}};
    const auto exact_resolution =
        resolve_tab_order(exact_candidates, exact_case);
    ERUI_TEST_CHECK(
        exact_resolution.ordered_tabs[0].effective_order == 9);
    ERUI_TEST_CHECK(exact_resolution.stale_overrides.size() == 1);
}

void test_invalid_catalogue_is_not_publishable() {
    using erui::detail::TabOrderCandidate;
    using erui::detail::parse_tab_configuration;
    using erui::detail::resolve_tab_order;

    const auto configuration = parse_tab_configuration("");
    const std::array candidates{
        TabOrderCandidate{"provider", "main", 0, 1},
        TabOrderCandidate{"provider", "main", 1, 2},
        TabOrderCandidate{"bad/provider", "main", 2, 3},
    };
    const auto resolution = resolve_tab_order(candidates, configuration);
    ERUI_TEST_CHECK(!resolution.complete);
    ERUI_TEST_CHECK(resolution.ordered_tabs.size() == 1);
    ERUI_TEST_CHECK(resolution.diagnostics.size() == 2);
    ERUI_TEST_CHECK(resolution.diagnostics[0].code ==
        TabConfigurationDiagnosticCode::duplicate_registered_tab);
    ERUI_TEST_CHECK(resolution.diagnostics[1].code ==
        TabConfigurationDiagnosticCode::invalid_registered_tab);
}

void test_diagnostic_report_is_stable_and_complete() {
    using erui::detail::TabOrderCandidate;
    using erui::detail::make_tab_diagnostics_report;
    using erui::detail::parse_tab_configuration;
    using erui::detail::resolve_tab_order;
    using erui::detail::serialize_tab_diagnostics_report;

    const auto configuration = parse_tab_configuration(
        "[Tabs]\n"
        "MinimumTabSlots=10\n"
        "MouseDwellMinimumDelayMs=20\n"
        "Unknown=bad\\\"value\n"
        "[TabOrder]\n"
        "provider/main=10\n"
        "stale/tab=20\n");
    const std::array candidates{
        TabOrderCandidate{"provider", "main", 50, 7}};
    const auto resolution = resolve_tab_order(candidates, configuration);
    const auto report = make_tab_diagnostics_report(
        configuration, resolution);
    const std::string first = serialize_tab_diagnostics_report(report);
    const std::string second = serialize_tab_diagnostics_report(report);
    ERUI_TEST_CHECK(first == second);
    ERUI_TEST_CHECK(first.starts_with("format=ERNativeUI-tabs-v1\n"));
    ERUI_TEST_CHECK(first.find(
        "setting MinimumTabSlots=10\n") != std::string::npos);
    ERUI_TEST_CHECK(first.find(
        "setting MouseDwellMinimumDelayMs=20\n") != std::string::npos);
    ERUI_TEST_CHECK(first.find(
        "tab position=1 key=\"provider/main\" preferred_order=50 "
        "player_override=10 effective_order=10\n") !=
        std::string::npos);
    // The runtime-only source index is deliberately absent: registration
    // timing must not make the player-facing report change between runs.
    ERUI_TEST_CHECK(first.find("source_index") == std::string::npos);
    ERUI_TEST_CHECK(first.find(
        "stale_override key=\"stale/tab\" order=20 source_line=7\n") !=
        std::string::npos);
    ERUI_TEST_CHECK(first.find(
        "code=unknown_tabs_setting") != std::string::npos);
    ERUI_TEST_CHECK(first.find("value=\"bad\\\\\\\"value\"") !=
        std::string::npos);
}

void test_oversized_input_fails_closed() {
    using erui::detail::kMaximumTabConfigurationBytes;
    using erui::detail::parse_tab_configuration;

    const auto configuration = parse_tab_configuration(
        std::string(kMaximumTabConfigurationBytes + 1, 'x'));
    ERUI_TEST_CHECK(configuration.order_overrides.empty());
    ERUI_TEST_CHECK(configuration.settings ==
        erui::detail::TabNavigationSettings{});
    ERUI_TEST_CHECK(count_diagnostic(
        configuration,
        TabConfigurationDiagnosticCode::file_too_large) == 1);
}

void test_near_limit_malformed_input_has_bounded_diagnostics() {
    using erui::detail::kMaximumRetainedTabDiagnostics;
    using erui::detail::kMaximumTabConfigurationBytes;
    using erui::detail::kMaximumTabDiagnosticFieldBytes;
    using erui::detail::make_tab_diagnostics_report;
    using erui::detail::parse_tab_configuration;
    using erui::detail::resolve_tab_order;
    using erui::detail::serialize_tab_diagnostics_report;

    std::string input = "[Tabs]\nUnknown=";
    input.append(kMaximumTabDiagnosticFieldBytes * 8, 'x');
    input.push_back('\n');
    constexpr std::string_view malformed = "malformed\n";
    while (input.size() + malformed.size() + 1 <=
        kMaximumTabConfigurationBytes) {
        input.append(malformed);
    }
    // Fill the accepted one-MiB boundary with one ignored comment line.
    if (input.size() < kMaximumTabConfigurationBytes) {
        input.push_back('#');
        input.append(kMaximumTabConfigurationBytes - input.size(), 'x');
    }
    ERUI_TEST_CHECK(input.size() == kMaximumTabConfigurationBytes);

    const auto configuration = parse_tab_configuration(input);
    ERUI_TEST_CHECK(
        configuration.diagnostics.size() ==
        kMaximumRetainedTabDiagnostics);
    ERUI_TEST_CHECK(
        configuration.diagnostics.back().code ==
        TabConfigurationDiagnosticCode::diagnostics_suppressed);
    ERUI_TEST_CHECK(count_diagnostic(
        configuration,
        TabConfigurationDiagnosticCode::diagnostics_suppressed) == 1);
    for (const auto& diagnostic : configuration.diagnostics) {
        ERUI_TEST_CHECK(
            diagnostic.section.size() <= kMaximumTabDiagnosticFieldBytes);
        ERUI_TEST_CHECK(
            diagnostic.key.size() <= kMaximumTabDiagnosticFieldBytes);
        ERUI_TEST_CHECK(
            diagnostic.value.size() <= kMaximumTabDiagnosticFieldBytes);
    }

    const std::array<erui::detail::TabOrderCandidate, 0> no_tabs{};
    const auto resolution = resolve_tab_order(no_tabs, configuration);
    const auto report = make_tab_diagnostics_report(
        configuration, resolution);
    ERUI_TEST_CHECK(
        report.diagnostics.size() == kMaximumRetainedTabDiagnostics);
    const std::string serialized = serialize_tab_diagnostics_report(report);
    ERUI_TEST_CHECK(
        serialized.size() <=
        erui::detail::kMaximumTabDiagnosticsReportBytes);
    ERUI_TEST_CHECK(count_occurrences(
        serialized,
        "code=diagnostics_suppressed") == 1);
}

void test_near_limit_valid_input_has_bounded_report() {
    using erui::detail::kMaximumTabConfigurationBytes;
    using erui::detail::kMaximumTabDiagnosticsReportBytes;
    using erui::detail::kMaximumTabOrderOverrides;
    using erui::detail::make_tab_diagnostics_report;
    using erui::detail::parse_tab_configuration;
    using erui::detail::resolve_tab_order;
    using erui::detail::serialize_tab_diagnostics_report;

    std::string input = "[TabOrder]\n";
    for (std::size_t index = 0;
         index < kMaximumTabOrderOverrides; ++index) {
        input += "provider";
        input += std::to_string(index);
        input.append(220, 'a');
        input += "/main=";
        input += std::to_string(index);
        input.push_back('\n');
    }
    ERUI_TEST_CHECK(input.size() < kMaximumTabConfigurationBytes);
    input.push_back('#');
    input.append(kMaximumTabConfigurationBytes - input.size(), 'x');
    ERUI_TEST_CHECK(input.size() == kMaximumTabConfigurationBytes);

    const auto configuration = parse_tab_configuration(input);
    ERUI_TEST_CHECK(
        configuration.order_overrides.size() ==
        kMaximumTabOrderOverrides);
    ERUI_TEST_CHECK(configuration.diagnostics.empty());

    const std::array<erui::detail::TabOrderCandidate, 0> no_tabs{};
    const auto resolution = resolve_tab_order(no_tabs, configuration);
    ERUI_TEST_CHECK(
        resolution.stale_overrides.size() ==
        kMaximumTabOrderOverrides);
    const auto report = make_tab_diagnostics_report(
        configuration, resolution);
    const std::string serialized = serialize_tab_diagnostics_report(report);
    ERUI_TEST_CHECK(serialized.size() <= kMaximumTabDiagnosticsReportBytes);
    ERUI_TEST_CHECK(
        serialized.size() > kMaximumTabDiagnosticsReportBytes - 4096);
    ERUI_TEST_CHECK(count_occurrences(
        serialized,
        "code=diagnostics_suppressed") == 1);
}

} // namespace

int main() {
    test_identifier_and_stable_key_contract();
    test_defaults_and_exact_case_sections();
    test_tabs_settings_validation();
    test_minimum_tab_slots_range();
    test_order_parser_duplicate_and_malformed_policy();
    test_deterministic_resolution_and_stale_overrides();
    test_invalid_catalogue_is_not_publishable();
    test_diagnostic_report_is_stable_and_complete();
    test_oversized_input_fails_closed();
    test_near_limit_malformed_input_has_bounded_diagnostics();
    test_near_limit_valid_input_has_bounded_report();
    return 0;
}
