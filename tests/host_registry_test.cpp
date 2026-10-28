#include "host_registry.hpp"
#include "menu_compiler.hpp"
#include "native_tab_catalogue.hpp"
#include "row_presentation_state.hpp"
#include "row_update_state.hpp"
#include "row_visibility_state.hpp"
#include "steam_language.hpp"
#include "submenu_update_state.hpp"
#include "tab_update_state.hpp"

#include <Windows.h>

#include "test_assertions.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ERUI_StringView ascii(const char* value) {
    ERUI_StringView result{};
    result.data = value;
    result.length = static_cast<std::uint32_t>(std::char_traits<char>::length(value));
    return result;
}

ERUI_StringView ascii(std::string_view value) {
    ERUI_StringView result{};
    result.data = value.data();
    result.length = static_cast<std::uint32_t>(value.size());
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
    result.data = value.empty()
        ? nullptr
        : reinterpret_cast<const std::uint16_t*>(value.data());
    result.length = static_cast<std::uint32_t>(value.size());
    return result;
}

ERUI_TextStyleSpan text_style_span(
    std::uint32_t offset,
    std::uint32_t length,
    ERUI_TextStyleFlags flags,
    ERUI_Color color = {}) {
    ERUI_TextStyleSpan result{};
    result.size = sizeof(result);
    result.flags = flags;
    result.offset = offset;
    result.length = length;
    result.color = color;
    return result;
}

ERUI_StyledTextView styled_text(
    std::wstring_view text,
    const ERUI_TextStyleSpan* spans = nullptr,
    std::uint32_t span_count = 0) {
    ERUI_StyledTextView result{};
    result.size = sizeof(result);
    result.text = utf16(text);
    result.spans = spans;
    result.span_count = span_count;
    result.span_stride = sizeof(ERUI_TextStyleSpan);
    return result;
}

void ERUI_CALL changed(void* user_data, std::uint8_t value) {
    *static_cast<std::uint8_t*>(user_data) = value;
}

struct ValueObservation {
    std::uint8_t value{};
    unsigned calls{};
};

void ERUI_CALL observe_value(void* user_data, std::uint8_t value) {
    auto& observation = *static_cast<ValueObservation*>(user_data);
    observation.value = value;
    ++observation.calls;
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

struct ColorObservation {
    ERUI_ProviderHandle provider{};
    ERUI_RowHandle row{};
    ERUI_Color value{};
    unsigned calls{};
};

struct BindingObservation {
    ERUI_ProviderHandle provider{};
    ERUI_InputActionHandle action{};
    ERUI_InputDevices devices{};
    unsigned calls{};
};

void ERUI_CALL binding_activated(
    void* user_data,
    const ERUI_InputActionActivatedContext* context) {
    auto& observation = *static_cast<BindingObservation*>(user_data);
    ERUI_TEST_CHECK(context != nullptr);
    ERUI_TEST_CHECK(context->size == sizeof(*context));
    ERUI_TEST_CHECK(context->flags == 0);
    ERUI_TEST_CHECK(context->reserved == 0);
    observation.provider = context->provider;
    observation.action = context->action;
    observation.devices = context->devices;
    ++observation.calls;
}

ERUI_ActionInputs default_action_inputs() {
    ERUI_ActionInputs result{};
    result.size = sizeof(result);
    result.controller = {
        ERUI_INPUT_SLOT_BOUND,
        ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER,
    };
    result.keyboard = {
        ERUI_INPUT_SLOT_BOUND,
        ERUI_KEYBOARD_KEY_F,
    };
    result.mouse = {
        ERUI_INPUT_SLOT_UNBOUND,
        ERUI_MOUSE_BUTTON_INVALID,
    };
    return result;
}

void ERUI_CALL color_changed(
    void* user_data,
    const ERUI_ColorPickerChangeContext* context) {
    auto& observation = *static_cast<ColorObservation*>(user_data);
    ERUI_TEST_CHECK(context != nullptr);
    ERUI_TEST_CHECK(context->size == sizeof(*context));
    ERUI_TEST_CHECK(context->flags == 0);
    ERUI_TEST_CHECK(context->reserved == 0);
    ERUI_TEST_CHECK(context->value.reserved == 0);
    observation.provider = context->provider;
    observation.row = context->row;
    observation.value = context->value;
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
    provider.provider_id = ascii("text-input-lengths");
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

void verify_programmatic_value_writes_are_silent() {
    erui::host::Registry registry{};
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_1;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("silent-value-writes");
    provider.display_name = utf16(L"Silent value writes");

    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    std::array<ValueObservation, 4> observations{};
    std::array<ERUI_RowHandle, 4> handles{};

    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16(L"Toggle");
    toggle.changed_callback = &observe_value;
    toggle.user_data = &observations[0];
    toggle.enabled = 1;
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, root, &toggle, &handles[0]) == ERUI_OK);

    ERUI_SliderDesc slider{};
    slider.size = sizeof(slider);
    slider.label = utf16(L"Slider");
    slider.changed_callback = &observe_value;
    slider.user_data = &observations[1];
    slider.minimum = 0;
    slider.maximum = 10;
    slider.step = 2;
    slider.initial_value = 3;
    slider.enabled = 1;
    ERUI_TEST_CHECK(registry.add_slider(
        provider_handle, root, &slider, &handles[1]) == ERUI_OK);

    const ERUI_Utf16View options[] = {
        utf16(L"First"), utf16(L"Second"), utf16(L"Third")};
    ERUI_ChoiceDesc choice{};
    choice.size = sizeof(choice);
    choice.label = utf16(L"Inline choice");
    choice.changed_callback = &observe_value;
    choice.user_data = &observations[2];
    choice.options = options;
    choice.option_count = 3;
    choice.initial_index = 0;
    ERUI_TEST_CHECK(registry.add_inline_choice(
        provider_handle, root, &choice, &handles[2]) == ERUI_OK);

    choice.label = utf16(L"Popup choice");
    choice.user_data = &observations[3];
    choice.initial_index = 1;
    ERUI_TEST_CHECK(registry.add_popup_choice(
        provider_handle, root, &choice, &handles[3]) == ERUI_OK);

    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);
    std::uint8_t initial_slider_value{};
    ERUI_TEST_CHECK(registry.get_row_value(
        provider_handle, handles[1], &initial_slider_value) == ERUI_OK);
    ERUI_TEST_CHECK(initial_slider_value == 2);
    ERUI_TEST_CHECK(observations[1].calls == 0);
    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == handles.size());

    // Mirrors the value-specific part of runtime::poll_changes. Keeping the
    // callback bridge under test avoids installing native game hooks here.
    const auto poll_row = [](erui::detail::CompiledRow& row) {
        std::uint8_t current = *row.byte_value;
        if (row.kind == erui::RowKind::toggle) {
            current = current == 0 ? 0 : 1;
        } else if (row.kind == erui::RowKind::slider) {
            const int clamped = std::clamp<int>(
                current, row.slider.minimum, row.slider.maximum);
            const int offset = clamped - row.slider.minimum;
            current = static_cast<std::uint8_t>(row.slider.minimum +
                (offset / row.slider.step) * row.slider.step);
        } else if (row.kind == erui::RowKind::inline_choice) {
            if (current >= row.choice_ids.size()) current = 0;
        } else if (row.kind == erui::RowKind::popup_choice) {
            current = erui::detail::synchronize_popup_choice_state(
                *row.byte_value,
                row.popup_choice_state,
                row.last_observed_value,
                row.choice_ids.size());
        }
        *row.byte_value = current;
        if (current == row.last_observed_value) return false;
        row.last_observed_value = current;
        row.value_action.invoke(current);
        return true;
    };

    constexpr std::array<std::uint8_t, 4> programmed{1, 6, 2, 0};
    for (std::size_t index = 0; index < handles.size(); ++index) {
        ERUI_TEST_CHECK(registry.set_row_value(
            provider_handle, handles[index], programmed[index]) == ERUI_OK);
    }
    registry.apply_pending_values();

    for (std::size_t index = 0; index < observations.size(); ++index) {
        erui::detail::CompiledRow& row = compiled->root_page().rows[index];
        ERUI_TEST_CHECK(*row.byte_value == programmed[index]);
        ERUI_TEST_CHECK(poll_row(row));
        ERUI_TEST_CHECK(observations[index].calls == 0);
    }
    ERUI_TEST_CHECK(compiled->root_page().rows[3]
        .popup_choice_state.native_selection() == 1);

    // A second client write may arrive after one value was applied but before
    // the worker polls it. Both native transitions remain callback-silent,
    // and get_row_value keeps reporting the newer client value throughout.
    erui::detail::CompiledRow& racing_slider =
        compiled->root_page().rows[1];
    ERUI_TEST_CHECK(registry.set_row_value(
        provider_handle, handles[1], 4) == ERUI_OK);
    registry.apply_pending_values();
    ERUI_TEST_CHECK(registry.set_row_value(
        provider_handle, handles[1], 6) == ERUI_OK);
    ERUI_TEST_CHECK(poll_row(racing_slider));
    ERUI_TEST_CHECK(observations[1].calls == 0);
    std::uint8_t racing_public_value{};
    ERUI_TEST_CHECK(registry.get_row_value(
        provider_handle, handles[1], &racing_public_value) == ERUI_OK);
    ERUI_TEST_CHECK(racing_public_value == 6);
    registry.apply_pending_values();
    ERUI_TEST_CHECK(poll_row(racing_slider));
    ERUI_TEST_CHECK(observations[1].calls == 0);

    constexpr std::array<std::uint8_t, 4> player_values{0, 8, 1, 2};
    for (std::size_t index = 0; index < observations.size(); ++index) {
        erui::detail::CompiledRow& row = compiled->root_page().rows[index];
        if (row.kind == erui::RowKind::popup_choice) {
            row.popup_choice_state.set_public_selection(player_values[index]);
        } else {
            *row.byte_value = player_values[index];
        }
        ERUI_TEST_CHECK(poll_row(row));
        ERUI_TEST_CHECK(observations[index].calls == 1);
        ERUI_TEST_CHECK(observations[index].value == player_values[index]);
    }
}

void verify_provider_identifier_contract() {
    erui::host::Registry registry{};
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_1;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.display_name = utf16(L"Identifier Test");

    constexpr std::array<const char*, 9> valid_ids{
        "my-mod",
        "My_Mod",
        "mod.example-2",
        "Az09_.-",
        ".",
        "..",
        ".leading",
        "trailing.",
        "two..dots",
    };
    for (const char* id : valid_ids) {
        provider.provider_id = ascii(id);
        ERUI_ProviderHandle handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_1, &provider, &handle, &root) == ERUI_OK);
        ERUI_TEST_CHECK(handle != ERUI_INVALID_PROVIDER);
        ERUI_TEST_CHECK(root != ERUI_INVALID_PAGE);
        ERUI_TEST_CHECK(registry.abort_provider(handle) == ERUI_OK);
    }

    constexpr std::array<const char*, 7> invalid_ids{
        "",
        "my mod",
        "my/mod",
        "my\\mod",
        "my:mod",
        "my@mod",
        "m\xC3\xB6" "d",
    };
    for (const char* id : invalid_ids) {
        provider.provider_id = ascii(id);
        ERUI_ProviderHandle handle{UINT64_C(0xA55AA55AA55AA55A)};
        ERUI_PageHandle root{UINT64_C(0x5AA55AA55AA55AA5)};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_1, &provider, &handle, &root) ==
            ERUI_INVALID_ARGUMENT);
        ERUI_TEST_CHECK(handle == ERUI_INVALID_PROVIDER);
        ERUI_TEST_CHECK(root == ERUI_INVALID_PAGE);
    }

    constexpr char embedded_nul_id[]{'m', 'y', '\0', 'm', 'o', 'd'};
    provider.provider_id.data = embedded_nul_id;
    provider.provider_id.length = static_cast<std::uint32_t>(
        sizeof embedded_nul_id);
    provider.provider_id.reserved = 0;
    ERUI_ProviderHandle embedded_nul_handle{};
    ERUI_PageHandle embedded_nul_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &embedded_nul_handle,
        &embedded_nul_root) == ERUI_INVALID_ARGUMENT);

    const std::string maximum_id(255, 'a');
    provider.provider_id.data = maximum_id.data();
    provider.provider_id.length = static_cast<std::uint32_t>(maximum_id.size());
    provider.provider_id.reserved = 0;
    ERUI_ProviderHandle maximum_handle{};
    ERUI_PageHandle maximum_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &maximum_handle,
        &maximum_root) == ERUI_OK);
    ERUI_TEST_CHECK(registry.abort_provider(maximum_handle) == ERUI_OK);

    provider.provider_id = ascii("mod");
    ERUI_ProviderHandle lowercase_handle{};
    ERUI_PageHandle lowercase_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &lowercase_handle,
        &lowercase_root) == ERUI_OK);
    provider.provider_id = ascii("Mod");
    ERUI_ProviderHandle uppercase_handle{};
    ERUI_PageHandle uppercase_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &uppercase_handle,
        &uppercase_root) == ERUI_OK);
    ERUI_TEST_CHECK(registry.abort_provider(uppercase_handle) == ERUI_OK);
    ERUI_TEST_CHECK(registry.abort_provider(lowercase_handle) == ERUI_OK);

    const std::string oversized_id(256, 'a');
    provider.provider_id.data = oversized_id.data();
    provider.provider_id.length = static_cast<std::uint32_t>(
        oversized_id.size());
    provider.provider_id.reserved = 0;
    ERUI_ProviderHandle oversized_handle{};
    ERUI_PageHandle oversized_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_1,
        &provider,
        &oversized_handle,
        &oversized_root) == ERUI_INVALID_ARGUMENT);

    provider.api_version = ERUI_API_VERSION_1_0;
    provider.provider_id = ascii("legacy provider/id");
    ERUI_ProviderHandle legacy_handle{};
    ERUI_PageHandle legacy_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_0,
        &provider,
        &legacy_handle,
        &legacy_root) == ERUI_OK);
    ERUI_TEST_CHECK(registry.abort_provider(legacy_handle) == ERUI_OK);
}

void verify_title_screen_text_registration_contract() {
    // A copied 1.3 entry point fails closed without touching its descriptor
    // until an authoritative title-GFX backend has been published.
    {
        erui::host::Registry unavailable{};
        unavailable.open_registration();
        ERUI_TEST_CHECK(unavailable.add_title_screen_text(
            ERUI_INVALID_PROVIDER, nullptr) == ERUI_NOT_SUPPORTED);
    }

    // Even with the backend available, legacy providers cannot use a 1.3-only
    // descriptor or entry point.
    {
        erui::host::Registry legacy{};
        ERUI_TEST_CHECK(legacy.enable_title_screen_text_backend());
        legacy.open_registration();
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_2;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii("title-text.legacy");
        provider.display_name = utf16(L"Legacy title text");
        ERUI_ProviderHandle handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(legacy.register_provider(
            ERUI_API_VERSION_1_2, &provider, &handle, &root) == ERUI_OK);
        ERUI_TEST_CHECK(legacy.add_title_screen_text(handle, nullptr) ==
            ERUI_NOT_SUPPORTED);
    }

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_title_screen_text_backend());
    registry.open_registration();

    const auto register_provider = [&](const char* id,
                                       std::int32_t priority,
                                       ERUI_ProviderHandle& handle) {
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_3;
        provider.root_priority = priority;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii(id);
        provider.display_name = utf16(L"Title-screen text");
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_3, &provider, &handle, &root) == ERUI_OK);
    };

    ERUI_ProviderHandle provider_z{};
    ERUI_ProviderHandle provider_a{};
    register_provider("title-text.z", 100, provider_z);
    register_provider("title-text.a", 50, provider_a);

    std::wstring copied = L"Z first";
    ERUI_TitleScreenTextDesc line{};
    line.size = sizeof(line);
    line.text = utf16(copied);
    line.corner = ERUI_TITLE_SCREEN_CORNER_TOP_LEFT;
    ERUI_TEST_CHECK(registry.add_title_screen_text(provider_z, &line) ==
        ERUI_OK);
    copied.assign(L"mutated");

    ERUI_TitleScreenTextDesc colored{};
    colored.size = sizeof(colored);
    colored.flags = ERUI_TITLE_SCREEN_TEXT_USE_CUSTOM_COLOR;
    colored.text = utf16(L"Z second");
    colored.color = {216u, 167u, 103u, 0u};
    colored.corner = ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT;
    ERUI_TEST_CHECK(registry.add_title_screen_text(provider_z, &colored) ==
        ERUI_OK);

    ERUI_TitleScreenTextDesc first_provider{};
    first_provider.size = sizeof(first_provider);
    first_provider.text = utf16(L"A first");
    first_provider.color = {1u, 2u, 3u, 0u};
    first_provider.corner = ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT;
    ERUI_TEST_CHECK(registry.add_title_screen_text(
        provider_a, &first_provider) == ERUI_OK);

    const auto reject = [&](ERUI_TitleScreenTextDesc invalid) {
        ERUI_TEST_CHECK(registry.add_title_screen_text(
            provider_z, &invalid) == ERUI_INVALID_ARGUMENT);
    };
    ERUI_TitleScreenTextDesc invalid = line;
    invalid.size = sizeof(invalid) - 1u;
    reject(invalid);
    invalid = line;
    invalid.text = {};
    reject(invalid);
    invalid = line;
    invalid.text = utf16(L"two\nlines");
    reject(invalid);
    invalid.text = utf16(L"two\rlines");
    reject(invalid);
    for (const wchar_t separator : {
             static_cast<wchar_t>(0x0085u),
             static_cast<wchar_t>(0x2028u),
             static_cast<wchar_t>(0x2029u)}) {
        const std::array<wchar_t, 3> separated{L'A', separator, L'B'};
        invalid.text = utf16(std::wstring_view(
            separated.data(), separated.size()));
        reject(invalid);
    }
    const std::array<wchar_t, 3> embedded_nul{L'A', L'\0', L'B'};
    invalid.text = utf16(std::wstring_view(
        embedded_nul.data(), embedded_nul.size()));
    reject(invalid);
    const std::array<wchar_t, 1> lone_high{
        static_cast<wchar_t>(0xD800u)};
    invalid.text = utf16(std::wstring_view(
        lone_high.data(), lone_high.size()));
    reject(invalid);
    const std::array<wchar_t, 1> lone_low{
        static_cast<wchar_t>(0xDC00u)};
    invalid.text = utf16(std::wstring_view(
        lone_low.data(), lone_low.size()));
    reject(invalid);
    const std::wstring oversized(
        ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS + 1u, L'X');
    invalid.text = utf16(oversized);
    reject(invalid);
    invalid = line;
    invalid.flags = UINT32_C(0x80000000);
    reject(invalid);
    invalid = line;
    invalid.color.reserved = 1;
    reject(invalid);
    invalid = line;
    invalid.corner = 0;
    reject(invalid);
    invalid = line;
    invalid.corner = 5;
    reject(invalid);
    invalid = line;
    invalid.corner = UINT32_MAX;
    reject(invalid);
    invalid = line;
    invalid.reserved[1] = 1;
    reject(invalid);

    const std::wstring maximum_length(
        ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS, L'M');
    ERUI_TitleScreenTextDesc maximum{};
    maximum.size = sizeof(maximum);
    maximum.text = utf16(maximum_length);
    maximum.corner = ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT;
    ERUI_TEST_CHECK(registry.add_title_screen_text(
        provider_a, &maximum) == ERUI_OK);

    ERUI_TEST_CHECK(registry.commit_provider(provider_z) == ERUI_OK);
    ERUI_TEST_CHECK(registry.add_title_screen_text(provider_z, &line) ==
        ERUI_ALREADY_COMMITTED);
    ERUI_TEST_CHECK(registry.commit_provider(provider_a) == ERUI_OK);
    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(registry.add_title_screen_text(provider_a, &line) ==
        ERUI_REGISTRATION_CLOSED);
    ERUI_TEST_CHECK(menu->title_screen_text_count() == 4);
    const auto title_lines = menu->title_screen_texts();
    ERUI_TEST_CHECK(title_lines.size() == 4);
    ERUI_TEST_CHECK(title_lines[0].provider_id ==
        "title-text.a");
    ERUI_TEST_CHECK(title_lines[0].text == L"A first");
    ERUI_TEST_CHECK(!title_lines[0].color.has_value());
    ERUI_TEST_CHECK(title_lines[0].corner ==
        ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT);
    ERUI_TEST_CHECK(title_lines[1].provider_id == "title-text.a");
    ERUI_TEST_CHECK(title_lines[1].text.size() ==
        ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS);
    ERUI_TEST_CHECK(title_lines[1].text == maximum_length);
    ERUI_TEST_CHECK(!title_lines[1].color.has_value());
    ERUI_TEST_CHECK(title_lines[1].corner ==
        ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT);
    ERUI_TEST_CHECK(title_lines[2].provider_id ==
        "title-text.z");
    ERUI_TEST_CHECK(title_lines[2].text == L"Z first");
    ERUI_TEST_CHECK(!title_lines[2].color.has_value());
    ERUI_TEST_CHECK(title_lines[2].corner ==
        ERUI_TITLE_SCREEN_CORNER_TOP_LEFT);
    ERUI_TEST_CHECK(title_lines[3].text == L"Z second");
    ERUI_TEST_CHECK(title_lines[3].color.has_value());
    ERUI_TEST_CHECK(*title_lines[3].color ==
        (erui::detail::RgbColor{216u, 167u, 103u}));
    ERUI_TEST_CHECK(title_lines[3].corner ==
        ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT);
}

void verify_title_screen_text_abort_reclaims_draft() {
    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_title_screen_text_backend());
    registry.open_registration();

    const auto register_provider = [&](const char* id) {
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_3;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii(id);
        provider.display_name = utf16(L"Title-line abort test");
        ERUI_ProviderHandle handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_3, &provider, &handle, &root) == ERUI_OK);
        return handle;
    };
    const auto add_lines = [&](ERUI_ProviderHandle provider,
                               const wchar_t* value) {
        ERUI_TitleScreenTextDesc line{};
        line.size = sizeof(line);
        line.text = utf16(value);
        line.corner = ERUI_TITLE_SCREEN_CORNER_TOP_LEFT;
        for (std::size_t index = 0;
             index < ERUI_TITLE_SCREEN_TEXT_MAX_LINES;
             ++index) {
            ERUI_TEST_CHECK(registry.add_title_screen_text(
                provider, &line) == ERUI_OK);
        }
    };

    const ERUI_ProviderHandle abandoned =
        register_provider("title-abort.abandoned");
    add_lines(abandoned, L"Abandoned line");
    ERUI_TEST_CHECK(registry.abort_provider(abandoned) == ERUI_OK);

    const ERUI_ProviderHandle replacement =
        register_provider("title-abort.replacement");
    add_lines(replacement, L"Replacement line");
    ERUI_TEST_CHECK(registry.commit_provider(replacement) == ERUI_OK);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(menu->title_screen_text_count() ==
        ERUI_TITLE_SCREEN_TEXT_MAX_LINES);
    for (const erui::TitleScreenTextSpec& line :
         menu->title_screen_texts()) {
        ERUI_TEST_CHECK(line.provider_id == "title-abort.replacement");
        ERUI_TEST_CHECK(line.text == L"Replacement line");
    }
}

void verify_title_screen_text_committed_limit() {
    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_title_screen_text_backend());
    registry.open_registration();

    const auto register_provider = [&](const char* id,
                                       ERUI_ProviderHandle& handle) {
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_3;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii(id);
        provider.display_name = utf16(L"Title line limit");
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_3, &provider, &handle, &root) == ERUI_OK);
    };
    const auto add_line = [&](ERUI_ProviderHandle provider) {
        ERUI_TitleScreenTextDesc line{};
        line.size = sizeof(line);
        line.text = utf16(L"Bounded line");
        line.corner = ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT;
        return registry.add_title_screen_text(provider, &line);
    };

    ERUI_ProviderHandle first{};
    ERUI_ProviderHandle second{};
    register_provider("title-limit.first", first);
    register_provider("title-limit.second", second);
    for (std::size_t index = 0;
         index < ERUI_TITLE_SCREEN_TEXT_MAX_LINES;
         ++index) {
        ERUI_TEST_CHECK(add_line(first) == ERUI_OK);
    }
    ERUI_TEST_CHECK(add_line(first) ==
        ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED);

    // Draft providers do not reserve the committed budget; commit performs
    // the same limit check atomically.
    ERUI_TEST_CHECK(add_line(second) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(first) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(second) ==
        ERUI_TITLE_SCREEN_TEXT_LIMIT_REACHED);
    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(menu->title_screen_text_count() ==
        ERUI_TITLE_SCREEN_TEXT_MAX_LINES);
    for (const erui::TitleScreenTextSpec& line :
         menu->title_screen_texts()) {
        ERUI_TEST_CHECK(line.provider_id == "title-limit.first");
    }
}

