#include <ernativeui/erui.h>

int main(void) {
    ERUI_Api api = {0};
    api.size = (uint32_t)sizeof(api);
    return api.size == ERUI_API_V1_0_SIZE ? 0 : 1;
}
