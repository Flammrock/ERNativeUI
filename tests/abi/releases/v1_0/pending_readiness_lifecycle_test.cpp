#include "host_loader.h"
#include "pending_v1_0_control.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <future>
#include <string_view>

namespace {

using namespace std::chrono_literals;

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

void ERUI_CALL changed(void*, std::uint8_t) noexcept {}

template <typename Function>
Function load_export(HMODULE module, const char* name) noexcept {
    const FARPROC address = GetProcAddress(module, name);
    static_assert(sizeof(Function) == sizeof(address));
    Function function{};
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

struct LanguageCall {
    ERUI_Result result{ERUI_INTERNAL_ERROR};
    ERUI_GameLanguageInfo language{};
};

} // namespace

int main(int argc, char** argv) {
    static_assert(ERUI_API_VERSION_CURRENT == ERUI_API_VERSION_1_0);
    static_assert(sizeof(ERUI_Api) == 128u);
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != nullptr);

    const auto settle_language = load_export<ERUI_TestSettleLanguageFn>(
        host.module, "ERUI_TestSettleLanguage");
    const auto set_runtime_ready = load_export<ERUI_TestSetRuntimeReadyFn>(
        host.module, "ERUI_TestSetRuntimeReady");
    CHECK(settle_language != nullptr);
    CHECK(set_runtime_ready != nullptr);

    // A frozen 1.0 client must still obtain its exact 128-byte table while
    // Steam language discovery is pending.
    ERUI_Api api{};
    api.size = ERUI_API_V1_0_SIZE;
    CHECK(host.get_api(ERUI_API_VERSION_1_0, &api) == ERUI_OK);
    CHECK(api.api_version == ERUI_API_VERSION_1_0);
    CHECK(api.size == ERUI_API_V1_0_SIZE);
    CHECK(api.get_game_language != nullptr);

    // The old wrapper queries language immediately after negotiation. It
    // must wait for the one stable snapshot instead of observing a transient
    // HOST_NOT_READY result and abandoning registration.
    std::atomic<bool> language_call_started{};
    auto language_future = std::async(std::launch::async, [&] {
        LanguageCall call{};
        call.language.size = sizeof(call.language);
        language_call_started.store(true, std::memory_order_release);
        call.result = api.get_game_language(&call.language);
        return call;
    });
    while (!language_call_started.load(std::memory_order_acquire)) Sleep(1);
    const auto language_before_settlement = language_future.wait_for(100ms);

    // Always release the worker before evaluating the timeout assertion so a
    // failed check cannot strand a test thread inside the production wait.
    const ERUI_Result settled = settle_language("french");
    const auto language_after_settlement = language_future.wait_for(2s);
    CHECK(language_before_settlement == std::future_status::timeout);
    CHECK(settled == ERUI_OK);
    CHECK(language_after_settlement == std::future_status::ready);
    const LanguageCall language = language_future.get();
    CHECK(language.result == ERUI_OK);
    CHECK(language.language.known_language == ERUI_GAME_LANGUAGE_FRENCH);
    CHECK(std::string_view(
        language.language.identifier.data,
        language.language.identifier.length) == "french");

    ERUI_ProviderDesc provider{};
    provider.size = sizeof(provider);
    provider.api_version = ERUI_API_VERSION_1_0;
    provider.owner_module = GetModuleHandleW(nullptr);
    provider.provider_id = string_view(
        "tests.v1-0-pending-readiness-lifecycle");
    provider.display_name = utf16_view(L"Frozen v1.0 lifecycle client");

    ERUI_ProviderHandle provider_handle{ERUI_INVALID_PROVIDER};
    ERUI_PageHandle root{ERUI_INVALID_PAGE};
    CHECK(api.register_provider(
        &provider, &provider_handle, &root) == ERUI_OK);

    ERUI_ToggleDesc toggle{};
    toggle.size = sizeof(toggle);
    toggle.label = utf16_view(L"Frozen lifecycle toggle");
    toggle.help = utf16_view(L"Provider survives pending readiness");
    toggle.changed_callback = &changed;
    toggle.initial_value = 1u;
    toggle.enabled = 1u;
    ERUI_RowHandle row{ERUI_INVALID_ROW};
    CHECK(api.add_toggle(provider_handle, root, &toggle, &row) == ERUI_OK);

    // A public 1.0 commit keeps its historical synchronous behavior: it
    // returns only once the native runtime is ready (or has failed).
    std::atomic<bool> commit_started{};
    auto commit_future = std::async(std::launch::async, [&] {
        commit_started.store(true, std::memory_order_release);
        return api.commit_provider(provider_handle);
    });
    while (!commit_started.load(std::memory_order_acquire)) Sleep(1);
    const auto commit_before_runtime = commit_future.wait_for(100ms);

    // As above, release the production wait before asserting the observation.
    set_runtime_ready();
    const auto commit_after_runtime = commit_future.wait_for(2s);
    CHECK(commit_before_runtime == std::future_status::timeout);
    CHECK(commit_after_runtime == std::future_status::ready);
    CHECK(commit_future.get() == ERUI_OK);

    std::uint8_t value{};
    CHECK(api.get_row_value(provider_handle, row, &value) == ERUI_OK);
    CHECK(value == 1u);

    erui_unload_test_host(&host);
    return 0;
}
