#ifndef ERNATIVEUI_ERUI_H_INCLUDED
#define ERNATIVEUI_ERUI_H_INCLUDED

/*
 * ERNativeUI public binary interface.
 *
 * This header is intentionally valid C99 and C++. Do not add STL types,
 * C++ classes, exceptions, compiler-owned strings, or allocator ownership to
 * this boundary. All input strings are copied before an API call returns.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(_WIN64)
#  error ERNativeUI supports Windows x64 clients only.
#endif

#if defined(_WIN32)
#  define ERUI_CALL __cdecl
#  if defined(ERUI_HOST_EXPORTS)
#    define ERUI_EXPORT __declspec(dllexport)
#  else
#    define ERUI_EXPORT
#  endif
#else
#  define ERUI_CALL
#  define ERUI_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ERUI_VERSION_ENCODE(major, minor) \
    ((((uint32_t)(major)) << 16u) | ((uint32_t)(minor) & 0xFFFFu))
#define ERUI_API_VERSION_1_0 ERUI_VERSION_ENCODE(1u, 0u)
#define ERUI_API_VERSION_CURRENT ERUI_API_VERSION_1_0

typedef uint32_t ERUI_Result;
enum {
    ERUI_OK = 0u,
    ERUI_INVALID_ARGUMENT = 1u,
    ERUI_HOST_NOT_READY = 2u,
    ERUI_HOST_FAILED = 3u,
    ERUI_UNSUPPORTED_VERSION = 4u,
    ERUI_DUPLICATE_PROVIDER_ID = 5u,
    ERUI_INVALID_HANDLE = 6u,
    ERUI_ALREADY_COMMITTED = 7u,
    ERUI_REGISTRATION_CLOSED = 8u,
    ERUI_OUT_OF_MEMORY = 9u,
    ERUI_CALLBACK_REJECTED = 10u,
    ERUI_INTERNAL_ERROR = 11u,
    ERUI_QUEUE_FULL = 12u,
    ERUI_NOT_SUPPORTED = 13u
};

typedef uint64_t ERUI_ProviderHandle;
typedef uint64_t ERUI_PageHandle;
typedef uint64_t ERUI_RowHandle;

#define ERUI_INVALID_PROVIDER ((ERUI_ProviderHandle)0u)
#define ERUI_INVALID_PAGE ((ERUI_PageHandle)0u)
#define ERUI_INVALID_ROW ((ERUI_RowHandle)0u)

typedef uint64_t ERUI_Capabilities;
enum {
    ERUI_CAP_TOGGLE = UINT64_C(1) << 0,
    ERUI_CAP_SLIDER = UINT64_C(1) << 1,
    ERUI_CAP_BUTTON = UINT64_C(1) << 2,
    ERUI_CAP_SUBMENU = UINT64_C(1) << 3,
    ERUI_CAP_PAGINATION = UINT64_C(1) << 4,
    ERUI_CAP_HOST_OWNED_VALUES = UINT64_C(1) << 5,
    ERUI_CAP_PAGE_PRESENTATION = UINT64_C(1) << 6,
    ERUI_CAP_ALERT = UINT64_C(1) << 7,
    ERUI_CAP_INLINE_CHOICE = UINT64_C(1) << 8,
    ERUI_CAP_POPUP_CHOICE = UINT64_C(1) << 9,
    ERUI_CAP_GAME_LANGUAGE = UINT64_C(1) << 10
};

/* UTF-8 is used for stable machine identifiers. */
typedef struct ERUI_StringView {
    const char* data;
    uint32_t length;
    uint32_t reserved;
} ERUI_StringView;

/* User-facing text is UTF-16 and does not depend on compiler wchar_t mode. */
typedef struct ERUI_Utf16View {
    const uint16_t* data;
    uint32_t length;
    uint32_t reserved;
} ERUI_Utf16View;

/*
 * Convenience classification for the exact Steam language identifier.
 * UNKNOWN means Steam returned a valid identifier not known to this host;
 * it does not mean that language discovery failed.
 */
