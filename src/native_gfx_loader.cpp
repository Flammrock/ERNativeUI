#include "native_gfx_loader.hpp"

#include "game_build.hpp"
#include "gfx/settings_gfx_runtime.hpp"
#include "module.hpp"
#include "runtime_log.hpp"

#include <Windows.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using erui::gfx::SettingsGfxRuntimeCache;
using erui::gfx::SettingsGfxRuntimeOutcome;
using erui::gfx::SettingsGfxRuntimeResult;
using erui::gfx::SettingsGfxRuntimeTarget;

constexpr std::uintptr_t kFileOpener271Rva = 0xD6D280;
constexpr std::uintptr_t kFileOpenerVtable271Rva = 0x2BBE498;
constexpr std::uintptr_t kMemoryFileVtable271Rva = 0x2BA7D70;
constexpr std::array<std::uint8_t, 24> kFileOpener271Entry{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41,
    0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xD9,
    0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x48};
constexpr std::array<std::uintptr_t, 12> kMemoryFile271VslotRvas{
    0xCE9730, 0xCE9960, 0xCE9B30, 0xCE9B40,
    0xCE9C50, 0xCE9B80, 0xCE9970, 0xCE9B50,
    0xCE9950, 0xCE9C60, 0xCE9B90, 0xCE9C20};

constexpr std::size_t kMaximumPathLength = 511;
constexpr std::size_t kTargetCount = 2;

using OpenFileFn = void*(__fastcall*)(
    void* file_opener,
    const char* path,
    std::int32_t flags,
    std::int32_t mode);

struct MemoryFileSnapshot {
    void* vtable{};
    std::uint32_t refcount{};
    const std::uint8_t* bytes{};
    std::int32_t length{};
    std::int32_t cursor{};
    std::uint8_t valid{};
};

struct MemoryFileView {
    const std::uint8_t* bytes{};
    std::int32_t length{};
    std::int32_t cursor{};
};

static_assert(sizeof(MemoryFileView) == 0x10);

enum class NativePatchFailure : std::uint8_t {
    none,
    null_file,
    unreadable_layout,
    unexpected_file_type,
    invalid_state,
    invalid_length,
    pointer_overflow,
    allocation_failed,
    unreadable_bytes,
    changed_during_copy,
    destination_not_writable,
    write_fault,
    verification_failed,
    restoration_failed,
};

SafetyHookInline g_open_file_hook{};
std::atomic<OpenFileFn> g_original_open_file{};
std::atomic<SettingsGfxRuntimeCache*> g_runtime_cache{};
std::atomic_bool g_installed{};
std::mutex g_install_mutex{};
void* g_expected_memory_file_vtable{};

std::array<std::atomic_bool, kTargetCount> g_success_reported{};
std::array<std::atomic_bool, kTargetCount> g_unchanged_reported{};
std::array<std::atomic_bool, kTargetCount> g_transform_failure_reported{};
std::array<std::atomic_bool, kTargetCount> g_native_failure_reported{};

[[nodiscard]] std::size_t target_index(
    SettingsGfxRuntimeTarget target) noexcept {
    switch (target) {
    case SettingsGfxRuntimeTarget::game_options: return 0;
    case SettingsGfxRuntimeTarget::advanced_settings: return 1;
    case SettingsGfxRuntimeTarget::none: break;
    }
    return kTargetCount;
}

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

bool read_memory_file_snapshot(
    const void* file,
    MemoryFileSnapshot& snapshot) noexcept {
    snapshot = {};
    if (!file) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(file);
    if (address > (std::numeric_limits<std::uintptr_t>::max)() - 0x29U) {
        return false;
    }
    const auto* const object = static_cast<const std::byte*>(file);
    return guarded_copy(&snapshot.vtable, object, sizeof(snapshot.vtable)) &&
        guarded_copy(
            &snapshot.refcount, object + 0x08, sizeof(snapshot.refcount)) &&
        guarded_copy(&snapshot.bytes, object + 0x18, sizeof(snapshot.bytes)) &&
        guarded_copy(&snapshot.length, object + 0x20, sizeof(snapshot.length)) &&
        guarded_copy(&snapshot.cursor, object + 0x24, sizeof(snapshot.cursor)) &&
        guarded_copy(&snapshot.valid, object + 0x28, sizeof(snapshot.valid));
}

