#include "connection_control.h"
#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

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

void action_activated(const erui::ActionActivation&) noexcept {}

struct AssignmentObservation {
    erui::StorageSection storage{};
    erui::InputAction action{};
    erui::ActionInputs previous{};
    erui::ActionInputs current{};
    erui::AssignmentChangeReason reason{erui::AssignmentChangeReason::unknown};
    std::string action_id{};
    ERUI_Result apply_result{ERUI_INTERNAL_ERROR};
    ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
    bool controller_changed{};
    bool keyboard_changed{};
    bool mouse_changed{};
    unsigned calls{};
};

void assignments_changed(
    AssignmentObservation& observation,
    const erui::AssignmentsChangedEvent& event) noexcept {
    observation.provider = event.provider_handle();
    observation.apply_result = observation.storage.apply(event);
    for (const erui::AssignmentChange change : event.changes()) {
        observation.action = change.action();
        observation.previous = change.previous();
        observation.current = change.current();
        observation.reason = change.reason();
        observation.action_id.assign(change.action_id());
        observation.controller_changed = change.controller_changed();
        observation.keyboard_changed = change.keyboard_changed();
        observation.mouse_changed = change.mouse_changed();
        ++observation.calls;
    }
}

bool same(const erui::ActionInputs& left,
    const erui::ActionInputs& right) noexcept {
    return left == right;
}

} // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    const auto configure = resolve<ERUI_TestConfigureConnectionFn>(
        host.module, "ERUI_TestConfigureConnection");
    const auto emit = resolve<ERUI_TestEmitAssignmentChangeFn>(
        host.module, "ERUI_TestEmitAssignmentChange");
    CHECK(configure != nullptr);
    CHECK(emit != nullptr);

    ERUI_TestConnectionConfig fixture{};
    fixture.size = sizeof(fixture);
    fixture.version = ERUI_TEST_CONNECTION_CONTROL_VERSION;
    fixture.terminal_result = ERUI_OK;
    fixture.language_result = ERUI_OK;
    CHECK(configure(&fixture) == ERUI_OK);

    auto connection = erui::connect(0ms);
    CHECK(connection.success());

    const erui::ActionInputs defaults = erui::inputs::all(
        erui::KeyboardKey::key_q,
        erui::MouseButton::button4,
        erui::ControllerButton::right_trigger);
    const erui::ActionInputs controller_defaults = erui::inputs::controller(
        erui::ControllerButton::face_north);

    erui::InputAction action{};
    erui::InputAction controller_only{};
    erui::Storage storage{};
    erui::StorageSection settings{};
    erui::StorageSection binding_store{};
    AssignmentObservation observation{};
    bool builder_ok = true;

    erui::ProviderOptions provider{};
    provider.provider_id = "tests.input-bindings-wrapper";
    provider.display_name = L"Input bindings wrapper";
    provider.owner_module = GetModuleHandleW(nullptr);
    const auto registered = connection.value().register_menu(
        provider,
        [&](erui::Menu& menu) noexcept -> void {
#define BUILDER_CHECK(condition) do { \
    if (!(condition)) { builder_ok = false; return; } \
} while (false)
            storage = menu.storage(erui::StorageOptions::beside_module(
                L"tests/input-bindings-wrapper.ini"));
            BUILDER_CHECK(storage.valid());
            settings = storage.section("settings");
            binding_store = storage.section("bindings");
            BUILDER_CHECK(settings.valid());
            BUILDER_CHECK(binding_store.valid());

            erui::StorageInfo initial_info{};
            BUILDER_CHECK(storage.query_info(initial_info) == ERUI_OK);
            BUILDER_CHECK(!initial_info.loaded);
            BUILDER_CHECK(settings.get<std::string>("name").result() ==
                ERUI_STORAGE_NOT_LOADED);
            BUILDER_CHECK(storage.load() == ERUI_OK);
            BUILDER_CHECK(storage.is_loaded());

            auto bindings = menu.input_bindings();
            BUILDER_CHECK(bindings.valid());
            auto section = bindings.add_section(L"Wrapper actions");
            BUILDER_CHECK(section.valid());
            action = section.add_action<&action_activated>(
                "quick-action", L"Quick action", defaults);
            controller_only = section.add_action<&action_activated>(
                "controller-only", L"Controller only", controller_defaults);
            BUILDER_CHECK(action.valid());
            BUILDER_CHECK(controller_only.valid());

            observation.storage = binding_store;
            BUILDER_CHECK(bindings.on_assignments_changed<&assignments_changed>(
                observation) == ERUI_OK);
#undef BUILDER_CHECK
        });
    CHECK(registered.success());
    CHECK(builder_ok);
    CHECK(action.valid());
    CHECK(controller_only.valid());
    CHECK(storage.valid());

    erui::ActionInputs current{};
    erui::ActionInputs declared_defaults{};
    CHECK(action.query_inputs(current) == ERUI_OK);
    CHECK(action.query_default_inputs(declared_defaults) == ERUI_OK);
    CHECK(same(current, defaults));
    CHECK(same(declared_defaults, defaults));
    CHECK(action.is_bound());
    CHECK(action.is_fully_bound());
    CHECK(action.is_default());

    CHECK(action.devices().mouse().unbind() == ERUI_OK);
    CHECK(action.query_inputs(current) == ERUI_OK);
    CHECK(current.controller_input() == defaults.controller_input());
    CHECK(current.keyboard_input() == defaults.keyboard_input());
    CHECK(current.mouse_supported());
    CHECK(!current.mouse_input().has_value());
    CHECK(action.is_bound());
    CHECK(!action.is_fully_bound());
    CHECK(!action.is_default());

    CHECK(action.devices().keyboard().bind(erui::KeyboardKey::key_f) == ERUI_OK);
    CHECK(action.query_inputs(current) == ERUI_OK);
    CHECK(current.keyboard_input() == erui::KeyboardKey::key_f);
    CHECK(!current.mouse_input().has_value());

    // ActionInputs is a sparse patch for bind(): absent slots stay unchanged.
    CHECK(action.bind(erui::ActionInputs{}.controller()) == ERUI_OK);
    CHECK(action.query_inputs(current) == ERUI_OK);
    CHECK(!current.controller_input().has_value());
    CHECK(current.keyboard_input() == erui::KeyboardKey::key_f);
    CHECK(!current.mouse_input().has_value());

    CHECK(action.devices().controller().reset_to_default() == ERUI_OK);
    CHECK(action.query_inputs(current) == ERUI_OK);
    CHECK(current.controller_input() == defaults.controller_input());
    CHECK(current.keyboard_input() == erui::KeyboardKey::key_f);
    CHECK(action.unbind() == ERUI_OK);
    CHECK(!action.is_bound());
    CHECK(!action.is_fully_bound());
    CHECK(action.reset_to_defaults() == ERUI_OK);
    CHECK(action.is_default());
    CHECK(action.is_fully_bound());

    // A sparse patch may not introduce a device omitted by the declaration.
    CHECK(controller_only.bind(
        erui::ActionInputs{}.keyboard(erui::KeyboardKey::key_q)) ==
        ERUI_INVALID_ARGUMENT);
    CHECK(controller_only.devices().controller().is_supported());
    CHECK(!controller_only.devices().keyboard().is_supported());
    CHECK(!controller_only.devices().mouse().is_supported());

    CHECK(settings.set("name", "Tarnished") == ERUI_OK);
    CHECK(settings.set("enabled", true) == ERUI_OK);
    CHECK(settings.set("attempts", 17) == ERUI_OK);
    CHECK(settings.set("scale", 1.25) == ERUI_OK);
    CHECK(binding_store.set("quick-action", action.inputs()) == ERUI_OK);
    CHECK(settings.get<std::string>("name").value() == "Tarnished");
    CHECK(settings.get<bool>("enabled").value());
    CHECK(settings.get<int>("attempts").value() == 17);
    CHECK(settings.get<double>("scale").value() == 1.25);
    CHECK(binding_store.get<erui::ActionInputs>("quick-action").value() ==
        defaults);
    CHECK(!settings.get<std::string>("missing").found());
    CHECK(settings.get<std::string>("missing").result() == ERUI_NOT_FOUND);
    CHECK(storage.is_dirty());
    CHECK(storage.current_revision() >= 5);
    CHECK(storage.save() == ERUI_OK);
    CHECK(!storage.is_dirty());
    CHECK(storage.last_saved_revision() == storage.current_revision());

    ERUI_ActionInputs player_patch{};
    player_patch.size = sizeof(player_patch);
    player_patch.keyboard = {
        ERUI_INPUT_SLOT_UNBOUND, ERUI_KEYBOARD_KEY_INVALID};
    player_patch.mouse = {
        ERUI_INPUT_SLOT_BOUND, ERUI_MOUSE_BUTTON_5};
    CHECK(emit(action.handle(), &player_patch,
        ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT) == ERUI_OK);
    CHECK(observation.calls == 1);
    CHECK(observation.provider != ERUI_INVALID_PROVIDER);
    CHECK(observation.action_id == "quick-action");
    CHECK(observation.action.valid());
    CHECK(observation.action.handle() == action.handle());
    CHECK(observation.reason ==
        erui::AssignmentChangeReason::player_assignment);
    CHECK(!observation.controller_changed);
    CHECK(observation.keyboard_changed);
    CHECK(observation.mouse_changed);
    CHECK(observation.previous == defaults);
    CHECK(observation.current.controller_input() ==
        defaults.controller_input());
    CHECK(!observation.current.keyboard_input().has_value());
    CHECK(observation.current.mouse_input() == erui::MouseButton::button5);
    CHECK(observation.apply_result == ERUI_OK);
    CHECK(action.inputs() == observation.current);
    CHECK(binding_store.get<erui::ActionInputs>("quick-action").value() ==
        observation.current);

    CHECK(settings.erase("name") == ERUI_OK);
    CHECK(settings.erase("name") == ERUI_OK);
    CHECK(!settings.get<std::string>("name").found());
    CHECK(storage.save() == ERUI_OK);

    // The wrapper forwards stable IDs unchanged; the host remains the
    // authoritative validator and rejects malformed IDs without committing a
    // partially built provider.
    erui::ProviderOptions invalid_provider{};
    invalid_provider.provider_id = "tests.input-bindings-wrapper-invalid";
    invalid_provider.display_name = L"Invalid input binding ID";
    invalid_provider.owner_module = GetModuleHandleW(nullptr);
    const auto invalid_registration = connection.value().register_menu(
        invalid_provider,
        [](erui::Menu& menu) noexcept {
            auto section = menu.input_bindings().add_section(L"Invalid");
            (void)section.add_action<&action_activated>(
                "contains/slash", L"Rejected action");
        });
    CHECK(!invalid_registration.success());
    CHECK(invalid_registration.error().native_result() ==
        ERUI_INVALID_ARGUMENT);

    erui_unload_test_host(&host);
    return 0;
}
