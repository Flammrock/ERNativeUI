#include "host_registry.hpp"
#include "host_locale.hpp"
#include "addresses.hpp"
#include "hooks.hpp"
#include "localization_config.hpp"
#include "logger.hpp"
#include "module.hpp"
#include "native_dialog.hpp"
#include "native_gfx_loader.hpp"
#include "native_input_bindings.hpp"
#include "native_row_presentation.hpp"
#include "native_row_visibility.hpp"
#include "native_tabs.hpp"
#include "color_picker.hpp"
#include "runtime_log.hpp"
#include "steam_dlc.hpp"
#include "steam_language.hpp"
#include "tab_configuration.hpp"

#include "runtime.hpp"
#include "resource.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace erui::host {
namespace {

HMODULE g_module{};
std::atomic_bool g_stop{};
std::unique_ptr<erui::Menu> g_menu{};
erui::native::PreservedSharedMenuAddresses
    g_attach_shared_menu_snapshot{};
bool g_attach_shared_menu_snapshot_ready{};
bool g_attach_input_snapshot_ready{};
bool g_attach_color_snapshot_ready{};
bool g_attach_solid_uncapper_present{};

constexpr DWORD kSteamReadinessPollMs = 25;
constexpr std::uint64_t kSteamReadinessLogMs = 2000;
constexpr std::uint64_t kTitleCataloguePublicationGraceMs = 2000;
// ModEngine maps external DLLs serially, but each DLL may start its own
// worker before LoadLibrary returns and the next configured DLL is mapped.
// Give a later-listed Solid Uncapper a short bounded discovery window before
// releasing API 1.1+ clients. The wait ends immediately when the module is
// visible, so the ordinary compatibility path does not pay the full grace.
constexpr DWORD kSolidUncapperModuleDiscoveryGraceMs = 250;
constexpr DWORD kSolidUncapperCompatibilityTimeoutMs = 20000;
constexpr DWORD kSolidUncapperStableWindowMs = 500;
constexpr std::uint64_t kSolidUncapperCompatibilityWaitBudgetMs =
    static_cast<std::uint64_t>(kSolidUncapperCompatibilityTimeoutMs) +
    kSolidUncapperStableWindowMs;

class TitleCataloguePublicationGuard final {
public:
    void arm() noexcept { armed_ = true; }
    void settle() noexcept { armed_ = false; }

    ~TitleCataloguePublicationGuard() noexcept {
        if (armed_) {
            erui::native::abandon_native_title_screen_texts();
        }
    }

private:
    bool armed_{};
};

class UniqueWin32Handle final {
public:
    explicit UniqueWin32Handle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept
        : handle_(handle) {}

    ~UniqueWin32Handle() noexcept {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }

