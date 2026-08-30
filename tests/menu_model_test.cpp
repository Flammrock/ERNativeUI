#include "menu_compiler.hpp"

#include "menu.hpp"

#include "test_assertions.hpp"
#include <cstdint>
#include <cwchar>
#include <stdexcept>

namespace {

void changed(std::uint8_t, void*) noexcept {}
int button_press_count{};
void pressed(void*) noexcept { ++button_press_count; }

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
        erui::detail::controller_vanilla_capacity));
    ERUI_TEST_CHECK(!capacity_compiled->pagination_required_for_capacity(
        erui::detail::controller_max_visual_capacity));
    return 0;
}
