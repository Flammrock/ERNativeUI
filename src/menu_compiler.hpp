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
    input_binding_section,
    input_binding_label,
};

struct CompiledInputBinding {
    ERUI_InputActionHandle handle{};
    std::string provider_id{};
    std::string binding_id{};
    TextId label_id{};
    ERUI_ActionInputs default_inputs{};
    ERUI_ActionInputs current_inputs{};
    InputBindingAction action{};
    InputAssignmentsAction assignments_changed{};
};

struct CompiledInputBindingSection {
    TextId label_id{};
    std::vector<CompiledInputBinding> bindings{};
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
    ColorPickerState* color_picker_state{};
    Action action{};
    ValueAction value_action{};
    TextAction text_action{};
    ColorAction color_action{};
    std::size_t target_page_index{invalid_compiled_index};
    bool enabled{true};
    mutable PopupChoiceNativeState popup_choice_state{};
    std::uint8_t last_observed_value{};
};

struct BuiltinPhysicalTitleVariant {
    std::size_t slice_count{};
    std::vector<TextId> title_ids{};
};

// The number of built-in physical slices depends on live free capacity, which
// is unavailable during menu compilation. Precompile one immutable title
// vector per distinct slice count and map every accepted capacity to it.
// Capacities that produce the same count share a variant, while UI callbacks
// perform only bounded array/vector reads and never allocate.
struct BuiltinPhysicalTitlePlan {
    static constexpr std::uint8_t invalid_variant = 0xFF;

    BuiltinPhysicalTitlePlan() noexcept {
        variant_by_capacity.fill(invalid_variant);
    }

    [[nodiscard]] const std::vector<TextId>* titles_for(
        std::uint8_t first_page_capacity,
        std::size_t slice_count) const noexcept {
        if (first_page_capacity == 0 ||
            first_page_capacity > builtin_page_max_first_capacity) {
            return nullptr;
        }
        const std::uint8_t variant_index =
            variant_by_capacity[first_page_capacity];
        if (variant_index == invalid_variant ||
            variant_index >= variants.size() ||
            variants[variant_index].slice_count != slice_count) {
            return nullptr;
        }
        return &variants[variant_index].title_ids;
    }

    std::array<std::uint8_t, builtin_page_max_first_capacity + 1>
        variant_by_capacity{};
    std::vector<BuiltinPhysicalTitleVariant> variants{};
};

static_assert(
    builtin_page_max_first_capacity <
    BuiltinPhysicalTitlePlan::invalid_variant);

struct CompiledPage {
    PageKind kind{PageKind::root};
    BuiltinPage builtin_page{BuiltinPage::count};
    TextId title_id{};
    TextId help_id{};
    TextId outer_title_id{};
    std::vector<TextId> physical_title_ids{};
    std::unique_ptr<BuiltinPhysicalTitlePlan> builtin_physical_title_plan{};
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
    std::vector<CompiledInputBindingSection> input_binding_sections{};
    std::vector<bool> reachable_pages{};
    std::vector<std::size_t> tab_page_indices{};
    std::array<std::size_t, native_category_count> builtin_page_indices{
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
        invalid_compiled_index,
    };
    std::unordered_map<TextId, TextBinding> text_bindings{};
    std::vector<PagePlan> page_plans{};
    std::array<RootPagePlan, game_options_capacity_count> root_plans{};
    std::array<std::vector<TextId>, game_options_capacity_count>
        root_physical_title_ids{};
    std::size_t root_page_index{};
    std::size_t modeled_button_count{};
    std::size_t modeled_submenu_count{};
    std::size_t modeled_popup_choice_count{};
    std::size_t modeled_text_input_count{};
    std::size_t modeled_color_picker_count{};
    std::size_t modeled_input_binding_count{};
    bool pagination_required{};

    [[nodiscard]] CompiledPage& root_page() noexcept { return pages[root_page_index]; }
    [[nodiscard]] const CompiledPage& root_page() const noexcept { return pages[root_page_index]; }

    [[nodiscard]] std::size_t builtin_page_index(
        std::uint8_t native_category) const noexcept {
        return native_category < builtin_page_indices.size()
            ? builtin_page_indices[native_category]
            : invalid_compiled_index;
    }

