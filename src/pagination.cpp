#include "pagination.hpp"

#include <algorithm>
#include <stdexcept>

namespace erui::detail {
namespace {

PagePlan paginate_with_first_capacity(
    std::size_t logical_page_index,
    std::size_t logical_row_count,
    std::size_t first_page_capacity,
    std::size_t continuation_capacity) {
    if (first_page_capacity == 0 || continuation_capacity < 2) {
        throw std::invalid_argument("pagination capacity is too small");
    }

    PagePlan plan{};
    plan.logical_page_index = logical_page_index;
    plan.physical_capacity = continuation_capacity;

    if (logical_row_count <= first_page_capacity) {
        plan.slices.push_back(PageSlice{
            .logical_page_index = logical_page_index,
            .slice_index = 0,
            .slice_count = 1,
            .first_row = 0,
            .row_count = logical_row_count,
            .has_previous = false,
            .has_next = false,
        });
        return plan;
    }

    // The first page has no Previous row, so only one slot is reserved for
    // Next. Every middle page reserves both Previous and Next. The final page
    // reserves only Previous.
    const std::size_t first_content_capacity = first_page_capacity - 1;
    if (first_content_capacity == 0) {
        throw std::invalid_argument(
            "overflowing first page needs one content slot and one Next slot");
    }

    std::size_t consumed = std::min(logical_row_count, first_content_capacity);
    plan.slices.push_back(PageSlice{
        .logical_page_index = logical_page_index,
        .slice_index = 0,
        .first_row = 0,
        .row_count = consumed,
        .has_previous = false,
        .has_next = true,
    });

    while (consumed < logical_row_count) {
        const std::size_t remaining = logical_row_count - consumed;
        const std::size_t last_page_content_capacity = continuation_capacity - 1;
        const bool final_page = remaining <= last_page_content_capacity;
        const std::size_t content_capacity = final_page
            ? last_page_content_capacity
            : continuation_capacity - 2;
        if (content_capacity == 0) {
            throw std::invalid_argument(
                "continuation page needs room for content and navigation");
        }

        const std::size_t content_count = std::min(remaining, content_capacity);
        plan.slices.push_back(PageSlice{
            .logical_page_index = logical_page_index,
            .slice_index = plan.slices.size(),
            .first_row = consumed,
            .row_count = content_count,
            .has_previous = true,
            .has_next = !final_page,
        });
        consumed += content_count;
    }

    const std::size_t slice_count = plan.slices.size();
    for (PageSlice& slice : plan.slices) {
        slice.slice_count = slice_count;
    }
    return plan;
}

} // namespace

PagePlan paginate_subpage(
    std::size_t logical_page_index,
    std::size_t logical_row_count,
    std::size_t physical_capacity) {
    return paginate_with_first_capacity(
        logical_page_index,
        logical_row_count,
        physical_capacity,
        physical_capacity);
}

RootPagePlan paginate_root_page(
    std::size_t root_page_index,
    std::size_t logical_row_count,
    std::uint8_t native_capacity,
    std::size_t vanilla_row_count,
    std::size_t continuation_capacity) {
    if (native_capacity <= vanilla_row_count) {
        throw std::invalid_argument(
            "Controller Settings has no room for an ERNativeUI row");
    }

    RootPagePlan root{};
    root.native_capacity = native_capacity;
    root.vanilla_row_count = vanilla_row_count;
    root.custom_capacity =
        static_cast<std::size_t>(native_capacity) - vanilla_row_count;
    root.pages = paginate_with_first_capacity(
        root_page_index,
        logical_row_count,
        root.custom_capacity,
        continuation_capacity);
    return root;
}

} // namespace erui::detail
