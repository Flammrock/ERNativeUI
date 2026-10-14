#include "host_loader.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

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

static ERUI_Utf16View utf16_view(const wchar_t* value) {
    ERUI_Utf16View result = {0};
    result.data = (const uint16_t*)value;
    result.length = (uint32_t)wcslen(value);
    return result;
}

static void ERUI_CALL activated(
    void* user_data,
    const ERUI_InputActionActivatedContext* context) {
    (void)user_data;
    (void)context;
}

int main(int argc, char** argv) {
    ERUI_LoadedTestHost host = {0};
    ERUI_Api api = {0};
    ERUI_ProviderDesc provider_desc = {0};
    ERUI_ProviderHandle provider = ERUI_INVALID_PROVIDER;
    ERUI_PageHandle root = ERUI_INVALID_PAGE;
    ERUI_PageHandle sound = ERUI_INVALID_PAGE;
    ERUI_TextInputDesc text_desc = {0};
    ERUI_ColorPickerDesc color_desc = {0};
    ERUI_RowHandle text_row = ERUI_INVALID_ROW;
    ERUI_RowHandle color_row = ERUI_INVALID_ROW;
    ERUI_InputSectionDesc section_desc = {0};
    ERUI_InputSectionHandle section = ERUI_INVALID_INPUT_SECTION;
    ERUI_InputActionDesc action_desc = {0};
    ERUI_InputActionHandle action = ERUI_INVALID_INPUT_ACTION;

    _Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_1,
        "test must compile against the frozen API 1.1 header");
    _Static_assert(sizeof(ERUI_Api) == 320u,
        "test must compile against the frozen API 1.1 table");
    if (argc != 2) return 2;
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != NULL);

    api.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &api) == ERUI_OK);
    CHECK(api.size == ERUI_API_V1_1_SIZE);
    CHECK(api.api_version == ERUI_API_VERSION_1_1);

    provider_desc.size = sizeof(provider_desc);
    provider_desc.api_version = ERUI_API_VERSION_1_1;
    provider_desc.owner_module = GetModuleHandleW(NULL);
    provider_desc.provider_id = string_view("tests.v1-1-c-to-current");
    provider_desc.display_name = utf16_view(L"Frozen API 1.1 C client");
    CHECK(api.register_provider(
        &provider_desc, &provider, &root) == ERUI_OK);
    CHECK(api.get_builtin_page(
        provider, ERUI_BUILTIN_PAGE_SOUND, &sound) == ERUI_OK);

    text_desc.size = sizeof(text_desc);
    text_desc.label = utf16_view(L"Frozen text");
    text_desc.initial_value = utf16_view(L"API 1.1");
    text_desc.placeholder = utf16_view(L"Enter text");
    text_desc.maximum_length = 16u;
    CHECK(api.add_text_input(
        provider, sound, &text_desc, &text_row) == ERUI_OK);

    color_desc.size = sizeof(color_desc);
    color_desc.label = utf16_view(L"Frozen color");
    color_desc.initial_value.red = 1u;
    color_desc.initial_value.green = 2u;
    color_desc.initial_value.blue = 3u;
    color_desc.enabled = 1u;
    CHECK(api.add_color_picker(
        provider, sound, &color_desc, &color_row) == ERUI_OK);

    section_desc.size = sizeof(section_desc);
    section_desc.label = utf16_view(L"Frozen bindings");
    CHECK(api.add_input_section(
        provider, &section_desc, &section) == ERUI_OK);
    action_desc.size = sizeof(action_desc);
    action_desc.action_id = string_view("frozen-action");
    action_desc.label = utf16_view(L"Frozen action");
    action_desc.default_inputs.size = sizeof(action_desc.default_inputs);
    action_desc.default_inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    action_desc.default_inputs.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    action_desc.activated_callback = &activated;
    CHECK(api.add_input_action(
        provider, section, &action_desc, &action) == ERUI_OK);

    CHECK(text_row != ERUI_INVALID_ROW);
    CHECK(color_row != ERUI_INVALID_ROW);
    CHECK(action != ERUI_INVALID_INPUT_ACTION);
    CHECK(api.commit_provider(provider) == ERUI_OK);
    erui_unload_test_host(&host);
    return 0;
}
