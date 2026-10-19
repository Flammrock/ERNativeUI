#include "tab_configuration.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <set>
#include <utility>

namespace erui::detail {
namespace {

constexpr std::int32_t kMaximumDelayMs = 5000;
constexpr std::int32_t kMinimumRepeatDelayMs = 20;
constexpr std::int32_t kMaximumNavigationMargin = 3;
static_assert(kMaximumRetainedTabDiagnostics >= 1);
static_assert(kMaximumTabDiagnosticFieldBytes >= 3);

enum class Section : std::uint8_t {
    none,
    other,
    tabs,
    tab_order,
};

std::string_view trim_ascii_space(std::string_view value) noexcept {
    while (!value.empty() &&
        (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
        (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool equals_ascii_case_insensitive(
    std::string_view left,
    std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        unsigned char lhs = static_cast<unsigned char>(left[index]);
        unsigned char rhs = static_cast<unsigned char>(right[index]);
        if (lhs >= 'A' && lhs <= 'Z') lhs = static_cast<unsigned char>(lhs + 32);
        if (rhs >= 'A' && rhs <= 'Z') rhs = static_cast<unsigned char>(rhs + 32);
        if (lhs != rhs) return false;
    }
    return true;
}

bool parse_int32(std::string_view text, std::int32_t& output) noexcept {
    text = trim_ascii_space(text);
    if (text.empty()) return false;

    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) return false;

    std::uint64_t magnitude{};
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), magnitude, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return false;
    }

    const std::uint64_t positive_max = static_cast<std::uint64_t>(
        (std::numeric_limits<std::int32_t>::max)());
    const std::uint64_t negative_max = positive_max + 1;
    if ((!negative && magnitude > positive_max) ||
        (negative && magnitude > negative_max)) {
        return false;
    }
    if (negative && magnitude == negative_max) {
        output = (std::numeric_limits<std::int32_t>::min)();
    } else {
        const std::int32_t signed_magnitude =
            static_cast<std::int32_t>(magnitude);
        output = negative ? -signed_magnitude : signed_magnitude;
    }
    return true;
}

std::string copy_bounded_diagnostic_field(std::string_view value) {
    if (value.size() <= kMaximumTabDiagnosticFieldBytes) {
        return std::string(value);
    }
    std::string result(value.substr(
        0, kMaximumTabDiagnosticFieldBytes - 3));
    result += "...";
    return result;
}

void append_suppression_diagnostic(
    std::vector<TabConfigurationDiagnostic>& diagnostics) {
    if (!diagnostics.empty() &&
        diagnostics.back().code ==
            TabConfigurationDiagnosticCode::diagnostics_suppressed) {
        return;
    }
    if (diagnostics.size() >= kMaximumRetainedTabDiagnostics) {
        return;
    }
    diagnostics.push_back({
        TabConfigurationSeverity::warning,
        TabConfigurationDiagnosticCode::diagnostics_suppressed,
        0,
        0,
        {},
        {},
        {}});
}

void add_diagnostic(
    std::vector<TabConfigurationDiagnostic>& diagnostics,
    TabConfigurationSeverity severity,
    TabConfigurationDiagnosticCode code,
    std::size_t source_line,
    std::string_view section = {},
    std::string_view key = {},
    std::string_view value = {},
    std::size_t related_source_line = 0) {
    if (!diagnostics.empty() &&
        diagnostics.back().code ==
            TabConfigurationDiagnosticCode::diagnostics_suppressed) {
        return;
    }
    if (diagnostics.size() >= kMaximumRetainedTabDiagnostics - 1) {
        append_suppression_diagnostic(diagnostics);
        return;
    }
    diagnostics.push_back({
        severity,
        code,
        source_line,
        related_source_line,
        copy_bounded_diagnostic_field(section),
        copy_bounded_diagnostic_field(key),
        copy_bounded_diagnostic_field(value)});
}

void add_diagnostic(
    TabConfiguration& configuration,
    TabConfigurationSeverity severity,
    TabConfigurationDiagnosticCode code,
    std::size_t source_line,
    std::string_view section = {},
    std::string_view key = {},
    std::string_view value = {},
    std::size_t related_source_line = 0) {
    add_diagnostic(
        configuration.diagnostics,
        severity,
        code,
        source_line,
        section,
        key,
        value,
        related_source_line);
}

struct SettingSeen final {
    bool navigation_margin{};
    bool minimum_tab_slots{};
    bool dwell_enabled{};
    bool initial_delay{};
    bool repeat_delay{};
    bool decay{};
    bool minimum_delay{};
    std::size_t navigation_margin_line{};
    std::size_t minimum_tab_slots_line{};
    std::size_t dwell_enabled_line{};
    std::size_t initial_delay_line{};
    std::size_t repeat_delay_line{};
    std::size_t decay_line{};
    std::size_t minimum_delay_line{};
};

void assign_setting(
    TabConfiguration& configuration,
    SettingSeen& seen,
    std::string_view key,
    std::string_view value,
    std::size_t line) {
    const bool known = key == "NavigationMargin" ||
        key == "MinimumTabSlots" ||
        key == "MouseDwellEnabled" ||
        key == "MouseDwellInitialDelayMs" ||
        key == "MouseDwellRepeatDelayMs" ||
        key == "MouseDwellDecayMs" ||
        key == "MouseDwellMinimumDelayMs";
    if (!known) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::unknown_tabs_setting,
            line,
            "Tabs",
            key,
            value);
        return;
    }

