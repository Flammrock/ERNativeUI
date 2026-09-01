#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>

namespace erui::detail {

inline constexpr std::uint32_t text_input_default_maximum_length = 16;
inline constexpr std::uint32_t text_input_maximum_length = 35;
inline constexpr std::size_t native_menu_string_storage_size = 0x40;

// Host-owned state shared by the public registry, the compiled menu, and the
// native TextInput adapter. Registry rows are heap-stable and retained for the
// process lifetime, so native controllers may safely keep pointers to both
// this object and native_value.
class TextInputState {
public:
    struct Snapshot {
        std::wstring value{};
        std::uint64_t revision{};
    };

    TextInputState(
        std::wstring initial_value,
        std::wstring placeholder,
        std::uint32_t maximum_length);

    TextInputState(const TextInputState&) = delete;
    TextInputState& operator=(const TextInputState&) = delete;

    [[nodiscard]] std::uint32_t maximum_length() const noexcept {
        return maximum_length_;
    }
    [[nodiscard]] const std::wstring& placeholder() const noexcept {
        return placeholder_;
    }

    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool set_programmatic(std::wstring_view value);

    // Called only after the native editor has committed to native_value. A
    // changed value is queued for the host worker; an identical confirmation
    // intentionally produces no callback.
    [[nodiscard]] bool commit_native(std::wstring_view value);
    [[nodiscard]] bool pop_committed_change(std::wstring& value);

    // Opaque CS::MenuString storage. Only native_text_input.cpp constructs,
    // reads, or destroys this exact-build object.
    alignas(16) std::array<std::byte, native_menu_string_storage_size>
        native_value{};

private:
    mutable std::mutex mutex_{};
    std::wstring value_{};
    const std::wstring placeholder_{};
    const std::uint32_t maximum_length_{};
    std::uint64_t revision_{1};
    std::deque<std::wstring> committed_changes_{};
};

} // namespace erui::detail