typedef uint32_t ERUI_GameLanguage;
enum {
    ERUI_GAME_LANGUAGE_UNKNOWN = 0u,
    ERUI_GAME_LANGUAGE_ENGLISH = 1u,
    ERUI_GAME_LANGUAGE_GERMAN = 2u,
    ERUI_GAME_LANGUAGE_FRENCH = 3u,
    ERUI_GAME_LANGUAGE_ITALIAN = 4u,
    ERUI_GAME_LANGUAGE_KOREAN = 5u,
    ERUI_GAME_LANGUAGE_SPANISH = 6u,
    ERUI_GAME_LANGUAGE_CHINESE_SIMPLIFIED = 7u,
    ERUI_GAME_LANGUAGE_CHINESE_TRADITIONAL = 8u,
    ERUI_GAME_LANGUAGE_RUSSIAN = 9u,
    ERUI_GAME_LANGUAGE_THAI = 10u,
    ERUI_GAME_LANGUAGE_JAPANESE = 11u,
    ERUI_GAME_LANGUAGE_POLISH = 12u,
    ERUI_GAME_LANGUAGE_ARABIC = 13u,
    ERUI_GAME_LANGUAGE_PORTUGUESE_BRAZIL = 14u,
    ERUI_GAME_LANGUAGE_SPANISH_LATIN_AMERICA = 15u
};

/*
 * identifier is the exact UTF-8 token returned by Steam (for example
 * "french", "koreana", or an identifier unknown to ERNativeUI). Its memory
 * is host-owned and remains valid until process exit. The caller initializes
 * size before calling get_game_language.
 */
typedef struct ERUI_GameLanguageInfo {
    uint32_t size;
    ERUI_GameLanguage known_language;
    ERUI_StringView identifier;
    uint32_t reserved[2];
} ERUI_GameLanguageInfo;

/*
 * Page-title formatters write into host-owned storage. output_capacity and
 * out_length are measured in UTF-16 code units; no terminator is required.
 * Returning anything other than ERUI_OK, producing an empty/invalid string,
 * or reporting a length greater than output_capacity selects the host's safe
 * default title instead. All pointers in the context and the output buffer are
 * valid only for the duration of the callback.
 */
#define ERUI_PAGE_TITLE_BUFFER_CAPACITY 4096u

typedef struct ERUI_PageTitleFormatContext {
    uint32_t size;
    uint32_t flags;
    ERUI_ProviderHandle provider;
    ERUI_PageHandle page;
    ERUI_Utf16View base_title;
    uint32_t page_number;
    uint32_t page_count;
    uint32_t reserved[2];
} ERUI_PageTitleFormatContext;

typedef ERUI_Result (ERUI_CALL* ERUI_PageTitleFormatter)(
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    uint16_t* output,
    uint32_t output_capacity,
    uint32_t* out_length);

typedef void (ERUI_CALL* ERUI_ButtonCallback)(void* user_data);
typedef void (ERUI_CALL* ERUI_ValueChangedCallback)(
    void* user_data,
    uint8_t value);

/* Alert button layouts are closed choices, not combinable bit flags. */
typedef uint32_t ERUI_AlertButtons;
enum {
    ERUI_ALERT_BUTTONS_OK = 0u,
    ERUI_ALERT_BUTTONS_CANCEL = 1u,
    ERUI_ALERT_BUTTONS_YES = 2u,
    ERUI_ALERT_BUTTONS_NO = 3u,
    ERUI_ALERT_BUTTONS_OK_CANCEL = 4u,
    ERUI_ALERT_BUTTONS_YES_NO = 5u,
    ERUI_ALERT_BUTTONS_DISMISS_ONLY = 6u
};

typedef uint32_t ERUI_AlertPlacement;
enum {
    ERUI_ALERT_PLACEMENT_BOTTOM = 0u,
    ERUI_ALERT_PLACEMENT_CENTER = 1u
};

typedef uint32_t ERUI_AlertResponse;
enum {
    ERUI_ALERT_RESPONSE_NONE = 0u,
    ERUI_ALERT_RESPONSE_PRIMARY = 1u,
    ERUI_ALERT_RESPONSE_SECONDARY = 2u,
    ERUI_ALERT_RESPONSE_DISMISSED = 3u
};

/*
 * Alert completion callbacks are asynchronous and never run inside
 * enqueue_alert. ERUI_OK means the dialog completed normally and response
 * identifies how it completed. For another result, response is NONE because
 * the accepted request could not be presented or completed normally.
 */
typedef void (ERUI_CALL* ERUI_AlertCallback)(
    void* user_data,
    ERUI_Result completion_result,
    ERUI_AlertResponse response);

typedef struct ERUI_ProviderDesc {
    uint32_t size;
    uint32_t api_version;
    uint32_t flags;
    int32_t root_priority;
    void* owner_module;
    ERUI_StringView provider_id;
    ERUI_Utf16View display_name;
} ERUI_ProviderDesc;

typedef struct ERUI_ButtonDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ButtonCallback callback;
    void* user_data;
    uint32_t enabled;
    uint32_t reserved;
} ERUI_ButtonDesc;

typedef struct ERUI_ToggleDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ValueChangedCallback changed_callback;
    void* user_data;
    uint8_t initial_value;
    uint8_t reserved8[3];
    uint32_t enabled;
} ERUI_ToggleDesc;

