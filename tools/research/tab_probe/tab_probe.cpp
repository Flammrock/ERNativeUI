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
constexpr std::uintptr_t kBackTabCallbackRva = 0x968590;

constexpr std::uintptr_t kTopVtableRva = 0x2B16B48;
constexpr std::uintptr_t kTabControllerVtableRva = 0x2B16AB8;
constexpr std::uintptr_t kCategoryRecordVtableRva = 0x2AD6F20;

constexpr std::size_t kSelectorOffset = 0x0A38;
constexpr std::size_t kSelectorSelectedIndexOffset = 0x00D4;
constexpr std::size_t kCategoryStorageOffset = 0x1208;
constexpr std::size_t kCategoryCountOffset = 0x1760;
constexpr std::size_t kCompositeOffset = 0x1768;
constexpr std::size_t kCompositePanelCacheOffset = 0x0068;
constexpr std::size_t kCompositeCurrentPanelOffset = 0x00B8;
constexpr std::size_t kTabControllerOffset = 0x1870;
constexpr std::size_t kCategoryRecordSize = 0x88;
constexpr std::size_t kCategoryIdOffset = 0x40;
constexpr std::size_t kPresentationDescriptorOffset = 0x48;
constexpr std::size_t kNativeCategoryCapacity = 10;

constexpr std::uint32_t kEldenRing270Timestamp = 0x69E9C9B9;
constexpr std::size_t kEldenRing270ImageSize = 0x5E09600;
constexpr std::uint32_t kEldenRing271Timestamp = 0x6A96B418;
constexpr std::size_t kEldenRing271ImageSize = 0x5E0DA00;

constexpr std::uint32_t kMaximumAppendLogs = 64;
constexpr std::uint32_t kMaximumTransitionLogs = 256;
constexpr std::uint32_t kMaximumCallbackLogs = 256;

using CategoryAppendFn = void(__fastcall*)(
    void* storage,
    const void* source_record,
    std::uintptr_t argument3,
    std::uintptr_t argument4);
using TabDispatcherFn = void(__fastcall*)(
    void* tab_controller,
    float frame_value,
    const std::uint8_t* input_enabled);
using BackTabCallbackFn = void(__fastcall*)(
    void* callable,
    void* scene_proxy,
    const std::int32_t* physical_index);

struct GameImage {
    std::uint8_t* base{};
    std::size_t size{};
    std::uint32_t timestamp{};
};

struct TopSnapshot {
    void* top{};
    void* top_vtable{};
    void* controller_vtable{};
    void* selector_link{};
    void* composite_link{};
    void* current_panel{};
    void* current_panel_vtable{};
    std::uint64_t category_count{};
    std::int32_t selected_index{-1};
    bool valid{};
};

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
SRWLOCK g_log_lock = SRWLOCK_INIT;
GameImage g_game{};

SafetyHookInline g_category_append_hook{};
SafetyHookInline g_tab_dispatcher_hook{};
SafetyHookInline g_back_tab_callback_hook{};
std::atomic<CategoryAppendFn> g_original_category_append{};
std::atomic<TabDispatcherFn> g_original_tab_dispatcher{};
std::atomic<BackTabCallbackFn> g_original_back_tab_callback{};

std::atomic<std::uint32_t> g_append_log_count{};
std::atomic<std::uint32_t> g_transition_log_count{};
std::atomic<std::uint32_t> g_callback_log_count{};
std::atomic<void*> g_last_snapshot_top{};
std::atomic_bool g_host_path_logged{};

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
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [TabProbe] %s\r\n",
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
    if (length == 0 ||
        static_cast<std::size_t>(length) >= module_path.size()) {
        return false;
    }

    wchar_t* const last_separator = wcsrchr(module_path.data(), L'\\');
    if (!last_separator) return false;
    *(last_separator + 1) = L'\0';
    constexpr wchar_t kLogName[] = L"ERNativeUITabProbe.log";
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

void* read_vtable(void* object) noexcept {
    void* vtable{};
    return guarded_read(object, vtable) ? vtable : nullptr;
}

std::int32_t read_selected_index(void* top) noexcept {
    std::int32_t selected{-1};
    if (!top) return selected;
    const auto* const address = static_cast<const std::byte*>(top) +
        kSelectorOffset + kSelectorSelectedIndexOffset;
    (void)guarded_read(address, selected);
    return selected;
}

std::int32_t category_for_index(void* top, std::int32_t index) noexcept {
    if (!top || index < 0 ||
        static_cast<std::size_t>(index) >= kNativeCategoryCapacity) {
        return -1;
    }
    std::int32_t category{-1};
    const auto* const record = static_cast<const std::byte*>(top) +
        kCategoryStorageOffset +
        static_cast<std::size_t>(index) * kCategoryRecordSize;
    (void)guarded_read(record + kCategoryIdOffset, category);
    return category;
}

