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

static void ERUI_CALL action_activated(
    void* user_data,
    const ERUI_InputActionActivatedContext* context) {
    (void)user_data;
    (void)context;
}

static int all_bytes_equal(
    const unsigned char* bytes,
    size_t count,
    unsigned char expected) {
    size_t index;
    for (index = 0; index < count; ++index) {
        if (bytes[index] != expected) return 0;
    }
    return 1;
}

int main(int argc, char** argv) {
    typedef struct ApiBlock {
        ERUI_Api api;
        unsigned char canary[32];
    } ApiBlock;
    const ERUI_Capabilities required =
        ERUI_CAP_TEXT_INPUT | ERUI_CAP_COLOR_PICKER |
        ERUI_CAP_BUILTIN_PAGES | ERUI_CAP_INPUT_BINDINGS |
        ERUI_CAP_STORAGE;
    ERUI_LoadedTestHost host = {0};
    ApiBlock block;
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
    ERUI_ActionInputs inputs = {0};
    ERUI_StorageDesc storage_desc = {0};
    ERUI_StorageHandle storage = ERUI_INVALID_STORAGE;
    ERUI_StorageInfo storage_info = {0};

    _Static_assert(sizeof(wchar_t) == sizeof(uint16_t),
        "Windows wchar_t must be UTF-16");
    _Static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_1,
        "not a frozen API 1.1 client");
    _Static_assert(sizeof(ERUI_Api) == 320u, "frozen table drift");
    _Static_assert(offsetof(ApiBlock, canary) == 320u,
        "unexpected API block padding");

    if (argc != 2) return 2;
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != NULL);

    memset(&block, 0, sizeof(block));
    memset(block.canary, 0xA5, sizeof(block.canary));
    block.api.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &block.api) == ERUI_OK);
    CHECK(block.api.size == ERUI_API_V1_1_SIZE);
    CHECK(block.api.api_version == ERUI_API_VERSION_1_1);
    CHECK(all_bytes_equal(block.canary, sizeof(block.canary), 0xA5));
    CHECK((block.api.capabilities & required) == required);
    CHECK(block.api.register_provider && block.api.commit_provider &&
        block.api.add_text_input && block.api.set_text_input_value &&
        block.api.get_text_input_value && block.api.add_color_picker &&
        block.api.set_color_picker_value &&
        block.api.get_color_picker_value && block.api.get_builtin_page &&
        block.api.add_input_section && block.api.add_input_action &&
        block.api.set_action_inputs && block.api.get_action_inputs &&
        block.api.get_action_default_inputs &&
        block.api.reset_action_inputs && block.api.open_storage &&
        block.api.storage_load && block.api.storage_save &&
        block.api.storage_get_info);

    provider_desc.size = sizeof(provider_desc);
    provider_desc.api_version = ERUI_API_VERSION_1_1;
    provider_desc.owner_module = GetModuleHandleW(NULL);
    provider_desc.provider_id = string_view("tests.frozen-v1-1-c");
    provider_desc.display_name = utf16_view(L"Frozen v1.1 C client");
    CHECK(block.api.register_provider(
        &provider_desc, &provider, &root) == ERUI_OK);
    CHECK(provider != ERUI_INVALID_PROVIDER && root != ERUI_INVALID_PAGE);

    CHECK(block.api.get_builtin_page(
        provider, ERUI_BUILTIN_PAGE_SOUND, &sound) == ERUI_OK);
    CHECK(sound != ERUI_INVALID_PAGE && sound != root);

    text_desc.size = sizeof(text_desc);
    text_desc.label = utf16_view(L"Frozen text");
    text_desc.initial_value = utf16_view(L"value");
    text_desc.placeholder = utf16_view(L"placeholder");
    text_desc.maximum_length = 16u;
    CHECK(block.api.add_text_input(
        provider, sound, &text_desc, &text_row) == ERUI_OK);
    CHECK(text_row != ERUI_INVALID_ROW);

    color_desc.size = sizeof(color_desc);
    color_desc.label = utf16_view(L"Frozen color");
    color_desc.initial_value.red = 12u;
    color_desc.initial_value.green = 34u;
    color_desc.initial_value.blue = 56u;
    color_desc.enabled = 1u;
    CHECK(block.api.add_color_picker(
        provider, sound, &color_desc, &color_row) == ERUI_OK);
    CHECK(color_row != ERUI_INVALID_ROW);

    section_desc.size = sizeof(section_desc);
    section_desc.label = utf16_view(L"Frozen bindings");
    CHECK(block.api.add_input_section(
        provider, &section_desc, &section) == ERUI_OK);
    CHECK(section != ERUI_INVALID_INPUT_SECTION);

    action_desc.size = sizeof(action_desc);
    action_desc.action_id = string_view("frozen-action");
    action_desc.label = utf16_view(L"Frozen action");
    action_desc.default_inputs.size = sizeof(action_desc.default_inputs);
    action_desc.default_inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
    action_desc.default_inputs.keyboard.input = ERUI_KEYBOARD_KEY_Q;
    action_desc.activated_callback = &action_activated;
    CHECK(block.api.add_input_action(
        provider, section, &action_desc, &action) == ERUI_OK);
    CHECK(action != ERUI_INVALID_INPUT_ACTION);
    inputs.size = sizeof(inputs);
    CHECK(block.api.get_action_inputs(provider, action, &inputs) == ERUI_OK);
    CHECK(inputs.keyboard.state == ERUI_INPUT_SLOT_BOUND);
    CHECK(inputs.keyboard.input == ERUI_KEYBOARD_KEY_Q);

    storage_desc.size = sizeof(storage_desc);
    storage_desc.location = ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT;
    CHECK(block.api.open_storage(
        provider, &storage_desc, &storage) == ERUI_OK);
    CHECK(storage != ERUI_INVALID_STORAGE);
    CHECK(block.api.storage_load(provider, storage) == ERUI_OK);
    storage_info.size = sizeof(storage_info);
    CHECK(block.api.storage_get_info(
        provider, storage, &storage_info) == ERUI_OK);
    CHECK(storage_info.loaded == 1u);
    CHECK(block.api.storage_save(provider, storage) == ERUI_OK);

    CHECK(block.api.commit_provider(provider) == ERUI_OK);
    erui_unload_test_host(&host);
    return 0;
}
