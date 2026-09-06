#include "host_loader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (false)

bool bytes_equal(
    const std::byte* bytes,
    std::size_t count,
    std::byte expected) noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (bytes[index] != expected) return false;
    }
    return true;
}

int check_v1_0_prefix(ERUI_GetApiFn get_api) {
    struct alignas(ERUI_Api) Storage {
        std::array<std::byte, sizeof(ERUI_Api) + 32> bytes{};
    } storage;
    auto* api = ::new (storage.bytes.data()) ERUI_Api{};
    std::memset(
        storage.bytes.data() + ERUI_API_V1_0_SIZE,
        0xA5,
        storage.bytes.size() - ERUI_API_V1_0_SIZE);
    api->size = ERUI_API_V1_0_SIZE;

    CHECK(get_api(ERUI_API_VERSION_1_0, api) == ERUI_OK);
    CHECK(api->size == ERUI_API_V1_0_SIZE);
    CHECK(api->api_version == ERUI_API_VERSION_1_0);
    CHECK(api->register_provider != nullptr);
    CHECK(api->get_game_language != nullptr);
#if defined(ERUI_API_VERSION_1_1)
    CHECK((api->capabilities & ERUI_CAP_TEXT_INPUT) == 0);
    CHECK((api->capabilities & ERUI_CAP_COLOR_PICKER) == 0);
    CHECK((api->capabilities & ERUI_CAP_BUILTIN_PAGES) == 0);
    CHECK((api->capabilities & ERUI_CAP_INPUT_BINDINGS) == 0);
#endif
    CHECK(bytes_equal(
        storage.bytes.data() + ERUI_API_V1_0_SIZE,
        storage.bytes.size() - ERUI_API_V1_0_SIZE,
        std::byte{0xA5}));
    return 0;
}

int check_unknown_unchanged(ERUI_GetApiFn get_api) {
    ERUI_Api api{};
    std::memset(&api, 0x5A, sizeof(api));
    api.size = static_cast<std::uint32_t>(sizeof(api));
    std::array<std::byte, sizeof(api)> before{};
    std::memcpy(before.data(), &api, sizeof(api));
    CHECK(get_api(ERUI_VERSION_ENCODE(1u, 99u), &api) ==
        ERUI_UNSUPPORTED_VERSION);
    CHECK(std::memcmp(before.data(), &api, sizeof(api)) == 0);
    return 0;
}

int check_selected_current(ERUI_GetApiFn get_api) {
    ERUI_Api api{};
#if defined(ERUI_API_CURRENT_SIZE)
    api.size = ERUI_API_CURRENT_SIZE;
#else
    api.size = static_cast<std::uint32_t>(sizeof(api));
#endif
    CHECK(get_api(ERUI_API_VERSION_CURRENT, &api) == ERUI_OK);
    CHECK(api.api_version == ERUI_API_VERSION_CURRENT);
    CHECK(api.size >= ERUI_API_V1_0_SIZE);
#if defined(ERUI_API_VERSION_1_1)
    CHECK(api.size == ERUI_API_V1_1_SIZE);
    CHECK((api.capabilities & ERUI_CAP_TEXT_INPUT) != 0);
    CHECK(api.add_text_input != nullptr);
    CHECK(api.set_text_input_value != nullptr);
    CHECK(api.get_text_input_value != nullptr);
    CHECK((api.capabilities & ERUI_CAP_COLOR_PICKER) != 0);
    CHECK(api.add_color_picker != nullptr);
    CHECK(api.set_color_picker_value != nullptr);
    CHECK(api.get_color_picker_value != nullptr);
    CHECK((api.capabilities & ERUI_CAP_BUILTIN_PAGES) != 0);
    CHECK(api.get_builtin_page != nullptr);
    CHECK((api.capabilities & ERUI_CAP_INPUT_BINDINGS) != 0);
    CHECK(api.add_input_section != nullptr);
    CHECK(api.add_input_action != nullptr);
    CHECK(api.set_assignments_changed_handler != nullptr);
    CHECK(api.set_action_inputs != nullptr);
    CHECK(api.get_action_inputs != nullptr);
    CHECK(api.get_action_default_inputs != nullptr);
    CHECK(api.reset_action_inputs != nullptr);
    CHECK((api.capabilities & ERUI_CAP_STORAGE) != 0);
    CHECK(api.open_storage != nullptr);
    CHECK(api.storage_get_info != nullptr);
#endif
    return 0;
}

int check_old_host_exact_versions(ERUI_GetApiFn get_api) {
#if defined(ERUI_API_VERSION_1_1)
    ERUI_Api strict{};
    strict.size = ERUI_API_V1_1_SIZE;
    std::array<std::byte, sizeof(strict)> before{};
    std::memcpy(before.data(), &strict, sizeof(strict));
    CHECK(get_api(ERUI_API_VERSION_1_1, &strict) ==
        ERUI_UNSUPPORTED_VERSION);
    CHECK(std::memcmp(before.data(), &strict, sizeof(strict)) == 0);

    ERUI_Api selected_v1_0{};
    selected_v1_0.size = ERUI_API_V1_0_SIZE;
    CHECK(get_api(ERUI_API_VERSION_1_0, &selected_v1_0) == ERUI_OK);
    CHECK(selected_v1_0.api_version == ERUI_API_VERSION_1_0);
    CHECK(selected_v1_0.size == ERUI_API_V1_0_SIZE);
    CHECK((selected_v1_0.capabilities & ERUI_CAP_TEXT_INPUT) == 0);
    CHECK(selected_v1_0.add_text_input == nullptr);
    CHECK(selected_v1_0.set_text_input_value == nullptr);
    CHECK(selected_v1_0.get_text_input_value == nullptr);
    CHECK((selected_v1_0.capabilities & ERUI_CAP_COLOR_PICKER) == 0);
    CHECK(selected_v1_0.add_color_picker == nullptr);
    CHECK(selected_v1_0.set_color_picker_value == nullptr);
    CHECK(selected_v1_0.get_color_picker_value == nullptr);
    CHECK((selected_v1_0.capabilities & ERUI_CAP_BUILTIN_PAGES) == 0);
    CHECK(selected_v1_0.get_builtin_page == nullptr);
    CHECK((selected_v1_0.capabilities & ERUI_CAP_INPUT_BINDINGS) == 0);
    CHECK(selected_v1_0.add_input_section == nullptr);
    CHECK(selected_v1_0.add_input_action == nullptr);
    CHECK(selected_v1_0.open_storage == nullptr);
#else
    CHECK(check_v1_0_prefix(get_api) == 0);
#endif
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <host-dll> <current|old-host>\n", argv[0]);
        return 2;
    }

    ERUI_LoadedTestHost host{};
    CHECK(erui_load_test_host(argv[1], &host));
    CHECK(host.get_api != nullptr);
    CHECK(check_unknown_unchanged(host.get_api) == 0);

    int result = 0;
    if (std::strcmp(argv[2], "current") == 0) {
        result = check_v1_0_prefix(host.get_api);
        if (result == 0) result = check_selected_current(host.get_api);
    } else if (std::strcmp(argv[2], "old-host") == 0) {
        result = check_old_host_exact_versions(host.get_api);
    } else {
        result = 2;
    }
    erui_unload_test_host(&host);
    return result;
}
