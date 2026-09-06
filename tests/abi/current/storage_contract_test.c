#include "host_loader.h"

#include <Windows.h>

#include <stdint.h>
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

static ERUI_StorageKey storage_key(const char* section, const char* key) {
    ERUI_StorageKey result = {0};
    result.size = (uint32_t)sizeof(result);
    result.section = string_view(section);
    result.key = string_view(key);
    return result;
}

int main(int argc, char** argv) {
    ERUI_LoadedTestHost host = {0};
    ERUI_Api api = {0};
    wchar_t temporary_directory[MAX_PATH] = {0};
    wchar_t path[MAX_PATH] = {0};
    ERUI_ProviderDesc provider_description = {0};
    ERUI_ProviderHandle provider = ERUI_INVALID_PROVIDER;
    ERUI_PageHandle root = ERUI_INVALID_PAGE;
    ERUI_StorageDesc storage_description = {0};
    ERUI_StorageHandle storage = ERUI_INVALID_STORAGE;
    ERUI_StorageInfo info = {0};
    ERUI_StorageKey text_key;
    ERUI_StorageKey action_key;

    if (argc != 2) return 2;
    CHECK(erui_load_test_host(argv[1], &host));
    api.size = ERUI_API_V1_1_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_1, &api) == ERUI_OK);
    CHECK((api.capabilities & ERUI_CAP_STORAGE) != 0u);
    CHECK(api.open_storage != NULL);
    CHECK(api.storage_load != NULL);
    CHECK(api.storage_save != NULL);
    CHECK(api.storage_get_utf8 != NULL);
    CHECK(api.storage_set_utf8 != NULL);
    CHECK(api.storage_erase != NULL);
    CHECK(api.storage_get_action_inputs != NULL);
    CHECK(api.storage_set_action_inputs != NULL);
    CHECK(api.storage_apply_assignment_changes != NULL);
    CHECK(api.storage_get_info != NULL);

    CHECK(GetTempPathW(MAX_PATH, temporary_directory) != 0u);
    CHECK(_snwprintf_s(
        path,
        MAX_PATH,
        _TRUNCATE,
        L"%sERNativeUI-abi-storage-%lu.ini",
        temporary_directory,
        (unsigned long)GetCurrentProcessId()) > 0);
    DeleteFileW(path);

    provider_description.size = (uint32_t)sizeof(provider_description);
    provider_description.api_version = ERUI_API_VERSION_1_1;
    provider_description.owner_module = GetModuleHandleW(NULL);
    provider_description.provider_id = string_view(
        "tests.current-v1-1-storage-c");
    provider_description.display_name = utf16_view(L"Storage ABI contract");
    CHECK(api.register_provider(
        &provider_description, &provider, &root) == ERUI_OK);

    storage_description.size = (uint32_t)sizeof(storage_description);
    storage_description.location = ERUI_STORAGE_LOCATION_EXPLICIT_ABSOLUTE;
    storage_description.path = utf16_view(path);
    CHECK(api.open_storage(
        provider, &storage_description, &storage) == ERUI_OK);
    CHECK(storage != ERUI_INVALID_STORAGE);
    {
        ERUI_StorageHandle repeated = ERUI_INVALID_STORAGE;
        CHECK(api.open_storage(
            provider, &storage_description, &repeated) == ERUI_OK);
        CHECK(repeated == storage);
    }

    info.size = (uint32_t)sizeof(info);
    CHECK(api.storage_get_info(provider, storage, &info) == ERUI_OK);
    CHECK(info.loaded == 0u && info.dirty == 0u);
    text_key = storage_key("settings", "name");
    {
        const ERUI_StringView value = string_view("Tarnished");
        CHECK(api.storage_set_utf8(
            provider, storage, &text_key, &value) ==
            ERUI_STORAGE_NOT_LOADED);
    }
    CHECK(api.storage_save(provider, storage) == ERUI_STORAGE_NOT_LOADED);
    CHECK(api.storage_load(provider, storage) == ERUI_OK);
    CHECK(api.storage_load(provider, storage) == ERUI_STORAGE_ALREADY_LOADED);

    {
        uint32_t untouched_length = UINT32_C(0xA5A5A5A5);
        CHECK(api.storage_get_utf8(
            provider, storage, &text_key, NULL, 0, &untouched_length) ==
            ERUI_NOT_FOUND);
        CHECK(untouched_length == UINT32_C(0xA5A5A5A5));
    }
    {
        const ERUI_StringView value = string_view("Tarnished");
        uint32_t required = 0;
        char small[4] = {(char)0x5A, (char)0x5A, (char)0x5A, (char)0x5A};
        char output[16] = {0};
        CHECK(api.storage_set_utf8(
            provider, storage, &text_key, &value) == ERUI_OK);
        CHECK(api.storage_get_utf8(
            provider, storage, &text_key, NULL, 0, &required) == ERUI_OK);
        CHECK(required == 9u);
        CHECK(api.storage_get_utf8(
            provider, storage, &text_key, small, 4, &required) ==
            ERUI_BUFFER_TOO_SMALL);
        CHECK(small[0] == (char)0x5A && small[3] == (char)0x5A);
        CHECK(api.storage_get_utf8(
            provider, storage, &text_key, output, 16, &required) == ERUI_OK);
        CHECK(required == 9u && memcmp(output, "Tarnished", 9) == 0);
    }

    action_key = storage_key("bindings", "quick-action");
    {
        ERUI_ActionInputs value = {0};
        ERUI_ActionInputs output = {0};
        value.size = (uint32_t)sizeof(value);
        value.controller.state = ERUI_INPUT_SLOT_BOUND;
        value.controller.input = ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER;
        value.keyboard.state = ERUI_INPUT_SLOT_BOUND;
        value.keyboard.input = ERUI_KEYBOARD_KEY_Q;
        CHECK(api.storage_set_action_inputs(
            provider, storage, &action_key, &value) == ERUI_OK);
        output.size = (uint32_t)sizeof(output);
        CHECK(api.storage_get_action_inputs(
            provider, storage, &action_key, &output) == ERUI_OK);
        CHECK(memcmp(&value, &output, sizeof(value)) == 0);

        {
            ERUI_AssignmentChange change = {0};
            const ERUI_StringView section = string_view("bindings");
            change.size = (uint32_t)sizeof(change);
            change.action = UINT64_C(55);
            change.action_id = string_view("quick-action");
            change.previous = value;
            change.current = value;
            change.current.keyboard.state = ERUI_INPUT_SLOT_UNBOUND;
            change.current.keyboard.input = ERUI_KEYBOARD_KEY_INVALID;
            change.reason = ERUI_ASSIGNMENT_CHANGE_PLAYER_CLEAR;
            change.changed_devices = ERUI_INPUT_DEVICE_KEYBOARD;
            CHECK(api.storage_apply_assignment_changes(
                provider, storage, &section, &change, 1) == ERUI_OK);
        }
        output.size = (uint32_t)sizeof(output);
        CHECK(api.storage_get_action_inputs(
            provider, storage, &action_key, &output) == ERUI_OK);
        CHECK(output.controller.state == ERUI_INPUT_SLOT_BOUND);
        CHECK(output.controller.input ==
            ERUI_CONTROLLER_BUTTON_RIGHT_TRIGGER);
        CHECK(output.keyboard.state == ERUI_INPUT_SLOT_UNBOUND);
        CHECK(output.keyboard.input == ERUI_KEYBOARD_KEY_INVALID);
    }

    info.size = (uint32_t)sizeof(info);
    CHECK(api.storage_get_info(provider, storage, &info) == ERUI_OK);
    CHECK(info.loaded == 1u && info.dirty == 1u);
    CHECK(info.current_revision > info.last_saved_revision);
    CHECK(api.storage_save(provider, storage) == ERUI_OK);
    info.size = (uint32_t)sizeof(info);
    CHECK(api.storage_get_info(provider, storage, &info) == ERUI_OK);
    CHECK(info.dirty == 0u);
    CHECK(info.current_revision == info.last_saved_revision);

    CHECK(api.storage_erase(provider, storage, &text_key) == ERUI_OK);
    CHECK(api.storage_erase(provider, storage, &text_key) == ERUI_OK);
    /* Erase is idempotent in the public storage contract. */
    CHECK(api.storage_save(provider, storage) == ERUI_OK);
    CHECK(api.abort_provider(provider) == ERUI_OK);
    erui_unload_test_host(&host);
    CHECK(DeleteFileW(path) != 0);
    return 0;
}
