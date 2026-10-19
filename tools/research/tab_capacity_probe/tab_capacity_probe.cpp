#include "module.hpp"

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
#include <limits>
#include <utility>

namespace {

// This observer is intentionally locked to the executable for which every
// native address and object boundary below was recovered.
constexpr std::uint32_t kEldenRing271Timestamp = 0x6A96B418;
constexpr std::size_t kEldenRing271ImageSize = 0x5E0DA00;

constexpr std::uintptr_t kTopBuilderRva = 0x807430;
constexpr std::uintptr_t kSelectorGetterRva = 0x73AC70;
constexpr std::uintptr_t kTopVtableRva = 0x2B16B48;
constexpr std::uintptr_t kSelectorVtableRva = 0x2A94438;
constexpr std::uintptr_t kSelectorDiscoverGridRva = 0x738AB0;
constexpr std::uintptr_t kDisplayResolveItemRva = 0x737E10;
constexpr std::uintptr_t kCategoryRecordVtableRva = 0x2AD6F20;

constexpr std::size_t kSelectorOffset = 0x0A38;
constexpr std::size_t kSelectorActiveCountOffset = 0x00D0;
constexpr std::size_t kSelectorSelectedIndexOffset = 0x00D4;
constexpr std::size_t kSelectorAxis0CountOffset = 0x00D8;
constexpr std::size_t kSelectorAxis1CountOffset = 0x00DC;
constexpr std::size_t kSelectorDisplayAdapterOffset = 0x0698;
constexpr std::size_t kSelectorDiscoverGridVslot = 0x18;
constexpr std::size_t kDisplayResolveItemVslot = 0x20;

constexpr std::size_t kCategoryStorageOffset = 0x1208;
constexpr std::size_t kCategoryCountOffset = 0x1760;
constexpr std::size_t kCategoryRecordSize = 0x88;
constexpr std::size_t kCategoryIdOffset = 0x40;
constexpr std::size_t kPresentationDescriptorOffset = 0x48;
constexpr std::size_t kNativeCategoryCapacity = 10;

constexpr std::int32_t kUnobservedSelection =
    (std::numeric_limits<std::int32_t>::min)();

using TopBuilderFn = void*(__fastcall*)(void* title_menu_state, void* scene);
using SelectorGetterFn = std::int32_t(__fastcall*)(void* selector);

struct RecordSummary {
    void* vtable{};
    std::int32_t category{-1};
    std::uint32_t presentation{};
    bool readable{};
    bool valid_vtable{};
};

struct TopSnapshot {
    void* top{};
    void* top_vtable{};
    void* selector_vtable{};
    void* discover_grid_entry{};
    void* display_adapter{};
    void* display_adapter_vtable{};
    void* resolve_item_entry{};
    std::uint64_t record_count{};
    std::int32_t active_count{-1};
    std::int32_t selected_index{-1};
    std::int32_t axis0_count{-1};
    std::int32_t axis1_count{-1};
    std::array<RecordSummary, kNativeCategoryCapacity> records{};
    std::size_t records_read{};
    bool primary_readable{};
    bool native_types_valid{};
    bool records_valid{};
    bool record_count_matches_active{};
    bool selected_is_bounded{};
    bool active_fits_discovered_grid{};
};

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
SRWLOCK g_log_lock = SRWLOCK_INIT;
std::array<wchar_t, 32768> g_log_path{};

erui::native::ModuleView g_game{};
SafetyHookInline g_top_builder_hook{};
SafetyHookInline g_selector_getter_hook{};
std::atomic<TopBuilderFn> g_original_top_builder{};
std::atomic<SelectorGetterFn> g_original_selector_getter{};

std::atomic<void*> g_active_top{};
std::atomic<std::uint32_t> g_top_generation{};
std::atomic<std::uint32_t> g_builder_calls{};
std::atomic<std::int32_t> g_last_observed_selection{kUnobservedSelection};
std::atomic<std::int32_t> g_max_observed_selection{-1};

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

const std::byte* offset_address(
    const void* base,
    std::size_t offset) noexcept {
    if (!base) return nullptr;
    const auto value = reinterpret_cast<std::uintptr_t>(base);
    if (offset > (std::numeric_limits<std::uintptr_t>::max)() - value) {
        return nullptr;
    }
    return reinterpret_cast<const std::byte*>(value + offset);
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
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [TabCapacityProbe] %s\r\n",
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
    (void)FlushFileBuffers(g_log_file);
    ReleaseSRWLockExclusive(&g_log_lock);
}

bool wide_path_to_utf8(
    const wchar_t* path,
    std::array<char, 32768>& utf8) noexcept {
    utf8.fill('\0');
    if (!path || path[0] == L'\0') return false;
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        path,
        -1,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        nullptr);
    return converted > 0;
}

