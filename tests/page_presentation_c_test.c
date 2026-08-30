#include <ernativeui/erui.h>

#include <stddef.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ERUI_PageTitleFormatContext) == 56u,
    "ERUI_PageTitleFormatContext ABI drift");
_Static_assert(sizeof(ERUI_PagePresentationDesc) == 64u,
    "ERUI_PagePresentationDesc ABI drift");
_Static_assert(offsetof(ERUI_PageTitleFormatContext, base_title) == 24u,
    "ERUI page-title base view offset drift");
_Static_assert(offsetof(ERUI_PagePresentationDesc, formatter) == 40u,
    "ERUI page-title formatter offset drift");
_Static_assert(offsetof(ERUI_Api, set_page_presentation) == 72u,
    "ERUI page-presentation API offset drift");
#endif

static ERUI_Result ERUI_CALL format_title(
    void* user_data,
    const ERUI_PageTitleFormatContext* context,
    uint16_t* output,
    uint32_t output_capacity,
    uint32_t* out_length) {
    static const uint16_t title[] = {'C', ' ', 'A', 'P', 'I'};
    uint32_t index;
    (void)user_data;
    (void)context;
    if (!output || !out_length || output_capacity < 5u) {
        return ERUI_INVALID_ARGUMENT;
    }
    for (index = 0; index < 5u; ++index) {
        output[index] = title[index];
    }
    *out_length = 5u;
    return ERUI_OK;
}

int main(void) {
    ERUI_PageTitleFormatContext context = {0};
    ERUI_PagePresentationDesc presentation = {0};
    ERUI_Api api = {0};
    uint16_t output[ERUI_PAGE_TITLE_BUFFER_CAPACITY] = {0};
    uint32_t length = 0;

    context.size = (uint32_t)sizeof(context);
    context.page_number = 1u;
    context.page_count = 3u;
    presentation.size = (uint32_t)sizeof(presentation);
    presentation.formatter = &format_title;
    api.size = (uint32_t)sizeof(api);

    if (presentation.formatter(
            0, &context, output, ERUI_PAGE_TITLE_BUFFER_CAPACITY,
            &length) != ERUI_OK) {
        return 1;
    }
    if (length != 5u || output[0] != (uint16_t)'C') {
        return 2;
    }
    if (ERUI_CAP_PAGE_PRESENTATION == 0u ||
        ERUI_PAGE_TITLE_BUFFER_CAPACITY != 4096u) {
        return 3;
    }
    return 0;
}
