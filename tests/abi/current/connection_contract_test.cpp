#include "connection_control.h"
#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

template <typename Function>
Function resolve(HMODULE module, const char* name) noexcept {
    const FARPROC symbol = GetProcAddress(module, name);
    Function function{};
    if (!symbol || sizeof(symbol) != sizeof(function)) return function;
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

ERUI_TestConnectionConfig configuration(
    std::uint32_t not_ready_responses,
    ERUI_Result terminal_result,
    ERUI_Result language_result) noexcept {
    ERUI_TestConnectionConfig result{};
    result.size = sizeof(result);
    result.version = ERUI_TEST_CONNECTION_CONTROL_VERSION;
    result.not_ready_responses = not_ready_responses;
    result.terminal_result = terminal_result;
    result.language_result = language_result;
    return result;
}

ERUI_TestConnectionSnapshot snapshot(
    ERUI_TestGetConnectionSnapshotFn get_snapshot) {
    ERUI_TestConnectionSnapshot result{};
    result.size = sizeof(result);
    if (get_snapshot(&result) != ERUI_OK) std::abort();
    return result;
}

void ERUI_CALL raw_binding_activated(
    void*,
    const ERUI_InputActionActivatedContext*) noexcept {}

void binding_activated(const erui::ActionActivation&) noexcept {}

void stateful_binding_activated(
    unsigned& calls,
    const erui::ActionActivation&) noexcept {
    ++calls;
}

unsigned g_binding_calls{};

} // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    const auto configure = resolve<ERUI_TestConfigureConnectionFn>(
        host.module, "ERUI_TestConfigureConnection");
    const auto get_snapshot = resolve<ERUI_TestGetConnectionSnapshotFn>(
        host.module, "ERUI_TestGetConnectionSnapshot");
    CHECK(configure != nullptr);
    CHECK(get_snapshot != nullptr);

    {
        const auto config = configuration(2, ERUI_OK, ERUI_OK);
        CHECK(configure(&config) == ERUI_OK);

        auto connected = erui::connect(250ms);
        CHECK(connected.success());
        CHECK(connected.value().valid());
        CHECK(connected.value().api_version() == ERUI_API_VERSION_1_1);
        CHECK(connected.value().supports(erui::Capability::text_input));
        CHECK(connected.value().supports(erui::Capability::color_picker));
        CHECK(connected.value().supports(erui::Capability::builtin_pages));
        CHECK(connected.value().supports(erui::Capability::input_bindings));
        CHECK(connected.value().supports(erui::Capability::storage));

        auto observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 3);
        CHECK(observed.language_calls == 0);
        CHECK(observed.register_calls == 0);

        const erui::LanguageInfo language = connected.value().game_language();
        CHECK(language.result == ERUI_OK);
        CHECK(language.known == erui::GameLanguage::english);
        CHECK(language.identifier == "english");
        observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 3);
        CHECK(observed.language_calls == 1);

        bool invalid_builder_called = false;
        erui::ProviderOptions invalid_provider{};
        invalid_provider.provider_id = "invalid/provider";
        invalid_provider.display_name = L"Invalid Provider";
        invalid_provider.owner_module = GetModuleHandleW(nullptr);
        const erui::RegistrationResult invalid_registration =
            connected.value().register_menu(
                invalid_provider,
                [&](erui::Menu&) noexcept {
                    invalid_builder_called = true;
                });
        CHECK(!invalid_registration.success());
        CHECK(invalid_registration.error().native_result() ==
            ERUI_INVALID_ARGUMENT);
        CHECK(!invalid_builder_called);
        observed = snapshot(get_snapshot);
        CHECK(observed.register_calls == 0);

        bool builder_called = false;
        bool builtin_page_valid = false;
        bool binding_section_valid = false;
        ERUI_InputActionHandle raw_binding = ERUI_INVALID_INPUT_ACTION;
        ERUI_InputActionHandle static_binding = ERUI_INVALID_INPUT_ACTION;
        ERUI_InputActionHandle stateful_binding = ERUI_INVALID_INPUT_ACTION;
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.explicit-connection";
        provider.display_name = L"Explicit Connection";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                builder_called = true;
                builtin_page_valid = menu.page(erui::BuiltinPage::sound).valid();
                auto section = menu.input_bindings().add_section(
                    L"ERNativeUI tests");
                binding_section_valid = section.valid();
                raw_binding = section.add_action(
                    "raw-binding",
                    L"Raw binding",
                    erui::ActionInputs::all(),
                    &raw_binding_activated).handle();
                static_binding = section.add_action<&binding_activated>(
                    "static-binding",
                    L"Static binding").handle();
                stateful_binding = section.add_action<
                    &stateful_binding_activated>(
                    "stateful-binding",
                    L"Stateful binding",
                    erui::ActionInputs::all(),
                    g_binding_calls).handle();
            });
        CHECK(registered.success());
        CHECK(builder_called);
        CHECK(builtin_page_valid);
        CHECK(binding_section_valid);
        CHECK(raw_binding != ERUI_INVALID_INPUT_ACTION);
        CHECK(static_binding != ERUI_INVALID_INPUT_ACTION);
        CHECK(stateful_binding != ERUI_INVALID_INPUT_ACTION);
        CHECK(raw_binding != static_binding);
        CHECK(raw_binding != stateful_binding);
        CHECK(static_binding != stateful_binding);

        observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 3);
        CHECK(observed.language_calls == 1);
        CHECK(observed.register_calls == 1);
        CHECK(observed.commit_calls == 1);
        CHECK(observed.abort_calls == 0);
    }

    {
        const auto config = configuration(0, ERUI_UNSUPPORTED_VERSION,
            ERUI_NOT_SUPPORTED);
        CHECK(configure(&config) == ERUI_OK);
        const auto connected = erui::connect(0ms);
        CHECK(!connected.success());
        CHECK(connected.error().code() == erui::ErrorCode::incompatible_api);
        CHECK(connected.error().native_result() == ERUI_UNSUPPORTED_VERSION);
        const auto observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 1);
        CHECK(observed.language_calls == 0);
        CHECK(observed.register_calls == 0);
    }

    {
        const auto config = configuration(1000, ERUI_OK, ERUI_OK);
        CHECK(configure(&config) == ERUI_OK);
        const auto connected = erui::connect(0ms);
        CHECK(!connected.success());
        CHECK(connected.error().code() == erui::ErrorCode::host_not_ready);
        CHECK(connected.error().native_result() == ERUI_HOST_NOT_READY);
        const auto observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 1);
        CHECK(observed.language_calls == 0);
    }

    {
        const auto config = configuration(
            0, ERUI_HOST_FAILED, ERUI_NOT_SUPPORTED);
        CHECK(configure(&config) == ERUI_OK);
        const auto connected = erui::connect(0ms);
        CHECK(!connected.success());
        CHECK(connected.error().code() == erui::ErrorCode::host_failed);
        CHECK(connected.error().native_result() == ERUI_HOST_FAILED);
        const auto observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 1);
        CHECK(observed.language_calls == 0);
    }

    {
        const auto config = configuration(0, ERUI_OK, ERUI_NOT_SUPPORTED);
        CHECK(configure(&config) == ERUI_OK);
        auto connected = erui::connect(0ms);
        CHECK(connected.success());
        const erui::LanguageInfo language = connected.value().game_language();
        CHECK(language.result == ERUI_NOT_SUPPORTED);
        CHECK(!language.available());
        const auto observed = snapshot(get_snapshot);
        CHECK(observed.get_api_calls == 1);
        CHECK(observed.language_calls == 1);
    }

    erui_unload_test_host(&host);
    return 0;
}
