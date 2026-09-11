#include "host_registry.hpp"
#include "host_locale.hpp"
#include "addresses.hpp"
#include "hooks.hpp"
#include "logger.hpp"
#include "module.hpp"
#include "native_dialog.hpp"
#include "native_input_bindings.hpp"
#include "color_picker.hpp"
#include "runtime_log.hpp"
#include "steam_language.hpp"

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
#include <string>
#include <system_error>
#include <utility>

namespace erui::host {
namespace {

HMODULE g_module{};
std::atomic_bool g_stop{};
std::unique_ptr<erui::Menu> g_menu{};

constexpr DWORD kSteamReadinessPollMs = 25;
constexpr std::uint64_t kSteamReadinessLogMs = 2000;

void fail_pending_startup_for_stop() noexcept {
    set_api_state(ApiState::failed);
    if (steam_language_readiness() == SteamLanguageReadiness::pending) {
        SteamLanguageProbe unavailable{};
        unavailable.failure_code = SteamLanguageFailure::exception;
        unavailable.failure = "host stopped before Steam language settled";
        (void)settle_cached_steam_language(std::move(unavailable));
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
    const char* name = "INFO";
    if (level == erui::LogLevel::warning) name = "WARN";
    else if (level == erui::LogLevel::error) name = "ERROR";
    else if (level == erui::LogLevel::trace) name = "TRACE";
    log::write("[ERNativeUI/%s] %s", name, message ? message : "");
}

DWORD run_host() noexcept {
    try {
        const auto directory = module_directory(g_module);
        const auto ini = directory / L"ERNativeUI.ini";
        if (!ensure_default_ini(g_module, ini)) {
            OutputDebugStringA(
                "ERNativeUI: failed to create the default ERNativeUI.ini\n");
        }
        const bool logging_enabled = read_ini(
            ini, L"Logging", L"EnableLog", 0) != 0;
        if (logging_enabled) {
            log::initialize(directory / L"ERNativeUI.log");
        }
        erui::detail::set_log_sink(&runtime_log_sink);
        log::write("============================================================");
        log::write("ERNativeUI host v%s starting", ER_NATIVE_UI_BUILD_VERSION);
        log::write("Offline / EAC-disabled use only; hot unloading is unsupported");

        HMODULE pinned_host{};
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(g_module), &pinned_host)) {
            log::write("WARN: host module could not be pinned; do not hot-unload it");
        }

        HMODULE const solid_uncapper_module =
            GetModuleHandleW(L"Solid Uncapper.dll");
        const bool solid_uncapper_loaded = solid_uncapper_module != nullptr;
        erui::native::GameAddresses early_addresses{};
        std::array<std::array<unsigned char, 16>, 3>
            early_shared_entry_bytes{};
        if (solid_uncapper_loaded) {
            log::write(
                "Solid Uncapper detected; capturing native interfaces before its asynchronous hook installation");
            erui::native::ModuleView game{};
            if (!game.initialize(GetModuleHandleW(nullptr))) {
                log::write(
                    "ERROR: could not inspect the game image before Solid Uncapper initialization");
                set_api_state(ApiState::failed);
                return 1;
            }
            if (!erui::native::
                    capture_color_picker_visibility_before_third_party_hooks(
                        game)) {
                log::write(
                    "WARN: Color Picker visibility interface could not be captured early; menus without ColorPicker rows may remain available");
            }
            const bool captured_input_bindings =
                erui::native::capture_native_input_bindings_before_third_party_hooks();
            if (!captured_input_bindings) {
                log::write(
                    "WARN: native input-binding interfaces could not be captured early; menus without custom bindings may remain available");
            }
            if (!erui::native::resolve_game_addresses(
                    game, early_addresses,
                    true, true, true, true, true, false, true)) {
                log::write(
                    "ERROR: could not capture native interfaces before Solid Uncapper initialization");
                set_api_state(ApiState::failed);
                return 1;
            }
            const std::array<const unsigned char*, 3> shared_entries{
                reinterpret_cast<const unsigned char*>(
                    early_addresses.game_options_handler),
                reinterpret_cast<const unsigned char*>(
                    early_addresses.sub_handler),
                reinterpret_cast<const unsigned char*>(
                    early_addresses.text_resolver)};
            for (std::size_t index = 0;
                 index < shared_entries.size(); ++index) {
                if (absolute_indirect_jump_target(shared_entries[index])) {
                    log::write(
                        "ERROR: shared native interface %zu was detoured before its pristine entry could be captured",
                        index);
                    set_api_state(ApiState::failed);
                    return 1;
                }
                std::memcpy(
                    early_shared_entry_bytes[index].data(),
                    shared_entries[index],
                    early_shared_entry_bytes[index].size());
            }
            log::write(
                "Solid Uncapper compatibility: native interfaces captured");
        }

        const auto quiet_ms = static_cast<std::uint64_t>(std::clamp(
            read_ini(ini, L"Runtime", L"RegistrationQuietMs", 750), 100, 5000));
        const auto max_wait_ms = static_cast<std::uint64_t>(std::clamp(
            read_ini(ini, L"Runtime", L"RegistrationMaxWaitMs", 5000),
            static_cast<int>(quiet_ms), 15000));
        const auto steam_language_wait_ms = static_cast<std::uint64_t>(
            std::clamp(read_ini(
                ini, L"Runtime", L"SteamLanguageWaitMs", 5000),
                0, 30000));
        const auto cooldown = static_cast<std::uint32_t>(std::clamp(
            read_ini(ini, L"Runtime", L"InjectionCooldownMs", 250), 0, 5000));
        const bool diagnostics = read_ini(
            ini, L"Diagnostics", L"EnableDiagnostics", 0) != 0;

        // Preserve the released API 1.0 lifecycle: its table and provider
        // registration become available before Steam finishes initializing.
        // API 1.1 negotiation remains gated by steam_language_readiness().
        registry().open_registration();
        set_api_state(ApiState::accepting);
        log::write(
            "Provider registration open for API 1.0; waiting for Steam language before releasing API 1.1");

        const std::uint64_t language_wait_started = GetTickCount64();
        std::uint64_t language_last_log = 0;
        SteamLanguageFailure language_last_failure =
            SteamLanguageFailure::none;
        SteamLanguageProbe startup_language{};
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
        const std::uint64_t language_wait_elapsed =
            GetTickCount64() - language_wait_started;
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
        // immediate freeze before newly released API 1.1 clients register.
        const std::uint64_t opened_at = GetTickCount64();
        const ERUI_Result startup_language_result =
            settle_cached_steam_language(std::move(startup_language));
        if (startup_language_result == ERUI_OK) {
            log::write(
                "Startup language ready: current='%.*s' known=%u wait=%llu ms",
                static_cast<int>(startup_identifier.size()),
                startup_identifier.data(),
                static_cast<unsigned>(
                    classify_steam_language(startup_identifier)),
                static_cast<unsigned long long>(language_wait_elapsed));
        } else if (startup_language_result == ERUI_NOT_SUPPORTED) {
            log::write(
                "WARN: Steam language is terminally unavailable after %llu ms (failure=%u: %s); connections remain available and language queries will report unsupported",
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
        log::write(
            "Registry frozen: generation=%llu providers=%zu rootRows=%zu",
            static_cast<unsigned long long>(registry().generation()),
            registry().committed_provider_count(),
            registry().committed_root_row_count());

        if (solid_uncapper_loaded) {
            // DLL load order does not serialize worker-thread initialization.
            // Wait for the three shared entry points to be patched and stable,
            // then validate their destinations before adding our chain links.
            constexpr DWORD kSolidUncapperTimeoutMs = 20000;
            constexpr DWORD kStableWindowMs = 500;
            const std::array<const unsigned char*, 3> targets{
                reinterpret_cast<const unsigned char*>(
                    early_addresses.game_options_handler),
                reinterpret_cast<const unsigned char*>(early_addresses.sub_handler),
                reinterpret_cast<const unsigned char*>(early_addresses.text_resolver)};
            log::write(
                "Solid Uncapper compatibility: waiting up to %lu ms for shared menu hooks",
                static_cast<unsigned long>(kSolidUncapperTimeoutMs));
            const std::uint64_t wait_started = GetTickCount64();
            std::uint64_t all_changed_at = 0;
            while (!g_stop.load(std::memory_order_acquire) &&
                   GetTickCount64() - wait_started < kSolidUncapperTimeoutMs) {
                bool all_changed = true;
                for (std::size_t index = 0; index < targets.size(); ++index) {
                    if (std::memcmp(
                            early_shared_entry_bytes[index].data(),
                            targets[index],
                            early_shared_entry_bytes[index].size()) == 0) {
                        all_changed = false;
                        break;
                    }
                }
                const std::uint64_t now = GetTickCount64();
                if (all_changed) {
                    if (!all_changed_at) all_changed_at = now;
                    if (now - all_changed_at >= kStableWindowMs) break;
                } else {
                    all_changed_at = 0;
                }
                Sleep(50);
            }
            if (g_stop.load(std::memory_order_acquire)) {
                fail_pending_startup_for_stop();
                return 0;
            }
            std::size_t changed_count = 0;
            for (std::size_t index = 0; index < targets.size(); ++index) {
                if (std::memcmp(
                        early_shared_entry_bytes[index].data(),
                        targets[index],
                        early_shared_entry_bytes[index].size()) != 0) {
                    ++changed_count;
                }
            }
            log::write(
                "Solid Uncapper compatibility: shared hooks=%zu/3 elapsed=%llu ms",
                changed_count,
                static_cast<unsigned long long>(GetTickCount64() - wait_started));

            if (changed_count != 0 && changed_count != targets.size()) {
                log::write(
                    "ERROR: Solid Uncapper installed only %zu/3 shared hooks; refusing a partial chain",
                    changed_count);
                set_api_state(ApiState::failed);
                return 1;
            }

            if (changed_count == targets.size()) {
                erui::native::ModuleView solid_uncapper{};
                if (!solid_uncapper.initialize(
                        solid_uncapper_module)) {
                    log::write(
                        "ERROR: could not inspect Solid Uncapper image for hook ownership");
                    set_api_state(ApiState::failed);
                    return 1;
                }
                for (std::size_t index = 0; index < targets.size(); ++index) {
                    const void* destination =
                        absolute_indirect_jump_target(targets[index]);
                    if (!destination || !solid_uncapper.contains(destination) ||
                        !solid_uncapper.is_executable(destination)) {
                        log::write(
                            "ERROR: shared menu hook %zu has an unsupported or foreign detour",
                            index);
                        set_api_state(ApiState::failed);
                        return 1;
                    }
                }
                log::write(
                    "Solid Uncapper compatibility: three owned detours validated; installing chained hooks");
            } else {
                log::write(
                    "Solid Uncapper compatibility: no shared hooks appeared; continuing without menu chaining");
            }

            if (!erui::native::approve_captured_native_input_bindings(
                    solid_uncapper_module)) {
                log::write(
                    "WARN: Solid Uncapper input-binding chain was not approved; installation can continue only when no client registered custom bindings");
            } else {
                log::write(
                    "Solid Uncapper compatibility: input-binding chain approved");
            }
            if (!erui::native::approve_captured_color_picker_visibility(
                    solid_uncapper_module)) {
                log::write(
                    "WARN: Solid Uncapper Color Picker chain was not approved; installation can continue only when no client registered ColorPicker rows");
            } else {
                log::write(
                    "Solid Uncapper compatibility: Color Picker chain approved");
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
        options.log_sink = &runtime_log_sink;
        const erui::InstallResult installed = solid_uncapper_loaded
            ? erui::install_with_resolved_addresses(
                *g_menu, options, early_addresses)
            : erui::install(*g_menu, options);
        if (!installed.success()) {
            log::write("ERROR: native runtime installation failed (%u)",
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
                "Steam language: current='%.*s' known=%u",
                static_cast<int>(language.identifier.length),
                language.identifier.data,
                static_cast<unsigned>(language.known_language));
        } else {
            log::write(
                "Steam language unavailable: result=%u",
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
        log::write("ERROR: host bootstrap exception: %s", exception.what());
    } catch (...) {
        log::write("ERROR: host bootstrap unknown exception");
    }
    set_api_state(ApiState::failed);
    if (steam_language_readiness() == SteamLanguageReadiness::pending) {
        SteamLanguageProbe unavailable{};
        unavailable.failure_code = SteamLanguageFailure::exception;
        unavailable.failure = "host initialization failed before Steam language settled";
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
