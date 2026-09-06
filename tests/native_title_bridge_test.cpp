#include "native_title_bridge.hpp"

#include "test_assertions.hpp"
#include <cstddef>
#include <cstring>

int main() {
    using namespace erui;
    using namespace erui::native;

    static_assert(scaleform_path_result_size == 0x60);
    static_assert(scaleform_text_value_offset == 0x08);
    static_assert(scaleform_destructor_member_offset == 0x28);

    TitleCaptureState state{};
    std::byte persistent[scaleform_path_result_size]{};
    int movie_context = 0;
    const detail::PageRoute first = detail::PageRoute::submenu(3, 1);

    ERUI_TEST_CHECK(state.observe(
        nullptr, first, &movie_context, persistent) == nullptr);
    ERUI_TEST_CHECK(state.observe(
        native_menu_title_source_path, first, nullptr, persistent) == nullptr);
    ERUI_TEST_CHECK(state.observe(
        native_menu_title_source_path, first, &movie_context, nullptr) == nullptr);
    ERUI_TEST_CHECK(state.observe(
        "MenuTitle/Text", first, &movie_context, persistent) == nullptr);
    ERUI_TEST_CHECK(state.observe(
        "MenuTitle/Text_01", first, &movie_context, persistent) == nullptr);
    ERUI_TEST_CHECK(state.observe(
        native_menu_title_source_path,
        detail::PageRoute::root_main(0, detail::game_options_vanilla_capacity),
        &movie_context,
        persistent) == nullptr);

    const char* replacement = state.observe(
        native_menu_title_source_path, first, &movie_context, persistent);
    ERUI_TEST_CHECK(replacement != nullptr);
    ERUI_TEST_CHECK(std::strcmp(replacement, custom_page_title_path) == 0);

    const CapturedTitleTarget captured = state.consume(first);
    ERUI_TEST_CHECK(captured.valid());
    ERUI_TEST_CHECK(captured.movie_context == &movie_context);
    ERUI_TEST_CHECK(captured.persistent_result == persistent);
    ERUI_TEST_CHECK(captured.text_value() == persistent + scaleform_text_value_offset);
    ERUI_TEST_CHECK(!state.consume(first).valid());

    const detail::PageRoute second = detail::PageRoute::root_continuation(
        0, detail::game_options_max_visual_capacity, 2);
    ERUI_TEST_CHECK(state.observe(
        native_menu_title_source_path, second, &movie_context, persistent));
    ERUI_TEST_CHECK(!state.consume(first).valid());
    ERUI_TEST_CHECK(!state.consume(second).valid());
    ERUI_TEST_CHECK(state.observe(
        native_menu_title_source_path, second, &movie_context, persistent));
    ERUI_TEST_CHECK(state.consume(second).valid());

    state.reset();
    return 0;
}
