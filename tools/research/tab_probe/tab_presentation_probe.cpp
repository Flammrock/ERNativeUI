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

constexpr std::uintptr_t kCategoryAppendRva = 0x968CF0;
constexpr std::uintptr_t kTabDispatcherRva = 0x9680D0;
constexpr std::uintptr_t kBackTabPresentRva = 0x967DE0;
constexpr std::uintptr_t kTopVtableRva = 0x2B16B48;
constexpr std::uintptr_t kCategoryRecordVtableRva = 0x2AD6F20;

constexpr std::size_t kCategoryStorageOffset = 0x1208;
constexpr std::size_t kCategoryCountOffset = 0x1760;
constexpr std::size_t kCategoryRecordSize = 0x88;
constexpr std::size_t kTitleOffset = 0x08;
constexpr std::size_t kCategoryIdOffset = 0x40;
constexpr std::size_t kPresentationOffset = 0x48;
constexpr std::size_t kPresentationSize = 0x40;
constexpr std::size_t kNativeCategoryCapacity = 10;

constexpr std::uint32_t kEldenRing270Timestamp = 0x69E9C9B9;
constexpr std::size_t kEldenRing270ImageSize = 0x5E09600;
constexpr std::uint32_t kEldenRing271Timestamp = 0x6A96B418;
constexpr std::size_t kEldenRing271ImageSize = 0x5E0DA00;
constexpr std::uint64_t kValidatedPrivateHostFileSize = 984576;
constexpr wchar_t kGraphicsTitle[] = L"Graphics";
constexpr wchar_t kNativeTweaksTitle[] = L"NativeTweaks";

using BackTabPresentFn = void(__fastcall*)(
    void* top_reference,
    void* scene_object,
    std::uint32_t physical_index);

struct GameImage {
    std::uint8_t* base{};
    std::size_t size{};
    std::uint32_t timestamp{};
};

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
SRWLOCK g_log_lock = SRWLOCK_INIT;
GameImage g_game{};
SafetyHookInline g_present_hook{};
std::atomic<BackTabPresentFn> g_original_present{};
std::atomic_uint32_t g_validation_failures{};

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
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [TabPresentationProbe] %s\r\n",
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
    constexpr wchar_t kLogName[] = L"ERNativeUITabPresentationProbe.log";
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

void log_validation_failure(
    const char* reason,
    const void* top,
    std::uint64_t count) noexcept {
    const std::uint32_t failure =
        g_validation_failures.fetch_add(1, std::memory_order_relaxed);
    if (failure < 4) {
        log_line(
            "Skipped presentation mutation: %s (top=%p count=%llu)",
            reason,
            top,
            static_cast<unsigned long long>(count));
    } else if (failure == 4) {
        log_line("Further presentation-validation failures are suppressed");
    }
}

