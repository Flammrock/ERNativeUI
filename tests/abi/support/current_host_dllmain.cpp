#include "host_registry.hpp"

#include <Windows.h>

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        erui::host::registry().open_registration();
        erui::host::set_api_state(erui::host::ApiState::runtime_ready);
    }
    return TRUE;
}
