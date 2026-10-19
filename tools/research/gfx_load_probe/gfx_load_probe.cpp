#include <Windows.h>
#include <intrin.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <utility>

namespace {

constexpr std::uint32_t kEldenRing271Timestamp = 0x6A96B418;
constexpr std::size_t kEldenRing271ImageSize = 0x5E0DA00;

constexpr std::uintptr_t kFileOpener271Rva = 0xD6D280;
constexpr std::uintptr_t kFileOpenerVtableRva = 0x2BBE498;
constexpr std::uintptr_t kMemoryFileVtableRva = 0x2BA7D70;
constexpr std::array<std::uint8_t, 24> kFileOpenerEntry{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41,
    0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xD9,
    0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x48};
constexpr std::array<std::uintptr_t, 12> kMemoryFileVslotRvas{
    0xCE9730, 0xCE9960, 0xCE9B30, 0xCE9B40,
    0xCE9C50, 0xCE9B80, 0xCE9970, 0xCE9B50,
    0xCE9950, 0xCE9C60, 0xCE9B90, 0xCE9C20};

constexpr std::size_t kMaximumPathLength = 511;
constexpr std::size_t kVtableEntries = 12;
constexpr std::size_t kEventCapacity = 64;
constexpr std::size_t kTargetMovieCount = 2;
constexpr std::size_t kCaptureCapacity = 4 * 1024 * 1024;
constexpr std::array<const char*, kTargetMovieCount> kTargetMoviePaths{
    "data0:/menu/win/02_040_optionsetting.gfx",
    "data0:/menu/win/02_042_pc_graphicsetting.gfx"};
constexpr std::array<const wchar_t*, kTargetMovieCount> kCaptureFilenames{
    L"ERNativeUIGfxLoadProbe-capture-02_040_optionsetting.gfx",
    L"ERNativeUIGfxLoadProbe-capture-02_042_pc_graphicsetting.gfx"};
constexpr std::array<const wchar_t*, kTargetMovieCount> kReplacementFilenames{
    L"ERNativeUIGfxLoadProbe-replacement-02_040_optionsetting.gfx",
    L"ERNativeUIGfxLoadProbe-replacement-02_042_pc_graphicsetting.gfx"};

struct ExpectedMovieIdentity {
    std::int32_t source_length{};
    std::uint64_t source_fnv1a64{};
    std::int32_t replacement_length{};
    std::uint32_t replacement_declared_length{};
    std::uint64_t replacement_fnv1a64{};
};

constexpr std::array<ExpectedMovieIdentity, kTargetMovieCount>
    kExpectedMovieIdentities{{
        {44016, 0x681B713108A741E0ull,
         45129, 45120, 0x91FB285CE697E900ull},
        {27056, 0xF636DB56103A9404ull,
         27574, 27564, 0x7852BD4F465EC845ull},
    }};

using OpenFileFn = void*(__fastcall*)(
    void* file_opener,
    const char* path,
    std::int32_t flags,
    std::int32_t mode);

struct GameImage {
    std::uint8_t* base{};
    std::size_t size{};
    std::uint32_t timestamp{};
};

struct LoadEvent {
    DWORD thread_id{};
    void* caller{};
    void* file_opener{};
    std::int32_t flags{};
    std::int32_t mode{};
    void* result{};
    void* vtable{};
    std::array<void*, kVtableEntries> vtable_entries{};
    std::array<char, kMaximumPathLength + 1> path{};
    bool path_truncated{};
};

struct EventSlot {
    std::atomic_bool ready{};
    LoadEvent event{};
};

enum class CaptureState : std::uint32_t {
    empty,
    capturing,
    ready,
    failed,
};

enum class CaptureFailure : std::uint32_t {
    none,
    null_file,
    unreadable_vtable,
    unexpected_file_type,
    unreadable_layout,
    invalid_state,
    invalid_length,
    pointer_overflow,
    unreadable_bytes,
    changed_during_copy,
    invalid_header,
};

struct MemoryFileSnapshot {
    void* vtable{};
    std::uint32_t refcount{};
    const std::uint8_t* bytes{};
    std::int32_t length{};
    std::int32_t cursor{};
    std::uint8_t valid{};
};

struct CapturedMovie {
    std::atomic<CaptureState> state{CaptureState::empty};
    CaptureFailure failure{CaptureFailure::none};
    void* file{};
    void* vtable{};
    std::uint32_t refcount{};
    const std::uint8_t* source_bytes{};
    std::int32_t length{};
    std::int32_t cursor{};
    std::uint8_t valid{};
    std::array<char, kMaximumPathLength + 1> path{};
    std::array<std::uint8_t, kCaptureCapacity> bytes{};
};

struct ReplacementMovie {
    bool ready{};
    std::int32_t length{};
    std::uint32_t declared_length{};
    std::uint64_t fnv1a64{};
    std::array<std::uint8_t, kCaptureCapacity> bytes{};
};

enum class SubstitutionState : std::uint32_t {
    empty,
    applying,
    applied,
    failed,
};

enum class SubstitutionFailure : std::uint32_t {
    none,
    replacement_unavailable,
    source_capture_unavailable,
    source_identity_mismatch,
    unreadable_layout,
    unexpected_file_type,
    invalid_state,
    destination_not_writable,
    write_fault,
    verification_failed,
    restoration_failed,
};

struct SubstitutionResult {
    std::atomic<SubstitutionState> state{SubstitutionState::empty};
    SubstitutionFailure failure{SubstitutionFailure::none};
    void* file{};
    const std::uint8_t* original_bytes{};
    std::int32_t original_length{};
    const std::uint8_t* replacement_bytes{};
    std::int32_t replacement_length{};
};

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
std::array<wchar_t, 32768> g_output_directory{};
GameImage g_game{};
std::uintptr_t g_file_opener_rva{};
const char* g_profile_name{};

SafetyHookInline g_open_file_hook{};
std::atomic<OpenFileFn> g_original_open_file{};

std::array<EventSlot, kEventCapacity> g_events{};
std::atomic<std::uint32_t> g_event_count{};
std::atomic<std::uint32_t> g_dropped_events{};
std::array<CapturedMovie, kTargetMovieCount> g_captures{};
std::array<ReplacementMovie, kTargetMovieCount> g_replacements{};
std::array<SubstitutionResult, kTargetMovieCount> g_substitutions{};
std::atomic_bool g_substitution_enabled{};

std::uint64_t fnv1a64(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

void substitute_memory_file(std::size_t target_index, void* file) noexcept;

bool guarded_copy(void* destination, const void* source, std::size_t size) noexcept {
    if (!destination || !source || size == 0) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(destination, source, size);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool guarded_copy_path(
    const char* source,
    std::array<char, kMaximumPathLength + 1>& destination,
    bool& truncated) noexcept {
    destination.fill('\0');
    truncated = false;
    if (!source) return false;

#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0; index < kMaximumPathLength; ++index) {
            const char value = source[index];
            destination[index] = value;
            if (value == '\0') return true;
        }
        destination[kMaximumPathLength] = '\0';
        truncated = source[kMaximumPathLength] != '\0';
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        destination.fill('\0');
        truncated = false;
        return false;
    }
#endif
}

char ascii_lower(char value) noexcept {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value + ('a' - 'A'));
    }
    return value;
}

