#include "menu.hpp"

#include <stdexcept>
#include <utility>

namespace erui {
namespace {

[[nodiscard]] bool valid_title_screen_corner(
    ERUI_TitleScreenCorner corner) noexcept {
    switch (corner) {
    case ERUI_TITLE_SCREEN_CORNER_TOP_LEFT:
    case ERUI_TITLE_SCREEN_CORNER_TOP_RIGHT:
    case ERUI_TITLE_SCREEN_CORNER_BOTTOM_LEFT:
    case ERUI_TITLE_SCREEN_CORNER_BOTTOM_RIGHT:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_title_screen_text(
    std::wstring_view text) noexcept {
    if (text.empty() ||
        text.size() > ERUI_TITLE_SCREEN_TEXT_MAX_CODE_UNITS) {
        return false;
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto unit = static_cast<std::uint16_t>(text[index]);
        if (unit == 0u || unit == static_cast<std::uint16_t>(L'\r') ||
            unit == static_cast<std::uint16_t>(L'\n') ||
            unit == 0x0085u || unit == 0x2028u || unit == 0x2029u) {
            return false;
        }
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (++index >= text.size()) return false;
            const auto low = static_cast<std::uint16_t>(text[index]);
            if (low < 0xDC00u || low > 0xDFFFu) return false;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return false;
        }
    }
    return true;
}

} // namespace

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

void validate_formatted_slider(
    volatile std::uint8_t& selected_index,
    const std::vector<std::wstring>& display_values) {
    if (display_values.size() < ERUI_SLIDER_MIN_VALUE_COUNT ||
        display_values.size() > ERUI_SLIDER_MAX_VALUE_COUNT ||
        static_cast<std::size_t>(selected_index) >= display_values.size()) {
        throw std::invalid_argument(
            "formatted slider requires 2..256 values and a valid selected index");
    }
    for (const std::wstring& value : display_values) {
        if (value.empty()) {
            throw std::invalid_argument(
                "formatted slider values cannot be empty");
        }
    }
}

} // namespace

Page& Page::add_formatted_slider(
    RowKind kind,
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& selected_index,
    std::vector<std::wstring> display_values,
    bool enabled,
    ValueAction on_changed) {
    validate_formatted_slider(selected_index, display_values);
    const std::int32_t maximum = static_cast<std::int32_t>(
        display_values.size() - 1);
    rows_.push_back(RowDefinition{
        .kind = kind,
        .label = std::move(label),
        .help = std::move(help),
        .byte_value = &selected_index,
        .slider = {0, maximum, 1},
        .slider_value_texts = std::move(display_values),
        .value_action = on_changed,
        .enabled = enabled,
    });
    return *this;
}

Page& Page::add_numeric_slider(
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& selected_index,
    std::vector<std::wstring> display_values,
    bool enabled,
    ValueAction on_changed) {
    ensure_mutable();
    return add_formatted_slider(
        RowKind::numeric_slider,
        std::move(label),
        std::move(help),
        selected_index,
        std::move(display_values),
        enabled,
        on_changed);
}

Page& Page::add_custom_slider(
    std::wstring label,
    std::wstring help,
    volatile std::uint8_t& selected_index,
    std::vector<std::wstring> display_values,
    bool enabled,
    ValueAction on_changed) {
    ensure_mutable();
    return add_formatted_slider(
        RowKind::custom_slider,
        std::move(label),
        std::move(help),
        selected_index,
        std::move(display_values),
        enabled,
        on_changed);
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

Page& Page::add_color_picker(
    std::wstring label,
    std::wstring help,
    detail::ColorPickerState& state,
    bool enabled,
    detail::ColorAction on_changed) {
    ensure_mutable();
    rows_.push_back(RowDefinition{
        .kind = RowKind::color_picker,
        .label = std::move(label),
        .help = std::move(help),
        .color_picker_state = &state,
        .color_action = on_changed,
        .enabled = enabled,
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
    builtin_pages_[detail::builtin_page_offset(
        detail::BuiltinPage::game_options)] = root_page_;
}

Page& Menu::create_page(PageKind kind, std::wstring title, std::wstring help) {
    if (frozen_) {
        throw std::logic_error("ERNativeUI menu is already frozen");
    }
    pages_.push_back(std::unique_ptr<Page>(
        new Page(*this, kind, std::move(title), std::move(help))));
    return *pages_.back();
}

Page& Menu::builtin_page(detail::BuiltinPage page) {
    if (!detail::valid_builtin_page(page)) {
        throw std::invalid_argument("invalid built-in page destination");
    }
    Page*& destination = builtin_pages_[detail::builtin_page_offset(page)];
    if (!destination) {
        // Native top-level pages retain their game-owned localized title.
        // This neutral host title is used only by ERNativeUI continuation
        // pages when pagination is required.
        destination = &create_page(PageKind::root, title_, help_);
    }
    return *destination;
}

const Page* Menu::find_builtin_page(detail::BuiltinPage page) const noexcept {
    if (!detail::valid_builtin_page(page)) return nullptr;
    return builtin_pages_[detail::builtin_page_offset(page)];
}

Page& Menu::add_tab(std::wstring title) {
    Page& page = create_page(PageKind::tab, std::move(title), {});
    tab_pages_.push_back(&page);
    return page;
}

Page& Menu::add_tab(
    TabMetadata metadata,
    std::wstring title) {
    if (metadata.provider_id.empty() || metadata.tab_id.empty()) {
        throw std::invalid_argument("registered tab metadata has an empty ID");
    }
    if (metadata.icon.source == TabIconSource::fallback) {
        if (!metadata.icon.rgba8_pixels.empty()) {
            throw std::invalid_argument(
                "fallback tab icon unexpectedly owns RGBA pixels");
        }
    } else if (metadata.icon.source != TabIconSource::rgba8 ||
               metadata.icon.rgba8_pixels.size() !=
                   ERUI_TAB_ICON_RGBA8_BYTE_COUNT) {
        throw std::invalid_argument("registered tab icon has an invalid shape");
    }

    Page& page = create_page(PageKind::tab, std::move(title), {});
    try {
        page.tab_metadata_.emplace(std::move(metadata));
        tab_pages_.push_back(&page);
    } catch (...) {
        pages_.pop_back();
        throw;
    }
    return page;
}

void Menu::add_title_screen_text(TitleScreenTextSpec text) {
    if (frozen_) {
        throw std::logic_error("menu is frozen");
    }
    if (text.provider_id.empty() ||
        !valid_title_screen_corner(text.corner) ||
        !valid_title_screen_text(text.text)) {
        throw std::invalid_argument(
            "registered title-screen text has an invalid shape");
    }
    title_screen_texts_.push_back(std::move(text));
}

InputBindingSection::InputBindingSection(
    Menu& owner,
    std::string provider_id,
    std::wstring label)
    : owner_(&owner), provider_id_(std::move(provider_id)),
      label_(std::move(label)) {}

void InputBindingSection::ensure_mutable() const {
    if (!owner_ || owner_->frozen()) {
        throw std::logic_error("ERNativeUI menu is already frozen");
    }
}

InputBindingSection& InputBindingSection::add_binding(
    ERUI_InputActionHandle handle,
    std::string binding_id,
    std::wstring label,
    const ERUI_ActionInputs& default_inputs,
    const ERUI_ActionInputs& current_inputs,
    InputBindingAction action,
    InputAssignmentsAction assignments_changed) {
    ensure_mutable();
    if (handle == ERUI_INVALID_INPUT_ACTION || binding_id.empty() ||
        label.empty() || !action) {
        throw std::invalid_argument("invalid input binding definition");
    }
    bindings_.push_back({
        .handle = handle,
        .binding_id = std::move(binding_id),
        .label = std::move(label),
        .default_inputs = default_inputs,
        .current_inputs = current_inputs,
        .action = action,
        .assignments_changed = assignments_changed,
    });
    return *this;
}

InputBindingSection& Menu::add_input_binding_section(
    std::string provider_id,
    std::wstring label) {
    if (frozen_) {
        throw std::logic_error("ERNativeUI menu is already frozen");
    }
    if (provider_id.empty() || label.empty()) {
        throw std::invalid_argument("invalid input binding section");
    }
    input_binding_sections_.push_back(
        std::unique_ptr<InputBindingSection>(new InputBindingSection(
            *this, std::move(provider_id), std::move(label))));
    return *input_binding_sections_.back();
}

} // namespace erui
