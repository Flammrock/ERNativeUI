#include "guarded_buffer_win32.hpp"
#include "host_loader.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

ERUI_StringView string_view(const char* value) noexcept {
    ERUI_StringView result{};
    result.data = value;
    result.length = static_cast<std::uint32_t>(std::strlen(value));
    return result;
}

ERUI_Utf16View utf16_view(const wchar_t* value) noexcept {
    ERUI_Utf16View result{};
    result.data = reinterpret_cast<const std::uint16_t*>(value);
    result.length = static_cast<std::uint32_t>(std::wcslen(value));
    return result;
}

void ERUI_CALL binding_activated(
    void*,
    const ERUI_InputActionActivatedContext*) noexcept {}

void ERUI_CALL assignments_changed(
    void*,
    const ERUI_AssignmentsChangedContext*) noexcept {}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != nullptr);
    if (host.get_control) {
        ERUI_CompatControl control{};
        control.size = sizeof(control);
        CHECK(host.get_control(
            ERUI_COMPAT_CONTROL_VERSION, &control) == ERUI_OK);
        control.reset();
    }

    ERUI_Api api{};
    api.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &api) == ERUI_OK);

    erui::abi_test::GuardedObject<ERUI_ProviderDesc> guarded;
    ERUI_ProviderDesc& description = *guarded;
    description.size = static_cast<std::uint32_t>(
        sizeof(ERUI_ProviderDesc) - 1u);
    description.api_version = ERUI_API_VERSION_1_0;
    description.owner_module = GetModuleHandleW(nullptr);
    description.provider_id = string_view("tests.guard-v1-0");
    description.display_name = utf16_view(L"Guarded v1.0 provider");

    ERUI_ProviderHandle provider = UINT64_C(0xAAAAAAAAAAAAAAAA);
    ERUI_PageHandle root = UINT64_C(0xBBBBBBBBBBBBBBBB);
    CHECK(api.register_provider(&description, &provider, &root) ==
        ERUI_INVALID_ARGUMENT);
    CHECK(provider == ERUI_INVALID_PROVIDER);
    CHECK(root == ERUI_INVALID_PAGE);

    description.size = sizeof(ERUI_ProviderDesc);
    CHECK(api.register_provider(&description, &provider, &root) == ERUI_OK);
    CHECK(provider != ERUI_INVALID_PROVIDER);
    CHECK(root != ERUI_INVALID_PAGE);

