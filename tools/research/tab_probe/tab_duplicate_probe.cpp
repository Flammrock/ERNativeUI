#include <Windows.h>
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

constexpr std::uintptr_t kTopConstructorRva = 0x9672C0;
constexpr std::uintptr_t kCategoryAppendRva = 0x968CF0;
constexpr std::uintptr_t kTabDispatcherRva = 0x9680D0;
constexpr std::uintptr_t kTopVtableRva = 0x2B16B48;
constexpr std::uintptr_t kCategoryRecordVtableRva = 0x2AD6F20;

constexpr std::size_t kCategoryStorageOffset = 0x1208;
constexpr std::size_t kCategoryCountOffset = 0x1760;
constexpr std::size_t kCategoryRecordSize = 0x88;
constexpr std::size_t kCategoryIdOffset = 0x40;
constexpr std::size_t kNativeCategoryCapacity = 10;

constexpr std::uint32_t kEldenRing270Timestamp = 0x69E9C9B9;
constexpr std::size_t kEldenRing270ImageSize = 0x5E09600;
constexpr std::uint32_t kEldenRing271Timestamp = 0x6A96B418;
constexpr std::size_t kEldenRing271ImageSize = 0x5E0DA00;
constexpr std::uint64_t kValidatedPrivateHostFileSize = 984576;

using TopConstructorFn = void*(__fastcall*)(
    void* top,
    std::uintptr_t movie,
    std::uint8_t argument3,
    std::uint8_t title_menu);

struct GameImage {
    std::uint8_t* base{};
    std::size_t size{};
    std::uint32_t timestamp{};
};

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
SRWLOCK g_log_lock = SRWLOCK_INIT;
GameImage g_game{};
SafetyHookInline g_constructor_hook{};
std::atomic<TopConstructorFn> g_original_constructor{};
std::atomic_uint32_t g_constructor_calls{};

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

template <typename T>
bool guarded_read(const void* source, T& value) noexcept {
    return guarded_copy(&value, source, sizeof(value));
}

