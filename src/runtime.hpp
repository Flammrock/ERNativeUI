#pragma once

#include "menu.hpp"

#include <cstddef>
#include <cstdint>

namespace erui::native {
struct GameAddresses;
}

namespace erui {

enum class LogLevel : std::uint8_t {
    info,
    warning,
    error,
    trace,
};

using LogSink = void (*)(LogLevel level, const char* message) noexcept;

inline constexpr std::uint32_t runtime_api_version = 0x0009'0300;

struct RuntimeOptions {
    bool enable_row_injection{true};
    bool enable_custom_text{true};
    bool enable_diagnostics{false};
    std::uint8_t reserved0{};

    // Conservative planning fallback used before the native Controller
    // Settings page exists. Injection reads the actual 6..13 capacity from
    // memory.
    std::uint8_t controller_visual_capacity{6};

    std::uint8_t reserved1{};
    std::uint8_t reserved2{};

    std::uint32_t injection_cooldown_ms{250};
    LogSink log_sink{};
};

#if defined(_WIN64)
static_assert(sizeof(RuntimeOptions) == 24,
    "Unexpected ERNativeUI RuntimeOptions layout");
static_assert(offsetof(RuntimeOptions, enable_row_injection) == 0);
static_assert(offsetof(RuntimeOptions, enable_custom_text) == 1);
static_assert(offsetof(RuntimeOptions, enable_diagnostics) == 2);
static_assert(offsetof(RuntimeOptions, controller_visual_capacity) == 4);
static_assert(offsetof(RuntimeOptions, injection_cooldown_ms) == 8);
static_assert(offsetof(RuntimeOptions, log_sink) == 16,
    "Unexpected ERNativeUI RuntimeOptions::log_sink offset");
#endif

enum class InstallError : std::uint8_t {
    none,
    already_installed,
    invalid_options,
    menu_compile_failed,
    game_module_invalid,
    address_resolution_failed,
    hook_install_failed,
};

struct InstallResult {
    InstallError error{InstallError::none};
    [[nodiscard]] bool success() const noexcept { return error == InstallError::none; }
};

struct Capabilities {
    bool custom_text_runtime{true};
    bool toggle_runtime{true};
    bool slider_runtime{true};
    bool inline_choice_runtime{true};
    bool popup_choice_runtime{true};
    bool button_model{true};
    bool submenu_model{true};
    bool button_runtime{true};
    bool submenu_runtime{true};
    bool pagination_runtime{true};
    bool controller_capacity_detection{true};
};

[[nodiscard]] InstallResult install(Menu& menu, RuntimeOptions options = {}) noexcept;
// Internal host integration point used when native addresses must be captured
// before another asynchronously initializing menu mod installs its detours.
[[nodiscard]] InstallResult install_with_resolved_addresses(
    Menu& menu,
    RuntimeOptions options,
    const native::GameAddresses& addresses) noexcept;
void uninstall() noexcept;
[[nodiscard]] bool installed() noexcept;
[[nodiscard]] Capabilities capabilities() noexcept;
[[nodiscard]] std::uint64_t custom_text_hit_count() noexcept;
[[nodiscard]] std::uint64_t button_action_hit_count() noexcept;
[[nodiscard]] std::uint64_t submenu_open_request_count() noexcept;
[[nodiscard]] std::uint64_t submenu_page_hit_count() noexcept;
[[nodiscard]] std::uint64_t pagination_next_hit_count() noexcept;
[[nodiscard]] std::uint64_t pagination_previous_hit_count() noexcept;

// Poll bound toggle/slider/choice bytes and invoke ValueAction callbacks. The
// host DLL calls this from its worker thread.
std::size_t poll_changes() noexcept;

} // namespace erui
