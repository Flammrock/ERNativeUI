#pragma once

#include "menu_compiler.hpp"
#include "scaleform_html.hpp"

#include <ernativeui/erui.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace erui::detail {

inline constexpr std::size_t row_presentation_max_text_units =
    scaleform_html_max_units;

struct RowPresentationUpdate {
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
    ERUI_RowHandle row{ERUI_INVALID_ROW};
    std::wstring label{};
    std::wstring help{};
    std::optional<std::wstring> caption{};
    bool interactive{true};
    ERUI_AvailabilityAppearance availability_appearance{
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY};
};

struct RowPresentationView {
    const wchar_t* label{};
    const wchar_t* help{};
    // Null keeps the right-hand control text under native ownership.
    const wchar_t* caption{};
    std::uint64_t revision{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return revision != 0 && label && help;
    }
};

struct RowAvailabilityView {
    bool interactive{true};
    ERUI_AvailabilityAppearance appearance{
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY};
    std::uint64_t revision{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return revision != 0;
    }
};

// Accepts a fully validated, complete snapshot for every target row. The
// call owns no native objects and may run on any client thread. Either the
// complete vector is published or the previous desired generation remains
// unchanged.
[[nodiscard]] ERUI_Result submit_row_presentation_updates(
    std::vector<RowPresentationUpdate> updates) noexcept;

// Assigns stable presentation slots after ordinary menu compilation. Updates
// accepted between provider commit and runtime compilation are preserved.
[[nodiscard]] bool configure_row_presentation_state(
    CompiledMenu& menu) noexcept;

// Copies the newest desired generation into fixed-address UTF-16 buffers.
// Native code calls this only at a validated UI boundary.
void synchronize_pending_row_presentations() noexcept;

[[nodiscard]] RowPresentationView row_presentation_view(
    std::size_t slot) noexcept;

[[nodiscard]] RowAvailabilityView row_availability_view(
    std::size_t slot) noexcept;

[[nodiscard]] bool row_presentation_updates_pending() noexcept;
[[nodiscard]] std::size_t editable_row_presentation_count() noexcept;

// Runtime uninstall/process teardown only. Call after native callbacks have
// been disarmed; live native rows may retain pointers into these buffers.
void reset_row_presentation_state() noexcept;

} // namespace erui::detail
