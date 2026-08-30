#include "logger.hpp"

#include <Windows.h>

#include <cstdarg>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace erui::host::log {
namespace {

HANDLE g_file = INVALID_HANDLE_VALUE;
SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic_bool g_enabled{};

void emit(const char* text, DWORD length) noexcept {
    OutputDebugStringA(text);

    AcquireSRWLockExclusive(&g_lock);
    if (g_file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_file, text, length, &written, nullptr);
        FlushFileBuffers(g_file);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace

bool initialize(const std::filesystem::path& path) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }

    g_file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    const bool ok = g_file != INVALID_HANDLE_VALUE;
    g_enabled.store(ok, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

void shutdown() noexcept {
    g_enabled.store(false, std::memory_order_release);
    AcquireSRWLockExclusive(&g_lock);
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void write(const char* format, ...) noexcept {
    if (!g_enabled.load(std::memory_order_acquire)) return;
    char message[1792]{};

    va_list arguments;
    va_start(arguments, format);
    const int message_length = vsnprintf_s(
        message,
        sizeof(message),
        _TRUNCATE,
        format,
        arguments);
    va_end(arguments);

    SYSTEMTIME now{};
    GetLocalTime(&now);

    char line[2048]{};
    const int line_length = ::_snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [T%lu] %s\r\n",
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute),
        static_cast<unsigned>(now.wSecond),
        static_cast<unsigned>(now.wMilliseconds),
        static_cast<unsigned long>(GetCurrentThreadId()),
        message_length < 0 ? "<formatting error>" : message);

    if (line_length <= 0) {
        static constexpr char fallback[] = "[ERNativeUI] logging failure\r\n";
        emit(fallback, static_cast<DWORD>(sizeof(fallback) - 1));
        return;
    }

    emit(line, static_cast<DWORD>(line_length));
}

} // namespace erui::host::log