bool virtual_path_equals_ignore_case(
    const char* left,
    const char* right) noexcept {
    if (!left || !right) return false;
    std::size_t index{};
    while (left[index] != '\0' && right[index] != '\0') {
        const char left_value = left[index] == '\\' ? '/' : left[index];
        const char right_value = right[index] == '\\' ? '/' : right[index];
        if (ascii_lower(left_value) != ascii_lower(right_value)) return false;
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

std::size_t target_movie_index(const char* path) noexcept {
    if (!path) return kTargetMovieCount;
    for (std::size_t index = 0; index < kTargetMoviePaths.size(); ++index) {
        if (virtual_path_equals_ignore_case(path, kTargetMoviePaths[index])) {
            return index;
        }
    }
    return kTargetMovieCount;
}

void log_line(const char* format, ...) noexcept {
    if (g_log_file == INVALID_HANDLE_VALUE || !format) return;

    std::array<char, 4096> message{};
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf_s(
        message.data(), message.size(), _TRUNCATE, format, arguments);
    va_end(arguments);
    if (result < 0 && message[0] == '\0') return;

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::array<char, 4608> line{};
    const int line_length = snprintf(
        line.data(),
        line.size(),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [GfxLoadProbe] %s\r\n",
        static_cast<unsigned>(time.wYear),
        static_cast<unsigned>(time.wMonth),
        static_cast<unsigned>(time.wDay),
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        static_cast<unsigned long>(GetCurrentThreadId()),
        message.data());
    if (line_length <= 0) return;

    const DWORD bytes_to_write = static_cast<DWORD>(
        (std::min)(static_cast<std::size_t>(line_length), line.size() - 1));
    DWORD bytes_written{};
    (void)WriteFile(
        g_log_file, line.data(), bytes_to_write, &bytes_written, nullptr);
    (void)FlushFileBuffers(g_log_file);
}

bool initialize_log() noexcept {
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(
        g_module, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || static_cast<std::size_t>(length) >= module_path.size()) {
        return false;
    }

    wchar_t* const last_separator = wcsrchr(module_path.data(), L'\\');
    if (!last_separator) return false;
    *(last_separator + 1) = L'\0';
    if (wcscpy_s(
            g_output_directory.data(),
            g_output_directory.size(),
            module_path.data()) != 0) {
        return false;
    }
    constexpr wchar_t kLogName[] = L"ERNativeUIGfxLoadProbe.log";
    const std::size_t prefix_length = wcslen(module_path.data());
    if (prefix_length + _countof(kLogName) > module_path.size()) return false;
    wcscat_s(module_path.data(), module_path.size(), kLogName);

    g_log_file = CreateFileW(
        module_path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return g_log_file != INVALID_HANDLE_VALUE;
}

bool inspect_game_image(GameImage& image) noexcept {
    image = {};
    auto* const base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!base) return false;

#if defined(_MSC_VER)
    __try {
#endif
        const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
            dos->e_lfanew > 0x100000) {
            return false;
        }
        const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return false;
        }
        image.base = base;
        image.size = nt->OptionalHeader.SizeOfImage;
        image.timestamp = nt->FileHeader.TimeDateStamp;
        return image.size >= 0x1000;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        image = {};
        return false;
    }
#endif
}