void verify_custom_tab_registration_contract() {
    // A copied 1.3 entry point must reject an older provider before touching
    // either 1.3-only pointer.
    {
        erui::host::Registry legacy{};
        ERUI_TEST_CHECK(legacy.enable_custom_tabs_backend());
        legacy.open_registration();
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_2;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii("tabs.legacy");
        provider.display_name = utf16(L"Legacy tabs");
        ERUI_ProviderHandle handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(legacy.register_provider(
            ERUI_API_VERSION_1_2, &provider, &handle, &root) == ERUI_OK);
        ERUI_TEST_CHECK(legacy.add_tab(handle, nullptr, nullptr) ==
            ERUI_NOT_SUPPORTED);
    }

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_custom_tabs_backend());
    erui::detail::TabConfiguration configuration{};
    erui::detail::TabOrderOverride override{};
    override.provider_id = "tabs.z";
    override.tab_id = "main";
    override.stable_key = "tabs.z/main";
    override.order = -10;
    configuration.order_overrides.emplace(override.stable_key, override);
    registry.set_tab_configuration(std::move(configuration));
    registry.open_registration();

    const auto register_provider = [&](const char* id,
                                       ERUI_ProviderHandle& handle,
                                       ERUI_PageHandle& root) {
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_3;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii(id);
        provider.display_name = utf16(L"Custom tabs");
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_3, &provider, &handle, &root) == ERUI_OK);
    };

    ERUI_ProviderHandle provider_z{};
    ERUI_PageHandle root_z{};
    register_provider("tabs.z", provider_z, root_z);
    ERUI_ProviderHandle provider_a{};
    ERUI_PageHandle root_a{};
    register_provider("tabs.a", provider_a, root_a);

    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = 12;
        pixels[offset + 1] = 34;
        pixels[offset + 2] = 56;
        pixels[offset + 3] = 255;
    }
    pixels[3] = 0;
    pixels[7] = 127;
    pixels[11] = 255;
    ERUI_TabIconDesc icon{};
    icon.size = sizeof(icon);
    icon.format = ERUI_TAB_ICON_FORMAT_RGBA8;
    icon.byte_count = static_cast<std::uint32_t>(pixels.size());
    icon.pixels = pixels.data();

    std::string copied_id = "main";
    std::wstring copied_title = L"Copied title";
    ERUI_TabDesc tab{};
    tab.size = sizeof(tab);
    tab.tab_id = ascii(copied_id);
    tab.title = utf16(copied_title);
    tab.preferred_order = 50;
    tab.icon = &icon;

    ERUI_PageHandle copied_page{};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &tab, &copied_page) == ERUI_OK);
    ERUI_TEST_CHECK(copied_page != ERUI_INVALID_PAGE &&
        copied_page != root_z);

    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16(L"Copied row");
    toggle.help = utf16(L"Copied row help");
    toggle.enabled = 1;
    ERUI_RowHandle row{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_z, copied_page, &toggle, &row) == ERUI_OK);

    // All client memory is synchronous: overwriting it cannot alter the
    // catalogue, icon, title, or row ownership retained by the host.
    copied_id.assign("xxxx");
    copied_title.assign(L"Changed title");
    pixels.fill(0);

    ERUI_TabDesc duplicate = tab;
    duplicate.tab_id = ascii("main");
    duplicate.title = utf16(L"Duplicate");
    duplicate.icon = nullptr;
    ERUI_PageHandle duplicate_page{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &duplicate, &duplicate_page) == ERUI_DUPLICATE_TAB_ID);
    ERUI_TEST_CHECK(duplicate_page == ERUI_INVALID_PAGE);

    const auto add_fallback = [&](ERUI_ProviderHandle provider,
                                  const char* id,
                                  std::int32_t order,
                                  ERUI_PageHandle& output) {
        ERUI_TabDesc fallback{};
        fallback.size = sizeof(fallback);
        fallback.tab_id = ascii(id);
        fallback.title = utf16(L"Fallback tab");
        fallback.preferred_order = order;
        ERUI_TEST_CHECK(registry.add_tab(
            provider, &fallback, &output) == ERUI_OK);
    };
    ERUI_PageHandle provider_a_main{};
    ERUI_PageHandle provider_a_settings{};
    ERUI_PageHandle provider_z_fallback{};
    add_fallback(provider_a, "main", 50, provider_a_main);
    add_fallback(provider_a, "settings", 50, provider_a_settings);
    add_fallback(provider_z, "fallback", 100, provider_z_fallback);

    // Reject malformed descriptors without publishing a page handle.
    ERUI_TabDesc invalid = duplicate;
    ERUI_PageHandle rejected{UINT64_C(0x5AA55AA55AA55AA5)};
    invalid.tab_id = ascii("invalid/id");
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(rejected == ERUI_INVALID_PAGE);
    invalid = duplicate;
    invalid.title = {};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
    std::wstring maximum_title(
        ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS, L'M');
    invalid = duplicate;
    invalid.tab_id = ascii("maximum-title");
    invalid.title = utf16(maximum_title);
    ERUI_PageHandle maximum_title_page{};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &invalid, &maximum_title_page) == ERUI_OK);
    ERUI_PagePresentationDesc maximum_presentation{};
    maximum_presentation.size = sizeof(maximum_presentation);
    maximum_presentation.page_title = utf16(maximum_title);
    ERUI_TEST_CHECK(registry.set_page_presentation(
        provider_z,
        maximum_title_page,
        &maximum_presentation) == ERUI_OK);
    std::wstring oversized_title(
        ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS + 1u, L'X');
    invalid = duplicate;
    invalid.tab_id = ascii("oversized-title");
    invalid.title = utf16(oversized_title);
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);
    ERUI_PagePresentationDesc oversized_presentation{};
    oversized_presentation.size = sizeof(oversized_presentation);
    oversized_presentation.page_title = utf16(oversized_title);
    ERUI_TEST_CHECK(registry.set_page_presentation(
        provider_z,
        maximum_title_page,
        &oversized_presentation) == ERUI_INVALID_ARGUMENT);
    invalid = duplicate;
    invalid.reserved[1] = 1;
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);

    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT> invalid_pixels{};
    ERUI_TabIconDesc bad_icon{};
    bad_icon.size = sizeof(bad_icon);
    bad_icon.format = static_cast<ERUI_TabIconFormat>(2u);
    bad_icon.byte_count = static_cast<std::uint32_t>(invalid_pixels.size());
    bad_icon.pixels = invalid_pixels.data();
    invalid = duplicate;
    invalid.tab_id = ascii("invalid-icon-format");
    invalid.icon = &bad_icon;
    ERUI_TEST_CHECK(registry.add_tab(
        provider_z, &invalid, &rejected) == ERUI_INVALID_ARGUMENT);

    ERUI_TEST_CHECK(registry.commit_provider(provider_z) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_a) == ERUI_OK);
    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(menu->tab_count() == 5);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->tabs.size() == 5);
    ERUI_TEST_CHECK(compiled->tab_page_indices.size() ==
        compiled->tabs.size());

    constexpr std::array<std::string_view, 5> expected_keys{
        "tabs.z/main",
        "tabs.a/main",
        "tabs.a/settings",
        "tabs.z/maximum-title",
        "tabs.z/fallback",
    };
    for (std::size_t index = 0; index < compiled->tabs.size(); ++index) {
        const auto& compiled_tab = compiled->tabs[index];
        ERUI_TEST_CHECK(compiled_tab.page_index ==
            compiled->tab_page_indices[index]);
        ERUI_TEST_CHECK(compiled_tab.metadata.has_value());
        const erui::TabMetadata& metadata = *compiled_tab.metadata;
        ERUI_TEST_CHECK(metadata.provider_id + "/" + metadata.tab_id ==
            expected_keys[index]);
    }
    const erui::TabMetadata& copied_metadata =
        *compiled->tabs.front().metadata;
    ERUI_TEST_CHECK(copied_metadata.preferred_order == 50);
    ERUI_TEST_CHECK(copied_metadata.player_order.has_value() &&
        *copied_metadata.player_order == -10);
    ERUI_TEST_CHECK(copied_metadata.effective_order == -10);
    ERUI_TEST_CHECK(copied_metadata.icon.source ==
        erui::TabIconSource::rgba8);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels.size() ==
        ERUI_TAB_ICON_RGBA8_BYTE_COUNT);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels[0] == 12);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels[1] == 34);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels[2] == 56);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels[3] == 0);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels[7] == 127);
    ERUI_TEST_CHECK(copied_metadata.icon.rgba8_pixels[11] == 255);

    const erui::detail::CompiledPage& copied_compiled_page =
        compiled->pages[compiled->tabs.front().page_index];
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        copied_compiled_page.title_id)) == L"Copied title");
    ERUI_TEST_CHECK(copied_compiled_page.rows.size() == 1);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        copied_compiled_page.rows.front().label_id)) == L"Copied row");
    ERUI_TEST_CHECK(compiled->tabs[1].metadata->icon.source ==
        erui::TabIconSource::fallback);
    ERUI_TEST_CHECK(compiled->tabs[1].metadata->icon.rgba8_pixels.empty());

    const erui::detail::TabDiagnosticsReport report =
        registry.tab_diagnostics_report();
    ERUI_TEST_CHECK(report.order_complete);
    ERUI_TEST_CHECK(report.ordered_tabs.size() == expected_keys.size());
    for (std::size_t index = 0; index < expected_keys.size(); ++index) {
        ERUI_TEST_CHECK(report.ordered_tabs[index].stable_key ==
            expected_keys[index]);
        ERUI_TEST_CHECK(report.ordered_tabs[index].position == index + 1);
    }
}

void verify_custom_tab_backend_gate() {
    erui::host::Registry registry{};
    ERUI_TEST_CHECK(!registry.custom_tabs_backend_available());
    registry.open_registration();
    ERUI_TEST_CHECK(!registry.enable_custom_tabs_backend());
    ERUI_TEST_CHECK(!registry.custom_tabs_backend_available());

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_3;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("tabs.backend-disabled");
    provider.display_name = utf16(L"Disabled tab backend");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_3,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    // A copied 1.3 entry point fails closed before inspecting either public
    // descriptor pointer when its authoritative native backend is absent.
    ERUI_PageHandle untouched{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_handle, nullptr, &untouched) == ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched == UINT64_C(0xA55AA55AA55AA55A));
}

void verify_custom_tab_committed_limit() {
    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_custom_tabs_backend());
    registry.open_registration();

    const auto register_provider = [&](const char* id,
                                       ERUI_ProviderHandle& handle) {
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_3;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii(id);
        provider.display_name = utf16(L"Tab limit");
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_3, &provider, &handle, &root) == ERUI_OK);
    };
    const auto add_tabs = [&](ERUI_ProviderHandle provider,
                              std::string_view prefix,
                              std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            const std::string id = std::string(prefix) +
                std::to_string(index);
            ERUI_TabDesc tab{};
            tab.size = sizeof(tab);
            tab.tab_id = ascii(id);
            tab.title = utf16(L"Bounded tab");
            tab.preferred_order = static_cast<std::int32_t>(index);
            ERUI_PageHandle page{};
            ERUI_TEST_CHECK(registry.add_tab(
                provider, &tab, &page) == ERUI_OK);
        }
    };

    ERUI_ProviderHandle first{};
    ERUI_ProviderHandle second{};
    register_provider("limit.first", first);
    register_provider("limit.second", second);

    // One provider can own the complete API 1.3 budget. This also exercises
    // the per-provider page budget at 1 root page + 1,024 tab pages and proves
    // that tab indices above the former eight-bit range are accepted.
    add_tabs(first, "first-", ERUI_CUSTOM_TAB_MAX_COUNT);
    ERUI_TabDesc overflow{};
    overflow.size = sizeof(overflow);
    overflow.tab_id = ascii("overflow");
    overflow.title = utf16(L"Overflow");
    ERUI_PageHandle overflow_page{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(registry.add_tab(
        first, &overflow, &overflow_page) == ERUI_TAB_LIMIT_REACHED);
    ERUI_TEST_CHECK(overflow_page == ERUI_INVALID_PAGE);

    // Draft tabs do not reserve the process-wide committed budget. The
    // second provider can stage a tab, but its commit must fail atomically
    // after the first provider commits the complete budget.
    add_tabs(second, "second-", 1);
    ERUI_TEST_CHECK(registry.commit_provider(first) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(second) ==
        ERUI_TAB_LIMIT_REACHED);
    ERUI_TEST_CHECK(registry.abort_provider(second) == ERUI_OK);

    ERUI_ProviderHandle after_limit{};
    register_provider("limit.after", after_limit);
    ERUI_TEST_CHECK(registry.add_tab(
        after_limit, &overflow, &overflow_page) == ERUI_TAB_LIMIT_REACHED);
    ERUI_TEST_CHECK(registry.abort_provider(after_limit) == ERUI_OK);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(menu->tab_count() == ERUI_CUSTOM_TAB_MAX_COUNT);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->tabs.size() == ERUI_CUSTOM_TAB_MAX_COUNT);
    ERUI_TEST_CHECK(compiled->tabs[255].metadata.has_value());
    ERUI_TEST_CHECK(compiled->tabs[256].metadata.has_value());
    ERUI_TEST_CHECK(compiled->tabs.back().metadata.has_value());
    ERUI_TEST_CHECK(compiled->tabs[255].metadata->tab_id == "first-255");
    ERUI_TEST_CHECK(compiled->tabs[256].metadata->tab_id == "first-256");
    ERUI_TEST_CHECK(compiled->tabs.back().metadata->tab_id == "first-1023");
}

void verify_fixed_fifty_custom_tab_stress() {
    constexpr std::size_t kProviderCount = 2;
    constexpr std::size_t kStressTabCount = 50;
    constexpr std::size_t kTabsPerProvider =
        kStressTabCount / kProviderCount;
    constexpr std::size_t kRowsPerTab = 20;
    constexpr std::uint8_t kLiveFirstPageCapacity = 7;
    static_assert(kTabsPerProvider * kProviderCount ==
        kStressTabCount);
    static_assert(ERUI_CUSTOM_TAB_MAX_COUNT >= kStressTabCount);

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_custom_tabs_backend());
    registry.open_registration();

    struct ProviderFixture final {
        const char* id{};
        std::wstring title_prefix{};
        std::int32_t order_offset{};
        ERUI_ProviderHandle handle{ERUI_INVALID_PROVIDER};
    };
    std::array<ProviderFixture, kProviderCount> providers{{
        {"stress.alpha", L"Alpha", 0},
        {"stress.beta", L"Beta", 1},
    }};

    for (ProviderFixture& fixture : providers) {
        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_3;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii(fixture.id);
        provider.display_name = utf16(fixture.title_prefix);
        ERUI_PageHandle root{ERUI_INVALID_PAGE};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_3,
            &provider,
            &fixture.handle,
            &root) == ERUI_OK);
        ERUI_TEST_CHECK(fixture.handle != ERUI_INVALID_PROVIDER);
        ERUI_TEST_CHECK(root != ERUI_INVALID_PAGE);
    }

    for (std::size_t provider_index = 0;
         provider_index < providers.size(); ++provider_index) {
        ProviderFixture& fixture = providers[provider_index];
        for (std::size_t tab_index = 0;
             tab_index < kTabsPerProvider; ++tab_index) {
            std::string tab_id = "tab-" + std::to_string(tab_index);
            std::wstring title = fixture.title_prefix + L" Tab " +
                std::to_wstring(tab_index);

            ERUI_TabDesc tab{};
            tab.size = sizeof(tab);
            tab.tab_id = ascii(tab_id);
            tab.title = utf16(title);
            tab.preferred_order = static_cast<std::int32_t>(
                tab_index * providers.size() + provider_index);
            ERUI_PageHandle page{ERUI_INVALID_PAGE};
            ERUI_TEST_CHECK(registry.add_tab(
                fixture.handle, &tab, &page) == ERUI_OK);
            ERUI_TEST_CHECK(page != ERUI_INVALID_PAGE);

            // Every public view is borrowed only for the call. Overwrite the
            // source storage immediately so the compiled assertions below
            // prove that the Registry retained independent copies.
            tab_id.assign("mutated");
            title.assign(L"mutated");

            for (std::size_t row_index = 0;
                 row_index < kRowsPerTab; ++row_index) {
                std::wstring label = fixture.title_prefix + L" Tab " +
                    std::to_wstring(tab_index) + L" Row " +
                    std::to_wstring(row_index);
                std::wstring row_help = fixture.title_prefix + L" Help " +
                    std::to_wstring(tab_index) + L"/" +
                    std::to_wstring(row_index);
                const std::uint8_t initial_value =
                    static_cast<std::uint8_t>(
                        (provider_index + tab_index + row_index) % 2);
                ERUI_ToggleDesc toggle{};
                toggle.size = sizeof(toggle);
                toggle.label = utf16(label);
                toggle.help = utf16(row_help);
                toggle.initial_value = initial_value;
                toggle.enabled = 1;
                ERUI_RowHandle row{ERUI_INVALID_ROW};
                ERUI_TEST_CHECK(registry.add_toggle(
                    fixture.handle, page, &toggle, &row) == ERUI_OK);
                ERUI_TEST_CHECK(row != ERUI_INVALID_ROW);
                label.assign(L"mutated");
                row_help.assign(L"mutated");
            }
        }
    }

    for (const ProviderFixture& fixture : providers) {
        ERUI_TEST_CHECK(registry.commit_provider(fixture.handle) == ERUI_OK);
    }

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    ERUI_TEST_CHECK(menu->tab_count() == kStressTabCount);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->tabs.size() == kStressTabCount);
    ERUI_TEST_CHECK(compiled->tab_page_indices.size() ==
        kStressTabCount);
    ERUI_TEST_CHECK(compiled->pagination_required);

    std::size_t observed_rows = 0;
    for (std::size_t ordered_index = 0;
         ordered_index < compiled->tabs.size(); ++ordered_index) {
        const std::size_t provider_index = ordered_index % providers.size();
        const std::size_t tab_index = ordered_index / providers.size();
        const ProviderFixture& fixture = providers[provider_index];
        const erui::detail::CompiledTab& compiled_tab =
            compiled->tabs[ordered_index];
        ERUI_TEST_CHECK(compiled_tab.metadata.has_value());
        ERUI_TEST_CHECK(compiled_tab.page_index ==
            compiled->tab_page_indices[ordered_index]);
        ERUI_TEST_CHECK(compiled_tab.page_index < compiled->pages.size());

        const erui::TabMetadata& metadata = *compiled_tab.metadata;
        ERUI_TEST_CHECK(metadata.provider_id == fixture.id);
        ERUI_TEST_CHECK(metadata.tab_id ==
            "tab-" + std::to_string(tab_index));
        ERUI_TEST_CHECK(metadata.preferred_order ==
            static_cast<std::int32_t>(ordered_index));
        ERUI_TEST_CHECK(metadata.effective_order ==
            static_cast<std::int32_t>(ordered_index));
        ERUI_TEST_CHECK(!metadata.player_order.has_value());

        const erui::detail::CompiledPage& page =
            compiled->pages[compiled_tab.page_index];
        ERUI_TEST_CHECK(page.kind == erui::PageKind::tab);
        ERUI_TEST_CHECK(page.rows.size() == kRowsPerTab);
        ERUI_TEST_CHECK(page.tab_physical_title_plan != nullptr);
        ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(page.title_id)) ==
            fixture.title_prefix + L" Tab " + std::to_wstring(tab_index));

        for (std::size_t row_index = 0;
             row_index < page.rows.size(); ++row_index) {
            const erui::detail::CompiledRow& row = page.rows[row_index];
            const std::uint8_t expected_value =
                static_cast<std::uint8_t>(
                    (provider_index + tab_index + row_index) % 2);
            ERUI_TEST_CHECK(row.kind == erui::RowKind::toggle);
            ERUI_TEST_CHECK(row.byte_value != nullptr);
            ERUI_TEST_CHECK(*row.byte_value == expected_value);
            ERUI_TEST_CHECK(row.last_observed_value == expected_value);
            ERUI_TEST_CHECK(std::wstring(
                compiled->texts.lookup(row.label_id)) ==
                fixture.title_prefix + L" Tab " +
                    std::to_wstring(tab_index) + L" Row " +
                    std::to_wstring(row_index));
        }
        observed_rows += page.rows.size();

        erui::detail::PageSlice first{};
        erui::detail::PageSlice continuation{};
        ERUI_TEST_CHECK(erui::detail::resolve_paginated_slice(
            compiled_tab.page_index,
            page.rows.size(),
            kLiveFirstPageCapacity,
            erui::detail::native_subpage_capacity,
            0,
            first));
        ERUI_TEST_CHECK(erui::detail::resolve_paginated_slice(
            compiled_tab.page_index,
            page.rows.size(),
            kLiveFirstPageCapacity,
            erui::detail::native_subpage_capacity,
            1,
            continuation));
        ERUI_TEST_CHECK(first.slice_count == 2 &&
            first.first_row == 0 && first.row_count == 6 &&
            !first.has_previous && first.has_next);
        ERUI_TEST_CHECK(continuation.slice_count == 2 &&
            continuation.first_row == 6 && continuation.row_count == 14 &&
            continuation.has_previous && !continuation.has_next);

        const erui::detail::PageRoute main_route =
            erui::detail::PageRoute::tab_main(
                compiled_tab.page_index, kLiveFirstPageCapacity);
        const erui::detail::PageRoute continuation_route =
            erui::detail::PageRoute::tab_continuation(
                compiled_tab.page_index, kLiveFirstPageCapacity, 1);
        ERUI_TEST_CHECK(erui::detail::is_previous_route_target(
            continuation_route, main_route));
        const erui::detail::ResolvedPagePresentation presentation =
            compiled->resolve_presentation(continuation_route);
        ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
            presentation.outer_title_id)) == fixture.title_prefix);
        ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
            presentation.page_title_id)) ==
            fixture.title_prefix + L" Tab " +
                std::to_wstring(tab_index) + L" (2/2)");
    }
    ERUI_TEST_CHECK(observed_rows == kStressTabCount * kRowsPerTab);

    const erui::detail::TabDiagnosticsReport report =
        registry.tab_diagnostics_report();
    ERUI_TEST_CHECK(report.order_complete);
    ERUI_TEST_CHECK(report.ordered_tabs.size() == kStressTabCount);
    for (std::size_t index = 0; index < report.ordered_tabs.size(); ++index) {
        ERUI_TEST_CHECK(report.ordered_tabs[index].position == index + 1);
        ERUI_TEST_CHECK(report.ordered_tabs[index].stable_key ==
            std::string(providers[index % providers.size()].id) + "/tab-" +
                std::to_string(index / providers.size()));
    }
}

