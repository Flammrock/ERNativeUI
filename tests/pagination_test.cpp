#include "pagination.hpp"

#include "test_assertions.hpp"
#include <cstddef>
#include <stdexcept>

namespace {

void assert_slice(
    const erui::detail::PageSlice& slice,
    std::size_t first,
    std::size_t count,
    bool previous,
    bool next,
    std::size_t index,
    std::size_t total) {
    ERUI_TEST_CHECK(slice.first_row == first);
    ERUI_TEST_CHECK(slice.row_count == count);
    ERUI_TEST_CHECK(slice.has_previous == previous);
    ERUI_TEST_CHECK(slice.has_next == next);
    ERUI_TEST_CHECK(slice.slice_index == index);
    ERUI_TEST_CHECK(slice.slice_count == total);
}

void assert_plan_is_contiguous(
    const erui::detail::PagePlan& plan,
    std::size_t logical_rows,
    std::size_t capacity) {
    std::size_t consumed = 0;
    for (const auto& slice : plan.slices) {
        ERUI_TEST_CHECK(slice.first_row == consumed);
        ERUI_TEST_CHECK(slice.row_count + (slice.has_previous ? 1u : 0u) +
            (slice.has_next ? 1u : 0u) <= capacity);
        consumed += slice.row_count;
    }
    ERUI_TEST_CHECK(consumed == logical_rows);
}

} // namespace