#if defined(ERUI_API_VERSION_1_1)
    ERUI_Api api_v1_1{};
    api_v1_1.size = ERUI_API_V1_1_SIZE;
    const ERUI_Result v1_1_result = host.get_api(
        ERUI_API_VERSION_1_1, &api_v1_1);
    CHECK(v1_1_result == ERUI_OK ||
        v1_1_result == ERUI_UNSUPPORTED_VERSION);
    if (v1_1_result == ERUI_OK) {
        CHECK((api_v1_1.capabilities & ERUI_CAP_TEXT_INPUT) != 0);
        CHECK(api_v1_1.add_text_input != nullptr);
        CHECK((api_v1_1.capabilities & ERUI_CAP_COLOR_PICKER) != 0);
        CHECK(api_v1_1.add_color_picker != nullptr);
        CHECK((api_v1_1.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0);
        CHECK(api_v1_1.add_input_section != nullptr);
        CHECK(api_v1_1.add_input_action != nullptr);

        ERUI_ProviderDesc invalid_id_description{};
        invalid_id_description.size = sizeof(invalid_id_description);
        invalid_id_description.api_version = ERUI_API_VERSION_1_1;
        invalid_id_description.owner_module = GetModuleHandleW(nullptr);
        invalid_id_description.provider_id = string_view("invalid/provider");
        invalid_id_description.display_name =
            utf16_view(L"Invalid provider ID");
        ERUI_ProviderHandle invalid_id_provider =
            UINT64_C(0xAAAAAAAAAAAAAAAA);
        ERUI_PageHandle invalid_id_root = UINT64_C(0xBBBBBBBBBBBBBBBB);
        CHECK(api_v1_1.register_provider(
            &invalid_id_description,
            &invalid_id_provider,
            &invalid_id_root) == ERUI_INVALID_ARGUMENT);
        CHECK(invalid_id_provider == ERUI_INVALID_PROVIDER);
        CHECK(invalid_id_root == ERUI_INVALID_PAGE);

        // A provider negotiated through 1.0 must be rejected before either
        // 1.1-only descriptor or output pointer is dereferenced.
        erui::abi_test::GuardedObject<ERUI_InputSectionDesc>
            unreadable_section;
        auto* const no_access_section = unreadable_section.get() + 1;
        CHECK(api_v1_1.add_input_section(
            provider,
            no_access_section,
            reinterpret_cast<ERUI_InputSectionHandle*>(
                no_access_section)) == ERUI_NOT_SUPPORTED);
        erui::abi_test::GuardedObject<ERUI_InputActionDesc>
            unreadable_binding;
        auto* const no_access_binding = unreadable_binding.get() + 1;
        CHECK(api_v1_1.add_input_action(
            provider,
            ERUI_INVALID_INPUT_SECTION,
            no_access_binding,
            reinterpret_cast<ERUI_InputActionHandle*>(
                no_access_binding)) == ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_PageHandle> unreadable_page;
        auto* const no_access_page = unreadable_page.get() + 1;
        CHECK(api_v1_1.get_builtin_page(
            provider, ERUI_BUILTIN_PAGE_SOUND, no_access_page) ==
            ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_TextInputDesc> unreadable_text;
        auto* const no_access_text = unreadable_text.get() + 1;
        CHECK(api_v1_1.add_text_input(
            provider, root, no_access_text,
            reinterpret_cast<ERUI_RowHandle*>(no_access_text)) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.set_text_input_value(
            provider, ERUI_INVALID_ROW,
            reinterpret_cast<const ERUI_Utf16View*>(no_access_text)) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.get_text_input_value(
            provider, ERUI_INVALID_ROW,
            reinterpret_cast<std::uint16_t*>(no_access_text), 1,
            reinterpret_cast<std::uint32_t*>(no_access_text)) ==
            ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_ColorPickerDesc>
            unreadable_color;
        auto* const no_access_color = unreadable_color.get() + 1;
        CHECK(api_v1_1.add_color_picker(
            provider, root, no_access_color,
            reinterpret_cast<ERUI_RowHandle*>(no_access_color)) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.set_color_picker_value(
            provider, ERUI_INVALID_ROW,
            reinterpret_cast<const ERUI_Color*>(no_access_color)) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.get_color_picker_value(
            provider, ERUI_INVALID_ROW,
            reinterpret_cast<ERUI_Color*>(no_access_color)) ==
            ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_AssignmentsChangedHandlerDesc>
            unreadable_handler;
        auto* const no_access_handler = unreadable_handler.get() + 1;
        CHECK(api_v1_1.set_assignments_changed_handler(
            provider, no_access_handler) == ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_ActionInputs>
            unreadable_inputs;
        auto* const no_access_inputs = unreadable_inputs.get() + 1;
        CHECK(api_v1_1.set_action_inputs(
            provider, ERUI_INVALID_INPUT_ACTION, no_access_inputs) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.get_action_inputs(
            provider, ERUI_INVALID_INPUT_ACTION, no_access_inputs) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.get_action_default_inputs(
            provider, ERUI_INVALID_INPUT_ACTION, no_access_inputs) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.reset_action_inputs(
            provider, ERUI_INVALID_INPUT_ACTION,
            ERUI_INPUT_DEVICE_ALL) == ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_StorageDesc>
            unreadable_storage_description;
        auto* const no_access_storage_description =
            unreadable_storage_description.get() + 1;
        erui::abi_test::GuardedObject<ERUI_StorageHandle>
            unreadable_storage_handle;
        auto* const no_access_storage_handle =
            unreadable_storage_handle.get() + 1;
        CHECK(api_v1_1.open_storage(
            provider, no_access_storage_description,
            no_access_storage_handle) == ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_load(
            provider, ERUI_INVALID_STORAGE) == ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_save(
            provider, ERUI_INVALID_STORAGE) == ERUI_NOT_SUPPORTED);


        erui::abi_test::GuardedObject<ERUI_StorageKey>
            unreadable_storage_key;
        auto* const no_access_storage_key =
            unreadable_storage_key.get() + 1;
        erui::abi_test::GuardedObject<ERUI_StringView>
            unreadable_string_view;
        auto* const no_access_string_view =
            unreadable_string_view.get() + 1;
        CHECK(api_v1_1.storage_get_utf8(
            provider, ERUI_INVALID_STORAGE, no_access_storage_key,
            reinterpret_cast<char*>(no_access_storage_key), 1,
            reinterpret_cast<std::uint32_t*>(no_access_storage_key)) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_set_utf8(
            provider, ERUI_INVALID_STORAGE, no_access_storage_key,
            no_access_string_view) == ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_erase(
            provider, ERUI_INVALID_STORAGE, no_access_storage_key) ==
            ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_get_action_inputs(
            provider, ERUI_INVALID_STORAGE, no_access_storage_key,
            no_access_inputs) == ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_set_action_inputs(
            provider, ERUI_INVALID_STORAGE, no_access_storage_key,
            no_access_inputs) == ERUI_NOT_SUPPORTED);
        CHECK(api_v1_1.storage_apply_assignment_changes(
            provider, ERUI_INVALID_STORAGE, no_access_string_view,
            reinterpret_cast<const ERUI_AssignmentChange*>(
                no_access_storage_key), 1) == ERUI_NOT_SUPPORTED);
        erui::abi_test::GuardedObject<ERUI_StorageInfo>
            unreadable_storage_info;
        CHECK(api_v1_1.storage_get_info(
            provider, ERUI_INVALID_STORAGE,
            unreadable_storage_info.get() + 1) == ERUI_NOT_SUPPORTED);


        ERUI_ProviderDesc text_provider_description{};
        text_provider_description.size = sizeof(text_provider_description);
        text_provider_description.api_version = ERUI_API_VERSION_1_1;
        text_provider_description.owner_module = GetModuleHandleW(nullptr);
        text_provider_description.provider_id = string_view(
            "tests.guard-v1-1-text-input");
        text_provider_description.display_name = utf16_view(
            L"Guarded v1.1 TextInput provider");

        ERUI_ProviderHandle text_provider{};
        ERUI_PageHandle text_root{};
        CHECK(api_v1_1.register_provider(
            &text_provider_description, &text_provider, &text_root) == ERUI_OK);

        erui::abi_test::GuardedObject<ERUI_TextInputDesc> guarded_text;
        ERUI_TextInputDesc& text = *guarded_text;
        text.size = static_cast<std::uint32_t>(
            sizeof(ERUI_TextInputDesc) - 1u);
        text.label = utf16_view(L"Guarded TextInput");
        text.initial_value = utf16_view(L"ABC");
        text.placeholder = utf16_view(L"Enter text");
        text.maximum_length = 3;

        ERUI_RowHandle text_row = UINT64_C(0xCCCCCCCCCCCCCCCC);
        CHECK(api_v1_1.add_text_input(
            text_provider, text_root, &text, &text_row) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(text_row == ERUI_INVALID_ROW);

        text.size = sizeof(ERUI_TextInputDesc);
        CHECK(api_v1_1.add_text_input(
            text_provider, text_root, &text, &text_row) == ERUI_OK);
        CHECK(text_row != ERUI_INVALID_ROW);

        erui::abi_test::GuardedObject<ERUI_ColorPickerDesc> guarded_color;
        ERUI_ColorPickerDesc& color = *guarded_color;
        color.size = static_cast<std::uint32_t>(
            sizeof(ERUI_ColorPickerDesc) - 1u);
        color.label = utf16_view(L"Guarded ColorPicker");
        color.initial_value = ERUI_Color{10u, 20u, 30u, 0u};
        color.enabled = 1u;

        ERUI_RowHandle color_row = UINT64_C(0xDDDDDDDDDDDDDDDD);
        CHECK(api_v1_1.add_color_picker(
            text_provider, text_root, &color, &color_row) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(color_row == ERUI_INVALID_ROW);

        color.size = sizeof(ERUI_ColorPickerDesc);
        CHECK(api_v1_1.add_color_picker(
            text_provider, text_root, &color, &color_row) == ERUI_OK);
        CHECK(color_row != ERUI_INVALID_ROW);

        erui::abi_test::GuardedObject<ERUI_InputSectionDesc>
            guarded_section;
        ERUI_InputSectionDesc& binding_section = *guarded_section;
        binding_section.size = static_cast<std::uint32_t>(
            sizeof(ERUI_InputSectionDesc) - 1u);
        binding_section.label = utf16_view(L"Guarded bindings");
        ERUI_InputSectionHandle section = UINT64_C(0xEEEEEEEEEEEEEEEE);
        CHECK(api_v1_1.add_input_section(
            text_provider, &binding_section, &section) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(section == ERUI_INVALID_INPUT_SECTION);

        binding_section.size = sizeof(ERUI_InputSectionDesc);
        CHECK(api_v1_1.add_input_section(
            text_provider, &binding_section, &section) == ERUI_OK);
        CHECK(section != ERUI_INVALID_INPUT_SECTION);

        erui::abi_test::GuardedObject<ERUI_InputActionDesc> guarded_binding;
        ERUI_InputActionDesc& binding = *guarded_binding;
        binding.size = static_cast<std::uint32_t>(
            sizeof(ERUI_InputActionDesc) - 1u);
        binding.action_id = string_view("guarded-binding");
        binding.label = utf16_view(L"Guarded binding");
        binding.default_inputs.size = sizeof(binding.default_inputs);
        binding.default_inputs.keyboard.state = ERUI_INPUT_SLOT_UNBOUND;
        binding.activated_callback = &binding_activated;
        ERUI_InputActionHandle binding_handle = UINT64_C(0xFFFFFFFFFFFFFFFF);
        CHECK(api_v1_1.add_input_action(
            text_provider, section, &binding, &binding_handle) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(binding_handle == ERUI_INVALID_INPUT_ACTION);

        binding.size = sizeof(ERUI_InputActionDesc);
        CHECK(api_v1_1.add_input_action(
            text_provider, section, &binding, &binding_handle) == ERUI_OK);
        CHECK(binding_handle != ERUI_INVALID_INPUT_ACTION);

        erui::abi_test::GuardedObject<ERUI_ActionInputs> guarded_inputs;
        ERUI_ActionInputs& inputs = *guarded_inputs;
        inputs.size = static_cast<std::uint32_t>(sizeof(inputs) - 1u);
        inputs.keyboard.state = ERUI_INPUT_SLOT_BOUND;
        inputs.keyboard.input = ERUI_KEYBOARD_KEY_Q;
        CHECK(api_v1_1.set_action_inputs(
            text_provider, binding_handle, &inputs) == ERUI_INVALID_ARGUMENT);
        inputs.size = sizeof(inputs);
        CHECK(api_v1_1.set_action_inputs(
            text_provider, binding_handle, &inputs) == ERUI_OK);

        erui::abi_test::GuardedObject<ERUI_AssignmentsChangedHandlerDesc>
            guarded_handler;
        ERUI_AssignmentsChangedHandlerDesc& handler = *guarded_handler;
        handler.size = static_cast<std::uint32_t>(sizeof(handler) - 1u);
        handler.callback = &assignments_changed;
        CHECK(api_v1_1.set_assignments_changed_handler(
            text_provider, &handler) == ERUI_INVALID_ARGUMENT);
        handler.size = sizeof(handler);
        CHECK(api_v1_1.set_assignments_changed_handler(
            text_provider, &handler) == ERUI_OK);

        erui::abi_test::GuardedObject<ERUI_StorageDesc> guarded_storage;
        ERUI_StorageDesc& storage_description = *guarded_storage;
        storage_description.size = static_cast<std::uint32_t>(
            sizeof(storage_description) - 1u);
        storage_description.location = ERUI_STORAGE_LOCATION_PROVIDER_DEFAULT;
        ERUI_StorageHandle storage = UINT64_C(0xABABABABABABABAB);
        CHECK(api_v1_1.open_storage(
            text_provider, &storage_description, &storage) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(storage == ERUI_INVALID_STORAGE);
        storage_description.size = sizeof(storage_description);
        CHECK(api_v1_1.open_storage(
            text_provider, &storage_description, &storage) == ERUI_OK);
        CHECK(storage != ERUI_INVALID_STORAGE);
        CHECK(api_v1_1.storage_load(text_provider, storage) == ERUI_OK);

        erui::abi_test::GuardedObject<ERUI_StorageKey> guarded_key;
        ERUI_StorageKey& key = *guarded_key;
        key.size = static_cast<std::uint32_t>(sizeof(key) - 1u);
        key.section = string_view("bindings");
        key.key = string_view("guarded-binding");
        std::uint32_t untouched_length = 0xA5A5A5A5u;
        CHECK(api_v1_1.storage_get_utf8(
            text_provider, storage, &key, nullptr, 0, &untouched_length) ==
            ERUI_INVALID_ARGUMENT);
        CHECK(untouched_length == 0xA5A5A5A5u);
        key.size = sizeof(key);

        erui::abi_test::GuardedObject<ERUI_StorageInfo> guarded_info;
        ERUI_StorageInfo& info = *guarded_info;
        info.size = static_cast<std::uint32_t>(sizeof(info) - 1u);
        CHECK(api_v1_1.storage_get_info(
            text_provider, storage, &info) == ERUI_BUFFER_TOO_SMALL);
        info.size = sizeof(info);
        CHECK(api_v1_1.storage_get_info(
            text_provider, storage, &info) == ERUI_OK);
        CHECK(info.loaded == 1u);

        ERUI_ActionInputs stored_inputs{};
        stored_inputs.size = sizeof(stored_inputs);
        stored_inputs.controller.state = ERUI_INPUT_SLOT_BOUND;
        stored_inputs.controller.input = ERUI_CONTROLLER_BUTTON_FACE_SOUTH;
        CHECK(api_v1_1.storage_set_action_inputs(
            text_provider, storage, &key, &stored_inputs) == ERUI_OK);
        erui::abi_test::GuardedObject<ERUI_ActionInputs> guarded_output;
        ERUI_ActionInputs& output = *guarded_output;
        output.size = static_cast<std::uint32_t>(sizeof(output) - 1u);
        CHECK(api_v1_1.storage_get_action_inputs(
            text_provider, storage, &key, &output) == ERUI_BUFFER_TOO_SMALL);
        output.size = sizeof(output);
        CHECK(api_v1_1.storage_get_action_inputs(
            text_provider, storage, &key, &output) == ERUI_OK);
        CHECK(output.controller.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(output.controller.input == ERUI_CONTROLLER_BUTTON_FACE_SOUTH);

        CHECK(api_v1_1.abort_provider(text_provider) == ERUI_OK);
    }
#endif

    erui_unload_test_host(&host);
    return 0;
}
