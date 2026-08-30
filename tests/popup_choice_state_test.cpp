#include "popup_choice_state.hpp"

#include "test_assertions.hpp"

#include <cstdint>

int main() {
    using erui::detail::PopupChoiceNativeState;
    using erui::detail::synchronize_popup_choice_state;

    volatile std::uint8_t public_selection = 0;
    PopupChoiceNativeState native{};
    native.set_public_selection(0);

    ERUI_TEST_CHECK(native.native_selection() == 1);
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 0, 8) == 0);

    // A confirmed native selection is one-based and becomes zero-based at the
    // model/public boundary.
    *native.native_selection_address() = 8;
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 0, 8) == 7);
    ERUI_TEST_CHECK(public_selection == 7);

    // A public write since the last observation wins and updates native state.
    public_selection = 2;
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 7, 8) == 2);
    ERUI_TEST_CHECK(native.native_selection() == 3);

    // Cancel/no commit leaves both sides unchanged.
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 2, 8) == 2);
    ERUI_TEST_CHECK(native.native_selection() == 3);

    // Invalid native bytes are repaired without reporting an invalid index.
    *native.native_selection_address() = 0;
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 2, 8) == 2);
    ERUI_TEST_CHECK(native.native_selection() == 3);
    *native.native_selection_address() = 9;
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 2, 8) == 2);
    ERUI_TEST_CHECK(native.native_selection() == 3);

    // Rows retain independent native storage.
    PopupChoiceNativeState second{};
    second.set_public_selection(1);
    ERUI_TEST_CHECK(second.native_selection() == 2);
    ERUI_TEST_CHECK(native.native_selection() == 3);

    // Structural boundaries accepted by the native 32-slot container.
    public_selection = 0;
    native.set_public_selection(0);
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 0, 1) == 0);
    public_selection = 31;
    ERUI_TEST_CHECK(synchronize_popup_choice_state(
        public_selection, native, 0, 32) == 31);
    ERUI_TEST_CHECK(native.native_selection() == 32);

    return 0;
}
