#include "connection_control.h"
#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <chrono>
#include <cstdlib>
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
    ERUI_Result language_result,
    std::uint32_t disabled_features = 0) noexcept {
    ERUI_TestConnectionConfig result{};
    result.size = sizeof(result);
    result.version = ERUI_TEST_CONNECTION_CONTROL_VERSION;
    result.not_ready_responses = not_ready_responses;
    result.terminal_result = terminal_result;
    result.language_result = language_result;
    result.reserved[0] = disabled_features;
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

bool control_value_equals(
    const ERUI_TestRowControlValueObservation& observation,
    std::wstring_view expected) noexcept {
    if (observation.length != expected.size()) return false;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (observation.text[index] !=
            static_cast<std::uint16_t>(expected[index])) {
            return false;
        }
    }
    return true;
}

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
    const auto get_row_update = resolve<ERUI_TestGetRowUpdateObservationFn>(
        host.module, "ERUI_TestGetRowUpdateObservation");
    const auto get_row_control_value =
        resolve<ERUI_TestGetRowControlValueObservationFn>(
            host.module, "ERUI_TestGetRowControlValueObservation");
    CHECK(configure != nullptr);
    CHECK(get_snapshot != nullptr);
    CHECK(get_row_update != nullptr);
    CHECK(get_row_control_value != nullptr);

    {
        ERUI_Api capability_gated{};
        capability_gated.size = ERUI_API_V1_3_SIZE;
        CHECK(host.get_api(ERUI_API_VERSION_1_3, &capability_gated) == ERUI_OK);
        CHECK(capability_gated.add_tab != nullptr);
        CHECK(capability_gated.add_title_screen_text != nullptr);
        capability_gated.capabilities &=
            ~(ERUI_CAP_CUSTOM_TABS | ERUI_CAP_TITLE_SCREEN_TEXT);
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_3));
    }

    {
        ERUI_Api capability_gated{};
        capability_gated.size = ERUI_API_V1_4_SIZE;
        CHECK(host.get_api(ERUI_API_VERSION_1_4, &capability_gated) == ERUI_OK);
        CHECK(capability_gated.commit_updates != nullptr);
        capability_gated.capabilities &=
            ~(ERUI_CAP_UPDATES | ERUI_CAP_ROW_AVAILABILITY |
                ERUI_CAP_ROW_VISIBILITY | ERUI_CAP_SUBMENU_UPDATES |
                ERUI_CAP_TAB_UPDATES | ERUI_CAP_ROW_CONFIGURATION);
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_ROW_AVAILABILITY;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_UPDATES;
        capability_gated.capabilities &= ~ERUI_CAP_ROW_AVAILABILITY;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_UPDATES;
        capability_gated.capabilities |= ERUI_CAP_ROW_VISIBILITY;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_UPDATES;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_UPDATES;
        capability_gated.capabilities |= ERUI_CAP_TAB_UPDATES;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_UPDATES;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_CUSTOM_TABS;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_CUSTOM_TABS;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_TAB_UPDATES;
        capability_gated.capabilities &= ~ERUI_CAP_UPDATES;
        capability_gated.capabilities |= ERUI_CAP_SUBMENU_UPDATES;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_UPDATES;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_SUBMENU;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_SUBMENU;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_UPDATES;
        capability_gated.capabilities |= ERUI_CAP_ROW_CONFIGURATION;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_UPDATES;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities &= ~ERUI_CAP_NUMERIC_SLIDER;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_NUMERIC_SLIDER;
        capability_gated.capabilities &= ~ERUI_CAP_CUSTOM_SLIDER;
        CHECK(!erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
        capability_gated.capabilities |= ERUI_CAP_CUSTOM_SLIDER;
        CHECK(erui::detail::api_complete_for_version(
            capability_gated, ERUI_API_VERSION_1_4));
    }

    {
        const auto config = configuration(0, ERUI_OK, ERUI_OK,
            ERUI_TEST_CONNECTION_DISABLE_ROW_VISIBILITY);
        CHECK(configure(&config) == ERUI_OK);
        auto connected = erui::connect(0ms);
        CHECK(connected.success());
        CHECK(connected.value().supports(erui::Capability::updates));
        CHECK(!connected.value().supports(
            erui::Capability::row_visibility));

        erui::ToggleRow toggle{};
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.row-visibility-capability-absent";
        provider.display_name = L"Row visibility capability absent";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                toggle = menu.root().add_toggle(
                    L"Toggle", L"Visibility must be capability gated", 1u);
            });
        CHECK(registered.success());
        CHECK(toggle.valid());
        CHECK(toggle.edit().visible(false).commit() == ERUI_NOT_SUPPORTED);
    }

    {
        const auto config = configuration(0, ERUI_OK, ERUI_OK,
            ERUI_TEST_CONNECTION_DISABLE_ROW_AVAILABILITY);
        CHECK(configure(&config) == ERUI_OK);
        auto connected = erui::connect(0ms);
        CHECK(connected.success());
        CHECK(connected.value().supports(erui::Capability::updates));
        CHECK(!connected.value().supports(
            erui::Capability::row_availability));

        erui::ToggleRow toggle{};
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.row-availability-capability-absent";
        provider.display_name = L"Row availability capability absent";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                toggle = menu.root().add_toggle(
                    L"Toggle", L"Availability must be capability gated", 1u);
            });
        CHECK(registered.success());
        CHECK(toggle.valid());
        CHECK(toggle.edit().interactive(false).commit() ==
            ERUI_NOT_SUPPORTED);
        CHECK(toggle.edit()
            .availability_appearance(
                erui::AvailabilityAppearance::disabled)
            .commit() == ERUI_NOT_SUPPORTED);
        CHECK(toggle.edit().help(L"Text updates remain available").commit() ==
            ERUI_OK);
    }

    {
        const auto config = configuration(0, ERUI_OK, ERUI_OK,
            ERUI_TEST_CONNECTION_DISABLE_ROW_CONFIGURATION);
        CHECK(configure(&config) == ERUI_OK);
        auto connected = erui::connect(0ms);
        CHECK(connected.success());
        CHECK(connected.value().supports(erui::Capability::updates));
        CHECK(!connected.value().supports(
            erui::Capability::row_configuration));

        erui::NumericSliderRow numeric{};
        erui::CustomSliderRow custom{};
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.row-configuration-capability-absent";
        provider.display_name = L"Row configuration capability absent";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                erui::NumericSliderOptions numeric_options{};
                numeric_options.minimum = 0.0;
                numeric_options.maximum = 2.0;
                numeric_options.step = 1.0;
                numeric = menu.root().add_slider(
                    L"Numeric", L"Configuration must be capability gated",
                    numeric_options);

                erui::CustomSliderOptions custom_options{};
                custom_options.value_count = 3;
                custom_options.format_value = [](
                    std::uint32_t index, std::uint32_t) {
                    return std::to_wstring(index);
                };
                custom = menu.root().add_custom_slider(
                    L"Custom", L"Configuration must be capability gated",
                    custom_options);
            });
        CHECK(registered.success());
        CHECK(numeric.valid());
        CHECK(custom.valid());
        CHECK(numeric.edit().value_labels(
            {L"Low", L"Default", L"High"}).commit() ==
            ERUI_NOT_SUPPORTED);
        CHECK(custom.edit().values(
            {L"One", L"Two", L"Three"}).commit() ==
            ERUI_NOT_SUPPORTED);
        ERUI_TestRowUpdateObservation absent{};
        absent.size = sizeof(absent);
        CHECK(get_row_update(0u, &absent) == ERUI_NOT_FOUND);
    }

    {
        const auto config = configuration(0, ERUI_OK, ERUI_OK,
            ERUI_TEST_CONNECTION_DISABLE_SUBMENU_UPDATES);
        CHECK(configure(&config) == ERUI_OK);
        auto connected = erui::connect(0ms);
        CHECK(connected.success());
        CHECK(connected.value().supports(erui::Capability::updates));
        CHECK(!connected.value().supports(
            erui::Capability::submenu_updates));

        erui::Submenu submenu{};
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.submenu-updates-capability-absent";
        provider.display_name = L"Submenu update capability absent";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                submenu = menu.root().add_submenu(
                    L"Submenu", L"Open submenu", L"Child page");
                submenu.add_toggle(
                    L"Child sentinel", L"Keeps the page non-empty", 1u);
            });
        CHECK(registered.success());
        CHECK(submenu.valid());
        CHECK(submenu.edit().label(L"Rejected").commit() ==
            ERUI_NOT_SUPPORTED);
    }

    {
        const auto config = configuration(0, ERUI_OK, ERUI_OK,
            ERUI_TEST_CONNECTION_DISABLE_TITLE_SCREEN_TEXT);
        CHECK(configure(&config) == ERUI_OK);
        auto connected = erui::connect(0ms);
        CHECK(connected.success());
        CHECK(!connected.value().supports(
            erui::Capability::title_screen_text));

        erui::ProviderOptions provider{};
        provider.provider_id = "tests.title-text-capability-absent";
        provider.display_name = L"Title text capability absent";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [](erui::Menu& menu) noexcept {
                menu.add_title_screen_text(
                    erui::TitleScreenCorner::top_left,
                    {L"Must not reach the host"});
            });
        CHECK(!registered.success());
        CHECK(registered.error().native_result() == ERUI_NOT_SUPPORTED);
        const auto observed = snapshot(get_snapshot);
        CHECK(observed.register_calls == 1);
        CHECK(observed.commit_calls == 0);
        CHECK(observed.abort_calls == 1);
    }

    {
        const auto config = configuration(2, ERUI_OK, ERUI_OK);
        CHECK(configure(&config) == ERUI_OK);

        auto connected = erui::connect(250ms);
        CHECK(connected.success());
        CHECK(connected.value().valid());
        CHECK(connected.value().api_version() == ERUI_API_VERSION_CURRENT);
        CHECK(connected.value().supports(erui::Capability::numeric_slider));
        CHECK(connected.value().supports(erui::Capability::custom_slider));
        CHECK(connected.value().supports(erui::Capability::text_input));
        CHECK(connected.value().supports(erui::Capability::color_picker));
        CHECK(connected.value().supports(erui::Capability::builtin_pages));
        CHECK(connected.value().supports(erui::Capability::input_bindings));
        CHECK(connected.value().supports(erui::Capability::storage));
        CHECK(connected.value().supports(
            erui::Capability::title_screen_text));
        CHECK(connected.value().supports(erui::Capability::updates));
        CHECK(connected.value().supports(
            erui::Capability::row_availability));
        CHECK(connected.value().supports(
            erui::Capability::submenu_updates));
        CHECK(connected.value().supports(
            erui::Capability::row_visibility));
        CHECK(connected.value().supports(
            erui::Capability::row_configuration));

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
        erui::ToggleRow mutable_row{};
        erui::NumericSliderRow numeric_row{};
        erui::CustomSliderRow custom_row{};
        erui::ProviderOptions provider{};
        provider.provider_id = "tests.explicit-connection";
        provider.display_name = L"Explicit Connection";
        provider.owner_module = GetModuleHandleW(nullptr);
        const auto registered = connected.value().register_menu(
            provider,
            [&](erui::Menu& menu) noexcept {
                builder_called = true;
                mutable_row = menu.root().add_toggle(
                    L"Mutable row", L"Before update", 1u);
                erui::NumericSliderOptions numeric_options{};
                numeric_options.minimum = 0.0;
                numeric_options.maximum = 2.0;
                numeric_options.step = 1.0;
                numeric_options.initial_value = 1.0;
                numeric_row = menu.root().add_slider(
                    L"Numeric row", L"Runtime labels", numeric_options);
                erui::CustomSliderOptions custom_options{};
                custom_options.value_count = 3;
                custom_options.initial_index = 1;
                custom_options.format_value = [](
                    std::uint32_t index, std::uint32_t) {
                    return std::to_wstring(index);
                };
                custom_row = menu.root().add_custom_slider(
                    L"Custom row", L"Runtime values", custom_options);
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
        CHECK(numeric_row.valid());
        CHECK(custom_row.valid());
        CHECK(mutable_row.edit()
            .label(L"Updated row")
            .help(L"Updated help")
            .visible(false)
            .commit() == ERUI_OK);
        CHECK(mutable_row.edit().visible(true).commit() == ERUI_OK);
        CHECK(numeric_row.edit().value_labels(
            {L"Low", L"Default", L"High"}).commit() == ERUI_OK);
        CHECK(custom_row.edit().values(
            {L"One", L"Two", L"Three"}).commit() == ERUI_OK);

        ERUI_TestRowUpdateObservation visibility_only{};
        visibility_only.size = sizeof(visibility_only);
        CHECK(get_row_update(1u, &visibility_only) == ERUI_OK);
        CHECK(visibility_only.descriptor_size == sizeof(ERUI_RowUpdateDesc));
        CHECK(visibility_only.row == mutable_row.handle());
        CHECK(visibility_only.fields == ERUI_ROW_UPDATE_FIELD_VISIBLE);
        CHECK(visibility_only.visible == 1u);
        CHECK(visibility_only.reserved[0] == 0u);
        CHECK(visibility_only.reserved[1] == 0u);

        ERUI_TestRowUpdateObservation numeric_configuration{};
        numeric_configuration.size = sizeof(numeric_configuration);
        CHECK(get_row_update(2u, &numeric_configuration) == ERUI_OK);
        CHECK(numeric_configuration.descriptor_size ==
            sizeof(ERUI_RowUpdateDesc));
        CHECK(numeric_configuration.row == numeric_row.handle());
        CHECK(numeric_configuration.fields ==
            ERUI_ROW_UPDATE_FIELD_CONTROL_CONFIGURATION);
        CHECK(numeric_configuration.control_descriptor_size ==
            sizeof(ERUI_RowControlUpdateDesc));
        CHECK(numeric_configuration.control_kind ==
            ERUI_ROW_CONTROL_UPDATE_KIND_NUMERIC_SLIDER);
        CHECK(numeric_configuration.control_payload_size ==
            sizeof(ERUI_NumericSliderControlUpdateDesc));
        CHECK(numeric_configuration.control_fields ==
            ERUI_NUMERIC_SLIDER_CONTROL_UPDATE_FIELD_VALUE_LABELS);
        CHECK(numeric_configuration.control_value_count == 3u);
        CHECK(numeric_configuration.control_value_stride ==
            sizeof(ERUI_Utf16View));
        CHECK(numeric_configuration.reserved[0] == 0u);
        CHECK(numeric_configuration.reserved[1] == 0u);

        ERUI_TestRowControlValueObservation control_value{};
        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(2u, 0u, &control_value) == ERUI_OK);
        CHECK(control_value_equals(control_value, L"Low"));
        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(2u, 1u, &control_value) == ERUI_OK);
        CHECK(control_value_equals(control_value, L"Default"));
        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(2u, 2u, &control_value) == ERUI_OK);
        CHECK(control_value_equals(control_value, L"High"));
        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(2u, 3u, &control_value) ==
            ERUI_NOT_FOUND);

        ERUI_TestRowUpdateObservation custom_configuration{};
        custom_configuration.size = sizeof(custom_configuration);
        CHECK(get_row_update(3u, &custom_configuration) == ERUI_OK);
        CHECK(custom_configuration.descriptor_size ==
            sizeof(ERUI_RowUpdateDesc));
        CHECK(custom_configuration.row == custom_row.handle());
        CHECK(custom_configuration.fields ==
            ERUI_ROW_UPDATE_FIELD_CONTROL_CONFIGURATION);
        CHECK(custom_configuration.control_descriptor_size ==
            sizeof(ERUI_RowControlUpdateDesc));
        CHECK(custom_configuration.control_kind ==
            ERUI_ROW_CONTROL_UPDATE_KIND_CUSTOM_SLIDER);
        CHECK(custom_configuration.control_payload_size ==
            sizeof(ERUI_CustomSliderControlUpdateDesc));
        CHECK(custom_configuration.control_fields ==
            ERUI_CUSTOM_SLIDER_CONTROL_UPDATE_FIELD_VALUES);
        CHECK(custom_configuration.control_value_count == 3u);
        CHECK(custom_configuration.control_value_stride ==
            sizeof(ERUI_Utf16View));
        CHECK(custom_configuration.reserved[0] == 0u);
        CHECK(custom_configuration.reserved[1] == 0u);

        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(3u, 0u, &control_value) == ERUI_OK);
        CHECK(control_value_equals(control_value, L"One"));
        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(3u, 1u, &control_value) == ERUI_OK);
        CHECK(control_value_equals(control_value, L"Two"));
        control_value.size = sizeof(control_value);
        CHECK(get_row_control_value(3u, 2u, &control_value) == ERUI_OK);
        CHECK(control_value_equals(control_value, L"Three"));

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