int main() {
    using namespace erui::detail;

    {
        const PagePlan plan = paginate_subpage(3, 0);
        ERUI_TEST_CHECK(plan.slices.size() == 1);
        assert_slice(plan.slices[0], 0, 0, false, false, 0, 1);
        assert_plan_is_contiguous(plan, 0, 15);
    }

    {
        const PagePlan plan = paginate_subpage(3, 15);
        ERUI_TEST_CHECK(plan.slices.size() == 1);
        assert_slice(plan.slices[0], 0, 15, false, false, 0, 1);
        assert_plan_is_contiguous(plan, 15, 15);
    }

    {
        const PagePlan plan = paginate_subpage(3, 16);
        ERUI_TEST_CHECK(plan.slices.size() == 2);
        assert_slice(plan.slices[0], 0, 14, false, true, 0, 2);
        assert_slice(plan.slices[1], 14, 2, true, false, 1, 2);
        assert_plan_is_contiguous(plan, 16, 15);
    }

    {
        const PagePlan plan = paginate_subpage(3, 32);
        ERUI_TEST_CHECK(plan.slices.size() == 3);
        assert_slice(plan.slices[0], 0, 14, false, true, 0, 3);
        assert_slice(plan.slices[1], 14, 13, true, true, 1, 3);
        assert_slice(plan.slices[2], 27, 5, true, false, 2, 3);
        assert_plan_is_contiguous(plan, 32, 15);
    }

    {
        const RootPagePlan plan = paginate_root_page(0, 5, 7);
        ERUI_TEST_CHECK(plan.custom_capacity == 3);
        ERUI_TEST_CHECK(plan.pages.slices.size() == 2);
        assert_slice(plan.pages.slices[0], 0, 2, false, true, 0, 2);
        assert_slice(plan.pages.slices[1], 2, 3, true, false, 1, 2);
        assert_plan_is_contiguous(plan.pages, 5, 15);
    }

    {
        const RootPagePlan plan = paginate_root_page(0, 5, 6);
        ERUI_TEST_CHECK(plan.custom_capacity == 2);
        ERUI_TEST_CHECK(plan.pages.slices.size() == 2);
        assert_slice(plan.pages.slices[0], 0, 1, false, true, 0, 2);
        assert_slice(plan.pages.slices[1], 1, 4, true, false, 1, 2);
        assert_plan_is_contiguous(plan.pages, 5, 15);
    }

    {
        const RootPagePlan plan = paginate_root_page(0, 5, 5);
        ERUI_TEST_CHECK(plan.custom_capacity == 1);
        ERUI_TEST_CHECK(plan.pages.slices.size() == 2);
        assert_slice(plan.pages.slices[0], 0, 0, false, true, 0, 2);
        assert_slice(plan.pages.slices[1], 0, 5, true, false, 1, 2);
        assert_plan_is_contiguous(plan.pages, 5, 15);
    }

    // Live root capacity accounts for rows installed before ERNativeUI.
    ERUI_TEST_CHECK(derive_root_plan_capacity(13, 4) == 13);
    ERUI_TEST_CHECK(derive_root_plan_capacity(13, 5) == 12);
    ERUI_TEST_CHECK(derive_root_plan_capacity(13, 12) == 5);
    ERUI_TEST_CHECK(derive_root_plan_capacity(13, 13) == 0);
    ERUI_TEST_CHECK(derive_root_plan_capacity(13, 14) == 0);
    ERUI_TEST_CHECK(derive_root_plan_capacity(6, 4) == 6);
    ERUI_TEST_CHECK(derive_root_plan_capacity(6, 5) == 5);
    ERUI_TEST_CHECK(derive_root_plan_capacity(6, 6) == 0);
    ERUI_TEST_CHECK(derive_root_plan_capacity(6, 3) == 0);
    ERUI_TEST_CHECK(derive_root_plan_capacity(5, 4) == 0);

    {
        // Patched 13-row GFX with five earlier rows leaves eight physical
        // slots: seven client rows plus Next, never a fourteenth row.
        const RootPagePlan plan = paginate_root_page(0, 12, 12);
        ERUI_TEST_CHECK(plan.custom_capacity == 8);
        ERUI_TEST_CHECK(plan.pages.slices.size() == 2);
        assert_slice(plan.pages.slices[0], 0, 7, false, true, 0, 2);
        assert_slice(plan.pages.slices[1], 7, 5, true, false, 1, 2);
        assert_plan_is_contiguous(plan.pages, 12, 15);
    }

    {
        const RootPagePlan plan = paginate_root_page(0, 34, 7);
        ERUI_TEST_CHECK(plan.pages.slices.size() == 4);
        assert_slice(plan.pages.slices[0], 0, 2, false, true, 0, 4);
        assert_slice(plan.pages.slices[1], 2, 13, true, true, 1, 4);
        assert_slice(plan.pages.slices[2], 15, 13, true, true, 2, 4);
        assert_slice(plan.pages.slices[3], 28, 6, true, false, 3, 4);
        assert_plan_is_contiguous(plan.pages, 34, 15);
    }

    {
        bool rejected = false;
        try {
            (void)paginate_root_page(0, 1, 4);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        ERUI_TEST_CHECK(rejected);
    }

    // Built-in pages expose their remaining capacity only after Elden Ring
    // has materialized the vanilla rows, so production uses the allocation-
    // free resolver rather than a compile-time PagePlan.
    {
        PageSlice slice{};
        ERUI_TEST_CHECK(!resolve_paginated_slice(9, 1, 0, 15, 0, slice));
    }

    {
        PageSlice slice{};
        ERUI_TEST_CHECK(resolve_paginated_slice(9, 3, 3, 15, 0, slice));
        assert_slice(slice, 0, 3, false, false, 0, 1);
        ERUI_TEST_CHECK(!resolve_paginated_slice(9, 3, 3, 15, 1, slice));
    }

    {
        PageSlice first{};
        PageSlice continuation{};
        ERUI_TEST_CHECK(resolve_paginated_slice(9, 3, 1, 15, 0, first));
        ERUI_TEST_CHECK(resolve_paginated_slice(
            9, 3, 1, 15, 1, continuation));
        // With one free native slot the first page contains only Next. No
        // client row is dropped; all content moves to the continuation.
        assert_slice(first, 0, 0, false, true, 0, 2);
        assert_slice(continuation, 0, 3, true, false, 1, 2);
    }

    {
        PageSlice first{};
        PageSlice middle{};
        PageSlice last{};
        ERUI_TEST_CHECK(resolve_paginated_slice(9, 29, 3, 15, 0, first));
        ERUI_TEST_CHECK(resolve_paginated_slice(9, 29, 3, 15, 1, middle));
        ERUI_TEST_CHECK(resolve_paginated_slice(9, 29, 3, 15, 2, last));
        assert_slice(first, 0, 2, false, true, 0, 3);
        assert_slice(middle, 2, 13, true, true, 1, 3);
        assert_slice(last, 15, 14, true, false, 2, 3);
    }

    // Exercise a broad matrix to catch gaps, overlaps, empty overflow slices,
    // and navigation rows exceeding the native capacity.
    for (std::size_t capacity = 3; capacity <= 32; ++capacity) {
        for (std::size_t rows = 0; rows <= 512; ++rows) {
            const PagePlan plan = paginate_subpage(7, rows, capacity);
            assert_plan_is_contiguous(plan, rows, capacity);
            for (const PageSlice& slice : plan.slices) {
                ERUI_TEST_CHECK(slice.logical_page_index == 7);
                if (plan.slices.size() > 1) {
                    ERUI_TEST_CHECK(slice.row_count != 0);
                }
            }
        }
    }

    constexpr PageRoute a = PageRoute::submenu(4, 1);
    constexpr PageRoute b = PageRoute::submenu(4, 2);
    constexpr PageRoute c = PageRoute::root_continuation(0, 7, 1);
    constexpr PageRoute d = PageRoute::root_continuation(0, 6, 1);
    constexpr PageRoute camera_a =
        PageRoute::builtin_continuation(8, 1, 3, 1);
    constexpr PageRoute camera_b =
        PageRoute::builtin_continuation(8, 1, 3, 2);
    constexpr PageRoute sound =
        PageRoute::builtin_continuation(8, 3, 3, 1);
    constexpr PageRoute camera_other_capacity =
        PageRoute::builtin_continuation(8, 1, 4, 1);
    static_assert(same_native_page_domain(a, b));
    static_assert(!same_native_page_domain(a, c));
    static_assert(!same_native_page_domain(c, d));
    static_assert(same_native_page_domain(camera_a, camera_b));
    static_assert(!same_native_page_domain(camera_a, sound));
    static_assert(!same_native_page_domain(
        camera_a, camera_other_capacity));
    static_assert(is_previous_route_target(
        PageRoute::submenu(4, 2), PageRoute::submenu(4, 1)));
    static_assert(!is_previous_route_target(
        PageRoute::submenu(4, 2), PageRoute::submenu(4, 0)));
    static_assert(is_previous_route_target(
        PageRoute::root_continuation(0, 7, 1),
        PageRoute::root_main(0, 7)));
    static_assert(!is_previous_route_target(
        PageRoute::root_continuation(0, 7, 1),
        PageRoute::root_main(0, 6)));
    static_assert(is_previous_route_target(
        PageRoute::builtin_continuation(8, 1, 3, 1),
        PageRoute::builtin_main(8, 1, 3)));
    static_assert(!is_previous_route_target(
        PageRoute::builtin_continuation(8, 1, 3, 1),
        PageRoute::builtin_main(8, 3, 3)));

    return 0;
}