[[nodiscard]] bool same_snapshot(
    const MemoryFileSnapshot& left,
    const MemoryFileSnapshot& right) noexcept {
    return left.vtable == right.vtable &&
        left.refcount == right.refcount &&
        left.bytes == right.bytes &&
        left.length == right.length &&
        left.cursor == right.cursor &&
        left.valid == right.valid;
}

[[nodiscard]] bool writable_region(
    const void* address,
    std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xFFU;
    const bool writable = protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
    if (!writable) return false;

    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto region_start =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    if (start < region_start ||
        start > (std::numeric_limits<std::uintptr_t>::max)() - size ||
        region_start > (std::numeric_limits<std::uintptr_t>::max)() -
            information.RegionSize) {
        return false;
    }
    return start + size <= region_start + information.RegionSize;
}

[[nodiscard]] void* memory_file_view_address(void* file) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(file);
    if (!address ||
        address > (std::numeric_limits<std::uintptr_t>::max)() - 0x18U) {
        return nullptr;
    }
    return reinterpret_cast<void*>(address + 0x18U);
}

bool restore_memory_file_view(
    void* file,
    const MemoryFileSnapshot& expected,
    const MemoryFileView& original) noexcept {
    void* const destination = memory_file_view_address(file);
    if (!destination ||
        !guarded_copy(destination, &original, sizeof(original))) {
        return false;
    }
    MemoryFileSnapshot restored{};
    return read_memory_file_snapshot(file, restored) &&
        same_snapshot(expected, restored);
}

[[nodiscard]] const char* native_patch_failure_name(
    NativePatchFailure failure) noexcept {
    switch (failure) {
    case NativePatchFailure::none: return "none";
    case NativePatchFailure::null_file: return "null_file";
    case NativePatchFailure::unreadable_layout: return "unreadable_layout";
    case NativePatchFailure::unexpected_file_type:
        return "unexpected_file_type";
    case NativePatchFailure::invalid_state: return "invalid_state";
    case NativePatchFailure::invalid_length: return "invalid_length";
    case NativePatchFailure::pointer_overflow: return "pointer_overflow";
    case NativePatchFailure::allocation_failed: return "allocation_failed";
    case NativePatchFailure::unreadable_bytes: return "unreadable_bytes";
    case NativePatchFailure::changed_during_copy:
        return "changed_during_copy";
    case NativePatchFailure::destination_not_writable:
        return "destination_not_writable";
    case NativePatchFailure::write_fault: return "write_fault";
    case NativePatchFailure::verification_failed:
        return "verification_failed";
    case NativePatchFailure::restoration_failed:
        return "restoration_failed";
    }
    return "unknown";
}

void report_native_failure_once(
    SettingsGfxRuntimeTarget target,
    NativePatchFailure failure) noexcept {
    // Restoration failure is materially different from an ordinary fail-open
    // boundary rejection: the native file view may no longer be coherent.
    // Never let an earlier one-shot warning suppress this error.
    if (failure == NativePatchFailure::restoration_failed) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Runtime GFX completion could not restore '%s' after a failed MemoryFile write; the file view may be inconsistent for this process",
            erui::gfx::settings_gfx_runtime_target_name(target));
        return;
    }
    const std::size_t index = target_index(target);
    if (index >= kTargetCount ||
        g_native_failure_reported[index].exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    erui::detail::logf(
        erui::LogLevel::warning,
        "Runtime GFX completion left '%s' unchanged: native MemoryFile boundary failed (%s)",
        erui::gfx::settings_gfx_runtime_target_name(target),
        native_patch_failure_name(failure));
}