    UniqueWin32Handle(const UniqueWin32Handle&) = delete;
    UniqueWin32Handle& operator=(const UniqueWin32Handle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

void fail_pending_startup_for_stop() noexcept {
    set_api_state(ApiState::failed);
    if (steam_language_readiness() == SteamLanguageReadiness::pending) {
        SteamLanguageProbe unavailable{};
        unavailable.failure_code = SteamLanguageFailure::exception;
        unavailable.failure =
            "host stopped before localization language selection settled";
        (void)settle_cached_steam_language(std::move(unavailable));
    }
}

HMODULE wait_for_solid_uncapper_module() noexcept {
    const std::uint64_t started = GetTickCount64();
    for (;;) {
        if (HMODULE const module =
                GetModuleHandleW(L"Solid Uncapper.dll")) {
            return module;
        }
        if (g_stop.load(std::memory_order_acquire)) return nullptr;

        const std::uint64_t elapsed = GetTickCount64() - started;
        if (elapsed >= kSolidUncapperModuleDiscoveryGraceMs) return nullptr;
        const auto remaining = static_cast<DWORD>(
            kSolidUncapperModuleDiscoveryGraceMs - elapsed);
        Sleep((std::min)(remaining, kSteamReadinessPollMs));
    }
}

std::filesystem::path module_directory(HMODULE module) {
    wchar_t buffer[32768]{};
    const DWORD length = GetModuleFileNameW(module, buffer, _countof(buffer));
    if (length && length < _countof(buffer)) {
        return std::filesystem::path(buffer).parent_path();
    }
    std::error_code error{};
    const auto current = std::filesystem::current_path(error);
    return error ? std::filesystem::path(L".") : current;
}

int read_ini(const std::filesystem::path& path, const wchar_t* section,
    const wchar_t* key, int fallback) noexcept {
    return static_cast<int>(GetPrivateProfileIntW(section, key, fallback, path.c_str()));
}

std::wstring read_ini_text(
    const std::filesystem::path& path,
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* fallback) {
    // Language identifiers are limited to 64 characters. A larger buffer
    // ensures an overlong setting remains overlong and is rejected instead of
    // being mistaken for a valid truncated identifier.
    std::array<wchar_t, 256> buffer{};
    const DWORD length = GetPrivateProfileStringW(
        section, key, fallback, buffer.data(),
        static_cast<DWORD>(buffer.size()), path.c_str());
    return std::wstring(buffer.data(), static_cast<std::size_t>(length));
}

bool ensure_default_ini(
    HMODULE module,
    const std::filesystem::path& path) noexcept {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    HRSRC resource = FindResourceW(
        module,
        MAKEINTRESOURCEW(ERUI_DEFAULT_INI_RESOURCE),
        RT_RCDATA);
    if (!resource) return false;
    HGLOBAL loaded = LoadResource(module, resource);
    const DWORD size = SizeofResource(module, resource);
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    if (!bytes || size == 0) return false;

    std::filesystem::path temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
    HANDLE file = CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    DWORD written{};
    const bool wrote = WriteFile(file, bytes, size, &written, nullptr) != FALSE &&
        written == size && FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!wrote) {
        DeleteFileW(temporary.c_str());
        return false;
    }

    if (MoveFileExW(
            temporary.c_str(),
            path.c_str(),
            MOVEFILE_WRITE_THROUGH) != FALSE) {
        return true;
    }

    // Another loader instance may have won the create race. Never overwrite
    // an existing user configuration.
    DeleteFileW(temporary.c_str());
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool read_tab_configuration_file(
    const std::filesystem::path& path,
    std::string& output,
    DWORD& error) noexcept {
    output.clear();
    error = ERROR_SUCCESS;
    UniqueWin32Handle file(CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (!file.valid()) {
        error = GetLastError();
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart < 0) {
        error = GetLastError();
        return false;
    }

    try {
        if (static_cast<unsigned long long>(size.QuadPart) >
            erui::detail::kMaximumTabConfigurationBytes) {
            // The pure parser owns the policy and diagnostic. A bounded
            // sentinel is enough to report file_too_large without allocating
            // attacker-sized input on the host worker.
            output.assign(
                erui::detail::kMaximumTabConfigurationBytes + 1,
                '\0');
            return true;
        }
        output.resize(static_cast<std::size_t>(size.QuadPart));
    } catch (const std::bad_alloc&) {
        error = ERROR_OUTOFMEMORY;
        return false;
    } catch (...) {
        error = ERROR_GEN_FAILURE;
        return false;
    }
    DWORD read{};
    const bool success = output.empty() ||
        (ReadFile(
             file.get(),
             output.data(),
             static_cast<DWORD>(output.size()),
             &read,
             nullptr) != FALSE &&
            read == static_cast<DWORD>(output.size()));
    if (!success) error = GetLastError();
    if (!success) output.clear();
    return success;
}

bool write_utf8_file_atomically(
    const std::filesystem::path& path,
    std::string_view contents,
    DWORD& error) noexcept {
    error = ERROR_SUCCESS;
    try {
        std::filesystem::path temporary = path;
        temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
        bool flushed = false;
        {
            UniqueWin32Handle file(CreateFileW(
                temporary.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));
            if (!file.valid()) {
                error = GetLastError();
                return false;
            }

            if (contents.size() > (std::numeric_limits<DWORD>::max)()) {
                error = ERROR_FILE_TOO_LARGE;
            } else {
                DWORD written{};
                bool wrote = contents.empty();
                if (!contents.empty()) {
                    if (WriteFile(
                            file.get(),
                            contents.data(),
                            static_cast<DWORD>(contents.size()),
                            &written,
                            nullptr) == FALSE) {
                        error = GetLastError();
                    } else if (written !=
                        static_cast<DWORD>(contents.size())) {
                        error = ERROR_WRITE_FAULT;
                    } else {
                        wrote = true;
                    }
                }
                if (!wrote) {
                    if (error == ERROR_SUCCESS) error = ERROR_WRITE_FAULT;
                } else {
                    flushed = FlushFileBuffers(file.get()) != FALSE;
                    if (!flushed) error = GetLastError();
                }
            }
        }
        if (!flushed) {
            DeleteFileW(temporary.c_str());
            return false;
        }

        if (MoveFileExW(
                temporary.c_str(),
                path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
            error = GetLastError();
            DeleteFileW(temporary.c_str());
            return false;
        }
        return true;
    } catch (const std::bad_alloc&) {
        error = ERROR_OUTOFMEMORY;
        return false;
    } catch (...) {
        error = ERROR_GEN_FAILURE;
        return false;
    }
}

const void* absolute_indirect_jump_target(const unsigned char* entry) noexcept {
    if (!entry || entry[0] != 0xFF || entry[1] != 0x25) return nullptr;
    std::int32_t displacement{};
    std::memcpy(&displacement, entry + 2, sizeof(displacement));

    const std::uintptr_t instruction_end =
        reinterpret_cast<std::uintptr_t>(entry) + 6u;
    std::uintptr_t slot_value{};
    if (displacement >= 0) {
        const auto distance = static_cast<std::uintptr_t>(displacement);
        if (instruction_end >
            (std::numeric_limits<std::uintptr_t>::max)() - distance) {
            return nullptr;
        }
        slot_value = instruction_end + distance;
    } else {
        const auto distance = static_cast<std::uintptr_t>(
            -static_cast<std::int64_t>(displacement));
        if (instruction_end < distance) return nullptr;
        slot_value = instruction_end - distance;
    }
    const auto* const slot = reinterpret_cast<const void*>(slot_value);

    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(slot, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return nullptr;
    }
    const auto region_end = reinterpret_cast<std::uintptr_t>(
        memory.BaseAddress) + memory.RegionSize;
    if (slot_value > region_end ||
        sizeof(void*) > region_end - slot_value) {
        return nullptr;
    }

    const void* target{};
#if defined(_MSC_VER)
    __try {
#endif
    std::memcpy(&target, slot, sizeof(target));
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
#endif
    return target;
}

void runtime_log_sink(erui::LogLevel level, const char* message) noexcept {
    log::write(level, "%s", message ? message : "");
}

enum class SolidCompatibilityStatus {
    ready,
    stopped,
    failed,
};

SolidCompatibilityStatus settle_solid_uncapper_compatibility(
    HMODULE solid_uncapper_module,
    erui::native::PreservedSharedMenuAddresses& preserved,
    bool slider_snapshot_ready) noexcept {
    if (!solid_uncapper_module || !preserved.required_complete()) {
        return SolidCompatibilityStatus::failed;
    }

    erui::native::ModuleView solid_uncapper{};
    if (!solid_uncapper.initialize(solid_uncapper_module)) {
        log::write(
            erui::LogLevel::fatal,
            "Could not inspect Solid Uncapper image for hook ownership");
        return SolidCompatibilityStatus::failed;
    }

    // DLL load order serializes mapping, not worker-thread initialization.
    // Wait until all three row-related entry points have changed and their
    // complete captured prefixes have remained byte-for-byte stable, then
    // accept only FF 25 detours into executable Solid-owned image memory.
    // Selector::select is tracked independently: some Solid versions patch
    // it for combo repaint while others leave it pristine, so it is not part
    // of the row entries' all-or-none rule. Its final state must nevertheless
    // have its own complete stable observation before it can be approved.
    const std::array<const unsigned char*, 3> targets{
        reinterpret_cast<const unsigned char*>(
            preserved.game_options_handler),
        reinterpret_cast<const unsigned char*>(preserved.sub_handler),
        reinterpret_cast<const unsigned char*>(preserved.text_resolver)};
    const std::array<const unsigned char*, 3> pristine_prefixes{
        preserved.game_options_pristine_prefix.data(),
        preserved.sub_pristine_prefix.data(),
        preserved.text_pristine_prefix.data()};
    std::array<std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>, 3>
        last_observed{};
    const auto* const selector_select_entry =
        static_cast<const unsigned char*>(preserved.selector_select);
    if (!selector_select_entry) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper compatibility: Selector::select was not captured at process attach");
        return SolidCompatibilityStatus::failed;
    }
    std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
        last_selector_observed{};
    bool have_stable_observation = false;
    bool converged = false;
    std::uint64_t stable_since = 0;
    bool have_selector_observation = false;
    bool selector_observation_stable = false;
    std::uint64_t selector_stable_since = 0;

    log::write(
        "Solid Uncapper compatibility: waiting up to %lu ms for shared menu hooks",
        static_cast<unsigned long>(
            kSolidUncapperCompatibilityWaitBudgetMs));
    const std::uint64_t wait_started = GetTickCount64();
    while (!g_stop.load(std::memory_order_acquire) &&
           GetTickCount64() - wait_started <
               kSolidUncapperCompatibilityWaitBudgetMs) {
        std::array<std::array<unsigned char,
            erui::native::PreservedSharedMenuAddresses::entry_prefix_size>, 3>
            observed{};
        bool all_changed = true;
        for (std::size_t index = 0; index < targets.size(); ++index) {
            std::memcpy(
                observed[index].data(),
                targets[index],
                observed[index].size());
            if (std::memcmp(
                    observed[index].data(),
                    pristine_prefixes[index],
                    observed[index].size()) == 0) {
                all_changed = false;
            }
        }

        const std::uint64_t now = GetTickCount64();
        std::array<unsigned char,
            erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
            selector_observed{};
        std::memcpy(
            selector_observed.data(),
            selector_select_entry,
            selector_observed.size());
        if (!have_selector_observation ||
            selector_observed != last_selector_observed) {
            last_selector_observed = selector_observed;
            have_selector_observation = true;
            selector_observation_stable = false;
            selector_stable_since = now;
        } else if (now - selector_stable_since >=
                   kSolidUncapperStableWindowMs) {
            selector_observation_stable = true;
        }

        if (!all_changed) {
            have_stable_observation = false;
            converged = false;
            stable_since = 0;
        } else if (!have_stable_observation ||
                   observed != last_observed) {
            last_observed = observed;
            have_stable_observation = true;
            converged = false;
            stable_since = now;
        } else if (now - stable_since >= kSolidUncapperStableWindowMs) {
            converged = true;
        }
        if (converged && selector_observation_stable) {
            break;
        }
        Sleep(50);
    }
    if (g_stop.load(std::memory_order_acquire)) {
        return SolidCompatibilityStatus::stopped;
    }

    std::size_t changed_count = 0;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (std::memcmp(
                pristine_prefixes[index],
                targets[index],
                erui::native::PreservedSharedMenuAddresses::
                    entry_prefix_size) != 0) {
            ++changed_count;
        }
    }
    log::write(
        "Solid Uncapper compatibility: shared hooks=%zu/3 elapsed=%llu ms",
        changed_count,
        static_cast<unsigned long long>(GetTickCount64() - wait_started));

    if (changed_count != 0 && changed_count != targets.size()) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper installed only %zu/3 shared hooks; refusing a partial chain",
            changed_count);
        return SolidCompatibilityStatus::failed;
    }

    if (changed_count == targets.size() && !converged) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper installed all shared hooks, but their entry bytes did not remain stable for %lu ms before the readiness deadline",
            static_cast<unsigned long>(kSolidUncapperStableWindowMs));
        return SolidCompatibilityStatus::failed;
    }

