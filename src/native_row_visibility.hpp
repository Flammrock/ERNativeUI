#pragma once

#include "pagination.hpp"

#include <cstdint>

namespace erui::native {

struct GameAddresses;
struct RowInjectionOutcome;
struct CapturedTitleTarget;
class ModuleView;

// Opaque materialization snapshot used to bind one physical OptionSetting
// page to the applied visibility projection without retaining compiled-row
// addresses in the native lifecycle layer.
struct RowVisibilityMaterializationTicket {
    void* page{};
    erui::detail::PageRoute route{};
    std::uint64_t count_before{};
    std::uint64_t projection_revision{};
    bool internal_rebuild{};
    bool armed{};
};

[[nodiscard]] bool native_row_visibility_supported(
    const GameAddresses& addresses) noexcept;

// Startup preflight used before a complete GameAddresses aggregate exists.
// It admits only the exact live-proved executable, and every row-lifecycle
// and scrollbar-reconciliation entry must retain its canonical prefix.
[[nodiscard]] bool native_row_visibility_build_supported(
    const ModuleView& game) noexcept;

// True only while the proven destructive lifecycle is reconstructing this
// physical page. Internal materialization must not publish another queued
// generation: the transaction has already captured an exact projection and
// will verify the rebuilt native graph against it before releasing input.
[[nodiscard]] bool native_row_visibility_internal_materialization(
    const void* page) noexcept;

[[nodiscard]] RowVisibilityMaterializationTicket
begin_native_row_visibility_materialization(
    void* page,
    erui::detail::PageRoute route,
    const GameAddresses& addresses) noexcept;

void complete_native_row_visibility_materialization(
    const GameAddresses& addresses,
    RowVisibilityMaterializationTicket ticket,
    const RowInjectionOutcome& outcome) noexcept;

// Associates the page-owned title result captured during native submenu
// construction with its visibility binding. The result remains non-owning
// and is discarded with the physical page binding.
void bind_native_row_visibility_title_target(
    void* page,
    const CapturedTitleTarget& captured) noexcept;

// Called around the ordinary native page frame. prepare() may request a
// frame-local disabled input byte while a stable rebuild boundary is reached.
[[nodiscard]] bool prepare_native_row_visibility_page_frame(
    void* page,
    bool native_input_enabled,
    const GameAddresses& addresses) noexcept;

void observe_native_row_visibility_page_frame(
    void* page,
    const GameAddresses& addresses) noexcept;

void retire_native_row_visibility_page(void* page) noexcept;
void reset_native_row_visibility() noexcept;

} // namespace erui::native
