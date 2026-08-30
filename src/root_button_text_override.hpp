#pragma once

#include <cstdint>

namespace erui::native {

// The native Controller-page action-row constructor requests these two fixed
// messages while building its richer left-side label/help object. Restrict
// substitution to the constructing thread so vanilla rows remain untouched.
constexpr std::uint32_t root_button_native_label_id = 0x1B199;
constexpr std::uint32_t root_button_native_help_id = 0x0BC2;

struct RootButtonTextOverride {
    std::uint32_t label_id{};
    std::uint32_t help_id{};
    bool active{};
};

inline thread_local RootButtonTextOverride root_button_text_override{};

} // namespace erui::native