    if (!selector_observation_stable) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper Selector::select entry did not remain stable for %lu ms before the readiness deadline",
            static_cast<unsigned long>(kSolidUncapperStableWindowMs));
        return SolidCompatibilityStatus::failed;
    }

    if (changed_count == 0) {
        log::write(
            "Solid Uncapper compatibility: no shared menu hooks appeared before the readiness deadline; continuing through the canonical game entries");
    }

    std::array<std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>, 3>
        approved_prefixes{};
    std::array<const void*, 3> approved_detour_targets{};
    for (std::size_t index = 0; index < targets.size(); ++index) {
        std::memcpy(
            approved_prefixes[index].data(),
            targets[index],
            approved_prefixes[index].size());
        if (changed_count == targets.size() &&
            approved_prefixes[index] != last_observed[index]) {
            log::write(
                erui::LogLevel::fatal,
                "Shared menu hook %zu changed after the stable readiness observation",
                index);
            return SolidCompatibilityStatus::failed;
        }
        if (changed_count == 0 &&
            std::memcmp(
                approved_prefixes[index].data(),
                pristine_prefixes[index],
                approved_prefixes[index].size()) != 0) {
            log::write(
                erui::LogLevel::fatal,
                "Shared menu entry %zu changed after the pristine fallback decision",
                index);
            return SolidCompatibilityStatus::failed;
        }
    }

    if (changed_count == targets.size()) {
        for (std::size_t index = 0; index < targets.size(); ++index) {
            const void* destination =
                absolute_indirect_jump_target(targets[index]);
            if (!destination || !solid_uncapper.contains(destination) ||
                !solid_uncapper.is_executable(destination)) {
                log::write(
                    erui::LogLevel::fatal,
                    "Shared menu hook %zu has an unsupported or foreign detour",
                    index);
                return SolidCompatibilityStatus::failed;
            }
            approved_detour_targets[index] = destination;
            if (std::memcmp(
                    approved_prefixes[index].data(),
                    targets[index],
                    approved_prefixes[index].size()) != 0) {
                log::write(
                    erui::LogLevel::fatal,
                    "Shared menu hook %zu changed while ownership was being validated",
                    index);
                return SolidCompatibilityStatus::failed;
            }
        }
        log::write(
            "Solid Uncapper compatibility: three owned detours validated; installing chained hooks");
    }
    preserved.game_options_approved_prefix = approved_prefixes[0];
    preserved.sub_approved_prefix = approved_prefixes[1];
    preserved.text_approved_prefix = approved_prefixes[2];
    preserved.approved_detour_targets = approved_detour_targets;
    preserved.required_entries_approved = true;

    std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
        approved_selector_prefix{};
    std::memcpy(
        approved_selector_prefix.data(),
        selector_select_entry,
        approved_selector_prefix.size());
    if (approved_selector_prefix != last_selector_observed) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper Selector::select entry changed after its stable readiness observation");
        return SolidCompatibilityStatus::failed;
    }

    const bool selector_changed = approved_selector_prefix !=
        preserved.selector_select_pristine_prefix;
    const void* approved_selector_target{};
    if (selector_changed) {
        approved_selector_target =
            absolute_indirect_jump_target(selector_select_entry);
        if (!approved_selector_target ||
            !solid_uncapper.contains(approved_selector_target) ||
            !solid_uncapper.is_executable(approved_selector_target)) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper Selector::select entry has an unsupported or foreign detour");
            return SolidCompatibilityStatus::failed;
        }
        if (std::memcmp(
                approved_selector_prefix.data(),
                selector_select_entry,
                approved_selector_prefix.size()) != 0) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper Selector::select entry changed while ownership was being validated");
            return SolidCompatibilityStatus::failed;
        }
        const void* const confirmed_selector_target =
            absolute_indirect_jump_target(selector_select_entry);
        if (confirmed_selector_target != approved_selector_target) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper Selector::select detour target changed while ownership was being validated");
            return SolidCompatibilityStatus::failed;
        }
        log::write(
            "Solid Uncapper compatibility: Selector::select chain approved");
    } else {
        log::write(
            "Solid Uncapper compatibility: Selector::select remained pristine");
    }
    preserved.selector_select_approved_prefix = approved_selector_prefix;
    preserved.selector_select_approved_detour_target =
        approved_selector_target;
    preserved.selector_select_entry_approved = true;

    // Solid Uncapper may also own SliderCtrl's refresh entry for its
    // multiplier/seconds presentation. Preserve formatted-slider support only
    // for an unchanged pristine entry or a validated Solid-owned FF 25 detour.
    if (slider_snapshot_ready && preserved.slider_refresh) {
        const auto* const slider_entry =
            reinterpret_cast<const unsigned char*>(preserved.slider_refresh);
        std::array<unsigned char,
            erui::native::PreservedSharedMenuAddresses::entry_prefix_size>
            approved_slider_prefix{};
        std::memcpy(
            approved_slider_prefix.data(),
            slider_entry,
            approved_slider_prefix.size());
        const bool changed = std::memcmp(
            preserved.slider_pristine_prefix.data(),
            approved_slider_prefix.data(),
            preserved.slider_pristine_prefix.size()) != 0;
        if (changed) {
            const void* const destination =
                absolute_indirect_jump_target(slider_entry);
            if (!destination || !solid_uncapper.contains(destination) ||
                !solid_uncapper.is_executable(destination)) {
                log::write(
                    erui::LogLevel::warning,
                    "Solid Uncapper formatted-slider chain was not approved; API 1.2 formatted sliders will be unavailable if requested");
                preserved.slider_refresh = nullptr;
                preserved.slider_pristine_prefix = {};
                preserved.slider_approved_detour_target = nullptr;
            } else {
                if (std::memcmp(
                        approved_slider_prefix.data(),
                        slider_entry,
                        approved_slider_prefix.size()) != 0) {
                    log::write(
                        erui::LogLevel::warning,
                        "Solid Uncapper formatted-slider entry changed while ownership was being validated; formatted sliders will be unavailable");
                    preserved.slider_refresh = nullptr;
                    preserved.slider_pristine_prefix = {};
                    preserved.slider_approved_detour_target = nullptr;
                }
                if (preserved.slider_refresh) {
                    preserved.slider_approved_detour_target = destination;
                    log::write(
                        "Solid Uncapper compatibility: formatted-slider chain approved");
                }
            }
        } else {
            log::write(
                "Solid Uncapper compatibility: SliderCtrl refresh remained pristine");
        }
        if (preserved.slider_refresh) {
            preserved.slider_approved_prefix = approved_slider_prefix;
            preserved.slider_entry_approved = true;
        }
    } else {
        preserved.slider_refresh = nullptr;
        preserved.slider_pristine_prefix = {};
        preserved.slider_approved_detour_target = nullptr;
        log::write(
            erui::LogLevel::warning,
            "SliderCtrl refresh was not captured early; API 1.2 formatted sliders will be unavailable with Solid Uncapper");
    }

    if (!erui::native::approve_captured_native_input_bindings(
            solid_uncapper_module)) {
        log::write(
            erui::LogLevel::warning,
            "Solid Uncapper input-binding chain was not approved; installation can continue only when no client registered custom bindings");
    } else {
        log::write(
            "Solid Uncapper compatibility: input-binding chain approved");
    }
    if (!erui::native::approve_captured_color_picker_visibility(
            solid_uncapper_module)) {
        log::write(
            erui::LogLevel::warning,
            "Solid Uncapper Color Picker chain was not approved; installation can continue only when no client registered ColorPicker rows");
    } else {
        log::write(
            "Solid Uncapper compatibility: Color Picker chain approved");
    }
    return SolidCompatibilityStatus::ready;
}