TopSnapshot inspect_top_from_controller(void* tab_controller) noexcept {
    TopSnapshot snapshot{};
    if (!tab_controller) return snapshot;

    auto* const top = static_cast<std::byte*>(tab_controller) -
        kTabControllerOffset;
    snapshot.top = top;
    snapshot.top_vtable = read_vtable(top);
    snapshot.controller_vtable = read_vtable(tab_controller);
    (void)guarded_read(
        static_cast<std::byte*>(tab_controller) + 0x10,
        snapshot.selector_link);
    (void)guarded_read(
        static_cast<std::byte*>(tab_controller) + 0x18,
        snapshot.composite_link);
    (void)guarded_read(
        top + kCategoryCountOffset,
        snapshot.category_count);
    snapshot.selected_index = read_selected_index(top);
    (void)guarded_read(
        top + kCompositeOffset + kCompositeCurrentPanelOffset,
        snapshot.current_panel);
    snapshot.current_panel_vtable = read_vtable(snapshot.current_panel);

    snapshot.valid = snapshot.top_vtable == g_game.base + kTopVtableRva &&
        snapshot.controller_vtable ==
            g_game.base + kTabControllerVtableRva &&
        snapshot.selector_link == top + kSelectorOffset &&
        snapshot.composite_link == top + kCompositeOffset &&
        snapshot.category_count <= kNativeCategoryCapacity &&
        snapshot.selected_index >= -1 &&
        (snapshot.selected_index == -1 ||
            static_cast<std::uint64_t>(snapshot.selected_index) <
                snapshot.category_count);
    return snapshot;
}

void log_category_records(const TopSnapshot& snapshot) noexcept {
    if (!snapshot.valid) return;
    auto* const top = static_cast<std::byte*>(snapshot.top);
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(snapshot.category_count);
         ++index) {
        const auto* const record = top + kCategoryStorageOffset +
            index * kCategoryRecordSize;
        void* record_vtable{};
        std::int32_t category{-1};
        std::array<std::uint32_t, 16> descriptor{};
        if (!guarded_read(record, record_vtable) ||
            !guarded_read(record + kCategoryIdOffset, category) ||
            !guarded_copy(
                descriptor.data(),
                record + kPresentationDescriptorOffset,
                sizeof(descriptor))) {
            log_line(
                "snapshot record[%zu] unreadable address=%p",
                index,
                static_cast<const void*>(record));
            continue;
        }
        log_line(
            "snapshot record[%zu] address=%p vtable=%p validVtable=%d category=%d presentation=%u descriptor=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X",
            index,
            static_cast<const void*>(record),
            record_vtable,
            record_vtable == g_game.base + kCategoryRecordVtableRva ? 1 : 0,
            category,
            static_cast<unsigned>(descriptor[0]),
            descriptor[0], descriptor[1], descriptor[2], descriptor[3],
            descriptor[4], descriptor[5], descriptor[6], descriptor[7],
            descriptor[8], descriptor[9], descriptor[10], descriptor[11],
            descriptor[12], descriptor[13], descriptor[14], descriptor[15]);
    }
}

void log_panel_cache(const TopSnapshot& snapshot) noexcept {
    if (!snapshot.valid) return;
    auto* const top = static_cast<std::byte*>(snapshot.top);
    for (std::size_t category = 0;
         category < kNativeCategoryCapacity;
         ++category) {
        void* panel{};
        (void)guarded_read(
            top + kCompositeOffset + kCompositePanelCacheOffset +
                category * sizeof(void*),
            panel);
        if (!panel) continue;
        log_line(
            "snapshot panelCache[%zu] panel=%p vtable=%p%s",
            category,
            panel,
            read_vtable(panel),
            panel == snapshot.current_panel ? " current" : "");
    }
}

void log_top_snapshot(const TopSnapshot& snapshot, const char* reason) noexcept {
    if (!g_host_path_logged.exchange(true, std::memory_order_acq_rel)) {
        std::array<wchar_t, 32768> host_path{};
        HMODULE const host = GetModuleHandleW(L"ERNativeUI.dll");
        const DWORD length = host
            ? GetModuleFileNameW(
                host,
                host_path.data(),
                static_cast<DWORD>(host_path.size()))
            : 0;
        if (length != 0 &&
            static_cast<std::size_t>(length) < host_path.size()) {
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
            if (converted > 0) {
                utf8_path[static_cast<std::size_t>(converted)] = '\0';
                log_line("loaded ERNativeUI host path=%s", utf8_path.data());
            }
        } else {
            log_line("loaded ERNativeUI host path unavailable");
        }
    }
    log_line(
        "snapshot reason=%s top=%p valid=%d topVtable=%p controllerVtable=%p selector=%p composite=%p count=%llu selectedIndex=%d selectedCategory=%d currentPanel=%p currentPanelVtable=%p",
        reason ? reason : "unknown",
        snapshot.top,
        snapshot.valid ? 1 : 0,
        snapshot.top_vtable,
        snapshot.controller_vtable,
        snapshot.selector_link,
        snapshot.composite_link,
        static_cast<unsigned long long>(snapshot.category_count),
        snapshot.selected_index,
        category_for_index(snapshot.top, snapshot.selected_index),
        snapshot.current_panel,
        snapshot.current_panel_vtable);
    log_category_records(snapshot);
    log_panel_cache(snapshot);
}