    std::int32_t parsed{};
    if (!parse_int32(value, parsed)) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::invalid_tabs_setting,
            line,
            "Tabs",
            key,
            value);
        return;
    }

    bool* was_seen{};
    std::size_t* previous_line{};
    bool valid = true;
    if (key == "NavigationMargin") {
        was_seen = &seen.navigation_margin;
        previous_line = &seen.navigation_margin_line;
        valid = parsed >= 0 && parsed <= kMaximumNavigationMargin;
        if (valid) {
            configuration.settings.navigation_margin =
                static_cast<std::uint32_t>(parsed);
        }
    } else if (key == "MinimumTabSlots") {
        was_seen = &seen.minimum_tab_slots;
        previous_line = &seen.minimum_tab_slots_line;
        valid = parsed >=
                static_cast<std::int32_t>(kDefaultMinimumTabSlots) &&
            parsed <= static_cast<std::int32_t>(
                kMaximumConfigurableMinimumTabSlots);
        if (valid) {
            configuration.settings.minimum_tab_slots =
                static_cast<std::uint32_t>(parsed);
        }
    } else if (key == "MouseDwellEnabled") {
        was_seen = &seen.dwell_enabled;
        previous_line = &seen.dwell_enabled_line;
        valid = parsed == 0 || parsed == 1;
        if (valid) configuration.settings.mouse_dwell_enabled = parsed != 0;
    } else if (key == "MouseDwellInitialDelayMs") {
        was_seen = &seen.initial_delay;
        previous_line = &seen.initial_delay_line;
        valid = parsed >= 0 && parsed <= kMaximumDelayMs;
        if (valid) {
            configuration.settings.mouse_dwell_timing.initial_delay_ms =
                static_cast<std::uint64_t>(parsed);
        }
    } else if (key == "MouseDwellRepeatDelayMs") {
        was_seen = &seen.repeat_delay;
        previous_line = &seen.repeat_delay_line;
        valid = parsed >= kMinimumRepeatDelayMs &&
            parsed <= kMaximumDelayMs;
        if (valid) {
            configuration.settings.mouse_dwell_timing.repeat_delay_ms =
                static_cast<std::uint64_t>(parsed);
        }
    } else if (key == "MouseDwellDecayMs") {
        was_seen = &seen.decay;
        previous_line = &seen.decay_line;
        valid = parsed >= 0 && parsed <= kMaximumDelayMs;
        if (valid) {
            // Zero intentionally means a fixed cadence.
            configuration.settings.mouse_dwell_timing.repeat_decay_ms =
                static_cast<std::uint64_t>(parsed);
        }
    } else if (key == "MouseDwellMinimumDelayMs") {
        was_seen = &seen.minimum_delay;
        previous_line = &seen.minimum_delay_line;
        valid = parsed >= kMinimumRepeatDelayMs &&
            parsed <= kMaximumDelayMs;
        if (valid) {
            configuration.settings.mouse_dwell_timing
                .minimum_repeat_delay_ms =
                static_cast<std::uint64_t>(parsed);
        }
    }

    if (!valid) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::invalid_tabs_setting,
            line,
            "Tabs",
            key,
            value);
        return;
    }
    if (*was_seen) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::duplicate_tabs_setting,
            line,
            "Tabs",
            key,
            value,
            *previous_line);
    }
    *was_seen = true;
    *previous_line = line;
}