bool revalidate_solid_uncapper_shared_entries(
    HMODULE solid_uncapper_module,
    erui::native::PreservedSharedMenuAddresses& preserved) noexcept {
    erui::native::ModuleView solid_uncapper{};
    if (!solid_uncapper.initialize(solid_uncapper_module) ||
        !preserved.required_entries_approved) {
        return false;
    }

    const std::array<const unsigned char*, 3> entries{
        reinterpret_cast<const unsigned char*>(
            preserved.game_options_handler),
        reinterpret_cast<const unsigned char*>(preserved.sub_handler),
        reinterpret_cast<const unsigned char*>(preserved.text_resolver)};
    const std::array<const std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>*, 3>
        pristine{{
            &preserved.game_options_pristine_prefix,
            &preserved.sub_pristine_prefix,
            &preserved.text_pristine_prefix,
        }};
    const std::array<const std::array<unsigned char,
        erui::native::PreservedSharedMenuAddresses::entry_prefix_size>*, 3>
        approved{{
            &preserved.game_options_approved_prefix,
            &preserved.sub_approved_prefix,
            &preserved.text_approved_prefix,
        }};
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (std::memcmp(
                entries[index],
                approved[index]->data(),
                approved[index]->size()) != 0) {
            log::write(
                erui::LogLevel::fatal,
                "Shared menu hook %zu changed after compatibility approval",
                index);
            return false;
        }
        if (*approved[index] != *pristine[index]) {
            const void* const destination =
                absolute_indirect_jump_target(entries[index]);
            if (!destination ||
                destination != preserved.approved_detour_targets[index] ||
                !solid_uncapper.contains(destination) ||
                !solid_uncapper.is_executable(destination)) {
                log::write(
                    erui::LogLevel::fatal,
                    "Shared menu hook %zu no longer resolves to approved Solid Uncapper code",
                    index);
                return false;
            }
        }
    }

    if (!preserved.selector_select ||
        !preserved.selector_select_entry_approved) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper Selector::select chain has no approved process-attach snapshot");
        return false;
    }
    const auto* const selector_entry =
        static_cast<const unsigned char*>(preserved.selector_select);
    if (std::memcmp(
            selector_entry,
            preserved.selector_select_approved_prefix.data(),
            preserved.selector_select_approved_prefix.size()) != 0) {
        log::write(
            erui::LogLevel::fatal,
            "Solid Uncapper Selector::select entry changed after compatibility approval");
        return false;
    }
    if (preserved.selector_select_approved_prefix !=
        preserved.selector_select_pristine_prefix) {
        const void* const destination =
            absolute_indirect_jump_target(selector_entry);
        if (!destination ||
            destination !=
                preserved.selector_select_approved_detour_target ||
            !solid_uncapper.contains(destination) ||
            !solid_uncapper.is_executable(destination)) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper Selector::select entry no longer resolves to approved Solid Uncapper code");
            return false;
        }
    }

    if (preserved.slider_refresh) {
        const auto* const entry = reinterpret_cast<const unsigned char*>(
            preserved.slider_refresh);
        bool slider_valid = preserved.slider_entry_approved &&
            std::memcmp(
                entry,
                preserved.slider_approved_prefix.data(),
                preserved.slider_approved_prefix.size()) == 0;
        if (slider_valid &&
            preserved.slider_approved_prefix !=
                preserved.slider_pristine_prefix) {
            const void* const destination =
                absolute_indirect_jump_target(entry);
            slider_valid = destination &&
                destination == preserved.slider_approved_detour_target &&
                solid_uncapper.contains(destination) &&
                solid_uncapper.is_executable(destination);
        }
        if (!slider_valid) {
            preserved.slider_refresh = nullptr;
            preserved.slider_pristine_prefix = {};
            preserved.slider_approved_prefix = {};
            preserved.slider_entry_approved = false;
            log::write(
                erui::LogLevel::warning,
                "Formatted-slider shared entry changed after compatibility approval; disabling formatted-slider chaining");
        }
    }
    return true;
}