bool swap_presentations_if_expected(void* top) noexcept {
    if (!top) return false;

    void* top_vtable{};
    std::uint64_t count{};
    if (!guarded_read(top, top_vtable) ||
        top_vtable != g_game.base + kTopVtableRva ||
        !guarded_read(
            static_cast<const std::byte*>(top) + kCategoryCountOffset,
            count) ||
        count != 9) {
        log_validation_failure("not the validated nine-record top object", top, count);
        return false;
    }

    constexpr std::array<std::int32_t, 9> kExpectedCategories{
        0, 1, 2, 3, 5, 6, 7, 8, 4};
    constexpr std::array<std::uint32_t, 9> kExpectedIcons{
        20, 6, 10, 11, 4, 30, 7, 8, 3};
    std::array<std::array<std::byte, kCategoryRecordSize>, 9> records{};
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto* const record = static_cast<const std::byte*>(top) +
            kCategoryStorageOffset + index * kCategoryRecordSize;
        if (!guarded_copy(records[index].data(), record, kCategoryRecordSize)) {
            log_validation_failure("record storage was unreadable", top, count);
            return false;
        }

        void* record_vtable{};
        std::int32_t category{-1};
        std::uint32_t icon{};
        std::memcpy(&record_vtable, records[index].data(), sizeof(record_vtable));
        std::memcpy(
            &category,
            records[index].data() + kCategoryIdOffset,
            sizeof(category));
        std::memcpy(
            &icon,
            records[index].data() + kPresentationOffset,
            sizeof(icon));
        if (record_vtable != g_game.base + kCategoryRecordVtableRva ||
            category != kExpectedCategories[index]) {
            log_validation_failure("record identity/order differed", top, count);
            return false;
        }

        if (index < 7 && icon != kExpectedIcons[index]) {
            log_validation_failure("stock icon order differed", top, count);
            return false;
        }
    }

    std::uint32_t graphics_icon{};
    std::uint32_t native_tweaks_icon{};
    std::memcpy(
        &graphics_icon,
        records[7].data() + kPresentationOffset,
        sizeof(graphics_icon));
    std::memcpy(
        &native_tweaks_icon,
        records[8].data() + kPresentationOffset,
        sizeof(native_tweaks_icon));

    // This function is called once for each physical cell. The reversed pair
    // proves this top object has already been processed and makes the mutation
    // idempotent without retaining a pointer beyond the object's lifetime.
    if (graphics_icon == kExpectedIcons[8] &&
        native_tweaks_icon == kExpectedIcons[7]) {
        return true;
    }
    if (graphics_icon != kExpectedIcons[7] ||
        native_tweaks_icon != kExpectedIcons[8]) {
        log_validation_failure("target icon tokens differed", top, count);
        return false;
    }

    auto* const graphics_record = static_cast<std::byte*>(top) +
        kCategoryStorageOffset + 7 * kCategoryRecordSize;
    auto* const native_tweaks_record = static_cast<std::byte*>(top) +
        kCategoryStorageOffset + 8 * kCategoryRecordSize;

    // +0x08 is a non-owning resolved-title pointer. The native fallback string
    // at +0x10 remains untouched: raw relocation of that owning object would
    // be unsafe. This DLL is pinned before the hook is installed, so these
    // static UTF-16 buffers outlive every top dialog that refers to them.
    const wchar_t* const graphics_visible_title = kNativeTweaksTitle;
    const wchar_t* const native_tweaks_visible_title = kGraphicsTitle;

    if (!guarded_copy(
            graphics_record + kTitleOffset,
            &graphics_visible_title,
            sizeof(graphics_visible_title)) ||
        !guarded_copy(
            native_tweaks_record + kTitleOffset,
            &native_tweaks_visible_title,
            sizeof(native_tweaks_visible_title)) ||
        !guarded_copy(
            graphics_record + kPresentationOffset,
            records[8].data() + kPresentationOffset,
            kPresentationSize) ||
        !guarded_copy(
            native_tweaks_record + kPresentationOffset,
            records[7].data() + kPresentationOffset,
            kPresentationSize)) {
        log_line("ERROR: presentation swap faulted after validation");
        return false;
    }

    log_line(
        "Presentation swap applied before native BackTab rendering: record[7] keeps category 8 but displays NativeTweaks title/icon; record[8] keeps category 4 but displays Graphics title/icon");
    return true;
}

void __fastcall back_tab_present_detour(
    void* top_reference,
    void* scene_object,
    std::uint32_t physical_index) noexcept {
    void* top{};
    if (top_reference && guarded_read(top_reference, top)) {
        (void)swap_presentations_if_expected(top);
    }

    BackTabPresentFn const original =
        g_original_present.load(std::memory_order_acquire);
    if (original) original(top_reference, scene_object, physical_index);
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
                "ERROR: could not read native hook entries; presentation hook not installed");
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
        "ERROR: timed out waiting for NativeTweaks' private host hooks; presentation hook not installed");
    return false;
}

bool install_present_hook() noexcept {
    constexpr std::array<std::uint8_t, 16> kBackTabPresentEntry{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x57,
        0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00, 0x48};
    if (!entry_matches(kBackTabPresentRva, kBackTabPresentEntry)) {
        log_line(
            "ERROR: BackTab presentation function differs from the validated executable; no mutation hook installed");
        return false;
    }

    auto result = SafetyHookInline::create(
        g_game.base + kBackTabPresentRva,
        reinterpret_cast<void*>(&back_tab_present_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "ERROR: could not install BackTab presentation hook (SafetyHook error type %u)",
            static_cast<unsigned>(result.error().type));
        return false;
    }
    g_present_hook = std::move(*result);
    g_original_present.store(
        g_present_hook.original<BackTabPresentFn>(),
        std::memory_order_release);
    auto enabled = g_present_hook.enable();
    if (!enabled) {
        log_line(
            "ERROR: could not enable BackTab presentation hook (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_original_present.store(nullptr, std::memory_order_release);
        g_present_hook.reset();
        return false;
    }
    log_line(
        "Presentation probe installed at eldenring.exe+0x%llX",
        static_cast<unsigned long long>(kBackTabPresentRva));
    return true;
}

DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    log_line("============================================================");
    log_line("NativeTweaks Configuration-tab presentation probe starting");

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
    return install_present_hook() ? 0 : 1;
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