bool select_profile() noexcept {
    if (g_game.timestamp == kEldenRing271Timestamp &&
        g_game.size == kEldenRing271ImageSize) {
        g_file_opener_rva = kFileOpener271Rva;
        g_profile_name = "Elden Ring 2.7.1.0";
        return true;
    }
    return false;
}

bool entry_matches() noexcept {
    if (!g_game.base || g_file_opener_rva >= g_game.size ||
        kFileOpenerEntry.size() > g_game.size - g_file_opener_rva) {
        return false;
    }
    std::array<std::uint8_t, kFileOpenerEntry.size()> actual{};
    if (!guarded_copy(
               actual.data(),
               g_game.base + g_file_opener_rva,
               actual.size()) ||
        actual != kFileOpenerEntry) {
        return false;
    }

    void* virtual_open_file{};
    if (kFileOpenerVtableRva + 2 * sizeof(void*) > g_game.size ||
        !guarded_copy(
            &virtual_open_file,
            g_game.base + kFileOpenerVtableRva + sizeof(void*),
            sizeof(virtual_open_file))) {
        return false;
    }
    if (virtual_open_file != g_game.base + g_file_opener_rva ||
        kMemoryFileVtableRva +
                kMemoryFileVslotRvas.size() * sizeof(void*) >
            g_game.size) {
        return false;
    }

    std::array<void*, kMemoryFileVslotRvas.size()> memory_file_vslots{};
    if (!guarded_copy(
            memory_file_vslots.data(),
            g_game.base + kMemoryFileVtableRva,
            sizeof(memory_file_vslots))) {
        return false;
    }
    for (std::size_t index = 0; index < memory_file_vslots.size(); ++index) {
        if (memory_file_vslots[index] !=
            g_game.base + kMemoryFileVslotRvas[index]) {
            return false;
        }
    }
    return true;
}

std::uintptr_t image_rva(const void* address) noexcept {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(g_game.base);
    if (!address || value < base || value - base >= g_game.size) return 0;
    return value - base;
}

