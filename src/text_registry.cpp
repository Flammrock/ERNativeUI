#include "text_registry.hpp"

#include <limits>
#include <stdexcept>

namespace erui::detail {

TextId TextRegistry::add(std::wstring_view text) {
    if (frozen_) {
        throw std::logic_error("custom text registry is already frozen");
    }
    if (entries_.size() >=
        static_cast<std::size_t>(std::numeric_limits<TextId>::max() - base_id)) {
        throw std::overflow_error("custom text ID range exhausted");
    }

    entries_.emplace_back(text);
    return base_id + static_cast<TextId>(entries_.size() - 1);
}

void TextRegistry::freeze() {
    if (frozen_) {
        return;
    }
    entries_.shrink_to_fit();
    frozen_ = true;
}

const wchar_t* TextRegistry::lookup(int message_id) const noexcept {
    if (message_id < 0) {
        return nullptr;
    }
    const auto id = static_cast<TextId>(message_id);
    if (id < base_id) {
        return nullptr;
    }

    const auto index = static_cast<std::size_t>(id - base_id);
    if (index >= entries_.size()) {
        return nullptr;
    }
    return entries_[index].c_str();
}

} // namespace erui::detail
