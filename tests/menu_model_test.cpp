#include "menu_compiler.hpp"

#include "menu.hpp"

#include "test_assertions.hpp"
#include <array>
#include <cstdint>
#include <cwchar>
#include <stdexcept>

namespace {

void changed(std::uint8_t, void*) noexcept {}
int button_press_count{};
void pressed(void*) noexcept { ++button_press_count; }
std::uint32_t observed_binding_devices{};
void binding_activated(std::uint32_t devices, void*) noexcept {
    observed_binding_devices = devices;
}
void binding_assignments_changed(
    const ERUI_ActionInputs&,
    const ERUI_ActionInputs&,
    ERUI_AssignmentChangeReason,
    ERUI_InputDevices,
    void*) noexcept {}

ERUI_ActionInputs all_unbound_inputs() noexcept {
    ERUI_ActionInputs result{};
    result.size = sizeof(result);
    result.controller.state = ERUI_INPUT_SLOT_UNBOUND;
    result.keyboard.state = ERUI_INPUT_SLOT_UNBOUND;
    result.mouse.state = ERUI_INPUT_SLOT_UNBOUND;
    return result;
}

} // namespace

int main() {
    volatile std::uint8_t enabled = 1;
    volatile std::uint8_t level = 50;
    volatile std::uint8_t mode = 1;
    volatile std::uint8_t popup_mode = 2;

    erui::Menu menu(L"Demo", L"Demo help");
    menu.root()
        .add_toggle(
            L"Enabled",
            L"Toggle help",
            enabled,
            true,
            {.callback = &changed})
        .add_slider(
            L"Level",
            L"Slider help",
            level,
            {.minimum = 0, .maximum = 100, .step = 5},
            true,
            {.callback = &changed})
        .add_inline_choice(
            L"Popup-style wording remains inline",
            L"Choice help",
            mode,
            {L"Off", L"Balanced", L"Strong"},
            true,
            {.callback = &changed})
        .add_popup_choice(
            L"Popup Mode",
            L"Popup choice help",
            popup_mode,
            {L"Minimal", L"Balanced", L"Maximum"},
            true,
            {.callback = &changed})
        .add_button(
            L"Action",
            L"Button help",
            {.callback = &pressed});

    erui::Page& child = menu.root().add_submenu(
        L"Advanced",
        L"Submenu help");
    child.add_button(
        L"Action",
        L"Button help",
        {.callback = &pressed});

    erui::Page& tab = menu.add_tab(L"Extra", L"Tab help");
    tab.add_slider(
        L"Tab level",
        L"Tab slider help",
        level,
        {.minimum = 0, .maximum = 100, .step = 5});

    erui::InputBindingSection& bindings = menu.add_input_binding_section(
        "menu-model", L"Example Controls");
    const ERUI_ActionInputs unbound = all_unbound_inputs();
    bindings
        .add_binding(
            101,
            "open-dialog",
            L"Open Dialog",
            unbound,
            unbound,
            {.callback = &binding_activated},
            {.callback = &binding_assignments_changed})
        .add_binding(
            102,
            "toggle-feature",
            L"Toggle Feature",
            unbound,
            unbound,
            {.callback = &binding_activated},
            {.callback = &binding_assignments_changed});

    auto compiled = erui::detail::MenuCompiler::compile(menu);
    ERUI_TEST_CHECK(compiled);
    ERUI_TEST_CHECK(menu.frozen());
    ERUI_TEST_CHECK(compiled->pages.size() == 3);
    ERUI_TEST_CHECK(compiled->root_page().rows.size() == 6);
    ERUI_TEST_CHECK(compiled->root_page().rows[2].choice_ids.size() == 3);
    ERUI_TEST_CHECK(compiled->root_page().rows[2].kind ==
        erui::RowKind::inline_choice);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled->root_page().rows[2].choice_ids[1])) == L"Balanced");
    ERUI_TEST_CHECK(compiled->tab_page_indices.size() == 1);
    ERUI_TEST_CHECK(compiled->modeled_button_count == 2);
    ERUI_TEST_CHECK(compiled->modeled_submenu_count == 1);
    ERUI_TEST_CHECK(compiled->modeled_popup_choice_count == 1);
    ERUI_TEST_CHECK(compiled->modeled_input_binding_count == 2);
    ERUI_TEST_CHECK(compiled->input_binding_sections.size() == 1);
    const auto& compiled_binding_section =
        compiled->input_binding_sections.front();
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_binding_section.label_id)) == L"Example Controls");
    ERUI_TEST_CHECK(compiled_binding_section.bindings.size() == 2);
    const auto& compiled_binding = compiled_binding_section.bindings.front();
    ERUI_TEST_CHECK(compiled_binding.provider_id ==
        "menu-model");
    ERUI_TEST_CHECK(compiled_binding.binding_id == "open-dialog");
    ERUI_TEST_CHECK(compiled_binding.handle == 101);
    ERUI_TEST_CHECK(compiled_binding.current_inputs.controller.state ==
        ERUI_INPUT_SLOT_UNBOUND);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        compiled_binding.label_id)) == L"Open Dialog");
    const auto* binding_text = compiled->text_binding(
        compiled_binding.label_id);
    ERUI_TEST_CHECK(binding_text != nullptr);
    ERUI_TEST_CHECK(binding_text->role ==
        erui::detail::TextRole::input_binding_label);
    compiled_binding.action.invoke(5u);
    ERUI_TEST_CHECK(observed_binding_devices == 5u);
    const auto& popup = compiled->root_page().rows[3];
    ERUI_TEST_CHECK(popup.kind == erui::RowKind::popup_choice);
    ERUI_TEST_CHECK(popup.choice_ids.size() == 3);
    ERUI_TEST_CHECK(popup.popup_choice_state.native_selection() == 3);
    ERUI_TEST_CHECK(popup.popup_choice_state.address() !=
        compiled->root_page().rows[2].popup_choice_state.address());
    ERUI_TEST_CHECK(compiled->pagination_required);
    ERUI_TEST_CHECK(compiled->previous_page_label_id != 0);
    ERUI_TEST_CHECK(compiled->next_page_label_id != 0);
    ERUI_TEST_CHECK(compiled->root_plan(6).pages.slices.size() == 2);
    ERUI_TEST_CHECK(compiled->root_plan(13).pages.slices.size() == 1);
    for (std::uint8_t capacity = 6; capacity <= 13; ++capacity) {
        const auto& plan = compiled->root_plan(capacity);
        ERUI_TEST_CHECK(plan.native_capacity == capacity);
        ERUI_TEST_CHECK(plan.custom_capacity == capacity - 4);
        ERUI_TEST_CHECK(
            compiled->root_physical_title_ids[capacity - 6].size() ==
            plan.pages.slices.size());
    }
    ERUI_TEST_CHECK(compiled->page_plans[1].slices.size() == 1);
    ERUI_TEST_CHECK(compiled->pages[1].physical_title_ids.size() == 1);
    const auto child_presentation = compiled->resolve_presentation(
        erui::detail::PageRoute::submenu(1));
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        child_presentation.outer_title_id)) == L"Demo");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        child_presentation.page_title_id)) == L"Advanced");
    const auto& vanilla_titles = compiled->root_physical_title_ids[0];
    ERUI_TEST_CHECK(vanilla_titles.size() == 2);
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        vanilla_titles[0])) ==
        L"Demo (1/2)");
    ERUI_TEST_CHECK(std::wstring(compiled->texts.lookup(
        vanilla_titles[1])) ==
        L"Demo (2/2)");
    const auto* physical_binding = compiled->text_binding(
        vanilla_titles[0]);
    ERUI_TEST_CHECK(physical_binding != nullptr);
    ERUI_TEST_CHECK(physical_binding->role ==
        erui::detail::TextRole::physical_page_title);
    ERUI_TEST_CHECK(physical_binding->slice_index == 0);

    const auto& toggle = compiled->root_page().rows[0];
    ERUI_TEST_CHECK(toggle.kind == erui::RowKind::toggle);
    const wchar_t* label = compiled->texts.lookup(static_cast<int>(toggle.label_id));
    const wchar_t* help = compiled->texts.lookup(static_cast<int>(toggle.help_id));
    ERUI_TEST_CHECK(label != nullptr && std::wcscmp(label, L"Enabled") == 0);
    ERUI_TEST_CHECK(help != nullptr && std::wcscmp(help, L"Toggle help") == 0);
    ERUI_TEST_CHECK(toggle.label_id != toggle.help_id);
    ERUI_TEST_CHECK(toggle.byte_value == &enabled);

    const auto* label_binding = compiled->text_binding(toggle.label_id);
    const auto* help_binding = compiled->text_binding(toggle.help_id);
    ERUI_TEST_CHECK(label_binding != nullptr);
    ERUI_TEST_CHECK(label_binding->role == erui::detail::TextRole::row_label);
    ERUI_TEST_CHECK(label_binding->page_index == compiled->root_page_index);
    ERUI_TEST_CHECK(label_binding->row_index == 0);
    ERUI_TEST_CHECK(help_binding != nullptr);
    ERUI_TEST_CHECK(help_binding->role == erui::detail::TextRole::row_help);
    ERUI_TEST_CHECK(help_binding->page_index == compiled->root_page_index);
    ERUI_TEST_CHECK(help_binding->row_index == 0);

    const auto& button = compiled->root_page().rows[4];
    ERUI_TEST_CHECK(button.kind == erui::RowKind::button);
    ERUI_TEST_CHECK(button.action);
    button.action.invoke();
    ERUI_TEST_CHECK(button_press_count == 1);

    const auto& submenu = compiled->root_page().rows[5];
    ERUI_TEST_CHECK(submenu.kind == erui::RowKind::submenu);
    ERUI_TEST_CHECK(submenu.target_page_index == 1);
    ERUI_TEST_CHECK(compiled->pages[submenu.target_page_index].kind == erui::PageKind::submenu);
    ERUI_TEST_CHECK(compiled->pages[submenu.target_page_index].rows.size() == 1);

    bool frozen_rejected = false;
    try {
        menu.root().add_toggle(L"Late row", L"Must fail", enabled);
    } catch (const std::logic_error&) {
        frozen_rejected = true;
    }
    ERUI_TEST_CHECK(frozen_rejected);
    bool frozen_binding_rejected = false;
    try {
        bindings.add_binding(
            103,
            "late-binding",
            L"Late Binding",
            unbound,
            unbound,
            {.callback = &binding_activated},
            {.callback = &binding_assignments_changed});
    } catch (const std::logic_error&) {
        frozen_binding_rejected = true;
    }
    ERUI_TEST_CHECK(frozen_binding_rejected);

    volatile std::uint8_t first = 1;
    volatile std::uint8_t second = 1;
    volatile std::uint8_t third = 1;
    erui::Menu capacity_menu(L"Capacity");
    capacity_menu.root()
        .add_toggle(L"First", L"", first)
        .add_toggle(L"Second", L"", second)
        .add_toggle(L"Third", L"", third);
    erui::Page& unreachable = capacity_menu.root().add_submenu(
        L"Hidden", L"", {}, {}, false);
    for (int index = 0; index < 20; ++index) {
        unreachable.add_button(
            L"Unreachable", L"", {.callback = &pressed});
    }
    auto capacity_compiled = erui::detail::MenuCompiler::compile(capacity_menu);
    ERUI_TEST_CHECK(capacity_compiled->root_page().rows.size() == 3);
    ERUI_TEST_CHECK(capacity_compiled->modeled_submenu_count == 0);
    ERUI_TEST_CHECK(capacity_compiled->modeled_button_count == 0);
    ERUI_TEST_CHECK(!capacity_compiled->page_reachable(1));
    ERUI_TEST_CHECK(capacity_compiled->pagination_required_for_capacity(
        erui::detail::game_options_vanilla_capacity));
    ERUI_TEST_CHECK(!capacity_compiled->pagination_required_for_capacity(
        erui::detail::game_options_max_visual_capacity));

    // Every native Configuration destination is a separate logical root.
    // Game Options remains exactly the legacy root object, while all other
    // roots are lazy and compile to Elden Ring's sparse category IDs.
    erui::Menu destinations(L"Built-in Test", L"Built-in help");
    ERUI_TEST_CHECK(&destinations.builtin_page(
        erui::detail::BuiltinPage::game_options) == &destinations.root());
    ERUI_TEST_CHECK(destinations.find_builtin_page(
        erui::detail::BuiltinPage::camera_options) == nullptr);

    constexpr std::array builtin_destinations{
        erui::detail::BuiltinPage::game_options,
        erui::detail::BuiltinPage::camera_options,
        erui::detail::BuiltinPage::display,
        erui::detail::BuiltinPage::sound,
        erui::detail::BuiltinPage::network,
        erui::detail::BuiltinPage::keyboard_mouse,
        erui::detail::BuiltinPage::graphics,
    };
    constexpr std::array<std::uint8_t, builtin_destinations.size()>
        native_categories{0u, 1u, 2u, 3u, 5u, 7u, 8u};
    for (std::size_t index = 0; index < builtin_destinations.size(); ++index) {
        erui::Page& destination = destinations.builtin_page(
            builtin_destinations[index]);
        ERUI_TEST_CHECK(&destination == destinations.find_builtin_page(
            builtin_destinations[index]));
        destination.add_button(
            L"Destination row " + std::to_wstring(index),
            L"Destination help",
            {.callback = &pressed});
    }
    erui::Page& camera_child = destinations.builtin_page(
        erui::detail::BuiltinPage::camera_options).add_submenu(
            L"Camera child", L"Camera child help");
    camera_child.add_button(
        L"Nested action", L"Nested help", {.callback = &pressed});
    ERUI_TEST_CHECK(destinations.page_count() == 8);
    bool invalid_destination_rejected = false;
    try {
        (void)destinations.builtin_page(erui::detail::BuiltinPage::count);
    } catch (const std::invalid_argument&) {
        invalid_destination_rejected = true;
    }
    ERUI_TEST_CHECK(invalid_destination_rejected);

    auto destination_compiled =
        erui::detail::MenuCompiler::compile(destinations);
    ERUI_TEST_CHECK(destination_compiled->root_page_index == 0);
    ERUI_TEST_CHECK(destination_compiled->has_builtin_pages());
    for (std::size_t index = 0; index < builtin_destinations.size(); ++index) {
        const std::uint8_t native_category = native_categories[index];
        const std::size_t page_index =
            destination_compiled->builtin_page_index(native_category);
        ERUI_TEST_CHECK(page_index != erui::detail::invalid_compiled_index);
        ERUI_TEST_CHECK(destination_compiled->builtin_page(native_category) ==
            &destination_compiled->pages[page_index]);
        ERUI_TEST_CHECK(destination_compiled->pages[page_index].builtin_page ==
            builtin_destinations[index]);
        ERUI_TEST_CHECK(destination_compiled->page_reachable(page_index));
        ERUI_TEST_CHECK(destination_compiled->page_plans[page_index]
            .slices.empty());
        ERUI_TEST_CHECK(destination_compiled->pages[page_index]
            .physical_title_ids.empty());
    }
    ERUI_TEST_CHECK(destination_compiled->builtin_page_index(4u) ==
        erui::detail::invalid_compiled_index);
    ERUI_TEST_CHECK(destination_compiled->builtin_page_index(6u) ==
        erui::detail::invalid_compiled_index);
    ERUI_TEST_CHECK(destination_compiled->builtin_page_index(9u) ==
        erui::detail::invalid_compiled_index);
    ERUI_TEST_CHECK(destination_compiled->builtin_page_index(10u) ==
        erui::detail::invalid_compiled_index);
    ERUI_TEST_CHECK(destination_compiled->builtin_page(4u) == nullptr);
    ERUI_TEST_CHECK(destination_compiled->modeled_button_count == 8);
    ERUI_TEST_CHECK(destination_compiled->modeled_submenu_count == 1);
    const std::size_t camera_index =
        destination_compiled->builtin_page_index(1u);
    const auto camera_main = erui::detail::PageRoute::builtin_main(
        camera_index, 1u, 4u);
    ERUI_TEST_CHECK(destination_compiled->resolve_slice(camera_main) ==
        nullptr);
    const auto camera_main_presentation =
        destination_compiled->resolve_presentation(camera_main);
    ERUI_TEST_CHECK(camera_main_presentation.outer_title_id == 0);
    ERUI_TEST_CHECK(camera_main_presentation.page_title_id == 0);
    const auto camera_continuation =
        erui::detail::PageRoute::builtin_continuation(
            camera_index, 1u, 1u, 1u);
    ERUI_TEST_CHECK(destination_compiled->resolve_slice(
        camera_continuation) == nullptr);
    const auto camera_continuation_presentation =
        destination_compiled->resolve_presentation(camera_continuation);
    ERUI_TEST_CHECK(std::wstring(destination_compiled->texts.lookup(
        camera_continuation_presentation.outer_title_id)) ==
        L"Built-in Test");
    ERUI_TEST_CHECK(std::wstring(destination_compiled->texts.lookup(
        camera_continuation_presentation.page_title_id)) ==
        L"Built-in Test (2/2)");
    // Both Camera rows fit exactly when the live panel exposes two slots, so
    // a continuation route for that capacity is impossible and fails closed.
    const auto exact_fit_presentation =
        destination_compiled->resolve_presentation(
            erui::detail::PageRoute::builtin_continuation(
                camera_index, 1u, 2u, 1u));
    ERUI_TEST_CHECK(exact_fit_presentation.outer_title_id == 0);
    ERUI_TEST_CHECK(exact_fit_presentation.page_title_id == 0);
    const auto wrong_builtin_presentation =
        destination_compiled->resolve_presentation(
            erui::detail::PageRoute::builtin_continuation(
                camera_index, 2u, 1u, 1u));
    ERUI_TEST_CHECK(wrong_builtin_presentation.outer_title_id == 0);
    ERUI_TEST_CHECK(wrong_builtin_presentation.page_title_id == 0);
    const auto& camera_submenu =
        destination_compiled->pages[camera_index].rows.back();
    ERUI_TEST_CHECK(camera_submenu.kind == erui::RowKind::submenu);
    ERUI_TEST_CHECK(destination_compiled->page_reachable(
        camera_submenu.target_page_index));
    ERUI_TEST_CHECK(destination_compiled->page_plans[
        camera_submenu.target_page_index].slices.size() == 1);

    // Built-in title variants are keyed by the runtime first-page capacity,
    // deduplicated by resulting slice count, and retain the normal n/t title
    // policy without allocating from a UI callback.
    erui::Menu builtin_pagination(L"ERNativeUI");
    erui::Page& graphics = builtin_pagination.builtin_page(
        erui::detail::BuiltinPage::graphics);
    for (int index = 0; index < 29; ++index) {
        graphics.add_button(
            L"Graphics row " + std::to_wstring(index),
            L"Graphics help",
            {.callback = &pressed});
    }
    auto builtin_pagination_compiled =
        erui::detail::MenuCompiler::compile(builtin_pagination);
    const std::size_t graphics_index =
        builtin_pagination_compiled->builtin_page_index(8u);
    ERUI_TEST_CHECK(graphics_index != erui::detail::invalid_compiled_index);
    const erui::detail::CompiledPage& compiled_graphics =
        builtin_pagination_compiled->pages[graphics_index];
    ERUI_TEST_CHECK(compiled_graphics.builtin_physical_title_plan != nullptr);
    const auto& title_plan =
        *compiled_graphics.builtin_physical_title_plan;
    ERUI_TEST_CHECK(title_plan.variant_by_capacity[1] ==
        title_plan.variant_by_capacity[2]);
    ERUI_TEST_CHECK(title_plan.variant_by_capacity[1] !=
        title_plan.variant_by_capacity[3]);

    const auto graphics_second =
        builtin_pagination_compiled->resolve_presentation(
            erui::detail::PageRoute::builtin_continuation(
                graphics_index, 8u, 3u, 1u));
    const auto graphics_third =
        builtin_pagination_compiled->resolve_presentation(
            erui::detail::PageRoute::builtin_continuation(
                graphics_index, 8u, 3u, 2u));
    ERUI_TEST_CHECK(std::wstring(builtin_pagination_compiled->texts.lookup(
        graphics_second.page_title_id)) == L"ERNativeUI (2/3)");
    ERUI_TEST_CHECK(std::wstring(builtin_pagination_compiled->texts.lookup(
        graphics_third.page_title_id)) == L"ERNativeUI (3/3)");

    const auto presentation_fails_closed = [&](erui::detail::PageRoute route) {
        const auto presentation =
            builtin_pagination_compiled->resolve_presentation(route);
        ERUI_TEST_CHECK(presentation.outer_title_id == 0);
        ERUI_TEST_CHECK(presentation.page_title_id == 0);
    };
    presentation_fails_closed(
        erui::detail::PageRoute::builtin_main(
            graphics_index, 8u, 3u));
    presentation_fails_closed(
        erui::detail::PageRoute::builtin_continuation(
            graphics_index, 7u, 3u, 1u));
    presentation_fails_closed(
        erui::detail::PageRoute::builtin_continuation(
            graphics_index, 8u, 0u, 1u));
    presentation_fails_closed(
        erui::detail::PageRoute::builtin_continuation(
            graphics_index,
            8u,
            static_cast<std::uint8_t>(
                erui::detail::builtin_page_max_first_capacity + 1u),
            1u));
    presentation_fails_closed(
        erui::detail::PageRoute::builtin_continuation(
            graphics_index, 8u, 3u, 3u));
    presentation_fails_closed(
        erui::detail::PageRoute::builtin_continuation(
            builtin_pagination_compiled->root_page_index, 0u, 1u, 1u));
    return 0;
}
