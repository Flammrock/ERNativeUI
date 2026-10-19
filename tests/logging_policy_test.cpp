#include "log_level.hpp"
#include "runtime_log.hpp"

#include "test_assertions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

struct CapturedRecord {
    erui::LogLevel level{};
    std::array<char, 64> message{};
};

std::array<CapturedRecord, 16> g_records{};
std::size_t g_record_count{};

void capture(erui::LogLevel level, const char* message) noexcept {
    if (g_record_count >= g_records.size()) return;
    CapturedRecord& record = g_records[g_record_count++];
    record.level = level;
    if (message) {
        std::strncpy(record.message.data(), message,
            record.message.size() - 1U);
    }
}

void reset_capture() noexcept {
    g_records = {};
    g_record_count = 0;
}

void emit_all_levels() {
    erui::detail::logf(erui::LogLevel::info, "info");
    erui::detail::logf(erui::LogLevel::warning, "warning");
    erui::detail::logf(erui::LogLevel::error, "error");
    erui::detail::logf(erui::LogLevel::fatal, "fatal");
    erui::detail::logf(erui::LogLevel::verbose, "verbose");
    erui::detail::logf(erui::LogLevel::trace, "trace");
}

} // namespace

int main() {
    // Preserve the numeric values already used by the internal LogSink
    // callback contract. Newly introduced levels are append-only.
    static_assert(static_cast<std::uint8_t>(erui::LogLevel::info) == 0);
    static_assert(static_cast<std::uint8_t>(erui::LogLevel::warning) == 1);
    static_assert(static_cast<std::uint8_t>(erui::LogLevel::error) == 2);
    static_assert(static_cast<std::uint8_t>(erui::LogLevel::trace) == 3);
    static_assert(static_cast<std::uint8_t>(erui::LogLevel::verbose) == 4);
    static_assert(static_cast<std::uint8_t>(erui::LogLevel::fatal) == 5);

    ERUI_TEST_CHECK(std::strcmp(erui::detail::log_level_name(
        erui::LogLevel::info), "INFO") == 0);
    ERUI_TEST_CHECK(std::strcmp(erui::detail::log_level_name(
        erui::LogLevel::warning), "WARN") == 0);
    ERUI_TEST_CHECK(std::strcmp(erui::detail::log_level_name(
        erui::LogLevel::error), "ERROR") == 0);
    ERUI_TEST_CHECK(std::strcmp(erui::detail::log_level_name(
        erui::LogLevel::fatal), "FATAL") == 0);
    ERUI_TEST_CHECK(std::strcmp(erui::detail::log_level_name(
        erui::LogLevel::verbose), "VERBOSE") == 0);
    ERUI_TEST_CHECK(std::strcmp(erui::detail::log_level_name(
        erui::LogLevel::trace), "TRACE") == 0);

    ERUI_TEST_CHECK(erui::detail::should_flush_log_level(
        erui::LogLevel::error));
    ERUI_TEST_CHECK(erui::detail::should_flush_log_level(
        erui::LogLevel::fatal));
    ERUI_TEST_CHECK(!erui::detail::should_flush_log_level(
        erui::LogLevel::info));
    ERUI_TEST_CHECK(!erui::detail::should_flush_log_level(
        erui::LogLevel::warning));
    ERUI_TEST_CHECK(!erui::detail::should_flush_log_level(
        erui::LogLevel::verbose));
    ERUI_TEST_CHECK(!erui::detail::should_flush_log_level(
        erui::LogLevel::trace));

    erui::detail::set_log_sink(&capture);
    erui::detail::set_diagnostic_logging(false);
    reset_capture();
    emit_all_levels();
    ERUI_TEST_CHECK(g_record_count == 4);
    ERUI_TEST_CHECK(g_records[0].level == erui::LogLevel::info);
    ERUI_TEST_CHECK(g_records[1].level == erui::LogLevel::warning);
    ERUI_TEST_CHECK(g_records[2].level == erui::LogLevel::error);
    ERUI_TEST_CHECK(g_records[3].level == erui::LogLevel::fatal);

    erui::detail::set_diagnostic_logging(true);
    reset_capture();
    emit_all_levels();
    ERUI_TEST_CHECK(g_record_count == 6);
    ERUI_TEST_CHECK(g_records[4].level == erui::LogLevel::verbose);
    ERUI_TEST_CHECK(g_records[5].level == erui::LogLevel::trace);
    ERUI_TEST_CHECK(std::strcmp(g_records[5].message.data(), "trace") == 0);

    erui::detail::set_log_sink(nullptr);
    reset_capture();
    emit_all_levels();
    ERUI_TEST_CHECK(g_record_count == 0);
    erui::detail::set_diagnostic_logging(false);
    return 0;
}
