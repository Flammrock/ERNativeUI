#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <cstdio>
#include <cstring>
#include <string_view>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

struct Observation {
    int button_calls{};
    int value_calls{};
    int alert_calls{};
    std::uint8_t last_value{};
    ERUI_Result alert_result{};
    erui::AlertResponse alert_response{erui::AlertResponse::none};
};

void ERUI_CALL button_callback(void* user_data) noexcept {
    auto* observation = static_cast<Observation*>(user_data);
    if (observation) ++observation->button_calls;
}

void ERUI_CALL value_callback(
    void* user_data,
    std::uint8_t value) noexcept {
    auto* observation = static_cast<Observation*>(user_data);
    if (!observation) return;
    ++observation->value_calls;
    observation->last_value = value;
}

void ERUI_CALL alert_callback(
    void* user_data,
    ERUI_Result result,
    erui::AlertResponse response) noexcept {
    auto* observation = static_cast<Observation*>(user_data);
    if (!observation) return;
    ++observation->alert_calls;
    observation->alert_result = result;
    observation->alert_response = response;
}

} // namespace

int main(int argc, char** argv) {
    static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_0);
    static_assert(sizeof(ERUI_Api) == 128u);
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_control != nullptr);
    ERUI_CompatControl control{};
    control.size = sizeof(control);
    CHECK(host.get_control(ERUI_COMPAT_CONTROL_VERSION, &control) == ERUI_OK);
    control.reset();

    Observation observation{};
    erui::RowHandle button{};
    erui::RowHandle toggle{};
    erui::RowHandle choice{};
    erui::ProviderOptions options{};
    options.provider_id = "tests.frozen-v1-0-cpp";
    options.display_name = L"Frozen v1.0 C++ client";
    options.owner_module = GetModuleHandleW(nullptr);

    auto result = erui::register_menu(options, [&](erui::Menu& menu) {
        auto root = menu.root();
        button = root.add_button(
            L"Frozen C++ button", L"Button help",
            &button_callback, &observation);
        toggle = root.add_toggle(
            L"Frozen C++ toggle", L"Toggle help", 0,
            &value_callback, &observation);

        erui::SliderOptions slider{};
        slider.minimum = 0;
        slider.maximum = 100;
        slider.step = 5;
        slider.initial_value = 25;
        root.add_slider(
            L"Frozen C++ slider", L"Slider help", slider,
            &value_callback, &observation);

        const std::wstring_view values[] = {L"Alpha", L"Beta"};
        erui::ChoiceOptions choices{};
        choices.values = values;
        choices.count = 2;
        choices.initial_index = 0;
        choice = root.add_popup_choice(
            L"Frozen C++ choice", L"Choice help", choices,
            &value_callback, &observation);
        root.add_submenu(L"Frozen child", L"Child help");
    });
    CHECK(result.success());
    CHECK(button != ERUI_INVALID_ROW);
    CHECK(toggle != ERUI_INVALID_ROW);
    CHECK(choice != ERUI_INVALID_ROW);

    CHECK(control.trigger_button(button) == ERUI_OK);
    CHECK(observation.button_calls == 1);
    CHECK(control.trigger_value(toggle, 1u) == ERUI_OK);
    CHECK(observation.value_calls == 1);
    CHECK(observation.last_value == 1u);

    CHECK(result.value().set_value(toggle, 0u) == ERUI_OK);
    std::uint8_t value = 9u;
    CHECK(result.value().get_value(toggle, value) == ERUI_OK);
    CHECK(value == 0u);

    erui::AlertOptions alert_options{};
    alert_options.buttons = erui::AlertButtons::yes_no;
    alert_options.placement = erui::AlertPlacement::center;
    CHECK(result.value().alert(
        L"Frozen wrapper alert", alert_options,
        &alert_callback, &observation) == ERUI_OK);
    CHECK(control.complete_alert(
        ERUI_OK, ERUI_ALERT_RESPONSE_SECONDARY) == ERUI_OK);
    CHECK(observation.alert_calls == 1);
    CHECK(observation.alert_result == ERUI_OK);
    CHECK(observation.alert_response == erui::AlertResponse::secondary);

    CHECK(result.value().game_language().available());
    CHECK(result.value().game_language().known == erui::GameLanguage::english);
    CHECK(result.value().game_language().identifier == "english");

    std::uint16_t copied[32]{};
    std::uint32_t copied_length{};
    CHECK(control.get_choice_text(
        choice, 1u, copied, 32u, &copied_length) == ERUI_OK);
    CHECK(copied_length == 4u);
    CHECK(std::memcmp(copied, L"Beta", 4u * sizeof(std::uint16_t)) == 0);

    erui_unload_test_host(&host);
    return 0;
}
