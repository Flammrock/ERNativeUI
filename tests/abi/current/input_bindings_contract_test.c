#include "host_loader.h"

#include <Windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static ERUI_StringView string_view(const char* value) {
    ERUI_StringView result = {0};
    result.data = value;
    result.length = (uint32_t)strlen(value);
    return result;
}

static ERUI_Utf16View utf16_view(
    const uint16_t* value,
    uint32_t length) {
    ERUI_Utf16View result = {0};
    result.data = value;
    result.length = length;
    return result;
}

static void ERUI_CALL binding_activated(
    void* user_data,
    const ERUI_InputActionActivatedContext* context) {
    (void)user_data;
    (void)context;
}

static void ERUI_CALL assignments_changed(
    void* user_data,
    const ERUI_AssignmentsChangedContext* context) {
    (void)user_data;
    (void)context;
}

static ERUI_Result register_provider(
    const ERUI_Api* api,
    uint32_t api_version,
    const char* provider_id,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root) {
    static const uint16_t display_name[] = {
        'I', 'n', 'p', 'u', 't', ' ', 'b', 'i', 'n', 'd', 'i', 'n', 'g', 's'};
    ERUI_ProviderDesc description = {0};
    description.size = (uint32_t)sizeof(description);
    description.api_version = api_version;
    description.owner_module = GetModuleHandleW(NULL);
    description.provider_id = string_view(provider_id);
    description.display_name = utf16_view(
        display_name,
        (uint32_t)(sizeof(display_name) / sizeof(display_name[0])));
    return api->register_provider(&description, out_provider, out_root);
}

static ERUI_InputSectionDesc section_description(void) {
    static const uint16_t label[] = {
        'E', 'R', 'N', 'a', 't', 'i', 'v', 'e', 'U', 'I'};
    ERUI_InputSectionDesc description = {0};
    description.size = (uint32_t)sizeof(description);
    description.label = utf16_view(
        label, (uint32_t)(sizeof(label) / sizeof(label[0])));
    return description;
}

