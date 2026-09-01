#ifndef ERNATIVEUI_TEST_COMPAT_CONTROL_H_INCLUDED
#define ERNATIVEUI_TEST_COMPAT_CONTROL_H_INCLUDED

/* Test-only ABI. This header is never installed with the public SDK. */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  define ERUI_COMPAT_CALL __cdecl
#  if defined(ERUI_COMPAT_HOST_EXPORTS)
#    define ERUI_COMPAT_EXPORT __declspec(dllexport)
#  else
#    define ERUI_COMPAT_EXPORT
#  endif
#else
#  define ERUI_COMPAT_CALL
#  define ERUI_COMPAT_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ERUI_COMPAT_CONTROL_VERSION 1u

typedef uint32_t ERUI_CompatResult;
typedef uint64_t ERUI_CompatPageHandle;
typedef uint64_t ERUI_CompatRowHandle;

typedef struct ERUI_CompatControl {
    uint32_t size;
    uint32_t version;

    void (ERUI_COMPAT_CALL* reset)(void);
    ERUI_CompatResult (ERUI_COMPAT_CALL* trigger_button)(
        ERUI_CompatRowHandle row);
    ERUI_CompatResult (ERUI_COMPAT_CALL* trigger_value)(
        ERUI_CompatRowHandle row,
        uint8_t value);
    ERUI_CompatResult (ERUI_COMPAT_CALL* format_page_title)(
        ERUI_CompatPageHandle page,
        uint32_t page_number,
        uint32_t page_count,
        uint16_t* output,
        uint32_t output_capacity,
        uint32_t* out_length);
    ERUI_CompatResult (ERUI_COMPAT_CALL* complete_alert)(
        uint32_t completion_result,
        uint32_t response);
    ERUI_CompatResult (ERUI_COMPAT_CALL* get_row_label)(
        ERUI_CompatRowHandle row,
        uint16_t* output,
        uint32_t output_capacity,
        uint32_t* out_length);
    ERUI_CompatResult (ERUI_COMPAT_CALL* get_choice_text)(
        ERUI_CompatRowHandle row,
        uint32_t option_index,
        uint16_t* output,
        uint32_t output_capacity,
        uint32_t* out_length);
} ERUI_CompatControl;

#define ERUI_COMPAT_CONTROL_V1_SIZE ((uint32_t)( \
    offsetof(ERUI_CompatControl, get_choice_text) + \
    sizeof(((ERUI_CompatControl*)0)->get_choice_text)))

typedef ERUI_CompatResult (ERUI_COMPAT_CALL* ERUI_CompatGetControlFn)(
    uint32_t requested_version,
    ERUI_CompatControl* out_control);

ERUI_COMPAT_EXPORT ERUI_CompatResult ERUI_COMPAT_CALL ERUI_CompatGetControl(
    uint32_t requested_version,
    ERUI_CompatControl* out_control);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_TEST_COMPAT_CONTROL_H_INCLUDED */
