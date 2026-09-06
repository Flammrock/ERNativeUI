#include "color_picker_state.hpp"

namespace erui::detail {

ColorPickerState::ColorPickerState(RgbColor initial_value) noexcept
    : packed_value_(pack_native_color(initial_value)) {}

RgbColor ColorPickerState::value() const noexcept {
    return unpack_native_color(
        packed_value_.load(std::memory_order_acquire));
}

std::uint32_t ColorPickerState::native_value() const noexcept {
    return packed_value_.load(std::memory_order_acquire);
}

bool ColorPickerState::set_programmatic(RgbColor value) noexcept {
    const std::uint32_t packed = pack_native_color(value);
    const std::uint32_t previous = packed_value_.exchange(
        packed, std::memory_order_acq_rel);
    if (previous == packed) return false;
    request_preview_refresh();
    return true;
}

bool ColorPickerState::accept_native(
    std::uint32_t packed_value,
    RgbColor& accepted_value) noexcept {
    accepted_value = unpack_native_color(packed_value);
    const std::uint32_t normalized = pack_native_color(accepted_value);
    const std::uint32_t previous = packed_value_.exchange(
        normalized, std::memory_order_acq_rel);
    request_preview_refresh();
    return previous != normalized;
}

void ColorPickerState::request_preview_refresh() noexcept {
    preview_refresh_pending_.store(true, std::memory_order_release);
}

bool ColorPickerState::consume_preview_refresh() noexcept {
    return preview_refresh_pending_.exchange(
        false, std::memory_order_acq_rel);
}

} // namespace erui::detail
