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

static unsigned g_callback_count;

static void ERUI_CALL text_changed(
    void* user_data,
    const ERUI_TextInputChangeContext* context) {
    unsigned* count = (unsigned*)user_data;
    if (count) ++*count;
    if (context && context->size == sizeof(*context)) {
        ++g_callback_count;
    }
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

static void fill_units(uint16_t* output, uint32_t length, uint16_t value) {
    uint32_t index;
    for (index = 0; index < length; ++index) output[index] = value;
}

static int all_units_equal(
    const uint16_t* value,
    uint32_t length,
    uint16_t expected) {
    uint32_t index;
    for (index = 0; index < length; ++index) {
        if (value[index] != expected) return 0;
    }
    return 1;
}

int main(int argc, char** argv) {
    static const uint16_t provider_name[] = {
        'T','e','x','t','I','n','p','u','t',' ','A','B','I'};
    static const uint16_t label[] = {
        'B','o','u','n','d','a','r','y',' ','i','n','p','u','t'};
    static const uint16_t help[] = {
        'A','B','I',' ','c','o','p','y',' ','t','e','s','t'};
    static const uint16_t placeholder[] = {
        'E','n','t','e','r',' ','t','e','x','t'};
    static const uint32_t declared_limits[] = {0, 1, 3, 7, 16, 17, 35};
    static const uint32_t effective_limits[] = {16, 1, 3, 7, 16, 17, 35};
    ERUI_LoadedTestHost host;
    ERUI_Api api;
    ERUI_ProviderDesc provider_description;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle root;
    ERUI_RowHandle rows[sizeof(declared_limits) / sizeof(declared_limits[0])];
    unsigned callback_observation = 0;
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
    CHECK(api.api_version == ERUI_API_VERSION_1_1);
    CHECK(api.size == ERUI_API_V1_1_SIZE);
    CHECK((api.capabilities & ERUI_CAP_TEXT_INPUT) != 0);
    CHECK(api.add_text_input != NULL);
    CHECK(api.set_text_input_value != NULL);
    CHECK(api.get_text_input_value != NULL);

    memset(&provider_description, 0, sizeof(provider_description));
    provider_description.size = sizeof(provider_description);
    provider_description.api_version = ERUI_API_VERSION_1_1;
    provider_description.owner_module = GetModuleHandleW(NULL);
    provider_description.provider_id = string_view(
        "tests.current-v1-1-text-input-c");
    provider_description.display_name = utf16_view(
        provider_name,
        (uint32_t)(sizeof(provider_name) / sizeof(provider_name[0])));
    provider = ERUI_INVALID_PROVIDER;
    root = ERUI_INVALID_PAGE;
    CHECK(api.register_provider(
        &provider_description, &provider, &root) == ERUI_OK);

    for (index = 0; index < sizeof(rows) / sizeof(rows[0]); ++index) {
        uint16_t initial[ERUI_TEXT_INPUT_MAX_LENGTH];
        ERUI_TextInputDesc description;
        uint32_t effective = effective_limits[index];
        fill_units(initial, effective, 'A');
        memset(&description, 0, sizeof(description));
        description.size = sizeof(description);
        description.label = utf16_view(
            label, (uint32_t)(sizeof(label) / sizeof(label[0])));
        description.help = utf16_view(
            help, (uint32_t)(sizeof(help) / sizeof(help[0])));
        description.initial_value = utf16_view(initial, effective);
        description.placeholder = utf16_view(
            placeholder,
            (uint32_t)(sizeof(placeholder) / sizeof(placeholder[0])));
        description.changed_callback = text_changed;
        description.user_data = &callback_observation;
        description.maximum_length = declared_limits[index];
        rows[index] = ERUI_INVALID_ROW;
        CHECK(api.add_text_input(
            provider, root, &description, &rows[index]) == ERUI_OK);
        CHECK(rows[index] != ERUI_INVALID_ROW);
        /* Every descriptor view is borrowed only for this call. */
        fill_units(initial, effective, 'Z');
    }

    {
        ERUI_TextInputDesc invalid;
        ERUI_RowHandle rejected = UINT64_C(0xA55AA55AA55AA55A);
        memset(&invalid, 0, sizeof(invalid));
        invalid.size = sizeof(invalid);
        invalid.label = utf16_view(
            label, (uint32_t)(sizeof(label) / sizeof(label[0])));
        invalid.maximum_length = ERUI_TEXT_INPUT_MAX_LENGTH + 1u;
        CHECK(api.add_text_input(
            provider, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
        CHECK(rejected == ERUI_INVALID_ROW);
    }

    CHECK(api.commit_provider(provider) == ERUI_OK);

    for (index = 0; index < sizeof(rows) / sizeof(rows[0]); ++index) {
        uint16_t output[ERUI_TEXT_INPUT_MAX_LENGTH];
        uint16_t exact[ERUI_TEXT_INPUT_MAX_LENGTH];
        uint16_t too_long[ERUI_TEXT_INPUT_MAX_LENGTH + 1u];
        ERUI_Utf16View value;
        uint32_t required = UINT32_C(0xA5A5A5A5);
        uint32_t effective = effective_limits[index];

        CHECK(api.get_text_input_value(
            provider, rows[index], NULL, 0, &required) == ERUI_OK);
        CHECK(required == effective);
        memset(output, 0, sizeof(output));
        CHECK(api.get_text_input_value(
            provider, rows[index], output, effective, &required) == ERUI_OK);
        CHECK(required == effective);
        CHECK(all_units_equal(output, effective, 'A'));

        fill_units(output, ERUI_TEXT_INPUT_MAX_LENGTH, UINT16_C(0xA5A5));
        required = 0;
        CHECK(api.get_text_input_value(
            provider,
            rows[index],
            output,
            effective - 1u,
            &required) == ERUI_BUFFER_TOO_SMALL);
        CHECK(required == effective);
        CHECK(all_units_equal(
            output, ERUI_TEXT_INPUT_MAX_LENGTH, UINT16_C(0xA5A5)));

        fill_units(exact, effective, 'B');
        value = utf16_view(exact, effective);
        CHECK(api.set_text_input_value(
            provider, rows[index], &value) == ERUI_OK);
        CHECK(callback_observation == 0);
        CHECK(g_callback_count == 0);

        fill_units(too_long, effective + 1u, 'C');
        value = utf16_view(too_long, effective + 1u);
        CHECK(api.set_text_input_value(
            provider, rows[index], &value) == ERUI_INVALID_ARGUMENT);

        memset(output, 0, sizeof(output));
        required = 0;
        CHECK(api.get_text_input_value(
            provider, rows[index], output, effective, &required) == ERUI_OK);
        CHECK(required == effective);
        CHECK(all_units_equal(output, effective, 'B'));
    }

    erui_unload_test_host(&host);
    return 0;
}
