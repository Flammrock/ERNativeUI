#ifndef ERNATIVEUI_TEST_PENDING_V1_0_CONTROL_H_INCLUDED
#define ERNATIVEUI_TEST_PENDING_V1_0_CONTROL_H_INCLUDED

#include <ernativeui/erui.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef ERUI_Result (ERUI_CALL* ERUI_TestSettleLanguageFn)(
    const char* identifier);
typedef void (ERUI_CALL* ERUI_TestSetRuntimeReadyFn)(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_TEST_PENDING_V1_0_CONTROL_H_INCLUDED */
