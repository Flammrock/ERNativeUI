#include <ernativeui/ERNativeUI.hpp>

#include "test_assertions.hpp"
#include <iterator>
#include <type_traits>

namespace {
void action() noexcept {}
void ERUI_CALL changed(void*, std::uint8_t) noexcept {}
struct AlertObservation {
    ERUI_Result result{ERUI_INTERNAL_ERROR};
    erui::AlertResponse response{erui::AlertResponse::none};
    bool called{};
};
void ERUI_CALL alert_completed(
    void* context,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    auto* const observation = static_cast<AlertObservation*>(context);
    observation->result = result;
    observation->response = response;
    observation->called = true;
}
ERUI_Result ERUI_CALL format_title(
    const erui::PageTitleFormatContext*,
    std::uint16_t* output,
    std::uint32_t capacity,
    std::uint32_t* length) noexcept {
    return erui::write_page_title(L"Static title", output, capacity, length);
}
}

int main() {
    static_assert(__cplusplus >= 201703L);
    static_assert(sizeof(ERUI_StringView) == 16);
    static_assert(sizeof(ERUI_Utf16View) == 16);
    static_assert(sizeof(ERUI_GameLanguageInfo) == 32);
    static_assert(sizeof(ERUI_ProviderDesc) == 56);
    static_assert(sizeof(ERUI_ButtonDesc) == 64);
    static_assert(sizeof(ERUI_ToggleDesc) == 64);
    static_assert(sizeof(ERUI_SliderDesc) == 80);
    static_assert(sizeof(ERUI_ChoiceDesc) == 72);
    static_assert(sizeof(ERUI_TextInputChangeContext) == 48);
    static_assert(sizeof(ERUI_TextInputDesc) == 96);
    static_assert(sizeof(ERUI_SubmenuDesc) == 80);
    static_assert(sizeof(ERUI_PageTitleFormatContext) == 56);
    static_assert(sizeof(ERUI_PagePresentationDesc) == 64);
    static_assert(sizeof(ERUI_AlertDesc) == 48);
    static_assert(sizeof(ERUI_Api) == 152);
    static_assert(ERUI_API_V1_0_SIZE == 128);
    static_assert(ERUI_API_V1_1_SIZE == 152);
    static_assert(std::is_trivially_copyable<ERUI_Api>::value);
    static_assert(std::is_pointer<erui::ButtonCallback>::value);
    static_assert(std::is_pointer<erui::AlertCallback>::value);
    static_assert(sizeof(erui::AlertOptions) == sizeof(std::uint32_t) * 2);
    static_assert(std::is_same<
        std::underlying_type<erui::AlertResponse>::type,
        std::uint32_t>::value);
    static_assert(static_cast<std::uint32_t>(erui::AlertButtons::dismiss_only) ==
        ERUI_ALERT_BUTTONS_DISMISS_ONLY);
    static_assert(static_cast<std::uint32_t>(erui::AlertPlacement::center) ==
        ERUI_ALERT_PLACEMENT_CENTER);
    static_assert(static_cast<std::uint32_t>(erui::AlertResponse::dismissed) ==
        ERUI_ALERT_RESPONSE_DISMISSED);

    erui::ProviderOptions provider{};
    provider.provider_id = "com.example.compile-test";
    provider.display_name = L"Compile Test";
    erui::SliderOptions slider{};
    slider.initial_value = 50;
    ERUI_TEST_CHECK(provider.connect_timeout.count() == 5000);
    ERUI_TEST_CHECK(slider.maximum == 100);
    erui::Page page{};
    const std::wstring_view choices[] = {L"One", L"Two"};
    erui::ChoiceOptions choice{};
    choice.values = choices;
    choice.count = std::size(choices);
    choice.initial_index = 1;
    ERUI_TEST_CHECK(page.add_inline_choice(
        L"Inline", L"", choice, &changed) == ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(page.add_popup_choice(
        L"Popup", L"", choice, &changed) == ERUI_INVALID_ROW);
    page.set_presentation<&format_title>(L"Outer", L"Base");
    erui::PagePresentation presentation{};
    presentation.menu_title = L"Outer";
    presentation.page_title = L"Base";
    ERUI_TEST_CHECK(presentation.formatter == nullptr);
    erui::Registration registration{};
    ERUI_TEST_CHECK(!registration.game_language().available());
    ERUI_TEST_CHECK(registration.game_language().known ==
        erui::GameLanguage::unknown);
    ERUI_TEST_CHECK(registration.alert(L"Compile test") == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registration.alert(L"Compile test", &alert_completed) ==
        ERUI_INVALID_HANDLE);
    erui::AlertOptions alert_options{};
    ERUI_TEST_CHECK(alert_options.buttons == erui::AlertButtons::ok);
    ERUI_TEST_CHECK(alert_options.placement == erui::AlertPlacement::bottom);
    alert_options.buttons = erui::AlertButtons::yes_no;
    alert_options.placement = erui::AlertPlacement::center;
    ERUI_TEST_CHECK(registration.alert(
        L"Compile test", alert_options, &alert_completed) == ERUI_INVALID_HANDLE);

    AlertObservation observation{};
    auto* callback_state = new erui::detail::AlertCallbackState{
        &alert_completed, &observation};
    erui::detail::alert_callback_thunk(
        callback_state, ERUI_OK, ERUI_ALERT_RESPONSE_SECONDARY);
    ERUI_TEST_CHECK(observation.called);
    ERUI_TEST_CHECK(observation.result == ERUI_OK);
    ERUI_TEST_CHECK(observation.response == erui::AlertResponse::secondary);
    (void)&action;
    (void)&changed;
    return 0;
}
