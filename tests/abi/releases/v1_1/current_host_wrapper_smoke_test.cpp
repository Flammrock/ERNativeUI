#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <chrono>
#include <cstdio>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

void activated(const erui::ActionActivation&) noexcept {}

} // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_1);
    static_assert(sizeof(ERUI_Api) == 320u);
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    const erui::ConnectionResult connected = erui::connect(0ms);
    CHECK(connected.success());
    CHECK(connected.value().api_version() == ERUI_API_VERSION_1_1);

    erui::RowHandle text{};
    erui::RowHandle color{};
    erui::InputAction action{};
    erui::ProviderOptions options{};
    options.provider_id = "tests.v1-1-wrapper-to-current";
    options.display_name = L"Frozen API 1.1 wrapper client";
    options.owner_module = GetModuleHandleW(nullptr);
    const erui::RegistrationResult registered =
        connected.value().register_menu(
            options,
            [&](erui::Menu& menu) noexcept {
                erui::Page sound = menu.page(erui::BuiltinPage::sound);
                erui::TextInputOptions text_options{};
                text_options.initial_value = L"API 1.1";
                text_options.placeholder = L"Enter text";
                text = sound.add_text_input(
                    L"Frozen text", L"ABI smoke test", text_options);

                erui::ColorPickerOptions color_options{};
                color_options.initial_value = erui::Color{1u, 2u, 3u};
                color = sound.add_color_picker(
                    L"Frozen color", L"ABI smoke test", color_options);

                erui::InputSection section =
                    menu.input_bindings().add_section(L"Frozen bindings");
                action = section.add_action<&activated>(
                    "frozen-action",
                    L"Frozen action",
                    erui::inputs::keyboard(erui::KeyboardKey::key_q));
            });
    CHECK(registered.success());
    CHECK(text != ERUI_INVALID_ROW);
    CHECK(color != ERUI_INVALID_ROW);
    CHECK(action.valid());

    erui_unload_test_host(&host);
    return 0;
}
