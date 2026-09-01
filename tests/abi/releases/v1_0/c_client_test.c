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

typedef struct Observation {
    unsigned button_calls;
    unsigned value_calls;
    unsigned alert_calls;
    uint8_t last_value;
    ERUI_Result alert_result;
    ERUI_AlertResponse alert_response;
} Observation;

static ERUI_Utf16View utf16_view(const wchar_t* text) {
    ERUI_Utf16View view = {0};
    view.data = (const uint16_t*)text;
    view.length = (uint32_t)wcslen(text);
    return view;
}

static ERUI_StringView string_view(const char* text) {
    ERUI_StringView view = {0};
    view.data = text;
    view.length = (uint32_t)strlen(text);
    return view;
}

static void ERUI_CALL button_callback(void* user_data) {
    Observation* observation = (Observation*)user_data;
    if (observation) ++observation->button_calls;
}

static void ERUI_CALL value_callback(void* user_data, uint8_t value) {
    Observation* observation = (Observation*)user_data;
    if (!observation) return;
    ++observation->value_calls;
    observation->last_value = value;
}

static void ERUI_CALL alert_callback(
    void* user_data,
    ERUI_Result result,
    ERUI_AlertResponse response) {
    Observation* observation = (Observation*)user_data;
    if (!observation) return;
    ++observation->alert_calls;
    observation->alert_result = result;
    observation->alert_response = response;
}

