#include "runtime.hpp"
#include "runtime_requirements.hpp"

#include "test_assertions.hpp"
#include <cstddef>

int main() {
    const erui::RuntimeOptions options{};
    ERUI_TEST_CHECK(options.enable_row_injection);
    ERUI_TEST_CHECK(options.enable_custom_text);
    ERUI_TEST_CHECK(!options.enable_diagnostics);
    ERUI_TEST_CHECK(options.game_options_visual_capacity == 6);
    ERUI_TEST_CHECK(options.injection_cooldown_ms == 250);
    ERUI_TEST_CHECK(options.log_sink == nullptr);

#if defined(_WIN64)
    static_assert(sizeof(erui::RuntimeOptions) == 24);
    static_assert(offsetof(erui::RuntimeOptions, game_options_visual_capacity) == 4);
    static_assert(offsetof(erui::RuntimeOptions, injection_cooldown_ms) == 8);
    static_assert(offsetof(erui::RuntimeOptions, log_sink) == 16);
#endif
    static_assert(erui::runtime_api_version == 0x0009'0300);
    static_assert(erui::Capabilities{}.button_runtime);
    static_assert(erui::Capabilities{}.submenu_runtime);
    static_assert(erui::Capabilities{}.pagination_runtime);
    static_assert(erui::Capabilities{}.game_options_capacity_detection);

    // A custom tab can be the only logical page that overflows. Its
    // continuation is still materialized as a native subpage, so active
    // pagination alone must request the submenu interfaces.
    ERUI_TEST_CHECK(erui::detail::pagination_requires_submenus(
        true, true, false, false));
    ERUI_TEST_CHECK(erui::detail::pagination_requires_submenus(
        true, false, true, false));
    ERUI_TEST_CHECK(erui::detail::pagination_requires_submenus(
        true, false, false, true));
    ERUI_TEST_CHECK(erui::detail::pagination_requires_submenus(
        true, false, false, false, true));
    ERUI_TEST_CHECK(!erui::detail::pagination_requires_submenus(
        true, false, false, false));
    ERUI_TEST_CHECK(!erui::detail::pagination_requires_submenus(
        false, true, true, true));

    // The build-locked API 1.4 presentation pump is not required merely
    // because an earlier-API client registered ordinary rows.
    static_assert(!erui::detail::runtime_requires_row_presentations(
        true, false, 12));
    static_assert(!erui::detail::runtime_requires_row_presentations(
        false, true, 12));
    static_assert(!erui::detail::runtime_requires_row_presentations(
        true, true, 0));
    static_assert(erui::detail::runtime_requires_row_presentations(
        true, true, 1));

    // Capability preflight may intentionally withhold only row visibility
    // when another mod already patched one of its optional native entries.
    // That reduced API 1.4 host must still install. Conversely, a backend
    // advertised before registration must fail closed if its exact build or
    // native aggregate changes before hook installation.
    static_assert(erui::detail::runtime_row_visibility_backend_valid(
        false, true, false));
    static_assert(erui::detail::runtime_row_visibility_backend_valid(
        false, false, false));
    static_assert(erui::detail::runtime_row_visibility_backend_valid(
        true, true, true));
    static_assert(!erui::detail::runtime_row_visibility_backend_valid(
        true, true, false));
    static_assert(!erui::detail::runtime_row_visibility_backend_valid(
        true, false, true));
    return 0;
}
