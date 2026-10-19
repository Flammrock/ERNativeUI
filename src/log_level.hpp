#pragma once

#include <cstdint>

namespace erui {

// Internal runtime logging contract. The original numeric values are retained
// because LogSink callbacks may have been compiled with the earlier internal
// layout. New levels are appended rather than inserted by severity.
enum class LogLevel : std::uint8_t {
    info = 0,
    warning = 1,
    error = 2,
    trace = 3,
    verbose = 4,
    fatal = 5,
};

namespace detail {

[[nodiscard]] constexpr bool is_diagnostic_log_level(
    LogLevel level) noexcept {
    return level == LogLevel::verbose || level == LogLevel::trace;
}

[[nodiscard]] constexpr bool should_emit_log_level(
    LogLevel level,
    bool diagnostics_enabled) noexcept {
    return diagnostics_enabled || !is_diagnostic_log_level(level);
}

// ERROR and FATAL records describe failures that may immediately terminate
// bootstrap or precede a process crash. Ordinary writes remain in the Windows
// file cache; forcing every INFO/TRACE record to stable storage would stall the
// game thread during repeated UI activity.
[[nodiscard]] constexpr bool should_flush_log_level(
    LogLevel level) noexcept {
    return level == LogLevel::error || level == LogLevel::fatal;
}

[[nodiscard]] constexpr const char* log_level_name(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::info:
        return "INFO";
    case LogLevel::warning:
        return "WARN";
    case LogLevel::error:
        return "ERROR";
    case LogLevel::trace:
        return "TRACE";
    case LogLevel::verbose:
        return "VERBOSE";
    case LogLevel::fatal:
        return "FATAL";
    }
    return "UNKNOWN";
}

} // namespace detail
} // namespace erui
