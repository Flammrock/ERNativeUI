#include "text_input_state.hpp"

#include <stdexcept>
#include <utility>

namespace erui::detail {

TextInputState::TextInputState(
    std::wstring initial_value,
    std::wstring placeholder,
    std::uint32_t maximum_length)
    : value_(std::move(initial_value)),
      placeholder_(std::move(placeholder)),
      maximum_length_(maximum_length) {
    if (maximum_length_ == 0 ||
        maximum_length_ > text_input_maximum_length ||
        value_.size() > maximum_length_) {
        throw std::invalid_argument("invalid TextInput state");
    }
}

TextInputState::Snapshot TextInputState::snapshot() const {
    std::lock_guard lock(mutex_);
    return {.value = value_, .revision = revision_};
}

bool TextInputState::set_programmatic(std::wstring_view value) {
    if (value.size() > maximum_length_) return false;
    std::lock_guard lock(mutex_);
    if (value_ == value) return true;
    value_.assign(value);
    ++revision_;
    return true;
}

bool TextInputState::commit_native(std::wstring_view value) {
    if (value.size() > maximum_length_) return false;
    // Build the owned snapshot before changing canonical state. If allocation
    // fails, the caller can recover without observing a value whose callback
    // was silently lost.
    std::wstring committed(value);
    std::lock_guard lock(mutex_);
    if (value_ == value) return true;
    committed_changes_.push_back(committed);
    value_.swap(committed);
    ++revision_;
    return true;
}

bool TextInputState::pop_committed_change(std::wstring& value) {
    std::lock_guard lock(mutex_);
    if (committed_changes_.empty()) return false;
    value = std::move(committed_changes_.front());
    committed_changes_.pop_front();
    return true;
}

} // namespace erui::detail
