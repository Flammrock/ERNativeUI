#include "menu.hpp"

#include <stdexcept>
#include <utility>

namespace erui {

Page::Page(Menu& owner, PageKind kind, std::wstring title, std::wstring help)
    : owner_(&owner),
      kind_(kind),
      title_(std::move(title)),
      help_(std::move(help)) {}

void Page::ensure_mutable() const {
    if (!owner_ || owner_->frozen()) {
        throw std::logic_error("ERNativeUI menu is already frozen");
    }
}

Page& Page::add_toggle(
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& value,
    bool enabled,
    ValueAction on_changed) {
    ensure_mutable();
    value = value == 0 ? 0 : 1;
    rows_.push_back(RowDefinition{
        .kind = RowKind::toggle,
        .label = std::move(label),
        .help = std::move(help),
        .byte_value = &value,
        .value_action = on_changed,
        .enabled = enabled,
    });
    return *this;
}

Page& Page::add_slider(
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& value,
    SliderSpec range,
    bool enabled,
    ValueAction on_changed) {
    ensure_mutable();
    if (range.minimum < 0 || range.maximum > 255 || range.minimum > range.maximum) {
        throw std::invalid_argument("native byte slider range must stay inside 0..255");
    }
    if (range.step <= 0) {
        throw std::invalid_argument("native slider step must be positive");
    }

    const auto numeric_value = static_cast<std::int32_t>(value);
    if (numeric_value < range.minimum || numeric_value > range.maximum) {
        throw std::invalid_argument("native slider value is outside its declared range");
    }

    rows_.push_back(RowDefinition{
        .kind = RowKind::slider,
        .label = std::move(label),
        .help = std::move(help),
        .byte_value = &value,
        .slider = range,
        .value_action = on_changed,
        .enabled = enabled,
    });
    return *this;
}

namespace {

void validate_choice(
    volatile std::uint8_t& selected_index,
    const std::vector<std::wstring>& options) {
    if (options.empty() || options.size() > 32 ||
        selected_index >= options.size()) {
        throw std::invalid_argument(
            "native choice requires 1..32 options and a valid selected index");
    }
    for (const auto& option : options) {
        if (option.empty()) {
            throw std::invalid_argument("native choice options cannot be empty");
        }
    }
}

} // namespace

Page& Page::add_inline_choice(
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& selected_index,
    std::vector<std::wstring> options,
    bool enabled,
    ValueAction on_changed) {
    ensure_mutable();
    validate_choice(selected_index, options);
    rows_.push_back(RowDefinition{
        .kind = RowKind::inline_choice,
        .label = std::move(label),
        .help = std::move(help),
        .byte_value = &selected_index,
        .choices = std::move(options),
        .value_action = on_changed,
        .enabled = enabled,
    });
    return *this;
}

Page& Page::add_popup_choice(
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& selected_index,
    std::vector<std::wstring> options,
    bool enabled,
    ValueAction on_changed) {
    ensure_mutable();
    validate_choice(selected_index, options);
    rows_.push_back(RowDefinition{
        .kind = RowKind::popup_choice,
        .label = std::move(label),
        .help = std::move(help),
        .byte_value = &selected_index,
        .choices = std::move(options),
        .value_action = on_changed,
        .enabled = enabled,
    });
    return *this;
}

Page& Page::add_text_input(
    std::wstring label,
    std::wstring help,
    detail::TextInputState& state,
    TextAction on_changed) {
    ensure_mutable();
    rows_.push_back(RowDefinition{
        .kind = RowKind::text_input,
        .label = std::move(label),
        .help = std::move(help),
        .text_input_state = &state,
        .text_action = on_changed,
    });
    return *this;
}

Page& Page::add_button(
    std::wstring label,
    std::wstring help,
    Action action,
    bool enabled) {
    ensure_mutable();
    if (!action) {
        throw std::invalid_argument("native button action must not be null");
    }

    rows_.push_back(RowDefinition{
        .kind = RowKind::button,
        .label = std::move(label),
        .help = std::move(help),
        .action = action,
        .enabled = enabled,
    });
    return *this;
}

Page& Page::add_submenu(
    std::wstring label,
    std::wstring help,
    std::wstring page_title,
    std::wstring page_help,
    bool enabled) {
    ensure_mutable();
    if (page_title.empty()) {
        page_title = label;
    }
    if (page_help.empty()) {
        page_help = help;
    }

    Page& child = owner_->create_page(
        PageKind::submenu,
        std::move(page_title),
        std::move(page_help));

    rows_.push_back(RowDefinition{
        .kind = RowKind::submenu,
        .label = std::move(label),
        .help = std::move(help),
        .target_page = &child,
        .enabled = enabled,
    });
    return child;
}

Page& Page::set_presentation(PagePresentationSpec presentation) {
    ensure_mutable();
    presentation_ = std::move(presentation);
    return *this;
}

Menu::Menu(std::wstring title, std::wstring help, MenuLocalization localization)
    : title_(std::move(title)), help_(std::move(help)),
      localization_(std::move(localization)) {
    root_page_ = &create_page(PageKind::root, title_, help_);
}

Page& Menu::create_page(PageKind kind, std::wstring title, std::wstring help) {
    if (frozen_) {
        throw std::logic_error("ERNativeUI menu is already frozen");
    }
    pages_.push_back(std::unique_ptr<Page>(
        new Page(*this, kind, std::move(title), std::move(help))));
    return *pages_.back();
}

Page& Menu::add_tab(std::wstring title, std::wstring help) {
    Page& page = create_page(PageKind::tab, std::move(title), std::move(help));
    tab_pages_.push_back(&page);
    return page;
}

} // namespace erui
