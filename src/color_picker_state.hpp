#pragma once

#include <atomic>
#include <cstdint>

namespace erui::detail {

struct RgbColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

[[nodiscard]] constexpr bool operator==(
    RgbColor left,
    RgbColor right) noexcept {
    return left.red == right.red && left.green == right.green &&
        left.blue == right.blue;
}

[[nodiscard]] constexpr bool operator!=(
    RgbColor left,
    RgbColor right) noexcept {
    return !(left == right);
}

[[nodiscard]] constexpr std::uint32_t pack_native_color(
    RgbColor color) noexcept {
    return static_cast<std::uint32_t>(color.red) |
        (static_cast<std::uint32_t>(color.green) << 8u) |
        (static_cast<std::uint32_t>(color.blue) << 16u) |
        UINT32_C(0xFF000000);
}

[[nodiscard]] constexpr RgbColor unpack_native_color(
    std::uint32_t packed) noexcept {
    return {
        static_cast<std::uint8_t>(packed & UINT32_C(0xFF)),
        static_cast<std::uint8_t>((packed >> 8u) & UINT32_C(0xFF)),
        static_cast<std::uint8_t>((packed >> 16u) & UINT32_C(0xFF)),
    };
}

using ColorChangedCallback = void (*)(RgbColor, void* user_data) noexcept;

struct ColorAction {
    ColorChangedCallback callback{};
    void* user_data{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return callback != nullptr;
    }

    void invoke(RgbColor value) const noexcept {
        if (callback) callback(value, user_data);
    }
};

// Host-owned canonical state shared by the public registry, compiled menu,
// and native ColorPicker presentation. The native editor works with an opaque
// packed value, while the public API intentionally exposes named RGB channels.
class ColorPickerState {
public:
    explicit ColorPickerState(RgbColor initial_value) noexcept;

    [[nodiscard]] RgbColor value() const noexcept;
    [[nodiscard]] std::uint32_t native_value() const noexcept;

    // Programmatic writes and accepted player edits both publish canonical
    // state atomically. The caller decides whether an accepted change should
    // notify client code; setters never synthesize callbacks.
    [[nodiscard]] bool set_programmatic(RgbColor value) noexcept;
    [[nodiscard]] bool accept_native(
        std::uint32_t packed_value,
        RgbColor& accepted_value) noexcept;

    void request_preview_refresh() noexcept;
    [[nodiscard]] bool consume_preview_refresh() noexcept;

private:
    std::atomic<std::uint32_t> packed_value_{};
    std::atomic_bool preview_refresh_pending_{};
};

} // namespace erui::detail
