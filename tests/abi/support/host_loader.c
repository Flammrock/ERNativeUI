#include "host_loader.h"

#include <stdlib.h>
#include <string.h>

static FARPROC resolve_symbol(HMODULE module, const char* name) {
    return module ? GetProcAddress(module, name) : NULL;
}

int erui_load_test_host(
    const char* utf8_path,
    ERUI_LoadedTestHost* out_host) {
    int wide_count;
    wchar_t* wide_path;
    FARPROC symbol;

    if (!utf8_path || !out_host) return 0;
    memset(out_host, 0, sizeof(*out_host));

    wide_count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path, -1, NULL, 0);
    if (wide_count <= 0) return 0;
    wide_path = (wchar_t*)malloc((size_t)wide_count * sizeof(wchar_t));
    if (!wide_path) return 0;
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path, -1,
            wide_path, wide_count) != wide_count) {
        free(wide_path);
        return 0;
    }

    out_host->module = LoadLibraryW(wide_path);
    free(wide_path);
    if (!out_host->module) return 0;

    symbol = resolve_symbol(out_host->module, "ERUI_GetApi");
    if (!symbol || sizeof(symbol) != sizeof(out_host->get_api)) {
        erui_unload_test_host(out_host);
        return 0;
    }
    memcpy(&out_host->get_api, &symbol, sizeof(out_host->get_api));

    symbol = resolve_symbol(out_host->module, "ERUI_CompatGetControl");
    if (symbol && sizeof(symbol) == sizeof(out_host->get_control)) {
        memcpy(&out_host->get_control, &symbol, sizeof(out_host->get_control));
    }
    return 1;
}

void erui_unload_test_host(ERUI_LoadedTestHost* host) {
    if (!host) return;
    if (host->module) FreeLibrary(host->module);
    memset(host, 0, sizeof(*host));
}