void publish_event(
    void* caller,
    void* file_opener,
    std::int32_t flags,
    std::int32_t mode,
    void* result,
    const std::array<char, kMaximumPathLength + 1>& path,
    bool path_truncated) noexcept {
    const std::uint32_t index =
        g_event_count.fetch_add(1, std::memory_order_relaxed);
    if (index >= kEventCapacity) {
        g_dropped_events.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    LoadEvent event{};
    event.thread_id = GetCurrentThreadId();
    event.caller = caller;
    event.file_opener = file_opener;
    event.flags = flags;
    event.mode = mode;
    event.result = result;
    event.path = path;
    event.path_truncated = path_truncated;
    if (result) {
        (void)guarded_copy(&event.vtable, result, sizeof(event.vtable));
        if (event.vtable) {
            (void)guarded_copy(
                event.vtable_entries.data(),
                event.vtable,
                sizeof(event.vtable_entries));
        }
    }

    g_events[index].event = event;
    g_events[index].ready.store(true, std::memory_order_release);
}

void finish_capture_failure(
    CapturedMovie& capture,
    CaptureFailure failure) noexcept {
    capture.failure = failure;
    capture.state.store(CaptureState::failed, std::memory_order_release);
}

bool read_memory_file_snapshot(
    const void* file,
    MemoryFileSnapshot& snapshot) noexcept {
    snapshot = {};
    if (!file) return false;
    const auto* const object = static_cast<const std::byte*>(file);
    return guarded_copy(&snapshot.vtable, object, sizeof(snapshot.vtable)) &&
        guarded_copy(
            &snapshot.refcount, object + 0x08, sizeof(snapshot.refcount)) &&
        guarded_copy(&snapshot.bytes, object + 0x18, sizeof(snapshot.bytes)) &&
        guarded_copy(&snapshot.length, object + 0x20, sizeof(snapshot.length)) &&
        guarded_copy(&snapshot.cursor, object + 0x24, sizeof(snapshot.cursor)) &&
        guarded_copy(&snapshot.valid, object + 0x28, sizeof(snapshot.valid));
}

bool same_memory_file_snapshot(
    const MemoryFileSnapshot& left,
    const MemoryFileSnapshot& right) noexcept {
    return left.vtable == right.vtable &&
        left.refcount == right.refcount &&
        left.bytes == right.bytes &&
        left.length == right.length &&
        left.cursor == right.cursor &&
        left.valid == right.valid;
}

void capture_memory_file(
    std::size_t target_index,
    void* file,
    const std::array<char, kMaximumPathLength + 1>& path) noexcept {
    if (target_index >= g_captures.size()) return;

    CapturedMovie& capture = g_captures[target_index];
    CaptureState expected = CaptureState::empty;
    if (!capture.state.compare_exchange_strong(
            expected,
            CaptureState::capturing,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return;
    }

    capture.file = file;
    capture.path = path;
    if (!file) {
        finish_capture_failure(capture, CaptureFailure::null_file);
        return;
    }

    void* dynamic_vtable{};
    if (!guarded_copy(&dynamic_vtable, file, sizeof(dynamic_vtable))) {
        finish_capture_failure(capture, CaptureFailure::unreadable_vtable);
        return;
    }
    capture.vtable = dynamic_vtable;
    if (dynamic_vtable != g_game.base + kMemoryFileVtableRva) {
        finish_capture_failure(capture, CaptureFailure::unexpected_file_type);
        return;
    }

    MemoryFileSnapshot before{};
    if (!read_memory_file_snapshot(file, before)) {
        finish_capture_failure(capture, CaptureFailure::unreadable_layout);
        return;
    }
    capture.vtable = before.vtable;
    capture.refcount = before.refcount;
    capture.source_bytes = before.bytes;
    capture.length = before.length;
    capture.cursor = before.cursor;
    capture.valid = before.valid;
    if (before.vtable != dynamic_vtable) {
        finish_capture_failure(capture, CaptureFailure::unexpected_file_type);
        return;
    }
    if (before.refcount == 0 || !before.bytes || before.valid != 1 ||
        before.cursor != 0) {
        finish_capture_failure(capture, CaptureFailure::invalid_state);
        return;
    }
    if (before.length < 12 ||
        static_cast<std::size_t>(before.length) > capture.bytes.size()) {
        finish_capture_failure(capture, CaptureFailure::invalid_length);
        return;
    }
    const auto source_address =
        reinterpret_cast<std::uintptr_t>(before.bytes);
    const auto capture_length = static_cast<std::size_t>(before.length);
    if (source_address > UINTPTR_MAX - capture_length) {
        finish_capture_failure(capture, CaptureFailure::pointer_overflow);
        return;
    }
    if (!guarded_copy(
            capture.bytes.data(), before.bytes, capture_length)) {
        finish_capture_failure(capture, CaptureFailure::unreadable_bytes);
        return;
    }

    MemoryFileSnapshot after{};
    if (!read_memory_file_snapshot(file, after) ||
        !same_memory_file_snapshot(before, after)) {
        finish_capture_failure(capture, CaptureFailure::changed_during_copy);
        return;
    }

    const bool gfx_magic = capture.bytes[0] == 'G' &&
        capture.bytes[1] == 'F' && capture.bytes[2] == 'X';
    const std::uint32_t declared_length =
        static_cast<std::uint32_t>(capture.bytes[4]) |
        (static_cast<std::uint32_t>(capture.bytes[5]) << 8) |
        (static_cast<std::uint32_t>(capture.bytes[6]) << 16) |
        (static_cast<std::uint32_t>(capture.bytes[7]) << 24);
    if (!gfx_magic || declared_length < 12 ||
        declared_length > capture_length) {
        finish_capture_failure(capture, CaptureFailure::invalid_header);
        return;
    }

    capture.state.store(CaptureState::ready, std::memory_order_release);
}

struct MemoryFileView {
    const std::uint8_t* bytes{};
    std::int32_t length{};
    std::int32_t cursor{};
};

static_assert(sizeof(MemoryFileView) == 0x10);

bool writable_region(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xFFu;
    const bool writable = protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
    if (!writable) return false;

    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto region_start =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    if (start < region_start || start > UINTPTR_MAX - size ||
        region_start > UINTPTR_MAX - information.RegionSize) {
        return false;
    }
    return start + size <= region_start + information.RegionSize;
}

void finish_substitution_failure(
    SubstitutionResult& substitution,
    SubstitutionFailure failure) noexcept {
    substitution.failure = failure;
    substitution.state.store(SubstitutionState::failed, std::memory_order_release);
}

bool restore_memory_file_view(
    void* file,
    const MemoryFileSnapshot& expected,
    const MemoryFileView& original_view) noexcept {
    auto* const destination = static_cast<std::byte*>(file) + 0x18;
    if (!guarded_copy(destination, &original_view, sizeof(original_view))) {
        return false;
    }
    MemoryFileSnapshot restored{};
    return read_memory_file_snapshot(file, restored) &&
        same_memory_file_snapshot(expected, restored);
}

void substitute_memory_file(std::size_t target_index, void* file) noexcept {
    if (!g_substitution_enabled.load(std::memory_order_acquire) ||
        target_index >= g_substitutions.size()) {
        return;
    }

    SubstitutionResult& substitution = g_substitutions[target_index];
    SubstitutionState expected_state = SubstitutionState::empty;
    if (!substitution.state.compare_exchange_strong(
            expected_state,
            SubstitutionState::applying,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return;
    }
    substitution.file = file;

    const ReplacementMovie& replacement = g_replacements[target_index];
    if (!replacement.ready) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::replacement_unavailable);
        return;
    }

    const CapturedMovie& capture = g_captures[target_index];
    if (capture.state.load(std::memory_order_acquire) != CaptureState::ready ||
        capture.file != file) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::source_capture_unavailable);
        return;
    }
    const ExpectedMovieIdentity& expected =
        kExpectedMovieIdentities[target_index];
    if (capture.length != expected.source_length ||
        fnv1a64(
            capture.bytes.data(),
            static_cast<std::size_t>(capture.length)) !=
            expected.source_fnv1a64) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::source_identity_mismatch);
        return;
    }

    MemoryFileSnapshot before{};
    if (!read_memory_file_snapshot(file, before)) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::unreadable_layout);
        return;
    }
    substitution.original_bytes = before.bytes;
    substitution.original_length = before.length;
    substitution.replacement_bytes = replacement.bytes.data();
    substitution.replacement_length = replacement.length;
    if (before.vtable != g_game.base + kMemoryFileVtableRva) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::unexpected_file_type);
        return;
    }
    if (before.refcount == 0 || before.valid != 1 || before.cursor != 0 ||
        before.bytes != capture.source_bytes ||
        before.length != capture.length) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::invalid_state);
        return;
    }

    auto* const destination = static_cast<std::byte*>(file) + 0x18;
    if (!writable_region(destination, sizeof(MemoryFileView))) {
        finish_substitution_failure(
            substitution, SubstitutionFailure::destination_not_writable);
        return;
    }

    const MemoryFileView original_view{
        before.bytes, before.length, before.cursor};
    const MemoryFileView replacement_view{
        replacement.bytes.data(), replacement.length, 0};
    if (!guarded_copy(destination, &replacement_view, sizeof(replacement_view))) {
        const bool restored =
            restore_memory_file_view(file, before, original_view);
        finish_substitution_failure(
            substitution,
            restored ? SubstitutionFailure::write_fault
                     : SubstitutionFailure::restoration_failed);
        return;
    }

    MemoryFileSnapshot after{};
    const bool verified = read_memory_file_snapshot(file, after) &&
        after.vtable == before.vtable &&
        after.refcount == before.refcount &&
        after.bytes == replacement.bytes.data() &&
        after.length == replacement.length &&
        after.cursor == 0 && after.valid == before.valid;
    if (!verified) {
        const bool restored =
            restore_memory_file_view(file, before, original_view);
        finish_substitution_failure(
            substitution,
            restored ? SubstitutionFailure::verification_failed
                     : SubstitutionFailure::restoration_failed);
        return;
    }

    substitution.state.store(
        SubstitutionState::applied, std::memory_order_release);
}