void parse_order_override(
    TabConfiguration& configuration,
    std::string_view key,
    std::string_view value,
    std::size_t line) {
    std::string_view provider_id{};
    std::string_view tab_id{};
    if (!split_tab_stable_key(key, provider_id, tab_id)) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::invalid_order_key,
            line,
            "TabOrder",
            key,
            value);
        return;
    }

    std::int32_t parsed{};
    if (!parse_int32(value, parsed)) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::invalid_order_value,
            line,
            "TabOrder",
            key,
            value);
        return;
    }

    const std::string stable_key(key);
    const auto existing = configuration.order_overrides.find(stable_key);
    if (existing == configuration.order_overrides.end() &&
        configuration.order_overrides.size() >= kMaximumTabOrderOverrides) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::error,
            TabConfigurationDiagnosticCode::override_limit_exceeded,
            line,
            "TabOrder",
            key,
            value);
        return;
    }

    if (existing != configuration.order_overrides.end()) {
        add_diagnostic(
            configuration,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::duplicate_order_override,
            line,
            "TabOrder",
            key,
            value,
            existing->second.source_line);
    }
    configuration.order_overrides.insert_or_assign(
        stable_key,
        TabOrderOverride{
            std::string(provider_id),
            std::string(tab_id),
            stable_key,
            parsed,
            line});
}

void append_escaped(std::string& output, std::string_view value) {
    constexpr char digits[] = "0123456789ABCDEF";
    const bool truncated = value.size() > kMaximumTabDiagnosticFieldBytes;
    if (truncated) {
        value = value.substr(0, kMaximumTabDiagnosticFieldBytes - 3);
    }
    output.push_back('"');
    for (const unsigned char byte : value) {
        switch (byte) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\t': output += "\\t"; break;
        case '\r': output += "\\r"; break;
        case '\n': output += "\\n"; break;
        default:
            if (byte < 0x20 || byte > 0x7E) {
                output += "\\x";
                output.push_back(digits[(byte >> 4) & 0x0F]);
                output.push_back(digits[byte & 0x0F]);
            } else {
                output.push_back(static_cast<char>(byte));
            }
            break;
        }
    }
    if (truncated) output += "...";
    output.push_back('"');
}

template <typename Integer>
void append_integer(std::string& output, Integer value) {
    std::array<char, 32> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    output.append(buffer.data(), converted.ptr);
}

} // namespace

bool is_valid_tab_identifier(std::string_view identifier) noexcept {
    if (identifier.empty() ||
        identifier.size() > kMaximumTabIdentifierBytes) {
        return false;
    }
    return std::all_of(
        identifier.begin(), identifier.end(), [](unsigned char value) {
            return (value >= 'A' && value <= 'Z') ||
                (value >= 'a' && value <= 'z') ||
                (value >= '0' && value <= '9') || value == '.' ||
                value == '_' || value == '-';
        });
}

bool split_tab_stable_key(
    std::string_view stable_key,
    std::string_view& provider_id,
    std::string_view& tab_id) noexcept {
    const std::size_t separator = stable_key.find('/');
    if (separator == std::string_view::npos ||
        stable_key.find('/', separator + 1) != std::string_view::npos) {
        return false;
    }
    const std::string_view provider = stable_key.substr(0, separator);
    const std::string_view tab = stable_key.substr(separator + 1);
    if (!is_valid_tab_identifier(provider) ||
        !is_valid_tab_identifier(tab)) {
        return false;
    }
    provider_id = provider;
    tab_id = tab;
    return true;
}

std::string make_tab_stable_key(
    std::string_view provider_id,
    std::string_view tab_id) {
    if (!is_valid_tab_identifier(provider_id) ||
        !is_valid_tab_identifier(tab_id)) {
        return {};
    }
    std::string result{};
    result.reserve(provider_id.size() + 1 + tab_id.size());
    result.append(provider_id);
    result.push_back('/');
    result.append(tab_id);
    return result;
}

