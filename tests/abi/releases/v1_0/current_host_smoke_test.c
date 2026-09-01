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

static void ERUI_CALL changed(void* user_data, uint8_t value) {
    uint8_t* observed = (uint8_t*)user_data;
    if (observed) *observed = value;
}

int main(int argc, char** argv) {
    typedef struct ApiBlock {
        ERUI_Api api;
        unsigned char canary[32];
    } ApiBlock;
    ERUI_LoadedTestHost host = {0};
    ApiBlock block;
    ERUI_ProviderDesc provider = {0};
    ERUI_ProviderHandle provider_handle = ERUI_INVALID_PROVIDER;
    ERUI_PageHandle root = ERUI_INVALID_PAGE;
    ERUI_ToggleDesc toggle = {0};
    ERUI_RowHandle toggle_row = ERUI_INVALID_ROW;
    uint8_t observed = 0;
    uint8_t value = 0;
    size_t index;

    _Static_assert(sizeof(ERUI_Api) == 128u,
        "test must compile against the frozen v1.0 table");
    if (argc != 2) return 2;
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != NULL);

    memset(&block, 0, sizeof(block));
    memset(block.canary, 0xA5, sizeof(block.canary));
    block.api.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &block.api) == ERUI_OK);
    CHECK(block.api.size == ERUI_API_V1_0_SIZE);
    CHECK(block.api.api_version == ERUI_API_VERSION_1_0);
    for (index = 0; index < sizeof(block.canary); ++index) {
        CHECK(block.canary[index] == 0xA5u);
    }

    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_0;
    provider.owner_module = GetModuleHandleW(NULL);
    provider.provider_id = string_view("tests.v1-0-to-current-smoke");
    provider.display_name = utf16_view(L"Frozen client to current host");
    CHECK(block.api.register_provider(
        &provider, &provider_handle, &root) == ERUI_OK);

    toggle.size = sizeof(toggle);
    toggle.label = utf16_view(L"Frozen toggle");
    toggle.help = utf16_view(L"ABI smoke test");
    toggle.changed_callback = &changed;
    toggle.user_data = &observed;
    toggle.initial_value = 1u;
    toggle.enabled = 1u;
    CHECK(block.api.add_toggle(
        provider_handle, root, &toggle, &toggle_row) == ERUI_OK);
    CHECK(block.api.commit_provider(provider_handle) == ERUI_OK);
    CHECK(block.api.set_row_value(provider_handle, toggle_row, 0u) == ERUI_OK);
    CHECK(block.api.get_row_value(
        provider_handle, toggle_row, &value) == ERUI_OK);
    CHECK(value == 0u);

    erui_unload_test_host(&host);
    return 0;
}
