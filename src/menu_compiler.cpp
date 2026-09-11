#include "menu_compiler.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace erui::detail {
namespace {

void register_text_binding(
    CompiledMenu& menu,
    TextId id,
    TextRole role,
    std::size_t page_index = invalid_compiled_index,
    std::size_t row_index = invalid_compiled_index,
    std::size_t slice_index = invalid_compiled_index) {
    const auto [_, inserted] = menu.text_bindings.emplace(
        id,
        TextBinding{
            .role = role,
            .page_index = page_index,
            .row_index = row_index,
            .slice_index = slice_index,
        });
    if (!inserted) {
        throw std::logic_error("compiled custom text ID was registered twice");
    }
}

std::wstring default_physical_title(
    std::wstring_view base_title,
    std::size_t page_number,
    std::size_t page_count) {
    if (page_count <= 1) return std::wstring(base_title);
    std::wstring result(base_title);
    result.append(L" (");
    result.append(std::to_wstring(page_number));
    result.push_back(L'/');
    result.append(std::to_wstring(page_count));
    result.push_back(L')');
    return result;
}

bool valid_formatted_title(std::wstring_view title) noexcept {
    return !title.empty() && title.find(L'\0') == std::wstring_view::npos;
}

TextId compile_physical_title(
    CompiledMenu& compiled,
    const Page& input,
    const CompiledPage& output,
    std::size_t page_index,
    std::size_t slice_index,
    std::size_t slice_count) {
    const PagePresentationSpec& presentation = input.presentation();
    const std::wstring_view base_title = presentation.page_title.empty()
        ? input.title()
        : std::wstring_view(presentation.page_title);
    std::wstring title{};
    bool formatted = false;
    if (presentation.formatter) {
        formatted = presentation.formatter(
            PageTitleFormatRequest{
                .base_title = base_title,
                .page_number = slice_index + 1,
                .page_count = slice_count,
            },
            title,
            presentation.user_data);
        formatted = formatted && valid_formatted_title(title);
    }
    if (!formatted) {
        if (slice_count == 1) return output.title_id;
        title = default_physical_title(base_title, slice_index + 1, slice_count);
    }
    const TextId id = compiled.texts.add(title);
    register_text_binding(
        compiled,
        id,
        TextRole::physical_page_title,
        page_index,
        invalid_compiled_index,
        slice_index);
    return id;
}

void compile_physical_titles(
    CompiledMenu& compiled,
    const Page& input,
    const CompiledPage& output,
    std::size_t page_index,
    std::size_t slice_count,
    std::vector<TextId>& destination) {
    destination.reserve(slice_count);
    for (std::size_t slice_index = 0; slice_index < slice_count; ++slice_index) {
        destination.push_back(compile_physical_title(
            compiled, input, output, page_index, slice_index, slice_count));
    }
}

void compile_builtin_physical_titles(
    CompiledMenu& compiled,
    const Page& input,
    CompiledPage& output,
    std::size_t page_index) {
    auto plan = std::make_unique<BuiltinPhysicalTitlePlan>();
    const std::size_t logical_row_count = output.rows.size();
    for (std::uint8_t first_page_capacity = 1;
         first_page_capacity <= builtin_page_max_first_capacity;
         ++first_page_capacity) {
        PageSlice first_slice{};
        if (!resolve_paginated_slice(
                page_index,
                logical_row_count,
                first_page_capacity,
                native_subpage_capacity,
                0,
                first_slice)) {
            throw std::logic_error(
                "built-in page title plan could not resolve its first slice");
        }
        // The native main page keeps Elden Ring's localized title. It needs no
        // ERNativeUI title metadata unless a continuation actually exists.
        if (first_slice.slice_count <= 1) continue;

        const auto existing = std::find_if(
            plan->variants.begin(),
            plan->variants.end(),
            [&](const BuiltinPhysicalTitleVariant& variant) {
                return variant.slice_count == first_slice.slice_count;
            });
        std::size_t variant_index{};
        if (existing == plan->variants.end()) {
            BuiltinPhysicalTitleVariant variant{};
            variant.slice_count = first_slice.slice_count;
            compile_physical_titles(
                compiled,
                input,
                output,
                page_index,
                first_slice.slice_count,
                variant.title_ids);
            plan->variants.push_back(std::move(variant));
            variant_index = plan->variants.size() - 1;
        } else {
            variant_index = static_cast<std::size_t>(
                std::distance(plan->variants.begin(), existing));
        }
        if (variant_index >= BuiltinPhysicalTitlePlan::invalid_variant) {
            throw std::logic_error(
                "built-in page title variant range was exhausted");
        }
        plan->variant_by_capacity[first_page_capacity] =
            static_cast<std::uint8_t>(variant_index);
    }
    if (!plan->variants.empty()) {
        output.builtin_physical_title_plan = std::move(plan);
    }
}

} // namespace

