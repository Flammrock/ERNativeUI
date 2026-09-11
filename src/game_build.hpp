#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace erui::native {

// Private native-integration identity. This is deliberately separate from
// both the ERNativeUI release version and the public ERUI API version.
enum class EldenRingBuild : std::uint8_t {
    unsupported,
    v2_7_0_0,
    v2_7_1_0,
};

struct EldenRingBuildIdentity {
    EldenRingBuild build{};
    std::string_view product_version{};
    std::uint32_t pe_timestamp{};
    std::size_t image_size{};
};

inline constexpr std::array<EldenRingBuildIdentity, 2>
    kSupportedEldenRingBuilds{{
        {
            EldenRingBuild::v2_7_0_0,
            "2.7.0.0",
            0x69E9C9B9,
            0x5E09600,
        },
        {
            EldenRingBuild::v2_7_1_0,
            "2.7.1.0",
            0x6A96B418,
            0x5E0DA00,
        },
    }};

[[nodiscard]] constexpr const EldenRingBuildIdentity*
find_elden_ring_build(
    std::uint32_t pe_timestamp,
    std::size_t image_size) noexcept {
    for (const EldenRingBuildIdentity& identity :
         kSupportedEldenRingBuilds) {
        if (identity.pe_timestamp == pe_timestamp &&
            identity.image_size == image_size) {
            return &identity;
        }
    }
    return nullptr;
}

[[nodiscard]] constexpr EldenRingBuild identify_elden_ring_build(
    std::uint32_t pe_timestamp,
    std::size_t image_size) noexcept {
    const EldenRingBuildIdentity* const identity =
        find_elden_ring_build(pe_timestamp, image_size);
    return identity ? identity->build : EldenRingBuild::unsupported;
}

[[nodiscard]] constexpr std::string_view elden_ring_build_name(
    EldenRingBuild build) noexcept {
    for (const EldenRingBuildIdentity& identity :
         kSupportedEldenRingBuilds) {
        if (identity.build == build) return identity.product_version;
    }
    return "unsupported";
}

} // namespace erui::native
