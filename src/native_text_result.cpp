#include "native_text_result.hpp"

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace erui::detail {
namespace {

constexpr std::size_t kDirectTextOffset = 0x00;
constexpr std::size_t kTextStorageOffset = 0x10;
constexpr std::size_t kLengthOffset = 0x20;
constexpr std::size_t kCapacityOffset = 0x28;
constexpr std::uint64_t kInlineCapacity = 7;
constexpr std::uint64_t kMaximumReasonableCapacity = 1u << 20;

static_assert(sizeof(void*) == 8, "Elden Ring native text layout requires x64");
static_assert(sizeof(wchar_t) == 2, "Elden Ring native text layout requires Windows UTF-16 wchar_t");

template <typename T>
T read_native_field(const std::byte* base, std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

template <typename T>
void write_native_field(std::byte* base, std::size_t offset, const T& value) noexcept {
    std::memcpy(base + offset, &value, sizeof(value));
}

std::size_t utf16_length(const wchar_t* text) noexcept {
    std::size_t length = 0;
    while (text[length] != L'\0') {
        ++length;
    }
    return length;
}

} // namespace

NativeTextPatchOutcome patch_native_text_result(
    void* native_result,
    const wchar_t* replacement) noexcept {
    NativeTextPatchOutcome outcome{};
    if (!native_result || !replacement) {
        return outcome;
    }

#if defined(_MSC_VER) && defined(_WIN32)
    __try {
#endif
        auto* base = static_cast<std::byte*>(native_result);
        const std::size_t length = utf16_length(replacement);
        const std::uint64_t capacity =
            read_native_field<std::uint64_t>(base, kCapacityOffset);

        // The high-level UI resolver keeps its resolved pointer at +0x00.
        // The observed native custom-text path updates this field before
        // refreshing the backing string storage.
        write_native_field(base, kDirectTextOffset, replacement);

        outcome.status = NativeTextPatchStatus::direct_pointer_only;
        outcome.text_length = length;
        outcome.native_capacity = capacity;

        if (capacity > kMaximumReasonableCapacity || capacity < length) {
            return outcome;
        }

        wchar_t* destination = reinterpret_cast<wchar_t*>(base + kTextStorageOffset);
        if (capacity > kInlineCapacity) {
            destination = read_native_field<wchar_t*>(base, kTextStorageOffset);
        }
        if (!destination) {
            return outcome;
        }

        std::memcpy(destination, replacement, (length + 1) * sizeof(wchar_t));
        const std::uint64_t native_length = static_cast<std::uint64_t>(length);
        write_native_field(base, kLengthOffset, native_length);
        outcome.status = NativeTextPatchStatus::direct_pointer_and_buffer;
        return outcome;
#if defined(_MSC_VER) && defined(_WIN32)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        outcome.status = NativeTextPatchStatus::access_violation;
        return outcome;
    }
#endif
}

} // namespace erui::detail
