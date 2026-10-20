#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace erui::detail {

using Sha256Digest = std::array<std::uint8_t, 32>;

// Allocation-free SHA-256 over an immutable byte range. This small internal
// primitive is used for exact runtime asset identity, not for authentication.
[[nodiscard]] Sha256Digest sha256(
    std::span<const std::uint8_t> input) noexcept;

} // namespace erui::detail
