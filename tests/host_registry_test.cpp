#include "host_registry.hpp"
#include "menu_compiler.hpp"

#include <Windows.h>

#include "test_assertions.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace {

ERUI_StringView ascii(const char* value) {
    ERUI_StringView result{};
    result.data = value;
    result.length = static_cast<std::uint32_t>(std::char_traits<char>::length(value));
    return result;
}

ERUI_Utf16View utf16(const wchar_t* value) {
    ERUI_Utf16View result{};
    result.data = reinterpret_cast<const std::uint16_t*>(value);
    result.length = static_cast<std::uint32_t>(std::char_traits<wchar_t>::length(value));
    return result;
}

ERUI_Utf16View utf16(std::wstring_view value) {
    ERUI_Utf16View result{};
    result.data = reinterpret_cast<const std::uint16_t*>(value.data());
    result.length = static_cast<std::uint32_t>(value.size());
    return result;
}

void ERUI_CALL changed(void* user_data, std::uint8_t value) {
    *static_cast<std::uint8_t*>(user_data) = value;
}

void ERUI_CALL pressed(void* user_data) {
    ++*static_cast<unsigned*>(user_data);
}

struct TextObservation {
    ERUI_ProviderHandle provider{};
    ERUI_RowHandle row{};
    std::wstring value{};
    unsigned calls{};
};

void ERUI_CALL text_changed(
    void* user_data,
    const ERUI_TextInputChangeContext* context) {
    auto& observation = *static_cast<TextObservation*>(user_data);
    ERUI_TEST_CHECK(context != nullptr);
    ERUI_TEST_CHECK(context->size == sizeof(*context));
    ERUI_TEST_CHECK(context->flags == 0);
    ERUI_TEST_CHECK(context->reserved[0] == 0 && context->reserved[1] == 0);
    observation.provider = context->provider;
    observation.row = context->row;
    observation.value.assign(context->value.length, L'\0');
    if (context->value.length != 0) {
        std::memcpy(
            observation.value.data(),
            context->value.data,
            static_cast<std::size_t>(context->value.length) *
                sizeof(std::uint16_t));
    }
    ++observation.calls;
}

struct FormatterObservation {
    ERUI_ProviderHandle provider{};
    ERUI_PageHandle page{};
    std::wstring base_title{};
    std::uint32_t calls{};
};

ERUI_Result ERUI_CALL format_page_title(
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    std::uint32_t* out_length) {
    auto& observation = *static_cast<FormatterObservation*>(user_data);
    ERUI_TEST_CHECK(context != nullptr);
    ERUI_TEST_CHECK(context->size == sizeof(*context));
    ERUI_TEST_CHECK(context->flags == 0);
    ERUI_TEST_CHECK(context->reserved[0] == 0 && context->reserved[1] == 0);
    ERUI_TEST_CHECK(context->page_number >= 1 &&
        context->page_number <= context->page_count);
    observation.provider = context->provider;
    observation.page = context->page;
    observation.base_title.assign(context->base_title.length, L'\0');
    std::memcpy(
        observation.base_title.data(),
        context->base_title.data,
        static_cast<std::size_t>(context->base_title.length) * sizeof(std::uint16_t));
    ++observation.calls;
    *out_length = 0;
    if (context->page_number == 2) return ERUI_INTERNAL_ERROR;
    const std::wstring title = L"Formatted " +
        std::to_wstring(context->page_number) + L"/" +
        std::to_wstring(context->page_count);
    if (title.size() > output_capacity) return ERUI_INVALID_ARGUMENT;
    std::memcpy(output, title.data(), title.size() * sizeof(std::uint16_t));
    *out_length = static_cast<std::uint32_t>(title.size());
    return ERUI_OK;
}