void verify_api_1_4_row_update_contract() {
    using erui::detail::RowPresentationView;
    erui::detail::reset_row_presentation_state();

    erui::host::Registry unavailable{};
    unavailable.open_registration();
    ERUI_ProviderDesc unavailable_provider{};
    unavailable_provider.size = sizeof(unavailable_provider);
    unavailable_provider.api_version = ERUI_API_VERSION_1_4;
    unavailable_provider.owner_module = GetModuleHandleW(nullptr);
    unavailable_provider.provider_id = ascii("updates.unavailable");
    unavailable_provider.display_name = utf16(L"Unavailable updates");
    ERUI_ProviderHandle unavailable_handle{};
    ERUI_PageHandle unavailable_root{};
    ERUI_TEST_CHECK(unavailable.register_provider(
        ERUI_API_VERSION_1_4,
        &unavailable_provider,
        &unavailable_handle,
        &unavailable_root) == ERUI_OK);
    ERUI_ToggleDesc unavailable_toggle{};
    unavailable_toggle.size = sizeof(unavailable_toggle);
    unavailable_toggle.label = utf16(L"Original");
    unavailable_toggle.help = utf16(L"Original help");
    unavailable_toggle.enabled = 1;
    ERUI_RowHandle unavailable_row{};
    ERUI_TEST_CHECK(unavailable.add_toggle(
        unavailable_handle,
        unavailable_root,
        &unavailable_toggle,
        &unavailable_row) == ERUI_OK);
    ERUI_TEST_CHECK(unavailable.commit_provider(unavailable_handle) == ERUI_OK);

    ERUI_RowUpdateDesc unavailable_row_update{};
    unavailable_row_update.size = sizeof(unavailable_row_update);
    unavailable_row_update.row = unavailable_row;
    unavailable_row_update.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    unavailable_row_update.label = styled_text(L"Rejected");
    ERUI_UpdateDesc unavailable_update{};
    unavailable_update.size = sizeof(unavailable_update);
    unavailable_update.kind = ERUI_UPDATE_KIND_ROW;
    unavailable_update.payload = &unavailable_row_update;
    ERUI_UpdateBatchDesc unavailable_batch{};
    unavailable_batch.size = sizeof(unavailable_batch);
    unavailable_batch.updates = &unavailable_update;
    unavailable_batch.update_count = 1;
    unavailable_batch.update_stride = sizeof(unavailable_update);
    ERUI_TEST_CHECK(unavailable.commit_updates(
        unavailable_handle, &unavailable_batch) == ERUI_NOT_SUPPORTED);

    // Negotiating API 1.4 does not by itself opt rows into the native
    // presentation backend. When ERUI_CAP_UPDATES is unavailable, ordinary
    // rows must compile exactly like earlier APIs and require no update hook.
    std::unique_ptr<erui::Menu> unavailable_menu =
        unavailable.freeze_and_build();
    ERUI_TEST_CHECK(unavailable_menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> unavailable_compiled =
        erui::detail::MenuCompiler::compile(*unavailable_menu);
    ERUI_TEST_CHECK(unavailable_compiled != nullptr);
    ERUI_TEST_CHECK(unavailable_compiled->root_page().rows.size() == 1);
    const erui::detail::CompiledRow& unavailable_compiled_row =
        unavailable_compiled->root_page().rows.front();
    ERUI_TEST_CHECK(
        unavailable_compiled_row.provider == ERUI_INVALID_PROVIDER);
    ERUI_TEST_CHECK(unavailable_compiled_row.row == ERUI_INVALID_ROW);
    ERUI_TEST_CHECK(unavailable_compiled_row.presentation_slot ==
        erui::detail::invalid_compiled_index);
    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(
        *unavailable_compiled));
    ERUI_TEST_CHECK(
        erui::detail::editable_row_presentation_count() == 0);
    erui::detail::reset_row_presentation_state();

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    registry.open_registration();
    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("updates.contract");
    provider.display_name = utf16(L"Update contract");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    ERUI_SliderDesc slider{};
    slider.size = sizeof(slider);
    slider.label = utf16(L"Initial <label> & value");
    slider.help = utf16(L"Initial help <&>");
    slider.minimum = 0;
    slider.maximum = 2;
    slider.step = 1;
    slider.enabled = 1;
    ERUI_RowHandle row{};
    ERUI_TEST_CHECK(registry.add_slider(
        provider_handle, root, &slider, &row) == ERUI_OK);

    ERUI_ToggleDesc companion_toggle{};
    companion_toggle.size = sizeof(companion_toggle);
    companion_toggle.label = utf16(L"Companion <plain>");
    companion_toggle.help = utf16(L"Companion & help");
    companion_toggle.enabled = 1;
    ERUI_RowHandle companion_row{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle,
        root,
        &companion_toggle,
        &companion_row) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    constexpr ERUI_Color kGold{0x6B, 0x64, 0x50, 0};
    const ERUI_TextStyleSpan companion_label_span = text_style_span(
        0,
        9,
        ERUI_TEXT_STYLE_CUSTOM_COLOR | ERUI_TEXT_STYLE_UNDERLINE,
        kGold);
    const ERUI_TextStyleSpan companion_help_span = text_style_span(
        10,
        4,
        ERUI_TEXT_STYLE_BOLD);
    ERUI_RowUpdateDesc companion_update{};
    companion_update.size = sizeof(companion_update);
    companion_update.row = companion_row;
    companion_update.fields =
        ERUI_ROW_UPDATE_FIELD_LABEL | ERUI_ROW_UPDATE_FIELD_HELP;
    companion_update.label = styled_text(
        L"Companion <styled>", &companion_label_span, 1);
    companion_update.help = styled_text(
        L"Companion BOLD help", &companion_help_span, 1);
    ERUI_UpdateDesc update{};
    update.size = sizeof(update);
    update.kind = ERUI_UPDATE_KIND_ROW;
    update.payload = &companion_update;
    ERUI_UpdateBatchDesc batch{};
    batch.size = sizeof(batch);
    batch.updates = &update;
    batch.update_count = 1;
    batch.update_stride = sizeof(update);

    // Duplicate targets make the whole batch invalid.
    std::array<ERUI_UpdateDesc, 2> duplicates{update, update};
    batch.updates = duplicates.data();
    batch.update_count = static_cast<std::uint32_t>(duplicates.size());
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);
    batch.updates = &update;
    batch.update_count = 1;

    // Field bits that are absent require a canonical all-zero styled view.
    ERUI_RowUpdateDesc noncanonical = companion_update;
    noncanonical.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    noncanonical.help = styled_text(L"Hidden payload");
    update.payload = &noncanonical;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);
    update.payload = &companion_update;

    // A valid styled update accepted before compilation must survive the
    // preconfiguration queue and become the first fixed-address snapshot.
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == 2);
    const erui::detail::CompiledRow& compiled_row =
        compiled->root_page().rows.front();
    const erui::detail::CompiledRow& compiled_companion =
        compiled->root_page().rows[1];
    ERUI_TEST_CHECK(compiled_row.provider == provider_handle);
    ERUI_TEST_CHECK(compiled_row.row == row);
    ERUI_TEST_CHECK(compiled_companion.row == companion_row);

    // Plain registration is encoded before any HTML-mode Scaleform setter
    // can observe it. This is the first-frame path, independent of updates.
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_row.label_id)) ==
        L"Initial &lt;label&gt; &amp; value");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_row.help_id)) ==
        L"Initial help &lt;&amp;&gt;");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_companion.label_id)) ==
        L"<font color=\"#6B6450\"><u>Companion</u></font> "
        L"&lt;styled&gt;");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_companion.help_id)) ==
        L"Companion <b>BOLD</b> help");

    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(
        *compiled));
    erui::detail::synchronize_pending_row_presentations();
    ERUI_TEST_CHECK(!erui::detail::row_presentation_view(
        compiled_row.presentation_slot));
    const RowPresentationView companion_initial =
        erui::detail::row_presentation_view(
            compiled_companion.presentation_slot);
    ERUI_TEST_CHECK(companion_initial);
    ERUI_TEST_CHECK(std::wstring(companion_initial.label) ==
        L"<font color=\"#6B6450\"><u>Companion</u></font> "
        L"&lt;styled&gt;");
    ERUI_TEST_CHECK(std::wstring(companion_initial.help) ==
        L"Companion <b>BOLD</b> help");

    constexpr ERUI_Color kRed{0xFF, 0x40, 0x40, 0};
    constexpr ERUI_Color kBlue{0x40, 0xA0, 0xFF, 0};
    const ERUI_TextStyleSpan label_span = text_style_span(
        7,
        3,
        ERUI_TEXT_STYLE_CUSTOM_COLOR |
            ERUI_TEXT_STYLE_BOLD |
            ERUI_TEXT_STYLE_UNDERLINE,
        kRed);
    const ERUI_TextStyleSpan help_span = text_style_span(
        5,
        4,
        ERUI_TEXT_STYLE_CUSTOM_COLOR | ERUI_TEXT_STYLE_UNDERLINE,
        kBlue);
    ERUI_RowUpdateDesc row_update{};
    row_update.size = sizeof(row_update);
    row_update.row = row;
    row_update.fields =
        ERUI_ROW_UPDATE_FIELD_LABEL | ERUI_ROW_UPDATE_FIELD_HELP;
    row_update.label = styled_text(
        L"Styled RED & safe", &label_span, 1);
    row_update.help = styled_text(
        L"Help BLUE <tag>", &help_span, 1);
    update.payload = &row_update;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView initial =
        erui::detail::row_presentation_view(compiled_row.presentation_slot);
    ERUI_TEST_CHECK(initial);
    ERUI_TEST_CHECK(std::wstring(initial.label) ==
        L"Styled <font color=\"#FF4040\"><b><u>RED</u></b></font> "
        L"&amp; safe");
    ERUI_TEST_CHECK(std::wstring(initial.help) ==
        L"Help <font color=\"#40A0FF\"><u>BLUE</u></font> "
        L"&lt;tag&gt;");

    // A plain partial update resets styles for that field while preserving
    // the untouched field's styled representation.
    row_update.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    row_update.label = {};
    row_update.help = styled_text(L"Live <help> & plain");
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    const RowPresentationView before_sync =
        erui::detail::row_presentation_view(compiled_row.presentation_slot);
    ERUI_TEST_CHECK(before_sync.revision == initial.revision);
    ERUI_TEST_CHECK(std::wstring(before_sync.help) ==
        L"Help <font color=\"#40A0FF\"><u>BLUE</u></font> "
        L"&lt;tag&gt;");
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView after_sync =
        erui::detail::row_presentation_view(compiled_row.presentation_slot);
    ERUI_TEST_CHECK(std::wstring(after_sync.label) ==
        L"Styled <font color=\"#FF4040\"><b><u>RED</u></b></font> "
        L"&amp; safe");
    ERUI_TEST_CHECK(std::wstring(after_sync.help) ==
        L"Live &lt;help&gt; &amp; plain");
    ERUI_TEST_CHECK(after_sync.revision > initial.revision);

    row_update.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    row_update.label = styled_text(L"Plain <label> & reset");
    row_update.help = {};
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView plain_label =
        erui::detail::row_presentation_view(compiled_row.presentation_slot);
    ERUI_TEST_CHECK(std::wstring(plain_label.label) ==
        L"Plain &lt;label&gt; &amp; reset");
    ERUI_TEST_CHECK(std::wstring(plain_label.help) ==
        L"Live &lt;help&gt; &amp; plain");

    // One malformed span rejects the complete multi-row batch. Neither the
    // already-presented row nor its companion advances to a new revision.
    ERUI_RowUpdateDesc valid_in_failed_batch{};
    valid_in_failed_batch.size = sizeof(valid_in_failed_batch);
    valid_in_failed_batch.row = row;
    valid_in_failed_batch.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    valid_in_failed_batch.help = styled_text(L"Must not publish");

    ERUI_TextStyleSpan malformed_span = text_style_span(
        0, 6, ERUI_TEXT_STYLE_UNDERLINE);
    malformed_span.reserved[0] = 1;
    ERUI_RowUpdateDesc malformed_update{};
    malformed_update.size = sizeof(malformed_update);
    malformed_update.row = companion_row;
    malformed_update.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    malformed_update.label = styled_text(
        L"Broken", &malformed_span, 1);

    std::array failed_updates{
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &valid_in_failed_batch,
            {0, 0},
        },
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &malformed_update,
            {0, 0},
        },
    };
    batch.updates = failed_updates.data();
    batch.update_count = static_cast<std::uint32_t>(failed_updates.size());
    const std::uint64_t row_revision_before_failure = plain_label.revision;
    const std::uint64_t companion_revision_before_failure =
        companion_initial.revision;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView row_after_failure =
        erui::detail::row_presentation_view(compiled_row.presentation_slot);
    const RowPresentationView companion_after_failure =
        erui::detail::row_presentation_view(
            compiled_companion.presentation_slot);
    ERUI_TEST_CHECK(row_after_failure.revision ==
        row_revision_before_failure);
    ERUI_TEST_CHECK(companion_after_failure.revision ==
        companion_revision_before_failure);
    ERUI_TEST_CHECK(std::wstring(row_after_failure.help) ==
        L"Live &lt;help&gt; &amp; plain");
    ERUI_TEST_CHECK(std::wstring(companion_after_failure.label) ==
        L"<font color=\"#6B6450\"><u>Companion</u></font> "
        L"&lt;styled&gt;");

    // Subsequent partial commits prove the rejected batch did not mutate the
    // registry model behind the unchanged presentation generation.
    batch.updates = &update;
    batch.update_count = 1;
    row_update.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    row_update.label = styled_text(L"After failure");
    row_update.help = {};
    update.payload = &row_update;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView row_after_recovery =
        erui::detail::row_presentation_view(compiled_row.presentation_slot);
    ERUI_TEST_CHECK(std::wstring(row_after_recovery.label) ==
        L"After failure");
    ERUI_TEST_CHECK(std::wstring(row_after_recovery.help) ==
        L"Live &lt;help&gt; &amp; plain");

    ERUI_RowUpdateDesc companion_recovery{};
    companion_recovery.size = sizeof(companion_recovery);
    companion_recovery.row = companion_row;
    companion_recovery.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    companion_recovery.help = styled_text(L"Recovered help");
    update.payload = &companion_recovery;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView companion_after_recovery =
        erui::detail::row_presentation_view(
            compiled_companion.presentation_slot);
    ERUI_TEST_CHECK(std::wstring(companion_after_recovery.label) ==
        L"<font color=\"#6B6450\"><u>Companion</u></font> "
        L"&lt;styled&gt;");
    ERUI_TEST_CHECK(std::wstring(companion_after_recovery.help) ==
        L"Recovered help");

    erui::detail::reset_row_presentation_state();
}

void verify_api_1_4_button_styled_update_before_freeze() {
    using erui::detail::RowPresentationView;
    erui::detail::reset_row_presentation_state();

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("updates.button.prefreeze");
    provider.display_name = utf16(L"Button pre-freeze update");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    unsigned activations{};
    ERUI_ButtonDesc button{};
    button.size = sizeof(button);
    button.label = utf16(L"Styled text presentation");
    button.help = utf16(L"Initial plain help");
    button.callback = &pressed;
    button.user_data = &activations;
    button.enabled = 1;
    ERUI_RowHandle button_row{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle,
        root,
        &button,
        &button_row) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    constexpr ERUI_Color kGold{218u, 185u, 98u, 0u};
    constexpr ERUI_Color kBlue{96u, 180u, 255u, 0u};
    constexpr ERUI_Color kRed{255u, 96u, 96u, 0u};
    constexpr ERUI_Color kGreen{96u, 220u, 128u, 0u};
    const std::array label_spans{
        text_style_span(
            13,
            5,
            ERUI_TEXT_STYLE_CUSTOM_COLOR,
            kGold),
        text_style_span(
            21,
            9,
            ERUI_TEXT_STYLE_CUSTOM_COLOR | ERUI_TEXT_STYLE_UNDERLINE,
            kBlue),
    };
    const std::array help_spans{
        text_style_span(
            17,
            7,
            ERUI_TEXT_STYLE_CUSTOM_COLOR,
            kRed),
        text_style_span(
            43,
            10,
            ERUI_TEXT_STYLE_CUSTOM_COLOR | ERUI_TEXT_STYLE_UNDERLINE,
            kGreen),
    };

    ERUI_RowUpdateDesc row_update{};
    row_update.size = sizeof(row_update);
    row_update.row = button_row;
    row_update.fields =
        ERUI_ROW_UPDATE_FIELD_LABEL | ERUI_ROW_UPDATE_FIELD_HELP;
    row_update.label = styled_text(
        L"Styled text: COLOR / UNDERLINE",
        label_spans.data(),
        static_cast<std::uint32_t>(label_spans.size()));
    row_update.help = styled_text(
        L"Fragments can be colored independently and underlined without raw HTML.",
        help_spans.data(),
        static_cast<std::uint32_t>(help_spans.size()));
    ERUI_UpdateDesc update{};
    update.size = sizeof(update);
    update.kind = ERUI_UPDATE_KIND_ROW;
    update.payload = &row_update;
    ERUI_UpdateBatchDesc batch{};
    batch.size = sizeof(batch);
    batch.updates = &update;
    batch.update_count = 1;
    batch.update_stride = sizeof(update);
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == 1);
    const erui::detail::CompiledRow& compiled_button =
        compiled->root_page().rows.front();
    ERUI_TEST_CHECK(compiled_button.kind == erui::RowKind::button);
    ERUI_TEST_CHECK(compiled_button.provider == provider_handle);
    ERUI_TEST_CHECK(compiled_button.row == button_row);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_button.label_id)) ==
        L"Styled text: <font color=\"#DAB962\">COLOR</font> / "
        L"<font color=\"#60B4FF\"><u>UNDERLINE</u></font>");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_button.help_id)) ==
        L"Fragments can be <font color=\"#FF6060\">colored</font> "
        L"independently and <font color=\"#60DC80\"><u>underlined</u>"
        L"</font> without raw HTML.");

    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(
        *compiled));
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView presentation =
        erui::detail::row_presentation_view(
            compiled_button.presentation_slot);
    ERUI_TEST_CHECK(presentation);
    ERUI_TEST_CHECK(std::wstring(presentation.label) ==
        std::wstring(compiled->texts.lookup(compiled_button.label_id)));
    ERUI_TEST_CHECK(std::wstring(presentation.help) ==
        std::wstring(compiled->texts.lookup(compiled_button.help_id)));

    erui::detail::reset_row_presentation_state();
}

void verify_api_1_4_caption_update_contract() {
    using erui::detail::RowPresentationView;
    erui::detail::reset_row_presentation_state();

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("updates.caption.contract");
    provider.display_name = utf16(L"Caption update contract");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    unsigned presses{};
    ERUI_ButtonDesc button{};
    button.size = sizeof(button);
    button.label = utf16(L"Button row");
    button.help = utf16(L"Button help");
    button.callback = &pressed;
    button.user_data = &presses;
    button.enabled = 1;
    ERUI_RowHandle button_row{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle, root, &button, &button_row) == ERUI_OK);
    const ERUI_Utf16View choices[] = {
        utf16(L"Zero"), utf16(L"One"), utf16(L"Two")};
    ERUI_ChoiceDesc popup{};
    popup.size = sizeof(popup);
    popup.label = utf16(L"Popup row");
    popup.help = utf16(L"Popup help");
    popup.options = choices;
    popup.option_count = 3;
    popup.initial_index = 1;
    ERUI_RowHandle popup_row{};
    ERUI_TEST_CHECK(registry.add_popup_choice(
        provider_handle, root, &popup, &popup_row) == ERUI_OK);

    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16(L"Toggle row");
    toggle.help = utf16(L"Toggle help");
    toggle.enabled = 1;
    ERUI_RowHandle toggle_row{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, root, &toggle, &toggle_row) == ERUI_OK);

    ERUI_SubmenuDesc submenu{};
    submenu.size = sizeof(submenu);
    submenu.label = utf16(L"Submenu row");
    submenu.help = utf16(L"Submenu help");
    submenu.page_title = utf16(L"Caption child");
    submenu.enabled = 1;
    ERUI_PageHandle child{};
    ERUI_RowHandle submenu_row{};
    ERUI_TEST_CHECK(registry.add_submenu(
        provider_handle,
        root,
        &submenu,
        &child,
        &submenu_row) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    ERUI_UpdateDesc update{};
    update.size = sizeof(update);
    update.kind = ERUI_UPDATE_KIND_ROW;
    ERUI_UpdateBatchDesc batch{};
    batch.size = sizeof(batch);
    batch.updates = &update;
    batch.update_count = 1;
    batch.update_stride = sizeof(update);

    // The caption-expanded API 1.4 descriptor is larger than the earlier
    // development preview. Its leading size is rejected before the host
    // copies the complete current descriptor.
    alignas(ERUI_RowUpdateDesc) std::array<std::byte, 128> stale_storage{};
    const std::uint32_t stale_size =
        static_cast<std::uint32_t>(stale_storage.size());
    std::memcpy(stale_storage.data(), &stale_size, sizeof(stale_size));
    update.payload = stale_storage.data();
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc noncanonical{};
    noncanonical.size = sizeof(noncanonical);
    noncanonical.row = button_row;
    noncanonical.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    noncanonical.label = styled_text(L"Label only");
    noncanonical.caption = styled_text(L"Hidden caption payload");
    update.payload = &noncanonical;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc empty_caption{};
    empty_caption.size = sizeof(empty_caption);
    empty_caption.row = button_row;
    empty_caption.fields = ERUI_ROW_UPDATE_FIELD_CAPTION;
    empty_caption.caption = styled_text(L"");
    update.payload = &empty_caption;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc forbidden_caption{};
    forbidden_caption.size = sizeof(forbidden_caption);
    forbidden_caption.row = toggle_row;
    forbidden_caption.fields = ERUI_ROW_UPDATE_FIELD_CAPTION;
    forbidden_caption.caption = styled_text(L"Not a toggle caption");
    update.payload = &forbidden_caption;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    constexpr ERUI_Color kGold{0xDAu, 0xB9u, 0x62u, 0u};
    constexpr ERUI_Color kBlue{0x60u, 0xB4u, 0xFFu, 0u};
    const ERUI_TextStyleSpan button_span = text_style_span(
        7,
        4,
        ERUI_TEXT_STYLE_CUSTOM_COLOR | ERUI_TEXT_STYLE_UNDERLINE,
        kGold);
    const ERUI_TextStyleSpan popup_span = text_style_span(
        6,
        4,
        ERUI_TEXT_STYLE_CUSTOM_COLOR,
        kBlue);

    ERUI_RowUpdateDesc button_caption{};
    button_caption.size = sizeof(button_caption);
    button_caption.row = button_row;
    button_caption.fields = ERUI_ROW_UPDATE_FIELD_CAPTION;
    button_caption.caption = styled_text(
        L"Button GOLD caption", &button_span, 1);

    ERUI_RowUpdateDesc popup_caption{};
    popup_caption.size = sizeof(popup_caption);
    popup_caption.row = popup_row;
    popup_caption.fields = ERUI_ROW_UPDATE_FIELD_CAPTION;
    popup_caption.caption = styled_text(
        L"Popup BLUE", &popup_span, 1);

    ERUI_RowUpdateDesc submenu_caption{};
    submenu_caption.size = sizeof(submenu_caption);
    submenu_caption.row = submenu_row;
    submenu_caption.fields = ERUI_ROW_UPDATE_FIELD_CAPTION;
    submenu_caption.caption = styled_text(L"Open details");

    ERUI_RowUpdateDesc toggle_help{};
    toggle_help.size = sizeof(toggle_help);
    toggle_help.row = toggle_row;
    toggle_help.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    toggle_help.help = styled_text(L"Updated toggle help");

    std::array valid_updates{
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &button_caption,
            {0, 0},
        },
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &popup_caption,
            {0, 0},
        },
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &submenu_caption,
            {0, 0},
        },
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &toggle_help,
            {0, 0},
        },
    };
    batch.updates = valid_updates.data();
    batch.update_count = static_cast<std::uint32_t>(valid_updates.size());
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);

    // Kind validation participates in the complete transaction. The valid
    // Button mutation must not publish when a later Toggle caption is invalid.
    ERUI_RowUpdateDesc rejected_button_caption = button_caption;
    rejected_button_caption.caption = styled_text(L"Must not publish");
    std::array rejected_updates{
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &rejected_button_caption,
            {0, 0},
        },
        ERUI_UpdateDesc{
            sizeof(ERUI_UpdateDesc),
            ERUI_UPDATE_KIND_ROW,
            &forbidden_caption,
            {0, 0},
        },
    };
    batch.updates = rejected_updates.data();
    batch.update_count = static_cast<std::uint32_t>(rejected_updates.size());
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    // A later label-only update must carry the established caption override
    // forward rather than returning that field to native ownership.
    ERUI_RowUpdateDesc button_label{};
    button_label.size = sizeof(button_label);
    button_label.row = button_row;
    button_label.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    button_label.label = styled_text(L"Updated button row");
    update.payload = &button_label;
    batch.updates = &update;
    batch.update_count = 1;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(
        *compiled));
    erui::detail::synchronize_pending_row_presentations();

    const auto find_row = [&](ERUI_RowHandle handle)
        -> const erui::detail::CompiledRow* {
        for (const erui::detail::CompiledPage& page : compiled->pages) {
            for (const erui::detail::CompiledRow& candidate : page.rows) {
                if (candidate.row == handle) return &candidate;
            }
        }
        return nullptr;
    };
    const erui::detail::CompiledRow* const compiled_button =
        find_row(button_row);
    const erui::detail::CompiledRow* const compiled_popup =
        find_row(popup_row);
    const erui::detail::CompiledRow* const compiled_submenu =
        find_row(submenu_row);
    const erui::detail::CompiledRow* const compiled_toggle =
        find_row(toggle_row);
    ERUI_TEST_CHECK(compiled_button && compiled_popup && compiled_submenu &&
        compiled_toggle);

    const RowPresentationView button_view =
        erui::detail::row_presentation_view(
            compiled_button->presentation_slot);
    const RowPresentationView popup_view =
        erui::detail::row_presentation_view(
            compiled_popup->presentation_slot);
    const RowPresentationView submenu_view =
        erui::detail::row_presentation_view(
            compiled_submenu->presentation_slot);
    const RowPresentationView toggle_view =
        erui::detail::row_presentation_view(
            compiled_toggle->presentation_slot);
    ERUI_TEST_CHECK(button_view && popup_view && submenu_view && toggle_view);
    ERUI_TEST_CHECK(std::wstring(button_view.label) == L"Updated button row");
    ERUI_TEST_CHECK(button_view.caption != nullptr);
    ERUI_TEST_CHECK(std::wstring(button_view.caption) ==
        L"Button <font color=\"#DAB962\"><u>GOLD</u></font> caption");
    ERUI_TEST_CHECK(popup_view.caption != nullptr);
    ERUI_TEST_CHECK(std::wstring(popup_view.caption) ==
        L"Popup <font color=\"#60B4FF\">BLUE</font>");
    ERUI_TEST_CHECK(submenu_view.caption != nullptr);
    ERUI_TEST_CHECK(std::wstring(submenu_view.caption) == L"Open details");
    ERUI_TEST_CHECK(toggle_view.caption == nullptr);
    ERUI_TEST_CHECK(std::wstring(toggle_view.help) ==
        L"Updated toggle help");

    const wchar_t* const stable_popup_caption = popup_view.caption;
    ERUI_RowUpdateDesc popup_help{};
    popup_help.size = sizeof(popup_help);
    popup_help.row = popup_row;
    popup_help.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    popup_help.help = styled_text(L"Popup help changed later");
    update.payload = &popup_help;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const RowPresentationView popup_after_help =
        erui::detail::row_presentation_view(
            compiled_popup->presentation_slot);
    ERUI_TEST_CHECK(popup_after_help.caption == stable_popup_caption);
    ERUI_TEST_CHECK(std::wstring(popup_after_help.caption) ==
        L"Popup <font color=\"#60B4FF\">BLUE</font>");
    ERUI_TEST_CHECK(std::wstring(popup_after_help.help) ==
        L"Popup help changed later");

    erui::detail::reset_row_presentation_state();
}

