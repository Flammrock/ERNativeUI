#include "runtime_state.hpp"

namespace erui::detail {

RuntimeState& runtime_state() noexcept {
    static RuntimeState instance;
    return instance;
}

bool diagnostics_enabled() noexcept {
    return runtime_state().options.enable_diagnostics;
}

bool verbose_logging() noexcept {
    return diagnostics_enabled();
}

bool trace_logging() noexcept {
    return diagnostics_enabled();
}

bool message_trace_logging() noexcept {
    return diagnostics_enabled();
}

} // namespace erui::detail
