#include "color_picker_state.hpp"

#include "test_assertions.hpp"

#include <cstdint>

int main() {
    using erui::detail::ColorPickerState;
    using erui::detail::RgbColor;
    using erui::detail::pack_native_color;
    using erui::detail::unpack_native_color;

    constexpr RgbColor boundary{0u, 127u, 255u};
    static_assert(pack_native_color(boundary) == UINT32_C(0xFFFF7F00));
    static_assert(unpack_native_color(UINT32_C(0x12FF7F00)) == boundary);

    ColorPickerState first{boundary};
    ColorPickerState second{{255u, 1u, 0u}};
    ERUI_TEST_CHECK(first.value() == boundary);
    ERUI_TEST_CHECK(first.native_value() == UINT32_C(0xFFFF7F00));
    ERUI_TEST_CHECK(second.value() == (RgbColor{255u, 1u, 0u}));
    ERUI_TEST_CHECK(!first.consume_preview_refresh());
    ERUI_TEST_CHECK(!second.consume_preview_refresh());

    // Identical programmatic writes are no-ops and do not request repaint.
    ERUI_TEST_CHECK(!first.set_programmatic(boundary));
    ERUI_TEST_CHECK(!first.consume_preview_refresh());

    // Programmatic state is row-local and queues one safe-frame repaint.
    constexpr RgbColor programmed{10u, 20u, 30u};
    ERUI_TEST_CHECK(first.set_programmatic(programmed));
    ERUI_TEST_CHECK(first.value() == programmed);
    ERUI_TEST_CHECK(second.value() == (RgbColor{255u, 1u, 0u}));
    ERUI_TEST_CHECK(first.consume_preview_refresh());
    ERUI_TEST_CHECK(!first.consume_preview_refresh());

    // Cancel is represented by no accepted native value. A programmatic write
    // made while an editor is open therefore remains canonical.
    constexpr RgbColor while_open{40u, 50u, 60u};
    ERUI_TEST_CHECK(first.set_programmatic(while_open));
    ERUI_TEST_CHECK(first.value() == while_open);
    ERUI_TEST_CHECK(first.consume_preview_refresh());

    // A later confirmation wins, normalizes alpha to opaque, and reports a
    // change only when its RGB channels differ from canonical state.
    RgbColor accepted{};
    ERUI_TEST_CHECK(first.accept_native(UINT32_C(0x01785634), accepted));
    ERUI_TEST_CHECK(accepted == (RgbColor{0x34u, 0x56u, 0x78u}));
    ERUI_TEST_CHECK(first.value() == accepted);
    ERUI_TEST_CHECK(first.native_value() == UINT32_C(0xFF785634));
    ERUI_TEST_CHECK(first.consume_preview_refresh());

    ERUI_TEST_CHECK(!first.accept_native(UINT32_C(0x00785634), accepted));
    ERUI_TEST_CHECK(accepted == (RgbColor{0x34u, 0x56u, 0x78u}));
    ERUI_TEST_CHECK(first.native_value() == UINT32_C(0xFF785634));
    ERUI_TEST_CHECK(first.consume_preview_refresh());

    return 0;
}