static ERUI_Result ERUI_CALL title_formatter(
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    uint16_t* output,
    uint32_t output_capacity,
    uint32_t* out_length) {
    static const wchar_t title[] = L"Frozen title 2/3";
    const uint32_t length = (uint32_t)(sizeof(title) / sizeof(title[0]) - 1u);
    if (!user_data || !context || context->size != sizeof(*context) ||
        context->page_number != 2u || context->page_count != 3u ||
        !output || !out_length || output_capacity < length) {
        return ERUI_INVALID_ARGUMENT;
    }
    memcpy(output, title, (size_t)length * sizeof(uint16_t));
    *out_length = length;
    return ERUI_OK;
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
    ERUI_LoadedTestHost host = {0};
    ERUI_CompatControl control = {0};
    ApiBlock block;
    ERUI_Capabilities required_capabilities;
    ERUI_ProviderDesc provider = {0};
    ERUI_ProviderHandle provider_handle = ERUI_INVALID_PROVIDER;
    ERUI_PageHandle root = ERUI_INVALID_PAGE;
    ERUI_PageHandle child = ERUI_INVALID_PAGE;
    ERUI_RowHandle button = ERUI_INVALID_ROW;
    ERUI_RowHandle toggle = ERUI_INVALID_ROW;
    ERUI_RowHandle slider = ERUI_INVALID_ROW;
    ERUI_RowHandle inline_choice = ERUI_INVALID_ROW;
    ERUI_RowHandle popup_choice = ERUI_INVALID_ROW;
    ERUI_RowHandle submenu_row = ERUI_INVALID_ROW;
    Observation observation = {0};
    wchar_t mutable_label[] = L"Frozen button";
    wchar_t first_choice[] = L"First";
    wchar_t second_choice[] = L"Second";
    ERUI_Utf16View choices[2];
    ERUI_ButtonDesc button_desc = {0};
    ERUI_ToggleDesc toggle_desc = {0};
    ERUI_SliderDesc slider_desc = {0};
    ERUI_ChoiceDesc choice_desc = {0};
    ERUI_SubmenuDesc submenu_desc = {0};
    ERUI_PagePresentationDesc presentation = {0};
    ERUI_AlertDesc alert = {0};
    ERUI_GameLanguageInfo language = {0};
    uint16_t copied[64];
    uint32_t copied_length = 0;
    uint8_t value = 0;
    int formatter_token = 1;

    _Static_assert(sizeof(wchar_t) == sizeof(uint16_t),
        "Windows wchar_t must be UTF-16");
    _Static_assert(sizeof(ERUI_Api) == 128u, "not a frozen v1.0 client");
    _Static_assert(offsetof(ApiBlock, canary) == 128u,
        "unexpected API block padding");

    if (argc != 2) return 2;
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != NULL && host.get_control != NULL);
    control.size = sizeof(control);
    CHECK(host.get_control(ERUI_COMPAT_CONTROL_VERSION, &control) == ERUI_OK);
    CHECK(control.size == ERUI_COMPAT_CONTROL_V1_SIZE);
    CHECK(control.version == ERUI_COMPAT_CONTROL_VERSION);
    CHECK(control.reset != NULL);
    control.reset();

    memset(&block, 0, sizeof(block));
    memset(block.canary, 0xA5, sizeof(block.canary));
    block.api.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &block.api) == ERUI_OK);
    CHECK(block.api.size == ERUI_API_V1_0_SIZE);
    CHECK(block.api.api_version == ERUI_API_VERSION_1_0);
    CHECK(all_bytes_equal(block.canary, sizeof(block.canary), 0xA5));

    required_capabilities = ERUI_CAP_TOGGLE | ERUI_CAP_SLIDER |
        ERUI_CAP_BUTTON | ERUI_CAP_SUBMENU | ERUI_CAP_PAGINATION |
        ERUI_CAP_HOST_OWNED_VALUES | ERUI_CAP_PAGE_PRESENTATION |
        ERUI_CAP_ALERT | ERUI_CAP_INLINE_CHOICE |
        ERUI_CAP_POPUP_CHOICE | ERUI_CAP_GAME_LANGUAGE;
    CHECK((block.api.capabilities & required_capabilities) ==
        required_capabilities);
    CHECK(block.api.register_provider && block.api.add_button &&
        block.api.add_toggle && block.api.add_slider &&
        block.api.add_inline_choice && block.api.add_popup_choice &&
        block.api.add_submenu && block.api.set_page_presentation &&
        block.api.commit_provider && block.api.abort_provider &&
        block.api.set_row_value && block.api.get_row_value &&
        block.api.enqueue_alert && block.api.get_game_language);

    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_0;
    provider.root_priority = 7;
    provider.owner_module = GetModuleHandleW(NULL);
    provider.provider_id = string_view("tests.frozen-v1-0-c");
    provider.display_name = utf16_view(L"Frozen v1.0 C client");
    CHECK(block.api.register_provider(
        &provider, &provider_handle, &root) == ERUI_OK);

    button_desc.size = sizeof(button_desc);
    button_desc.label = utf16_view(mutable_label);
    button_desc.help = utf16_view(L"Copied button help");
    button_desc.callback = &button_callback;
    button_desc.user_data = &observation;
    button_desc.enabled = 1u;
    CHECK(block.api.add_button(
        provider_handle, root, &button_desc, &button) == ERUI_OK);
    mutable_label[0] = L'X';

    toggle_desc.size = sizeof(toggle_desc);
    toggle_desc.label = utf16_view(L"Frozen toggle");
    toggle_desc.help = utf16_view(L"Toggle help");
    toggle_desc.changed_callback = &value_callback;
    toggle_desc.user_data = &observation;
    toggle_desc.enabled = 1u;
    CHECK(block.api.add_toggle(
        provider_handle, root, &toggle_desc, &toggle) == ERUI_OK);

    slider_desc.size = sizeof(slider_desc);
    slider_desc.label = utf16_view(L"Frozen slider");
    slider_desc.help = utf16_view(L"Slider help");
    slider_desc.changed_callback = &value_callback;
    slider_desc.user_data = &observation;
    slider_desc.minimum = 0;
    slider_desc.maximum = 100;
    slider_desc.step = 5;
    slider_desc.initial_value = 25;
    slider_desc.enabled = 1u;
    CHECK(block.api.add_slider(
        provider_handle, root, &slider_desc, &slider) == ERUI_OK);

    choices[0] = utf16_view(first_choice);
    choices[1] = utf16_view(second_choice);
    choice_desc.size = sizeof(choice_desc);
    choice_desc.label = utf16_view(L"Frozen choice");
    choice_desc.help = utf16_view(L"Choice help");
    choice_desc.changed_callback = &value_callback;
    choice_desc.user_data = &observation;
    choice_desc.options = choices;
    choice_desc.option_count = 2u;
    choice_desc.initial_index = 0u;
    CHECK(block.api.add_inline_choice(
        provider_handle, root, &choice_desc, &inline_choice) == ERUI_OK);
    CHECK(block.api.add_popup_choice(
        provider_handle, root, &choice_desc, &popup_choice) == ERUI_OK);
    first_choice[0] = L'X';

    submenu_desc.size = sizeof(submenu_desc);
    submenu_desc.label = utf16_view(L"Frozen submenu");
    submenu_desc.help = utf16_view(L"Submenu help");
    submenu_desc.page_title = utf16_view(L"Frozen child");
    submenu_desc.page_help = utf16_view(L"Child help");
    submenu_desc.enabled = 1u;
    CHECK(block.api.add_submenu(
        provider_handle, root, &submenu_desc,
        &child, &submenu_row) == ERUI_OK);

    presentation.size = sizeof(presentation);
    presentation.page_title = utf16_view(L"Frozen base title");
    presentation.formatter = &title_formatter;
    presentation.user_data = &formatter_token;
    CHECK(block.api.set_page_presentation(
        provider_handle, child, &presentation) == ERUI_OK);
    CHECK(block.api.commit_provider(provider_handle) == ERUI_OK);

    CHECK(control.get_row_label(
        button, copied, 64u, &copied_length) == ERUI_OK);
    CHECK(copied_length == wcslen(L"Frozen button"));
    CHECK(memcmp(copied, L"Frozen button",
        copied_length * sizeof(uint16_t)) == 0);
    CHECK(control.get_choice_text(
        inline_choice, 0u, copied, 64u, &copied_length) == ERUI_OK);
    CHECK(copied_length == wcslen(L"First"));
    CHECK(memcmp(copied, L"First", copied_length * sizeof(uint16_t)) == 0);

    CHECK(control.trigger_button(button) == ERUI_OK);
    CHECK(observation.button_calls == 1u);
    CHECK(control.trigger_value(toggle, 1u) == ERUI_OK);
    CHECK(observation.value_calls == 1u && observation.last_value == 1u);
    CHECK(block.api.set_row_value(provider_handle, toggle, 0u) == ERUI_OK);
    CHECK(block.api.get_row_value(provider_handle, toggle, &value) == ERUI_OK);
    CHECK(value == 0u);

    CHECK(control.format_page_title(
        child, 2u, 3u, copied, 64u, &copied_length) == ERUI_OK);
    CHECK(copied_length == wcslen(L"Frozen title 2/3"));

    alert.size = sizeof(alert);
    alert.message = utf16_view(L"Frozen alert");
    alert.callback = &alert_callback;
    alert.user_data = &observation;
    alert.buttons = ERUI_ALERT_BUTTONS_OK_CANCEL;
    alert.placement = ERUI_ALERT_PLACEMENT_CENTER;
    CHECK(block.api.enqueue_alert(provider_handle, &alert) == ERUI_OK);
    CHECK(control.complete_alert(
        ERUI_OK, ERUI_ALERT_RESPONSE_SECONDARY) == ERUI_OK);
    CHECK(observation.alert_calls == 1u);
    CHECK(observation.alert_result == ERUI_OK);
    CHECK(observation.alert_response == ERUI_ALERT_RESPONSE_SECONDARY);

    language.size = sizeof(language);
    CHECK(block.api.get_game_language(&language) == ERUI_OK);
    CHECK(language.known_language == ERUI_GAME_LANGUAGE_ENGLISH);
    CHECK(language.identifier.length == 7u);
    CHECK(memcmp(language.identifier.data, "english", 7u) == 0);
    CHECK(block.api.abort_provider(provider_handle) == ERUI_ALREADY_COMMITTED);

    erui_unload_test_host(&host);
    return 0;
}