TabConfiguration parse_tab_configuration(std::string_view ini_bytes) {
    TabConfiguration result{};
    if (ini_bytes.size() > kMaximumTabConfigurationBytes) {
        add_diagnostic(
            result,
            TabConfigurationSeverity::error,
            TabConfigurationDiagnosticCode::file_too_large,
            0);
        return result;
    }

    if (ini_bytes.size() >= 3 &&
        static_cast<unsigned char>(ini_bytes[0]) == 0xEF &&
        static_cast<unsigned char>(ini_bytes[1]) == 0xBB &&
        static_cast<unsigned char>(ini_bytes[2]) == 0xBF) {
        ini_bytes.remove_prefix(3);
    }

    Section current = Section::none;
    std::string current_name{};
    SettingSeen setting_seen{};
    std::size_t line_number = 0;
    while (!ini_bytes.empty()) {
        ++line_number;
        const std::size_t newline = ini_bytes.find('\n');
        std::string_view line = newline == std::string_view::npos
            ? ini_bytes
            : ini_bytes.substr(0, newline);
        if (newline == std::string_view::npos) {
            ini_bytes = {};
        } else {
            ini_bytes.remove_prefix(newline + 1);
        }
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        line = trim_ascii_space(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }

        if (line.front() == '[') {
            if (line.size() < 2 || line.back() != ']') {
                add_diagnostic(
                    result,
                    TabConfigurationSeverity::warning,
                    TabConfigurationDiagnosticCode::malformed_section,
                    line_number,
                    {},
                    {},
                    line);
                current = Section::none;
                current_name.clear();
                continue;
            }
            const std::string_view name = trim_ascii_space(
                line.substr(1, line.size() - 2));
            current_name.assign(name);
            if (name == "Tabs") {
                current = Section::tabs;
            } else if (name == "TabOrder") {
                current = Section::tab_order;
            } else {
                current = Section::other;
                if (equals_ascii_case_insensitive(name, "Tabs") ||
                    equals_ascii_case_insensitive(name, "TabOrder")) {
                    add_diagnostic(
                        result,
                        TabConfigurationSeverity::warning,
                        TabConfigurationDiagnosticCode::incorrect_section_case,
                        line_number,
                        name);
                }
            }
            continue;
        }

        if (current != Section::tabs && current != Section::tab_order) {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            add_diagnostic(
                result,
                TabConfigurationSeverity::warning,
                TabConfigurationDiagnosticCode::malformed_record,
                line_number,
                current_name,
                line);
            continue;
        }
        const std::string_view key = trim_ascii_space(line.substr(0, equals));
        const std::string_view value =
            trim_ascii_space(line.substr(equals + 1));
        if (key.empty()) {
            add_diagnostic(
                result,
                TabConfigurationSeverity::warning,
                TabConfigurationDiagnosticCode::malformed_record,
                line_number,
                current_name,
                key,
                value);
            continue;
        }

        if (current == Section::tabs) {
            assign_setting(result, setting_seen, key, value, line_number);
        } else {
            parse_order_override(result, key, value, line_number);
        }
    }

    auto& timing = result.settings.mouse_dwell_timing;
    if (timing.minimum_repeat_delay_ms > timing.repeat_delay_ms) {
        add_diagnostic(
            result,
            TabConfigurationSeverity::warning,
            TabConfigurationDiagnosticCode::minimum_delay_clamped,
            setting_seen.minimum_delay_line != 0
                ? setting_seen.minimum_delay_line
                : setting_seen.repeat_delay_line,
            "Tabs",
            "MouseDwellMinimumDelayMs");
        timing.minimum_repeat_delay_ms = timing.repeat_delay_ms;
    }
    return result;
}

TabOrderResolution resolve_tab_order(
    std::span<const TabOrderCandidate> candidates,
    const TabConfiguration& configuration) {
    TabOrderResolution result{};
    std::set<std::string, std::less<>> registered_keys{};
    std::set<std::string, std::less<>> matched_overrides{};
    result.ordered_tabs.reserve(candidates.size());

    for (const TabOrderCandidate& candidate : candidates) {
        const std::string stable_key = make_tab_stable_key(
            candidate.provider_id, candidate.tab_id);
        if (stable_key.empty()) {
            result.complete = false;
            add_diagnostic(
                result.diagnostics,
                TabConfigurationSeverity::error,
                TabConfigurationDiagnosticCode::invalid_registered_tab,
                0,
                {},
                candidate.provider_id,
                candidate.tab_id);
            continue;
        }
        if (!registered_keys.insert(stable_key).second) {
            result.complete = false;
            add_diagnostic(
                result.diagnostics,
                TabConfigurationSeverity::error,
                TabConfigurationDiagnosticCode::duplicate_registered_tab,
                0,
                {},
                stable_key,
                {});
            continue;
        }

        std::optional<std::int32_t> player_override{};
        const auto override = configuration.order_overrides.find(stable_key);
        if (override != configuration.order_overrides.end()) {
            player_override = override->second.order;
            matched_overrides.insert(stable_key);
        }
        result.ordered_tabs.push_back({
            0,
            candidate.source_index,
            std::string(candidate.provider_id),
            std::string(candidate.tab_id),
            stable_key,
            candidate.preferred_order,
            player_override,
            player_override.value_or(candidate.preferred_order)});
    }

    std::sort(
        result.ordered_tabs.begin(),
        result.ordered_tabs.end(),
        [](const ResolvedTabOrderEntry& left,
           const ResolvedTabOrderEntry& right) {
            if (left.effective_order != right.effective_order) {
                return left.effective_order < right.effective_order;
            }
            if (left.provider_id != right.provider_id) {
                return left.provider_id < right.provider_id;
            }
            return left.tab_id < right.tab_id;
        });
    for (std::size_t index = 0; index < result.ordered_tabs.size(); ++index) {
        result.ordered_tabs[index].position = index + 1;
    }

    for (const auto& [key, override] : configuration.order_overrides) {
        if (!matched_overrides.contains(key)) {
            result.stale_overrides.push_back(override);
        }
    }
    return result;
}

