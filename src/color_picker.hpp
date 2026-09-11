#pragma once

#include "color_picker_state.hpp"
#include "module.hpp"

#include <ernativeui/erui.h>

namespace erui::native {

// Solid Uncapper uses the same Scaleform property setter as ColorPicker.
// Capture its pristine exact-build entry before the third-party worker can
// install its hook, then approve only a narrowly owned chain after startup.
[[nodiscard]] bool capture_color_picker_visibility_before_third_party_hooks(
    const ModuleView& game) noexcept;
[[nodiscard]] bool approve_captured_color_picker_visibility(
    void* allowed_detour_module) noexcept;

// Scaleform panel that owns the physical row. Built-in Configuration pages
// share the same native row constructors but live under distinct WindowList
// paths; keeping this separate from button text layout prevents Camera's
// special text-reference ABI from being conflated with widget routing.
enum class ColorPickerWidgetHost : unsigned char {
    subpage,
    game_options,
    camera_options,
    display,
    sound,
    network,
    keyboard_mouse,
    graphics,
};

// The ordinary Game Options action controller remains responsible for focus
// and mouse/controller activation. This narrow scope marks the one native
// action-row construction whose Widgets/Button lookup may be redirected to
// the optional standalone Widgets/ColorPicker sibling.
class ColorPickerRowPresentationScope {
public:
    ColorPickerRowPresentationScope(
        void* page,
        detail::ColorPickerState* state,
        ColorPickerWidgetHost host) noexcept;
    ~ColorPickerRowPresentationScope();

    ColorPickerRowPresentationScope(
        const ColorPickerRowPresentationScope&) = delete;
    ColorPickerRowPresentationScope& operator=(
        const ColorPickerRowPresentationScope&) = delete;

private:
    bool active_{};
};

// Shared by the central Scaleform path hook. The override is active only
// during the exact native action-widget producer call for a ColorPicker row.
// The returned pointer has static storage duration.
[[nodiscard]] const char* color_picker_widget_path_override(
    const char* observed_path,
    const void* source_proxy) noexcept;

void set_color_picker_widget_path_bridge_available(bool available) noexcept;
[[nodiscard]] void* color_picker_widget_path_hook_target() noexcept;

// A custom sibling is unknown to Elden Ring's vanilla widget producers. Hide
// every physical instance before materialization so pagination and recycled
// rows cannot retain a ColorPicker selected on an earlier page.
void reset_color_picker_page_widgets(
    void* page,
    ColorPickerWidgetHost host) noexcept;

// Applies programmatic values at the native UI frame boundary. No client
// thread calls into Scaleform directly.
void synchronize_color_picker_previews(void* current_page) noexcept;

[[nodiscard]] bool install_color_picker(
    const ModuleView& game) noexcept;
void remove_color_picker() noexcept;
[[nodiscard]] bool color_picker_available() noexcept;
// True from native job submission until the editor reaches a terminal state
// or its native lifetime ends. The alert transport uses this to serialize the
// two modal systems without rejecting or dropping queued alerts.
[[nodiscard]] bool color_picker_session_active() noexcept;

// Called synchronously by the native action-row callback while its owning
// page is known and alive. Completion is dispatched later on the host worker.
[[nodiscard]] ERUI_Result request_color_picker(
    void* page,
    detail::ColorPickerState& state,
    detail::ColorAction changed_action) noexcept;

// Called after the original MenuWindowJob poll. Kinds 2 and 3 are the native
// observed accept and cancel terminal states for this exact native job.
void observe_color_picker_job_poll(
    void* job,
    void* scheduler_state) noexcept;

void dispatch_color_picker_callbacks() noexcept;

} // namespace erui::native
