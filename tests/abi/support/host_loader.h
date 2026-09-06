#ifndef ERNATIVEUI_TEST_HOST_LOADER_H_INCLUDED
#define ERNATIVEUI_TEST_HOST_LOADER_H_INCLUDED

#include <ernativeui/erui.h>
#include "compat_control.h"

#if !defined(_WIN32)
#  error The ERNativeUI ABI runtime tests require Windows.
#endif

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ERUI_LoadedTestHost {
    HMODULE module;
    ERUI_GetApiFn get_api;
    ERUI_CompatGetControlFn get_control;
} ERUI_LoadedTestHost;

int erui_load_test_host(
    const char* utf8_path,
    ERUI_LoadedTestHost* out_host);
void erui_unload_test_host(ERUI_LoadedTestHost* host);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_TEST_HOST_LOADER_H_INCLUDED */
