#pragma once

#include "menu_compiler.hpp"

#include "runtime.hpp"

#include <atomic>
#include <memory>

namespace erui::detail {

struct RuntimeState {
    RuntimeOptions options{};
    std::unique_ptr<CompiledMenu> menu{};
    std::atomic_bool installed{false};
    std::atomic<std::uint64_t> custom_text_hits{0};
    std::atomic<std::uint32_t> diagnostic_value_logs{0};
    std::atomic<std::uint64_t> button_action_hits{0};
    std::atomic<std::uint32_t> diagnostic_button_action_logs{0};
    std::atomic<std::uint32_t> button_action_faults{0};
    std::atomic<std::uint64_t> submenu_open_requests{0};
    std::atomic<std::uint64_t> submenu_page_hits{0};
    std::atomic<std::uint32_t> submenu_open_faults{0};
    std::atomic<std::uint32_t> diagnostic_submenu_logs{0};
    std::atomic<std::uint64_t> pagination_next_hits{0};
    std::atomic<std::uint64_t> pagination_previous_hits{0};
};

RuntimeState& runtime_state() noexcept;
[[nodiscard]] bool diagnostics_enabled() noexcept;
[[nodiscard]] bool verbose_logging() noexcept;
[[nodiscard]] bool trace_logging() noexcept;
[[nodiscard]] bool message_trace_logging() noexcept;

} // namespace erui::detail
