#include "logger.hpp"

#include "test_assertions.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace {

[[nodiscard]] std::filesystem::path test_log_path(const wchar_t* suffix) {
    std::error_code error{};
    std::filesystem::path directory =
        std::filesystem::temp_directory_path(error);
    if (error) {
        error.clear();
        directory = std::filesystem::current_path(error);
    }
    return directory /
        (L"ERNativeUI-logger-integration-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" + suffix +
            L".log");
}

[[nodiscard]] std::string read_all(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

void emit_every_level() {
    using erui::LogLevel;
    erui::host::log::write(LogLevel::info, "info marker");
    erui::host::log::write(LogLevel::warning, "warning marker");
    erui::host::log::write(LogLevel::error, "error marker");
    erui::host::log::write(LogLevel::fatal, "fatal marker");
    erui::host::log::write(LogLevel::verbose, "verbose marker");
    erui::host::log::write(LogLevel::trace, "trace marker");
}

void test_default_log() {
    const auto path = test_log_path(L"default");
    std::error_code error{};
    std::filesystem::remove(path, error);

    ERUI_TEST_CHECK(erui::host::log::initialize(path));
    erui::host::log::set_diagnostics_enabled(false);
    emit_every_level();
    erui::host::log::shutdown();

    const std::string contents = read_all(path);
    ERUI_TEST_CHECK(contents.find("[ERNativeUI/INFO] info marker") !=
        std::string::npos);
    ERUI_TEST_CHECK(contents.find("[ERNativeUI/WARN] warning marker") !=
        std::string::npos);
    ERUI_TEST_CHECK(contents.find("[ERNativeUI/ERROR] error marker") !=
        std::string::npos);
    ERUI_TEST_CHECK(contents.find("[ERNativeUI/FATAL] fatal marker") !=
        std::string::npos);
    ERUI_TEST_CHECK(contents.find("verbose marker") == std::string::npos);
    ERUI_TEST_CHECK(contents.find("trace marker") == std::string::npos);

    error.clear();
    std::filesystem::remove(path, error);
}

void test_diagnostic_log() {
    const auto path = test_log_path(L"diagnostic");
    std::error_code error{};
    std::filesystem::remove(path, error);

    ERUI_TEST_CHECK(erui::host::log::initialize(path));
    erui::host::log::set_diagnostics_enabled(true);
    emit_every_level();
    erui::host::log::shutdown();

    const std::string contents = read_all(path);
    ERUI_TEST_CHECK(contents.find("[ERNativeUI/VERBOSE] verbose marker") !=
        std::string::npos);
    ERUI_TEST_CHECK(contents.find("[ERNativeUI/TRACE] trace marker") !=
        std::string::npos);

    error.clear();
    std::filesystem::remove(path, error);
}

} // namespace

int main() {
    test_default_log();
    test_diagnostic_log();
    return 0;
}