TabDiagnosticsReport make_tab_diagnostics_report(
    const TabConfiguration& configuration,
    const TabOrderResolution& resolution) {
    TabDiagnosticsReport result{};
    result.settings = configuration.settings;
    result.order_complete = resolution.complete;
    result.ordered_tabs = resolution.ordered_tabs;
    result.stale_overrides = resolution.stale_overrides;
    result.diagnostics.reserve(kMaximumRetainedTabDiagnostics);

    bool suppressed = false;
    const auto append_source = [&](const auto& source) {
        for (const TabConfigurationDiagnostic& diagnostic : source) {
            if (diagnostic.code ==
                TabConfigurationDiagnosticCode::diagnostics_suppressed) {
                suppressed = true;
                break;
            }
            if (result.diagnostics.size() >=
                kMaximumRetainedTabDiagnostics - 1) {
                suppressed = true;
                break;
            }
            add_diagnostic(
                result.diagnostics,
                diagnostic.severity,
                diagnostic.code,
                diagnostic.source_line,
                diagnostic.section,
                diagnostic.key,
                diagnostic.value,
                diagnostic.related_source_line);
        }
    };
    append_source(configuration.diagnostics);
    if (!suppressed) append_source(resolution.diagnostics);
    if (suppressed) append_suppression_diagnostic(result.diagnostics);
    return result;
}

