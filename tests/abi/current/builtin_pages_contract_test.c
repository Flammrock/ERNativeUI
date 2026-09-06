#include "host_loader.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static void ERUI_CALL button_pressed(void* user_data) {
    (void)user_data;
}

static ERUI_StringView string_view(const char* value) {
    ERUI_StringView result;
    memset(&result, 0, sizeof(result));
    result.data = value;
    result.length = (uint32_t)strlen(value);
    return result;
}

static ERUI_Utf16View utf16_view(
    const uint16_t* value,
    uint32_t length) {
    ERUI_Utf16View result;
    memset(&result, 0, sizeof(result));
    result.data = value;
    result.length = length;
    return result;
}

static ERUI_Result register_provider(
    const ERUI_Api* api,
    uint32_t api_version,
    const char* id,
    ERUI_ProviderHandle* out_provider,
    ERUI_PageHandle* out_root) {
    static const uint16_t display_name[] = {
        'B','u','i','l','t','-','i','n',' ','p','a','g','e','s'};
    ERUI_ProviderDesc description;
    memset(&description, 0, sizeof(description));
    description.size = sizeof(description);
    description.api_version = api_version;
    description.owner_module = GetModuleHandleW(NULL);
    description.provider_id = string_view(id);
    description.display_name = utf16_view(
        display_name,
        (uint32_t)(sizeof(display_name) / sizeof(display_name[0])));
    return api->register_provider(&description, out_provider, out_root);
}

int main(int argc, char** argv) {
    static const ERUI_BuiltinPage builtin_pages[] = {
        ERUI_BUILTIN_PAGE_GAME_OPTIONS,
        ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
        ERUI_BUILTIN_PAGE_DISPLAY,
        ERUI_BUILTIN_PAGE_SOUND,
        ERUI_BUILTIN_PAGE_NETWORK,
        ERUI_BUILTIN_PAGE_KEYBOARD_MOUSE,
        ERUI_BUILTIN_PAGE_GRAPHICS
    };
    static const uint16_t row_label[] = {
        'B','u','i','l','t','-','i','n',' ','r','o','w'};
    ERUI_LoadedTestHost host;
    ERUI_Api api;
    ERUI_Api api_v1_0;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle root;
    ERUI_PageHandle pages[ERUI_BUILTIN_PAGE_COUNT];
    size_t index;

    if (argc != 2) return 2;
    memset(&host, 0, sizeof(host));
    CHECK(erui_load_test_host(argv[1], &host));

    if (host.get_control) {
        ERUI_CompatControl control;
        memset(&control, 0, sizeof(control));
        control.size = sizeof(control);
        CHECK(host.get_control(
            ERUI_COMPAT_CONTROL_VERSION, &control) == ERUI_OK);
        control.reset();
    }

    memset(&api, 0, sizeof(api));
    api.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &api) == ERUI_OK);
    CHECK(api.size == ERUI_API_V1_1_SIZE);
    CHECK((api.capabilities & ERUI_CAP_BUILTIN_PAGES) != 0);
    CHECK(api.get_builtin_page != NULL);

    provider = ERUI_INVALID_PROVIDER;
    root = ERUI_INVALID_PAGE;
    CHECK(register_provider(
        &api,
        ERUI_API_VERSION_1_1,
        "tests.current-v1-1-builtin-pages-c",
        &provider,
        &root) == ERUI_OK);
    CHECK(provider != ERUI_INVALID_PROVIDER);
    CHECK(root != ERUI_INVALID_PAGE);

    for (index = 0; index < ERUI_BUILTIN_PAGE_COUNT; ++index) {
        ERUI_PageHandle repeated = ERUI_INVALID_PAGE;
        ERUI_ButtonDesc description;
        ERUI_RowHandle row = ERUI_INVALID_ROW;
        size_t previous;

        pages[index] = ERUI_INVALID_PAGE;
        CHECK(api.get_builtin_page(
            provider, builtin_pages[index], &pages[index]) == ERUI_OK);
        CHECK(pages[index] != ERUI_INVALID_PAGE);
        CHECK(api.get_builtin_page(
            provider, builtin_pages[index], &repeated) == ERUI_OK);
        CHECK(repeated == pages[index]);
        for (previous = 0; previous < index; ++previous) {
            CHECK(pages[index] != pages[previous]);
        }

        memset(&description, 0, sizeof(description));
        description.size = sizeof(description);
        description.label = utf16_view(
            row_label,
            (uint32_t)(sizeof(row_label) / sizeof(row_label[0])));
        description.callback = &button_pressed;
        description.enabled = 1u;
        CHECK(api.add_button(
            provider, pages[index], &description, &row) == ERUI_OK);
        CHECK(row != ERUI_INVALID_ROW);
    }
    CHECK(pages[ERUI_BUILTIN_PAGE_GAME_OPTIONS] == root);

    {
        ERUI_PageHandle output = UINT64_C(0xA55AA55AA55AA55A);
        CHECK(api.get_builtin_page(
            provider, ERUI_BUILTIN_PAGE_COUNT, &output) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(output == ERUI_INVALID_PAGE);
        output = UINT64_C(0xA55AA55AA55AA55A);
        CHECK(api.get_builtin_page(
            provider, UINT32_MAX, &output) == ERUI_INVALID_ARGUMENT);
        CHECK(output == ERUI_INVALID_PAGE);
        CHECK(api.get_builtin_page(
            provider, ERUI_BUILTIN_PAGE_SOUND, NULL) ==
            ERUI_INVALID_ARGUMENT);
        output = UINT64_C(0xA55AA55AA55AA55A);
        CHECK(api.get_builtin_page(
            ERUI_INVALID_PROVIDER,
            ERUI_BUILTIN_PAGE_SOUND,
            &output) == ERUI_INVALID_HANDLE);
        CHECK(output == UINT64_C(0xA55AA55AA55AA55A));
    }

    memset(&api_v1_0, 0, sizeof(api_v1_0));
    api_v1_0.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &api_v1_0) == ERUI_OK);
    {
        ERUI_ProviderHandle provider_v1_0 = ERUI_INVALID_PROVIDER;
        ERUI_PageHandle root_v1_0 = ERUI_INVALID_PAGE;
        ERUI_PageHandle output = UINT64_C(0xA55AA55AA55AA55A);
        CHECK(register_provider(
            &api_v1_0,
            ERUI_API_VERSION_1_0,
            "tests.v1-0-rejects-builtin-pages-c",
            &provider_v1_0,
            &root_v1_0) == ERUI_OK);
        CHECK(api.get_builtin_page(
            provider_v1_0,
            ERUI_BUILTIN_PAGE_GAME_OPTIONS,
            &output) == ERUI_NOT_SUPPORTED);
        CHECK(output == UINT64_C(0xA55AA55AA55AA55A));
        CHECK(api_v1_0.abort_provider(provider_v1_0) == ERUI_OK);
    }

    CHECK(api.commit_provider(provider) == ERUI_OK);
    erui_unload_test_host(&host);
    return 0;
}