void verify_text_input_length_contract() {
    erui::host::Registry registry{};
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_1;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("com.example.text-input-lengths");
    provider.display_name = utf16(L"TextInput length contract");

    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    constexpr std::array<std::uint32_t, 7> declared_limits{
        0, 1, 3, 7, 16, 17, 35};
    constexpr std::array<std::uint32_t, 7> effective_limits{
        ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH, 1, 3, 7, 16, 17, 35};
    std::array<ERUI_RowHandle, declared_limits.size()> rows{};

    for (std::size_t index = 0; index < declared_limits.size(); ++index) {
        const std::wstring exact(effective_limits[index], L'A');
        ERUI_TextInputDesc description{};
        description.size = sizeof(description);
        description.label = utf16(L"Boundary input");
        description.help = utf16(L"Exact maximum test");
        description.initial_value = utf16(exact);
        description.placeholder = utf16(L"Placeholder copied by host");
        description.maximum_length = declared_limits[index];
        ERUI_TEST_CHECK(registry.add_text_input(
            provider_handle,
            root,
            &description,
            &rows[index]) == ERUI_OK);
        ERUI_TEST_CHECK(rows[index] != ERUI_INVALID_ROW);
    }

    ERUI_TextInputDesc invalid{};
    invalid.size = sizeof(invalid);
    invalid.label = utf16(L"Rejected input");
    invalid.maximum_length = ERUI_TEXT_INPUT_MAX_LENGTH + 1u;
    ERUI_RowHandle rejected{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(rejected == ERUI_INVALID_ROW);

    invalid.maximum_length = 3;
    invalid.initial_value = utf16(L"Four");
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &invalid, nullptr) == ERUI_INVALID_ARGUMENT);

    const std::uint16_t malformed_utf16[] = {0xD800u, 'x'};
    invalid.initial_value = {malformed_utf16, 2, 0};
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &invalid, nullptr) == ERUI_INVALID_ARGUMENT);

    invalid = {};
    invalid.size = static_cast<std::uint32_t>(sizeof(invalid) - 1u);
    invalid.label = utf16(L"Short descriptor");
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &invalid, nullptr) == ERUI_INVALID_ARGUMENT);

    invalid = {};
    invalid.size = sizeof(invalid);
    invalid.label = utf16(L"Reserved descriptor");
    invalid.reserved = 1;
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &invalid, nullptr) == ERUI_INVALID_ARGUMENT);

    invalid.reserved = 0;
    invalid.user_data = &registry;
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &invalid, nullptr) == ERUI_INVALID_ARGUMENT);

    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    for (std::size_t index = 0; index < rows.size(); ++index) {
        const std::wstring exact(effective_limits[index], L'B');
        const ERUI_Utf16View exact_view = utf16(exact);
        ERUI_TEST_CHECK(registry.set_text_input_value(
            provider_handle, rows[index], &exact_view) == ERUI_OK);

        const std::wstring too_long(effective_limits[index] + 1u, L'C');
        const ERUI_Utf16View too_long_view = utf16(too_long);
        ERUI_TEST_CHECK(registry.set_text_input_value(
            provider_handle, rows[index], &too_long_view) ==
            ERUI_INVALID_ARGUMENT);

        std::uint32_t required{};
        ERUI_TEST_CHECK(registry.get_text_input_value(
            provider_handle, rows[index], nullptr, 0, &required) == ERUI_OK);
        ERUI_TEST_CHECK(required == effective_limits[index]);
        std::wstring copied(required, L'\0');
        std::uint32_t copied_length{};
        ERUI_TEST_CHECK(registry.get_text_input_value(
            provider_handle,
            rows[index],
            reinterpret_cast<std::uint16_t*>(copied.data()),
            required,
            &copied_length) == ERUI_OK);
        ERUI_TEST_CHECK(copied_length == effective_limits[index]);
        ERUI_TEST_CHECK(copied == exact);
    }

    auto menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(menu->root().row_count() == declared_limits.size());
    auto compiled = erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() ==
        declared_limits.size());
    for (std::size_t index = 0; index < declared_limits.size(); ++index) {
        const auto& row = compiled->root_page().rows[index];
        ERUI_TEST_CHECK(row.text_input_state != nullptr);
        ERUI_TEST_CHECK(row.text_input_state->maximum_length() ==
            effective_limits[index]);
    }
}

} // namespace

