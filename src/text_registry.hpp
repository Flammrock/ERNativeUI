#pragma once

#include "menu.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace erui::detail {

class TextRegistry {
public:
    static constexpr TextId base_id = 0x0F2000;

    TextId add(std::wstring_view text);
    void freeze();

    [[nodiscard]] const wchar_t* lookup(int message_id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }

private:
    std::vector<std::wstring> entries_{};
    bool frozen_{false};
};

} // namespace erui::detail