void* __fastcall open_file_detour(
    void* file_opener,
    const char* path,
    std::int32_t flags,
    std::int32_t mode) noexcept {
    void* const caller = _ReturnAddress();
    std::array<char, kMaximumPathLength + 1> copied_path{};
    bool path_truncated{};
    const bool path_valid = guarded_copy_path(path, copied_path, path_truncated);

    OpenFileFn const original =
        g_original_open_file.load(std::memory_order_acquire);
    void* const result = original
        ? original(file_opener, path, flags, mode)
        : nullptr;

    const std::size_t target_index = path_valid && !path_truncated
        ? target_movie_index(copied_path.data())
        : kTargetMovieCount;
    if (target_index < kTargetMovieCount) {
        publish_event(
            caller,
            file_opener,
            flags,
            mode,
            result,
            copied_path,
            path_truncated);
        capture_memory_file(target_index, result, copied_path);
        substitute_memory_file(target_index, result);
    }
    return result;
}

void drain_events() noexcept {
    static std::array<bool, kEventCapacity> consumed{};
    for (std::size_t index = 0; index < g_events.size(); ++index) {
        if (consumed[index] ||
            !g_events[index].ready.load(std::memory_order_acquire)) {
            continue;
        }

        const LoadEvent& event = g_events[index].event;
        log_line(
            "targetLoad[%zu] sourceThread=%lu caller=%p(rva=0x%llX) opener=%p flags=%d mode=%d result=%p vtable=%p(rva=0x%llX) path='%s'%s",
            index + 1,
            static_cast<unsigned long>(event.thread_id),
            event.caller,
            static_cast<unsigned long long>(image_rva(event.caller)),
            event.file_opener,
            event.flags,
            event.mode,
            event.result,
            event.vtable,
            static_cast<unsigned long long>(image_rva(event.vtable)),
            event.path.data(),
            event.path_truncated ? " [truncated]" : "");
        log_line(
            "targetLoad[%zu] vslots rva=[0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX]",
            index + 1,
            static_cast<unsigned long long>(image_rva(event.vtable_entries[0])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[1])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[2])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[3])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[4])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[5])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[6])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[7])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[8])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[9])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[10])),
            static_cast<unsigned long long>(image_rva(event.vtable_entries[11])));
        consumed[index] = true;
    }
}