static ERUI_InputActionDesc binding_description(const char* binding_id) {
    static const uint16_t label[] = {
        'S', 'h', 'o', 'w', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e'};
    ERUI_InputActionDesc description = {0};
    description.size = (uint32_t)sizeof(description);
    description.action_id = string_view(binding_id);
    description.label = utf16_view(
        label, (uint32_t)(sizeof(label) / sizeof(label[0])));
    description.activated_callback = &binding_activated;
    description.default_inputs.size =
        (uint32_t)sizeof(description.default_inputs);
    description.default_inputs.controller.state = ERUI_INPUT_SLOT_BOUND;
    description.default_inputs.controller.input =
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER;
    description.default_inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    description.default_inputs.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    description.default_inputs.mouse.state = ERUI_INPUT_SLOT_UNBOUND;
    return description;
}

int main(int argc, char** argv) {
    ERUI_LoadedTestHost host;
    ERUI_Api api;
    ERUI_Api api_v1_0;
    ERUI_ProviderHandle legacy_provider;
    ERUI_PageHandle legacy_root;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle root;
    ERUI_InputSectionHandle first_section;
    ERUI_InputSectionHandle second_section;
    ERUI_InputActionHandle first_binding;
    ERUI_InputActionHandle second_binding;
    ERUI_InputSectionDesc section;
    ERUI_InputActionDesc binding;

    if (argc != 2) return 2;
    memset(&host, 0, sizeof(host));
    CHECK(erui_load_test_host(argv[1], &host));

    memset(&api, 0, sizeof(api));
    api.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &api) == ERUI_OK);
    CHECK(api.size == ERUI_API_V1_1_SIZE);
    CHECK((api.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0u);
    CHECK(api.add_input_section != NULL);
    CHECK(api.add_input_action != NULL);
    CHECK(api.set_action_inputs != NULL);
    CHECK(api.get_action_inputs != NULL);
    CHECK(api.get_action_default_inputs != NULL);
    CHECK(api.reset_action_inputs != NULL);
    CHECK(api.set_assignments_changed_handler != NULL);

    /* A provider is bound to the exact table that registered it. */
    memset(&api_v1_0, 0, sizeof(api_v1_0));
    api_v1_0.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &api_v1_0) == ERUI_OK);
    legacy_provider = ERUI_INVALID_PROVIDER;
    legacy_root = ERUI_INVALID_PAGE;
    CHECK(register_provider(
        &api_v1_0,
        ERUI_API_VERSION_1_0,
        "tests.current-v1-0-binding-rejection-c",
        &legacy_provider,
        &legacy_root) == ERUI_OK);
    section = section_description();
    first_section = UINT64_C(0xAAAAAAAAAAAAAAAA);
    CHECK(api.add_input_section(
        legacy_provider, &section, &first_section) == ERUI_NOT_SUPPORTED);
    CHECK(first_section == UINT64_C(0xAAAAAAAAAAAAAAAA));
    binding = binding_description("legacy-binding");
    first_binding = UINT64_C(0xBBBBBBBBBBBBBBBB);
    CHECK(api.add_input_action(
        legacy_provider,
        ERUI_INVALID_INPUT_SECTION,
        &binding,
        &first_binding) == ERUI_NOT_SUPPORTED);
    CHECK(first_binding == UINT64_C(0xBBBBBBBBBBBBBBBB));
    CHECK(api_v1_0.abort_provider(legacy_provider) == ERUI_OK);

    provider = ERUI_INVALID_PROVIDER;
    root = ERUI_INVALID_PAGE;
    CHECK(register_provider(
        &api,
        ERUI_API_VERSION_1_1,
        "tests.current-v1-1-input-bindings-c",
        &provider,
        &root) == ERUI_OK);

    first_section = ERUI_INVALID_INPUT_SECTION;
    CHECK(api.add_input_section(
        provider, &section, &first_section) == ERUI_OK);
    CHECK(first_section != ERUI_INVALID_INPUT_SECTION);
    second_section = ERUI_INVALID_INPUT_SECTION;
    CHECK(api.add_input_section(
        provider, &section, &second_section) == ERUI_OK);
    CHECK(second_section != ERUI_INVALID_INPUT_SECTION);
    CHECK(second_section != first_section);

    binding = binding_description("show-message");
    first_binding = ERUI_INVALID_INPUT_ACTION;
    CHECK(api.add_input_action(
        provider, first_section, &binding, &first_binding) == ERUI_OK);
    CHECK(first_binding != ERUI_INVALID_INPUT_ACTION);
    {
        ERUI_ActionInputs current = {0};
        ERUI_ActionInputs defaults = {0};
        ERUI_ActionInputs patch = {0};
        ERUI_AssignmentsChangedHandlerDesc handler = {0};
        current.size = (uint32_t)sizeof(current);
        defaults.size = (uint32_t)sizeof(defaults);
        CHECK(api.get_action_inputs(
            provider, first_binding, &current) == ERUI_OK);
        CHECK(api.get_action_default_inputs(
            provider, first_binding, &defaults) == ERUI_OK);
        CHECK(memcmp(&current, &defaults, sizeof(current)) == 0);
        CHECK(defaults.controller.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(defaults.controller.input ==
            ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER);
        CHECK(defaults.keyboard.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(defaults.keyboard.input == ERUI_KEYBOARD_KEY_Q);
        CHECK(defaults.mouse.state == ERUI_INPUT_SLOT_UNBOUND);

        {
            ERUI_ActionInputs undersized;
            ERUI_ActionInputs before;
            memset(&undersized, 0xA5, sizeof(undersized));
            undersized.size = (uint32_t)sizeof(undersized) - 1u;
            before = undersized;
            CHECK(api.get_action_inputs(
                provider, first_binding, &undersized) ==
                ERUI_BUFFER_TOO_SMALL);
            CHECK(memcmp(&undersized, &before, sizeof(undersized)) == 0);
            CHECK(api.get_action_default_inputs(
                provider, first_binding, &undersized) ==
                ERUI_BUFFER_TOO_SMALL);
            CHECK(memcmp(&undersized, &before, sizeof(undersized)) == 0);
        }

        patch.size = (uint32_t)sizeof(patch);
        patch.keyboard.state = ERUI_INPUT_SLOT_BOUND;
        patch.keyboard.input = ERUI_KEYBOARD_KEY_F;
        CHECK(api.set_action_inputs(
            provider, first_binding, &patch) == ERUI_OK);
        current.size = (uint32_t)sizeof(current);
        CHECK(api.get_action_inputs(
            provider, first_binding, &current) == ERUI_OK);
        /* ABSENT slots in the patch are untouched, not made unsupported. */
        CHECK(current.controller.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(current.controller.input ==
            ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER);
        CHECK(current.keyboard.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(current.keyboard.input == ERUI_KEYBOARD_KEY_F);
        CHECK(current.mouse.state == ERUI_INPUT_SLOT_UNBOUND);

        memset(&patch, 0, sizeof(patch));
        patch.size = (uint32_t)sizeof(patch);
        CHECK(api.set_action_inputs(
            provider, first_binding, &patch) == ERUI_OK);
        CHECK(api.get_action_inputs(
            provider, first_binding, &current) == ERUI_OK);
        CHECK(current.keyboard.input == ERUI_KEYBOARD_KEY_F);

        CHECK(api.reset_action_inputs(
            provider, first_binding, ERUI_INPUT_DEVICE_KEYBOARD) == ERUI_OK);
        current.size = (uint32_t)sizeof(current);
        CHECK(api.get_action_inputs(
            provider, first_binding, &current) == ERUI_OK);
        CHECK(current.keyboard.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(current.keyboard.input == ERUI_KEYBOARD_KEY_Q);
        CHECK(api.reset_action_inputs(
            provider, first_binding, ERUI_INPUT_DEVICE_NONE) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(api.reset_action_inputs(
            provider, first_binding, ERUI_INPUT_DEVICE_ALL | (1u << 9)) ==
            ERUI_INVALID_ARGUMENT);

        memset(&patch, 0, sizeof(patch));
        patch.size = (uint32_t)sizeof(patch);
        patch.keyboard.state = ERUI_INPUT_SLOT_BOUND;
        patch.keyboard.input = ERUI_KEYBOARD_KEY_COUNT;
        CHECK(api.set_action_inputs(
            provider, first_binding, &patch) == ERUI_INVALID_ARGUMENT);
        patch.keyboard.state = ERUI_INPUT_SLOT_UNBOUND;
        patch.keyboard.input = ERUI_KEYBOARD_KEY_Q;
        CHECK(api.set_action_inputs(
            provider, first_binding, &patch) == ERUI_INVALID_ARGUMENT);
        patch.keyboard.input = ERUI_KEYBOARD_KEY_INVALID;
        patch.flags = 1u;
        CHECK(api.set_action_inputs(
            provider, first_binding, &patch) == ERUI_INVALID_ARGUMENT);

        handler.size = (uint32_t)sizeof(handler);
        handler.callback = &assignments_changed;
        CHECK(api.set_assignments_changed_handler(
            provider, &handler) == ERUI_OK);

        handler.size = (uint32_t)sizeof(handler) - 1u;
        CHECK(api.set_assignments_changed_handler(
            provider, &handler) == ERUI_INVALID_ARGUMENT);
        handler.size = (uint32_t)sizeof(handler);
        handler.flags = 1u;
        CHECK(api.set_assignments_changed_handler(
            provider, &handler) == ERUI_INVALID_ARGUMENT);
        handler.flags = 0u;
        handler.callback = NULL;
        handler.user_data = &handler;
        CHECK(api.set_assignments_changed_handler(
            provider, &handler) == ERUI_INVALID_ARGUMENT);
        handler.user_data = NULL;
        CHECK(api.set_assignments_changed_handler(
            provider, &handler) == ERUI_OK);
    }

    /* IDs are provider-wide, not section-local. */
    second_binding = UINT64_C(0xCCCCCCCCCCCCCCCC);
    CHECK(api.add_input_action(
        provider, second_section, &binding, &second_binding) ==
        ERUI_DUPLICATE_ACTION_ID);
    CHECK(second_binding == ERUI_INVALID_INPUT_ACTION);

    binding = binding_description("second-action");
    binding.default_inputs.keyboard.state = ERUI_INPUT_SLOT_ABSENT;
    binding.default_inputs.keyboard.input = ERUI_KEYBOARD_KEY_INVALID;
    binding.default_inputs.mouse.state = ERUI_INPUT_SLOT_ABSENT;
    binding.default_inputs.mouse.input = ERUI_MOUSE_BUTTON_INVALID;
    second_binding = ERUI_INVALID_INPUT_ACTION;
    CHECK(api.add_input_action(
        provider, second_section, &binding, &second_binding) == ERUI_OK);
    CHECK(second_binding != ERUI_INVALID_INPUT_ACTION);
    CHECK(second_binding != first_binding);
    {
        ERUI_ActionInputs unsupported_patch = {0};
        unsupported_patch.size = (uint32_t)sizeof(unsupported_patch);
        unsupported_patch.mouse.state = ERUI_INPUT_SLOT_BOUND;
        unsupported_patch.mouse.input = ERUI_MOUSE_BUTTON_4;
        CHECK(api.set_action_inputs(
            provider, second_binding, &unsupported_patch) ==
            ERUI_INVALID_ARGUMENT);
    }

    /* The output handle follows ordinary add-row semantics and is optional. */
    binding = binding_description("discarded-handle");
    CHECK(api.add_input_action(
        provider, first_section, &binding, NULL) == ERUI_OK);

    {
        ERUI_InputSectionDesc invalid_section = section_description();
        ERUI_InputSectionHandle rejected = UINT64_C(0x1111111111111111);
        invalid_section.flags = 1u;
        CHECK(api.add_input_section(
            provider, &invalid_section, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_SECTION);
        invalid_section.flags = 0u;
        invalid_section.reserved[1] = 1u;
        rejected = UINT64_C(0x2222222222222222);
        CHECK(api.add_input_section(
            provider, &invalid_section, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_SECTION);
    }

    {
        static const char embedded_nul[] = {'b', 'a', 'd', '\0', 'i', 'd'};
        static const char utf8_id[] = {'b', 'a', 'd', '-',
            (char)0xC3, (char)0xA9};
        ERUI_InputActionDesc invalid_binding = binding_description("invalid");
        ERUI_InputActionHandle rejected = UINT64_C(0x3333333333333333);
        invalid_binding.flags = 1u;
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);
        invalid_binding.flags = 0u;
        invalid_binding.activated_callback = NULL;
        rejected = UINT64_C(0x4444444444444444);
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);
        invalid_binding = binding_description("invalid");
        invalid_binding.action_id.data = embedded_nul;
        invalid_binding.action_id.length =
            (uint32_t)sizeof(embedded_nul);
        rejected = UINT64_C(0x5555555555555555);
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);

        invalid_binding = binding_description("bad/id");
        rejected = UINT64_C(0x5656565656565656);
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);

        invalid_binding = binding_description("bad id");
        rejected = UINT64_C(0x5757575757575757);
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);

        invalid_binding = binding_description("invalid");
        invalid_binding.action_id.data = utf8_id;
        invalid_binding.action_id.length = (uint32_t)sizeof(utf8_id);
        rejected = UINT64_C(0x5858585858585858);
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);

        invalid_binding = binding_description("no-supported-device");
        memset(&invalid_binding.default_inputs, 0,
            sizeof(invalid_binding.default_inputs));
        invalid_binding.default_inputs.size =
            (uint32_t)sizeof(invalid_binding.default_inputs);
        rejected = UINT64_C(0x6666666666666666);
        CHECK(api.add_input_action(
            provider, first_section, &invalid_binding, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_INPUT_ACTION);
    }

    CHECK(api.commit_provider(provider) == ERUI_OK);
    first_section = UINT64_C(0x6666666666666666);
    CHECK(api.add_input_section(
        provider, &section, &first_section) == ERUI_ALREADY_COMMITTED);
    CHECK(first_section == ERUI_INVALID_INPUT_SECTION);
    {
        ERUI_AssignmentsChangedHandlerDesc handler = {0};
        handler.size = (uint32_t)sizeof(handler);
        handler.callback = &assignments_changed;
        CHECK(api.set_assignments_changed_handler(provider, &handler) ==
            ERUI_ALREADY_COMMITTED);
    }

    /* The same stable action ID is valid under a different provider. */
    provider = ERUI_INVALID_PROVIDER;
    root = ERUI_INVALID_PAGE;
    CHECK(register_provider(
        &api,
        ERUI_API_VERSION_1_1,
        "tests.current-v1-1-input-bindings-second-provider-c",
        &provider,
        &root) == ERUI_OK);
    first_section = ERUI_INVALID_INPUT_SECTION;
    CHECK(api.add_input_section(
        provider, &section, &first_section) == ERUI_OK);
    binding = binding_description("show-message");
    first_binding = ERUI_INVALID_INPUT_ACTION;
    CHECK(api.add_input_action(
        provider, first_section, &binding, &first_binding) == ERUI_OK);
    CHECK(first_binding != ERUI_INVALID_INPUT_ACTION);
    CHECK(api.abort_provider(provider) == ERUI_OK);

    erui_unload_test_host(&host);
    return 0;
}
