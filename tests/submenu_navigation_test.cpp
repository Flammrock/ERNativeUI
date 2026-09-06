#include "submenu_navigation.hpp"

#include "test_assertions.hpp"
#include <cstddef>
#include <cstdint>

int main() {
    using erui::detail::PageRoute;
    using erui::detail::SubmenuNavigation;
    using erui::detail::SubmenuResolutionKind;

    SubmenuNavigation navigation;
    ERUI_TEST_CHECK(navigation.initialize(4, 8));
    ERUI_TEST_CHECK(navigation.page_count() == 4);
    ERUI_TEST_CHECK(!navigation.pending_route().valid());

    auto* page_one = reinterpret_cast<void*>(0x1000);
    auto* page_two = reinterpret_cast<void*>(0x2000);
    auto* page_three = reinterpret_cast<void*>(0x3000);

    const PageRoute first_slice = PageRoute::submenu(1, 0);
    const PageRoute second_slice = PageRoute::submenu(1, 1);
    const PageRoute root_continuation =
        PageRoute::root_continuation(0, 7, 1);

    auto* root_page = reinterpret_cast<void*>(0x9000);
    ERUI_TEST_CHECK(navigation.begin_request(first_slice, 100, root_page));
    auto resolution = navigation.resolve(page_one, 125, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::pending_request);
    ERUI_TEST_CHECK(resolution.route == first_slice);
    ERUI_TEST_CHECK(navigation.native_page(first_slice) == page_one);
    ERUI_TEST_CHECK(navigation.parent_page(page_one) == root_page);
    ERUI_TEST_CHECK(!navigation.pending_route().valid());

    ERUI_TEST_CHECK(navigation.should_inject(first_slice, page_one, 125, 250));
    ERUI_TEST_CHECK(!navigation.should_inject(first_slice, page_one, 200, 250));
    ERUI_TEST_CHECK(navigation.should_inject(first_slice, page_one, 375, 250));

    resolution = navigation.resolve(page_one, 500, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::bound_page);
    ERUI_TEST_CHECK(resolution.route == first_slice);

    // A second physical slice of the same logical page has its own native
    // page and remains independently addressable for the native Back stack.
    ERUI_TEST_CHECK(navigation.begin_request(second_slice, 600, page_one));
    resolution = navigation.resolve(page_two, 610, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::pending_request);
    ERUI_TEST_CHECK(resolution.route == second_slice);
    ERUI_TEST_CHECK(navigation.native_page(first_slice) == page_one);
    ERUI_TEST_CHECK(navigation.native_page(second_slice) == page_two);
    ERUI_TEST_CHECK(navigation.parent_page(page_two) == page_one);

    ERUI_TEST_CHECK(navigation.begin_request(root_continuation, 700));
    resolution = navigation.resolve(page_three, 720, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::pending_request);
    ERUI_TEST_CHECK(resolution.route == root_continuation);
    ERUI_TEST_CHECK(navigation.native_page(root_continuation) == page_three);

    navigation.unbind_native_page(page_three);
    ERUI_TEST_CHECK(!navigation.route_for_native_page(page_three).valid());
    ERUI_TEST_CHECK(navigation.native_page(root_continuation) == nullptr);
    ERUI_TEST_CHECK(navigation.parent_page(page_three) == nullptr);

    // Reopening the same route replaces only its stale native pointer.
    auto* reopened = reinterpret_cast<void*>(0x4000);
    ERUI_TEST_CHECK(navigation.begin_request(second_slice, 800));
    resolution = navigation.resolve(reopened, 810, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::pending_request);
    ERUI_TEST_CHECK(navigation.native_page(second_slice) == reopened);
    ERUI_TEST_CHECK(!navigation.route_for_native_page(page_two).valid());
    ERUI_TEST_CHECK(navigation.route_for_native_page(page_one) == first_slice);

    const PageRoute expired_target = PageRoute::submenu(3, 0);
    ERUI_TEST_CHECK(navigation.begin_request(expired_target, 1000));
    resolution = navigation.resolve(reinterpret_cast<void*>(0x5000), 4001, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::expired_request);
    ERUI_TEST_CHECK(!resolution.route.valid());

    // Built-in continuation pages participate in the same fixed native-page
    // binding table, while their already-existing main pages are navigation-
    // only targets and must never be bound as newly opened subpages.
    const PageRoute builtin_continuation =
        PageRoute::builtin_continuation(2, 1, 1, 1);
    const PageRoute builtin_main = PageRoute::builtin_main(2, 1, 1);
    auto* builtin_parent = reinterpret_cast<void*>(0x6000);
    auto* builtin_child = reinterpret_cast<void*>(0x7000);
    ERUI_TEST_CHECK(navigation.begin_request(
        builtin_continuation, 5000, builtin_parent));
    resolution = navigation.resolve(builtin_child, 5010, 2000);
    ERUI_TEST_CHECK(resolution.kind == SubmenuResolutionKind::pending_request);
    ERUI_TEST_CHECK(resolution.route == builtin_continuation);
    ERUI_TEST_CHECK(
        navigation.native_page(builtin_continuation) == builtin_child);
    ERUI_TEST_CHECK(navigation.parent_page(builtin_child) == builtin_parent);
    ERUI_TEST_CHECK(!navigation.begin_request(
        builtin_main, 5100, builtin_child));

    navigation.reset();
    ERUI_TEST_CHECK(navigation.page_count() == 0);
    return 0;
}