int main() {
    erui::host::set_api_state(erui::host::ApiState::accepting);

    verify_text_input_length_contract();

    ERUI_Api short_table{};
    short_table.size = ERUI_API_V1_0_SIZE - 1u;
    ERUI_TEST_CHECK(ERUI_GetApi(ERUI_API_VERSION_CURRENT, &short_table) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_Api short_v1_1_table{};
    short_v1_1_table.size = ERUI_API_V1_1_SIZE - 1u;
    ERUI_TEST_CHECK(ERUI_GetApi(
        ERUI_API_VERSION_1_1, &short_v1_1_table) == ERUI_INVALID_ARGUMENT);

    struct ApiWithCanary {
        ERUI_Api table{};
        std::uint64_t canary{UINT64_C(0xA55AA55AF00DF00D)};
    } negotiated{};
    negotiated.table.size = sizeof(negotiated.table);
    ERUI_TEST_CHECK(ERUI_GetApi(ERUI_VERSION_ENCODE(1u, 99u), &negotiated.table) ==
        ERUI_UNSUPPORTED_VERSION);
    ERUI_TEST_CHECK(ERUI_GetApi(ERUI_API_VERSION_CURRENT, &negotiated.table) == ERUI_OK);
    ERUI_TEST_CHECK(negotiated.table.api_version == ERUI_API_VERSION_CURRENT);
    ERUI_TEST_CHECK(negotiated.table.size == sizeof(ERUI_Api));
    ERUI_TEST_CHECK((negotiated.table.capabilities & ERUI_CAP_ALERT) != 0);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_INLINE_CHOICE) != 0);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_POPUP_CHOICE) != 0);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_GAME_LANGUAGE) != 0);
    ERUI_TEST_CHECK(negotiated.table.add_inline_choice != nullptr);
    ERUI_TEST_CHECK(negotiated.table.add_popup_choice != nullptr);
    ERUI_TEST_CHECK(negotiated.table.enqueue_alert != nullptr);
    ERUI_TEST_CHECK(negotiated.table.get_game_language != nullptr);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_TEXT_INPUT) != 0);
    ERUI_TEST_CHECK(negotiated.table.add_text_input != nullptr);
    ERUI_TEST_CHECK(negotiated.table.set_text_input_value != nullptr);
    ERUI_TEST_CHECK(negotiated.table.get_text_input_value != nullptr);
    ERUI_TEST_CHECK(negotiated.canary == UINT64_C(0xA55AA55AF00DF00D));

    // An exact 1.0 request writes only the frozen 128-byte prefix. The newer
    // table tail is a canary, not writable capacity selected by that version.
    alignas(ERUI_Api) std::array<std::byte, ERUI_API_V1_0_SIZE + 32>
        v1_storage{};
    std::memset(v1_storage.data(), 0xA5, v1_storage.size());
    auto* const v1_table = reinterpret_cast<ERUI_Api*>(v1_storage.data());
    v1_table->size = ERUI_API_V1_0_SIZE;
    ERUI_TEST_CHECK(ERUI_GetApi(ERUI_API_VERSION_1_0, v1_table) == ERUI_OK);
    ERUI_TEST_CHECK(v1_table->size == ERUI_API_V1_0_SIZE);
    ERUI_TEST_CHECK(v1_table->api_version == ERUI_API_VERSION_1_0);
    ERUI_TEST_CHECK((v1_table->capabilities & ERUI_CAP_TEXT_INPUT) == 0);
    for (std::size_t index = ERUI_API_V1_0_SIZE;
         index < v1_storage.size(); ++index) {
        ERUI_TEST_CHECK(v1_storage[index] == std::byte{0xA5});
    }

    // A provider is bound to the version-specific registration trampoline,
    // so even deliberately mixing tables cannot grant a 1.0 provider access
    // to 1.1-only operations or make the host inspect 1.1 pointers.
    erui::host::registry().open_registration();
    ERUI_ProviderDesc v1_provider{};
    v1_provider.size = sizeof(v1_provider);
    v1_provider.api_version = ERUI_API_VERSION_1_0;
    v1_provider.owner_module = GetModuleHandleW(nullptr);
    v1_provider.provider_id = ascii("com.example.v1-mixed-table");
    v1_provider.display_name = utf16(L"Frozen v1 Provider");
    ERUI_ProviderHandle v1_provider_handle{};
    ERUI_PageHandle v1_root{};
    ERUI_TEST_CHECK(v1_table->register_provider(
        &v1_provider, &v1_provider_handle, &v1_root) == ERUI_OK);
    ERUI_RowHandle untouched_row{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(negotiated.table.add_text_input(
        v1_provider_handle, v1_root, nullptr, &untouched_row) ==
        ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched_row == UINT64_C(0xA55AA55AA55AA55A));
    ERUI_TEST_CHECK(negotiated.table.set_text_input_value(
        v1_provider_handle, UINT64_C(123), nullptr) == ERUI_NOT_SUPPORTED);
    std::uint32_t untouched_length{0xA5A5A5A5u};
    ERUI_TEST_CHECK(negotiated.table.get_text_input_value(
        v1_provider_handle, UINT64_C(123), nullptr, 0, &untouched_length) ==
        ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched_length == 0xA5A5A5A5u);
    ERUI_TEST_CHECK(v1_table->abort_provider(v1_provider_handle) == ERUI_OK);

    erui::host::Registry registry{};
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_CURRENT;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("com.example.registry-test");
    provider.display_name = utf16(L"Registry Test");
    provider.root_priority = 10;

    ERUI_ProviderDesc wrong_provider_version = provider;
    wrong_provider_version.api_version = ERUI_VERSION_ENCODE(1u, 99u);
    ERUI_ProviderHandle wrong_version_handle{};
    ERUI_PageHandle wrong_version_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_CURRENT,
        &wrong_provider_version,
        &wrong_version_handle,
        &wrong_version_root) == ERUI_INVALID_ARGUMENT);

    ERUI_ProviderHandle handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_CURRENT, &provider, &handle, &root) == ERUI_OK);
    ERUI_TEST_CHECK(handle != root);

    ERUI_ProviderHandle duplicate{};
    ERUI_PageHandle duplicate_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_CURRENT,
        &provider, &duplicate, &duplicate_root) == ERUI_DUPLICATE_PROVIDER_ID);
    ERUI_TEST_CHECK(duplicate == ERUI_INVALID_PROVIDER);
    ERUI_TEST_CHECK(duplicate_root == ERUI_INVALID_PAGE);

    std::uint8_t observed = 1;
    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16(L"Enabled");
    toggle.help = utf16(L"Toggle help");
    toggle.initial_value = 1;
    toggle.enabled = 1;
    toggle.changed_callback = &changed;
    toggle.user_data = &observed;
    ERUI_RowHandle toggle_row{};
    ERUI_TEST_CHECK(registry.add_toggle(handle, root, &toggle, &toggle_row) == ERUI_OK);
    ERUI_TEST_CHECK(toggle_row != handle && toggle_row != root);

    const ERUI_Utf16View choice_values[] = {
        utf16(L"Off"), utf16(L"Balanced"), utf16(L"Strong")};
    ERUI_ChoiceDesc choice{};
    choice.size = sizeof(choice);
    choice.label = utf16(L"Mode");
    choice.help = utf16(L"Choice help");
    choice.changed_callback = &changed;
    choice.user_data = &observed;
    choice.options = choice_values;
    choice.option_count = 3;
    choice.initial_index = 1;
    ERUI_RowHandle inline_choice_row{};
    ERUI_TEST_CHECK(registry.add_inline_choice(
        handle, root, &choice, &inline_choice_row) == ERUI_OK);
    ERUI_ChoiceDesc popup_choice = choice;
    popup_choice.label = utf16(L"Popup Mode");
    popup_choice.initial_index = 2;
    ERUI_RowHandle popup_choice_row{};
    ERUI_TEST_CHECK(registry.add_popup_choice(
        handle, root, &popup_choice, &popup_choice_row) == ERUI_OK);
    ERUI_ChoiceDesc invalid_choice = choice;
    invalid_choice.initial_index = 3;
    ERUI_TEST_CHECK(registry.add_inline_choice(
        handle, root, &invalid_choice, nullptr) == ERUI_INVALID_ARGUMENT);
    std::array<ERUI_Utf16View, 33> too_many_choices{};
    too_many_choices.fill(utf16(L"Item"));
    invalid_choice = choice;
    invalid_choice.options = too_many_choices.data();
    invalid_choice.option_count =
        static_cast<std::uint32_t>(too_many_choices.size());
    ERUI_TEST_CHECK(registry.add_popup_choice(
        handle, root, &invalid_choice, nullptr) == ERUI_INVALID_ARGUMENT);

    TextObservation text_observation{};
    ERUI_TextInputDesc text_input{};
    text_input.size = sizeof(text_input);
    text_input.label = utf16(L"Player Alias");
    text_input.help = utf16(L"TextInput help");
    text_input.initial_value = utf16(L"Tarnished");
    text_input.placeholder = utf16(L"Enter an alias");
    text_input.changed_callback = &text_changed;
    text_input.user_data = &text_observation;
    // Zero deliberately exercises the stable default rather than relying on
    // the C++ wrapper's member initializer.
    text_input.maximum_length = 0;
    ERUI_RowHandle text_input_row{};
    ERUI_TEST_CHECK(registry.add_text_input(
        handle, root, &text_input, &text_input_row) == ERUI_OK);
    ERUI_TEST_CHECK(text_input_row != ERUI_INVALID_ROW);

    ERUI_TextInputDesc invalid_text_input = text_input;
    invalid_text_input.maximum_length = ERUI_TEXT_INPUT_MAX_LENGTH + 1u;
    ERUI_RowHandle rejected_text_row{UINT64_C(0x123456789ABCDEF0)};
    ERUI_TEST_CHECK(registry.add_text_input(
        handle, root, &invalid_text_input, &rejected_text_row) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(rejected_text_row == ERUI_INVALID_ROW);
    invalid_text_input = text_input;
    invalid_text_input.maximum_length = 3;
    invalid_text_input.initial_value = utf16(L"Four");
    ERUI_TEST_CHECK(registry.add_text_input(
        handle, root, &invalid_text_input, nullptr) == ERUI_INVALID_ARGUMENT);

    ERUI_RowHandle wrong_kind_output{99};
    ERUI_TEST_CHECK(registry.add_toggle(
        handle, toggle_row, &toggle, &wrong_kind_output) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(wrong_kind_output == ERUI_INVALID_ROW);

    const std::uint16_t embedded_nul[] = {'B', 0, 'a', 'd'};
    ERUI_ToggleDesc invalid_text = toggle;
    invalid_text.label = {embedded_nul, 4, 0};
    ERUI_TEST_CHECK(registry.add_toggle(
        handle, root, &invalid_text, nullptr) == ERUI_INVALID_ARGUMENT);
    const std::uint16_t malformed_utf16[] = {0xD800u, 'x'};
    invalid_text.label = {malformed_utf16, 2, 0};
    ERUI_TEST_CHECK(registry.add_toggle(
        handle, root, &invalid_text, nullptr) == ERUI_INVALID_ARGUMENT);

    unsigned presses = 0;
    ERUI_ButtonDesc button{};
    button.size = sizeof(button);
    button.label = utf16(L"Action");
    button.help = utf16(L"Action help");
    button.enabled = 1;
    button.callback = &pressed;
    button.user_data = &presses;
    ERUI_ButtonDesc short_button = button;
    short_button.size = static_cast<std::uint32_t>(
        offsetof(ERUI_ButtonDesc, reserved));
    ERUI_TEST_CHECK(registry.add_button(
        handle, root, &short_button, nullptr) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.add_button(handle, root, &button, nullptr) == ERUI_OK);

    ERUI_SubmenuDesc submenu{};
    submenu.size = sizeof(submenu);
    submenu.label = utf16(L"Advanced");
    submenu.help = utf16(L"Advanced help");
    submenu.enabled = 1;
    ERUI_SubmenuDesc short_submenu = submenu;
    short_submenu.size = static_cast<std::uint32_t>(
        offsetof(ERUI_SubmenuDesc, reserved));
    ERUI_PageHandle rejected_child{99};
    ERUI_TEST_CHECK(registry.add_submenu(
        handle, root, &short_submenu, &rejected_child, nullptr) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(rejected_child == ERUI_INVALID_PAGE);
    ERUI_PageHandle child{};
    ERUI_TEST_CHECK(registry.add_submenu(
        handle, root, &submenu, &child, nullptr) == ERUI_OK);
    ERUI_TEST_CHECK(registry.add_button(handle, child, &button, nullptr) == ERUI_OK);

    FormatterObservation formatter_observation{};
    ERUI_PagePresentationDesc presentation{};
    presentation.size = sizeof(presentation);
    presentation.menu_title = utf16(L"Provider Outer");
    presentation.page_title = utf16(L"Override Base");
    presentation.formatter = &format_page_title;
    presentation.user_data = &formatter_observation;
    ERUI_TEST_CHECK(registry.set_page_presentation(
        handle, root, &presentation) == ERUI_INVALID_ARGUMENT);
    ERUI_PagePresentationDesc invalid_presentation{};
    invalid_presentation.size = sizeof(invalid_presentation);
    invalid_presentation.user_data = &formatter_observation;
    ERUI_TEST_CHECK(registry.set_page_presentation(
        handle, child, &invalid_presentation) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.set_page_presentation(
        handle, child, &presentation) == ERUI_OK);
    for (unsigned index = 0; index < 15; ++index) {
        ERUI_TEST_CHECK(registry.add_button(handle, child, &button, nullptr) == ERUI_OK);
    }
    ERUI_SubmenuDesc default_outer_submenu = submenu;
    default_outer_submenu.label = utf16(L"Default Outer");
    default_outer_submenu.page_title = utf16(L"Default Child");
    ERUI_PageHandle default_outer_child{};
    ERUI_TEST_CHECK(registry.add_submenu(
        handle,
        root,
        &default_outer_submenu,
        &default_outer_child,
        nullptr) == ERUI_OK);

    ERUI_TEST_CHECK(registry.commit_provider(handle) == ERUI_OK);
    ERUI_TEST_CHECK(registry.committed_provider_count() == 1);
    ERUI_TEST_CHECK(registry.committed_root_row_count() == 7);

    std::uint32_t text_length{};
    ERUI_TEST_CHECK(registry.get_text_input_value(
        handle, text_input_row, nullptr, 0, &text_length) == ERUI_OK);
    ERUI_TEST_CHECK(text_length == 9);
    std::array<std::uint16_t, 16> text_buffer{};
    text_buffer.fill(0xA5A5u);
    std::uint32_t required{};
    ERUI_TEST_CHECK(registry.get_text_input_value(
        handle, text_input_row, text_buffer.data(), 4, &required) ==
        ERUI_BUFFER_TOO_SMALL);
    ERUI_TEST_CHECK(required == 9);
    ERUI_TEST_CHECK(text_buffer[0] == 0xA5A5u);
    ERUI_TEST_CHECK(registry.get_text_input_value(
        handle, text_input_row, text_buffer.data(),
        static_cast<std::uint32_t>(text_buffer.size()), &required) == ERUI_OK);
    ERUI_TEST_CHECK(required == 9);
    ERUI_TEST_CHECK(std::memcmp(
        text_buffer.data(), L"Tarnished", 9 * sizeof(std::uint16_t)) == 0);

    const ERUI_Utf16View programmed_text = utf16(L"Melina");
    ERUI_TEST_CHECK(registry.set_text_input_value(
        handle, text_input_row, &programmed_text) == ERUI_OK);
    ERUI_TEST_CHECK(text_observation.calls == 0);
    const ERUI_Utf16View too_long_text = utf16(L"12345678901234567");
    ERUI_TEST_CHECK(registry.set_text_input_value(
        handle, text_input_row, &too_long_text) == ERUI_INVALID_ARGUMENT);

    ERUI_AlertDesc alert{};
    alert.size = sizeof(alert);
    alert.message = utf16(L"Registry alert");
    // The registry validates provider/descriptor ownership before reaching
    // the unavailable native transport used by this isolated unit test.
    ERUI_TEST_CHECK(registry.enqueue_alert(ERUI_INVALID_PROVIDER, &alert) ==
        ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registry.enqueue_alert(handle, &alert) == ERUI_NOT_SUPPORTED);
    alert.buttons = static_cast<ERUI_AlertButtons>(99);
    ERUI_TEST_CHECK(registry.enqueue_alert(handle, &alert) ==
        ERUI_INVALID_ARGUMENT);
    alert.buttons = ERUI_ALERT_BUTTONS_OK;
    alert.placement = static_cast<ERUI_AlertPlacement>(99);
    ERUI_TEST_CHECK(registry.enqueue_alert(handle, &alert) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_TEST_CHECK(registry.set_row_value(handle, toggle_row, 0) == ERUI_OK);
    std::uint8_t value = 1;
    ERUI_TEST_CHECK(registry.get_row_value(handle, toggle_row, &value) == ERUI_OK);
    ERUI_TEST_CHECK(value == 0);
    ERUI_TEST_CHECK(registry.set_row_value(
        handle, inline_choice_row, 2) == ERUI_OK);
    ERUI_TEST_CHECK(registry.get_row_value(
        handle, inline_choice_row, &value) == ERUI_OK);
    ERUI_TEST_CHECK(value == 2);
    ERUI_TEST_CHECK(registry.set_row_value(handle, inline_choice_row, 3) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.set_row_value(
        handle, popup_choice_row, 0) == ERUI_OK);
    ERUI_TEST_CHECK(registry.get_row_value(
        handle, popup_choice_row, &value) == ERUI_OK);
    ERUI_TEST_CHECK(value == 0);
    registry.apply_pending_values();

    // Providers are merged globally by priority and their rows remain in
    // provider-local insertion order. Together they deliberately overflow the
    // Controller root so the ordinary pagination compiler is exercised.
    const auto add_provider = [&](const char* id, const wchar_t* name,
                                  std::int32_t priority, unsigned rows) {
        ERUI_ProviderDesc desc{};
        desc.size = sizeof(desc);
        desc.api_version = ERUI_API_VERSION_CURRENT;
        desc.owner_module = GetModuleHandleW(nullptr);
        desc.provider_id = ascii(id);
        desc.display_name = utf16(name);
        desc.root_priority = priority;
        ERUI_ProviderHandle provider_handle{};
        ERUI_PageHandle provider_root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_CURRENT,
            &desc, &provider_handle, &provider_root) == ERUI_OK);
        for (unsigned index = 0; index < rows; ++index) {
            ERUI_ButtonDesc row = button;
            row.label = utf16(name);
            ERUI_TEST_CHECK(registry.add_button(
                provider_handle, provider_root, &row, nullptr) == ERUI_OK);
        }
        ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);
    };
    add_provider("com.example.late-priority", L"Last Provider", 20, 5);
    add_provider("com.example.first-priority", L"First Provider", 5, 1);

    ERUI_ProviderDesc active_draft = provider;
    active_draft.provider_id = ascii("com.example.active-draft");
    active_draft.display_name = utf16(L"Active Draft");
    ERUI_ProviderHandle active_handle{};
    ERUI_PageHandle active_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_CURRENT,
        &active_draft, &active_handle, &active_root) == ERUI_OK);
    ERUI_TEST_CHECK(!registry.try_freeze_and_build(
        GetTickCount64(), 0, (std::numeric_limits<std::uint64_t>::max)()));

    auto menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu);
    ERUI_TEST_CHECK(menu->root().row_count() == 13);
    auto compiled = erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == 13);
    ERUI_TEST_CHECK(compiled->modeled_popup_choice_count == 1);
    ERUI_TEST_CHECK(compiled->modeled_text_input_count == 1);
    ERUI_TEST_CHECK(compiled->pagination_required);
    const auto& first_row = compiled->root_page().rows.front();
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(first_row.label_id)) ==
        L"First Provider");
    const auto compiled_text_input = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::text_input;
        });
    ERUI_TEST_CHECK(compiled_text_input != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(compiled_text_input->text_input_state != nullptr);
    ERUI_TEST_CHECK(compiled_text_input->text_input_state->maximum_length() ==
        ERUI_TEXT_INPUT_DEFAULT_MAX_LENGTH);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_text_input->label_id)) == L"Player Alias");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_text_input->help_id)) == L"TextInput help");
    ERUI_TEST_CHECK(compiled_text_input->text_input_state->placeholder() ==
        L"Enter an alias");
    ERUI_TEST_CHECK(compiled_text_input->text_input_state->snapshot().value ==
        L"Melina");
    ERUI_TEST_CHECK(compiled_text_input->text_input_state->commit_native(
        L"Ranni"));
    std::wstring committed_text{};
    ERUI_TEST_CHECK(compiled_text_input->text_input_state->pop_committed_change(
        committed_text));
    compiled_text_input->text_action.invoke(committed_text);
    ERUI_TEST_CHECK(text_observation.calls == 1);
    ERUI_TEST_CHECK(text_observation.provider == handle);
    ERUI_TEST_CHECK(text_observation.row == text_input_row);
    ERUI_TEST_CHECK(text_observation.value == L"Ranni");
    ERUI_TEST_CHECK(compiled_text_input->text_input_state->commit_native(
        L"Ranni"));
    ERUI_TEST_CHECK(!compiled_text_input->text_input_state->pop_committed_change(
        committed_text));
    const auto submenu_row = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::submenu;
        });
    ERUI_TEST_CHECK(submenu_row != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(submenu_row->target_page_index < compiled->pages.size());
    const std::size_t child_page_index = submenu_row->target_page_index;
    ERUI_TEST_CHECK(compiled->page_plans[child_page_index].slices.size() == 2);
    ERUI_TEST_CHECK(formatter_observation.provider == handle);
    ERUI_TEST_CHECK(formatter_observation.page == child);
    ERUI_TEST_CHECK(formatter_observation.base_title == L"Override Base");
    ERUI_TEST_CHECK(formatter_observation.calls == 2);
    const auto first_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::submenu(child_page_index, 0));
    const auto second_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::submenu(child_page_index, 1));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        first_presentation.outer_title_id)) == L"Provider Outer");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        first_presentation.page_title_id)) == L"Formatted 1/2");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        second_presentation.page_title_id)) ==
        L"Override Base (2/2)");
    const auto default_outer_row = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [&](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::submenu &&
                row.target_page_index < compiled->pages.size() &&
                std::wstring(compiled->texts.lookup(
                    compiled->pages[row.target_page_index].title_id)) ==
                    L"Default Child";
        });
    ERUI_TEST_CHECK(default_outer_row != compiled->root_page().rows.end());
    const auto default_outer_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::submenu(
            default_outer_row->target_page_index));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        default_outer_presentation.outer_title_id)) == L"Registry Test");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        default_outer_presentation.page_title_id)) == L"Default Child");
    const auto root_main_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::root_main(
            compiled->root_page_index,
            erui::detail::controller_vanilla_capacity));
    ERUI_TEST_CHECK(root_main_presentation.outer_title_id == 0);
    ERUI_TEST_CHECK(root_main_presentation.page_title_id == 0);
    const auto root_continuation_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::root_continuation(
            compiled->root_page_index,
            erui::detail::controller_vanilla_capacity,
            1));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        root_continuation_presentation.outer_title_id)) == L"ERNativeUI");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        root_continuation_presentation.page_title_id)) ==
        L"ERNativeUI (2/2)");
    ERUI_TEST_CHECK(!registry.registration_open());
    ERUI_TEST_CHECK(registry.abort_provider(active_handle) == ERUI_OK);
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_CURRENT,
        &provider, &duplicate, &duplicate_root) == ERUI_REGISTRATION_CLOSED);
    ERUI_TEST_CHECK(duplicate == ERUI_INVALID_PROVIDER);
    ERUI_TEST_CHECK(duplicate_root == ERUI_INVALID_PAGE);

    erui::host::set_api_state(erui::host::ApiState::failed);
    std::uint8_t ignored_value{};
    ERUI_TEST_CHECK(negotiated.table.set_row_value(handle, toggle_row, 1) == ERUI_HOST_FAILED);
    ERUI_TEST_CHECK(negotiated.table.get_row_value(
        handle, toggle_row, &ignored_value) == ERUI_HOST_FAILED);
    return 0;
}