bool initialize_log() noexcept {
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(
        g_module, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || static_cast<std::size_t>(length) >= module_path.size()) {
        return false;
    }

    wchar_t* const last_backslash = wcsrchr(module_path.data(), L'\\');
    wchar_t* const last_slash = wcsrchr(module_path.data(), L'/');
    wchar_t* const last_separator =
        last_backslash && last_slash
        ? (std::max)(last_backslash, last_slash)
        : (last_backslash ? last_backslash : last_slash);
    if (!last_separator) return false;
    *(last_separator + 1) = L'\0';

    constexpr wchar_t kLogName[] = L"ERNativeUITabCapacityProbe.log";
    if (wcslen(module_path.data()) + _countof(kLogName) > module_path.size()) {
        return false;
    }
    wcscat_s(module_path.data(), module_path.size(), kLogName);
    if (wcscpy_s(
            g_log_path.data(), g_log_path.size(), module_path.data()) != 0) {
        return false;
    }

    g_log_file = CreateFileW(
        g_log_path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return g_log_file != INVALID_HANDLE_VALUE;
}

void log_module_path(const wchar_t* module_name) noexcept {
    HMODULE const module = GetModuleHandleW(module_name);
    if (!module) {
        std::array<char, 512> name{};
        const int converted = WideCharToMultiByte(
            CP_UTF8,
            0,
            module_name,
            -1,
            name.data(),
            static_cast<int>(name.size()),
            nullptr,
            nullptr);
        log_line(
            "module name=%s loaded=0",
            converted > 0 ? name.data() : "<conversion-failed>");
        return;
    }

    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(
        module, path.data(), static_cast<DWORD>(path.size()));
    std::array<char, 32768> utf8{};
    if (length == 0 || static_cast<std::size_t>(length) >= path.size() ||
        !wide_path_to_utf8(path.data(), utf8)) {
        log_line("module handle=%p path=<unavailable>", static_cast<void*>(module));
        return;
    }
    log_line("module handle=%p path=%s", static_cast<void*>(module), utf8.data());
}

RecordSummary inspect_record(const void* top, std::size_t index) noexcept {
    RecordSummary summary{};
    if (!top || index >= kNativeCategoryCapacity) return summary;

    const std::size_t offset =
        kCategoryStorageOffset + index * kCategoryRecordSize;
    const std::byte* const record = offset_address(top, offset);
    if (!record ||
        !guarded_read(record, summary.vtable) ||
        !guarded_read(record + kCategoryIdOffset, summary.category) ||
        !guarded_read(
            record + kPresentationDescriptorOffset,
            summary.presentation)) {
        return summary;
    }
    summary.readable = true;
    summary.valid_vtable =
        summary.vtable == g_game.at_rva(kCategoryRecordVtableRva);
    return summary;
}

TopSnapshot inspect_top(void* top) noexcept {
    TopSnapshot snapshot{};
    snapshot.top = top;
    if (!top) return snapshot;

    const std::byte* const selector = offset_address(top, kSelectorOffset);
    const std::byte* const count_address =
        offset_address(top, kCategoryCountOffset);
    if (!selector || !count_address ||
        !guarded_read(top, snapshot.top_vtable) ||
        !guarded_read(selector, snapshot.selector_vtable) ||
        !guarded_read(
            selector + kSelectorActiveCountOffset,
            snapshot.active_count) ||
        !guarded_read(
            selector + kSelectorSelectedIndexOffset,
            snapshot.selected_index) ||
        !guarded_read(
            selector + kSelectorAxis0CountOffset,
            snapshot.axis0_count) ||
        !guarded_read(
            selector + kSelectorAxis1CountOffset,
            snapshot.axis1_count) ||
        !guarded_read(
            selector + kSelectorDisplayAdapterOffset,
            snapshot.display_adapter) ||
        !guarded_read(count_address, snapshot.record_count)) {
        return snapshot;
    }
    snapshot.primary_readable = true;

    if (snapshot.selector_vtable) {
        (void)guarded_read(
            offset_address(
                snapshot.selector_vtable,
                kSelectorDiscoverGridVslot),
            snapshot.discover_grid_entry);
    }
    if (snapshot.display_adapter &&
        guarded_read(
            snapshot.display_adapter,
            snapshot.display_adapter_vtable) &&
        snapshot.display_adapter_vtable) {
        (void)guarded_read(
            offset_address(
                snapshot.display_adapter_vtable,
                kDisplayResolveItemVslot),
            snapshot.resolve_item_entry);
    }

    const bool count_in_range =
        snapshot.record_count <= kNativeCategoryCapacity;
    const bool active_in_range =
        snapshot.active_count >= 0 &&
        snapshot.active_count <=
            static_cast<std::int32_t>(kNativeCategoryCapacity);
    const bool axes_in_range =
        snapshot.axis0_count > 0 && snapshot.axis0_count <= 32 &&
        snapshot.axis1_count > 0 && snapshot.axis1_count <= 64;
    snapshot.native_types_valid =
        snapshot.top_vtable == g_game.at_rva(kTopVtableRva) &&
        snapshot.selector_vtable == g_game.at_rva(kSelectorVtableRva) &&
        snapshot.discover_grid_entry ==
            g_game.at_rva(kSelectorDiscoverGridRva) &&
        snapshot.resolve_item_entry ==
            g_game.at_rva(kDisplayResolveItemRva) &&
        count_in_range && active_in_range && axes_in_range;

    snapshot.records_valid = count_in_range;
    if (count_in_range) {
        snapshot.records_read = static_cast<std::size_t>(snapshot.record_count);
        for (std::size_t index = 0; index < snapshot.records_read; ++index) {
            snapshot.records[index] = inspect_record(top, index);
            snapshot.records_valid = snapshot.records_valid &&
                snapshot.records[index].readable &&
                snapshot.records[index].valid_vtable;
        }
    }

    snapshot.record_count_matches_active = active_in_range &&
        snapshot.record_count ==
            static_cast<std::uint64_t>(snapshot.active_count);
    snapshot.selected_is_bounded = active_in_range &&
        snapshot.selected_index >= -1 &&
        (snapshot.selected_index == -1 ||
            snapshot.selected_index < snapshot.active_count);

    if (active_in_range && axes_in_range) {
        const std::int64_t discovered_cells =
            static_cast<std::int64_t>(snapshot.axis0_count) *
            static_cast<std::int64_t>(snapshot.axis1_count);
        snapshot.active_fits_discovered_grid =
            snapshot.active_count <= discovered_cells;
    }
    return snapshot;
}

void log_record_list(const TopSnapshot& snapshot) noexcept {
    if (snapshot.records_read == 0) {
        log_line("records top=%p count=0", snapshot.top);
        return;
    }
    for (std::size_t index = 0; index < snapshot.records_read; ++index) {
        const RecordSummary& record = snapshot.records[index];
        log_line(
            "record top=%p index=%zu readable=%d vtable=%p validVtable=%d category=%d presentation0=0x%08X",
            snapshot.top,
            index,
            record.readable ? 1 : 0,
            record.vtable,
            record.valid_vtable ? 1 : 0,
            record.category,
            static_cast<unsigned>(record.presentation));
    }
}

void log_snapshot(
    const TopSnapshot& snapshot,
    std::uint32_t call,
    std::uint32_t generation) noexcept {
    const bool axes_valid =
        snapshot.axis0_count > 0 && snapshot.axis0_count <= 32 &&
        snapshot.axis1_count > 0 && snapshot.axis1_count <= 64;
    const std::int32_t discovered_cells = axes_valid
        ? snapshot.axis0_count * snapshot.axis1_count
        : -1;
    const bool singleton_axis = axes_valid &&
        (snapshot.axis0_count == 1 || snapshot.axis1_count == 1);
    const std::int32_t spare_discovered_cells =
        axes_valid && snapshot.active_count >= 0 &&
            discovered_cells >= snapshot.active_count
        ? discovered_cells - snapshot.active_count
        : -1;

    log_line(
        "post-constructor call=%u generation=%u top=%p readable=%d nativeTypesValid=%d recordsValid=%d",
        static_cast<unsigned>(call),
        static_cast<unsigned>(generation),
        snapshot.top,
        snapshot.primary_readable ? 1 : 0,
        snapshot.native_types_valid ? 1 : 0,
        snapshot.records_valid ? 1 : 0);
    log_line(
        "capacity top=%p categoryRecords=%llu activeSelectable(D0)=%d selected(D4)=%d discoveredTrailingExtent(D8)=%d discoveredLeadingExtent(DC)=%d discoveredCells=%d spareDiscoveredCells=%d",
        snapshot.top,
        static_cast<unsigned long long>(snapshot.record_count),
        snapshot.active_count,
        snapshot.selected_index,
        snapshot.axis0_count,
        snapshot.axis1_count,
        discovered_cells,
        spare_discovered_cells);
    log_line(
        "capacity-invariants top=%p recordCountEqualsActive=%d selectedBoundedByActive=%d activeFitsDiscoveredGrid=%d singletonAxis=%d",
        snapshot.top,
        snapshot.record_count_matches_active ? 1 : 0,
        snapshot.selected_is_bounded ? 1 : 0,
        snapshot.active_fits_discovered_grid ? 1 : 0,
        singleton_axis ? 1 : 0);
    log_line(
        "discovery-path top=%p topVtable=%p selectorVtable=%p discoverGridSlot18=%p displayAdapter=%p adapterVtable=%p resolveItemSlot20=%p",
        snapshot.top,
        snapshot.top_vtable,
        snapshot.selector_vtable,
        snapshot.discover_grid_entry,
        snapshot.display_adapter,
        snapshot.display_adapter_vtable,
        snapshot.resolve_item_entry);
    log_record_list(snapshot);
}

void update_max_selection(std::int32_t value) noexcept {
    std::int32_t current =
        g_max_observed_selection.load(std::memory_order_relaxed);
    while (value > current &&
           !g_max_observed_selection.compare_exchange_weak(
               current,
               value,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

void* __fastcall top_builder_detour(
    void* title_menu_state,
    void* scene) noexcept {
    // A reused allocation may have the same address as the previous dialog.
    // Stop optional getter observations before native construction begins so
    // an in-constructor getter cannot be attributed to the retired instance.
    g_active_top.store(nullptr, std::memory_order_release);
    g_last_observed_selection.store(
        kUnobservedSelection,
        std::memory_order_release);

    TopBuilderFn const original =
        g_original_top_builder.load(std::memory_order_acquire);
    void* const top = original ? original(title_menu_state, scene) : nullptr;

    const std::uint32_t call =
        g_builder_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::uint32_t generation =
        g_top_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    const TopSnapshot snapshot = inspect_top(top);
    log_snapshot(snapshot, call, generation);

    g_last_observed_selection.store(
        kUnobservedSelection,
        std::memory_order_release);
    g_max_observed_selection.store(-1, std::memory_order_release);
    g_active_top.store(
        snapshot.native_types_valid && snapshot.records_valid ? top : nullptr,
        std::memory_order_release);
    return top;
}

std::int32_t __fastcall selector_getter_detour(void* selector) noexcept {
    SelectorGetterFn const original =
        g_original_selector_getter.load(std::memory_order_acquire);
    const std::int32_t result = original ? original(selector) : -1;

    void* const top = g_active_top.load(std::memory_order_acquire);
    if (!top || selector != offset_address(top, kSelectorOffset)) return result;

    const std::int32_t previous =
        g_last_observed_selection.exchange(result, std::memory_order_acq_rel);
    if (previous == result) return result;

    const TopSnapshot snapshot = inspect_top(top);
    if (!snapshot.native_types_valid || !snapshot.records_valid) return result;
    update_max_selection(result);

    const std::int32_t discovered_cells =
        snapshot.axis0_count > 0 && snapshot.axis1_count > 0
        ? snapshot.axis0_count * snapshot.axis1_count
        : -1;

    RecordSummary selected_record{};
    if (result >= 0 &&
        static_cast<std::uint64_t>(result) < snapshot.record_count) {
        selected_record = snapshot.records[static_cast<std::size_t>(result)];
    }
    log_line(
        "selection generation=%u top=%p getter=%d fieldD4=%d previous=%d activeSelectable(D0)=%d discoveredTrailingExtent(D8)=%d discoveredLeadingExtent(DC)=%d discoveredCells=%d maxObserved=%d category=%d presentation0=0x%08X bounded=%d",
        static_cast<unsigned>(
            g_top_generation.load(std::memory_order_acquire)),
        top,
        result,
        snapshot.selected_index,
        previous,
        snapshot.active_count,
        snapshot.axis0_count,
        snapshot.axis1_count,
        discovered_cells,
        g_max_observed_selection.load(std::memory_order_relaxed),
        selected_record.category,
        static_cast<unsigned>(selected_record.presentation),
        result >= -1 &&
                (result == -1 || result < snapshot.active_count)
            ? 1
            : 0);
    return result;
}

bool validate_static_profile() noexcept {
    if (g_game.timestamp() != kEldenRing271Timestamp ||
        g_game.image_size() != kEldenRing271ImageSize) {
        log_line(
            "ERROR: unsupported executable timestamp=0x%08X size=0x%zX; expected Elden Ring 2.7.1.0; no hooks installed",
            static_cast<unsigned>(g_game.timestamp()),
            g_game.image_size());
        return false;
    }

    constexpr char kTopBuilderEntry[] =
        "48 8B C4 56 57 41 56 48 81 EC 90 00 00 00 48 C7 "
        "40 80 FE FF FF FF 48 89";
    if (!g_game.matches(g_game.at_rva(kTopBuilderRva), kTopBuilderEntry)) {
        log_line(
            "ERROR: mandatory top-builder entry eldenring.exe+0x%llX is not pristine/validated; load this probe last and remove any conflicting hook; no hooks installed",
            static_cast<unsigned long long>(kTopBuilderRva));
        return false;
    }

    void* discover_grid{};
    if (!guarded_read(
            g_game.at_rva(
                kSelectorVtableRva + kSelectorDiscoverGridVslot),
            discover_grid) ||
        discover_grid != g_game.at_rva(kSelectorDiscoverGridRva)) {
        log_line(
            "ERROR: selector discovery vtable slot differs from the validated 2.7.1 profile; no hooks installed");
        return false;
    }
    return true;
}

bool prepare_top_builder_hook() noexcept {
    auto result = SafetyHookInline::create(
        g_game.at_rva(kTopBuilderRva),
        reinterpret_cast<void*>(&top_builder_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "ERROR: could not prepare mandatory top-builder hook (SafetyHook error type %u)",
            static_cast<unsigned>(result.error().type));
        return false;
    }
    g_top_builder_hook = std::move(*result);
    g_original_top_builder.store(
        g_top_builder_hook.original<TopBuilderFn>(),
        std::memory_order_release);
    auto enabled = g_top_builder_hook.enable();
    if (!enabled) {
        log_line(
            "ERROR: could not enable mandatory top-builder hook (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_original_top_builder.store(nullptr, std::memory_order_release);
        g_top_builder_hook.reset();
        return false;
    }
    log_line(
        "mandatory post-original top-builder observer installed at eldenring.exe+0x%llX",
        static_cast<unsigned long long>(kTopBuilderRva));
    return true;
}

void prepare_optional_selector_getter_hook() noexcept {
    constexpr char kSelectorGetterEntry[] = "8B 81 D4 00 00 00 C3";
    if (!g_game.matches(
            g_game.at_rva(kSelectorGetterRva),
            kSelectorGetterEntry)) {
        log_line(
            "WARNING: optional selector getter eldenring.exe+0x%llX is already claimed or differs; constructor capacity snapshots remain enabled, natural selection observations are unavailable",
            static_cast<unsigned long long>(kSelectorGetterRva));
        return;
    }

    auto result = SafetyHookInline::create(
        g_game.at_rva(kSelectorGetterRva),
        reinterpret_cast<void*>(&selector_getter_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "WARNING: optional selector-getter hook could not be prepared (SafetyHook error type %u); constructor snapshots remain enabled",
            static_cast<unsigned>(result.error().type));
        return;
    }
    g_selector_getter_hook = std::move(*result);
    g_original_selector_getter.store(
        g_selector_getter_hook.original<SelectorGetterFn>(),
        std::memory_order_release);
    auto enabled = g_selector_getter_hook.enable();
    if (!enabled) {
        log_line(
            "WARNING: optional selector-getter hook could not be enabled (SafetyHook error type %u); constructor snapshots remain enabled",
            static_cast<unsigned>(enabled.error().type));
        g_original_selector_getter.store(nullptr, std::memory_order_release);
        g_selector_getter_hook.reset();
        return;
    }
    log_line(
        "optional post-original selector-getter observer installed at eldenring.exe+0x%llX",
        static_cast<unsigned long long>(kSelectorGetterRva));
}

DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    log_line("============================================================");
    log_line("Observation-only live Configuration-tab capacity probe starting");

    std::array<char, 32768> utf8_log_path{};
    if (wide_path_to_utf8(g_log_path.data(), utf8_log_path)) {
        log_line("logPath=%s", utf8_log_path.data());
    }
    std::array<wchar_t, 32768> probe_path{};
    if (GetModuleFileNameW(
            g_module,
            probe_path.data(),
            static_cast<DWORD>(probe_path.size())) != 0) {
        std::array<char, 32768> utf8_probe_path{};
        if (wide_path_to_utf8(probe_path.data(), utf8_probe_path)) {
            log_line("probePath=%s", utf8_probe_path.data());
        }
    }

    HMODULE pinned{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(g_module),
            &pinned)) {
        log_line("ERROR: probe DLL could not be pinned; no hooks installed");
        return 1;
    }

    // The probe is documented as the last external DLL. Give earlier DLLs'
    // worker-based initializers time to claim their own seams before checking
    // that this observer's two entries remain pristine.
    Sleep(1500);
    log_module_path(L"ERNativeUI.dll");
    log_module_path(L"NativeTweaks.dll");

    if (!g_game.initialize(GetModuleHandleW(nullptr))) {
        log_line("ERROR: could not inspect the main executable image");
        return 1;
    }
    log_line(
        "gameImage base=%p timestamp=0x%08X size=0x%zX",
        static_cast<void*>(g_game.base()),
        static_cast<unsigned>(g_game.timestamp()),
        g_game.image_size());
    if (!validate_static_profile()) return 1;

    try {
        if (!prepare_top_builder_hook()) return 1;
        prepare_optional_selector_getter_hook();
    } catch (...) {
        log_line("ERROR: unexpected exception while installing probe hooks");
        g_original_selector_getter.store(nullptr, std::memory_order_release);
        g_selector_getter_hook.reset();
        g_original_top_builder.store(nullptr, std::memory_order_release);
        g_top_builder_hook.reset();
        return 1;
    }

    log_line(
        "probe ready; open System only after this line appears (the observer performs no tab, category-record, selection, or GFX-data writes)");
    return 0;
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