DWORD run_host() noexcept {
    try {
        // The compatibility snapshots were taken synchronously during DLL
        // attachment. Copy their status before doing filesystem, INI, or log
        // work. A failed optional snapshot gets one immediate worker fallback;
        // a successful attach snapshot is never reset or overwritten.
        erui::native::PreservedSharedMenuAddresses
            preserved_shared_addresses =
                g_attach_shared_menu_snapshot;
        const bool shared_snapshot_ready =
            g_attach_shared_menu_snapshot_ready;
        bool slider_snapshot_ready = shared_snapshot_ready &&
            preserved_shared_addresses.slider_refresh != nullptr;
        bool input_snapshot_ready = g_attach_input_snapshot_ready;
        bool color_snapshot_ready = g_attach_color_snapshot_ready;
        erui::native::ModuleView compatibility_game{};
        if (!color_snapshot_ready &&
            compatibility_game.initialize(GetModuleHandleW(nullptr))) {
            color_snapshot_ready = erui::native::
                capture_color_picker_visibility_before_third_party_hooks(
                    compatibility_game, true);
        }
        if (!input_snapshot_ready) {
            input_snapshot_ready = erui::native::
                capture_native_input_bindings_before_third_party_hooks(true);
        }
        const bool solid_uncapper_present_at_snapshot =
            g_attach_solid_uncapper_present;

        const auto directory = module_directory(g_module);
        const auto ini = directory / L"ERNativeUI.ini";
        if (!ensure_default_ini(g_module, ini)) {
            OutputDebugStringA(
                "ERNativeUI: failed to create the default ERNativeUI.ini\n");
        }
        const bool logging_enabled = read_ini(
            ini, L"Logging", L"EnableLog", 0) != 0;
        const bool diagnostics = read_ini(
            ini, L"Diagnostics", L"EnableDiagnostics", 0) != 0;
        const bool runtime_gfx_patching_enabled = read_ini(
            ini, L"GFX", L"EnableRuntimePatching", 1) != 0;
        const bool logging_ready = logging_enabled &&
            log::initialize(directory / L"ERNativeUI.log");
        log::set_diagnostics_enabled(diagnostics);
        erui::detail::set_diagnostic_logging(diagnostics);
        erui::detail::set_log_sink(
            logging_ready ? &runtime_log_sink : nullptr);
        log::write("============================================================");
        log::write("ERNativeUI host v%s starting", ER_NATIVE_UI_BUILD_VERSION);
        log::write("Offline / EAC-disabled use only; hot unloading is unsupported");

        std::string tab_configuration_bytes{};
        DWORD tab_configuration_error{};
        erui::detail::TabConfiguration tab_configuration{};
        if (read_tab_configuration_file(
                ini,
                tab_configuration_bytes,
                tab_configuration_error)) {
            tab_configuration = erui::detail::parse_tab_configuration(
                tab_configuration_bytes);
        } else {
            log::write(
                erui::LogLevel::warning,
                "Could not read exact-case tab configuration from ERNativeUI.ini (Win32 error=%lu); using tab defaults",
                static_cast<unsigned long>(tab_configuration_error));
            tab_configuration = erui::detail::parse_tab_configuration({});
        }
        const auto tab_warning_count = static_cast<std::size_t>(std::count_if(
            tab_configuration.diagnostics.begin(),
            tab_configuration.diagnostics.end(),
            [](const erui::detail::TabConfigurationDiagnostic& diagnostic) {
                return diagnostic.severity ==
                    erui::detail::TabConfigurationSeverity::warning;
            }));
        const auto tab_error_count = static_cast<std::size_t>(std::count_if(
            tab_configuration.diagnostics.begin(),
            tab_configuration.diagnostics.end(),
            [](const erui::detail::TabConfigurationDiagnostic& diagnostic) {
                return diagnostic.severity ==
                    erui::detail::TabConfigurationSeverity::error;
            }));
        log::write(
            "Tab configuration: minimumSlots=%u margin=%u mouseDwell=%u delays=%llu/%llu decay=%llu minimum=%llu ms overrides=%zu diagnostics=%zu warning/%zu error",
            static_cast<unsigned>(
                tab_configuration.settings.minimum_tab_slots),
            static_cast<unsigned>(
                tab_configuration.settings.navigation_margin),
            tab_configuration.settings.mouse_dwell_enabled ? 1U : 0U,
            static_cast<unsigned long long>(
                tab_configuration.settings.mouse_dwell_timing
                    .initial_delay_ms),
            static_cast<unsigned long long>(
                tab_configuration.settings.mouse_dwell_timing
                    .repeat_delay_ms),
            static_cast<unsigned long long>(
                tab_configuration.settings.mouse_dwell_timing
                    .repeat_decay_ms),
            static_cast<unsigned long long>(
                tab_configuration.settings.mouse_dwell_timing
                    .minimum_repeat_delay_ms),
            tab_configuration.order_overrides.size(),
            tab_warning_count,
            tab_error_count);
        // Registry ordering and the installed native catalogue must consume
        // the same immutable settings. Keep this local copy for the runtime
        // handoff after registration freezes.
        registry().set_tab_configuration(tab_configuration);

        // The title movie can be opened only once before Scaleform caches its
        // MovieDef. Read every bounded startup phase before installing the
        // hook so its first-open handoff can cover the whole path to immutable
        // catalogue publication.
        const auto quiet_ms = static_cast<std::uint64_t>(std::clamp(
            read_ini(ini, L"Runtime", L"RegistrationQuietMs", 750),
            100, 5000));
        const auto max_wait_ms = static_cast<std::uint64_t>(std::clamp(
            read_ini(ini, L"Runtime", L"RegistrationMaxWaitMs", 5000),
            static_cast<int>(quiet_ms), 15000));
        const auto steam_language_wait_ms = static_cast<std::uint64_t>(
            std::clamp(read_ini(
                ini, L"Runtime", L"SteamLanguageWaitMs", 5000),
                0, 30000));
        // The hook becomes active before language readiness, API 1.3 client
        // registration, registry freeze, and the DLC observation. Its first
        // title-file wait must therefore cover that complete publication
        // lifecycle, not only RegistrationMaxWaitMs. The Solid Uncapper
        // budget lets its asynchronous hooks converge before publication;
        // the grace covers bounded DLC retries, synchronous catalogue
        // conversion, and scheduling.
        const std::uint32_t title_catalogue_wait_ms =
            static_cast<std::uint32_t>((std::min)(
                steam_language_wait_ms + max_wait_ms +
                    kSolidUncapperModuleDiscoveryGraceMs +
                    kSolidUncapperCompatibilityWaitBudgetMs +
                    kTitleCataloguePublicationGraceMs,
                static_cast<std::uint64_t>(erui::native::
                    kNativeTitleCatalogueWaitMaximumMilliseconds)));

        bool title_text_backend_ready = false;
        TitleCataloguePublicationGuard title_catalogue_guard{};
        if (!runtime_gfx_patching_enabled) {
            log::write(
                "Runtime GFX patching disabled by configuration; the "
                "file-opening hook will not be installed and effective GFX "
                "files will remain unchanged by ERNativeUI");
        }
        HMODULE pinned_host{};
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(g_module), &pinned_host)) {
            log::write(
                erui::LogLevel::warning,
                "Host module could not be pinned; do not hot-unload it");
        } else if (runtime_gfx_patching_enabled) {
            erui::native::ModuleView game{};
            if (!game.initialize(GetModuleHandleW(nullptr))) {
                log::write(
                    erui::LogLevel::warning,
                    "Runtime GFX completion is unavailable because the game image could not be inspected");
            } else {
                std::uint16_t gfx_minimum_tab_slots =
                    static_cast<std::uint16_t>(
                        tab_configuration.settings.minimum_tab_slots);
#if defined(ER_NATIVE_UI_GFX_TAB_GROWTH_RESEARCH_HOST)
                // This build isolates Scaleform growth from the native-record
                // experiment. Configuration, the logical catalogue, and the
                // game's active tab count remain at eight; only the movie
                // transformer authors the requested dormant cells.
#if !defined(ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET)
#error "The GFX tab-growth research host requires a tab-slot target"
#endif
                static_assert(
                    ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET == 9 ||
                    ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET == 10);
                gfx_minimum_tab_slots = static_cast<std::uint16_t>(
                    ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET);
                log::write(
                    erui::LogLevel::warning,
                    "RESEARCH GFX tab-growth host active: configuration/native tabs remain at %u; Scaleform minimum forced to %u",
                    static_cast<unsigned>(
                        tab_configuration.settings.minimum_tab_slots),
                    static_cast<unsigned>(gfx_minimum_tab_slots));
#endif
                const erui::native::NativeGfxLoaderInstallStatus gfx_status =
                    erui::native::install_native_gfx_loader(
                        game,
                        gfx_minimum_tab_slots,
                        title_catalogue_wait_ms);
                if (gfx_status !=
                        erui::native::NativeGfxLoaderInstallStatus::installed &&
                    gfx_status != erui::native::
                        NativeGfxLoaderInstallStatus::already_installed) {
                    log::write(
                        erui::LogLevel::warning,
                        "Runtime GFX completion is unavailable (%s); loaded GFX files will remain unchanged",
                        erui::native::native_gfx_loader_install_status_name(
                            gfx_status));
                } else {
                    title_text_backend_ready = true;
                    title_catalogue_guard.arm();
                }
            }
        }

        if (title_text_backend_ready) {
            if (!registry().enable_title_screen_text_backend()) {
                log::write(
                    erui::LogLevel::fatal,
                    "Runtime title-GFX backend was installed but API 1.3 title-screen text could not be published before registration opened");
                set_api_state(ApiState::failed);
                return 1;
            }
            log::write(
                "Runtime title-GFX backend installed; API 1.3 title-screen text is available");
        } else if (!runtime_gfx_patching_enabled) {
            log::write(
                "API 1.3 title-screen text is unavailable because runtime "
                "GFX patching is disabled by configuration");
        } else {
            log::write(
                erui::LogLevel::warning,
                "API 1.3 title-screen text is unavailable because the runtime title-GFX backend could not be installed");
        }

        const erui::native::NativeTabsPreflightStatus tab_preflight =
            erui::native::preflight_native_tabs();
        if (tab_preflight ==
                erui::native::NativeTabsPreflightStatus::available) {
            if (!registry().enable_custom_tabs_backend()) {
                log::write(
                    erui::LogLevel::fatal,
                    "Native custom-tab backend passed preflight but could not be published before registration opened");
                set_api_state(ApiState::failed);
                return 1;
            }
            log::write(
                "Native custom-tab backend preflight passed; API 1.3 custom tabs are available");
        } else if (tab_preflight ==
                erui::native::NativeTabsPreflightStatus::unsupported_game) {
            log::write(
                erui::LogLevel::warning,
                "Native custom-tab backend is unavailable for this game executable; API 1.3 remains negotiable without ERUI_CAP_CUSTOM_TABS");
        } else {
            log::write(
                erui::LogLevel::warning,
                "Native custom-tab backend interface validation failed; API 1.3 remains negotiable without ERUI_CAP_CUSTOM_TABS");
        }

        erui::native::ModuleView row_update_game{};
        if (row_update_game.initialize(GetModuleHandleW(nullptr)) &&
            erui::native::native_row_presentation_build_supported(
                row_update_game) &&
            erui::native::preflight_row_presentation_addresses(
                row_update_game)) {
            if (!registry().enable_updates_backend()) {
                log::write(
                    erui::LogLevel::fatal,
                    "Runtime row-presentation backend passed preflight but API 1.4 updates could not be published before registration opened");
                set_api_state(ApiState::failed);
                return 1;
            }
            log::write(
                "Runtime row-presentation backend preflight passed; API 1.4 label/help updates are available");
            if (!registry().enable_submenu_updates_backend()) {
                log::write(
                    erui::LogLevel::fatal,
                    "Runtime title bridge passed preflight but rich submenu updates could not be published before registration opened");
                set_api_state(ApiState::failed);
                return 1;
            }
            log::write(
                "Runtime title bridge preflight passed; API 1.4 rich submenu updates are available");
            if (tab_preflight ==
                    erui::native::NativeTabsPreflightStatus::available) {
                if (!registry().enable_tab_updates_backend()) {
                    log::write(
                        erui::LogLevel::fatal,
                        "Runtime custom-tab and title backends passed preflight but rich tab updates could not be published before registration opened");
                    set_api_state(ApiState::failed);
                    return 1;
                }
                log::write(
                    "Runtime custom-tab lifecycle preflight passed; API 1.4 rich tab updates are available");
            }
            if (erui::native::native_row_availability_build_supported(
                    row_update_game)) {
                if (!registry().enable_row_availability_backend()) {
                    log::write(
                        erui::LogLevel::fatal,
                        "Runtime row-availability backend passed preflight but its API 1.4 capability could not be published before registration opened");
                    set_api_state(ApiState::failed);
                    return 1;
                }
                log::write(
                    "Runtime row-availability backend preflight passed; API 1.4 typed-row interaction and appearance updates are available");
            } else {
                log::write(
                    erui::LogLevel::warning,
                    "Runtime row-availability backend is unavailable for this game executable; API 1.4 remains negotiable without ERUI_CAP_ROW_AVAILABILITY");
            }
            if (erui::native::native_row_visibility_build_supported(
                    row_update_game)) {
                if (!registry().enable_row_visibility_backend()) {
                    log::write(
                        erui::LogLevel::fatal,
                        "Runtime row-visibility backend passed preflight but its API 1.4 capability could not be published before registration opened");
                    set_api_state(ApiState::failed);
                    return 1;
                }
                log::write(
                    "Runtime row-visibility backend preflight passed; API 1.4 live layout and pagination updates are available");
            } else {
                log::write(
                    erui::LogLevel::warning,
                    "Runtime row-visibility backend is unavailable for this game executable; API 1.4 remains negotiable without ERUI_CAP_ROW_VISIBILITY");
            }
        } else {
            log::write(
                erui::LogLevel::warning,
                "Runtime row-presentation backend is unavailable for this game executable; API 1.4 remains negotiable without ERUI_CAP_UPDATES");
        }

        const auto cooldown = static_cast<std::uint32_t>(std::clamp(
            read_ini(ini, L"Runtime", L"InjectionCooldownMs", 250), 0, 5000));
        LocalizationLanguageSetting language_setting =
            parse_localization_language(read_ini_text(
                ini, L"Localization", L"Language", L"auto"));
        if (language_setting.mode == LocalizationLanguageMode::invalid) {
            log::write(
                erui::LogLevel::warning,
                "Invalid Localization.Language setting; using automatic Steam game-language detection");
            language_setting = {
                LocalizationLanguageMode::automatic,
                {}};
        }
        const bool language_overridden =
            language_setting.mode ==
            LocalizationLanguageMode::explicit_identifier;

        // Preserve the released API 1.0 lifecycle: its table and provider
        // registration become available before Steam finishes initializing.
        // API 1.1 and later negotiation remains gated by
        // steam_language_readiness().
        registry().open_registration();
        set_api_state(ApiState::accepting);
        log::write(
            "Provider registration open for API 1.0; waiting for localization language selection before releasing API 1.1 and later");

        // API 1.3 examples can register thousands of rows and allocate their
        // icon catalogues as soon as language readiness is released. Keep
        // that work out of Solid Uncapper's asynchronous hook-installation
        // window: approve its complete shared chain first, while API 1.1+
        // negotiation still returns ERUI_HOST_NOT_READY. Released API 1.0
        // behavior remains unchanged.
        HMODULE const solid_uncapper_module =
            wait_for_solid_uncapper_module();
        if (g_stop.load(std::memory_order_acquire)) {
            fail_pending_startup_for_stop();
            return 0;
        }
        const bool solid_uncapper_loaded = solid_uncapper_module != nullptr;
        log::write(
            "Solid Uncapper compatibility: moduleAtAttach=%u moduleAtReadinessGate=%u sharedSnapshot=%u sliderSnapshot=%u inputSnapshot=%u colorSnapshot=%u",
            solid_uncapper_present_at_snapshot ? 1U : 0U,
            solid_uncapper_loaded ? 1U : 0U,
            shared_snapshot_ready ? 1U : 0U,
            slider_snapshot_ready ? 1U : 0U,
            input_snapshot_ready ? 1U : 0U,
            color_snapshot_ready ? 1U : 0U);
        if (solid_uncapper_loaded && !shared_snapshot_ready) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper is loaded but the required exact-profile shared-menu snapshot is unavailable; refusing unsafe hook installation");
            set_api_state(ApiState::failed);
            return 1;
        }
        if (solid_uncapper_loaded) {
            const SolidCompatibilityStatus compatibility =
                settle_solid_uncapper_compatibility(
                    solid_uncapper_module,
                    preserved_shared_addresses,
                    slider_snapshot_ready);
            if (compatibility == SolidCompatibilityStatus::stopped) {
                fail_pending_startup_for_stop();
                return 0;
            }
            if (compatibility == SolidCompatibilityStatus::failed) {
                set_api_state(ApiState::failed);
                return 1;
            }
        }

        const std::uint64_t language_wait_started = GetTickCount64();
        std::uint64_t language_last_log = 0;
        SteamLanguageFailure language_last_failure =
            SteamLanguageFailure::none;
        SteamLanguageProbe startup_language{};
        if (language_overridden) {
            startup_language.available = true;
            startup_language.current = language_setting.identifier;
            log::write(
                "Localization override: identifier='%s'; Steam language discovery skipped",
                startup_language.current.c_str());
        } else {
            for (;;) {
                if (g_stop.load(std::memory_order_acquire)) {
                    fail_pending_startup_for_stop();
                    return 0;
                }
                startup_language = probe_steam_language();
                const std::uint64_t now = GetTickCount64();
                const std::uint64_t elapsed = now - language_wait_started;
                const SteamLanguageProbeDecision decision =
                    decide_steam_language_probe(
                        startup_language, elapsed, steam_language_wait_ms);
                if (decision == SteamLanguageProbeDecision::settle) {
                    break;
                }

                if (startup_language.failure_code != language_last_failure ||
                    now - language_last_log >= kSteamReadinessLogMs) {
                    log::write(
                        "Steam language pending: failure=%u (%s) elapsed=%llu ms limit=%llu ms",
                        static_cast<unsigned>(startup_language.failure_code),
                        startup_language.failure
                            ? startup_language.failure
                            : "unknown transient failure",
                        static_cast<unsigned long long>(elapsed),
                        static_cast<unsigned long long>(steam_language_wait_ms));
                    language_last_failure = startup_language.failure_code;
                    language_last_log = now;
                }
                if (decision == SteamLanguageProbeDecision::timeout) {
                    startup_language = {};
                    startup_language.failure_code =
                        SteamLanguageFailure::readiness_timeout;
                    startup_language.failure =
                        "Steam language readiness deadline expired";
                    break;
                }
                Sleep(kSteamReadinessPollMs);
            }
        }
        const std::uint64_t language_wait_elapsed =
            language_overridden
            ? 0
            : GetTickCount64() - language_wait_started;
        const std::string startup_identifier = startup_language.available
            ? startup_language.current
            : std::string{};
        const SteamLanguageFailure startup_failure =
            startup_language.failure_code;
        const char* const startup_failure_text = startup_language.failure
            ? startup_language.failure
            : "unknown Steam language failure";

        HostLocale host_locale = load_host_locale(directory, startup_identifier);
        registry().set_host_locale(std::move(host_locale.pagination));
        log::write(
            "Host locale: identifier='%.*s' external=%d",
            static_cast<int>(startup_identifier.size()),
            startup_identifier.data() ? startup_identifier.data() : "",
            host_locale.loaded ? 1 : 0);

        // Start the registration clock at the exact readiness release. The
        // registry also requires a full quiet interval since this point, so
        // an API 1.0 provider committed during Steam startup cannot cause an
        // immediate freeze before API 1.1-or-later clients register.
        const std::uint64_t opened_at = GetTickCount64();
        const ERUI_Result startup_language_result =
            settle_cached_steam_language(std::move(startup_language));
        if (startup_language_result == ERUI_OK) {
            log::write(
                "Localization language ready: effective='%.*s' known=%u source=%s wait=%llu ms",
                static_cast<int>(startup_identifier.size()),
                startup_identifier.data(),
                static_cast<unsigned>(
                    classify_steam_language(startup_identifier)),
                language_overridden ? "configuration" : "steam",
                static_cast<unsigned long long>(language_wait_elapsed));
        } else if (startup_language_result == ERUI_NOT_SUPPORTED) {
            log::write(
                erui::LogLevel::warning,
                "Steam language is terminally unavailable after %llu ms (failure=%u: %s); connections remain available and language queries will report unsupported",
                static_cast<unsigned long long>(language_wait_elapsed),
                static_cast<unsigned>(startup_failure),
                startup_failure_text);
        } else {
            throw std::logic_error(
                "Steam language settlement did not reach a stable state");
        }
        log::write(
            "Provider registration readiness settled: quiet=%llu ms maxWait=%llu ms",
            static_cast<unsigned long long>(quiet_ms),
            static_cast<unsigned long long>(max_wait_ms));

        while (!g_stop.load(std::memory_order_acquire)) {
            if (auto menu = registry().try_freeze_and_build(
                    opened_at, quiet_ms, max_wait_ms)) {
                g_menu = std::move(menu);
                break;
            }
            Sleep(25);
        }
        if (g_stop.load(std::memory_order_acquire)) {
            fail_pending_startup_for_stop();
            return 0;
        }
        if (!g_menu) throw std::logic_error("registry did not publish a menu");

        // If a loader maps Solid only after the bounded readiness gate, do
        // not silently install an unchained second owner. Standard ModEngine
        // ordering maps the configured DLLs well inside the discovery grace;
        // this check turns an abnormal late load into a safe startup failure.
        HMODULE const solid_uncapper_at_freeze =
            GetModuleHandleW(L"Solid Uncapper.dll");
        if (solid_uncapper_at_freeze != solid_uncapper_module) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper module presence changed after the compatibility readiness gate; refusing unsafe hook installation");
            set_api_state(ApiState::failed);
            return 1;
        }

        if (title_text_backend_ready) {
            erui::native::NativeTitleDlcState native_dlc_state =
                erui::native::NativeTitleDlcState::unknown;
            if (g_menu->title_screen_text_count() != 0U) {
                const SteamDlcProbe dlc_probe =
                    probe_shadow_of_the_erdtree_dlc_with_retry();
                switch (dlc_probe.state) {
                case SteamDlcState::installed:
                    native_dlc_state =
                        erui::native::NativeTitleDlcState::installed;
                    log::write(
                        "Title-screen DLC layout: Shadow of the Erdtree is installed; preserving the native DLC label and authored gap (attempts=%u)",
                        static_cast<unsigned>(dlc_probe.attempts));
                    break;
                case SteamDlcState::absent:
                    native_dlc_state =
                        erui::native::NativeTitleDlcState::absent;
                    log::write(
                        "Title-screen DLC layout: Shadow of the Erdtree is not installed; the empty native DLC row may be reused when the title GFX matches a validated native profile (attempts=%u)",
                        static_cast<unsigned>(dlc_probe.attempts));
                    break;
                case SteamDlcState::unknown:
                    log::write(
                        erui::LogLevel::warning,
                        "Title-screen DLC layout could not determine whether Shadow of the Erdtree is installed (failure=%u: %s, attempts=%u); preserving the native DLC row to avoid overlap",
                        static_cast<unsigned>(dlc_probe.failure_code),
                        dlc_probe.failure
                            ? dlc_probe.failure
                            : "unknown failure",
                        static_cast<unsigned>(dlc_probe.attempts));
                    break;
                }
            }
            const std::array<erui::native::NativeTitleTextOccupancy, 1>
                solid_title_occupancy{{
                    erui::native::NativeTitleTextSide::right,
                    0,
                }};
            const std::span<const erui::native::NativeTitleTextOccupancy>
                external_title_occupancy = solid_uncapper_loaded
                ? std::span<const erui::native::NativeTitleTextOccupancy>(
                    solid_title_occupancy)
                : std::span<const erui::native::NativeTitleTextOccupancy>{};
            const auto title_publish =
                erui::native::publish_native_title_screen_texts(
                    g_menu->title_screen_texts(),
                    native_dlc_state,
                    external_title_occupancy);
            if (title_publish !=
                erui::native::NativeTitleTextPublishStatus::published) {
                log::write(
                    erui::LogLevel::fatal,
                    "Could not publish the frozen title-screen text catalogue (%s)",
                    erui::native::native_title_text_publish_status_name(
                        title_publish));
                set_api_state(ApiState::failed);
                return 1;
            }
            title_catalogue_guard.settle();
        }
        log::write(
            "Registry frozen: generation=%llu providers=%zu rootRows=%zu titleLines=%zu",
            static_cast<unsigned long long>(registry().generation()),
            registry().committed_provider_count(),
            registry().committed_root_row_count(),
            g_menu->title_screen_text_count());
        if (logging_enabled) {
            const erui::detail::TabDiagnosticsReport tab_report =
                registry().tab_diagnostics_report();
            const std::string tab_report_text =
                erui::detail::serialize_tab_diagnostics_report(tab_report);
            DWORD tab_report_error{};
            const std::filesystem::path tab_report_path =
                directory / L"ERNativeUI-tabs.log";
            if (write_utf8_file_atomically(
                    tab_report_path,
                    tab_report_text,
                    tab_report_error)) {
                log::write(
                    "Tab diagnostics written: customTabs=%zu staleOverrides=%zu orderComplete=%u path=ERNativeUI-tabs.log",
                    tab_report.ordered_tabs.size(),
                    tab_report.stale_overrides.size(),
                    tab_report.order_complete ? 1U : 0U);
            } else {
                log::write(
                    erui::LogLevel::warning,
                    "Could not write ERNativeUI-tabs.log (Win32 error=%lu)",
                    static_cast<unsigned long>(tab_report_error));
            }
        }

        erui::RuntimeOptions options{};
        options.enable_row_injection = true;
        options.enable_custom_text = true;
        options.enable_diagnostics = diagnostics;
        // Planning occurs before Game Options exists. Use the
        // conservative vanilla fallback; injection reads and validates the
        // live page object's validated capacity.
        options.game_options_visual_capacity = 6;
        options.injection_cooldown_ms = cooldown;
        options.log_sink = logging_ready ? &runtime_log_sink : nullptr;
        const bool updates_backend_available =
            registry().updates_backend_available();
        const bool row_visibility_backend_available =
            registry().row_visibility_backend_available();
        HMODULE const solid_uncapper_before_native_install =
            GetModuleHandleW(L"Solid Uncapper.dll");
        if (solid_uncapper_before_native_install != solid_uncapper_module) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper module presence changed immediately before native runtime installation; refusing unsafe hook installation");
            set_api_state(ApiState::failed);
            return 1;
        }
        if (solid_uncapper_loaded &&
            !revalidate_solid_uncapper_shared_entries(
                solid_uncapper_module,
                preserved_shared_addresses)) {
            log::write(
                erui::LogLevel::fatal,
                "Solid Uncapper shared-menu chain changed before native hook installation");
            set_api_state(ApiState::failed);
            return 1;
        }
        const erui::InstallResult installed = solid_uncapper_loaded
            ? erui::
                install_with_preserved_shared_addresses_and_tab_configuration(
                    *g_menu,
                    options,
                    preserved_shared_addresses,
                    tab_configuration,
                    updates_backend_available,
                    row_visibility_backend_available)
            : erui::install_with_tab_configuration(
                 *g_menu,
                 options,
                 tab_configuration,
                 updates_backend_available,
                 row_visibility_backend_available);
        if (!installed.success()) {
            log::write(
                erui::LogLevel::fatal,
                "Native runtime installation failed (%u)",
                static_cast<unsigned>(installed.error));
            set_api_state(ApiState::failed);
            return 1;
        }

        set_api_state(ApiState::runtime_ready);
        log::write("ERNativeUI host ready");
        ERUI_GameLanguageInfo language{};
        language.size = sizeof(language);
        const ERUI_Result language_result = get_cached_steam_language(&language);
        if (language_result == ERUI_OK) {
            log::write(
                "Localization language: effective='%.*s' known=%u source=%s",
                static_cast<int>(language.identifier.length),
                language.identifier.data,
                static_cast<unsigned>(language.known_language),
                language_overridden ? "configuration" : "steam");
        } else {
            log::write(
                "Localization language unavailable: result=%u source=steam",
                static_cast<unsigned>(language_result));
        }
        while (!g_stop.load(std::memory_order_acquire)) {
            registry().apply_pending_values();
            erui::poll_changes();
            erui::native::dispatch_native_dialog_callbacks();
            erui::native::dispatch_color_picker_callbacks();
            Sleep(50);
        }
        return 0;
    } catch (const std::exception& exception) {
        log::write(
            erui::LogLevel::fatal,
            "Host bootstrap exception: %s",
            exception.what());
    } catch (...) {
        log::write(
            erui::LogLevel::fatal,
            "Host bootstrap unknown exception");
    }
    set_api_state(ApiState::failed);
    if (steam_language_readiness() == SteamLanguageReadiness::pending) {
        SteamLanguageProbe unavailable{};
        unavailable.failure_code = SteamLanguageFailure::exception;
        unavailable.failure =
            "host initialization failed before localization language selection settled";
        (void)settle_cached_steam_language(std::move(unavailable));
    }
    return 1;
}

