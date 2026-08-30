#pragma once

#include <cstddef>
#include <cstdint>

namespace erui::detail {

enum class NativeTextPatchStatus : std::uint8_t {
    invalid_argument,
    direct_pointer_only,
    direct_pointer_and_buffer,
    access_violation,
};

struct NativeTextPatchOutcome {
    NativeTextPatchStatus status{NativeTextPatchStatus::invalid_argument};
    std::size_t text_length{};
    std::uint64_t native_capacity{};
};

// Patches the native text-result layout used by Elden Ring's current
// five-argument native UI text resolver. This is intentionally separate from
// MsgRepository::LookupEntry, which returns const wchar_t* directly. The
// replacement pointer must remain valid while the native menu can render it.
[[nodiscard]] NativeTextPatchOutcome patch_native_text_result(
    void* native_result,
    const wchar_t* replacement) noexcept;

} // namespace erui::detail
