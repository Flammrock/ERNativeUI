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
/* Current-host fixture only: pass in configuration.reserved[0]. Frozen
 * contract hosts continue to require every reserved value to be zero. */
#define ERUI_TEST_CONNECTION_DISABLE_TITLE_SCREEN_TEXT (1u << 0)
#define ERUI_TEST_CONNECTION_DISABLE_ROW_AVAILABILITY (1u << 1)
#define ERUI_TEST_CONNECTION_DISABLE_ROW_VISIBILITY (1u << 2)
#define ERUI_TEST_CONNECTION_DISABLE_SUBMENU_UPDATES (1u << 3)

/*
 * Keep the shared control protocol independent of symbols introduced by the
 * current SDK.  Frozen v1.1/v1.2 clients include this header through their
 * own erui.h snapshots, which intentionally do not know about title-screen
 * text.  The current fixture translates these wire-sized values to and from
 * the current public descriptor.
 */
#define ERUI_TEST_TITLE_SCREEN_TEXT_MAX_CODE_UNITS 255u

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

/* Exact descriptor observation used by the current-header wrapper test. */
typedef struct ERUI_TestTitleScreenTextObservation {
    uint32_t size;
    uint32_t descriptor_size;
    uint32_t flags;
    ERUI_Color color;
    uint32_t corner;
    uint32_t text_length;
    uint16_t text[ERUI_TEST_TITLE_SCREEN_TEXT_MAX_CODE_UNITS];
    uint32_t reserved[2];
} ERUI_TestTitleScreenTextObservation;

typedef ERUI_Result (ERUI_CALL* ERUI_TestGetTitleScreenTextObservationFn)(
    uint32_t index,
    ERUI_TestTitleScreenTextObservation* out_observation);

#if defined(ERUI_API_VERSION_1_4)
/* Exact row-update descriptor observation used only by the current wrapper. */
typedef struct ERUI_TestRowUpdateObservation {
    uint32_t size;
    uint32_t descriptor_size;
    ERUI_RowHandle row;
    ERUI_RowUpdateFields fields;
    uint32_t visible;
    uint32_t reserved[2];
} ERUI_TestRowUpdateObservation;

typedef ERUI_Result (ERUI_CALL* ERUI_TestGetRowUpdateObservationFn)(
    uint32_t index,
    ERUI_TestRowUpdateObservation* out_observation);
#endif

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
