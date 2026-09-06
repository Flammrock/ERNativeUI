#include "host_loader.h"

#include <ernativeui/ERNativeUI.hpp>

#include <chrono>
#include <cstdio>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

} // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    if (argc != 2) return 2;

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));

    const auto connection = erui::connect(0ms);
    CHECK(!connection.success());
    CHECK(connection.error().code() == erui::ErrorCode::incompatible_api);
    CHECK(connection.error().native_result() == ERUI_UNSUPPORTED_VERSION);
    CHECK(!connection.value().valid());
    CHECK(connection.value().api_version() == 0);
    CHECK(!connection.value().supports(erui::Capability::text_input));
    CHECK(!connection.value().supports(erui::Capability::color_picker));
    CHECK(!connection.value().supports(erui::Capability::input_bindings));

    erui_unload_test_host(&host);
    return 0;
}
