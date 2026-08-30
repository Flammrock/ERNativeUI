#include "native_text_result.hpp"

#include <array>
#include "test_assertions.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::size_t kDirectTextOffset = 0x00;
constexpr std::size_t kTextStorageOffset = 0x10;
constexpr std::size_t kLengthOffset = 0x20;
constexpr std::size_t kCapacityOffset = 0x28;

template <typename T>
void store(std::array<std::byte, 0x30>& object, std::size_t offset, T value) {
    std::memcpy(object.data() + offset, &value, sizeof(value));
}

template <typename T>
T load(const std::array<std::byte, 0x30>& object, std::size_t offset) {
    T value{};
    std::memcpy(&value, object.data() + offset, sizeof(value));
    return value;
}

bool same_text(const wchar_t* left, const wchar_t* right) {
    while (*left != L'\0' || *right != L'\0') {
        if (*left++ != *right++) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    {
        constexpr wchar_t replacement[] = L"Demo";
        const auto null_object = erui::detail::patch_native_text_result(
            nullptr, replacement);
        const auto null_text = erui::detail::patch_native_text_result(
            reinterpret_cast<void*>(0x1), nullptr);
        ERUI_TEST_CHECK(null_object.status == erui::detail::NativeTextPatchStatus::invalid_argument);
        ERUI_TEST_CHECK(null_text.status == erui::detail::NativeTextPatchStatus::invalid_argument);
    }

    {
        alignas(8) std::array<std::byte, 0x30> object{};
        store<std::uint64_t>(object, kCapacityOffset, 7);

        constexpr wchar_t replacement[] = L"Demo";
        const auto outcome = erui::detail::patch_native_text_result(
            object.data(), replacement);

        ERUI_TEST_CHECK(outcome.status == erui::detail::NativeTextPatchStatus::direct_pointer_and_buffer);
        ERUI_TEST_CHECK(load<const wchar_t*>(object, kDirectTextOffset) == replacement);
        ERUI_TEST_CHECK(load<std::uint64_t>(object, kLengthOffset) == 4);
        const auto* inline_text = reinterpret_cast<const wchar_t*>(
            object.data() + kTextStorageOffset);
        ERUI_TEST_CHECK(same_text(inline_text, L"Demo"));
    }

    {
        alignas(8) std::array<std::byte, 0x30> object{};
        wchar_t heap_buffer[64]{};
        store<wchar_t*>(object, kTextStorageOffset, heap_buffer);
        store<std::uint64_t>(object, kCapacityOffset, 63);

        constexpr wchar_t replacement[] = L"Custom help message";
        const auto outcome = erui::detail::patch_native_text_result(
            object.data(), replacement);

        ERUI_TEST_CHECK(outcome.status == erui::detail::NativeTextPatchStatus::direct_pointer_and_buffer);
        ERUI_TEST_CHECK(same_text(heap_buffer, replacement));
    }

    {
        alignas(8) std::array<std::byte, 0x30> object{};
        store<std::uint64_t>(object, kCapacityOffset, 1);

        constexpr wchar_t replacement[] = L"Long replacement";
        const auto outcome = erui::detail::patch_native_text_result(
            object.data(), replacement);

        ERUI_TEST_CHECK(outcome.status == erui::detail::NativeTextPatchStatus::direct_pointer_only);
        ERUI_TEST_CHECK(load<const wchar_t*>(object, kDirectTextOffset) == replacement);
    }

    {
        alignas(8) std::array<std::byte, 0x30> object{};
        store<std::uint64_t>(object, kCapacityOffset, 1ull << 40);

        constexpr wchar_t replacement[] = L"Safe pointer fallback";
        const auto outcome = erui::detail::patch_native_text_result(
            object.data(), replacement);

        ERUI_TEST_CHECK(outcome.status == erui::detail::NativeTextPatchStatus::direct_pointer_only);
        ERUI_TEST_CHECK(load<const wchar_t*>(object, kDirectTextOffset) == replacement);
    }

    return 0;
}
