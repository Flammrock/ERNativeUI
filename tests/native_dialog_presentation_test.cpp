#include "native_dialog_presentation.hpp"

#include "test_assertions.hpp"

#include <array>
#include <cstdint>

namespace {

struct ExpectedLayout {
    ERUI_AlertButtons buttons;
    std::uint32_t primary_label;
    std::uint32_t secondary_label;
    std::uint32_t button_count;
};

constexpr std::array<ExpectedLayout, 7> kLayouts{{
    {ERUI_ALERT_BUTTONS_OK, 1, 0, 1},
    {ERUI_ALERT_BUTTONS_CANCEL, 2, 0, 1},
    {ERUI_ALERT_BUTTONS_YES, 3, 0, 1},
    {ERUI_ALERT_BUTTONS_NO, 4, 0, 1},
    {ERUI_ALERT_BUTTONS_OK_CANCEL, 1, 2, 2},
    {ERUI_ALERT_BUTTONS_YES_NO, 3, 4, 2},
    {ERUI_ALERT_BUTTONS_DISMISS_ONLY, 0, 0, 0},
}};

} // namespace

int main() {
    for (const ExpectedLayout& expected : kLayouts) {
        for (const ERUI_AlertPlacement placement : {
                 ERUI_ALERT_PLACEMENT_BOTTOM,
                 ERUI_ALERT_PLACEMENT_CENTER}) {
            erui::native::NativeDialogPresentation presentation{};
            ERUI_TEST_CHECK(erui::native::resolve_native_dialog_presentation(
                expected.buttons, placement, presentation));
            ERUI_TEST_CHECK(
                presentation.primary_label == expected.primary_label);
            ERUI_TEST_CHECK(
                presentation.secondary_label == expected.secondary_label);
            ERUI_TEST_CHECK(
                presentation.button_count == expected.button_count);
            constexpr std::array<std::uint32_t, 3> bottom_builders{9, 7, 8};
            constexpr std::array<std::uint32_t, 3> center_builders{6, 1, 2};
            const auto& builders = placement == ERUI_ALERT_PLACEMENT_BOTTOM
                ? bottom_builders
                : center_builders;
            ERUI_TEST_CHECK(
                presentation.builder_kind == builders[expected.button_count]);
        }
    }

    erui::native::NativeDialogPresentation presentation{};
    ERUI_TEST_CHECK(!erui::native::resolve_native_dialog_presentation(
        static_cast<ERUI_AlertButtons>(99),
        ERUI_ALERT_PLACEMENT_BOTTOM,
        presentation));
    ERUI_TEST_CHECK(!erui::native::resolve_native_dialog_presentation(
        ERUI_ALERT_BUTTONS_OK,
        static_cast<ERUI_AlertPlacement>(99),
        presentation));

    using erui::native::normalize_native_alert_response;
    ERUI_TEST_CHECK(normalize_native_alert_response(0, 1) ==
        ERUI_ALERT_RESPONSE_NONE);
    ERUI_TEST_CHECK(normalize_native_alert_response(0, 2) ==
        ERUI_ALERT_RESPONSE_DISMISSED);
    ERUI_TEST_CHECK(normalize_native_alert_response(0, 3) ==
        ERUI_ALERT_RESPONSE_DISMISSED);
    ERUI_TEST_CHECK(normalize_native_alert_response(0, 4) ==
        ERUI_ALERT_RESPONSE_NONE);
    ERUI_TEST_CHECK(normalize_native_alert_response(1, 2) ==
        ERUI_ALERT_RESPONSE_PRIMARY);
    ERUI_TEST_CHECK(normalize_native_alert_response(1, 3) ==
        ERUI_ALERT_RESPONSE_PRIMARY);
    ERUI_TEST_CHECK(normalize_native_alert_response(1, 4) ==
        ERUI_ALERT_RESPONSE_NONE);
    ERUI_TEST_CHECK(normalize_native_alert_response(2, 2) ==
        ERUI_ALERT_RESPONSE_PRIMARY);
    ERUI_TEST_CHECK(normalize_native_alert_response(2, 3) ==
        ERUI_ALERT_RESPONSE_SECONDARY);
    ERUI_TEST_CHECK(normalize_native_alert_response(2, 4) ==
        ERUI_ALERT_RESPONSE_NONE);
    ERUI_TEST_CHECK(normalize_native_alert_response(3, 2) ==
        ERUI_ALERT_RESPONSE_NONE);
    return 0;
}