void verify_api_1_4_row_availability_contract() {
    erui::detail::reset_row_presentation_state();

    // Availability is independently gated even when ordinary text updates
    // are supported.
    erui::host::Registry unavailable{};
    ERUI_TEST_CHECK(unavailable.enable_updates_backend());
    unavailable.open_registration();
    ERUI_ProviderDesc unavailable_provider{};
    unavailable_provider.size = sizeof(unavailable_provider);
    unavailable_provider.api_version = ERUI_API_VERSION_1_4;
    unavailable_provider.owner_module = GetModuleHandleW(nullptr);
    unavailable_provider.provider_id = ascii("availability.unavailable");
    unavailable_provider.display_name = utf16(L"Availability unavailable");
    ERUI_ProviderHandle unavailable_handle{};
    ERUI_PageHandle unavailable_root{};
    ERUI_TEST_CHECK(unavailable.register_provider(
        ERUI_API_VERSION_1_4,
        &unavailable_provider,
        &unavailable_handle,
        &unavailable_root) == ERUI_OK);
    ERUI_ToggleDesc unavailable_toggle{};
    unavailable_toggle.size = sizeof(unavailable_toggle);
    unavailable_toggle.label = utf16(L"Toggle");
    unavailable_toggle.help = utf16(L"Help");
    unavailable_toggle.enabled = 1u;
    ERUI_RowHandle unavailable_row{};
    ERUI_TEST_CHECK(unavailable.add_toggle(
        unavailable_handle,
        unavailable_root,
        &unavailable_toggle,
        &unavailable_row) == ERUI_OK);
    ERUI_TEST_CHECK(unavailable.commit_provider(unavailable_handle) == ERUI_OK);
    ERUI_RowUpdateDesc unavailable_row_update{};
    unavailable_row_update.size = sizeof(unavailable_row_update);
    unavailable_row_update.row = unavailable_row;
    unavailable_row_update.fields = ERUI_ROW_UPDATE_FIELD_INTERACTIVE;
    unavailable_row_update.interactive = 0u;
    ERUI_UpdateDesc unavailable_update{};
    unavailable_update.size = sizeof(unavailable_update);
    unavailable_update.kind = ERUI_UPDATE_KIND_ROW;
    unavailable_update.payload = &unavailable_row_update;
    ERUI_UpdateBatchDesc unavailable_batch{};
    unavailable_batch.size = sizeof(unavailable_batch);
    unavailable_batch.updates = &unavailable_update;
    unavailable_batch.update_count = 1u;
    unavailable_batch.update_stride = sizeof(unavailable_update);
    ERUI_TEST_CHECK(unavailable.commit_updates(
        unavailable_handle, &unavailable_batch) == ERUI_NOT_SUPPORTED);

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(!registry.enable_row_availability_backend());
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    ERUI_TEST_CHECK(registry.enable_row_availability_backend());
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("availability.contract");
    provider.display_name = utf16(L"Availability contract");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16(L"Target toggle");
    toggle.help = utf16(L"Original toggle help");
    toggle.enabled = 1u;
    ERUI_RowHandle toggle_row{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, root, &toggle, &toggle_row) == ERUI_OK);

    ERUI_SliderDesc slider{};
    slider.size = sizeof(slider);
    slider.label = utf16(L"Slider");
    slider.help = utf16(L"Slider help");
    slider.minimum = 0u;
    slider.maximum = 2u;
    slider.step = 1u;
    slider.enabled = 1u;
    ERUI_RowHandle slider_row{};
    ERUI_TEST_CHECK(registry.add_slider(
        provider_handle, root, &slider, &slider_row) == ERUI_OK);

    unsigned button_presses{};
    ERUI_ButtonDesc button{};
    button.size = sizeof(button);
    button.label = utf16(L"Button");
    button.help = utf16(L"Button help");
    button.callback = &pressed;
    button.user_data = &button_presses;
    button.enabled = 1u;
    ERUI_RowHandle button_row{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle, root, &button, &button_row) == ERUI_OK);
    button.label = utf16(L"Construction-disabled button");
    button.help = utf16(L"This action row is not materialized.");
    button.enabled = 0u;
    ERUI_RowHandle disabled_button_row{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle, root, &button, &disabled_button_row) == ERUI_OK);

    ERUI_NumericSliderDesc numeric{};
    numeric.size = sizeof(numeric);
    numeric.label = utf16(L"Numeric slider");
    numeric.help = utf16(L"Numeric slider help");
    numeric.minimum_scaled_value = -10;
    numeric.maximum_scaled_value = 10;
    numeric.step_scaled_value = 1;
    numeric.initial_scaled_value = 0;
    numeric.decimal_places = 1u;
    numeric.enabled = 1u;
    ERUI_RowHandle numeric_row{};
    ERUI_TEST_CHECK(registry.add_numeric_slider(
        provider_handle, root, &numeric, &numeric_row) == ERUI_OK);

    const std::array<ERUI_Utf16View, 3> custom_values{
        utf16(L"Low"), utf16(L"Medium"), utf16(L"High")};
    ERUI_CustomSliderDesc custom{};
    custom.size = sizeof(custom);
    custom.label = utf16(L"Custom slider");
    custom.help = utf16(L"Custom slider help");
    custom.values = custom_values.data();
    custom.value_count = static_cast<std::uint32_t>(custom_values.size());
    custom.initial_index = 1u;
    custom.enabled = 1u;
    ERUI_RowHandle custom_row{};
    ERUI_TEST_CHECK(registry.add_custom_slider(
        provider_handle, root, &custom, &custom_row) == ERUI_OK);

    const std::array<ERUI_Utf16View, 3> choice_values{
        utf16(L"First"), utf16(L"Second"), utf16(L"Third")};
    ERUI_ChoiceDesc choice{};
    choice.size = sizeof(choice);
    choice.label = utf16(L"Choice");
    choice.help = utf16(L"Choice help");
    choice.options = choice_values.data();
    choice.option_count = static_cast<std::uint32_t>(choice_values.size());
    choice.initial_index = 1u;
    ERUI_RowHandle inline_choice_row{};
    ERUI_TEST_CHECK(registry.add_inline_choice(
        provider_handle, root, &choice, &inline_choice_row) == ERUI_OK);
    choice.label = utf16(L"Popup choice");
    ERUI_RowHandle popup_choice_row{};
    ERUI_TEST_CHECK(registry.add_popup_choice(
        provider_handle, root, &choice, &popup_choice_row) == ERUI_OK);

    ERUI_TextInputDesc text_input{};
    text_input.size = sizeof(text_input);
    text_input.label = utf16(L"Text input");
    text_input.help = utf16(L"Text input help");
    text_input.initial_value = utf16(L"Editable");
    text_input.placeholder = utf16(L"Type here");
    text_input.maximum_length = 16u;
    ERUI_RowHandle text_input_row{};
    ERUI_TEST_CHECK(registry.add_text_input(
        provider_handle, root, &text_input, &text_input_row) == ERUI_OK);

    ERUI_ColorPickerDesc color_picker{};
    color_picker.size = sizeof(color_picker);
    color_picker.label = utf16(L"Color picker");
    color_picker.help = utf16(L"Color picker help");
    color_picker.initial_value = ERUI_Color{96u, 180u, 255u, 0u};
    color_picker.enabled = 1u;
    ERUI_RowHandle color_picker_row{};
    ERUI_TEST_CHECK(registry.add_color_picker(
        provider_handle, root, &color_picker, &color_picker_row) == ERUI_OK);
    color_picker.label = utf16(L"Construction-disabled color picker");
    color_picker.help = utf16(L"This action row is not materialized.");
    color_picker.enabled = 0u;
    ERUI_RowHandle disabled_color_picker_row{};
    ERUI_TEST_CHECK(registry.add_color_picker(
        provider_handle,
        root,
        &color_picker,
        &disabled_color_picker_row) == ERUI_OK);

    ERUI_SubmenuDesc submenu{};
    submenu.size = sizeof(submenu);
    submenu.label = utf16(L"Submenu");
    submenu.help = utf16(L"Submenu help");
    submenu.page_title = utf16(L"Child page");
    submenu.enabled = 1u;
    ERUI_PageHandle child_page{};
    ERUI_RowHandle submenu_row{};
    ERUI_TEST_CHECK(registry.add_submenu(
        provider_handle,
        root,
        &submenu,
        &child_page,
        &submenu_row) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    ERUI_RowUpdateDesc availability{};
    availability.size = sizeof(availability);
    availability.row = toggle_row;
    availability.fields = ERUI_ROW_UPDATE_FIELD_INTERACTIVE |
        ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
    availability.interactive = 0u;
    availability.availability_appearance =
        ERUI_AVAILABILITY_APPEARANCE_DISABLED;
    ERUI_UpdateDesc update{};
    update.size = sizeof(update);
    update.kind = ERUI_UPDATE_KIND_ROW;
    update.payload = &availability;
    ERUI_UpdateBatchDesc batch{};
    batch.size = sizeof(batch);
    batch.updates = &update;
    batch.update_count = 1u;
    batch.update_stride = sizeof(update);
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);

    // Every type-specific row family validated in game accepts the same
    // availability fields. Submenu remains outside this contract.
    const std::array availability_rows{
        button_row,
        slider_row,
        numeric_row,
        custom_row,
        inline_choice_row,
        popup_choice_row,
        text_input_row,
        color_picker_row,
    };
    for (const ERUI_RowHandle row : availability_rows) {
        availability.row = row;
        update.payload = &availability;
        ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
            ERUI_OK);
    }
    availability.row = toggle_row;

    // Button and ColorPicker construction-disabled rows are intentionally
    // omitted by MenuCompiler. Availability cannot target a native row that
    // does not exist, so reject it instead of silently accepting no snapshot.
    const std::array omitted_availability_rows{
        disabled_button_row, disabled_color_picker_row};
    for (const ERUI_RowHandle row : omitted_availability_rows) {
        availability.row = row;
        update.payload = &availability;
        ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
            ERUI_INVALID_ARGUMENT);
    }
    availability.row = toggle_row;

    ERUI_RowUpdateDesc invalid = availability;
    invalid.fields = ERUI_ROW_UPDATE_FIELD_INTERACTIVE;
    invalid.interactive = 2u;
    invalid.availability_appearance =
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY;
    update.payload = &invalid;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    invalid = availability;
    invalid.fields = ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
    invalid.interactive = 0u;
    invalid.availability_appearance =
        static_cast<ERUI_AvailabilityAppearance>(99u);
    update.payload = &invalid;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    invalid = availability;
    invalid.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    invalid.help = styled_text(L"Hidden payload");
    invalid.interactive = 1u;
    invalid.availability_appearance =
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY;
    update.payload = &invalid;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    invalid = availability;
    invalid.fields = ERUI_ROW_UPDATE_FIELD_INTERACTIVE;
    invalid.interactive = 1u;
    invalid.availability_appearance = ERUI_AVAILABILITY_APPEARANCE_NORMAL;
    update.payload = &invalid;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    invalid = availability;
    invalid.row = submenu_row;
    invalid.fields = ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
    invalid.interactive = 0u;
    invalid.availability_appearance =
        ERUI_AVAILABILITY_APPEARANCE_DISABLED;
    update.payload = &invalid;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc valid_help{};
    valid_help.size = sizeof(valid_help);
    valid_help.row = toggle_row;
    valid_help.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    valid_help.help = styled_text(L"Must roll back");
    ERUI_UpdateDesc valid_help_update{};
    valid_help_update.size = sizeof(valid_help_update);
    valid_help_update.kind = ERUI_UPDATE_KIND_ROW;
    valid_help_update.payload = &valid_help;
    ERUI_UpdateDesc invalid_submenu_update{};
    invalid_submenu_update.size = sizeof(invalid_submenu_update);
    invalid_submenu_update.kind = ERUI_UPDATE_KIND_ROW;
    invalid_submenu_update.payload = &invalid;
    const std::array<ERUI_UpdateDesc, 2> atomic_updates{
        valid_help_update, invalid_submenu_update};
    batch.updates = atomic_updates.data();
    batch.update_count = static_cast<std::uint32_t>(atomic_updates.size());
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    const auto compiled_toggle = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [&](const erui::detail::CompiledRow& row) {
            return row.row == toggle_row;
        });
    ERUI_TEST_CHECK(compiled_toggle != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_toggle->help_id)) == L"Original toggle help");

    erui::detail::reset_row_presentation_state();
}

void verify_api_1_4_row_visibility_admission_edges() {
    const auto submit_visibility = [](
        erui::host::Registry& registry,
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        bool visible) {
        ERUI_RowUpdateDesc row_update{};
        row_update.size = sizeof(row_update);
        row_update.row = row;
        row_update.fields = ERUI_ROW_UPDATE_FIELD_VISIBLE;
        row_update.visible = visible ? 1u : 0u;
        ERUI_UpdateDesc update{};
        update.size = sizeof(update);
        update.kind = ERUI_UPDATE_KIND_ROW;
        update.payload = &row_update;
        ERUI_UpdateBatchDesc batch{};
        batch.size = sizeof(batch);
        batch.updates = &update;
        batch.update_count = 1u;
        batch.update_stride = sizeof(update);
        return registry.commit_updates(provider, &batch);
    };

    erui::detail::reset_row_presentation_state();
    erui::detail::reset_row_visibility_state();

    // Root and built-in pages merge independently registered providers. The
    // zero-page guard must evaluate that merged logical page rather than each
    // provider contribution in isolation.
    {
        erui::host::Registry registry{};
        ERUI_TEST_CHECK(registry.enable_updates_backend());
        ERUI_TEST_CHECK(registry.enable_row_visibility_backend());
        registry.open_registration();

        const auto register_provider = [&](const char* id,
                                           ERUI_ProviderHandle& handle,
                                           ERUI_PageHandle& root) {
            ERUI_ProviderDesc provider{};
            provider.size = sizeof(provider);
            provider.api_version = ERUI_API_VERSION_1_4;
            provider.owner_module = GetModuleHandleW(nullptr);
            provider.provider_id = ascii(id);
            provider.display_name = utf16(L"Merged visibility provider");
            ERUI_TEST_CHECK(registry.register_provider(
                ERUI_API_VERSION_1_4, &provider, &handle, &root) == ERUI_OK);
        };
        const auto add_toggle = [&](ERUI_ProviderHandle provider,
                                    ERUI_PageHandle page,
                                    const wchar_t* label,
                                    ERUI_RowHandle& row) {
            ERUI_ToggleDesc toggle{};
            toggle.size = sizeof(toggle);
            toggle.label = utf16(label);
            toggle.help = utf16(L"Merged-page sentinel");
            toggle.enabled = 1u;
            ERUI_TEST_CHECK(registry.add_toggle(
                provider, page, &toggle, &row) == ERUI_OK);
        };

        ERUI_ProviderHandle first_provider{};
        ERUI_PageHandle first_root{};
        register_provider(
            "visibility.merged.first", first_provider, first_root);
        ERUI_ProviderHandle second_provider{};
        ERUI_PageHandle second_root{};
        register_provider(
            "visibility.merged.second", second_provider, second_root);

        ERUI_PageHandle first_sound{};
        ERUI_PageHandle second_sound{};
        ERUI_TEST_CHECK(registry.get_builtin_page(
            first_provider, ERUI_BUILTIN_PAGE_SOUND, &first_sound) == ERUI_OK);
        ERUI_TEST_CHECK(registry.get_builtin_page(
            second_provider, ERUI_BUILTIN_PAGE_SOUND, &second_sound) ==
            ERUI_OK);

        ERUI_RowHandle first_root_row{};
        ERUI_RowHandle second_root_row{};
        ERUI_RowHandle first_sound_row{};
        ERUI_RowHandle second_sound_row{};
        add_toggle(first_provider, first_root, L"First root", first_root_row);
        add_toggle(
            second_provider, second_root, L"Second root", second_root_row);
        add_toggle(
            first_provider, first_sound, L"First sound", first_sound_row);
        add_toggle(
            second_provider, second_sound, L"Second sound", second_sound_row);
        ERUI_TEST_CHECK(registry.commit_provider(first_provider) == ERUI_OK);
        ERUI_TEST_CHECK(registry.commit_provider(second_provider) == ERUI_OK);

        ERUI_TEST_CHECK(submit_visibility(
            registry, first_provider, first_root_row, false) == ERUI_OK);
        ERUI_TEST_CHECK(submit_visibility(
            registry, second_provider, second_root_row, false) ==
            ERUI_INVALID_ARGUMENT);
        ERUI_TEST_CHECK(submit_visibility(
            registry, first_provider, first_sound_row, false) == ERUI_OK);
        ERUI_TEST_CHECK(submit_visibility(
            registry, second_provider, second_sound_row, false) ==
            ERUI_INVALID_ARGUMENT);
    }

    erui::detail::reset_row_presentation_state();
    erui::detail::reset_row_visibility_state();

    // Construction-disabled Button and ColorPicker rows are absent from the
    // compiled identity table. Disabled Toggles and Sliders retain a native
    // row and therefore remain valid visibility targets. Submenu visibility
    // is rejected independently of its construction-time state.
    {
        erui::host::Registry registry{};
        ERUI_TEST_CHECK(registry.enable_updates_backend());
        ERUI_TEST_CHECK(registry.enable_row_visibility_backend());
        registry.open_registration();

        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_4;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii("visibility.construction-disabled");
        provider.display_name = utf16(L"Construction-disabled visibility");
        ERUI_ProviderHandle provider_handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_4, &provider, &provider_handle, &root) ==
            ERUI_OK);

        ERUI_ToggleDesc toggle{};
        toggle.size = sizeof(toggle);
        toggle.label = utf16(L"Sentinel");
        toggle.help = utf16(L"Keeps the page nonempty");
        toggle.enabled = 1u;
        ERUI_RowHandle sentinel{};
        ERUI_TEST_CHECK(registry.add_toggle(
            provider_handle, root, &toggle, &sentinel) == ERUI_OK);
        toggle.label = utf16(L"Disabled toggle");
        toggle.enabled = 0u;
        ERUI_RowHandle disabled_toggle{};
        ERUI_TEST_CHECK(registry.add_toggle(
            provider_handle, root, &toggle, &disabled_toggle) == ERUI_OK);

        ERUI_SliderDesc slider{};
        slider.size = sizeof(slider);
        slider.label = utf16(L"Disabled slider");
        slider.help = utf16(L"Still materialized");
        slider.minimum = 0u;
        slider.maximum = 10u;
        slider.step = 1u;
        slider.enabled = 0u;
        ERUI_RowHandle disabled_slider{};
        ERUI_TEST_CHECK(registry.add_slider(
            provider_handle, root, &slider, &disabled_slider) == ERUI_OK);

        ERUI_ButtonDesc button{};
        button.size = sizeof(button);
        button.label = utf16(L"Disabled button");
        button.help = utf16(L"Omitted");
        button.callback = &pressed;
        button.enabled = 0u;
        ERUI_RowHandle disabled_button{};
        ERUI_TEST_CHECK(registry.add_button(
            provider_handle, root, &button, &disabled_button) == ERUI_OK);

        ERUI_ColorPickerDesc color{};
        color.size = sizeof(color);
        color.label = utf16(L"Disabled color picker");
        color.help = utf16(L"Omitted");
        color.initial_value = ERUI_Color{64u, 96u, 128u, 0u};
        color.enabled = 0u;
        ERUI_RowHandle disabled_color{};
        ERUI_TEST_CHECK(registry.add_color_picker(
            provider_handle, root, &color, &disabled_color) == ERUI_OK);

        ERUI_SubmenuDesc submenu{};
        submenu.size = sizeof(submenu);
        submenu.label = utf16(L"Disabled submenu");
        submenu.help = utf16(L"Omitted");
        submenu.page_title = utf16(L"Unreachable child");
        submenu.enabled = 0u;
        ERUI_PageHandle disabled_child{};
        ERUI_RowHandle disabled_submenu{};
        ERUI_TEST_CHECK(registry.add_submenu(
            provider_handle, root, &submenu,
            &disabled_child, &disabled_submenu) == ERUI_OK);
        ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

        ERUI_TEST_CHECK(submit_visibility(
            registry, provider_handle, disabled_button, false) ==
            ERUI_INVALID_ARGUMENT);
        ERUI_TEST_CHECK(submit_visibility(
            registry, provider_handle, disabled_color, false) ==
            ERUI_INVALID_ARGUMENT);
        ERUI_TEST_CHECK(submit_visibility(
            registry, provider_handle, disabled_submenu, false) ==
            ERUI_INVALID_ARGUMENT);
        ERUI_TEST_CHECK(submit_visibility(
            registry, provider_handle, disabled_toggle, false) == ERUI_OK);
        ERUI_TEST_CHECK(submit_visibility(
            registry, provider_handle, disabled_slider, false) == ERUI_OK);
    }

    erui::detail::reset_row_presentation_state();
    erui::detail::reset_row_visibility_state();
}

