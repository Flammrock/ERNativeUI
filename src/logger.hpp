#pragma once

#include "log_level.hpp"

#include <filesystem>

namespace erui::host::log {

bool initialize(const std::filesystem::path& path) noexcept;
void set_diagnostics_enabled(bool enabled) noexcept;
void shutdown() noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void write(const char* format, ...) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void write(LogLevel level, const char* format, ...) noexcept;

} // namespace erui::host::log
