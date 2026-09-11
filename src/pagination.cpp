#include "pagination.hpp"

#include <algorithm>
#include <stdexcept>

namespace erui::detail {
namespace {

std::size_t count_slices(
    std::size_t logical_row_count,
    std::size_t first_page_capacity,
    std::size_t continuation_capacity) noexcept {
    if (first_page_capacity == 0 || continuation_capacity < 2) return 0;
    if (logical_row_count <= first_page_capacity) return 1;

    const std::size_t first_content_capacity = first_page_capacity - 1;
    std::size_t remaining = logical_row_count -
        std::min(logical_row_count, first_content_capacity);
    std::size_t count = 1;
    while (remaining != 0) {
        const std::size_t last_content_capacity = continuation_capacity - 1;
        if (remaining <= last_content_capacity) return count + 1;
        const std::size_t middle_content_capacity = continuation_capacity - 2;
        if (middle_content_capacity == 0) return 0;
        remaining -= std::min(remaining, middle_content_capacity);
        ++count;
    }
    return count;
}

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

    const std::size_t slice_count = count_slices(
        logical_row_count, first_page_capacity, continuation_capacity);
    if (slice_count == 0) {
        throw std::invalid_argument(
            "continuation page needs room for content and navigation");
    }
    plan.slices.reserve(slice_count);
    for (std::size_t slice_index = 0; slice_index < slice_count; ++slice_index) {
        PageSlice slice{};
        if (!resolve_paginated_slice(
                logical_page_index,
                logical_row_count,
                first_page_capacity,
                continuation_capacity,
                slice_index,
                slice)) {
            throw std::logic_error("pagination slice resolution failed");
        }
        plan.slices.push_back(slice);
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
            "Game Options has no room for an ERNativeUI row");
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

std::uint8_t derive_root_plan_capacity(
    std::uint8_t visual_capacity,
    std::uint64_t materialized_row_count) noexcept {
    if (visual_capacity < game_options_vanilla_capacity ||
        visual_capacity > game_options_max_visual_capacity ||
        materialized_row_count < game_options_vanilla_row_count ||
        materialized_row_count >= visual_capacity) {
        return 0;
    }

    const std::uint64_t free_slots =
        static_cast<std::uint64_t>(visual_capacity) -
        materialized_row_count;
    const std::uint64_t effective_capacity =
        game_options_vanilla_row_count + free_slots;
    return effective_capacity >= game_options_min_plan_capacity &&
            effective_capacity <= game_options_max_visual_capacity
        ? static_cast<std::uint8_t>(effective_capacity)
        : 0;
}

bool resolve_paginated_slice(
    std::size_t logical_page_index,
    std::size_t logical_row_count,
    std::size_t first_page_capacity,
    std::size_t continuation_capacity,
    std::size_t slice_index,
    PageSlice& output) noexcept {
    output = {};
    const std::size_t slice_count = count_slices(
        logical_row_count, first_page_capacity, continuation_capacity);
    if (slice_count == 0 || slice_index >= slice_count) return false;

    output.logical_page_index = logical_page_index;
    output.slice_index = slice_index;
    output.slice_count = slice_count;
    if (slice_index == 0) {
        const bool overflows = logical_row_count > first_page_capacity;
        output.first_row = 0;
        output.row_count = overflows
            ? first_page_capacity - 1
            : logical_row_count;
        output.has_previous = false;
        output.has_next = overflows;
        return true;
    }

    std::size_t consumed = first_page_capacity - 1;
    for (std::size_t current = 1; current <= slice_index; ++current) {
        const std::size_t remaining = logical_row_count - consumed;
        const bool final_page = remaining <= continuation_capacity - 1;
        const std::size_t content_capacity = final_page
            ? continuation_capacity - 1
            : continuation_capacity - 2;
        const std::size_t content_count =
            std::min(remaining, content_capacity);
        if (current == slice_index) {
            output.first_row = consumed;
            output.row_count = content_count;
            output.has_previous = true;
            output.has_next = !final_page;
            return true;
        }
        consumed += content_count;
    }
    return false;
}

} // namespace erui::detail
