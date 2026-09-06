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

typedef struct ColorObservation {
    unsigned calls;
    ERUI_ProviderHandle provider;
    ERUI_RowHandle row;
    ERUI_Color value;
} ColorObservation;

static void ERUI_CALL color_changed(
    void* user_data,
    const ERUI_ColorPickerChangeContext* context) {
    ColorObservation* observation = (ColorObservation*)user_data;
    if (!observation || !context || context->size != sizeof(*context) ||
        context->flags != 0u || context->reserved != 0u ||
        context->value.reserved != 0u) {
        return;
    }
    ++observation->calls;
    observation->provider = context->provider;
    observation->row = context->row;
    observation->value = context->value;
}

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

static int colors_equal(ERUI_Color left, ERUI_Color right) {
    return left.red == right.red && left.green == right.green &&
        left.blue == right.blue && left.reserved == right.reserved;
}

static ERUI_Color color(uint8_t red, uint8_t green, uint8_t blue) {
    ERUI_Color result;
    memset(&result, 0, sizeof(result));
    result.red = red;
    result.green = green;
    result.blue = blue;
    return result;
}

int main(int argc, char** argv) {
    static const uint16_t provider_name[] = {
        'C','o','l','o','r','P','i','c','k','e','r',' ','A','B','I'};
    static const uint16_t legacy_provider_name[] = {
        'L','e','g','a','c','y',' ','C','o','l','o','r'};
    static const uint16_t first_label[] = {
        'P','r','i','m','a','r','y',' ','c','o','l','o','r'};
    static const uint16_t second_label[] = {
        'S','e','c','o','n','d','a','r','y',' ','c','o','l','o','r'};
    static const uint16_t help[] = {
        'O','p','e','n','s',' ','t','h','e',' ','n','a','t','i','v','e',' ',
        'R','G','B',' ','e','d','i','t','o','r'};
    ERUI_LoadedTestHost host;
    ERUI_Api api;
    ERUI_Api api_v1_0;
    ERUI_ProviderDesc provider_description;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle root;
    ERUI_RowHandle first_row;
    ERUI_RowHandle second_row;
    ERUI_RowHandle button_row;
    ColorObservation first_observation;
    ColorObservation second_observation;

    if (argc != 2) return 2;
    memset(&host, 0, sizeof(host));
    CHECK(erui_load_test_host(argv[1], &host));

    memset(&api, 0, sizeof(api));
    api.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &api) == ERUI_OK);
    CHECK(api.api_version == ERUI_API_VERSION_1_1);
    CHECK(api.size == ERUI_API_V1_1_SIZE);
    CHECK((api.capabilities & ERUI_CAP_COLOR_PICKER) != 0u);
    CHECK(api.add_color_picker != NULL);
    CHECK(api.set_color_picker_value != NULL);
    CHECK(api.get_color_picker_value != NULL);

    /* A provider negotiated through the frozen 1.0 prefix cannot consume a
       1.1 descriptor even if the caller obtained the function elsewhere. */
    memset(&api_v1_0, 0, sizeof(api_v1_0));
    api_v1_0.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &api_v1_0) == ERUI_OK);
    {
        ERUI_ProviderDesc legacy_description;
        ERUI_ProviderHandle legacy_provider = ERUI_INVALID_PROVIDER;
        ERUI_PageHandle legacy_root = ERUI_INVALID_PAGE;
        ERUI_ColorPickerDesc unsupported;
        ERUI_RowHandle unsupported_row = ERUI_INVALID_ROW;
        memset(&legacy_description, 0, sizeof(legacy_description));
        legacy_description.size = sizeof(legacy_description);
        legacy_description.api_version = ERUI_API_VERSION_1_0;
        legacy_description.owner_module = GetModuleHandleW(NULL);
        legacy_description.provider_id = string_view(
            "tests.current-v1-0-color-rejection-c");
        legacy_description.display_name = utf16_view(
            legacy_provider_name,
            (uint32_t)(sizeof(legacy_provider_name) /
                sizeof(legacy_provider_name[0])));
        CHECK(api_v1_0.register_provider(
            &legacy_description, &legacy_provider, &legacy_root) == ERUI_OK);

        memset(&unsupported, 0, sizeof(unsupported));
        unsupported.size = sizeof(unsupported);
        unsupported.label = utf16_view(
            first_label,
            (uint32_t)(sizeof(first_label) / sizeof(first_label[0])));
        unsupported.enabled = 1u;
        CHECK(api.add_color_picker(
            legacy_provider, legacy_root, &unsupported, &unsupported_row) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_0.abort_provider(legacy_provider) == ERUI_OK);
    }

    memset(&provider_description, 0, sizeof(provider_description));
    provider_description.size = sizeof(provider_description);
    provider_description.api_version = ERUI_API_VERSION_1_1;
    provider_description.owner_module = GetModuleHandleW(NULL);
    provider_description.provider_id = string_view(
        "tests.current-v1-1-color-picker-c");
    provider_description.display_name = utf16_view(
        provider_name,
        (uint32_t)(sizeof(provider_name) / sizeof(provider_name[0])));
    provider = ERUI_INVALID_PROVIDER;
    root = ERUI_INVALID_PAGE;
    CHECK(api.register_provider(
        &provider_description, &provider, &root) == ERUI_OK);

    memset(&first_observation, 0, sizeof(first_observation));
    memset(&second_observation, 0, sizeof(second_observation));
    {
        ERUI_ColorPickerDesc description;
        memset(&description, 0, sizeof(description));
        description.size = sizeof(description);
        description.label = utf16_view(
            first_label,
            (uint32_t)(sizeof(first_label) / sizeof(first_label[0])));
        description.help = utf16_view(
            help, (uint32_t)(sizeof(help) / sizeof(help[0])));
        description.changed_callback = &color_changed;
        description.user_data = &first_observation;
        description.initial_value = color(0u, 127u, 255u);
        description.enabled = 1u;
        first_row = ERUI_INVALID_ROW;
        CHECK(api.add_color_picker(
            provider, root, &description, &first_row) == ERUI_OK);
        CHECK(first_row != ERUI_INVALID_ROW);

        description.label = utf16_view(
            second_label,
            (uint32_t)(sizeof(second_label) / sizeof(second_label[0])));
        description.user_data = &second_observation;
        description.initial_value = color(255u, 1u, 0u);
        second_row = ERUI_INVALID_ROW;
        CHECK(api.add_color_picker(
            provider, root, &description, &second_row) == ERUI_OK);
        CHECK(second_row != ERUI_INVALID_ROW);
        CHECK(second_row != first_row);
    }

    {
        ERUI_ButtonDesc button;
        memset(&button, 0, sizeof(button));
        button.size = sizeof(button);
        button.label = utf16_view(
            first_label,
            (uint32_t)(sizeof(first_label) / sizeof(first_label[0])));
        button.callback = &button_pressed;
        button.enabled = 1u;
        button_row = ERUI_INVALID_ROW;
        CHECK(api.add_button(provider, root, &button, &button_row) == ERUI_OK);
    }

    {
        ERUI_ColorPickerDesc invalid;
        ERUI_RowHandle rejected;
        memset(&invalid, 0, sizeof(invalid));
        invalid.size = sizeof(invalid);
        invalid.label = utf16_view(
            first_label,
            (uint32_t)(sizeof(first_label) / sizeof(first_label[0])));
        invalid.enabled = 1u;

        rejected = UINT64_C(0x1111111111111111);
        invalid.flags = 1u;
        CHECK(api.add_color_picker(
            provider, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_ROW);

        rejected = UINT64_C(0x2222222222222222);
        invalid.flags = 0u;
        invalid.initial_value.reserved = 1u;
        CHECK(api.add_color_picker(
            provider, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_ROW);

        rejected = UINT64_C(0x3333333333333333);
        invalid.initial_value.reserved = 0u;
        invalid.enabled = 2u;
        CHECK(api.add_color_picker(
            provider, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_ROW);

        rejected = UINT64_C(0x4444444444444444);
        invalid.enabled = 1u;
        invalid.user_data = &first_observation;
        CHECK(api.add_color_picker(
            provider, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_ROW);
    }

    CHECK(api.commit_provider(provider) == ERUI_OK);

    {
        ERUI_Color first = color(99u, 99u, 99u);
        ERUI_Color second = color(99u, 99u, 99u);
        CHECK(api.get_color_picker_value(
            provider, first_row, &first) == ERUI_OK);
        CHECK(api.get_color_picker_value(
            provider, second_row, &second) == ERUI_OK);
        CHECK(colors_equal(first, color(0u, 127u, 255u)));
        CHECK(colors_equal(second, color(255u, 1u, 0u)));
    }

    {
        ERUI_Color programmed = color(255u, 255u, 255u);
        ERUI_Color first;
        ERUI_Color second;
        CHECK(api.set_color_picker_value(
            provider, first_row, &programmed) == ERUI_OK);
        /* Programmatic writes never masquerade as player confirmation. */
        CHECK(first_observation.calls == 0u);
        CHECK(second_observation.calls == 0u);

        memset(&first, 0, sizeof(first));
        memset(&second, 0, sizeof(second));
        CHECK(api.get_color_picker_value(
            provider, first_row, &first) == ERUI_OK);
        CHECK(api.get_color_picker_value(
            provider, second_row, &second) == ERUI_OK);
        CHECK(colors_equal(first, programmed));
        CHECK(colors_equal(second, color(255u, 1u, 0u)));

        programmed = color(0u, 0u, 0u);
        CHECK(api.set_color_picker_value(
            provider, second_row, &programmed) == ERUI_OK);
        CHECK(api.get_color_picker_value(
            provider, second_row, &second) == ERUI_OK);
        CHECK(colors_equal(second, programmed));
        CHECK(first_observation.calls == 0u);
        CHECK(second_observation.calls == 0u);
    }

    {
        ERUI_Color value = color(1u, 2u, 3u);
        ERUI_Color invalid = value;
        invalid.reserved = 1u;
        CHECK(api.set_color_picker_value(
            provider, first_row, &invalid) == ERUI_INVALID_ARGUMENT);
        CHECK(api.set_color_picker_value(
            provider, first_row, NULL) == ERUI_INVALID_ARGUMENT);
        CHECK(api.get_color_picker_value(
            provider, first_row, NULL) == ERUI_INVALID_ARGUMENT);
        CHECK(api.set_color_picker_value(
            provider, button_row, &value) == ERUI_INVALID_ARGUMENT);
        CHECK(api.get_color_picker_value(
            provider, button_row, &value) == ERUI_INVALID_ARGUMENT);
        CHECK(api.set_color_picker_value(
            ERUI_INVALID_PROVIDER, first_row, &value) == ERUI_INVALID_HANDLE);
        CHECK(api.get_color_picker_value(
            ERUI_INVALID_PROVIDER, first_row, &value) == ERUI_INVALID_HANDLE);
    }

    erui_unload_test_host(&host);
    return 0;
}