std::unique_ptr<CompiledMenu> MenuCompiler::compile(Menu& menu) {
    if (menu.pages_.empty() || !menu.root_page_) {
        throw std::logic_error("menu has no root page");
    }

    auto compiled = std::make_unique<CompiledMenu>();
    compiled->title_id = compiled->texts.add(menu.title_);
    compiled->help_id = compiled->texts.add(menu.help_);
    register_text_binding(*compiled, compiled->title_id, TextRole::menu_title);
    register_text_binding(*compiled, compiled->help_id, TextRole::menu_help);
    compiled->pages.reserve(menu.pages_.size());

    std::size_t estimated_text_count = 6 + menu.pages_.size() * 2;
    for (const auto& page : menu.pages_) {
        estimated_text_count += page->rows_.size() * 2;
    }
    for (const auto& section : menu.input_binding_sections_) {
        estimated_text_count += 1 + section->bindings_.size();
    }
    compiled->text_bindings.reserve(estimated_text_count);

    std::unordered_map<const Page*, std::size_t> page_indices;
    page_indices.reserve(menu.pages_.size());

    for (std::size_t index = 0; index < menu.pages_.size(); ++index) {
        const Page* page = menu.pages_[index].get();
        page_indices.emplace(page, index);

        CompiledPage output{};
        output.kind = page->kind_;
        const std::wstring_view effective_page_title =
            page->presentation_.page_title.empty()
                ? std::wstring_view(page->title_)
                : std::wstring_view(page->presentation_.page_title);
        output.title_id = compiled->texts.add(effective_page_title);
        output.help_id = compiled->texts.add(page->help_);
        output.outer_title_id = compiled->title_id;
        if (page->kind_ != PageKind::root &&
            !page->presentation_.menu_title.empty()) {
            output.outer_title_id = compiled->texts.add(
                page->presentation_.menu_title);
            register_text_binding(
                *compiled,
                output.outer_title_id,
                TextRole::page_outer_title,
                index);
        }
        output.rows.reserve(page->rows_.size());
        register_text_binding(
            *compiled,
            output.title_id,
            TextRole::page_title,
            index);
        register_text_binding(
            *compiled,
            output.help_id,
            TextRole::page_help,
            index);
        compiled->pages.push_back(std::move(output));
    }

    const auto root_it = page_indices.find(menu.root_page_);
    if (root_it == page_indices.end()) {
        throw std::logic_error("root page is not owned by its menu");
    }
    compiled->root_page_index = root_it->second;

    for (std::size_t destination_index = 0;
         destination_index < menu.builtin_pages_.size();
         ++destination_index) {
        const Page* page = menu.builtin_pages_[destination_index];
        if (!page) continue;
        const auto found = page_indices.find(page);
        if (found == page_indices.end()) {
            throw std::logic_error(
                "built-in page is not owned by its menu");
        }
        const auto destination = static_cast<BuiltinPage>(destination_index);
        const std::uint8_t native_id = native_category_id(destination);
        if (native_id >= compiled->builtin_page_indices.size()) {
            throw std::logic_error("built-in page has no native category");
        }
        compiled->builtin_page_indices[native_id] = found->second;
        compiled->pages[found->second].builtin_page = destination;
    }
    if (compiled->builtin_page_index(native_category_id(
            BuiltinPage::game_options)) != compiled->root_page_index) {
        throw std::logic_error("Game Options page is not the legacy root");
    }

    for (const Page* tab : menu.tab_pages_) {
        const auto tab_it = page_indices.find(tab);
        if (tab_it == page_indices.end()) {
            throw std::logic_error("tab page is not owned by its menu");
        }
        compiled->tab_page_indices.push_back(tab_it->second);
    }

    for (std::size_t page_index = 0; page_index < menu.pages_.size(); ++page_index) {
        const Page& input = *menu.pages_[page_index];
        CompiledPage& output = compiled->pages[page_index];

        for (std::size_t row_index = 0; row_index < input.rows_.size(); ++row_index) {
            const Page::RowDefinition& row = input.rows_[row_index];
            // Elden Ring's action-row constructor has no disabled state.
            // Disabled buttons/submenus are therefore omitted consistently
            // from both materialization and pagination.
            if (!row.enabled &&
                (row.kind == RowKind::button ||
                    row.kind == RowKind::color_picker ||
                    row.kind == RowKind::submenu)) {
                continue;
            }
            const std::size_t compiled_row_index = output.rows.size();
            CompiledRow compiled_row{};
            compiled_row.kind = row.kind;
            compiled_row.label_id = compiled->texts.add(row.label);
            compiled_row.help_id = compiled->texts.add(row.help);
            register_text_binding(
                *compiled,
                compiled_row.label_id,
                TextRole::row_label,
                page_index,
                compiled_row_index);
            register_text_binding(
                *compiled,
                compiled_row.help_id,
                TextRole::row_help,
                page_index,
                compiled_row_index);
            compiled_row.byte_value = row.byte_value;
            compiled_row.slider = row.slider;
            for (const auto& choice : row.choices) {
                compiled_row.choice_ids.push_back(compiled->texts.add(choice));
            }
            compiled_row.action = row.action;
            compiled_row.value_action = row.value_action;
            compiled_row.text_input_state = row.text_input_state;
            compiled_row.text_action = row.text_action;
            compiled_row.color_picker_state = row.color_picker_state;
            compiled_row.color_action = row.color_action;
            compiled_row.enabled = row.enabled;
            if (row.kind == RowKind::popup_choice && row.byte_value) {
                compiled_row.popup_choice_state.set_public_selection(
                    *row.byte_value);
            }
            compiled_row.last_observed_value = row.byte_value ? *row.byte_value : 0;

            if (row.target_page) {
                const auto target_it = page_indices.find(row.target_page);
                if (target_it == page_indices.end()) {
                    throw std::logic_error("submenu target is not owned by its menu");
                }
                compiled_row.target_page_index = target_it->second;
            }

            output.rows.push_back(compiled_row);
        }
    }

    compiled->input_binding_sections.reserve(
        menu.input_binding_sections_.size());
    for (std::size_t section_index = 0;
         section_index < menu.input_binding_sections_.size();
         ++section_index) {
        const InputBindingSection& input =
            *menu.input_binding_sections_[section_index];
        if (input.bindings_.empty()) continue;

        CompiledInputBindingSection output{};
        output.label_id = compiled->texts.add(input.label_);
        register_text_binding(
            *compiled,
            output.label_id,
            TextRole::input_binding_section,
            section_index);
        output.bindings.reserve(input.bindings_.size());
        for (std::size_t binding_index = 0;
             binding_index < input.bindings_.size();
             ++binding_index) {
            const InputBindingSection::Definition& definition =
                input.bindings_[binding_index];
            const TextId label_id = compiled->texts.add(definition.label);
            register_text_binding(
                *compiled,
                label_id,
                TextRole::input_binding_label,
                section_index,
                binding_index);
            output.bindings.push_back({
                .handle = definition.handle,
                .provider_id = input.provider_id_,
                .binding_id = definition.binding_id,
                .label_id = label_id,
                .default_inputs = definition.default_inputs,
                .current_inputs = definition.current_inputs,
                .action = definition.action,
                .assignments_changed = definition.assignments_changed,
            });
            ++compiled->modeled_input_binding_count;
        }
        compiled->input_binding_sections.push_back(std::move(output));
    }

    compiled->reachable_pages.assign(compiled->pages.size(), false);
    std::vector<std::size_t> pending_pages{};
    pending_pages.reserve(compiled->pages.size());
    const auto mark_reachable = [&](std::size_t index) {
        if (index < compiled->reachable_pages.size() &&
            !compiled->reachable_pages[index]) {
            compiled->reachable_pages[index] = true;
            pending_pages.push_back(index);
        }
    };
    mark_reachable(compiled->root_page_index);
    for (const std::size_t index : compiled->builtin_page_indices) {
        if (index != invalid_compiled_index) mark_reachable(index);
    }
    for (const std::size_t index : compiled->tab_page_indices) mark_reachable(index);
    for (std::size_t cursor = 0; cursor < pending_pages.size(); ++cursor) {
        const std::size_t page_index = pending_pages[cursor];
        for (const CompiledRow& row : compiled->pages[page_index].rows) {
            if (row.kind == RowKind::submenu && row.enabled &&
                row.target_page_index != invalid_compiled_index) {
                mark_reachable(row.target_page_index);
            }
        }
    }
    for (std::size_t page_index = 0; page_index < compiled->pages.size(); ++page_index) {
        if (!compiled->page_reachable(page_index)) continue;
        for (const CompiledRow& row : compiled->pages[page_index].rows) {
            if (row.kind == RowKind::button) ++compiled->modeled_button_count;
            else if (row.kind == RowKind::submenu) ++compiled->modeled_submenu_count;
            else if (row.kind == RowKind::popup_choice) {
                ++compiled->modeled_popup_choice_count;
            } else if (row.kind == RowKind::text_input) {
                ++compiled->modeled_text_input_count;
            } else if (row.kind == RowKind::color_picker) {
                ++compiled->modeled_color_picker_count;
            }
        }
    }

    compiled->previous_page_label_id = compiled->texts.add(
        menu.localization_.previous_label);
    compiled->previous_page_help_id = compiled->texts.add(
        menu.localization_.previous_help);
    compiled->next_page_label_id = compiled->texts.add(
        menu.localization_.next_label);
    compiled->next_page_help_id = compiled->texts.add(
        menu.localization_.next_help);
    register_text_binding(
        *compiled,
        compiled->previous_page_label_id,
        TextRole::pagination_label);
    register_text_binding(
        *compiled,
        compiled->previous_page_help_id,
        TextRole::pagination_help);
    register_text_binding(
        *compiled,
        compiled->next_page_label_id,
        TextRole::pagination_label);
    register_text_binding(
        *compiled,
        compiled->next_page_help_id,
        TextRole::pagination_help);

    compiled->page_plans.resize(compiled->pages.size());
    for (std::size_t page_index = 0; page_index < compiled->pages.size(); ++page_index) {
        if (compiled->is_builtin_page_index(page_index)) {
            continue;
        }
        compiled->page_plans[page_index] = paginate_subpage(
            page_index,
            compiled->pages[page_index].rows.size());
        if (compiled->page_reachable(page_index) &&
            compiled->page_plans[page_index].slices.size() > 1) {
            compiled->pagination_required = true;
        }
    }

    for (std::uint8_t capacity = game_options_min_plan_capacity;
         capacity <= game_options_max_visual_capacity; ++capacity) {
        RootPagePlan& plan = compiled->root_plans[
            capacity - game_options_min_plan_capacity];
        plan = paginate_root_page(
            compiled->root_page_index,
            compiled->root_page().rows.size(),
            capacity);
        if (plan.pages.slices.size() > 1) {
            compiled->pagination_required = true;
        }
    }

    for (std::size_t page_index = 0; page_index < compiled->pages.size(); ++page_index) {
        if (compiled->is_builtin_page_index(page_index)) continue;
        compile_physical_titles(
            *compiled,
            *menu.pages_[page_index],
            compiled->pages[page_index],
            page_index,
            compiled->page_plans[page_index].slices.size(),
            compiled->pages[page_index].physical_title_ids);
    }
    for (std::uint8_t capacity = game_options_min_plan_capacity;
         capacity <= game_options_max_visual_capacity; ++capacity) {
        const std::size_t index = capacity - game_options_min_plan_capacity;
        compile_physical_titles(
            *compiled,
            *menu.root_page_,
            compiled->root_page(),
            compiled->root_page_index,
            compiled->root_plans[index].pages.slices.size(),
            compiled->root_physical_title_ids[index]);
    }
    // Category zero is Game Options and already uses the root-title plans
    // above. Other built-in panels reveal their free capacity only after the
    // native materializer runs, so compile deduplicated variants for every
    // capacity accepted by the runtime validator.
    for (std::uint8_t native_category = 1;
         native_category < compiled->builtin_page_indices.size();
         ++native_category) {
        const std::size_t page_index =
            compiled->builtin_page_index(native_category);
        if (page_index >= compiled->pages.size()) continue;
        compile_builtin_physical_titles(
            *compiled,
            *menu.pages_[page_index],
            compiled->pages[page_index],
            page_index);
    }

    compiled->texts.freeze();
    menu.freeze();
    return compiled;
}

} // namespace erui::detail
