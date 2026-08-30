#pragma once

#include <filesystem>

namespace erui::host::log {

bool initialize(const std::filesystem::path& path) noexcept;
void shutdown() noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void write(const char* format, ...) noexcept;

} // namespace erui::host::log
