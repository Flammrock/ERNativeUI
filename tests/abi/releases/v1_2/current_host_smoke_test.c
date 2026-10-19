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
    ERUI_NumericSliderDesc numeric_desc = {0};
    ERUI_CustomSliderDesc custom_desc = {0};
    ERUI_Utf16View custom_values[3] = {0};
    ERUI_RowHandle text_row = ERUI_INVALID_ROW;
    ERUI_RowHandle color_row = ERUI_INVALID_ROW;
    ERUI_RowHandle numeric_row = ERUI_INVALID_ROW;
    ERUI_RowHandle custom_row = ERUI_INVALID_ROW;
    ERUI_DecimalValue numeric_value = {0};
    uint32_t custom_index = UINT32_MAX;
    ERUI_InputSectionDesc section_desc = {0};
    ERUI_InputSectionHandle section = ERUI_INVALID_INPUT_SECTION;
    ERUI_InputActionDesc action_desc = {0};
    ERUI_InputActionHandle action = ERUI_INVALID_INPUT_ACTION;

    _Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_2,
        "test must compile against the frozen API 1.2 header");
    _Static_assert(sizeof(ERUI_Api) == 368u,
        "test must compile against the frozen API 1.2 table");
    if (argc != 2) return 2;
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != NULL);

    api.size = ERUI_API_V1_2_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_2, &api) == ERUI_OK);
    CHECK(api.size == ERUI_API_V1_2_SIZE);
    CHECK(api.api_version == ERUI_API_VERSION_1_2);

    provider_desc.size = sizeof(provider_desc);
    provider_desc.api_version = ERUI_API_VERSION_1_2;
    provider_desc.owner_module = GetModuleHandleW(NULL);
    provider_desc.provider_id = string_view("tests.v1-2-c-to-current");
    provider_desc.display_name = utf16_view(L"Frozen API 1.2 C client");
    CHECK(api.register_provider(
        &provider_desc, &provider, &root) == ERUI_OK);
    CHECK(api.get_builtin_page(
        provider, ERUI_BUILTIN_PAGE_SOUND, &sound) == ERUI_OK);

    text_desc.size = sizeof(text_desc);
    text_desc.label = utf16_view(L"Frozen text");
    text_desc.initial_value = utf16_view(L"API 1.2");
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

    numeric_desc.size = sizeof(numeric_desc);
    numeric_desc.label = utf16_view(L"Frozen numeric slider");
    numeric_desc.minimum_scaled_value = -10;
    numeric_desc.maximum_scaled_value = 10;
    numeric_desc.step_scaled_value = 1;
    numeric_desc.initial_scaled_value = -5;
    numeric_desc.decimal_places = 1u;
    numeric_desc.enabled = 1u;
    numeric_desc.prefix = utf16_view(L"x");
    CHECK(api.add_numeric_slider(
        provider, sound, &numeric_desc, &numeric_row) == ERUI_OK);

    custom_values[0] = utf16_view(L"-7");
    custom_values[1] = utf16_view(L"1s");
    custom_values[2] = utf16_view(L"x1.3");
    custom_desc.size = sizeof(custom_desc);
    custom_desc.label = utf16_view(L"Frozen custom slider");
    custom_desc.values = custom_values;
    custom_desc.value_count = 3u;
    custom_desc.initial_index = 1u;
    custom_desc.enabled = 1u;
    CHECK(api.add_custom_slider(
        provider, sound, &custom_desc, &custom_row) == ERUI_OK);

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
    CHECK(numeric_row != ERUI_INVALID_ROW);
    CHECK(custom_row != ERUI_INVALID_ROW);
    CHECK(action != ERUI_INVALID_INPUT_ACTION);
    CHECK(api.commit_provider(provider) == ERUI_OK);

    numeric_value.scaled_value = 7;
    numeric_value.decimal_places = 1u;
    CHECK(api.set_numeric_slider_value(
        provider, numeric_row, &numeric_value) == ERUI_OK);
    numeric_value.scaled_value = 0;
    numeric_value.decimal_places = 0u;
    CHECK(api.get_numeric_slider_value(
        provider, numeric_row, &numeric_value) == ERUI_OK);
    CHECK(numeric_value.scaled_value == 7);
    CHECK(numeric_value.decimal_places == 1u);

    CHECK(api.set_custom_slider_index(
        provider, custom_row, 2u) == ERUI_OK);
    CHECK(api.get_custom_slider_index(
        provider, custom_row, &custom_index) == ERUI_OK);
    CHECK(custom_index == 2u);
    erui_unload_test_host(&host);
    return 0;
}
