#include "steam_dlc.hpp"

#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace erui::host {
namespace {

using SteamAppsAccessor = void* (__cdecl*)();
using SteamHandleQuery = std::int32_t (__cdecl*)();
using SteamDlcInstalledQuery = bool (__cdecl*)(
    void* steam_apps,
    std::uint32_t app_id);

template <typename Function>
Function load_export(HMODULE module, const char* name) noexcept {
    static_assert(sizeof(Function) == sizeof(FARPROC));
    const FARPROC address = GetProcAddress(module, name);
    Function function{};
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

SteamDlcProbe probe_shadow_of_the_erdtree_dlc_callback(
    void*) noexcept {
    return probe_shadow_of_the_erdtree_dlc();
}

void wait_for_steam_dlc_retry(
    void*,
    std::uint32_t delay_ms) noexcept {
    if (delay_ms == 0) {
        // A zero-delay policy still yields once rather than spinning through
        // every attempt in the same scheduler slice.
        SwitchToThread();
        return;
    }
    Sleep(delay_ms);
}

} // namespace

SteamDlcProbe evaluate_steam_dlc_observation(
    SteamDlcObservation observation) noexcept {
    switch (observation) {
    case SteamDlcObservation::module_not_loaded:
        return {
            SteamDlcState::unknown,
            SteamDlcFailure::module_not_loaded,
            "steam_api64.dll is not loaded",
        };
    case SteamDlcObservation::exports_unavailable:
        return {
            SteamDlcState::unknown,
            SteamDlcFailure::exports_unavailable,
            "required Steamworks DLC exports are unavailable",
        };
    case SteamDlcObservation::apps_not_initialized:
        return {
            SteamDlcState::unknown,
            SteamDlcFailure::apps_not_initialized,
            "SteamApps008 is not initialized",
        };
    case SteamDlcObservation::absent:
        return {SteamDlcState::absent, SteamDlcFailure::none, nullptr};
    case SteamDlcObservation::installed:
        return {SteamDlcState::installed, SteamDlcFailure::none, nullptr};
    case SteamDlcObservation::exception:
        return {
            SteamDlcState::unknown,
            SteamDlcFailure::exception,
            "exception while querying Steam DLC state",
        };
    }
    return {
        SteamDlcState::unknown,
        SteamDlcFailure::exception,
        "invalid Steam DLC observation",
    };
}

bool is_retryable_steam_dlc_failure(SteamDlcFailure failure) noexcept {
    switch (failure) {
    case SteamDlcFailure::module_not_loaded:
    case SteamDlcFailure::apps_not_initialized:
        return true;
    case SteamDlcFailure::none:
    case SteamDlcFailure::exports_unavailable:
    case SteamDlcFailure::exception:
        return false;
    }
    return false;
}

SteamDlcProbe retry_steam_dlc_probe(
    SteamDlcRetryPolicy policy,
    SteamDlcProbeCallback probe,
    SteamDlcWaitCallback wait,
    void* context) noexcept {
    if (!probe) {
        SteamDlcProbe invalid = evaluate_steam_dlc_observation(
            SteamDlcObservation::exception);
        invalid.failure = "Steam DLC retry probe callback is null";
        return invalid;
    }

    const std::uint32_t maximum_attempts = std::clamp(
        policy.maximum_attempts,
        1u,
        kSteamDlcRetryAttemptLimit);
    const std::uint32_t delay_ms = std::min(
        policy.delay_between_attempts_ms,
        kSteamDlcRetryDelayLimitMs);

    SteamDlcProbe result{};
    for (std::uint32_t attempt = 1; attempt <= maximum_attempts; ++attempt) {
        result = probe(context);
        result.attempts = attempt;

        if (!is_retryable_steam_dlc_failure(result.failure_code) ||
            attempt == maximum_attempts) {
            return result;
        }

        if (wait) {
            wait(context, delay_ms);
        }
    }

    // The normalized attempt count is always at least one, so this line is
    // unreachable. Keep the fail-safe result explicit for defensive builds.
    return result;
}

SteamDlcProbe probe_shadow_of_the_erdtree_dlc() noexcept {
    try {
        const HMODULE steam = GetModuleHandleW(L"steam_api64.dll");
        if (!steam) {
            return evaluate_steam_dlc_observation(
                SteamDlcObservation::module_not_loaded);
        }

        const auto steam_apps = load_export<SteamAppsAccessor>(
            steam, "SteamAPI_SteamApps_v008");
        const auto steam_user = load_export<SteamHandleQuery>(
            steam, "SteamAPI_GetHSteamUser");
        const auto steam_pipe = load_export<SteamHandleQuery>(
            steam, "SteamAPI_GetHSteamPipe");
        const auto is_dlc_installed = load_export<SteamDlcInstalledQuery>(
            steam, "SteamAPI_ISteamApps_BIsDlcInstalled");
        if (!steam_apps || !steam_user || !steam_pipe || !is_dlc_installed) {
            return evaluate_steam_dlc_observation(
                SteamDlcObservation::exports_unavailable);
        }

        // Elden Ring owns SteamAPI_Init. Do not enter the interface accessor
        // while its process-wide user/pipe context is still being constructed.
        if (steam_user() <= 0 || steam_pipe() <= 0) {
            return evaluate_steam_dlc_observation(
                SteamDlcObservation::apps_not_initialized);
        }

        void* const apps = steam_apps();
        if (!apps) {
            return evaluate_steam_dlc_observation(
                SteamDlcObservation::apps_not_initialized);
        }

        return evaluate_steam_dlc_observation(is_dlc_installed(
            apps, kShadowOfTheErdtreeAppId)
                ? SteamDlcObservation::installed
                : SteamDlcObservation::absent);
    } catch (...) {
        return evaluate_steam_dlc_observation(
            SteamDlcObservation::exception);
    }
}

SteamDlcProbe probe_shadow_of_the_erdtree_dlc_with_retry(
    SteamDlcRetryPolicy policy) noexcept {
    return retry_steam_dlc_probe(
        policy,
        &probe_shadow_of_the_erdtree_dlc_callback,
        &wait_for_steam_dlc_retry,
        nullptr);
}

} // namespace erui::host