DWORD WINAPI host_thread(void*) noexcept {
    return run_host();
}

} // namespace
} // namespace erui::host

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        erui::host::g_module = module;
        // Record the exact-profile shared interfaces while this DLL is still
        // being attached. ModEngine may load the next DLL as soon as we
        // return, so worker-thread-only capture is inherently racy when
        // ERNativeUI is listed before another menu hook. These narrow helpers
        // are loader-lock-safe: bounded PE/RVA reads and fixed copies only.
        HMODULE const game_module = GetModuleHandleW(nullptr);
        erui::host::g_attach_shared_menu_snapshot_ready =
            erui::native::capture_preserved_shared_menu_addresses_at_attach(
                game_module,
                erui::host::g_attach_shared_menu_snapshot);
        erui::host::g_attach_input_snapshot_ready =
            erui::native::capture_native_input_bindings_at_process_attach(
                game_module);
        erui::host::g_attach_color_snapshot_ready =
            erui::native::capture_color_picker_visibility_at_process_attach(
                game_module);
        erui::host::g_attach_solid_uncapper_present =
            GetModuleHandleW(L"Solid Uncapper.dll") != nullptr;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, &erui::host::host_thread,
                nullptr, 0, nullptr)) {
            CloseHandle(thread);
        } else {
            erui::host::set_api_state(erui::host::ApiState::failed);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        erui::host::g_stop.store(true, std::memory_order_release);
    }
    return TRUE;
}
