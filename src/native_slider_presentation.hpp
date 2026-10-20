#pragma once

#include "addresses.hpp"

#include <cstdint>

namespace erui::native {

// Installs the narrowly scoped SliderCtrl text bridge used by API 1.2
// NumericSlider and CustomSlider rows. Ordinary API 1.0/1.1 byte sliders are
// not registered with this bridge and retain Elden Ring's native rendering.
[[nodiscard]] bool install_native_slider_presentation(
    const GameAddresses& addresses,
    const ModuleView* game,
    const PreservedSharedMenuAddresses* preserved_shared) noexcept;
void remove_native_slider_presentation() noexcept;

// AddSlider refreshes once before storing its bound byte pointer. The native
// row materializer brackets that constructor call so the first Text_0/Text_1
// refresh can still be associated with the exact compiled formatted row.
void begin_native_slider_presentation_row(
    volatile std::uint8_t* bound_value) noexcept;
void end_native_slider_presentation_row() noexcept;

} // namespace erui::native