void report_transform_failure_once(
    const SettingsGfxRuntimeResult& result) noexcept {
    const std::size_t index = target_index(result.target);
    if (index >= kTargetCount ||
        g_transform_failure_reported[index].exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    erui::detail::logf(
        erui::LogLevel::warning,
        "Runtime GFX completion left '%s' unchanged: stage=%s document=%s settings=%s feature=%s detail=%u",
        erui::gfx::settings_gfx_runtime_target_name(result.target),
        erui::gfx::settings_gfx_runtime_error_name(result.error.code),
        erui::gfx::gfx_document_error_name(
            result.error.document_error.code),
        erui::gfx::settings_gfx_error_name(
            result.error.settings_error.code),
        erui::gfx::settings_gfx_feature_name(
            result.error.settings_error.feature),
        static_cast<unsigned>(result.error.settings_error.detail));
}

void report_unchanged_once(
    const SettingsGfxRuntimeResult& result,
    std::size_t source_size) noexcept {
    const std::size_t index = target_index(result.target);
    if (index >= kTargetCount ||
        g_unchanged_reported[index].exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    erui::detail::logf(
        erui::LogLevel::info,
        "Runtime GFX completion inspected '%s': already compatible, original MemoryFile retained (bytes=%zu rows=[game:%u camera:%u display:%u sound:%u network:%u keyboardMouse:%u graphics:%u] tabs=[frontCells:%u backCells:%u authoredCapacity:%u] textInput=%u colorPicker=%u)",
        erui::gfx::settings_gfx_runtime_target_name(result.target),
        source_size,
        static_cast<unsigned>(result.after.game_options_rows),
        static_cast<unsigned>(result.after.camera_options_rows),
        static_cast<unsigned>(result.after.display_rows),
        static_cast<unsigned>(result.after.sound_rows),
        static_cast<unsigned>(result.after.network_rows),
        static_cast<unsigned>(result.after.keyboard_mouse_rows),
        static_cast<unsigned>(result.after.graphics_rows),
        static_cast<unsigned>(result.after.tab_list_cells),
        static_cast<unsigned>(result.after.back_tab_list_cells),
        static_cast<unsigned>(result.after.authored_tab_slot_capacity),
        result.after.character_name_text_input ? 1U : 0U,
        result.after.character_creation_color_picker ? 1U : 0U);
}

void report_success_once(
    const SettingsGfxRuntimeResult& result,
    std::size_t source_size) noexcept {
    const std::size_t index = target_index(result.target);
    if (index >= kTargetCount ||
        g_success_reported[index].exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    erui::detail::logf(
        erui::LogLevel::info,
        "Runtime GFX completion applied '%s': bytes=%zu->%zu rows=[game:%u->%u camera:%u->%u display:%u->%u sound:%u->%u network:%u->%u keyboardMouse:%u->%u graphics:%u->%u] tabs=[frontCells:%u->%u backCells:%u->%u authoredCapacity:%u->%u] textInput=%u colorPicker=%u",
        erui::gfx::settings_gfx_runtime_target_name(result.target),
        source_size,
        result.replacement_size,
        static_cast<unsigned>(result.before.game_options_rows),
        static_cast<unsigned>(result.after.game_options_rows),
        static_cast<unsigned>(result.before.camera_options_rows),
        static_cast<unsigned>(result.after.camera_options_rows),
        static_cast<unsigned>(result.before.display_rows),
        static_cast<unsigned>(result.after.display_rows),
        static_cast<unsigned>(result.before.sound_rows),
        static_cast<unsigned>(result.after.sound_rows),
        static_cast<unsigned>(result.before.network_rows),
        static_cast<unsigned>(result.after.network_rows),
        static_cast<unsigned>(result.before.keyboard_mouse_rows),
        static_cast<unsigned>(result.after.keyboard_mouse_rows),
        static_cast<unsigned>(result.before.graphics_rows),
        static_cast<unsigned>(result.after.graphics_rows),
        static_cast<unsigned>(result.before.tab_list_cells),
        static_cast<unsigned>(result.after.tab_list_cells),
        static_cast<unsigned>(result.before.back_tab_list_cells),
        static_cast<unsigned>(result.after.back_tab_list_cells),
        static_cast<unsigned>(result.before.authored_tab_slot_capacity),
        static_cast<unsigned>(result.after.authored_tab_slot_capacity),
        result.after.character_name_text_input ? 1U : 0U,
        result.after.character_creation_color_picker ? 1U : 0U);
}

[[nodiscard]] bool capture_source(
    void* file,
    MemoryFileSnapshot& snapshot,
    std::vector<std::uint8_t>& source,
    NativePatchFailure& failure) noexcept {
    failure = NativePatchFailure::none;
    if (!file) {
        failure = NativePatchFailure::null_file;
        return false;
    }
    if (!read_memory_file_snapshot(file, snapshot)) {
        failure = NativePatchFailure::unreadable_layout;
        return false;
    }
    if (snapshot.vtable != g_expected_memory_file_vtable) {
        failure = NativePatchFailure::unexpected_file_type;
        return false;
    }
    if (snapshot.refcount == 0 || !snapshot.bytes || snapshot.cursor != 0 ||
        snapshot.valid != 1) {
        failure = NativePatchFailure::invalid_state;
        return false;
    }
    if (snapshot.length < 12 ||
        static_cast<std::size_t>(snapshot.length) >
            erui::gfx::kSettingsGfxRuntimeMaximumBytes) {
        failure = NativePatchFailure::invalid_length;
        return false;
    }
    const std::size_t length = static_cast<std::size_t>(snapshot.length);
    const auto source_address =
        reinterpret_cast<std::uintptr_t>(snapshot.bytes);
    if (source_address >
        (std::numeric_limits<std::uintptr_t>::max)() - length) {
        failure = NativePatchFailure::pointer_overflow;
        return false;
    }
    try {
        source.resize(length);
    } catch (...) {
        failure = NativePatchFailure::allocation_failed;
        return false;
    }
    if (!guarded_copy(source.data(), snapshot.bytes, length)) {
        failure = NativePatchFailure::unreadable_bytes;
        return false;
    }
    MemoryFileSnapshot after{};
    if (!read_memory_file_snapshot(file, after) ||
        !same_snapshot(snapshot, after)) {
        failure = NativePatchFailure::changed_during_copy;
        return false;
    }
    return true;
}

[[nodiscard]] NativePatchFailure publish_replacement(
    void* file,
    const MemoryFileSnapshot& captured,
    const SettingsGfxRuntimeResult& result) noexcept {
    MemoryFileSnapshot before{};
    if (!read_memory_file_snapshot(file, before)) {
        return NativePatchFailure::unreadable_layout;
    }

    // If another concurrent opener already published the same immutable
    // output into this object, the requested postcondition is satisfied.
    if (before.vtable == captured.vtable &&
        before.refcount == captured.refcount &&
        before.bytes == result.replacement_bytes &&
        before.length == static_cast<std::int32_t>(result.replacement_size) &&
        before.cursor == 0 && before.valid == captured.valid) {
        return NativePatchFailure::none;
    }
    if (!same_snapshot(captured, before)) {
        return NativePatchFailure::changed_during_copy;
    }
    if (result.replacement_size == 0 ||
        result.replacement_size >
            static_cast<std::size_t>(
                (std::numeric_limits<std::int32_t>::max)())) {
        return NativePatchFailure::invalid_length;
    }

    void* const destination = memory_file_view_address(file);
    if (!destination ||
        !writable_region(destination, sizeof(MemoryFileView))) {
        return NativePatchFailure::destination_not_writable;
    }
    const MemoryFileView original{
        before.bytes, before.length, before.cursor};
    const MemoryFileView replacement{
        result.replacement_bytes,
        static_cast<std::int32_t>(result.replacement_size),
        0};
    if (!guarded_copy(destination, &replacement, sizeof(replacement))) {
        return restore_memory_file_view(file, before, original)
            ? NativePatchFailure::write_fault
            : NativePatchFailure::restoration_failed;
    }

    MemoryFileSnapshot after{};
    const bool verified = read_memory_file_snapshot(file, after) &&
        after.vtable == before.vtable &&
        after.refcount == before.refcount &&
        after.bytes == result.replacement_bytes &&
        after.length == static_cast<std::int32_t>(result.replacement_size) &&
        after.cursor == 0 && after.valid == before.valid;
    if (!verified) {
        return restore_memory_file_view(file, before, original)
            ? NativePatchFailure::verification_failed
            : NativePatchFailure::restoration_failed;
    }
    return NativePatchFailure::none;
}

void* __fastcall open_file_detour(
    void* file_opener,
    const char* path,
    std::int32_t flags,
    std::int32_t mode) noexcept {
    std::array<char, kMaximumPathLength + 1> copied_path{};
    bool path_truncated{};
    const bool path_read =
        guarded_copy_path(path, copied_path, path_truncated);

    OpenFileFn const original =
        g_original_open_file.load(std::memory_order_acquire);
    void* const file = original
        ? original(file_opener, path, flags, mode)
        : nullptr;

    if (!path_read || path_truncated) return file;
    const SettingsGfxRuntimeTarget target =
        erui::gfx::classify_settings_gfx_runtime_path(copied_path.data());
    if (target == SettingsGfxRuntimeTarget::none) return file;

    SettingsGfxRuntimeCache* const cache =
        g_runtime_cache.load(std::memory_order_acquire);
    if (!cache) return file;

    MemoryFileSnapshot snapshot{};
    std::vector<std::uint8_t> source{};
    NativePatchFailure native_failure{};
    if (!capture_source(file, snapshot, source, native_failure)) {
        report_native_failure_once(target, native_failure);
        return file;
    }

    const SettingsGfxRuntimeResult prepared = cache->prepare(
        target,
        std::span<const std::uint8_t>(source.data(), source.size()));
    if (prepared.outcome == SettingsGfxRuntimeOutcome::failed) {
        report_transform_failure_once(prepared);
        return file;
    }
    if (prepared.outcome == SettingsGfxRuntimeOutcome::unchanged) {
        report_unchanged_once(prepared, source.size());
        return file;
    }
    if (!prepared.has_replacement()) {
        report_native_failure_once(target, NativePatchFailure::invalid_state);
        return file;
    }

    native_failure = publish_replacement(file, snapshot, prepared);
    if (native_failure != NativePatchFailure::none) {
        report_native_failure_once(target, native_failure);
        return file;
    }
    report_success_once(prepared, source.size());
    return file;
}

[[nodiscard]] bool validate_interface(
    const erui::native::ModuleView& game) noexcept {
    std::uint8_t* const entry = game.at_rva(kFileOpener271Rva);
    if (!entry || !game.contains(entry, kFileOpener271Entry.size())) {
        return false;
    }
    std::array<std::uint8_t, kFileOpener271Entry.size()> actual_entry{};
    if (!guarded_copy(actual_entry.data(), entry, actual_entry.size()) ||
        actual_entry != kFileOpener271Entry) {
        return false;
    }

    std::uint8_t* const opener_vtable =
        game.at_rva(kFileOpenerVtable271Rva);
    void* virtual_open_file{};
    if (!opener_vtable ||
        !game.contains(opener_vtable, 2U * sizeof(void*)) ||
        !guarded_copy(
            &virtual_open_file,
            opener_vtable + sizeof(void*),
            sizeof(virtual_open_file)) ||
        virtual_open_file != entry) {
        return false;
    }

    std::uint8_t* const memory_file_vtable =
        game.at_rva(kMemoryFileVtable271Rva);
    if (!memory_file_vtable ||
        !game.contains(memory_file_vtable,
            kMemoryFile271VslotRvas.size() * sizeof(void*))) {
        return false;
    }
    std::array<void*, kMemoryFile271VslotRvas.size()> slots{};
    if (!guarded_copy(slots.data(), memory_file_vtable, sizeof(slots))) {
        return false;
    }
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (slots[index] != game.at_rva(kMemoryFile271VslotRvas[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace erui::native {

NativeGfxLoaderInstallStatus install_native_gfx_loader(
    const ModuleView& game,
    std::uint16_t minimum_tab_slots) noexcept {
    try {
        std::lock_guard lock(g_install_mutex);
        if (g_installed.load(std::memory_order_acquire)) {
            return NativeGfxLoaderInstallStatus::already_installed;
        }
        if (identify_elden_ring_build(game.timestamp(), game.image_size()) !=
            EldenRingBuild::v2_7_1_0) {
            return NativeGfxLoaderInstallStatus::unsupported_game;
        }
        if (!validate_interface(game)) {
            return NativeGfxLoaderInstallStatus::interface_mismatch;
        }

        auto* const cache = new (std::nothrow)
            SettingsGfxRuntimeCache{minimum_tab_slots};
        if (!cache || !cache->ready()) {
            delete cache;
            return NativeGfxLoaderInstallStatus::allocation_failed;
        }

        g_expected_memory_file_vtable =
            game.at_rva(kMemoryFileVtable271Rva);
        // Deliberately process-lifetime. Once OpenFile can publish a
        // replacement pointer, neither the cache nor the hook may be reclaimed
        // independently.
        g_runtime_cache.store(cache, std::memory_order_release);

        auto hook = SafetyHookInline::create(
            game.at_rva(kFileOpener271Rva),
            reinterpret_cast<void*>(&open_file_detour),
            SafetyHookInline::StartDisabled);
        if (!hook) {
            detail::logf(
                LogLevel::warning,
                "Runtime GFX completion hook could not be prepared (SafetyHook error type %u)",
                static_cast<unsigned>(hook.error().type));
            return NativeGfxLoaderInstallStatus::hook_failed;
        }
        g_open_file_hook = std::move(*hook);
        g_original_open_file.store(
            g_open_file_hook.original<OpenFileFn>(),
            std::memory_order_release);
        const auto enabled = g_open_file_hook.enable();
        if (!enabled) {
            detail::logf(
                LogLevel::warning,
                "Runtime GFX completion hook could not be enabled (SafetyHook error type %u)",
                static_cast<unsigned>(enabled.error().type));
            g_original_open_file.store(nullptr, std::memory_order_release);
            g_open_file_hook.reset();
            return NativeGfxLoaderInstallStatus::hook_failed;
        }
        g_installed.store(true, std::memory_order_release);
        detail::logf(
            LogLevel::info,
            "Runtime GFX completion hook installed: Elden Ring 2.7.1.0 CSScaleformFileOpener::OpenFile RVA=0x%llX minimumTabSlots=%u",
            static_cast<unsigned long long>(kFileOpener271Rva),
            static_cast<unsigned>(minimum_tab_slots));
        return NativeGfxLoaderInstallStatus::installed;
    } catch (...) {
        // SafetyHook normally reports failures through its result type, but
        // installation is noexcept and must also leave no half-published hook
        // behind if the library raises unexpectedly while enabling it. Keep
        // the original callable published until reset has had a chance to
        // disable the detour, then clear the callable unconditionally.
        try {
            g_open_file_hook.reset();
        } catch (...) {
        }
        g_original_open_file.store(nullptr, std::memory_order_release);
        g_installed.store(false, std::memory_order_release);
        detail::logf(
            LogLevel::warning,
            "Runtime GFX completion hook installation raised an unexpected exception");
        return NativeGfxLoaderInstallStatus::hook_failed;
    }
}

const char* native_gfx_loader_install_status_name(
    NativeGfxLoaderInstallStatus status) noexcept {
    switch (status) {
    case NativeGfxLoaderInstallStatus::installed: return "installed";
    case NativeGfxLoaderInstallStatus::already_installed:
        return "already_installed";
    case NativeGfxLoaderInstallStatus::unsupported_game:
        return "unsupported_game";
    case NativeGfxLoaderInstallStatus::interface_mismatch:
        return "interface_mismatch";
    case NativeGfxLoaderInstallStatus::allocation_failed:
        return "allocation_failed";
    case NativeGfxLoaderInstallStatus::hook_failed: return "hook_failed";
    }
    return "unknown";
}

} // namespace erui::native