std::string serialize_tab_diagnostics_report(
    const TabDiagnosticsReport& report) {
    constexpr std::string_view suppression_record =
        "diagnostic severity=warning code=diagnostics_suppressed "
        "source_line=0 related_source_line=0 section=\"\" key=\"\" "
        "value=\"\"\n";
    static_assert(
        suppression_record.size() < kMaximumTabDiagnosticsReportBytes);

    std::string output{};
    output.reserve(4096);
    output += "format=ERNativeUI-tabs-v1\n";
    output += "setting NavigationMargin=";
    append_integer(output, report.settings.navigation_margin);
    output += "\nsetting MinimumTabSlots=";
    append_integer(output, report.settings.minimum_tab_slots);
    output += "\nsetting MouseDwellEnabled=";
    output.push_back(report.settings.mouse_dwell_enabled ? '1' : '0');
    output += "\nsetting MouseDwellInitialDelayMs=";
    append_integer(
        output, report.settings.mouse_dwell_timing.initial_delay_ms);
    output += "\nsetting MouseDwellRepeatDelayMs=";
    append_integer(
        output, report.settings.mouse_dwell_timing.repeat_delay_ms);
    output += "\nsetting MouseDwellDecayMs=";
    append_integer(
        output, report.settings.mouse_dwell_timing.repeat_decay_ms);
    output += "\nsetting MouseDwellMinimumDelayMs=";
    append_integer(
        output, report.settings.mouse_dwell_timing.minimum_repeat_delay_ms);
    output += "\norder_complete=";
    output.push_back(report.order_complete ? '1' : '0');
    output.push_back('\n');

    bool suppressed = false;
    bool accept_records = true;
    const auto append_record = [&](const auto& append) {
        if (!accept_records) return false;
        const std::size_t record_start = output.size();
        append();
        // Always retain room for the suppression record. This makes the bound
        // deterministic without ever cutting an escaped record mid-line.
        if (output.size() >
            kMaximumTabDiagnosticsReportBytes - suppression_record.size()) {
            output.resize(record_start);
            suppressed = true;
            accept_records = false;
            return false;
        }
        return true;
    };

    for (const ResolvedTabOrderEntry& entry : report.ordered_tabs) {
        if (!append_record([&] {
                output += "tab position=";
                append_integer(output, entry.position);
                output += " key=";
                append_escaped(output, entry.stable_key);
                output += " preferred_order=";
                append_integer(output, entry.preferred_order);
                output += " player_override=";
                if (entry.player_override) {
                    append_integer(output, *entry.player_override);
                } else {
                    output += "none";
                }
                output += " effective_order=";
                append_integer(output, entry.effective_order);
                output.push_back('\n');
            })) {
            break;
        }
    }
    if (accept_records) {
        for (const TabOrderOverride& stale : report.stale_overrides) {
            if (!append_record([&] {
                    output += "stale_override key=";
                    append_escaped(output, stale.stable_key);
                    output += " order=";
                    append_integer(output, stale.order);
                    output += " source_line=";
                    append_integer(output, stale.source_line);
                    output.push_back('\n');
                })) {
                break;
            }
        }
    }
    std::size_t diagnostic_details = 0;
    if (accept_records) {
        for (const TabConfigurationDiagnostic& diagnostic :
             report.diagnostics) {
            if (diagnostic.code ==
                TabConfigurationDiagnosticCode::diagnostics_suppressed) {
                suppressed = true;
                break;
            }
            if (diagnostic_details >=
                kMaximumRetainedTabDiagnostics - 1) {
                suppressed = true;
                break;
            }
            if (!append_record([&] {
                    output += "diagnostic severity=";
                    output += tab_configuration_severity_name(
                        diagnostic.severity);
                    output += " code=";
                    output += tab_configuration_diagnostic_name(
                        diagnostic.code);
                    output += " source_line=";
                    append_integer(output, diagnostic.source_line);
                    output += " related_source_line=";
                    append_integer(output, diagnostic.related_source_line);
                    output += " section=";
                    append_escaped(output, diagnostic.section);
                    output += " key=";
                    append_escaped(output, diagnostic.key);
                    output += " value=";
                    append_escaped(output, diagnostic.value);
                    output.push_back('\n');
                })) {
                break;
            }
            ++diagnostic_details;
        }
    }
    if (suppressed) output.append(suppression_record);
    return output;
}

const char* tab_configuration_severity_name(
    TabConfigurationSeverity severity) noexcept {
    switch (severity) {
    case TabConfigurationSeverity::warning: return "warning";
    case TabConfigurationSeverity::error: return "error";
    default: return "unknown";
    }
}

const char* tab_configuration_diagnostic_name(
    TabConfigurationDiagnosticCode code) noexcept {
    switch (code) {
    case TabConfigurationDiagnosticCode::file_too_large:
        return "file_too_large";
    case TabConfigurationDiagnosticCode::malformed_section:
        return "malformed_section";
    case TabConfigurationDiagnosticCode::incorrect_section_case:
        return "incorrect_section_case";
    case TabConfigurationDiagnosticCode::malformed_record:
        return "malformed_record";
    case TabConfigurationDiagnosticCode::unknown_tabs_setting:
        return "unknown_tabs_setting";
    case TabConfigurationDiagnosticCode::invalid_tabs_setting:
        return "invalid_tabs_setting";
    case TabConfigurationDiagnosticCode::duplicate_tabs_setting:
        return "duplicate_tabs_setting";
    case TabConfigurationDiagnosticCode::minimum_delay_clamped:
        return "minimum_delay_clamped";
    case TabConfigurationDiagnosticCode::invalid_order_key:
        return "invalid_order_key";
    case TabConfigurationDiagnosticCode::invalid_order_value:
        return "invalid_order_value";
    case TabConfigurationDiagnosticCode::duplicate_order_override:
        return "duplicate_order_override";
    case TabConfigurationDiagnosticCode::override_limit_exceeded:
        return "override_limit_exceeded";
    case TabConfigurationDiagnosticCode::invalid_registered_tab:
        return "invalid_registered_tab";
    case TabConfigurationDiagnosticCode::duplicate_registered_tab:
        return "duplicate_registered_tab";
    case TabConfigurationDiagnosticCode::diagnostics_suppressed:
        return "diagnostics_suppressed";
    default: return "unknown";
    }
}

} // namespace erui::detail