void __fastcall category_append_detour(
    void* storage,
    const void* source_record,
    std::uintptr_t argument3,
    std::uintptr_t argument4) noexcept {
    std::uint64_t count_before{};
    void* source_vtable{};
    std::int32_t category{-1};
    std::array<std::uint32_t, 16> descriptor{};
    const bool readable = storage && source_record &&
        guarded_read(
            static_cast<const std::byte*>(storage) +
                (kCategoryCountOffset - kCategoryStorageOffset),
            count_before) &&
        guarded_read(source_record, source_vtable) &&
        guarded_read(
            static_cast<const std::byte*>(source_record) + kCategoryIdOffset,
            category) &&
        guarded_copy(
            descriptor.data(),
            static_cast<const std::byte*>(source_record) +
                kPresentationDescriptorOffset,
            sizeof(descriptor));

    // A fresh list begins at count zero. Clear the last-instance marker here
    // so a native allocator reusing the same top-dialog address still causes
    // a complete first-frame snapshot after Configuration is reopened.
    if (readable && count_before == 0) {
        g_last_snapshot_top.store(nullptr, std::memory_order_release);
    }

    CategoryAppendFn original =
        g_original_category_append.load(std::memory_order_acquire);
    if (original) {
        original(storage, source_record, argument3, argument4);
    }

    std::uint64_t count_after{};
    (void)guarded_read(
        static_cast<const std::byte*>(storage) +
            (kCategoryCountOffset - kCategoryStorageOffset),
        count_after);
    const std::uint32_t event =
        g_append_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumAppendLogs) {
        auto* const top = storage
            ? static_cast<std::byte*>(storage) - kCategoryStorageOffset
            : nullptr;
        log_line(
            "append[%u] top=%p storage=%p source=%p readable=%d sourceVtable=%p validVtable=%d count=%llu->%llu category=%d presentation=%u",
            static_cast<unsigned>(event),
            static_cast<void*>(top),
            storage,
            source_record,
            readable ? 1 : 0,
            source_vtable,
            source_vtable == g_game.base + kCategoryRecordVtableRva ? 1 : 0,
            static_cast<unsigned long long>(count_before),
            static_cast<unsigned long long>(count_after),
            category,
            static_cast<unsigned>(descriptor[0]));
    }
}

void __fastcall tab_dispatcher_detour(
    void* tab_controller,
    float frame_value,
    const std::uint8_t* input_enabled) noexcept {
    const TopSnapshot before = inspect_top_from_controller(tab_controller);
    std::uint8_t input{};
    (void)guarded_read(input_enabled, input);

    TabDispatcherFn original =
        g_original_tab_dispatcher.load(std::memory_order_acquire);
    if (original) {
        original(tab_controller, frame_value, input_enabled);
    }

    const TopSnapshot after = inspect_top_from_controller(tab_controller);
    void* const previous_snapshot_top = g_last_snapshot_top.exchange(
        after.valid ? after.top : nullptr,
        std::memory_order_acq_rel);
    if (after.valid && previous_snapshot_top != after.top) {
        log_top_snapshot(after, "first-frame-of-configuration-instance");
    }

    if (!before.valid || !after.valid ||
        before.selected_index == after.selected_index) {
        return;
    }
    const std::uint32_t event =
        g_transition_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event > kMaximumTransitionLogs) return;
    log_line(
        "transition[%u] top=%p input=%u frame=%.6g selected=%d(category=%d)->%d(category=%d) panel=%p(vtable=%p)->%p(vtable=%p) count=%llu",
        static_cast<unsigned>(event),
        after.top,
        static_cast<unsigned>(input),
        static_cast<double>(frame_value),
        before.selected_index,
        category_for_index(before.top, before.selected_index),
        after.selected_index,
        category_for_index(after.top, after.selected_index),
        before.current_panel,
        before.current_panel_vtable,
        after.current_panel,
        after.current_panel_vtable,
        static_cast<unsigned long long>(after.category_count));
    log_panel_cache(after);
}