void verify_api_1_4_row_visibility_contract() {
    erui::detail::reset_row_presentation_state();
    erui::detail::reset_row_visibility_state();

    // Visibility has a narrower exact-build lifecycle than ordinary text
    // updates and therefore remains independently capability-gated.
    erui::host::Registry unavailable{};
    ERUI_TEST_CHECK(unavailable.enable_updates_backend());
    unavailable.open_registration();
    ERUI_ProviderDesc unavailable_provider{};
    unavailable_provider.size = sizeof(unavailable_provider);
    unavailable_provider.api_version = ERUI_API_VERSION_1_4;
    unavailable_provider.owner_module = GetModuleHandleW(nullptr);
    unavailable_provider.provider_id = ascii("visibility.unavailable");
    unavailable_provider.display_name = utf16(L"Visibility unavailable");
    ERUI_ProviderHandle unavailable_handle{};
    ERUI_PageHandle unavailable_root{};
    ERUI_TEST_CHECK(unavailable.register_provider(
        ERUI_API_VERSION_1_4, &unavailable_provider,
        &unavailable_handle, &unavailable_root) == ERUI_OK);
    ERUI_ToggleDesc unavailable_toggle{};
    unavailable_toggle.size = sizeof(unavailable_toggle);
    unavailable_toggle.label = utf16(L"Target");
    unavailable_toggle.help = utf16(L"Help");
    unavailable_toggle.enabled = 1u;
    ERUI_RowHandle unavailable_row{};
    ERUI_TEST_CHECK(unavailable.add_toggle(
        unavailable_handle, unavailable_root, &unavailable_toggle,
        &unavailable_row) == ERUI_OK);
    ERUI_TEST_CHECK(unavailable.commit_provider(unavailable_handle) == ERUI_OK);
    ERUI_RowUpdateDesc unavailable_visibility{};
    unavailable_visibility.size = sizeof(unavailable_visibility);
    unavailable_visibility.row = unavailable_row;
    unavailable_visibility.fields = ERUI_ROW_UPDATE_FIELD_VISIBLE;
    unavailable_visibility.visible = 0u;
    ERUI_UpdateDesc unavailable_update{};
    unavailable_update.size = sizeof(unavailable_update);
    unavailable_update.kind = ERUI_UPDATE_KIND_ROW;
    unavailable_update.payload = &unavailable_visibility;
    ERUI_UpdateBatchDesc unavailable_batch{};
    unavailable_batch.size = sizeof(unavailable_batch);
    unavailable_batch.updates = &unavailable_update;
    unavailable_batch.update_count = 1u;
    unavailable_batch.update_stride = sizeof(unavailable_update);
    ERUI_TEST_CHECK(unavailable.commit_updates(
        unavailable_handle, &unavailable_batch) == ERUI_NOT_SUPPORTED);

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(!registry.enable_row_visibility_backend());
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    ERUI_TEST_CHECK(registry.enable_row_visibility_backend());
    ERUI_TEST_CHECK(registry.enable_custom_tabs_backend());
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("visibility.contract");
    provider.display_name = utf16(L"Visibility contract");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4, &provider, &provider_handle, &root) == ERUI_OK);

    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16(L"Root toggle");
    toggle.help = utf16(L"Root original help");
    toggle.enabled = 1u;
    ERUI_RowHandle root_toggle{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, root, &toggle, &root_toggle) == ERUI_OK);

    ERUI_SubmenuDesc submenu{};
    submenu.size = sizeof(submenu);
    submenu.label = utf16(L"Child row");
    submenu.help = utf16(L"Open child");
    submenu.page_title = utf16(L"Child page");
    submenu.enabled = 1u;
    ERUI_PageHandle child_page{};
    ERUI_RowHandle submenu_row{};
    ERUI_TEST_CHECK(registry.add_submenu(
        provider_handle, root, &submenu, &child_page, &submenu_row) ==
        ERUI_OK);

    unsigned presses{};
    ERUI_ButtonDesc button{};
    button.size = sizeof(button);
    button.label = utf16(L"Child first");
    button.help = utf16(L"First help");
    button.callback = &pressed;
    button.user_data = &presses;
    button.enabled = 1u;
    ERUI_RowHandle child_first{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle, child_page, &button, &child_first) == ERUI_OK);
    button.label = utf16(L"Child second");
    button.help = utf16(L"Second original help");
    ERUI_RowHandle child_second{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle, child_page, &button, &child_second) == ERUI_OK);

    button.label = utf16(L"Omitted button");
    button.help = utf16(L"Construction-disabled");
    button.enabled = 0u;
    ERUI_RowHandle omitted_button{};
    ERUI_TEST_CHECK(registry.add_button(
        provider_handle, root, &button, &omitted_button) == ERUI_OK);

    ERUI_TabDesc tab{};
    tab.size = sizeof(tab);
    tab.tab_id = ascii("reactive");
    tab.title = utf16(L"Reactive tab");
    ERUI_PageHandle tab_page{};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_handle, &tab, &tab_page) == ERUI_OK);
    toggle.label = utf16(L"Tab first");
    ERUI_RowHandle tab_first{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, tab_page, &toggle, &tab_first) == ERUI_OK);
    toggle.label = utf16(L"Tab sentinel");
    ERUI_RowHandle tab_second{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, tab_page, &toggle, &tab_second) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    ERUI_UpdateDesc update{};
    update.size = sizeof(update);
    update.kind = ERUI_UPDATE_KIND_ROW;
    ERUI_UpdateBatchDesc batch{};
    batch.size = sizeof(batch);
    batch.updates = &update;
    batch.update_count = 1u;
    batch.update_stride = sizeof(update);

    ERUI_RowUpdateDesc malformed{};
    malformed.size = sizeof(malformed);
    malformed.row = root_toggle;
    malformed.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    malformed.help = styled_text(L"Canonical check");
    malformed.visible = 1u;
    update.payload = &malformed;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);
    malformed = {};
    malformed.size = sizeof(malformed);
    malformed.row = root_toggle;
    malformed.fields = ERUI_ROW_UPDATE_FIELD_VISIBLE;
    malformed.visible = 2u;
    update.payload = &malformed;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc omitted{};
    omitted.size = sizeof(omitted);
    omitted.row = omitted_button;
    omitted.fields = ERUI_ROW_UPDATE_FIELD_VISIBLE;
    omitted.visible = 0u;
    update.payload = &omitted;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    const auto visibility_update = [](ERUI_RowHandle row, bool visible) {
        ERUI_RowUpdateDesc result{};
        result.size = sizeof(result);
        result.row = row;
        result.fields = ERUI_ROW_UPDATE_FIELD_VISIBLE;
        result.visible = visible ? 1u : 0u;
        return result;
    };

    ERUI_RowUpdateDesc hide_root = visibility_update(root_toggle, false);
    ERUI_RowUpdateDesc hide_child = visibility_update(child_first, false);
    ERUI_RowUpdateDesc hide_tab = visibility_update(tab_first, false);
    std::array<ERUI_UpdateDesc, 3> initial_updates{};
    const std::array<ERUI_RowUpdateDesc*, 3> initial_payloads{
        &hide_root, &hide_child, &hide_tab};
    for (std::size_t index = 0; index < initial_updates.size(); ++index) {
        initial_updates[index].size = sizeof(ERUI_UpdateDesc);
        initial_updates[index].kind = ERUI_UPDATE_KIND_ROW;
        initial_updates[index].payload = initial_payloads[index];
    }
    batch.updates = initial_updates.data();
    batch.update_count = static_cast<std::uint32_t>(initial_updates.size());
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);

    // Hiding the remaining row in any logical page is rejected atomically.
    // Pair one attempt with a valid help edit to prove that the companion
    // presentation snapshot is not published either.
    ERUI_RowUpdateDesc rejected_help{};
    rejected_help.size = sizeof(rejected_help);
    rejected_help.row = child_second;
    rejected_help.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    rejected_help.help = styled_text(L"Must not publish");
    ERUI_RowUpdateDesc hide_last_child =
        visibility_update(child_second, false);
    std::array<ERUI_UpdateDesc, 2> rejected_updates{};
    rejected_updates[0] = {
        sizeof(ERUI_UpdateDesc), ERUI_UPDATE_KIND_ROW,
        &rejected_help, {0, 0}};
    rejected_updates[1] = {
        sizeof(ERUI_UpdateDesc), ERUI_UPDATE_KIND_ROW,
        &hide_last_child, {0, 0}};
    batch.updates = rejected_updates.data();
    batch.update_count = static_cast<std::uint32_t>(rejected_updates.size());
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc hide_last_root =
        visibility_update(submenu_row, false);
    update.payload = &hide_last_root;
    batch.updates = &update;
    batch.update_count = 1u;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_RowUpdateDesc hide_last_tab = visibility_update(tab_second, false);
    update.payload = &hide_last_tab;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(*compiled));
    ERUI_TEST_CHECK(erui::detail::configure_row_visibility_state(*compiled));
    erui::detail::synchronize_pending_row_updates();

    const auto find_row = [&](ERUI_RowHandle handle)
        -> std::pair<std::size_t, const erui::detail::CompiledRow*> {
        for (std::size_t page_index = 0;
             page_index < compiled->pages.size(); ++page_index) {
            for (const erui::detail::CompiledRow& candidate :
                 compiled->pages[page_index].rows) {
                if (candidate.row == handle) {
                    return {page_index, &candidate};
                }
            }
        }
        return {erui::detail::invalid_compiled_index, nullptr};
    };
    const auto [root_page_index, compiled_root] = find_row(root_toggle);
    const auto [child_page_index, compiled_child] = find_row(child_second);
    const auto [tab_page_index, compiled_tab] = find_row(tab_second);
    ERUI_TEST_CHECK(compiled_root && compiled_child && compiled_tab);
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(root_page_index)
            .visible_row_count == 1u);
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(child_page_index)
            .visible_row_count == 1u);
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(tab_page_index)
            .visible_row_count == 1u);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_child->help_id)) == L"Second original help");

    // A successful mixed batch stays invisible to readers until the shared
    // UI-boundary publication, then advances both stores together.
    ERUI_RowUpdateDesc show_child = visibility_update(child_first, true);
    ERUI_RowUpdateDesc change_help{};
    change_help.size = sizeof(change_help);
    change_help.row = child_second;
    change_help.fields = ERUI_ROW_UPDATE_FIELD_HELP |
        ERUI_ROW_UPDATE_FIELD_VISIBLE;
    change_help.help = styled_text(L"Published together");
    change_help.visible = 0u;
    std::array<ERUI_UpdateDesc, 2> mixed_updates{};
    mixed_updates[0] = {
        sizeof(ERUI_UpdateDesc), ERUI_UPDATE_KIND_ROW,
        &show_child, {0, 0}};
    mixed_updates[1] = {
        sizeof(ERUI_UpdateDesc), ERUI_UPDATE_KIND_ROW,
        &change_help, {0, 0}};
    batch.updates = mixed_updates.data();
    batch.update_count = static_cast<std::uint32_t>(mixed_updates.size());
    const auto before_projection =
        erui::detail::row_visibility_projection(child_page_index);
    const auto before_presentation = erui::detail::row_presentation_view(
        compiled_child->presentation_slot);
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(child_page_index).revision ==
        before_projection.revision);
    ERUI_TEST_CHECK(erui::detail::row_presentation_view(
        compiled_child->presentation_slot).revision ==
        before_presentation.revision);
    ERUI_TEST_CHECK(
        !erui::detail::synchronize_pending_row_updates_if_visibility_idle());
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(child_page_index).revision ==
        before_projection.revision);
    ERUI_TEST_CHECK(erui::detail::row_presentation_view(
        compiled_child->presentation_slot).revision ==
        before_presentation.revision);
    erui::detail::synchronize_pending_row_updates();
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(child_page_index)
            .visible_row_count == 1u);
    ERUI_TEST_CHECK(std::wstring(erui::detail::row_presentation_view(
        compiled_child->presentation_slot).help) == L"Published together");

    // Showing the ordinary root row makes the page nonempty independently,
    // proving that an enabled Submenu is rejected because its navigation
    // lifecycle is not admitted—not merely by the zero-row guard.
    ERUI_RowUpdateDesc show_root = visibility_update(root_toggle, true);
    update.payload = &show_root;
    batch.updates = &update;
    batch.update_count = 1u;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_OK);
    erui::detail::synchronize_pending_row_updates();
    ERUI_RowUpdateDesc hide_submenu = visibility_update(submenu_row, false);
    update.payload = &hide_submenu;
    ERUI_TEST_CHECK(registry.commit_updates(provider_handle, &batch) ==
        ERUI_INVALID_ARGUMENT);
    erui::detail::synchronize_pending_row_updates();
    ERUI_TEST_CHECK(
        erui::detail::row_visibility_projection(root_page_index)
            .visible_row_count == 2u);

    erui::detail::reset_row_presentation_state();
    erui::detail::reset_row_visibility_state();
}

void verify_api_1_4_submenu_update_contract() {
    erui::detail::reset_row_presentation_state();
    erui::detail::reset_submenu_update_state();

    const auto submit = [](
        erui::host::Registry& registry,
        ERUI_ProviderHandle provider,
        const ERUI_UpdateDesc* updates,
        std::uint32_t count) {
        ERUI_UpdateBatchDesc batch{};
        batch.size = sizeof(batch);
        batch.updates = updates;
        batch.update_count = count;
        batch.update_stride = sizeof(ERUI_UpdateDesc);
        return registry.commit_updates(provider, &batch);
    };
    const auto wrap_submenu = [](const ERUI_SubmenuUpdateDesc& submenu) {
        ERUI_UpdateDesc update{};
        update.size = sizeof(update);
        update.kind = ERUI_UPDATE_KIND_SUBMENU;
        update.payload = &submenu;
        return update;
    };

    // API 1.4 text updates remain separately capability-gated from the
    // ordinary update transport.
    {
        erui::host::Registry registry{};
        ERUI_TEST_CHECK(registry.enable_updates_backend());
        registry.open_registration();

        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_4;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii("submenu-updates.unavailable");
        provider.display_name = utf16(L"Unavailable submenu updates");
        ERUI_ProviderHandle provider_handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(registry.register_provider(
            ERUI_API_VERSION_1_4,
            &provider,
            &provider_handle,
            &root) == ERUI_OK);
        ERUI_SubmenuDesc submenu{};
        submenu.size = sizeof(submenu);
        submenu.label = utf16(L"Submenu");
        submenu.help = utf16(L"Open child");
        submenu.page_title = utf16(L"Child");
        submenu.enabled = 1u;
        ERUI_PageHandle child{};
        ERUI_RowHandle opener{};
        ERUI_TEST_CHECK(registry.add_submenu(
            provider_handle, root, &submenu, &child, &opener) == ERUI_OK);
        ERUI_ToggleDesc sentinel{};
        sentinel.size = sizeof(sentinel);
        sentinel.label = utf16(L"Sentinel");
        sentinel.help = utf16(L"Keeps the child page non-empty");
        sentinel.enabled = 1u;
        ERUI_TEST_CHECK(registry.add_toggle(
            provider_handle, child, &sentinel, nullptr) == ERUI_OK);
        ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

        ERUI_SubmenuUpdateDesc submenu_update{};
        submenu_update.size = sizeof(submenu_update);
        submenu_update.row = opener;
        submenu_update.page = child;
        submenu_update.fields = ERUI_SUBMENU_UPDATE_FIELD_LABEL;
        submenu_update.label = styled_text(L"Rejected");
        const ERUI_UpdateDesc update = wrap_submenu(submenu_update);
        ERUI_TEST_CHECK(submit(
            registry, provider_handle, &update, 1u) == ERUI_NOT_SUPPORTED);
    }

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_custom_tabs_backend());
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    ERUI_TEST_CHECK(registry.enable_submenu_updates_backend());
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("submenu-updates.contract");
    provider.display_name = utf16(L"Submenu update contract");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    ERUI_ToggleDesc rollback_toggle{};
    rollback_toggle.size = sizeof(rollback_toggle);
    rollback_toggle.label = utf16(L"Rollback sentinel");
    rollback_toggle.help = utf16(L"Rollback original help");
    rollback_toggle.enabled = 1u;
    ERUI_RowHandle rollback_row{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, root, &rollback_toggle, &rollback_row) == ERUI_OK);

    const auto add_submenu = [&](
        ERUI_PageHandle parent,
        const wchar_t* label,
        const wchar_t* title,
        ERUI_PageHandle& child,
        ERUI_RowHandle& opener) {
        ERUI_SubmenuDesc description{};
        description.size = sizeof(description);
        description.label = utf16(label);
        description.help = utf16(L"Original opener help");
        description.page_title = utf16(title);
        description.page_help = utf16(L"Original child help");
        description.enabled = 1u;
        ERUI_TEST_CHECK(registry.add_submenu(
            provider_handle,
            parent,
            &description,
            &child,
            &opener) == ERUI_OK);
    };
    const auto add_child_toggle = [&](ERUI_PageHandle page) {
        ERUI_ToggleDesc toggle{};
        toggle.size = sizeof(toggle);
        toggle.label = utf16(L"Child row");
        toggle.help = utf16(L"Child row help");
        toggle.enabled = 1u;
        ERUI_TEST_CHECK(registry.add_toggle(
            provider_handle, page, &toggle, nullptr) == ERUI_OK);
    };

    ERUI_PageHandle child{};
    ERUI_RowHandle opener{};
    add_submenu(root, L"Original opener", L"Original child", child, opener);
    for (unsigned index = 0;
         index < erui::detail::native_subpage_capacity + 1u;
         ++index) {
        add_child_toggle(child);
    }

    ERUI_PageHandle other_child{};
    ERUI_RowHandle other_opener{};
    add_submenu(
        root, L"Other opener", L"Other child", other_child, other_opener);
    add_child_toggle(other_child);

    ERUI_PageHandle formatter_child{};
    ERUI_RowHandle formatter_opener{};
    add_submenu(
        root,
        L"Formatter opener",
        L"Formatter child",
        formatter_child,
        formatter_opener);
    add_child_toggle(formatter_child);
    FormatterObservation formatter_observation{};
    ERUI_PagePresentationDesc formatter_presentation{};
    formatter_presentation.size = sizeof(formatter_presentation);
    formatter_presentation.page_title = utf16(L"Formatter base");
    formatter_presentation.formatter = &format_page_title;
    formatter_presentation.user_data = &formatter_observation;
    ERUI_TEST_CHECK(registry.set_page_presentation(
        provider_handle,
        formatter_child,
        &formatter_presentation) == ERUI_OK);

    ERUI_PageHandle sound_page{};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        provider_handle, ERUI_BUILTIN_PAGE_SOUND, &sound_page) == ERUI_OK);
    ERUI_ToggleDesc sound_sentinel = rollback_toggle;
    sound_sentinel.label = utf16(L"Sound sentinel");
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, sound_page, &sound_sentinel, nullptr) == ERUI_OK);

    ERUI_TabDesc tab{};
    tab.size = sizeof(tab);
    tab.tab_id = ascii("submenu-update-tab");
    tab.title = utf16(L"Update Tab");
    ERUI_PageHandle tab_page{};
    ERUI_TEST_CHECK(registry.add_tab(
        provider_handle, &tab, &tab_page) == ERUI_OK);
    ERUI_ToggleDesc tab_sentinel = rollback_toggle;
    tab_sentinel.label = utf16(L"Tab sentinel");
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, tab_page, &tab_sentinel, nullptr) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    ERUI_ProviderDesc foreign_provider = provider;
    foreign_provider.provider_id = ascii("submenu-updates.foreign");
    foreign_provider.display_name = utf16(L"Foreign submenu provider");
    ERUI_ProviderHandle foreign_handle{};
    ERUI_PageHandle foreign_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &foreign_provider,
        &foreign_handle,
        &foreign_root) == ERUI_OK);
    ERUI_SubmenuDesc foreign_submenu{};
    foreign_submenu.size = sizeof(foreign_submenu);
    foreign_submenu.label = utf16(L"Foreign opener");
    foreign_submenu.help = utf16(L"Foreign opener help");
    foreign_submenu.page_title = utf16(L"Foreign child");
    foreign_submenu.enabled = 1u;
    ERUI_PageHandle foreign_child{};
    ERUI_RowHandle foreign_opener{};
    ERUI_TEST_CHECK(registry.add_submenu(
        foreign_handle,
        foreign_root,
        &foreign_submenu,
        &foreign_child,
        &foreign_opener) == ERUI_OK);
    ERUI_ToggleDesc foreign_sentinel = rollback_toggle;
    foreign_sentinel.label = utf16(L"Foreign sentinel");
    ERUI_TEST_CHECK(registry.add_toggle(
        foreign_handle,
        foreign_child,
        &foreign_sentinel,
        nullptr) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(foreign_handle) == ERUI_OK);

    ERUI_ProviderDesc stale_provider = provider;
    stale_provider.provider_id = ascii("submenu-updates.stale");
    stale_provider.display_name = utf16(L"Aborted submenu provider");
    ERUI_ProviderHandle stale_provider_handle{};
    ERUI_PageHandle stale_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &stale_provider,
        &stale_provider_handle,
        &stale_root) == ERUI_OK);
    ERUI_SubmenuDesc stale_submenu = foreign_submenu;
    stale_submenu.label = utf16(L"Stale opener");
    ERUI_PageHandle stale_child{};
    ERUI_RowHandle stale_opener{};
    ERUI_TEST_CHECK(registry.add_submenu(
        stale_provider_handle,
        stale_root,
        &stale_submenu,
        &stale_child,
        &stale_opener) == ERUI_OK);
    ERUI_TEST_CHECK(registry.abort_provider(stale_provider_handle) == ERUI_OK);

    ERUI_SubmenuUpdateDesc base{};
    base.size = sizeof(base);
    base.row = opener;
    base.page = child;
    base.fields = ERUI_SUBMENU_UPDATE_FIELD_LABEL;
    base.label = styled_text(L"Candidate opener");

    unsigned expectation_index{};
    const auto expect = [&](ERUI_SubmenuUpdateDesc candidate,
                            ERUI_Result expected) {
        ++expectation_index;
        const ERUI_UpdateDesc update = wrap_submenu(candidate);
        const ERUI_Result result = submit(
            registry, provider_handle, &update, 1u);
        if (result != expected) {
            std::fprintf(
                stderr,
                "submenu expectation %u: expected %d, got %d\n",
                expectation_index,
                static_cast<int>(expected),
                static_cast<int>(result));
        }
        ERUI_TEST_CHECK(result == expected);
    };

    ERUI_SubmenuUpdateDesc invalid = base;
    invalid.page = other_child;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.row = rollback_row;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    for (const ERUI_PageHandle invalid_page :
         std::array{root, tab_page, sound_page}) {
        invalid = base;
        invalid.page = invalid_page;
        expect(invalid, ERUI_INVALID_ARGUMENT);
    }
    invalid = base;
    invalid.row = foreign_opener;
    invalid.page = foreign_child;
    expect(invalid, ERUI_INVALID_HANDLE);
    invalid = base;
    invalid.row = stale_opener;
    invalid.page = stale_child;
    expect(invalid, ERUI_INVALID_HANDLE);
    invalid = base;
    invalid.row = UINT64_C(0xF0F0F0F0F0F0F0F0);
    invalid.page = UINT64_C(0xE0E0E0E0E0E0E0E0);
    expect(invalid, ERUI_INVALID_HANDLE);

    invalid = base;
    invalid.fields = ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE;
    invalid.label = styled_text(L"Noncanonical hidden label");
    invalid.page_title = utf16(L"New child");
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.page_title = utf16(L"Noncanonical hidden title");
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.size = sizeof(invalid) - 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.flags = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.reserved0 = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = 0u;
    invalid.label = {};
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE;
    invalid.label = {};
    invalid.page_title = utf16(L"");
    expect(invalid, ERUI_INVALID_ARGUMENT);
    ERUI_TextStyleSpan malformed_span = text_style_span(
        0u, 4u, ERUI_TEXT_STYLE_UNDERLINE);
    malformed_span.reserved[0] = 1u;
    invalid = base;
    invalid.label = styled_text(L"Bad!", &malformed_span, 1u);
    expect(invalid, ERUI_INVALID_ARGUMENT);

    invalid = base;
    invalid.row = formatter_opener;
    invalid.page = formatter_child;
    invalid.fields = ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE;
    invalid.label = {};
    invalid.page_title = utf16(L"Formatter replacement");
    expect(invalid, ERUI_NOT_SUPPORTED);

    const ERUI_UpdateDesc duplicate_update = wrap_submenu(base);
    const std::array duplicate_updates{duplicate_update, duplicate_update};
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        duplicate_updates.data(),
        static_cast<std::uint32_t>(duplicate_updates.size())) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_RowUpdateDesc opener_row_update{};
    opener_row_update.size = sizeof(opener_row_update);
    opener_row_update.row = opener;
    opener_row_update.fields = ERUI_ROW_UPDATE_FIELD_LABEL;
    opener_row_update.label = styled_text(L"Cross-kind duplicate");
    ERUI_UpdateDesc opener_update{};
    opener_update.size = sizeof(opener_update);
    opener_update.kind = ERUI_UPDATE_KIND_ROW;
    opener_update.payload = &opener_row_update;
    const std::array cross_kind_updates{opener_update, duplicate_update};
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        cross_kind_updates.data(),
        static_cast<std::uint32_t>(cross_kind_updates.size())) ==
        ERUI_INVALID_ARGUMENT);

    // A valid ordinary row update preceding an invalid submenu relationship
    // must not leak through the failed atomic batch.
    ERUI_RowUpdateDesc rollback_update{};
    rollback_update.size = sizeof(rollback_update);
    rollback_update.row = rollback_row;
    rollback_update.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    rollback_update.help = styled_text(L"Must not publish");
    ERUI_UpdateDesc rollback_item{};
    rollback_item.size = sizeof(rollback_item);
    rollback_item.kind = ERUI_UPDATE_KIND_ROW;
    rollback_item.payload = &rollback_update;
    ERUI_SubmenuUpdateDesc wrong_relationship = base;
    wrong_relationship.page = other_child;
    const ERUI_UpdateDesc wrong_item = wrap_submenu(wrong_relationship);
    const std::array atomic_updates{rollback_item, wrong_item};
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        atomic_updates.data(),
        static_cast<std::uint32_t>(atomic_updates.size())) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_SubmenuUpdateDesc all_fields{};
    all_fields.size = sizeof(all_fields);
    all_fields.row = opener;
    all_fields.page = child;
    all_fields.fields = ERUI_SUBMENU_UPDATE_FIELD_LABEL |
        ERUI_SUBMENU_UPDATE_FIELD_HELP |
        ERUI_SUBMENU_UPDATE_FIELD_CAPTION |
        ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE |
        ERUI_SUBMENU_UPDATE_FIELD_PAGE_HELP;
    all_fields.label = styled_text(L"Updated <opener>");
    all_fields.help = styled_text(L"Updated opener & help");
    all_fields.caption = styled_text(L"Open >");
    all_fields.page_title = utf16(L"Updated child title");
    all_fields.page_help = utf16(L"Updated child help");
    const ERUI_UpdateDesc all_fields_update = wrap_submenu(all_fields);
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &all_fields_update, 1u) == ERUI_OK);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);

    const auto compiled_opener = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [&](const erui::detail::CompiledRow& row) {
            return row.provider == provider_handle && row.row == opener;
        });
    ERUI_TEST_CHECK(compiled_opener != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_opener->label_id)) == L"Updated &lt;opener&gt;");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_opener->help_id)) == L"Updated opener &amp; help");

    const auto compiled_rollback = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [&](const erui::detail::CompiledRow& row) {
            return row.provider == provider_handle && row.row == rollback_row;
        });
    ERUI_TEST_CHECK(compiled_rollback != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_rollback->help_id)) == L"Rollback original help");

    const auto compiled_child = std::find_if(
        compiled->pages.begin(),
        compiled->pages.end(),
        [&](const erui::detail::CompiledPage& page) {
            return page.provider == provider_handle && page.page == child;
        });
    ERUI_TEST_CHECK(compiled_child != compiled->pages.end());
    const std::size_t child_index = static_cast<std::size_t>(
        std::distance(compiled->pages.begin(), compiled_child));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_child->title_id)) == L"Updated child title");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_child->help_id)) == L"Updated child help");
    ERUI_TEST_CHECK(child_index < compiled->page_plans.size());
    const std::size_t slice_count =
        compiled->page_plans[child_index].slices.size();
    ERUI_TEST_CHECK(slice_count > 1u);

    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(
        *compiled));
    ERUI_TEST_CHECK(erui::detail::configure_submenu_update_state(
        *compiled));
    erui::detail::synchronize_pending_row_presentations();
    erui::detail::synchronize_pending_submenu_updates();
    const erui::detail::RowPresentationView opener_presentation =
        erui::detail::row_presentation_view(
            compiled_opener->presentation_slot);
    ERUI_TEST_CHECK(opener_presentation);
    ERUI_TEST_CHECK(std::wstring(opener_presentation.label) ==
        L"Updated &lt;opener&gt;");
    ERUI_TEST_CHECK(std::wstring(opener_presentation.help) ==
        L"Updated opener &amp; help");
    ERUI_TEST_CHECK(opener_presentation.caption != nullptr);
    ERUI_TEST_CHECK(std::wstring(opener_presentation.caption) ==
        L"Open &gt;");

    std::array<wchar_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY> title_buffer{};
    std::uint64_t title_revision{};
    ERUI_TEST_CHECK(erui::detail::format_submenu_page_title(
        compiled_child->submenu_update_slot,
        0u,
        slice_count,
        title_buffer.data(),
        title_buffer.size(),
        title_revision));
    ERUI_TEST_CHECK(title_revision != 0u);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Updated child title (1/" + std::to_wstring(slice_count) + L")");

    ERUI_SubmenuUpdateDesc live_title{};
    live_title.size = sizeof(live_title);
    live_title.row = opener;
    live_title.page = child;
    live_title.fields = ERUI_SUBMENU_UPDATE_FIELD_PAGE_TITLE;
    live_title.page_title = utf16(L"Live child title");
    const ERUI_UpdateDesc live_title_update = wrap_submenu(live_title);
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &live_title_update, 1u) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::submenu_updates_pending());
    erui::detail::synchronize_pending_submenu_updates();
    std::uint64_t live_revision{};
    ERUI_TEST_CHECK(erui::detail::format_submenu_page_title(
        compiled_child->submenu_update_slot,
        slice_count - 1u,
        slice_count,
        title_buffer.data(),
        title_buffer.size(),
        live_revision));
    ERUI_TEST_CHECK(live_revision > title_revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Live child title (" + std::to_wstring(slice_count) + L"/" +
            std::to_wstring(slice_count) + L")");

    erui::detail::reset_row_presentation_state();
    erui::detail::reset_submenu_update_state();
}

