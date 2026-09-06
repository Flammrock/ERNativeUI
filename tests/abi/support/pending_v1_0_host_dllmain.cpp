#include "pending_v1_0_control.h"

#include "host_registry.hpp"
#include "steam_language.hpp"

#include <Windows.h>

#include <utility>

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        // Match the production startup interval after registration opens but
        // before Steam language discovery and native runtime setup settle.
        erui::host::registry().open_registration();
        erui::host::set_api_state(erui::host::ApiState::accepting);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) ERUI_Result ERUI_CALL
ERUI_TestSettleLanguage(const char* identifier) {
    if (!identifier || identifier[0] == '\0') return ERUI_INVALID_ARGUMENT;

    erui::host::SteamLanguageProbe language{};
    language.available = true;
    language.current = identifier;
    return erui::host::settle_cached_steam_language(std::move(language));
}

extern "C" __declspec(dllexport) void ERUI_CALL
ERUI_TestSetRuntimeReady(void) {
    erui::host::set_api_state(erui::host::ApiState::runtime_ready);
}
