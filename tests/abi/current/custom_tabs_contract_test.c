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

static ERUI_StringView string_view(const char* value) {
    ERUI_StringView result;
    memset(&result, 0, sizeof(result));
    result.data = value;
    result.length = (uint32_t)strlen(value);
    return result;
}

static ERUI_Utf16View utf16_view(const uint16_t* value, uint32_t length) {
    ERUI_Utf16View result;
    memset(&result, 0, sizeof(result));
    result.data = value;
    result.length = length;
    return result;
}

int main(int argc, char** argv) {
    static const uint16_t provider_name[] = {
        'C','u','s','t','o','m',' ','T','a','b',' ','A','B','I'};
    static const uint16_t tab_title[] = {
        'C','l','i','e','n','t',' ','S','e','t','t','i','n','g','s'};
    static const uint16_t row_label[] = {'E','n','a','b','l','e','d'};
    static const uint16_t title_line[] = {
        'C',' ','A','P','I',' ','t','i','t','l','e',' ','l','i','n','e'};
    uint8_t pixels[ERUI_TAB_ICON_RGBA8_BYTE_COUNT];
    ERUI_LoadedTestHost host;
    ERUI_Api api;
    ERUI_ProviderDesc provider_description;
    ERUI_ProviderHandle provider = ERUI_INVALID_PROVIDER;
    ERUI_PageHandle root = ERUI_INVALID_PAGE;
    ERUI_PageHandle tab = ERUI_INVALID_PAGE;
    ERUI_TabIconDesc icon;
    ERUI_TabDesc description;
    ERUI_ToggleDesc toggle;
    ERUI_RowHandle row = ERUI_INVALID_ROW;
    uint32_t offset;

    if (argc != 2) return 2;
    memset(&host, 0, sizeof(host));
    CHECK(erui_load_test_host(argv[1], &host));

    memset(&api, 0, sizeof(api));
    api.size = ERUI_API_V1_3_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_3, &api) == ERUI_OK);
    CHECK(api.api_version == ERUI_API_VERSION_1_3);
    CHECK(api.size == ERUI_API_V1_3_SIZE);
    CHECK((api.capabilities & ERUI_CAP_CUSTOM_TABS) != 0u);
    CHECK(api.add_tab != NULL);
    CHECK((api.capabilities & ERUI_CAP_TITLE_SCREEN_TEXT) != 0u);
    CHECK(api.add_title_screen_text != NULL);

    memset(&provider_description, 0, sizeof(provider_description));
    provider_description.size = sizeof(provider_description);
    provider_description.api_version = ERUI_API_VERSION_1_3;
    provider_description.owner_module = GetModuleHandleW(NULL);
    provider_description.provider_id = string_view(
        "tests.current-v1-3-custom-tabs-c");
    provider_description.display_name = utf16_view(
        provider_name,
        (uint32_t)(sizeof(provider_name) / sizeof(provider_name[0])));
    CHECK(api.register_provider(
        &provider_description, &provider, &root) == ERUI_OK);

    {
        ERUI_TitleScreenTextDesc title_text;
        uint16_t maximum_line[ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS];
        uint16_t separated_line[3] = {'A', 0, 'B'};
        uint32_t index;
        memset(&title_text, 0, sizeof(title_text));
        title_text.size = sizeof(title_text);
        title_text.text = utf16_view(
            title_line,
            (uint32_t)(sizeof(title_line) / sizeof(title_line[0])));
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);

        title_text.corner = ERUI_TITLE_SCREEN_CORNER_TOP_LEFT;
        CHECK(api.add_title_screen_text(provider, &title_text) == ERUI_OK);

        title_text.flags = ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR;
        title_text.corner = ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT;
        title_text.color.red = 216u;
        title_text.color.green = 167u;
        title_text.color.blue = 103u;
        CHECK(api.add_title_screen_text(provider, &title_text) == ERUI_OK);

        title_text.size = sizeof(title_text) - 1u;
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);
        title_text.size = sizeof(title_text);
        title_text.flags = 0;
        memset(&title_text.color, 0, sizeof(title_text.color));
        for (index = 0; index < ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS;
             ++index) {
            maximum_line[index] = 'M';
        }
        title_text.text = utf16_view(
            maximum_line, ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS);
        title_text.corner = ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT;
        CHECK(api.add_title_screen_text(provider, &title_text) == ERUI_OK);

        title_text.text = utf16_view(
            title_line,
            (uint32_t)(sizeof(title_line) / sizeof(title_line[0])));
        title_text.corner = ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT;
        CHECK(api.add_title_screen_text(provider, &title_text) == ERUI_OK);
        title_text.corner = 5u;
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);
        title_text.corner = UINT32_MAX;
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);

        title_text.text = utf16_view(separated_line, 3u);
        title_text.corner = ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT;
        separated_line[1] = UINT16_C(0x0085);
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);
        separated_line[1] = UINT16_C(0x2028);
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);
        separated_line[1] = UINT16_C(0x2029);
        CHECK(api.add_title_screen_text(provider, &title_text) ==
            ERUI_INVALID_ARGUMENT);
    }

    {
        uint16_t oversized_title[
            ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS + 1u];
        ERUI_TabDesc oversized;
        ERUI_PageHandle rejected = ERUI_INVALID_PAGE;
        uint32_t index;

        for (index = 0u;
             index < (uint32_t)(sizeof(oversized_title) /
                 sizeof(oversized_title[0]));
             ++index) {
            oversized_title[index] = (uint16_t)'X';
        }
        memset(&oversized, 0, sizeof(oversized));
        oversized.size = sizeof(oversized);
        oversized.tab_id = string_view("oversized-title");
        oversized.title = utf16_view(
            oversized_title,
            (uint32_t)(sizeof(oversized_title) /
                sizeof(oversized_title[0])));
        CHECK(api.add_tab(provider, &oversized, &rejected) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_PAGE);
    }

    memset(pixels, 0, sizeof(pixels));
    for (offset = 3u; offset < (uint32_t)sizeof(pixels); offset += 4u) {
        pixels[offset] = 255u;
    }
    memset(&icon, 0, sizeof(icon));
    icon.size = sizeof(icon);
    icon.format = ERUI_TAB_ICON_FORMAT_RGBA8;
    icon.byte_count = sizeof(pixels);
    icon.pixels = pixels;

    memset(&description, 0, sizeof(description));
    description.size = sizeof(description);
    description.tab_id = string_view("settings");
    description.title = utf16_view(
        tab_title, (uint32_t)(sizeof(tab_title) / sizeof(tab_title[0])));
    description.preferred_order = -25;
    description.icon = &icon;
    CHECK(api.add_tab(provider, &description, &tab) == ERUI_OK);
    CHECK(tab != ERUI_INVALID_PAGE);
    CHECK(tab != root);

    memset(&toggle, 0, sizeof(toggle));
    toggle.size = sizeof(toggle);
    toggle.label = utf16_view(
        row_label, (uint32_t)(sizeof(row_label) / sizeof(row_label[0])));
    toggle.initial_value = 1u;
    toggle.enabled = 1u;
    CHECK(api.add_toggle(provider, tab, &toggle, &row) == ERUI_OK);
    CHECK(row != ERUI_INVALID_ROW);

    {
        ERUI_PageHandle duplicate = ERUI_INVALID_PAGE;
        CHECK(api.add_tab(provider, &description, &duplicate) ==
            ERUI_DUPLICATE_TAB_ID);
    }
    {
        ERUI_TabDesc fallback = description;
        ERUI_PageHandle fallback_page = ERUI_INVALID_PAGE;
        fallback.tab_id = string_view("fallback");
        fallback.icon = NULL;
        fallback.preferred_order = 100;
        CHECK(api.add_tab(provider, &fallback, &fallback_page) == ERUI_OK);
        CHECK(fallback_page != ERUI_INVALID_PAGE);
    }
    {
        ERUI_TabDesc alpha = description;
        ERUI_TabIconDesc alpha_icon = icon;
        ERUI_PageHandle alpha_page = ERUI_INVALID_PAGE;
        alpha.tab_id = string_view("transparent");
        pixels[3] = 0u;
        pixels[7] = 127u;
        pixels[11] = 255u;
        alpha_icon.format = ERUI_TAB_ICON_FORMAT_RGBA8;
        alpha.icon = &alpha_icon;
        CHECK(api.add_tab(provider, &alpha, &alpha_page) == ERUI_OK);
        CHECK(alpha_page != ERUI_INVALID_PAGE);
        pixels[3] = 255u;
        pixels[7] = 255u;
    }
    {
        ERUI_TabDesc invalid = description;
        ERUI_TabIconDesc invalid_icon = icon;
        ERUI_PageHandle rejected = ERUI_INVALID_PAGE;
        invalid.tab_id = string_view("invalid-format");
        invalid_icon.format = 2u;
        invalid.icon = &invalid_icon;
        CHECK(api.add_tab(provider, &invalid, &rejected) ==
            ERUI_INVALID_ARGUMENT);
    }
    {
        uint32_t index;
        for (index = 3u; index < ERUI_CUSTOM_TAB_MAX_COUNT; ++index) {
            char id[32];
            ERUI_TabDesc additional = description;
            ERUI_PageHandle additional_page = ERUI_INVALID_PAGE;
            CHECK(snprintf(id, sizeof(id), "tab-%u", index) > 0);
            additional.tab_id = string_view(id);
            additional.icon = NULL;
            additional.preferred_order = (int32_t)index;
            CHECK(api.add_tab(
                provider, &additional, &additional_page) == ERUI_OK);
            CHECK(additional_page != ERUI_INVALID_PAGE);
        }
    }
    {
        ERUI_TabDesc overflow = description;
        ERUI_PageHandle rejected = ERUI_INVALID_PAGE;
        overflow.tab_id = string_view("overflow");
        overflow.icon = NULL;
        CHECK(api.add_tab(provider, &overflow, &rejected) ==
            ERUI_TAB_LIMIT_REACHED);
    }
    CHECK(api.commit_provider(provider) == ERUI_OK);

    erui_unload_test_host(&host);
    return 0;
}
