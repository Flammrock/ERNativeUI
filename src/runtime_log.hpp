#pragma once

#include "runtime.hpp"

namespace erui::detail {

void set_log_sink(LogSink sink) noexcept;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void logf(LogLevel level, const char* format, ...) noexcept;

} // namespace erui::detail