const char* capture_failure_name(CaptureFailure failure) noexcept {
    switch (failure) {
    case CaptureFailure::none: return "none";
    case CaptureFailure::null_file: return "null_file";
    case CaptureFailure::unreadable_vtable: return "unreadable_vtable";
    case CaptureFailure::unexpected_file_type: return "unexpected_file_type";
    case CaptureFailure::unreadable_layout: return "unreadable_layout";
    case CaptureFailure::invalid_state: return "invalid_state";
    case CaptureFailure::invalid_length: return "invalid_length";
    case CaptureFailure::pointer_overflow: return "pointer_overflow";
    case CaptureFailure::unreadable_bytes: return "unreadable_bytes";
    case CaptureFailure::changed_during_copy: return "changed_during_copy";
    case CaptureFailure::invalid_header: return "invalid_header";
    }
    return "unknown";
}

std::uint64_t fnv1a64(
    const std::uint8_t* bytes,
    std::size_t size) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

bool inspect_gfx_header(
    const std::uint8_t* bytes,
    std::size_t length,
    std::uint32_t& declared_length) noexcept {
    declared_length = 0;
    if (!bytes || length < 12 || bytes[0] != 'G' ||
        bytes[1] != 'F' || bytes[2] != 'X') {
        return false;
    }
    declared_length = static_cast<std::uint32_t>(bytes[4]) |
        (static_cast<std::uint32_t>(bytes[5]) << 8) |
        (static_cast<std::uint32_t>(bytes[6]) << 16) |
        (static_cast<std::uint32_t>(bytes[7]) << 24);
    return declared_length >= 12 && declared_length <= length;
}

bool build_named_path(
    const wchar_t* filename,
    std::array<wchar_t, 32768>& output_path) noexcept {
    if (!filename || filename[0] == L'\0') return false;
    output_path = g_output_directory;
    const std::size_t prefix_length = wcslen(output_path.data());
    return prefix_length + wcslen(filename) + 1 <= output_path.size() &&
        wcscat_s(output_path.data(), output_path.size(), filename) == 0;
}

bool build_capture_path(
    std::size_t target_index,
    std::array<wchar_t, 32768>& output_path) noexcept {
    if (target_index >= kCaptureFilenames.size()) return false;
    return build_named_path(kCaptureFilenames[target_index], output_path);
}

bool load_replacement_movie(std::size_t target_index) noexcept {
    if (target_index >= g_replacements.size()) return false;
    ReplacementMovie& replacement = g_replacements[target_index];
    // ReplacementMovie owns a 4 MiB static backing array. Reset only its
    // metadata here: aggregate assignment may materialize a full-size
    // temporary on this worker thread's default stack before copying it.
    replacement.ready = false;
    replacement.length = 0;
    replacement.declared_length = 0;
    replacement.fnv1a64 = 0;

    std::array<wchar_t, 32768> path{};
    if (!build_named_path(kReplacementFilenames[target_index], path)) {
        log_line(
            "replacement[%zu] FAILED: could not construct input path",
            target_index + 1);
        return false;
    }

    HANDLE const file = CreateFileW(
        path.data(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        log_line(
            "replacement[%zu] FAILED: could not open '%ls' (GetLastError=%lu)",
            target_index + 1,
            path.data(),
            static_cast<unsigned long>(GetLastError()));
        return false;
    }

    LARGE_INTEGER file_size{};
    bool success = GetFileSizeEx(file, &file_size) != FALSE &&
        file_size.QuadPart ==
            kExpectedMovieIdentities[target_index].replacement_length &&
        file_size.QuadPart <= static_cast<LONGLONG>(replacement.bytes.size());
    DWORD bytes_read{};
    if (success) {
        success = ReadFile(
                file,
                replacement.bytes.data(),
                static_cast<DWORD>(file_size.QuadPart),
                &bytes_read,
                nullptr) != FALSE &&
            bytes_read == static_cast<DWORD>(file_size.QuadPart);
    }
    const DWORD io_error = success ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!success) {
        log_line(
            "replacement[%zu] FAILED: size/read validation failed expectedLength=%d observedLength=%lld bytesRead=%lu error=%lu",
            target_index + 1,
            kExpectedMovieIdentities[target_index].replacement_length,
            static_cast<long long>(file_size.QuadPart),
            static_cast<unsigned long>(bytes_read),
            static_cast<unsigned long>(io_error));
        return false;
    }

    replacement.length = static_cast<std::int32_t>(file_size.QuadPart);
    if (!inspect_gfx_header(
            replacement.bytes.data(),
            static_cast<std::size_t>(replacement.length),
            replacement.declared_length)) {
        log_line(
            "replacement[%zu] FAILED: invalid GFX header",
            target_index + 1);
        return false;
    }
    replacement.fnv1a64 = fnv1a64(
        replacement.bytes.data(),
        static_cast<std::size_t>(replacement.length));
    const ExpectedMovieIdentity& expected =
        kExpectedMovieIdentities[target_index];
    if (replacement.declared_length != expected.replacement_declared_length ||
        replacement.fnv1a64 != expected.replacement_fnv1a64) {
        log_line(
            "replacement[%zu] FAILED: identity mismatch length=%d declared=%u fnv1a64=%016llX",
            target_index + 1,
            replacement.length,
            static_cast<unsigned>(replacement.declared_length),
            static_cast<unsigned long long>(replacement.fnv1a64));
        return false;
    }
    replacement.ready = true;
    log_line(
        "replacement[%zu] ready length=%d declared=%u fnv1a64=%016llX input='%ls'",
        target_index + 1,
        replacement.length,
        static_cast<unsigned>(replacement.declared_length),
        static_cast<unsigned long long>(replacement.fnv1a64),
        path.data());
    return true;
}