bool guarded_write_i32(void* destination, std::int32_t value) noexcept {
    if (!destination) return false;
#if defined(_MSC_VER)
    __try {
#endif
        *static_cast<volatile std::int32_t*>(destination) = value;
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
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
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [TabDuplicateProbe] %s\r\n",
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

    const DWORD bytes_to_write = static_cast<DWORD>((std::min)(
        static_cast<std::size_t>(line_length), line.size() - 1));
    AcquireSRWLockExclusive(&g_log_lock);
    DWORD bytes_written{};
    (void)WriteFile(
        g_log_file, line.data(), bytes_to_write, &bytes_written, nullptr);
    ReleaseSRWLockExclusive(&g_log_lock);
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
    constexpr wchar_t kLogName[] = L"ERNativeUITabDuplicateProbe.log";
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

bool supported_game_image(const GameImage& image) noexcept {
    return (image.timestamp == kEldenRing270Timestamp &&
               image.size == kEldenRing270ImageSize) ||
        (image.timestamp == kEldenRing271Timestamp &&
            image.size == kEldenRing271ImageSize);
}

bool image_contains(std::uintptr_t rva, std::size_t size) noexcept {
    return g_game.base && rva < g_game.size && size <= g_game.size - rva;
}

template <std::size_t Size>
bool entry_matches(
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    if (!image_contains(rva, Size)) return false;
    std::array<std::uint8_t, Size> actual{};
    return guarded_copy(actual.data(), g_game.base + rva, Size) &&
        actual == expected;
}

bool validate_private_host() noexcept {
    std::array<wchar_t, 32768> host_path{};
    HMODULE const host = GetModuleHandleW(L"ERNativeUI.dll");
    const DWORD length = host
        ? GetModuleFileNameW(
              host,
              host_path.data(),
              static_cast<DWORD>(host_path.size()))
        : 0;
    if (length == 0 || static_cast<std::size_t>(length) >= host_path.size()) {
        log_line("ERROR: loaded ERNativeUI host path unavailable");
        return false;
    }

    std::array<char, 32768> utf8_path{};
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        host_path.data(),
        static_cast<int>(length),
        utf8_path.data(),
        static_cast<int>(utf8_path.size() - 1),
        nullptr,
        nullptr);
    if (converted <= 0) {
        log_line("ERROR: loaded ERNativeUI host path conversion failed");
        return false;
    }
    utf8_path[static_cast<std::size_t>(converted)] = '\0';
    log_line("Loaded ERNativeUI host path=%s", utf8_path.data());

    constexpr wchar_t kExpectedSuffix[] = L"\\NativeTweaks\\ERNativeUI.dll";
    const std::size_t path_length = static_cast<std::size_t>(length);
    const std::size_t suffix_length = _countof(kExpectedSuffix) - 1;
    if (path_length < suffix_length ||
        _wcsicmp(
            host_path.data() + path_length - suffix_length,
            kExpectedSuffix) != 0) {
        log_line(
            "ERROR: this probe requires the validated NativeTweaks private host path suffix; no hook installed");
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(
            host_path.data(), GetFileExInfoStandard, &attributes)) {
        log_line("ERROR: could not inspect the private host file; no hook installed");
        return false;
    }
    const std::uint64_t file_size =
        (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
        attributes.nFileSizeLow;
    if (file_size != kValidatedPrivateHostFileSize) {
        log_line(
            "ERROR: private host size %llu differs from validated size %llu; no hook installed",
            static_cast<unsigned long long>(file_size),
            static_cast<unsigned long long>(kValidatedPrivateHostFileSize));
        return false;
    }
    log_line(
        "Validated NativeTweaks private host path and file size (%llu bytes)",
        static_cast<unsigned long long>(file_size));
    return true;
}

void log_categories(const void* top, const char* phase) noexcept {
    if (!top) return;
    std::uint64_t count{};
    if (!guarded_read(
            static_cast<const std::byte*>(top) + kCategoryCountOffset,
            count)) {
        log_line("%s: category count unreadable", phase);
        return;
    }

    std::array<char, 256> categories{};
    std::size_t used{};
    for (std::size_t index = 0;
         index < (std::min)(
             static_cast<std::size_t>(count), kNativeCategoryCapacity);
         ++index) {
        std::int32_t category{-1};
        const auto* const address = static_cast<const std::byte*>(top) +
            kCategoryStorageOffset + index * kCategoryRecordSize +
            kCategoryIdOffset;
        (void)guarded_read(address, category);
        const int written = snprintf(
            categories.data() + used,
            categories.size() - used,
            "%s%d",
            index == 0 ? "" : ",",
            category);
        if (written <= 0 ||
            static_cast<std::size_t>(written) >= categories.size() - used) {
            break;
        }
        used += static_cast<std::size_t>(written);
    }
    log_line(
        "%s: top=%p count=%llu categories=%s",
        phase,
        top,
        static_cast<unsigned long long>(count),
        categories.data());
}

bool mutate_duplicate_category(void* top) noexcept {
    if (!top) return false;

    void* top_vtable{};
    std::uint64_t count{};
    if (!guarded_read(top, top_vtable) ||
        top_vtable != g_game.base + kTopVtableRva ||
        !guarded_read(
            static_cast<const std::byte*>(top) + kCategoryCountOffset,
            count) ||
        count != 9) {
        log_line(
            "Skipped mutation: expected validated nine-record top object (top=%p vtable=%p count=%llu)",
            top,
            top_vtable,
            static_cast<unsigned long long>(count));
        return false;
    }

    constexpr std::array<std::int32_t, 9> kExpectedCategories{
        0, 1, 2, 3, 5, 6, 7, 8, 4};
    for (std::size_t index = 0; index < kExpectedCategories.size(); ++index) {
        const auto* const record = static_cast<const std::byte*>(top) +
            kCategoryStorageOffset + index * kCategoryRecordSize;
        void* record_vtable{};
        std::int32_t category{-1};
        if (!guarded_read(record, record_vtable) ||
            record_vtable != g_game.base + kCategoryRecordVtableRva ||
            !guarded_read(record + kCategoryIdOffset, category) ||
            category != kExpectedCategories[index]) {
            log_line(
                "Skipped mutation: record[%zu] failed validation (vtable=%p category=%d expected=%d)",
                index,
                record_vtable,
                category,
                kExpectedCategories[index]);
            return false;
        }
    }

    auto* const graphics_category = static_cast<std::byte*>(top) +
        kCategoryStorageOffset + 7 * kCategoryRecordSize + kCategoryIdOffset;
    if (!guarded_write_i32(graphics_category, 4)) {
        log_line("ERROR: could not change record[7] category from 8 to 4");
        return false;
    }

    std::int32_t readback{-1};
    if (!guarded_read(graphics_category, readback) || readback != 4) {
        log_line(
            "ERROR: record[7] category write did not persist (readback=%d)",
            readback);
        return false;
    }
    log_line(
        "Mutation applied: physical records 7 and 8 now both route through native category 4; record 7 retains Graphics presentation and record 8 retains NativeTweaks presentation");
    return true;
}

void* __fastcall top_constructor_detour(
    void* top,
    std::uintptr_t movie,
    std::uint8_t argument3,
    std::uint8_t title_menu) noexcept {
    TopConstructorFn const original =
        g_original_constructor.load(std::memory_order_acquire);
    void* const result = original
        ? original(top, movie, argument3, title_menu)
        : nullptr;

    const std::uint32_t call =
        g_constructor_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    log_line(
        "Top constructor[%u] returned top=%p result=%p titleMenu=%u",
        static_cast<unsigned>(call),
        top,
        result,
        static_cast<unsigned>(title_menu));
    log_categories(top, "before duplicate-category mutation");
    if (mutate_duplicate_category(top)) {
        log_categories(top, "after duplicate-category mutation");
    }
    return result;
}

bool wait_for_private_host_hooks() noexcept {
    constexpr std::array<std::uint8_t, 16> kCategoryAppendEntry{
        0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
        0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48};
    constexpr std::array<std::uint8_t, 16> kTabDispatcherEntry{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74,
        0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48};

    constexpr DWORD kPollMilliseconds = 25;
    constexpr DWORD kTimeoutMilliseconds = 60000;
    for (DWORD elapsed = 0; elapsed < kTimeoutMilliseconds;
         elapsed += kPollMilliseconds) {
        std::array<std::uint8_t, 16> append_actual{};
        std::array<std::uint8_t, 16> dispatcher_actual{};
        if (!guarded_copy(
                append_actual.data(),
                g_game.base + kCategoryAppendRva,
                append_actual.size()) ||
            !guarded_copy(
                dispatcher_actual.data(),
                g_game.base + kTabDispatcherRva,
                dispatcher_actual.size())) {
            log_line(
                "ERROR: could not read native hook entries; constructor hook not installed");
            return false;
        }
        if (append_actual != kCategoryAppendEntry &&
            dispatcher_actual != kTabDispatcherEntry) {
            log_line(
                "Observed both private-host hook entries modified after %lu ms",
                static_cast<unsigned long>(elapsed));
            return true;
        }
        Sleep(kPollMilliseconds);
    }
    log_line(
        "ERROR: timed out waiting for NativeTweaks' private host hooks; constructor hook not installed");
    return false;
}

bool install_constructor_hook() noexcept {
    constexpr std::array<std::uint8_t, 16> kTopConstructorEntry{
        0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41,
        0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC};
    if (!entry_matches(kTopConstructorRva, kTopConstructorEntry)) {
        log_line(
            "ERROR: top constructor differs from the validated executable; no mutation hook installed");
        return false;
    }

    auto result = SafetyHookInline::create(
        g_game.base + kTopConstructorRva,
        reinterpret_cast<void*>(&top_constructor_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "ERROR: could not install top constructor hook (SafetyHook error type %u)",
            static_cast<unsigned>(result.error().type));
        return false;
    }
    g_constructor_hook = std::move(*result);
    g_original_constructor.store(
        g_constructor_hook.original<TopConstructorFn>(),
        std::memory_order_release);
    auto enabled = g_constructor_hook.enable();
    if (!enabled) {
        log_line(
            "ERROR: could not enable top constructor hook (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_original_constructor.store(nullptr, std::memory_order_release);
        g_constructor_hook.reset();
        return false;
    }
    log_line(
        "Duplicate-category probe installed at eldenring.exe+0x%llX",
        static_cast<unsigned long long>(kTopConstructorRva));
    return true;
}

DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    log_line("============================================================");
    log_line("NativeTweaks duplicate-category Configuration-tab probe starting");

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
    if (!supported_game_image(g_game)) {
        log_line(
            "ERROR: unsupported Elden Ring executable identity; expected 2.7.0.0 or 2.7.1.0; no hook installed");
        return 1;
    }

    if (!validate_private_host()) return 1;
    if (!wait_for_private_host_hooks()) return 1;
    return install_constructor_hook() ? 0 : 1;
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