typedef struct ERUI_SliderDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ValueChangedCallback changed_callback;
    void* user_data;
    int32_t minimum;
    int32_t maximum;
    int32_t step;
    uint8_t initial_value;
    uint8_t reserved8[3];
    uint32_t enabled;
} ERUI_SliderDesc;

typedef struct ERUI_ChoiceDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_ValueChangedCallback changed_callback;
    void* user_data;
    const ERUI_Utf16View* options;
    uint32_t option_count;
    uint8_t initial_index;
    uint8_t reserved8[3];
} ERUI_ChoiceDesc;

typedef struct ERUI_SubmenuDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View label;
    ERUI_Utf16View help;
    ERUI_Utf16View page_title;
    ERUI_Utf16View page_help;
    uint32_t enabled;
    uint32_t reserved;
} ERUI_SubmenuDesc;

/*
 * menu_title changes the native outer heading for a provider-owned submenu.
 * page_title changes the base title for its physical slices. Empty views keep
 * the corresponding host default. formatter is optional and, when present,
 * receives that effective base title once per physical slice during menu
 * compilation. The host copies every view and every successful formatter
 * result; it never assumes ownership of client memory. formatter and
 * user_data must remain valid until commit_provider returns.
 */
typedef struct ERUI_PagePresentationDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View menu_title;
    ERUI_Utf16View page_title;
    ERUI_PageTitleFormatter formatter;
    void* user_data;
    uint32_t reserved[2];
} ERUI_PagePresentationDesc;

/*
 * Queues one native Elden Ring message dialog. buttons selects its button
 * layout and placement selects its screen position. The host copies message
 * before enqueue_alert returns. callback is optional; user_data must be null
 * when callback is null. Requests are presented globally in FIFO order, one at
 * a time. callback and user_data are retained asynchronously and must remain
 * valid until callback is invoked.
 */
typedef struct ERUI_AlertDesc {
    uint32_t size;
    uint32_t flags;
    ERUI_Utf16View message;
    ERUI_AlertCallback callback;
    void* user_data;
    ERUI_AlertButtons buttons;
    ERUI_AlertPlacement placement;
} ERUI_AlertDesc;

typedef struct ERUI_Api {
    /* Client initializes size; host writes only this many bytes. */
    uint32_t size;
    uint32_t api_version;
    ERUI_Capabilities capabilities;

    ERUI_Result (ERUI_CALL* register_provider)(
        const ERUI_ProviderDesc* description,
        ERUI_ProviderHandle* out_provider,
        ERUI_PageHandle* out_root_page);
    ERUI_Result (ERUI_CALL* add_button)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ButtonDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_toggle)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ToggleDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_slider)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_SliderDesc* description,
        ERUI_RowHandle* out_row);
    /*
     * Both choice functions copy the descriptor and options synchronously.
     * Indices are zero-based. An inline choice changes with left/right input;
     * a popup choice opens Elden Ring's native selection list.
     */
    ERUI_Result (ERUI_CALL* add_inline_choice)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_popup_choice)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* add_submenu)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle parent_page,
        const ERUI_SubmenuDesc* description,
        ERUI_PageHandle* out_child_page,
        ERUI_RowHandle* out_row);
    ERUI_Result (ERUI_CALL* set_page_presentation)(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_PagePresentationDesc* description);
    ERUI_Result (ERUI_CALL* commit_provider)(ERUI_ProviderHandle provider);
    ERUI_Result (ERUI_CALL* abort_provider)(ERUI_ProviderHandle provider);
    ERUI_Result (ERUI_CALL* set_row_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint8_t value);
    ERUI_Result (ERUI_CALL* get_row_value)(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        uint8_t* out_value);
    ERUI_Result (ERUI_CALL* enqueue_alert)(
        ERUI_ProviderHandle provider,
        const ERUI_AlertDesc* description);
    ERUI_Result (ERUI_CALL* get_game_language)(
        ERUI_GameLanguageInfo* out_language);
} ERUI_Api;

/* Complete function-table prefix required by API 1.0 clients. */
#define ERUI_API_V1_0_SIZE ((uint32_t)( \
    offsetof(ERUI_Api, get_game_language) + \
    sizeof(((ERUI_Api*)0)->get_game_language)))

typedef ERUI_Result (ERUI_CALL* ERUI_GetApiFn)(
    uint32_t requested_version,
    ERUI_Api* out_api);

ERUI_EXPORT ERUI_Result ERUI_CALL ERUI_GetApi(
    uint32_t requested_version,
    ERUI_Api* out_api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ERNATIVEUI_ERUI_H_INCLUDED */
