#include "runtime_log.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace erui::detail {
namespace {

std::atomic<LogSink> g_sink{nullptr};

} // namespace

void set_log_sink(LogSink sink) noexcept {
    g_sink.store(sink, std::memory_order_release);
}

void logf(LogLevel level, const char* format, ...) noexcept {
    LogSink sink = g_sink.load(std::memory_order_acquire);
    if (!sink || !format) {
        return;
    }

    char buffer[2048]{};
    va_list args;
    va_start(args, format);
#if defined(_MSC_VER)
    ::_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#else
    std::vsnprintf(buffer, sizeof(buffer), format, args);
#endif
    va_end(args);
    sink(level, buffer);
}

} // namespace erui::detail
