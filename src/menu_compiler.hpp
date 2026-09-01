#pragma once

#include "pagination.hpp"
#include "popup_choice_state.hpp"
#include "text_registry.hpp"

#include "menu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace erui::detail {

inline constexpr std::size_t invalid_compiled_index =
    static_cast<std::size_t>(-1);

enum class TextRole : std::uint8_t {
    menu_title,
    menu_help,
    page_title,
    page_help,
    page_outer_title,
    physical_page_title,
    row_label,
    row_help,
    pagination_label,
    pagination_help,
};

struct TextBinding {
    TextRole role{TextRole::menu_title};
    std::size_t page_index{invalid_compiled_index};
    std::size_t row_index{invalid_compiled_index};
    std::size_t slice_index{invalid_compiled_index};
};

struct CompiledRow {
    RowKind kind{RowKind::toggle};
    TextId label_id{};
    TextId help_id{};
    volatile std::uint8_t* byte_value{};
    SliderSpec slider{};
    std::vector<TextId> choice_ids{};
    TextInputState* text_input_state{};
    Action action{};
    ValueAction value_action{};
    TextAction text_action{};
    std::size_t target_page_index{invalid_compiled_index};
    bool enabled{true};
    mutable PopupChoiceNativeState popup_choice_state{};
    std::uint8_t last_observed_value{};
};

struct CompiledPage {
    PageKind kind{PageKind::root};
    TextId title_id{};
    TextId help_id{};
    TextId outer_title_id{};
    std::vector<TextId> physical_title_ids{};
    std::vector<CompiledRow> rows{};
};

struct ResolvedPagePresentation {
    TextId outer_title_id{};
    TextId page_title_id{};
};

struct CompiledMenu {
    TextRegistry texts{};
    TextId title_id{};
    TextId help_id{};
    TextId previous_page_label_id{};
    TextId previous_page_help_id{};
    TextId next_page_label_id{};
    TextId next_page_help_id{};
    std::vector<CompiledPage> pages{};
    std::vector<bool> reachable_pages{};
    std::vector<std::size_t> tab_page_indices{};
    std::unordered_map<TextId, TextBinding> text_bindings{};
    std::vector<PagePlan> page_plans{};
    std::array<RootPagePlan, controller_capacity_count> root_plans{};
    std::array<std::vector<TextId>, controller_capacity_count>
        root_physical_title_ids{};
    std::size_t root_page_index{};
    std::size_t modeled_button_count{};
    std::size_t modeled_submenu_count{};
    std::size_t modeled_popup_choice_count{};
    std::size_t modeled_text_input_count{};
    bool pagination_required{};

    [[nodiscard]] CompiledPage& root_page() noexcept { return pages[root_page_index]; }
    [[nodiscard]] const CompiledPage& root_page() const noexcept { return pages[root_page_index]; }

    [[nodiscard]] const TextBinding* text_binding(TextId id) const noexcept {
        const auto found = text_bindings.find(id);
        return found == text_bindings.end() ? nullptr : &found->second;
    }

    [[nodiscard]] const RootPagePlan& root_plan(
        std::uint8_t native_capacity) const noexcept {
        const std::uint8_t accepted =
            native_capacity >= controller_vanilla_capacity &&
                    native_capacity <= controller_max_visual_capacity
                ? native_capacity
                : controller_vanilla_capacity;
        return root_plans[accepted - controller_vanilla_capacity];
    }

    [[nodiscard]] bool page_reachable(std::size_t page_index) const noexcept {
        return page_index < reachable_pages.size() && reachable_pages[page_index];
    }

    [[nodiscard]] bool pagination_required_for_capacity(
        std::uint8_t native_capacity) const noexcept {
        if (root_plan(native_capacity).pages.slices.size() > 1) return true;
        for (std::size_t index = 0; index < page_plans.size(); ++index) {
            if (index != root_page_index && page_reachable(index) &&
                page_plans[index].slices.size() > 1) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const PagePlan* page_plan(
        std::size_t logical_page_index) const noexcept {
        return logical_page_index < page_plans.size()
            ? &page_plans[logical_page_index]
            : nullptr;
    }

    [[nodiscard]] const PageSlice* resolve_slice(
        const PageRoute& route) const noexcept {
        if (!route.valid() || route.logical_page_index >= pages.size() ||
            !page_reachable(route.logical_page_index)) {
            return nullptr;
        }
        switch (route.kind) {
        case PageRouteKind::submenu: {
            const PagePlan* plan = page_plan(route.logical_page_index);
            return plan ? plan->slice(route.slice_index) : nullptr;
        }
        case PageRouteKind::root_continuation:
        case PageRouteKind::root_main:
            if (route.logical_page_index != root_page_index) {
                return nullptr;
            }
            return root_plan(route.root_capacity).slice(route.slice_index);
        }
        return nullptr;
    }

    [[nodiscard]] ResolvedPagePresentation resolve_presentation(
        const PageRoute& route) const noexcept {
        if (route.kind == PageRouteKind::root_main || !resolve_slice(route)) {
            return {};
        }
        if (route.kind == PageRouteKind::submenu) {
            const CompiledPage& page = pages[route.logical_page_index];
            if (route.slice_index >= page.physical_title_ids.size()) return {};
            return {
                .outer_title_id = page.outer_title_id,
                .page_title_id = page.physical_title_ids[route.slice_index],
            };
        }
        const std::uint8_t capacity =
            route.root_capacity >= controller_vanilla_capacity &&
                    route.root_capacity <= controller_max_visual_capacity
                ? route.root_capacity
                : controller_vanilla_capacity;
        const std::vector<TextId>& titles =
            root_physical_title_ids[capacity - controller_vanilla_capacity];
        if (route.slice_index >= titles.size()) return {};
        return {
            .outer_title_id = title_id,
            .page_title_id = titles[route.slice_index],
        };
    }
};

class MenuCompiler {
public:
    static std::unique_ptr<CompiledMenu> compile(Menu& menu);
};

} // namespace erui::detail