bool load_replacement_movies() noexcept {
    bool ready = true;
    for (std::size_t index = 0; index < g_replacements.size(); ++index) {
        ready = load_replacement_movie(index) && ready;
    }
    g_substitution_enabled.store(ready, std::memory_order_release);
    log_line(
        "Live substitution mode: %s",
        ready ? "enabled for both exact stock movie identities"
              : "disabled; observation remains active");
    return ready;
}

bool write_capture_file(
    std::size_t target_index,
    const CapturedMovie& capture,
    std::array<wchar_t, 32768>& output_path,
    DWORD& error) noexcept {
    error = ERROR_SUCCESS;
    if (!build_capture_path(target_index, output_path)) {
        error = ERROR_INSUFFICIENT_BUFFER;
        return false;
    }

    HANDLE const file = CreateFileW(
        output_path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }

    DWORD bytes_written{};
    const DWORD requested = static_cast<DWORD>(capture.length);
    const bool success = WriteFile(
            file,
            capture.bytes.data(),
            requested,
            &bytes_written,
            nullptr) != FALSE &&
        bytes_written == requested;
    if (!success) {
        error = bytes_written != requested ? ERROR_WRITE_FAULT : GetLastError();
    }
    if (success && !FlushFileBuffers(file)) error = GetLastError();
    if (!CloseHandle(file) && error == ERROR_SUCCESS) error = GetLastError();
    if (error != ERROR_SUCCESS) return false;
    return success;
}

void drain_captures() noexcept {
    static std::array<bool, kTargetMovieCount> consumed{};
    for (std::size_t index = 0; index < g_captures.size(); ++index) {
        if (consumed[index]) continue;

        CapturedMovie& capture = g_captures[index];
        const CaptureState state = capture.state.load(std::memory_order_acquire);
        if (state == CaptureState::empty || state == CaptureState::capturing) {
            continue;
        }
        consumed[index] = true;

        if (state == CaptureState::failed) {
            log_line(
                "capture[%zu] FAILED reason=%s file=%p vtable=%p(rva=0x%llX) bytes=%p length=%d cursor=%d valid=%u path='%s'",
                index + 1,
                capture_failure_name(capture.failure),
                capture.file,
                capture.vtable,
                static_cast<unsigned long long>(image_rva(capture.vtable)),
                static_cast<const void*>(capture.source_bytes),
                capture.length,
                capture.cursor,
                static_cast<unsigned>(capture.valid),
                capture.path.data());
            continue;
        }

        const auto length = static_cast<std::size_t>(capture.length);
        const std::uint32_t declared_length =
            static_cast<std::uint32_t>(capture.bytes[4]) |
            (static_cast<std::uint32_t>(capture.bytes[5]) << 8) |
            (static_cast<std::uint32_t>(capture.bytes[6]) << 16) |
            (static_cast<std::uint32_t>(capture.bytes[7]) << 24);
        const std::uint64_t hash = fnv1a64(capture.bytes.data(), length);
        std::array<wchar_t, 32768> output_path{};
        DWORD write_error{};
        const bool written =
            write_capture_file(index, capture, output_path, write_error);
        log_line(
            "capture[%zu] file=%p MemoryFileVtableRva=0x%llX refcount=%u bytes=%p length=%d cursor=%d valid=%u header=%02X%02X%02X version=%u declaredLength=%u fnv1a64=%016llX outputWritten=%u",
            index + 1,
            capture.file,
            static_cast<unsigned long long>(image_rva(capture.vtable)),
            static_cast<unsigned>(capture.refcount),
            static_cast<const void*>(capture.source_bytes),
            capture.length,
            capture.cursor,
            static_cast<unsigned>(capture.valid),
            static_cast<unsigned>(capture.bytes[0]),
            static_cast<unsigned>(capture.bytes[1]),
            static_cast<unsigned>(capture.bytes[2]),
            static_cast<unsigned>(capture.bytes[3]),
            static_cast<unsigned>(declared_length),
            static_cast<unsigned long long>(hash),
            written ? 1u : 0u);
        log_line(
            "capture[%zu] output='%ls'",
            index + 1,
            output_path.data());
        if (!written) {
            log_line(
                "capture[%zu] ERROR: could not write the worker-owned byte copy (GetLastError=%lu)",
                index + 1,
                static_cast<unsigned long>(write_error));
        }
    }
}

