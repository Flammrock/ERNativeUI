#pragma once

#include "addresses.hpp"
#include "menu_compiler.hpp"

#include <cstdint>

namespace erui::native {

// Builds the immutable bound-value lookup used by the single production
// editor adapter. Call once after address resolution and before hooks can run.
[[nodiscard]] bool prepare_native_text_inputs(
    const erui::detail::CompiledMenu& menu,
    const GameAddresses& addresses) noexcept;

// Releases host-owned native MenuString instances and clears every binding.
// Call only after the hooks that may reach these bindings have been removed.
void reset_native_text_inputs() noexcept;

// Applies programmatic values to native CS::MenuString objects. This must run
// on the game's page/UI thread, never on an arbitrary client or host thread.
void synchronize_pending_text_inputs() noexcept;

[[nodiscard]] bool add_text_input(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool& faulted) noexcept;

} // namespace erui::native