void verify_api_1_4_tab_update_contract() {
    erui::detail::reset_row_presentation_state();
    erui::detail::reset_submenu_update_state();
    erui::detail::reset_tab_update_state();

    const auto submit = [](
        erui::host::Registry& registry,
        ERUI_ProviderHandle provider,
        const ERUI_UpdateDesc* updates,
        std::uint32_t count) {
        ERUI_UpdateBatchDesc batch{};
        batch.size = sizeof(batch);
        batch.updates = updates;
        batch.update_count = count;
        batch.update_stride = sizeof(ERUI_UpdateDesc);
        return registry.commit_updates(provider, &batch);
    };
    const auto wrap_tab = [](const ERUI_TabUpdateDesc& tab) {
        ERUI_UpdateDesc update{};
        update.size = sizeof(update);
        update.kind = ERUI_UPDATE_KIND_TAB;
        update.payload = &tab;
        return update;
    };

    // The rich-tab surface depends on all three foundations it composes:
    // custom tabs, the update transport, and live logical-page titles.
    {
        erui::host::Registry dependencies{};
        ERUI_TEST_CHECK(!dependencies.enable_tab_updates_backend());
        ERUI_TEST_CHECK(dependencies.enable_custom_tabs_backend());
        ERUI_TEST_CHECK(!dependencies.enable_tab_updates_backend());
        ERUI_TEST_CHECK(dependencies.enable_updates_backend());
        ERUI_TEST_CHECK(!dependencies.enable_tab_updates_backend());
        ERUI_TEST_CHECK(dependencies.enable_submenu_updates_backend());
        ERUI_TEST_CHECK(dependencies.enable_tab_updates_backend());
        ERUI_TEST_CHECK(dependencies.tab_updates_backend_available());
        dependencies.open_registration();
        ERUI_TEST_CHECK(!dependencies.enable_tab_updates_backend());
    }

    // A host may expose ordinary API 1.4 updates without admitting tab
    // updates. Kind 3 must then fail before it mutates the provider model.
    {
        erui::host::Registry unavailable{};
        ERUI_TEST_CHECK(unavailable.enable_custom_tabs_backend());
        ERUI_TEST_CHECK(unavailable.enable_updates_backend());
        ERUI_TEST_CHECK(unavailable.enable_submenu_updates_backend());
        unavailable.open_registration();

        ERUI_ProviderDesc provider{};
        provider.size = sizeof(provider);
        provider.api_version = ERUI_API_VERSION_1_4;
        provider.owner_module = GetModuleHandleW(nullptr);
        provider.provider_id = ascii("tab-updates.unavailable");
        provider.display_name = utf16(L"Unavailable tab updates");
        ERUI_ProviderHandle provider_handle{};
        ERUI_PageHandle root{};
        ERUI_TEST_CHECK(unavailable.register_provider(
            ERUI_API_VERSION_1_4,
            &provider,
            &provider_handle,
            &root) == ERUI_OK);
        ERUI_TabDesc description{};
        description.size = sizeof(description);
        description.tab_id = ascii("target");
        description.title = utf16(L"Target");
        ERUI_PageHandle tab_page{};
        ERUI_TEST_CHECK(unavailable.add_tab(
            provider_handle, &description, &tab_page) == ERUI_OK);
        ERUI_ToggleDesc sentinel{};
        sentinel.size = sizeof(sentinel);
        sentinel.label = utf16(L"Sentinel");
        sentinel.help = utf16(L"Keeps the tab non-empty");
        sentinel.enabled = 1u;
        ERUI_TEST_CHECK(unavailable.add_toggle(
            provider_handle, tab_page, &sentinel, nullptr) == ERUI_OK);
        ERUI_TEST_CHECK(unavailable.commit_provider(provider_handle) ==
            ERUI_OK);

        ERUI_TabUpdateDesc edit{};
        edit.size = sizeof(edit);
        edit.page = tab_page;
        edit.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE;
        edit.visible = 0u;
        const ERUI_UpdateDesc update = wrap_tab(edit);
        ERUI_TEST_CHECK(submit(
            unavailable, provider_handle, &update, 1u) ==
            ERUI_NOT_SUPPORTED);
    }

    erui::host::Registry registry{};
    ERUI_TEST_CHECK(registry.enable_custom_tabs_backend());
    ERUI_TEST_CHECK(registry.enable_updates_backend());
    ERUI_TEST_CHECK(registry.enable_submenu_updates_backend());
    ERUI_TEST_CHECK(registry.enable_tab_updates_backend());

    erui::detail::TabConfiguration configuration{};
    erui::detail::TabOrderOverride target_override{};
    target_override.provider_id = "tab-updates.contract";
    target_override.tab_id = "target";
    target_override.stable_key = "tab-updates.contract/target";
    target_override.order = 500;
    configuration.order_overrides.emplace(
        target_override.stable_key, target_override);
    registry.set_tab_configuration(configuration);
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_4;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("tab-updates.contract");
    provider.display_name = utf16(L"Tab update contract");
    ERUI_ProviderHandle provider_handle{};
    ERUI_PageHandle root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &provider,
        &provider_handle,
        &root) == ERUI_OK);

    ERUI_ToggleDesc rollback_toggle{};
    rollback_toggle.size = sizeof(rollback_toggle);
    rollback_toggle.label = utf16(L"Rollback row");
    rollback_toggle.help = utf16(L"Rollback original help");
    rollback_toggle.enabled = 1u;
    ERUI_RowHandle rollback_row{};
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, root, &rollback_toggle, &rollback_row) == ERUI_OK);

    ERUI_SubmenuDesc submenu{};
    submenu.size = sizeof(submenu);
    submenu.label = utf16(L"Mixed submenu");
    submenu.help = utf16(L"Mixed original help");
    submenu.page_title = utf16(L"Mixed child");
    submenu.enabled = 1u;
    ERUI_PageHandle child{};
    ERUI_RowHandle opener{};
    ERUI_TEST_CHECK(registry.add_submenu(
        provider_handle, root, &submenu, &child, &opener) == ERUI_OK);
    ERUI_ToggleDesc child_sentinel = rollback_toggle;
    child_sentinel.label = utf16(L"Child sentinel");
    ERUI_TEST_CHECK(registry.add_toggle(
        provider_handle, child, &child_sentinel, nullptr) == ERUI_OK);

    const auto add_tab = [&registry, provider_handle](
        const char* id,
        const wchar_t* title,
        std::int32_t order,
        ERUI_PageHandle& output) {
        ERUI_TabDesc description{};
        description.size = sizeof(description);
        description.tab_id = ascii(id);
        description.title = utf16(title);
        description.preferred_order = order;
        ERUI_TEST_CHECK(registry.add_tab(
            provider_handle, &description, &output) == ERUI_OK);
    };
    const auto add_tab_sentinel = [&registry, provider_handle](
        ERUI_PageHandle page,
        const wchar_t* label) {
        ERUI_ToggleDesc sentinel{};
        sentinel.size = sizeof(sentinel);
        sentinel.label = utf16(label);
        sentinel.help = utf16(L"Custom-tab sentinel");
        sentinel.enabled = 1u;
        ERUI_TEST_CHECK(registry.add_toggle(
            provider_handle, page, &sentinel, nullptr) == ERUI_OK);
    };

    ERUI_PageHandle target_page{};
    ERUI_PageHandle other_page{};
    ERUI_PageHandle formatter_page{};
    add_tab("target", L"Target original", 0, target_page);
    add_tab("other", L"Other original", 100, other_page);
    add_tab("formatter", L"Formatter original", 900, formatter_page);
    for (std::size_t index = 0;
         index < erui::detail::native_subpage_capacity + 1u;
         ++index) {
        add_tab_sentinel(target_page, L"Target pagination row");
    }
    add_tab_sentinel(other_page, L"Other sentinel");
    add_tab_sentinel(formatter_page, L"Formatter sentinel");

    FormatterObservation formatter_observation{};
    ERUI_PagePresentationDesc formatter_presentation{};
    formatter_presentation.size = sizeof(formatter_presentation);
    formatter_presentation.page_title = utf16(L"Formatter base");
    formatter_presentation.formatter = &format_page_title;
    formatter_presentation.user_data = &formatter_observation;
    ERUI_TEST_CHECK(registry.set_page_presentation(
        provider_handle,
        formatter_page,
        &formatter_presentation) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);

    ERUI_ProviderDesc foreign_provider = provider;
    foreign_provider.provider_id = ascii("tab-updates.foreign");
    foreign_provider.display_name = utf16(L"Foreign tab updates");
    ERUI_ProviderHandle foreign_handle{};
    ERUI_PageHandle foreign_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &foreign_provider,
        &foreign_handle,
        &foreign_root) == ERUI_OK);
    ERUI_TabDesc foreign_tab{};
    foreign_tab.size = sizeof(foreign_tab);
    foreign_tab.tab_id = ascii("foreign");
    foreign_tab.title = utf16(L"Foreign tab");
    ERUI_PageHandle foreign_page{};
    ERUI_TEST_CHECK(registry.add_tab(
        foreign_handle, &foreign_tab, &foreign_page) == ERUI_OK);
    ERUI_ToggleDesc foreign_sentinel = rollback_toggle;
    foreign_sentinel.label = utf16(L"Foreign sentinel");
    ERUI_TEST_CHECK(registry.add_toggle(
        foreign_handle, foreign_page, &foreign_sentinel, nullptr) == ERUI_OK);
    ERUI_TEST_CHECK(registry.commit_provider(foreign_handle) == ERUI_OK);

    ERUI_ProviderDesc stale_provider = provider;
    stale_provider.provider_id = ascii("tab-updates.stale");
    stale_provider.display_name = utf16(L"Stale tab updates");
    ERUI_ProviderHandle stale_handle{};
    ERUI_PageHandle stale_root{};
    ERUI_TEST_CHECK(registry.register_provider(
        ERUI_API_VERSION_1_4,
        &stale_provider,
        &stale_handle,
        &stale_root) == ERUI_OK);
    ERUI_TabDesc stale_tab = foreign_tab;
    stale_tab.tab_id = ascii("stale");
    stale_tab.title = utf16(L"Stale tab");
    ERUI_PageHandle stale_page{};
    ERUI_TEST_CHECK(registry.add_tab(
        stale_handle, &stale_tab, &stale_page) == ERUI_OK);
    ERUI_TEST_CHECK(registry.abort_provider(stale_handle) == ERUI_OK);

    ERUI_TabUpdateDesc base{};
    base.size = sizeof(base);
    base.page = target_page;
    base.fields = ERUI_TAB_UPDATE_FIELD_TITLE;
    base.title = utf16(L"Candidate title");

    unsigned expectation_index{};
    const auto expect = [&](ERUI_TabUpdateDesc candidate,
                            ERUI_Result expected) {
        ++expectation_index;
        const ERUI_UpdateDesc update = wrap_tab(candidate);
        const ERUI_Result result = submit(
            registry, provider_handle, &update, 1u);
        if (result != expected) {
            std::fprintf(
                stderr,
                "tab expectation %u: expected %d, got %d\n",
                expectation_index,
                static_cast<int>(expected),
                static_cast<int>(result));
        }
        ERUI_TEST_CHECK(result == expected);
    };

    ERUI_TabUpdateDesc invalid = base;
    invalid.page = root;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.page = child;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.page = foreign_page;
    expect(invalid, ERUI_INVALID_HANDLE);
    invalid = base;
    invalid.page = stale_page;
    expect(invalid, ERUI_INVALID_HANDLE);
    invalid = base;
    invalid.page = UINT64_C(0xD0D0D0D0D0D0D0D0);
    expect(invalid, ERUI_INVALID_HANDLE);
    invalid = base;
    invalid.size = sizeof(invalid) - 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.flags = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.reserved0 = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.reserved[1] = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = 0u;
    invalid.title = {};
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = UINT32_C(0x80000000);
    invalid.title = {};
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.title = {};
    expect(invalid, ERUI_INVALID_ARGUMENT);
    std::wstring oversized_title(
        ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS + 1u, L'X');
    invalid = base;
    invalid.title = utf16(oversized_title);
    expect(invalid, ERUI_INVALID_ARGUMENT);

    // Every payload whose bit is absent must remain canonically zero.
    invalid = base;
    invalid.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE;
    invalid.visible = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = ERUI_TAB_UPDATE_FIELD_ICON;
    invalid.title = utf16(L"Hidden noncanonical title");
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = ERUI_TAB_UPDATE_FIELD_TITLE;
    invalid.visible = 1u;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.preferred_order = 12;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE;
    invalid.title = {};
    invalid.visible = 2u;
    expect(invalid, ERUI_INVALID_ARGUMENT);

    std::array<std::uint8_t, ERUI_TAB_ICON_RGBA8_BYTE_COUNT> icon_pixels{};
    for (std::size_t index = 0; index < icon_pixels.size(); ++index) {
        icon_pixels[index] = static_cast<std::uint8_t>(index * 37u + 11u);
    }
    ERUI_TabIconDesc icon{};
    icon.size = sizeof(icon);
    icon.format = ERUI_TAB_ICON_FORMAT_RGBA8;
    icon.byte_count = static_cast<std::uint32_t>(icon_pixels.size());
    icon.pixels = icon_pixels.data();
    invalid = base;
    invalid.fields = ERUI_TAB_UPDATE_FIELD_TITLE;
    invalid.icon = &icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    invalid = base;
    invalid.fields = ERUI_TAB_UPDATE_FIELD_ICON;
    invalid.title = {};
    invalid.icon = &icon;
    ERUI_TabIconDesc malformed_icon = icon;
    malformed_icon.size = sizeof(malformed_icon) - 1u;
    invalid.icon = &malformed_icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    malformed_icon = icon;
    malformed_icon.flags = 1u;
    invalid.icon = &malformed_icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    malformed_icon = icon;
    malformed_icon.format = static_cast<ERUI_TabIconFormat>(2u);
    invalid.icon = &malformed_icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    malformed_icon = icon;
    malformed_icon.byte_count -= 1u;
    invalid.icon = &malformed_icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    malformed_icon = icon;
    malformed_icon.pixels = nullptr;
    invalid.icon = &malformed_icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);
    malformed_icon = icon;
    malformed_icon.reserved[0] = 1u;
    invalid.icon = &malformed_icon;
    expect(invalid, ERUI_INVALID_ARGUMENT);

    invalid = base;
    invalid.page = formatter_page;
    expect(invalid, ERUI_NOT_SUPPORTED);

    const ERUI_UpdateDesc duplicate = wrap_tab(base);
    const std::array duplicate_updates{duplicate, duplicate};
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        duplicate_updates.data(),
        static_cast<std::uint32_t>(duplicate_updates.size())) ==
        ERUI_INVALID_ARGUMENT);

    // A valid row edit preceding an invalid tab target must not escape the
    // failed batch. The frozen compiled row below proves registry rollback.
    ERUI_RowUpdateDesc rollback_update{};
    rollback_update.size = sizeof(rollback_update);
    rollback_update.row = rollback_row;
    rollback_update.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    rollback_update.help = styled_text(L"Must not publish");
    ERUI_UpdateDesc rollback_item{};
    rollback_item.size = sizeof(rollback_item);
    rollback_item.kind = ERUI_UPDATE_KIND_ROW;
    rollback_item.payload = &rollback_update;
    ERUI_TabUpdateDesc wrong_tab = base;
    wrong_tab.page = root;
    const ERUI_UpdateDesc wrong_tab_item = wrap_tab(wrong_tab);
    const std::array atomic_updates{rollback_item, wrong_tab_item};
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        atomic_updates.data(),
        static_cast<std::uint32_t>(atomic_updates.size())) ==
        ERUI_INVALID_ARGUMENT);

    const auto retained_icon_pixels = icon_pixels;
    ERUI_TabUpdateDesc all_fields{};
    all_fields.size = sizeof(all_fields);
    all_fields.page = target_page;
    all_fields.fields = ERUI_TAB_UPDATE_FIELD_TITLE |
        ERUI_TAB_UPDATE_FIELD_ICON |
        ERUI_TAB_UPDATE_FIELD_VISIBLE |
        ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    all_fields.title = utf16(L"Target staged");
    all_fields.icon = &icon;
    all_fields.visible = 0u;
    all_fields.preferred_order = -200;
    const ERUI_UpdateDesc all_fields_item = wrap_tab(all_fields);
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &all_fields_item, 1u) == ERUI_OK);
    icon_pixels.fill(0u);

    std::unique_ptr<erui::Menu> menu = registry.freeze_and_build();
    ERUI_TEST_CHECK(menu != nullptr);
    std::unique_ptr<erui::detail::CompiledMenu> compiled =
        erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled != nullptr);
    const erui::detail::NativeTabCatalogueBuildResult catalogue_result =
        erui::detail::build_native_tab_catalogue(
            *compiled, configuration);
    ERUI_TEST_CHECK(catalogue_result);
    ERUI_TEST_CHECK(catalogue_result.catalogue != nullptr);
    ERUI_TEST_CHECK(erui::detail::configure_row_presentation_state(
        *compiled));
    ERUI_TEST_CHECK(erui::detail::configure_submenu_update_state(
        *compiled));
    const bool tab_state_configured =
        erui::detail::configure_tab_update_state(
            *compiled, *catalogue_result.catalogue);
    if (!tab_state_configured) {
        for (const auto& catalogue_entry :
             catalogue_result.catalogue->custom_entries()) {
            const auto& page = compiled->pages[catalogue_entry.page_index];
            std::fprintf(
                stderr,
                "tab configure entry key=%s/%s pageIndex=%zu identity=%llu/%llu title=%ls\n",
                catalogue_entry.provider_id.c_str(),
                catalogue_entry.tab_id.c_str(),
                catalogue_entry.page_index,
                static_cast<unsigned long long>(page.provider),
                static_cast<unsigned long long>(page.page),
                compiled->texts.lookup(
                    static_cast<int>(catalogue_entry.title_id)));
        }
    }
    ERUI_TEST_CHECK(tab_state_configured);
    erui::detail::synchronize_pending_row_presentations();
    erui::detail::synchronize_pending_submenu_updates();

    const auto compiled_rollback = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [&](const erui::detail::CompiledRow& row) {
            return row.provider == provider_handle &&
                row.row == rollback_row;
        });
    ERUI_TEST_CHECK(compiled_rollback != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_rollback->help_id)) == L"Rollback original help");

    const auto compiled_opener = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [&](const erui::detail::CompiledRow& row) {
            return row.provider == provider_handle && row.row == opener;
        });
    ERUI_TEST_CHECK(compiled_opener != compiled->root_page().rows.end());

    const auto compiled_target = std::find_if(
        compiled->pages.begin(),
        compiled->pages.end(),
        [&](const erui::detail::CompiledPage& page) {
            return page.provider == provider_handle &&
                page.page == target_page;
        });
    ERUI_TEST_CHECK(compiled_target != compiled->pages.end());
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_target->title_id)) == L"Target staged");
    const auto compiled_other = std::find_if(
        compiled->pages.begin(),
        compiled->pages.end(),
        [&](const erui::detail::CompiledPage& page) {
            return page.provider == provider_handle &&
                page.page == other_page;
        });
    ERUI_TEST_CHECK(compiled_other != compiled->pages.end());
    const auto compiled_formatter = std::find_if(
        compiled->pages.begin(),
        compiled->pages.end(),
        [&](const erui::detail::CompiledPage& page) {
            return page.provider == provider_handle &&
                page.page == formatter_page;
        });
    ERUI_TEST_CHECK(compiled_formatter != compiled->pages.end());
    ERUI_TEST_CHECK(formatter_observation.calls != 0u);
    ERUI_TEST_CHECK(formatter_observation.page == formatter_page);

    auto generation = erui::detail::acquire_applied_tab_generation();
    ERUI_TEST_CHECK(generation != nullptr);
    const auto find_entry = [](const auto& state, ERUI_PageHandle page)
        -> const erui::detail::TabRuntimeEntry* {
        if (!state) return nullptr;
        const auto found = std::find_if(
            state->entries.begin(),
            state->entries.end(),
            [page](const erui::detail::TabRuntimeEntry& entry) {
                return entry.page == page;
            });
        return found == state->entries.end() ? nullptr : &*found;
    };
    const erui::detail::TabRuntimeEntry* target =
        find_entry(generation, target_page);
    const erui::detail::TabRuntimeEntry* other =
        find_entry(generation, other_page);
    ERUI_TEST_CHECK(target != nullptr && other != nullptr);
    ERUI_TEST_CHECK(target->title == L"Target staged");
    ERUI_TEST_CHECK(target->title_overridden);
    ERUI_TEST_CHECK(!other->title_overridden);
    ERUI_TEST_CHECK(!target->visible);
    ERUI_TEST_CHECK(target->preferred_order == -200);
    ERUI_TEST_CHECK(target->player_order == 500);
    ERUI_TEST_CHECK(target->effective_order == 500);
    ERUI_TEST_CHECK(target->icon.source == erui::TabIconSource::rgba8);
    ERUI_TEST_CHECK(target->icon.rgba8_pixels.size() ==
        retained_icon_pixels.size());
    ERUI_TEST_CHECK(std::equal(
        target->icon.rgba8_pixels.begin(),
        target->icon.rgba8_pixels.end(),
        retained_icon_pixels.begin(),
        retained_icon_pixels.end()));
    // Other, formatter, and the independently owned foreign tab remain
    // visible; only Target was filtered by the preconfiguration edit.
    ERUI_TEST_CHECK(generation->visible_order.size() == 3u);
    ERUI_TEST_CHECK(std::find(
        generation->visible_order.begin(),
        generation->visible_order.end(),
        target->stable_index) == generation->visible_order.end());

    const std::size_t target_page_index = static_cast<std::size_t>(
        std::distance(compiled->pages.begin(), compiled_target));
    ERUI_TEST_CHECK(target_page_index < compiled->page_plans.size());
    const std::size_t target_slices =
        compiled->page_plans[target_page_index].slices.size();
    ERUI_TEST_CHECK(target_slices > 1u);
    std::array<wchar_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY> title_buffer{};
    std::uint64_t title_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        0u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        title_revision));
    ERUI_TEST_CHECK(title_revision == generation->revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target staged (1/" + std::to_wstring(target_slices) + L")");
    // Untouched tabs do not create a rich-title override. Their registered
    // static PagePresentation (Other) or formatter (Formatter) remains the
    // page-heading authority even though the selector generation also owns a
    // title for each tab icon.
    const std::size_t other_page_index = static_cast<std::size_t>(
        std::distance(compiled->pages.begin(), compiled_other));
    title_buffer.fill(L'\0');
    std::uint64_t other_title_revision{};
    ERUI_TEST_CHECK(!erui::detail::format_applied_tab_page_title(
        other_page_index,
        0u,
        1u,
        title_buffer.data(),
        title_buffer.size(),
        other_title_revision));
    ERUI_TEST_CHECK(other_title_revision == 0u);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_other->title_id)) == L"Other original");
    const std::size_t formatter_page_index = static_cast<std::size_t>(
        std::distance(compiled->pages.begin(), compiled_formatter));
    std::uint64_t formatter_title_revision{};
    ERUI_TEST_CHECK(!erui::detail::format_applied_tab_page_title(
        formatter_page_index,
        0u,
        1u,
        title_buffer.data(),
        title_buffer.size(),
        formatter_title_revision));
    ERUI_TEST_CHECK(formatter_title_revision == 0u);
    // A custom-tab title is never staged through the independently applied
    // submenu-title channel.  Otherwise an icon+title edit could expose the
    // new heading before the selector's icon generation is installed.
    std::uint64_t unrelated_submenu_revision{};
    ERUI_TEST_CHECK(!erui::detail::format_submenu_page_title(
        compiled_target->submenu_update_slot,
        0u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        unrelated_submenu_revision));
    ERUI_TEST_CHECK(unrelated_submenu_revision == 0u);

    // A TAB descriptor cannot share a transaction with ROW or SUBMENU.  The
    // rejection happens before any of the three update channels is prepared,
    // so synchronizing the independent channels afterward changes nothing.
    ERUI_RowUpdateDesc mixed_row{};
    mixed_row.size = sizeof(mixed_row);
    mixed_row.row = rollback_row;
    mixed_row.fields = ERUI_ROW_UPDATE_FIELD_HELP;
    mixed_row.help = styled_text(L"Mixed row help");
    ERUI_UpdateDesc mixed_row_item{};
    mixed_row_item.size = sizeof(mixed_row_item);
    mixed_row_item.kind = ERUI_UPDATE_KIND_ROW;
    mixed_row_item.payload = &mixed_row;
    ERUI_SubmenuUpdateDesc mixed_submenu{};
    mixed_submenu.size = sizeof(mixed_submenu);
    mixed_submenu.row = opener;
    mixed_submenu.page = child;
    mixed_submenu.fields = ERUI_SUBMENU_UPDATE_FIELD_HELP;
    mixed_submenu.help = styled_text(L"Mixed opener help");
    ERUI_UpdateDesc mixed_submenu_item{};
    mixed_submenu_item.size = sizeof(mixed_submenu_item);
    mixed_submenu_item.kind = ERUI_UPDATE_KIND_SUBMENU;
    mixed_submenu_item.payload = &mixed_submenu;
    ERUI_TabUpdateDesc mixed_tab{};
    mixed_tab.size = sizeof(mixed_tab);
    mixed_tab.page = other_page;
    mixed_tab.fields = ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    mixed_tab.preferred_order = 1000;
    const ERUI_UpdateDesc mixed_tab_item = wrap_tab(mixed_tab);
    const std::array mixed_updates{
        mixed_row_item, mixed_submenu_item, mixed_tab_item};
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        mixed_updates.data(),
        static_cast<std::uint32_t>(mixed_updates.size())) ==
        ERUI_INVALID_ARGUMENT);
    erui::detail::synchronize_pending_row_presentations();
    erui::detail::synchronize_pending_submenu_updates();
    ERUI_TEST_CHECK(!erui::detail::row_presentation_view(
        compiled_rollback->presentation_slot));
    ERUI_TEST_CHECK(!erui::detail::row_presentation_view(
        compiled_opener->presentation_slot));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_rollback->help_id)) == L"Rollback original help");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_opener->help_id)) == L"Mixed original help");
    ERUI_TEST_CHECK(!erui::detail::tab_updates_pending());
    const auto desired_after_mixed_rejection =
        erui::detail::acquire_desired_tab_generation();
    const auto applied_after_mixed_rejection =
        erui::detail::acquire_applied_tab_generation();
    ERUI_TEST_CHECK(desired_after_mixed_rejection == generation);
    ERUI_TEST_CHECK(applied_after_mixed_rejection == generation);
    other = find_entry(generation, other_page);
    ERUI_TEST_CHECK(other != nullptr && other->preferred_order == 100);

    // Several TAB descriptors are one valid atomic tab-generation update.
    ERUI_TabUpdateDesc multi_target{};
    multi_target.size = sizeof(multi_target);
    multi_target.page = target_page;
    multi_target.fields = ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    multi_target.preferred_order = -900;
    ERUI_TabUpdateDesc multi_other{};
    multi_other.size = sizeof(multi_other);
    multi_other.page = other_page;
    multi_other.fields = ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    multi_other.preferred_order = 1000;
    const std::array multi_tab_updates{
        wrap_tab(multi_target), wrap_tab(multi_other)};
    const std::uint64_t multi_old_revision = generation->revision;
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        multi_tab_updates.data(),
        static_cast<std::uint32_t>(multi_tab_updates.size())) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::tab_updates_pending());
    const auto multi_desired =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(multi_desired != nullptr);
    ERUI_TEST_CHECK(multi_desired->revision > multi_old_revision);
    const auto* multi_desired_target = find_entry(
        multi_desired, target_page);
    const auto* multi_desired_other = find_entry(multi_desired, other_page);
    ERUI_TEST_CHECK(multi_desired_target != nullptr);
    ERUI_TEST_CHECK(multi_desired_other != nullptr);
    ERUI_TEST_CHECK(multi_desired_target->preferred_order == -900);
    ERUI_TEST_CHECK(multi_desired_other->preferred_order == 1000);
    ERUI_TEST_CHECK(
        erui::detail::acquire_applied_tab_generation() == generation);
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        multi_desired));
    ERUI_TEST_CHECK(!erui::detail::tab_updates_pending());
    generation = erui::detail::acquire_applied_tab_generation();
    target = find_entry(generation, target_page);
    other = find_entry(generation, other_page);
    ERUI_TEST_CHECK(target != nullptr && other != nullptr);
    ERUI_TEST_CHECK(target->preferred_order == -900);
    ERUI_TEST_CHECK(other->preferred_order == 1000);

    // Live title/default-icon/show/order is staged as one tab generation.
    ERUI_TabUpdateDesc live{};
    live.size = sizeof(live);
    live.page = target_page;
    live.fields = ERUI_TAB_UPDATE_FIELD_TITLE |
        ERUI_TAB_UPDATE_FIELD_ICON |
        ERUI_TAB_UPDATE_FIELD_VISIBLE |
        ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    live.title = utf16(L"Target live");
    live.icon = nullptr;
    live.visible = 1u;
    live.preferred_order = -1000;
    const ERUI_UpdateDesc live_item = wrap_tab(live);
    const std::uint64_t old_revision = generation->revision;
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &live_item, 1u) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::tab_updates_pending());
    ERUI_TEST_CHECK(erui::detail::acquire_applied_tab_generation()->revision ==
        old_revision);
    title_buffer.fill(L'\0');
    std::uint64_t staged_title_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        target_slices - 1u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        staged_title_revision));
    ERUI_TEST_CHECK(staged_title_revision == old_revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target staged (" + std::to_wstring(target_slices) + L"/" +
            std::to_wstring(target_slices) + L")");

    const auto live_desired =
        erui::detail::acquire_desired_tab_generation();
    const auto* desired_target = find_entry(live_desired, target_page);
    ERUI_TEST_CHECK(live_desired != nullptr && desired_target != nullptr);
    ERUI_TEST_CHECK(live_desired->revision > old_revision);
    ERUI_TEST_CHECK(desired_target->title == L"Target live");
    ERUI_TEST_CHECK(desired_target->title_overridden);
    ERUI_TEST_CHECK(desired_target->visible);
    ERUI_TEST_CHECK(desired_target->icon.source ==
        erui::TabIconSource::fallback);
    // A fresh Configuration movie may open before the prior movie's raster
    // references finish retiring.  The native adapter deliberately performs
    // no acknowledgement in that case.  Reacquiring the desired generation
    // on later frames (or another fast reopen) must preserve the exact retry
    // target while the last fully installed generation remains applied.
    const auto first_fresh_epoch_retry =
        erui::detail::acquire_desired_tab_generation();
    const auto second_fresh_epoch_retry =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(first_fresh_epoch_retry == live_desired);
    ERUI_TEST_CHECK(second_fresh_epoch_retry == live_desired);
    ERUI_TEST_CHECK(erui::detail::tab_updates_pending());
    ERUI_TEST_CHECK(erui::detail::acquire_applied_tab_generation()->revision ==
        old_revision);
    // Publishing unrelated submenu titles cannot leak the desired custom-tab
    // title.  The page heading remains tied to the applied icon generation.
    erui::detail::synchronize_pending_submenu_updates();
    title_buffer.fill(L'\0');
    std::uint64_t after_submenu_sync_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        target_slices - 1u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        after_submenu_sync_revision));
    ERUI_TEST_CHECK(after_submenu_sync_revision == old_revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target staged (" + std::to_wstring(target_slices) + L"/" +
            std::to_wstring(target_slices) + L")");

    // This explicit acknowledgement stands in for the native tab adapter,
    // which publishes only after the selector records and icon resources for
    // this exact immutable generation are installed.
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        live_desired));
    generation = erui::detail::acquire_applied_tab_generation();
    target = find_entry(generation, target_page);
    other = find_entry(generation, other_page);
    ERUI_TEST_CHECK(target != nullptr && other != nullptr);
    ERUI_TEST_CHECK(generation->revision > old_revision);
    ERUI_TEST_CHECK(target->title == L"Target live");
    ERUI_TEST_CHECK(target->visible);
    ERUI_TEST_CHECK(target->preferred_order == -1000);
    ERUI_TEST_CHECK(target->effective_order == 500);
    ERUI_TEST_CHECK(target->icon.source == erui::TabIconSource::fallback);
    ERUI_TEST_CHECK(target->icon.rgba8_pixels.empty());
    ERUI_TEST_CHECK(generation->visible_order.size() == 4u);
    const auto target_position = std::find(
        generation->visible_order.begin(),
        generation->visible_order.end(),
        target->stable_index);
    const auto other_position = std::find(
        generation->visible_order.begin(),
        generation->visible_order.end(),
        other->stable_index);
    ERUI_TEST_CHECK(target_position < other_position);

    std::uint64_t live_title_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        target_slices - 1u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        live_title_revision));
    ERUI_TEST_CHECK(live_title_revision > title_revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target live (" + std::to_wstring(target_slices) + L"/" +
            std::to_wstring(target_slices) + L")");

    // Recommitting byte-identical icon content advances the logical update
    // generation but not the immutable icon revision. The native adapter can
    // acknowledge this as a non-icon generation without allocating another
    // process-retained image root.
    ERUI_TabUpdateDesc identical_icon{};
    identical_icon.size = sizeof(identical_icon);
    identical_icon.page = target_page;
    identical_icon.fields = ERUI_TAB_UPDATE_FIELD_ICON;
    identical_icon.icon = nullptr;
    const ERUI_UpdateDesc identical_icon_item = wrap_tab(identical_icon);
    const std::uint64_t before_identical_icon_revision =
        target->icon_revision;
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &identical_icon_item, 1u) == ERUI_OK);
    const auto identical_icon_desired =
        erui::detail::acquire_desired_tab_generation();
    const auto* identical_icon_target = find_entry(
        identical_icon_desired, target_page);
    ERUI_TEST_CHECK(identical_icon_desired != nullptr);
    ERUI_TEST_CHECK(identical_icon_target != nullptr);
    ERUI_TEST_CHECK(identical_icon_target->icon_revision ==
        before_identical_icon_revision);
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        identical_icon_desired));
    generation = erui::detail::acquire_applied_tab_generation();
    target = find_entry(generation, target_page);
    ERUI_TEST_CHECK(target != nullptr);
    ERUI_TEST_CHECK(target->icon_revision == before_identical_icon_revision);

    // A title-only generation follows the same applied-generation boundary;
    // it must not bypass the native adapter merely because no icon changed.
    ERUI_TabUpdateDesc title_only{};
    title_only.size = sizeof(title_only);
    title_only.page = target_page;
    title_only.fields = ERUI_TAB_UPDATE_FIELD_TITLE;
    title_only.title = utf16(L"Target title only");
    const ERUI_UpdateDesc title_only_item = wrap_tab(title_only);
    const std::uint64_t before_title_only_revision = generation->revision;
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &title_only_item, 1u) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::tab_updates_pending());
    title_buffer.fill(L'\0');
    std::uint64_t before_title_publish_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        0u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        before_title_publish_revision));
    ERUI_TEST_CHECK(before_title_publish_revision ==
        before_title_only_revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target live (1/" + std::to_wstring(target_slices) + L")");
    const auto title_only_desired =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(title_only_desired != nullptr);
    desired_target = find_entry(title_only_desired, target_page);
    ERUI_TEST_CHECK(desired_target != nullptr);
    ERUI_TEST_CHECK(desired_target->title == L"Target title only");
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        title_only_desired));
    generation = erui::detail::acquire_applied_tab_generation();
    target = find_entry(generation, target_page);
    ERUI_TEST_CHECK(target != nullptr);
    ERUI_TEST_CHECK(target->title == L"Target title only");
    title_buffer.fill(L'\0');
    std::uint64_t title_only_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        target_slices - 1u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        title_only_revision));
    ERUI_TEST_CHECK(title_only_revision == generation->revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target title only (" + std::to_wstring(target_slices) + L"/" +
            std::to_wstring(target_slices) + L")");

    // Projection generations are acknowledged independently from commits.
    // Keep the current generation applied while the adapter owns a captured
    // projection, then queue a newer title generation before acknowledging
    // that projection.  The captured generation is accepted as applied but
    // is no longer latest, so the newer desired generation stays pending.
    const auto source_before_queued_projection = generation;
    ERUI_TabUpdateDesc queued_projection{};
    queued_projection.size = sizeof(queued_projection);
    queued_projection.page = target_page;
    queued_projection.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE |
        ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    queued_projection.visible = 0u;
    queued_projection.preferred_order = -1100;
    const ERUI_UpdateDesc queued_projection_item = wrap_tab(
        queued_projection);
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        &queued_projection_item,
        1u) == ERUI_OK);
    ERUI_TEST_CHECK(erui::detail::tab_updates_pending());
    const auto captured_projection =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(captured_projection != nullptr);
    ERUI_TEST_CHECK(captured_projection->revision >
        source_before_queued_projection->revision);
    const auto* captured_projection_target = find_entry(
        captured_projection, target_page);
    ERUI_TEST_CHECK(captured_projection_target != nullptr);
    ERUI_TEST_CHECK(!captured_projection_target->visible);
    ERUI_TEST_CHECK(captured_projection_target->preferred_order == -1100);
    ERUI_TEST_CHECK(captured_projection_target->title ==
        L"Target title only");
    ERUI_TEST_CHECK(erui::detail::acquire_applied_tab_generation() ==
        source_before_queued_projection);

    ERUI_TabUpdateDesc queued_title{};
    queued_title.size = sizeof(queued_title);
    queued_title.page = target_page;
    queued_title.fields = ERUI_TAB_UPDATE_FIELD_TITLE;
    queued_title.title = utf16(L"Target queued title");
    const ERUI_UpdateDesc queued_title_item = wrap_tab(queued_title);
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &queued_title_item, 1u) == ERUI_OK);
    const auto queued_newer =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(queued_newer != nullptr);
    ERUI_TEST_CHECK(queued_newer->revision > captured_projection->revision);
    const auto* queued_newer_target = find_entry(queued_newer, target_page);
    ERUI_TEST_CHECK(queued_newer_target != nullptr);
    ERUI_TEST_CHECK(!queued_newer_target->visible);
    ERUI_TEST_CHECK(queued_newer_target->preferred_order == -1100);
    ERUI_TEST_CHECK(queued_newer_target->title == L"Target queued title");
    ERUI_TEST_CHECK(erui::detail::acquire_applied_tab_generation() ==
        source_before_queued_projection);

    ERUI_TEST_CHECK(!erui::detail::publish_applied_tab_generation(
        captured_projection));
    ERUI_TEST_CHECK(erui::detail::acquire_applied_tab_generation() ==
        captured_projection);
    ERUI_TEST_CHECK(erui::detail::acquire_desired_tab_generation() ==
        queued_newer);
    ERUI_TEST_CHECK(erui::detail::tab_updates_pending());
    title_buffer.fill(L'\0');
    std::uint64_t captured_projection_title_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        0u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        captured_projection_title_revision));
    ERUI_TEST_CHECK(captured_projection_title_revision ==
        captured_projection->revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target title only (1/" + std::to_wstring(target_slices) + L")");

    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        queued_newer));
    generation = erui::detail::acquire_applied_tab_generation();
    ERUI_TEST_CHECK(generation == queued_newer);
    ERUI_TEST_CHECK(!erui::detail::tab_updates_pending());
    target = find_entry(generation, target_page);
    ERUI_TEST_CHECK(target != nullptr);
    ERUI_TEST_CHECK(!target->visible);
    ERUI_TEST_CHECK(target->preferred_order == -1100);
    ERUI_TEST_CHECK(target->title == L"Target queued title");

    // Finish the projection -> title -> projection cycle.  A later
    // projection must preserve the applied rich title while changing only
    // visibility/order, and it remains deferred until explicitly published.
    ERUI_TabUpdateDesc final_projection{};
    final_projection.size = sizeof(final_projection);
    final_projection.page = target_page;
    final_projection.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE |
        ERUI_TAB_UPDATE_FIELD_PREFERRED_ORDER;
    final_projection.visible = 1u;
    final_projection.preferred_order = -1200;
    const ERUI_UpdateDesc final_projection_item = wrap_tab(final_projection);
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        &final_projection_item,
        1u) == ERUI_OK);
    const auto final_projection_desired =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(final_projection_desired != nullptr);
    ERUI_TEST_CHECK(final_projection_desired->revision > generation->revision);
    const auto* final_projection_target = find_entry(
        final_projection_desired, target_page);
    ERUI_TEST_CHECK(final_projection_target != nullptr);
    ERUI_TEST_CHECK(final_projection_target->visible);
    ERUI_TEST_CHECK(final_projection_target->preferred_order == -1200);
    ERUI_TEST_CHECK(final_projection_target->title ==
        L"Target queued title");
    ERUI_TEST_CHECK(erui::detail::acquire_applied_tab_generation() ==
        generation);
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        final_projection_desired));
    generation = erui::detail::acquire_applied_tab_generation();
    target = find_entry(generation, target_page);
    ERUI_TEST_CHECK(target != nullptr);
    ERUI_TEST_CHECK(target->visible);
    ERUI_TEST_CHECK(target->preferred_order == -1200);
    ERUI_TEST_CHECK(target->title == L"Target queued title");
    title_buffer.fill(L'\0');
    std::uint64_t final_projection_title_revision{};
    ERUI_TEST_CHECK(erui::detail::format_applied_tab_page_title(
        target_page_index,
        target_slices - 1u,
        target_slices,
        title_buffer.data(),
        title_buffer.size(),
        final_projection_title_revision));
    ERUI_TEST_CHECK(final_projection_title_revision == generation->revision);
    ERUI_TEST_CHECK(std::wstring(title_buffer.data()) ==
        L"Target queued title (" + std::to_wstring(target_slices) + L"/" +
            std::to_wstring(target_slices) + L")");

    // Hiding every custom tab is valid: stock tabs remain the deterministic
    // fallback set. Re-showing restores stable identities and latest fields.
    std::vector<ERUI_TabUpdateDesc> hide_payloads{};
    std::vector<ERUI_UpdateDesc> hide_items{};
    hide_payloads.reserve(generation->entries.size());
    hide_items.reserve(generation->entries.size());
    for (const erui::detail::TabRuntimeEntry& entry : generation->entries) {
        if (entry.provider != provider_handle) continue;
        ERUI_TabUpdateDesc hide{};
        hide.size = sizeof(hide);
        hide.page = entry.page;
        hide.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE;
        hide.visible = 0u;
        hide_payloads.push_back(hide);
    }
    for (const ERUI_TabUpdateDesc& hide : hide_payloads) {
        hide_items.push_back(wrap_tab(hide));
    }
    ERUI_TEST_CHECK(submit(
        registry,
        provider_handle,
        hide_items.data(),
        static_cast<std::uint32_t>(hide_items.size())) == ERUI_OK);

    ERUI_TabUpdateDesc hide_foreign{};
    hide_foreign.size = sizeof(hide_foreign);
    hide_foreign.page = foreign_page;
    hide_foreign.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE;
    hide_foreign.visible = 0u;
    const ERUI_UpdateDesc hide_foreign_item = wrap_tab(hide_foreign);
    ERUI_TEST_CHECK(submit(
        registry, foreign_handle, &hide_foreign_item, 1u) == ERUI_OK);
    const auto hidden_desired =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(hidden_desired != nullptr);
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        hidden_desired));
    generation = erui::detail::acquire_applied_tab_generation();
    ERUI_TEST_CHECK(generation->visible_order.empty());

    ERUI_TabUpdateDesc restore{};
    restore.size = sizeof(restore);
    restore.page = target_page;
    restore.fields = ERUI_TAB_UPDATE_FIELD_VISIBLE;
    restore.visible = 1u;
    const ERUI_UpdateDesc restore_item = wrap_tab(restore);
    ERUI_TEST_CHECK(submit(
        registry, provider_handle, &restore_item, 1u) == ERUI_OK);
    const auto restored_desired =
        erui::detail::acquire_desired_tab_generation();
    ERUI_TEST_CHECK(restored_desired != nullptr);
    ERUI_TEST_CHECK(erui::detail::publish_applied_tab_generation(
        restored_desired));
    generation = erui::detail::acquire_applied_tab_generation();
    target = find_entry(generation, target_page);
    ERUI_TEST_CHECK(target != nullptr && target->visible);
    ERUI_TEST_CHECK(target->title == L"Target queued title");
    ERUI_TEST_CHECK(generation->visible_order.size() == 1u);
    ERUI_TEST_CHECK(generation->visible_order.front() ==
        target->stable_index);

    erui::detail::reset_row_presentation_state();
    erui::detail::reset_submenu_update_state();
    erui::detail::reset_tab_update_state();
}

} // namespace

