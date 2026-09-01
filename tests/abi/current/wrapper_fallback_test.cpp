#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <cstdio>

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
};

void ERUI_CALL button_pressed(void* user_data) noexcept {
    auto* observation = static_cast<Observation*>(user_data);
    if (observation) ++observation->button_calls;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_control != nullptr);
    ERUI_CompatControl control{};
    control.size = sizeof(control);
    CHECK(host.get_control(ERUI_COMPAT_CONTROL_VERSION, &control) == ERUI_OK);
    CHECK(control.reset != nullptr && control.trigger_button != nullptr);
    control.reset();

    Observation observation{};
    erui::RowHandle button{};
    bool text_input_reported{};
    erui::ProviderOptions options{};
    options.provider_id = "tests.current-wrapper-to-v1-0";
    options.display_name = L"Current wrapper fallback";
    options.owner_module = GetModuleHandleW(nullptr);

    auto result = erui::register_menu(options, [&](erui::Menu& menu) {
#if defined(ERUI_API_VERSION_1_1)
        text_input_reported = menu.supports(erui::Capability::text_input);
#endif
        button = menu.root().add_button(
            L"Fallback button", L"Existing functionality remains available.",
            &button_pressed, &observation);
    });
    CHECK(result.success());
    CHECK(button != ERUI_INVALID_ROW);
    CHECK(!text_input_reported);
#if defined(ERUI_API_VERSION_1_1)
    CHECK(result.value().api_version() == ERUI_API_VERSION_1_0);
    CHECK(!result.value().supports(erui::Capability::text_input));
#endif
    CHECK(control.trigger_button(button) == ERUI_OK);
    CHECK(observation.button_calls == 1);

    erui_unload_test_host(&host);
    return 0;
}