    [[nodiscard]] CompiledPage* builtin_page(
        std::uint8_t native_category) noexcept {
        const std::size_t index = builtin_page_index(native_category);
        return index < pages.size() ? &pages[index] : nullptr;
    }

    [[nodiscard]] const CompiledPage* builtin_page(
        std::uint8_t native_category) const noexcept {
        const std::size_t index = builtin_page_index(native_category);
        return index < pages.size() ? &pages[index] : nullptr;
    }

    [[nodiscard]] bool has_builtin_pages() const noexcept {
        for (const std::size_t index : builtin_page_indices) {
            if (index < pages.size() && !pages[index].rows.empty()) return true;
        }
        return false;
    }

    [[nodiscard]] bool is_builtin_page_index(
        std::size_t page_index) const noexcept {
        return page_index < pages.size() &&
            valid_builtin_page(pages[page_index].builtin_page);
    }

    [[nodiscard]] const TextBinding* text_binding(TextId id) const noexcept {
        const auto found = text_bindings.find(id);
        return found == text_bindings.end() ? nullptr : &found->second;
    }

    [[nodiscard]] const RootPagePlan& root_plan(
        std::uint8_t native_capacity) const noexcept {
        const std::uint8_t accepted =
            native_capacity >= game_options_min_plan_capacity &&
                    native_capacity <= game_options_max_visual_capacity
                ? native_capacity
                : game_options_vanilla_capacity;
        return root_plans[accepted - game_options_min_plan_capacity];
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
        case PageRouteKind::builtin_continuation:
        case PageRouteKind::builtin_main:
            // Built-in page capacities and slices are derived from the live
            // native panel. They intentionally are not compiler-owned.
            return nullptr;
        }
        return nullptr;
    }

    [[nodiscard]] ResolvedPagePresentation resolve_presentation(
        const PageRoute& route) const noexcept {
        if (route.kind == PageRouteKind::root_main ||
            route.kind == PageRouteKind::builtin_main) {
            return {};
        }
        if (route.kind == PageRouteKind::builtin_continuation) {
            if (!route.valid() || route.slice_index == 0 ||
                route.builtin_category ==
                    native_category_id(BuiltinPage::game_options) ||
                route.root_capacity == 0 ||
                route.root_capacity > builtin_page_max_first_capacity ||
                route.logical_page_index >= pages.size() ||
                builtin_page_index(route.builtin_category) !=
                    route.logical_page_index ||
                !page_reachable(route.logical_page_index)) {
                return {};
            }
            const CompiledPage& page = pages[route.logical_page_index];
            if (!page.builtin_physical_title_plan) return {};
            PageSlice slice{};
            if (!resolve_paginated_slice(
                    route.logical_page_index,
                    page.rows.size(),
                    route.root_capacity,
                    native_subpage_capacity,
                    route.slice_index,
                    slice) ||
                slice.slice_index == 0 || slice.slice_count <= 1) {
                return {};
            }
            const std::vector<TextId>* const titles =
                page.builtin_physical_title_plan->titles_for(
                    route.root_capacity, slice.slice_count);
            if (!titles || route.slice_index >= titles->size()) return {};
            return {
                .outer_title_id = page.outer_title_id,
                .page_title_id = (*titles)[route.slice_index],
            };
        }
        if (!resolve_slice(route)) return {};
        if (route.kind == PageRouteKind::submenu) {
            const CompiledPage& page = pages[route.logical_page_index];
            if (route.slice_index >= page.physical_title_ids.size()) return {};
            return {
                .outer_title_id = page.outer_title_id,
                .page_title_id = page.physical_title_ids[route.slice_index],
            };
        }
        const std::uint8_t capacity =
            route.root_capacity >= game_options_min_plan_capacity &&
                    route.root_capacity <= game_options_max_visual_capacity
                ? route.root_capacity
                : game_options_vanilla_capacity;
        const std::vector<TextId>& titles =
            root_physical_title_ids[
                capacity - game_options_min_plan_capacity];
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
