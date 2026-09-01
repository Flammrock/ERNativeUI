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
        CHECK(api_v1_1.abort_provider(text_provider) == ERUI_OK);
    }
#endif

    erui_unload_test_host(&host);
    return 0;
}