int main() {
    erui::host::set_api_state(erui::host::ApiState::accepting);
    verify_provider_identifier_contract();
    verify_title_screen_text_registration_contract();
    verify_title_screen_text_abort_reclaims_draft();
    verify_title_screen_text_committed_limit();
    verify_custom_tab_backend_gate();
    verify_custom_tab_registration_contract();
    verify_custom_tab_committed_limit();
    verify_fixed_fifty_custom_tab_stress();
    verify_api_1_4_row_update_contract();
    verify_api_1_4_button_styled_update_before_freeze();
    verify_api_1_4_caption_update_contract();
    verify_api_1_4_row_availability_contract();
    verify_api_1_4_row_visibility_admission_edges();
    verify_api_1_4_row_visibility_contract();
    verify_api_1_4_submenu_update_contract();
    verify_api_1_4_tab_update_contract();

    ERUI_Api early_v1{};
    early_v1.size = ERUI_API_V1_0_SIZE;
    ERUI_TEST_CHECK(ERUI_GetApi(
        ERUI_API_VERSION_1_0, &early_v1) == ERUI_OK);
    ERUI_Api pending_v1_1{};
    pending_v1_1.size = ERUI_API_V1_1_SIZE;
    ERUI_TEST_CHECK(ERUI_GetApi(
        ERUI_API_VERSION_1_1, &pending_v1_1) == ERUI_HOST_NOT_READY);

    erui::host::SteamLanguageProbe language{};
    language.available = true;
    language.current = "english";
    ERUI_TEST_CHECK(erui::host::settle_cached_steam_language(
        std::move(language)) == ERUI_OK);

    verify_text_input_length_contract();
    verify_programmatic_value_writes_are_silent();

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
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_INPUT_BINDINGS) != 0);
    ERUI_TEST_CHECK(negotiated.table.add_input_section != nullptr);
    ERUI_TEST_CHECK(negotiated.table.add_input_action != nullptr);
    ERUI_TEST_CHECK(negotiated.table.set_action_inputs != nullptr);
    ERUI_TEST_CHECK(negotiated.table.storage_get_info != nullptr);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_CUSTOM_TABS) == 0);
    ERUI_TEST_CHECK(negotiated.table.add_tab != nullptr);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_TITLE_SCREEN_TEXT) == 0);
    ERUI_TEST_CHECK((negotiated.table.capabilities &
        ERUI_CAP_ROW_CONFIGURATION) == 0);
    ERUI_TEST_CHECK(negotiated.table.add_title_screen_text != nullptr);
    ERUI_PageHandle unavailable_tab{UINT64_C(0x5AA55AA55AA55AA5)};
    ERUI_TEST_CHECK(negotiated.table.add_tab(
        ERUI_INVALID_PROVIDER, nullptr, &unavailable_tab) ==
        ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(unavailable_tab == UINT64_C(0x5AA55AA55AA55AA5));
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
    ERUI_TEST_CHECK(
        erui::host::registry().enable_custom_tabs_backend());
    ERUI_TEST_CHECK(
        erui::host::registry().enable_title_screen_text_backend());
    ERUI_TEST_CHECK(
        erui::host::registry().enable_updates_backend());
    ERUI_TEST_CHECK(
        erui::host::registry().enable_row_configuration_backend());
    ERUI_Api tabs_enabled{};
    tabs_enabled.size = sizeof(tabs_enabled);
    ERUI_TEST_CHECK(ERUI_GetApi(
        ERUI_API_VERSION_1_3, &tabs_enabled) == ERUI_OK);
    ERUI_TEST_CHECK((tabs_enabled.capabilities &
        ERUI_CAP_CUSTOM_TABS) != 0);
    ERUI_TEST_CHECK(tabs_enabled.add_tab != nullptr);
    ERUI_TEST_CHECK((tabs_enabled.capabilities &
        ERUI_CAP_TITLE_SCREEN_TEXT) != 0);
    ERUI_TEST_CHECK(tabs_enabled.add_title_screen_text != nullptr);
    ERUI_Api updates_enabled{};
    updates_enabled.size = sizeof(updates_enabled);
    ERUI_TEST_CHECK(ERUI_GetApi(
        ERUI_API_VERSION_1_4, &updates_enabled) == ERUI_OK);
    ERUI_TEST_CHECK((updates_enabled.capabilities &
        ERUI_CAP_UPDATES) != 0);
    ERUI_TEST_CHECK((updates_enabled.capabilities &
        ERUI_CAP_ROW_CONFIGURATION) != 0);
    erui::host::registry().open_registration();
    ERUI_TEST_CHECK(
        !erui::host::registry().enable_custom_tabs_backend());
    ERUI_TEST_CHECK(
        !erui::host::registry().enable_title_screen_text_backend());
    ERUI_TEST_CHECK(
        !erui::host::registry().enable_row_configuration_backend());
    ERUI_ProviderDesc v1_provider{};
    v1_provider.size = sizeof(v1_provider);
    v1_provider.api_version = ERUI_API_VERSION_1_0;
    v1_provider.owner_module = GetModuleHandleW(nullptr);
    v1_provider.provider_id = ascii("v1-mixed-table");
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
    ERUI_TEST_CHECK(negotiated.table.add_title_screen_text(
        v1_provider_handle, nullptr) == ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(negotiated.table.set_text_input_value(
        v1_provider_handle, UINT64_C(123), nullptr) == ERUI_NOT_SUPPORTED);
    std::uint32_t untouched_length{0xA5A5A5A5u};
    ERUI_TEST_CHECK(negotiated.table.get_text_input_value(
        v1_provider_handle, UINT64_C(123), nullptr, 0, &untouched_length) ==
        ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched_length == 0xA5A5A5A5u);
    ERUI_PageHandle v1_builtin_page{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(erui::host::registry().get_builtin_page(
        v1_provider_handle,
        ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
        &v1_builtin_page) == ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(v1_builtin_page == UINT64_C(0xA55AA55AA55AA55A));
    ERUI_InputSectionHandle untouched_section{
        UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(negotiated.table.add_input_section(
        v1_provider_handle, nullptr, &untouched_section) ==
        ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched_section == UINT64_C(0xA55AA55AA55AA55A));
    ERUI_InputActionHandle untouched_binding{UINT64_C(0x5AA55AA55AA55AA5)};
    ERUI_TEST_CHECK(negotiated.table.add_input_action(
        v1_provider_handle,
        ERUI_INVALID_INPUT_SECTION,
        nullptr,
        &untouched_binding) == ERUI_NOT_SUPPORTED);
    ERUI_TEST_CHECK(untouched_binding == UINT64_C(0x5AA55AA55AA55AA5));
    ERUI_TEST_CHECK(v1_table->abort_provider(v1_provider_handle) == ERUI_OK);

    erui::host::Registry registry{};
    registry.open_registration();

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_CURRENT;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = ascii("registry-test");
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

    ERUI_PageHandle invalid_builtin_output{UINT64_C(0x123456789ABCDEF0)};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle, ERUI_BUILTIN_PAGE_COUNT, &invalid_builtin_output) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(invalid_builtin_output == ERUI_INVALID_PAGE);
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle, UINT32_MAX, &invalid_builtin_output) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.get_builtin_page(
        ERUI_INVALID_PROVIDER,
        ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
        &invalid_builtin_output) == ERUI_INVALID_HANDLE);
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle, ERUI_BUILTIN_PAGE_CAMERA_OPTIONS, nullptr) ==
        ERUI_INVALID_ARGUMENT);

    ERUI_PageHandle game_options_page{};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle,
        ERUI_BUILTIN_PAGE_GAME_OPTIONS,
        &game_options_page) == ERUI_OK);
    ERUI_TEST_CHECK(game_options_page == root);
    ERUI_PageHandle camera_page{};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle,
        ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
        &camera_page) == ERUI_OK);
    ERUI_TEST_CHECK(camera_page != ERUI_INVALID_PAGE && camera_page != root);
    ERUI_PageHandle repeated_camera_page{};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle,
        ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
        &repeated_camera_page) == ERUI_OK);
    ERUI_TEST_CHECK(repeated_camera_page == camera_page);
    ERUI_PageHandle sound_page{};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle, ERUI_BUILTIN_PAGE_SOUND, &sound_page) == ERUI_OK);
    ERUI_TEST_CHECK(sound_page != camera_page && sound_page != root);

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

    ColorObservation color_observation{};
    ERUI_ColorPickerDesc color_picker{};
    color_picker.size = sizeof(color_picker);
    color_picker.label = utf16(L"Accent Color");
    color_picker.help = utf16(L"ColorPicker help");
    color_picker.changed_callback = &color_changed;
    color_picker.user_data = &color_observation;
    color_picker.initial_value = ERUI_Color{171u, 125u, 99u, 0u};
    color_picker.enabled = 1u;
    ERUI_RowHandle color_picker_row{};
    ERUI_TEST_CHECK(registry.add_color_picker(
        handle, root, &color_picker, &color_picker_row) == ERUI_OK);
    ERUI_TEST_CHECK(color_picker_row != ERUI_INVALID_ROW);

    ERUI_ColorPickerDesc invalid_color_picker = color_picker;
    ERUI_RowHandle rejected_color_row{UINT64_C(0x0FEDCBA987654321)};
    invalid_color_picker.initial_value.reserved = 1u;
    ERUI_TEST_CHECK(registry.add_color_picker(
        handle, root, &invalid_color_picker, &rejected_color_row) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(rejected_color_row == ERUI_INVALID_ROW);
    invalid_color_picker = color_picker;
    invalid_color_picker.enabled = 2u;
    ERUI_TEST_CHECK(registry.add_color_picker(
        handle, root, &invalid_color_picker, nullptr) ==
        ERUI_INVALID_ARGUMENT);

    BindingObservation binding_observation{};
    ERUI_InputSectionDesc binding_section{};
    binding_section.size = sizeof(binding_section);
    binding_section.label = utf16(L"Registry Bindings");
    ERUI_InputSectionHandle binding_section_handle{};
    ERUI_TEST_CHECK(registry.add_input_section(
        handle, &binding_section, &binding_section_handle) == ERUI_OK);
    ERUI_TEST_CHECK(binding_section_handle != ERUI_INVALID_INPUT_SECTION);

    ERUI_InputActionDesc binding{};
    binding.size = sizeof(binding);
    binding.action_id = ascii("show-dialog");
    binding.label = utf16(L"Show Native Dialog");
    binding.default_inputs = default_action_inputs();
    binding.activated_callback = &binding_activated;
    binding.user_data = &binding_observation;
    ERUI_InputActionHandle binding_handle{};
    ERUI_TEST_CHECK(registry.add_input_action(
        handle,
        binding_section_handle,
        &binding,
        &binding_handle) == ERUI_OK);
    ERUI_TEST_CHECK(binding_handle != ERUI_INVALID_INPUT_ACTION);

    ERUI_InputSectionDesc second_binding_section = binding_section;
    second_binding_section.label = utf16(L"Other Bindings");
    ERUI_InputSectionHandle second_binding_section_handle{};
    ERUI_TEST_CHECK(registry.add_input_section(
        handle,
        &second_binding_section,
        &second_binding_section_handle) == ERUI_OK);
    ERUI_InputActionHandle duplicate_binding{UINT64_C(0x123456789ABCDEF0)};
    ERUI_TEST_CHECK(registry.add_input_action(
        handle,
        second_binding_section_handle,
        &binding,
        &duplicate_binding) == ERUI_DUPLICATE_ACTION_ID);
    ERUI_TEST_CHECK(duplicate_binding == ERUI_INVALID_INPUT_ACTION);

    ERUI_InputActionDesc second_binding = binding;
    second_binding.action_id = ascii("toggle-feature");
    second_binding.label = utf16(L"Toggle Feature");
    ERUI_InputActionHandle second_binding_handle{};
    ERUI_TEST_CHECK(registry.add_input_action(
        handle,
        second_binding_section_handle,
        &second_binding,
        &second_binding_handle) == ERUI_OK);
    invalid_color_picker = color_picker;
    invalid_color_picker.changed_callback = nullptr;
    ERUI_TEST_CHECK(registry.add_color_picker(
        handle, root, &invalid_color_picker, nullptr) ==
        ERUI_INVALID_ARGUMENT);

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
    ERUI_ButtonDesc camera_button = button;
    camera_button.label = utf16(L"Registry Camera");
    ERUI_TEST_CHECK(registry.add_button(
        handle, camera_page, &camera_button, nullptr) == ERUI_OK);
    ERUI_ButtonDesc sound_button = button;
    sound_button.label = utf16(L"Registry Sound");
    ERUI_TEST_CHECK(registry.add_button(
        handle, sound_page, &sound_button, nullptr) == ERUI_OK);

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
    ERUI_SubmenuDesc camera_submenu = submenu;
    camera_submenu.label = utf16(L"Camera Advanced");
    camera_submenu.page_title = utf16(L"Camera Child");
    ERUI_PageHandle camera_child{};
    ERUI_TEST_CHECK(registry.add_submenu(
        handle,
        camera_page,
        &camera_submenu,
        &camera_child,
        nullptr) == ERUI_OK);

    FormatterObservation formatter_observation{};
    ERUI_PagePresentationDesc presentation{};
    presentation.size = sizeof(presentation);
    presentation.menu_title = utf16(L"Provider Outer");
    presentation.page_title = utf16(L"Override Base");
    presentation.formatter = &format_page_title;
    presentation.user_data = &formatter_observation;
    ERUI_TEST_CHECK(registry.set_page_presentation(
        handle, root, &presentation) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.set_page_presentation(
        handle, camera_page, &presentation) == ERUI_INVALID_ARGUMENT);
    ERUI_PagePresentationDesc camera_child_presentation{};
    camera_child_presentation.size = sizeof(camera_child_presentation);
    camera_child_presentation.page_title = utf16(L"Camera Child Override");
    ERUI_TEST_CHECK(registry.set_page_presentation(
        handle, camera_child, &camera_child_presentation) == ERUI_OK);
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
    ERUI_PageHandle committed_builtin{UINT64_C(0xA55AA55AA55AA55A)};
    ERUI_TEST_CHECK(registry.get_builtin_page(
        handle,
        ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
        &committed_builtin) == ERUI_ALREADY_COMMITTED);
    ERUI_TEST_CHECK(committed_builtin == ERUI_INVALID_PAGE);
    ERUI_TEST_CHECK(registry.committed_provider_count() == 1);
    ERUI_TEST_CHECK(registry.committed_root_row_count() == 8);

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

    ERUI_Color current_color{};
    ERUI_TEST_CHECK(registry.get_color_picker_value(
        handle, color_picker_row, &current_color) == ERUI_OK);
    ERUI_TEST_CHECK(current_color.red == 171u);
    ERUI_TEST_CHECK(current_color.green == 125u);
    ERUI_TEST_CHECK(current_color.blue == 99u);
    ERUI_TEST_CHECK(current_color.reserved == 0u);
    const ERUI_Color programmed_color{7u, 8u, 9u, 0u};
    ERUI_TEST_CHECK(registry.set_color_picker_value(
        handle, color_picker_row, &programmed_color) == ERUI_OK);
    ERUI_TEST_CHECK(color_observation.calls == 0);
    ERUI_TEST_CHECK(registry.get_color_picker_value(
        handle, color_picker_row, &current_color) == ERUI_OK);
    ERUI_TEST_CHECK(current_color.red == 7u && current_color.green == 8u &&
        current_color.blue == 9u && current_color.reserved == 0u);
    ERUI_Color invalid_programmed_color{1u, 2u, 3u, 1u};
    ERUI_TEST_CHECK(registry.set_color_picker_value(
        handle, color_picker_row, &invalid_programmed_color) ==
        ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.set_color_picker_value(
        handle, toggle_row, &programmed_color) == ERUI_INVALID_ARGUMENT);
    ERUI_TEST_CHECK(registry.get_color_picker_value(
        handle, toggle_row, &current_color) == ERUI_INVALID_ARGUMENT);

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
    // Game Options root so the ordinary pagination compiler is exercised.
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
        ERUI_PageHandle provider_camera{};
        ERUI_TEST_CHECK(registry.get_builtin_page(
            provider_handle,
            ERUI_BUILTIN_PAGE_CAMERA_OPTIONS,
            &provider_camera) == ERUI_OK);
        ERUI_ButtonDesc camera_row = button;
        camera_row.label = utf16(name);
        ERUI_TEST_CHECK(registry.add_button(
            provider_handle, provider_camera, &camera_row, nullptr) ==
            ERUI_OK);
        ERUI_TEST_CHECK(registry.commit_provider(provider_handle) == ERUI_OK);
    };
    add_provider("late-priority", L"Last Provider", 20, 5);
    add_provider("first-priority", L"First Provider", 5, 1);

    ERUI_ProviderDesc active_draft = provider;
    active_draft.provider_id = ascii("active-draft");
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
    ERUI_TEST_CHECK(menu->root().row_count() == 14);
    auto compiled = erui::detail::MenuCompiler::compile(*menu);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == 14);
    ERUI_TEST_CHECK(compiled->modeled_popup_choice_count == 1);
    ERUI_TEST_CHECK(compiled->modeled_text_input_count == 1);
    ERUI_TEST_CHECK(compiled->modeled_color_picker_count == 1);
    ERUI_TEST_CHECK(compiled->modeled_input_binding_count == 2);
    ERUI_TEST_CHECK(compiled->input_binding_sections.size() == 2);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled->input_binding_sections[0].label_id)) ==
        L"Registry Bindings");
    ERUI_TEST_CHECK(compiled->input_binding_sections[0].bindings.size() == 1);
    ERUI_TEST_CHECK(compiled->input_binding_sections[0].bindings[0].binding_id ==
        "show-dialog");
    compiled->input_binding_sections[0].bindings[0].action.invoke(
        ERUI_INPUT_DEVICE_CONTROLLER | ERUI_INPUT_DEVICE_MOUSE);
    ERUI_TEST_CHECK(binding_observation.calls == 1);
    ERUI_TEST_CHECK(binding_observation.provider == handle);
    ERUI_TEST_CHECK(binding_observation.action == binding_handle);
    ERUI_TEST_CHECK(binding_observation.devices ==
        (ERUI_INPUT_DEVICE_CONTROLLER | ERUI_INPUT_DEVICE_MOUSE));
    ERUI_TEST_CHECK(compiled->builtin_page_index(0u) ==
        compiled->root_page_index);
    const erui::detail::CompiledPage* compiled_camera =
        compiled->builtin_page(1u);
    ERUI_TEST_CHECK(compiled_camera != nullptr);
    ERUI_TEST_CHECK(compiled_camera->rows.size() == 4);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_camera->rows[0].label_id)) == L"First Provider");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_camera->rows[1].label_id)) == L"Registry Camera");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_camera->rows[2].label_id)) == L"Camera Advanced");
    ERUI_TEST_CHECK(compiled_camera->rows[2].kind == erui::RowKind::submenu);
    ERUI_TEST_CHECK(compiled->page_reachable(
        compiled_camera->rows[2].target_page_index));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled->pages[compiled_camera->rows[2].target_page_index].title_id)) ==
        L"Camera Child Override");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_camera->rows[3].label_id)) == L"Last Provider");
    ERUI_TEST_CHECK(compiled->page_reachable(
        compiled->builtin_page_index(1u)));
    ERUI_TEST_CHECK(compiled->page_plans[
        compiled->builtin_page_index(1u)].slices.empty());
    const erui::detail::CompiledPage* compiled_sound =
        compiled->builtin_page(3u);
    ERUI_TEST_CHECK(compiled_sound != nullptr);
    ERUI_TEST_CHECK(compiled_sound->rows.size() == 1);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_sound->rows[0].label_id)) == L"Registry Sound");
    ERUI_TEST_CHECK(compiled->builtin_page(4u) == nullptr);
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
    const auto compiled_color_picker = std::find_if(
        compiled->root_page().rows.begin(),
        compiled->root_page().rows.end(),
        [](const erui::detail::CompiledRow& row) {
            return row.kind == erui::RowKind::color_picker;
        });
    ERUI_TEST_CHECK(compiled_color_picker != compiled->root_page().rows.end());
    ERUI_TEST_CHECK(compiled_color_picker->color_picker_state != nullptr);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_color_picker->label_id)) == L"Accent Color");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_color_picker->help_id)) == L"ColorPicker help");
    ERUI_TEST_CHECK(compiled_color_picker->color_picker_state->value() ==
        (erui::detail::RgbColor{7u, 8u, 9u}));
    erui::detail::RgbColor confirmed_color{};
    ERUI_TEST_CHECK(compiled_color_picker->color_picker_state->accept_native(
        erui::detail::pack_native_color({40u, 50u, 60u}),
        confirmed_color));
    compiled_color_picker->color_action.invoke(confirmed_color);
    ERUI_TEST_CHECK(color_observation.calls == 1);
    ERUI_TEST_CHECK(color_observation.provider == handle);
    ERUI_TEST_CHECK(color_observation.row == color_picker_row);
    ERUI_TEST_CHECK(color_observation.value.red == 40u);
    ERUI_TEST_CHECK(color_observation.value.green == 50u);
    ERUI_TEST_CHECK(color_observation.value.blue == 60u);
    ERUI_TEST_CHECK(color_observation.value.reserved == 0u);
    ERUI_TEST_CHECK(!compiled_color_picker->color_picker_state->accept_native(
        erui::detail::pack_native_color({40u, 50u, 60u}),
        confirmed_color));
    ERUI_TEST_CHECK(color_observation.calls == 1);
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
    // This fixture does not enable the row-visibility backend, so the frozen
    // API 1.0-1.3 formatter contract remains exactly one call per physical
    // slice in the original full-row plan.
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
    const auto collapsed_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::submenu(child_page_index, 0), 1);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        collapsed_presentation.outer_title_id)) == L"Provider Outer");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        collapsed_presentation.page_title_id)) == L"Formatted 1/2");
    const auto removed_continuation_presentation =
        compiled->resolve_presentation(
            erui::detail::PageRoute::submenu(child_page_index, 1), 1);
    ERUI_TEST_CHECK(removed_continuation_presentation.outer_title_id == 0);
    ERUI_TEST_CHECK(removed_continuation_presentation.page_title_id == 0);
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
            erui::detail::game_options_vanilla_capacity));
    ERUI_TEST_CHECK(root_main_presentation.outer_title_id == 0);
    ERUI_TEST_CHECK(root_main_presentation.page_title_id == 0);
    const auto root_continuation_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::root_continuation(
            compiled->root_page_index,
            erui::detail::game_options_vanilla_capacity,
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
