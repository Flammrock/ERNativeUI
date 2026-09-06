#ifndef ERNATIVEUI_TEST_CONNECTION_CONTROL_H_INCLUDED
#define ERNATIVEUI_TEST_CONNECTION_CONTROL_H_INCLUDED

/* Test-only ABI for the explicit C++ Connection fixture. Never installed. */

#include <ernativeui/erui.h>

#include <stdint.h>

#if !defined(_WIN32)
#  error The ERNativeUI connection contract fixture requires Windows.
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define ERUI_TEST_CONNECTION_CONTROL_VERSION 1u

typedef struct ERUI_TestConnectionConfig {
    uint32_t size;
    uint32_t version;
    uint32_t not_ready_responses;
    ERUI_Result terminal_result;
    ERUI_Result language_result;
    uint32_t reserved[3];
} ERUI_TestConnectionConfig;

typedef struct ERUI_TestConnectionSnapshot {
    uint32_t size;
    uint32_t version;
    uint32_t get_api_calls;
    uint32_t language_calls;
    uint32_t register_calls;
    uint32_t commit_calls;
    uint32_t abort_calls;
    uint32_t reserved;
} ERUI_TestConnectionSnapshot;

typedef ERUI_Result (ERUI_CALL* ERUI_TestConfigureConnectionFn)(
    const ERUI_TestConnectionConfig* configuration);
typedef ERUI_Result (ERUI_CALL* ERUI_TestGetConnectionSnapshotFn)(
    ERUI_TestConnectionSnapshot* out_snapshot);

/*
 * Simulates one player-originated assignment update. The fixture updates its
 * live action state before synchronously invoking the provider-wide handler.
 * This lets the public C++ wrapper test borrowed event views and explicit
 * persistence without depending on Elden Ring's native input screens.
 */
typedef ERUI_Result (ERUI_CALL* ERUI_TestEmitAssignmentChangeFn)(
    ERUI_InputActionHandle action,
    const ERUI_ActionInputs* current,
    ERUI_AssignmentChangeReason reason);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_TEST_CONNECTION_CONTROL_H_INCLUDED */
