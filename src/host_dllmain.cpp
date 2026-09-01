#include "host_registry.hpp"
#include "host_locale.hpp"
#include "addresses.hpp"
#include "hooks.hpp"
#include "logger.hpp"
#include "module.hpp"
#include "native_dialog.hpp"
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
#include <system_error>

namespace erui::host {
namespace {

HMODULE g_module{};
std::atomic_bool g_stop{};
std::unique_ptr<erui::Menu> g_menu{};

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
    const auto* slot = entry + 6 + displacement;
    const void* target{};
    std::memcpy(&target, slot, sizeof(target));
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

        const bool solid_uncapper_loaded =
            GetModuleHandleW(L"Solid Uncapper.dll") != nullptr;
        erui::native::GameAddresses early_addresses{};
        if (solid_uncapper_loaded) {
            log::write(
                "Solid Uncapper detected; capturing native interfaces before its asynchronous hook installation");
            erui::native::ModuleView game{};
            if (!game.initialize(GetModuleHandleW(nullptr)) ||
                !erui::native::resolve_game_addresses(
                    game, early_addresses,
                    true, true, true, true, true, false, true)) {
                log::write(
                    "ERROR: could not capture native interfaces before Solid Uncapper initialization");
                set_api_state(ApiState::failed);
                return 1;
            }
            log::write(
                "Solid Uncapper compatibility: native interfaces captured");
        }

        const auto quiet_ms = static_cast<std::uint64_t>(std::clamp(
            read_ini(ini, L"Runtime", L"RegistrationQuietMs", 750), 100, 5000));
        const auto max_wait_ms = static_cast<std::uint64_t>(std::clamp(
            read_ini(ini, L"Runtime", L"RegistrationMaxWaitMs", 5000),
            static_cast<int>(quiet_ms), 15000));
        const auto cooldown = static_cast<std::uint32_t>(std::clamp(
            read_ini(ini, L"Runtime", L"InjectionCooldownMs", 250), 0, 5000));
        const bool diagnostics = read_ini(
            ini, L"Diagnostics", L"EnableDiagnostics", 0) != 0;

        ERUI_GameLanguageInfo startup_language{};
        startup_language.size = sizeof(startup_language);
        const ERUI_Result startup_language_result =
            get_cached_steam_language(&startup_language);
        std::string_view startup_identifier{};
        if (startup_language_result == ERUI_OK &&
            startup_language.identifier.data) {
            startup_identifier = std::string_view(
                startup_language.identifier.data,
                startup_language.identifier.length);
        }
        HostLocale host_locale = load_host_locale(directory, startup_identifier);
        registry().set_host_locale(std::move(host_locale.pagination));
        log::write(
            "Host locale: identifier='%.*s' external=%d",
            static_cast<int>(startup_identifier.size()),
            startup_identifier.data() ? startup_identifier.data() : "",
            host_locale.loaded ? 1 : 0);

        registry().open_registration();
        set_api_state(ApiState::accepting);
        const std::uint64_t opened_at = GetTickCount64();
        log::write(
            "Provider registration open: quiet=%llu ms maxWait=%llu ms",
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
        if (g_stop.load(std::memory_order_acquire)) return 0;
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
            std::array<std::array<unsigned char, 16>, 3> pristine{};
            const std::array<const unsigned char*, 3> targets{
                reinterpret_cast<const unsigned char*>(early_addresses.hub_handler),
                reinterpret_cast<const unsigned char*>(early_addresses.sub_handler),
                reinterpret_cast<const unsigned char*>(early_addresses.text_resolver)};
            for (std::size_t index = 0; index < targets.size(); ++index) {
                std::memcpy(pristine[index].data(), targets[index], pristine[index].size());
            }
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
                            pristine[index].data(), targets[index],
                            pristine[index].size()) == 0) {
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
            if (g_stop.load(std::memory_order_acquire)) return 0;
            std::size_t changed_count = 0;
            for (std::size_t index = 0; index < targets.size(); ++index) {
                if (std::memcmp(
                        pristine[index].data(), targets[index],
                        pristine[index].size()) != 0) {
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
                        GetModuleHandleW(L"Solid Uncapper.dll"))) {
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
        }

        erui::RuntimeOptions options{};
        options.enable_row_injection = true;
        options.enable_custom_text = true;
        options.enable_diagnostics = diagnostics;
        // Planning occurs before Controller Settings exists. Use the
        // conservative vanilla fallback; injection reads and validates the
        // live page object's validated capacity.
        options.controller_visual_capacity = 6;
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
            Sleep(50);
        }
        return 0;
    } catch (const std::exception& exception) {
        log::write("ERROR: host bootstrap exception: %s", exception.what());
    } catch (...) {
        log::write("ERROR: host bootstrap unknown exception");
    }
    set_api_state(ApiState::failed);
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