const char* substitution_failure_name(
    SubstitutionFailure failure) noexcept {
    switch (failure) {
    case SubstitutionFailure::none: return "none";
    case SubstitutionFailure::replacement_unavailable:
        return "replacement_unavailable";
    case SubstitutionFailure::source_capture_unavailable:
        return "source_capture_unavailable";
    case SubstitutionFailure::source_identity_mismatch:
        return "source_identity_mismatch";
    case SubstitutionFailure::unreadable_layout: return "unreadable_layout";
    case SubstitutionFailure::unexpected_file_type:
        return "unexpected_file_type";
    case SubstitutionFailure::invalid_state: return "invalid_state";
    case SubstitutionFailure::destination_not_writable:
        return "destination_not_writable";
    case SubstitutionFailure::write_fault: return "write_fault";
    case SubstitutionFailure::verification_failed:
        return "verification_failed";
    case SubstitutionFailure::restoration_failed:
        return "restoration_failed";
    }
    return "unknown";
}

void drain_substitutions() noexcept {
    static std::array<bool, kTargetMovieCount> consumed{};
    for (std::size_t index = 0; index < g_substitutions.size(); ++index) {
        if (consumed[index]) continue;
        const SubstitutionResult& substitution = g_substitutions[index];
        const SubstitutionState state =
            substitution.state.load(std::memory_order_acquire);
        if (state == SubstitutionState::empty ||
            state == SubstitutionState::applying) {
            continue;
        }
        consumed[index] = true;
        if (state == SubstitutionState::applied) {
            log_line(
                "substitution[%zu] APPLIED file=%p originalBytes=%p originalLength=%d replacementBytes=%p replacementLength=%d",
                index + 1,
                substitution.file,
                static_cast<const void*>(substitution.original_bytes),
                substitution.original_length,
                static_cast<const void*>(substitution.replacement_bytes),
                substitution.replacement_length);
        } else {
            log_line(
                "substitution[%zu] FAILED reason=%s file=%p originalBytes=%p originalLength=%d replacementBytes=%p replacementLength=%d",
                index + 1,
                substitution_failure_name(substitution.failure),
                substitution.file,
                static_cast<const void*>(substitution.original_bytes),
                substitution.original_length,
                static_cast<const void*>(substitution.replacement_bytes),
                substitution.replacement_length);
        }
    }
}

void remove_stale_capture_files() noexcept {
    for (std::size_t index = 0; index < kCaptureFilenames.size(); ++index) {
        std::array<wchar_t, 32768> path{};
        if (!build_capture_path(index, path)) {
            log_line(
                "WARNING: could not construct capture[%zu] output path",
                index + 1);
            continue;
        }
        if (DeleteFileW(path.data())) {
            log_line("Removed stale capture[%zu] output", index + 1);
            continue;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            log_line(
                "WARNING: could not remove stale capture[%zu] output (GetLastError=%lu)",
                index + 1,
                static_cast<unsigned long>(error));
        }
    }
}

bool install_hook() noexcept {
    if (!entry_matches()) {
        log_line(
            "ERROR: CSScaleformFileOpener::OpenFile entry differs from the validated %s executable at RVA 0x%llX; no hook installed",
            g_profile_name,
            static_cast<unsigned long long>(g_file_opener_rva));
        return false;
    }

    auto result = SafetyHookInline::create(
        g_game.base + g_file_opener_rva,
        reinterpret_cast<void*>(&open_file_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "ERROR: could not install CSScaleformFileOpener::OpenFile hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(g_file_opener_rva),
            static_cast<unsigned>(result.error().type));
        return false;
    }
    g_open_file_hook = std::move(*result);
    g_original_open_file.store(
        g_open_file_hook.original<OpenFileFn>(),
        std::memory_order_release);
    auto enabled = g_open_file_hook.enable();
    if (!enabled) {
        log_line(
            "ERROR: could not enable CSScaleformFileOpener::OpenFile hook (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_original_open_file.store(nullptr, std::memory_order_release);
        g_open_file_hook.reset();
        return false;
    }
    log_line(
        "GFX load/capture/substitution hook installed: %s CSScaleformFileOpener::OpenFile RVA=0x%llX",
        g_profile_name,
        static_cast<unsigned long long>(g_file_opener_rva));
    return true;
}

DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    log_line("============================================================");
    log_line("Exact-sidecar Scaleform GFX substitution probe starting");

    HMODULE pinned{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(g_module),
            &pinned)) {
        log_line("ERROR: probe DLL could not be pinned; no hook installed");
        return 1;
    }

    if (!inspect_game_image(g_game)) {
        log_line("ERROR: could not inspect the main executable image");
        return 1;
    }
    log_line(
        "Game image base=%p timestamp=0x%08X size=0x%zX",
        static_cast<void*>(g_game.base),
        static_cast<unsigned>(g_game.timestamp),
        g_game.size);
    if (!select_profile()) {
        log_line(
            "ERROR: unsupported Elden Ring executable profile; this probe is validated only for 2.7.1.0; no hook installed");
        return 1;
    }
    (void)load_replacement_movies();
    if (!install_hook()) return 1;
    remove_stale_capture_files();

    for (;;) {
        drain_events();
        drain_captures();
        drain_substitutions();
        Sleep(50);
    }
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, &run_probe, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
