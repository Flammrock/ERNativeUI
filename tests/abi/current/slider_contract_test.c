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

typedef struct NumericObservation {
    unsigned calls;
    ERUI_NumericSliderChangeContext last;
} NumericObservation;

typedef struct CustomObservation {
    unsigned calls;
    ERUI_CustomSliderChangeContext last;
} CustomObservation;

static void ERUI_CALL numeric_changed(
    void* user_data,
    const ERUI_NumericSliderChangeContext* context) {
    NumericObservation* observation = (NumericObservation*)user_data;
    if (!observation || !context) return;
    ++observation->calls;
    observation->last = *context;
}

static void ERUI_CALL custom_changed(
    void* user_data,
    const ERUI_CustomSliderChangeContext* context) {
    CustomObservation* observation = (CustomObservation*)user_data;
    if (!observation || !context) return;
    ++observation->calls;
    observation->last = *context;
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

static ERUI_Utf16View utf16_view(const uint16_t* value, uint32_t length) {
    ERUI_Utf16View result;
    memset(&result, 0, sizeof(result));
    result.data = value;
    result.length = length;
    return result;
}

static int decimal_equal(
    ERUI_DecimalValue value,
    int64_t scaled_value,
    uint32_t decimal_places) {
    return value.scaled_value == scaled_value &&
        value.decimal_places == decimal_places && value.reserved == 0u;
}

int main(int argc, char** argv) {
    static const uint16_t provider_name[] = {
        'S','l','i','d','e','r',' ','A','B','I'};
    static const uint16_t legacy_name[] = {
        'L','e','g','a','c','y',' ','S','l','i','d','e','r'};
    static const uint16_t numeric_label[] = {
        'N','u','m','e','r','i','c',' ','S','l','i','d','e','r'};
    static const uint16_t custom_label[] = {
        'C','u','s','t','o','m',' ','S','l','i','d','e','r'};
    static const uint16_t help[] = {
        'A','P','I',' ','1','.','2',' ','s','l','i','d','e','r'};
    static const uint16_t prefix[] = {'x'};
    static const uint16_t suffix[] = {'s'};
    static const uint16_t custom_0[] = {'-','7'};
    static const uint16_t custom_1[] = {'3'};
    static const uint16_t custom_2[] = {'7','.','8'};
    static const uint16_t custom_3[] = {'1','s'};
    static const uint16_t custom_4[] = {'h','e','l','l','o'};
    static const uint16_t custom_5[] = {'1','8'};
    static const uint16_t custom_6[] = {'x','1','.','3'};
    const ERUI_Utf16View custom_values[] = {
        {custom_0, 2u, 0u},
        {custom_1, 1u, 0u},
        {custom_2, 3u, 0u},
        {custom_3, 2u, 0u},
        {custom_4, 5u, 0u},
        {custom_5, 2u, 0u},
        {custom_6, 4u, 0u},
    };
    ERUI_LoadedTestHost host;
    ERUI_Api api;
    ERUI_Api api_v1_1;
    ERUI_ProviderDesc provider_description;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle root;
    ERUI_RowHandle numeric_row;
    ERUI_RowHandle custom_row;
    ERUI_RowHandle button_row;
    NumericObservation numeric_observation;
    CustomObservation custom_observation;

    if (argc != 2) return 2;
    memset(&host, 0, sizeof(host));
    CHECK(erui_load_test_host(argv[1], &host));

    memset(&api, 0, sizeof(api));
    api.size = ERUI_API_V1_2_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_2, &api) == ERUI_OK);
    CHECK(api.api_version == ERUI_API_VERSION_1_2);
    CHECK(api.size == ERUI_API_V1_2_SIZE);
    CHECK((api.capabilities & ERUI_CAP_NUMERIC_SLIDER) != 0u);
    CHECK((api.capabilities & ERUI_CAP_CUSTOM_SLIDER) != 0u);
    CHECK(api.add_numeric_slider != NULL);
    CHECK(api.set_numeric_slider_value != NULL);
    CHECK(api.get_numeric_slider_value != NULL);
    CHECK(api.add_custom_slider != NULL);
    CHECK(api.set_custom_slider_index != NULL);
    CHECK(api.get_custom_slider_index != NULL);

    /* Selecting the frozen 1.1 prefix never leaks 1.2 capabilities or
       writes into the append-only slider tail. */
    memset(&api_v1_1, 0, sizeof(api_v1_1));
    api_v1_1.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &api_v1_1) == ERUI_OK);
    CHECK(api_v1_1.api_version == ERUI_API_VERSION_1_1);
    CHECK(api_v1_1.size == ERUI_API_V1_1_SIZE);
    CHECK((api_v1_1.capabilities & ERUI_CAP_NUMERIC_SLIDER) == 0u);
    CHECK((api_v1_1.capabilities & ERUI_CAP_CUSTOM_SLIDER) == 0u);
    CHECK(api_v1_1.add_numeric_slider == NULL);
    CHECK(api_v1_1.add_custom_slider == NULL);

    /* A provider negotiated as 1.1 cannot borrow the 1.2 entry points. */
    {
        ERUI_ProviderDesc legacy_description;
        ERUI_ProviderHandle legacy_provider = ERUI_INVALID_PROVIDER;
        ERUI_PageHandle legacy_root = ERUI_INVALID_PAGE;
        ERUI_NumericSliderDesc unsupported;
        ERUI_RowHandle untouched = UINT64_C(0xA55AA55AA55AA55A);
        memset(&legacy_description, 0, sizeof(legacy_description));
        legacy_description.size = sizeof(legacy_description);
        legacy_description.api_version = ERUI_API_VERSION_1_1;
        legacy_description.owner_module = GetModuleHandleW(NULL);
        legacy_description.provider_id = string_view(
            "tests.current-v1-1-slider-rejection-c");
        legacy_description.display_name = utf16_view(
            legacy_name,
            (uint32_t)(sizeof(legacy_name) / sizeof(legacy_name[0])));
        CHECK(api_v1_1.register_provider(
            &legacy_description, &legacy_provider, &legacy_root) == ERUI_OK);
        memset(&unsupported, 0, sizeof(unsupported));
        unsupported.size = sizeof(unsupported);
        unsupported.label = utf16_view(
            numeric_label,
            (uint32_t)(sizeof(numeric_label) / sizeof(numeric_label[0])));
        unsupported.minimum_scaled_value = 0;
        unsupported.maximum_scaled_value = 1;
        unsupported.step_scaled_value = 1;
        unsupported.enabled = 1u;
        CHECK(api.add_numeric_slider(
            legacy_provider, legacy_root, &unsupported, &untouched) ==
            ERUI_NOT_SUPPORTED);
        CHECK(untouched == UINT64_C(0xA55AA55AA55AA55A));
        CHECK(api_v1_1.abort_provider(legacy_provider) == ERUI_OK);
    }

    memset(&provider_description, 0, sizeof(provider_description));
    provider_description.size = sizeof(provider_description);
    provider_description.api_version = ERUI_API_VERSION_1_2;
    provider_description.owner_module = GetModuleHandleW(NULL);
    provider_description.provider_id = string_view(
        "tests.current-v1-2-sliders-c");
    provider_description.display_name = utf16_view(
        provider_name,
        (uint32_t)(sizeof(provider_name) / sizeof(provider_name[0])));
    provider = ERUI_INVALID_PROVIDER;
    root = ERUI_INVALID_PAGE;
    CHECK(api.register_provider(
        &provider_description, &provider, &root) == ERUI_OK);

    memset(&numeric_observation, 0, sizeof(numeric_observation));
    memset(&custom_observation, 0, sizeof(custom_observation));
    {
        ERUI_NumericSliderDesc description;
        memset(&description, 0, sizeof(description));
        description.size = sizeof(description);
        description.label = utf16_view(
            numeric_label,
            (uint32_t)(sizeof(numeric_label) / sizeof(numeric_label[0])));
        description.help = utf16_view(
            help, (uint32_t)(sizeof(help) / sizeof(help[0])));
        description.changed_callback = &numeric_changed;
        description.user_data = &numeric_observation;
        description.minimum_scaled_value = -10;
        description.maximum_scaled_value = 10;
        description.step_scaled_value = 2;
        description.initial_scaled_value = -5;
        description.decimal_places = 1u;
        description.enabled = 1u;
        description.prefix = utf16_view(prefix, 1u);
        description.suffix = utf16_view(suffix, 1u);
        numeric_row = ERUI_INVALID_ROW;
        CHECK(api.add_numeric_slider(
            provider, root, &description, &numeric_row) == ERUI_OK);
        CHECK(numeric_row != ERUI_INVALID_ROW);

        {
            ERUI_NumericSliderDesc invalid = description;
            ERUI_RowHandle rejected;

            rejected = UINT64_C(0x1111111111111111);
            invalid.flags = 1u;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
            CHECK(rejected == ERUI_INVALID_ROW);

            invalid = description;
            invalid.decimal_places = ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES + 1u;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);

            invalid = description;
            invalid.step_scaled_value = 0;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);

            invalid = description;
            invalid.maximum_scaled_value = 11;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);

            invalid = description;
            invalid.maximum_scaled_value = 500;
            invalid.step_scaled_value = 1;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);

            invalid = description;
            invalid.initial_scaled_value = 11;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);

            invalid = description;
            invalid.changed_callback = NULL;
            CHECK(api.add_numeric_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
        }
    }

    {
        ERUI_CustomSliderDesc description;
        memset(&description, 0, sizeof(description));
        description.size = sizeof(description);
        description.label = utf16_view(
            custom_label,
            (uint32_t)(sizeof(custom_label) / sizeof(custom_label[0])));
        description.help = utf16_view(
            help, (uint32_t)(sizeof(help) / sizeof(help[0])));
        description.changed_callback = &custom_changed;
        description.user_data = &custom_observation;
        description.values = custom_values;
        description.value_count = 7u;
        description.initial_index = 4u;
        description.enabled = 1u;
        custom_row = ERUI_INVALID_ROW;
        CHECK(api.add_custom_slider(
            provider, root, &description, &custom_row) == ERUI_OK);
        CHECK(custom_row != ERUI_INVALID_ROW);
        CHECK(custom_row != numeric_row);

        {
            ERUI_CustomSliderDesc invalid = description;
            ERUI_Utf16View invalid_values[7];
            uint16_t too_long[ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH + 1u];
            uint32_t index;
            for (index = 0; index < 7u; ++index) {
                invalid_values[index] = custom_values[index];
            }
            for (index = 0; index <
                    ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH + 1u; ++index) {
                too_long[index] = 'x';
            }

            invalid.value_count = 1u;
            CHECK(api.add_custom_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
            invalid = description;
            invalid.value_count = ERUI_SLIDER_MAX_VALUE_COUNT + 1u;
            CHECK(api.add_custom_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
            invalid = description;
            invalid.initial_index = invalid.value_count;
            CHECK(api.add_custom_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
            invalid = description;
            invalid.values = NULL;
            CHECK(api.add_custom_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
            invalid = description;
            invalid_values[2].length = 0u;
            invalid.values = invalid_values;
            CHECK(api.add_custom_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
            invalid_values[2] = utf16_view(
                too_long, ERUI_CUSTOM_SLIDER_MAX_TEXT_LENGTH + 1u);
            CHECK(api.add_custom_slider(
                provider, root, &invalid, NULL) == ERUI_INVALID_ARGUMENT);
        }
    }

    {
        ERUI_ButtonDesc button;
        memset(&button, 0, sizeof(button));
        button.size = sizeof(button);
        button.label = utf16_view(
            custom_label,
            (uint32_t)(sizeof(custom_label) / sizeof(custom_label[0])));
        button.callback = &button_pressed;
        button.enabled = 1u;
        button_row = ERUI_INVALID_ROW;
        CHECK(api.add_button(provider, root, &button, &button_row) == ERUI_OK);
    }

    CHECK(api.commit_provider(provider) == ERUI_OK);

    {
        ERUI_DecimalValue value;
        memset(&value, 0, sizeof(value));
        CHECK(api.get_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_OK);
        /* -0.5 is off the 0.2 step grid and rounds down to -0.6. */
        CHECK(decimal_equal(value, -6, 1u));

        value.scaled_value = 3;
        value.decimal_places = 1u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_OK);
        CHECK(numeric_observation.calls == 0u);
        memset(&value, 0, sizeof(value));
        CHECK(api.get_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_OK);
        CHECK(decimal_equal(value, 2, 1u));

        /* A different scale is accepted only when conversion is exact. */
        value.scaled_value = 30;
        value.decimal_places = 2u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_OK);
        value.scaled_value = 0;
        value.decimal_places = 0u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_OK);
        memset(&value, 0, sizeof(value));
        CHECK(api.get_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_OK);
        CHECK(decimal_equal(value, 0, 1u));
        value.scaled_value = 35;
        value.decimal_places = 2u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_INVALID_ARGUMENT);
        value.scaled_value = 2;
        value.decimal_places = 0u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_INVALID_ARGUMENT);
        value.scaled_value = 0;
        value.decimal_places = 0u;
        value.reserved = 1u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_INVALID_ARGUMENT);
        value.reserved = 0u;
        value.decimal_places = ERUI_NUMERIC_SLIDER_MAX_DECIMAL_PLACES + 1u;
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, &value) == ERUI_INVALID_ARGUMENT);
        CHECK(api.set_numeric_slider_value(
            provider, numeric_row, NULL) == ERUI_INVALID_ARGUMENT);
        CHECK(api.get_numeric_slider_value(
            provider, numeric_row, NULL) == ERUI_INVALID_ARGUMENT);
        CHECK(api.get_numeric_slider_value(
            provider, button_row, &value) == ERUI_INVALID_ARGUMENT);
    }

    {
        uint32_t index = UINT32_MAX;
        CHECK(api.get_custom_slider_index(
            provider, custom_row, &index) == ERUI_OK);
        CHECK(index == 4u);
        CHECK(api.set_custom_slider_index(
            provider, custom_row, 6u) == ERUI_OK);
        CHECK(custom_observation.calls == 0u);
        index = UINT32_MAX;
        CHECK(api.get_custom_slider_index(
            provider, custom_row, &index) == ERUI_OK);
        CHECK(index == 6u);
        CHECK(api.set_custom_slider_index(
            provider, custom_row, 7u) == ERUI_INVALID_ARGUMENT);
        CHECK(api.get_custom_slider_index(
            provider, custom_row, NULL) == ERUI_INVALID_ARGUMENT);
        CHECK(api.set_custom_slider_index(
            provider, button_row, 0u) == ERUI_INVALID_ARGUMENT);
        CHECK(api.get_custom_slider_index(
            provider, button_row, &index) == ERUI_INVALID_ARGUMENT);
        CHECK(api.set_custom_slider_index(
            ERUI_INVALID_PROVIDER, custom_row, 0u) == ERUI_INVALID_HANDLE);
    }

    CHECK(numeric_observation.calls == 0u);
    CHECK(custom_observation.calls == 0u);
    erui_unload_test_host(&host);
    return 0;
}
