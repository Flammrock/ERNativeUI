#pragma once

#include "addresses.hpp"

#include <ernativeui/erui.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace erui::native {

struct SliderPresentationUpdate {
    volatile std::uint8_t* bound_value{};
    std::vector<std::wstring> values{};
};

// Holds the presentation-state lock after every replacement table has been
// validated and copied. Destroying an uncommitted token changes nothing.
// Committing is allocation-free and only queues the desired generation; the
// existing row-update UI boundary promotes it to the detour-visible state.
class PreparedSliderPresentationUpdates {
public:
    PreparedSliderPresentationUpdates() noexcept;
    ~PreparedSliderPresentationUpdates();
    PreparedSliderPresentationUpdates(
        PreparedSliderPresentationUpdates&&) noexcept;
    PreparedSliderPresentationUpdates& operator=(
        PreparedSliderPresentationUpdates&&) noexcept;
    PreparedSliderPresentationUpdates(
        const PreparedSliderPresentationUpdates&) = delete;
    PreparedSliderPresentationUpdates& operator=(
        const PreparedSliderPresentationUpdates&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_{};

    friend ERUI_Result prepare_native_slider_presentation_updates(
        std::vector<SliderPresentationUpdate>,
        PreparedSliderPresentationUpdates&) noexcept;
    friend void commit_prepared_native_slider_presentation_updates(
        PreparedSliderPresentationUpdates&&) noexcept;
};

[[nodiscard]] ERUI_Result prepare_native_slider_presentation_updates(
    std::vector<SliderPresentationUpdate> updates,
    PreparedSliderPresentationUpdates& output) noexcept;

void commit_prepared_native_slider_presentation_updates(
    PreparedSliderPresentationUpdates&& prepared) noexcept;

// Publishes the newest complete desired tables to the refresh detour. This is
// called only while the shared API 1.4 row-update publication lock is held.
void synchronize_pending_native_slider_presentations() noexcept;

[[nodiscard]] bool native_slider_presentation_updates_pending() noexcept;

// Repaints one materializer-authorized, currently live SliderCtrl from the
// newest applied immutable table. The caller must run on the native UI thread
// and provide the exact backing byte captured for that row. No controller
// pointer is retained by this bridge.
[[nodiscard]] bool refresh_native_slider_presentation(
    void* slider,
    const volatile std::uint8_t* expected_bound_value) noexcept;

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
