#include "host_registry.hpp"
#include "steam_language.hpp"

#include <Windows.h>

#include <utility>

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        erui::host::SteamLanguageProbe language{};
        language.available = true;
        language.current = "english";
        (void)erui::host::settle_cached_steam_language(
            std::move(language));
        if (!erui::host::registry().enable_custom_tabs_backend()) {
            return FALSE;
        }
        if (!erui::host::registry().enable_title_screen_text_backend()) {
            return FALSE;
        }
        if (!erui::host::registry().enable_updates_backend()) {
            return FALSE;
        }
        if (!erui::host::registry().enable_row_availability_backend()) {
            return FALSE;
        }
        if (!erui::host::registry().enable_row_visibility_backend()) {
            return FALSE;
        }
        erui::host::registry().open_registration();
        erui::host::set_api_state(erui::host::ApiState::runtime_ready);
    }
    return TRUE;
}