void __fastcall back_tab_callback_detour(
    void* callable,
    void* scene_proxy,
    const std::int32_t* physical_index) noexcept {
    std::int32_t index{-1};
    void* captured_top{};
    (void)guarded_read(physical_index, index);
    if (callable) {
        (void)guarded_read(
            static_cast<const std::byte*>(callable) + sizeof(void*),
            captured_top);
    }
    const std::uint32_t event =
        g_callback_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumCallbackLogs) {
        log_line(
            "backTabCallback[%u] callable=%p capturedTop=%p sceneProxy=%p callbackIndex=%d selectedBefore=%d",
            static_cast<unsigned>(event),
            callable,
            captured_top,
            scene_proxy,
            index,
            read_selected_index(captured_top));
    }

    BackTabCallbackFn original =
        g_original_back_tab_callback.load(std::memory_order_acquire);
    if (original) {
        original(callable, scene_proxy, physical_index);
    }
}

template <typename Function>
bool prepare_hook(
    std::uintptr_t rva,
    void* detour,
    SafetyHookInline& hook,
    std::atomic<Function>& original,
    const char* name) noexcept {
    auto result = SafetyHookInline::create(
        g_game.base + rva,
        detour,
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "ERROR: could not prepare %s hook at RVA 0x%llX (SafetyHook error type %u)",
            name,
            static_cast<unsigned long long>(rva),
            static_cast<unsigned>(result.error().type));
        return false;
    }
    hook = std::move(*result);
    original.store(hook.original<Function>(), std::memory_order_release);
    return true;
}

bool enable_hook(SafetyHookInline& hook, const char* name) noexcept {
    auto result = hook.enable();
    if (!result) {
        log_line(
            "ERROR: could not enable %s hook (SafetyHook error type %u)",
            name,
            static_cast<unsigned>(result.error().type));
        return false;
    }
    return true;
}

void reset_hooks() noexcept {
    g_back_tab_callback_hook.reset();
    g_tab_dispatcher_hook.reset();
    g_category_append_hook.reset();
    g_original_back_tab_callback.store(nullptr, std::memory_order_release);
    g_original_tab_dispatcher.store(nullptr, std::memory_order_release);
    g_original_category_append.store(nullptr, std::memory_order_release);
}

bool install_hooks() noexcept {
    constexpr std::array<std::uint8_t, 16> kCategoryAppendEntry{
        0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
        0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48};
    constexpr std::array<std::uint8_t, 16> kTabDispatcherEntry{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74,
        0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48};
    constexpr std::array<std::uint8_t, 16> kBackTabCallbackEntry{
        0x4C, 0x8B, 0xDC, 0x57, 0x48, 0x81, 0xEC, 0x90,
        0x00, 0x00, 0x00, 0x49, 0xC7, 0x43, 0x88, 0xFE};

    if (!entry_matches(kCategoryAppendRva, kCategoryAppendEntry) ||
        !entry_matches(kTabDispatcherRva, kTabDispatcherEntry) ||
        !entry_matches(kBackTabCallbackRva, kBackTabCallbackEntry)) {
        log_line(
            "ERROR: one or more probe entries differ from the validated executable; no hooks installed (another DLL may have detoured them first)");
        return false;
    }

    if (!prepare_hook(
            kCategoryAppendRva,
            reinterpret_cast<void*>(&category_append_detour),
            g_category_append_hook,
            g_original_category_append,
            "category append") ||
        !prepare_hook(
            kTabDispatcherRva,
            reinterpret_cast<void*>(&tab_dispatcher_detour),
            g_tab_dispatcher_hook,
            g_original_tab_dispatcher,
            "tab dispatcher") ||
        !prepare_hook(
            kBackTabCallbackRva,
            reinterpret_cast<void*>(&back_tab_callback_detour),
            g_back_tab_callback_hook,
            g_original_back_tab_callback,
            "BackTabList callback")) {
        reset_hooks();
        return false;
    }

    if (!enable_hook(g_category_append_hook, "category append") ||
        !enable_hook(g_tab_dispatcher_hook, "tab dispatcher") ||
        !enable_hook(g_back_tab_callback_hook, "BackTabList callback")) {
        reset_hooks();
        return false;
    }

    log_line(
        "Probe hooks installed: append=eldenring.exe+0x%llX dispatcher=+0x%llX BackTabListCallback=+0x%llX",
        static_cast<unsigned long long>(kCategoryAppendRva),
        static_cast<unsigned long long>(kTabDispatcherRva),
        static_cast<unsigned long long>(kBackTabCallbackRva));
    return true;
}

DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    log_line("============================================================");
    log_line("Observation-only Configuration tab probe starting");

    HMODULE pinned{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(g_module),
            &pinned)) {
        log_line("ERROR: probe DLL could not be pinned; no hooks installed");
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
            "ERROR: unsupported Elden Ring executable identity; expected 2.7.0.0 or 2.7.1.0; no hooks installed");
        return 1;
    }

    return install_hooks() ? 0 : 1;
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
