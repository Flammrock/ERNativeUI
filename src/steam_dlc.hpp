#pragma once

#include <cstdint>

namespace erui::host {

inline constexpr std::uint32_t kShadowOfTheErdtreeAppId = 2778580;

// "absent" is a successful Steam query whose answer was false. "unknown"
// means that ERNativeUI could not safely ask Steam yet (or at all), so callers
// must not treat it as evidence that the DLC is absent.
enum class SteamDlcState : std::uint8_t {
    unknown,
    absent,
    installed,
};

enum class SteamDlcFailure : std::uint8_t {
    none,
    module_not_loaded,
    exports_unavailable,
    apps_not_initialized,
    exception,
};

struct SteamDlcProbe {
    SteamDlcState state{SteamDlcState::unknown};
    SteamDlcFailure failure_code{SteamDlcFailure::none};
    const char* failure{};
    // Total number of platform observations made for this result. Direct
    // probes report one; bounded retry probes expose their actual work so the
    // startup log can distinguish an immediate answer from a late one.
    std::uint32_t attempts{1};
};

struct SteamDlcRetryPolicy {
    // This is a total-attempt count, not a retry count. Zero is normalized to
    // one so every call performs exactly one initial observation.
    std::uint32_t maximum_attempts{8};
    std::uint32_t delay_between_attempts_ms{10};
};

inline constexpr std::uint32_t kSteamDlcRetryAttemptLimit = 16;
inline constexpr std::uint32_t kSteamDlcRetryDelayLimitMs = 25;
inline constexpr SteamDlcRetryPolicy kDefaultSteamDlcRetryPolicy{};

// Injectable seams used by the deterministic retry-policy tests. Production
// callers should use probe_shadow_of_the_erdtree_dlc_with_retry() below.
using SteamDlcProbeCallback = SteamDlcProbe (*)(void* context) noexcept;
using SteamDlcWaitCallback = void (*)(
    void* context,
    std::uint32_t delay_ms) noexcept;

// Normalized outcomes from the platform-specific Steam observation. Keeping
// this mapping separate makes the important policy -- particularly
// "unavailable is not absent" -- exhaustively testable without Steam.
enum class SteamDlcObservation : std::uint8_t {
    module_not_loaded,
    exports_unavailable,
    apps_not_initialized,
    absent,
    installed,
    exception,
};

[[nodiscard]] SteamDlcProbe evaluate_steam_dlc_observation(
    SteamDlcObservation observation) noexcept;
[[nodiscard]] bool is_retryable_steam_dlc_failure(
    SteamDlcFailure failure) noexcept;

// Repeats only observations that can naturally resolve while Steam finishes
// starting. Attempts and per-attempt delay are independently capped, so even
// a malformed policy cannot turn this startup query into a long stall.
[[nodiscard]] SteamDlcProbe retry_steam_dlc_probe(
    SteamDlcRetryPolicy policy,
    SteamDlcProbeCallback probe,
    SteamDlcWaitCallback wait,
    void* context) noexcept;

// Performs ordinary, read-only calls through the Steamworks flat C API. It
// neither links the Steam SDK nor hooks any Steam/game function.
[[nodiscard]] SteamDlcProbe probe_shadow_of_the_erdtree_dlc() noexcept;

// Production entry point. Its default policy performs at most eight probes
// and seven 10 ms waits (70 ms total waiting). If Steam is still unavailable,
// the result remains "unknown" and callers must preserve the DLC layout.
[[nodiscard]] SteamDlcProbe probe_shadow_of_the_erdtree_dlc_with_retry(
    SteamDlcRetryPolicy policy = kDefaultSteamDlcRetryPolicy) noexcept;

} // namespace erui::host
