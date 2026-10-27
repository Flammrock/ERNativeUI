#pragma once

#include <ernativeui/erui.h>

#include "builtin_page.hpp"
#include "color_picker_state.hpp"
#include "text_input_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace erui {

using TextId = std::uint32_t;

enum class RowKind : std::uint8_t {
    toggle,
    slider,
    numeric_slider,
    custom_slider,
    inline_choice,
    popup_choice,
    text_input,
    color_picker,
    button,
    submenu,
};

enum class PageKind : std::uint8_t {
    root,
    submenu,
    tab,
};

struct SliderSpec {
    std::int32_t minimum{};
    std::int32_t maximum{100};
    std::int32_t step{1};
};

struct PageTitleFormatRequest {
    std::wstring_view base_title{};
    std::size_t page_number{};
    std::size_t page_count{};
};

using PageTitleFormatCallback = bool (*)(
    const PageTitleFormatRequest& request,
    std::wstring& output,
    void* user_data) noexcept;

struct PagePresentationSpec {
    std::wstring menu_title{};
    std::wstring page_title{};
    PageTitleFormatCallback formatter{};
    void* user_data{};
};

struct MenuLocalization {
    std::wstring previous_label{L"Previous Page"};
    std::wstring previous_help{L"Return to the preceding settings page."};
    std::wstring next_label{L"Next Page"};
    std::wstring next_help{L"Open the next settings page."};
};

enum class TabIconSource : std::uint8_t {
    fallback,
    rgba8,
};

// Immutable catalogue data owned by the host after provider registration.
// The RGBA image, when present, is always an exact API 1.3 32x32 wire image.
// Native rendering conversions remain outside the public ABI.
struct TabIconSpec {
    TabIconSource source{TabIconSource::fallback};
    std::vector<std::uint8_t> rgba8_pixels{};
};

struct TabMetadata {
    std::string provider_id{};
    std::string tab_id{};
    std::int32_t preferred_order{};
    std::optional<std::int32_t> player_order{};
    std::int32_t effective_order{};
    TabIconSpec icon{};
};

// Immutable title-screen line assembled from API 1.3 registrations. The
// provider ID is retained for diagnostics; presentation order is the vector
// order established by Registry during its deterministic freeze pass.
struct TitleScreenTextSpec {
    std::string provider_id{};
    std::wstring text{};
    std::optional<detail::RgbColor> color{};
    ERUI_TitleScreenCorner corner{};
};

using ActionCallback = void (*)(void* user_data) noexcept;
using ValueChangedCallback = void (*)(std::uint8_t value, void* user_data) noexcept;
using TextChangedCallback = void (*)(
    std::wstring_view value,
    void* user_data) noexcept;
using InputBindingActivatedCallback = void (*)(
    std::uint32_t devices,
    void* user_data) noexcept;
using InputAssignmentsChangedCallback = void (*)(
    const ERUI_ActionInputs& previous,
    const ERUI_ActionInputs& current,
    ERUI_AssignmentChangeReason reason,
    ERUI_InputDevices changed_devices,
    void* user_data) noexcept;

struct Action {
    ActionCallback callback{};
    void* user_data{};

    [[nodiscard]] explicit operator bool() const noexcept { return callback != nullptr; }
    void invoke() const noexcept {
        if (callback) {
            callback(user_data);
        }
    }
};

struct ValueAction {
    ValueChangedCallback callback{};
    void* user_data{};

    [[nodiscard]] explicit operator bool() const noexcept { return callback != nullptr; }
    void invoke(std::uint8_t value) const noexcept {
        if (callback) {
            callback(value, user_data);
        }
    }
};

struct TextAction {
    TextChangedCallback callback{};
    void* user_data{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return callback != nullptr;
    }
    void invoke(std::wstring_view value) const noexcept {
        if (callback) callback(value, user_data);
    }
};

struct InputBindingAction {
    InputBindingActivatedCallback callback{};
    void* user_data{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return callback != nullptr;
    }
    void invoke(std::uint32_t devices) const noexcept {
        if (callback) callback(devices, user_data);
    }
};

struct InputAssignmentsAction {
    InputAssignmentsChangedCallback callback{};
    void* user_data{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return callback != nullptr;
    }
    void invoke(
        const ERUI_ActionInputs& previous,
        const ERUI_ActionInputs& current,
        ERUI_AssignmentChangeReason reason,
        ERUI_InputDevices changed_devices) const noexcept {
        if (callback) {
            callback(
                previous,
                current,
                reason,
                changed_devices,
                user_data);
        }
    }
};

namespace detail {
class MenuCompiler;
}

class Menu;
class InputBindingSection;

// Declarative native Elden Ring page. Toggle, slider, action-button, and
// submenu rows are constructed by the Game Options runtime. Tab nodes retain
// the same model for the separate tab-routing backend.
class Page {
public:
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) = delete;
    Page& operator=(Page&&) = delete;
    ~Page() = default;

    Page& add_toggle(
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& value,
        bool enabled = true,
        ValueAction on_changed = {});

    Page& add_slider(
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& value,
        SliderSpec range,
        bool enabled = true,
        ValueAction on_changed = {});

    Page& add_numeric_slider(
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& selected_index,
        std::vector<std::wstring> display_values,
        bool enabled = true,
        ValueAction on_changed = {});

    Page& add_custom_slider(
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& selected_index,
        std::vector<std::wstring> display_values,
        bool enabled = true,
        ValueAction on_changed = {});

    Page& add_inline_choice(
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& selected_index,
        std::vector<std::wstring> options,
        bool enabled = true,
        ValueAction on_changed = {});

    Page& add_popup_choice(
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& selected_index,
        std::vector<std::wstring> options,
        bool enabled = true,
        ValueAction on_changed = {});

    Page& add_text_input(
        std::wstring label,
        std::wstring help,
        detail::TextInputState& state,
        TextAction on_changed = {});

    Page& add_color_picker(
        std::wstring label,
        std::wstring help,
        detail::ColorPickerState& state,
        bool enabled = true,
        detail::ColorAction on_changed = {});

    Page& add_button(
        std::wstring label,
        std::wstring help,
        Action action,
        bool enabled = true);

    // Returns the newly created child page for fluent nested declarations.
    Page& add_submenu(
        std::wstring label,
        std::wstring help,
        std::wstring page_title = {},
        std::wstring page_help = {},
        bool enabled = true);

    Page& set_presentation(PagePresentationSpec presentation);

    // Internal host bridge: associate the row most recently appended through
    // an add_* call with its stable public ABI identity. Direct Core users do
    // not set this, so their rows remain intentionally non-editable at
    // runtime.
    Page& identify_last_row(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        bool visibility_editable = false);

    [[nodiscard]] PageKind kind() const noexcept { return kind_; }
    [[nodiscard]] std::wstring_view title() const noexcept { return title_; }
    [[nodiscard]] std::wstring_view help() const noexcept { return help_; }
    [[nodiscard]] const PagePresentationSpec& presentation() const noexcept {
        return presentation_;
    }
    [[nodiscard]] const std::optional<TabMetadata>& tab_metadata() const noexcept {
        return tab_metadata_;
    }
    [[nodiscard]] std::size_t row_count() const noexcept { return rows_.size(); }

private:
    friend class Menu;
    friend class detail::MenuCompiler;

    struct RowDefinition {
        RowKind kind{RowKind::toggle};
        ERUI_ProviderHandle provider{ERUI_INVALID_PROVIDER};
        ERUI_RowHandle row{ERUI_INVALID_ROW};
        bool visibility_editable{};
        std::wstring label{};
        std::wstring help{};
        volatile std::uint8_t* byte_value{};
        SliderSpec slider{};
        std::vector<std::wstring> slider_value_texts{};
        std::vector<std::wstring> choices{};
        detail::TextInputState* text_input_state{};
        detail::ColorPickerState* color_picker_state{};
        Action action{};
        ValueAction value_action{};
        TextAction text_action{};
        detail::ColorAction color_action{};
        Page* target_page{};
        bool enabled{true};
    };

    Page(Menu& owner, PageKind kind, std::wstring title, std::wstring help);
    void ensure_mutable() const;
    Page& add_formatted_slider(
        RowKind kind,
        std::wstring label,
        std::wstring help,
        volatile std::uint8_t& selected_index,
        std::vector<std::wstring> display_values,
        bool enabled,
        ValueAction on_changed);

    Menu* owner_{};
    PageKind kind_{PageKind::root};
    std::wstring title_{};
    std::wstring help_{};
    PagePresentationSpec presentation_{};
    std::optional<TabMetadata> tab_metadata_{};
    std::vector<RowDefinition> rows_{};
};

// Declarative catalog rendered by Elden Ring's native Button Settings and
// Keyboard/Mouse Settings screens. It is intentionally separate from Page:
// those screens own a different scrolling model and one logical action shares
// its controller, keyboard, and mouse assignments.
class InputBindingSection {
public:
    InputBindingSection(const InputBindingSection&) = delete;
    InputBindingSection& operator=(const InputBindingSection&) = delete;
    InputBindingSection(InputBindingSection&&) = delete;
    InputBindingSection& operator=(InputBindingSection&&) = delete;
    ~InputBindingSection() = default;

    InputBindingSection& add_binding(
        ERUI_InputActionHandle handle,
        std::string binding_id,
        std::wstring label,
        const ERUI_ActionInputs& default_inputs,
        const ERUI_ActionInputs& current_inputs,
        InputBindingAction action,
        InputAssignmentsAction assignments_changed);

    [[nodiscard]] std::wstring_view label() const noexcept { return label_; }
    [[nodiscard]] std::size_t binding_count() const noexcept {
        return bindings_.size();
    }

private:
    friend class Menu;
    friend class detail::MenuCompiler;

    struct Definition {
        ERUI_InputActionHandle handle{};
        std::string binding_id{};
        std::wstring label{};
        ERUI_ActionInputs default_inputs{};
        ERUI_ActionInputs current_inputs{};
        InputBindingAction action{};
        InputAssignmentsAction assignments_changed{};
    };

    InputBindingSection(
        Menu& owner,
        std::string provider_id,
        std::wstring label);
    void ensure_mutable() const;

    Menu* owner_{};
    std::string provider_id_{};
    std::wstring label_{};
    std::vector<Definition> bindings_{};
};

class Menu {
public:
    explicit Menu(std::wstring title, std::wstring help = {},
        MenuLocalization localization = {});
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu(Menu&&) = delete;
    Menu& operator=(Menu&&) = delete;
    ~Menu() = default;

    [[nodiscard]] Page& root() noexcept { return *root_page_; }
    [[nodiscard]] const Page& root() const noexcept { return *root_page_; }

    // Returns the logical destination merged into an Elden Ring built-in
    // Configuration page. Game Options is the legacy root page. Other
    // destinations are created lazily so API 1.0 menus keep their historical
    // model shape and page numbering.
    Page& builtin_page(detail::BuiltinPage page);
    [[nodiscard]] const Page* find_builtin_page(
        detail::BuiltinPage page) const noexcept;

    Page& add_tab(std::wstring title);
    Page& add_tab(
        TabMetadata metadata,
        std::wstring title);
    void add_title_screen_text(TitleScreenTextSpec text);

    InputBindingSection& add_input_binding_section(
        std::string provider_id,
        std::wstring label);

    [[nodiscard]] std::wstring_view title() const noexcept { return title_; }
    [[nodiscard]] std::wstring_view help() const noexcept { return help_; }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] std::size_t page_count() const noexcept { return pages_.size(); }
    [[nodiscard]] std::size_t tab_count() const noexcept { return tab_pages_.size(); }
    [[nodiscard]] std::size_t title_screen_text_count() const noexcept {
        return title_screen_texts_.size();
    }
    [[nodiscard]] std::span<const TitleScreenTextSpec>
    title_screen_texts() const noexcept {
        return title_screen_texts_;
    }
    [[nodiscard]] std::size_t input_binding_section_count() const noexcept {
        return input_binding_sections_.size();
    }

private:
    friend class Page;
    friend class detail::MenuCompiler;

    Page& create_page(PageKind kind, std::wstring title, std::wstring help);
    void freeze() noexcept { frozen_ = true; }

    std::wstring title_{};
    std::wstring help_{};
    MenuLocalization localization_{};
    std::vector<std::unique_ptr<Page>> pages_{};
    std::vector<Page*> tab_pages_{};
    std::vector<TitleScreenTextSpec> title_screen_texts_{};
    std::vector<std::unique_ptr<InputBindingSection>>
        input_binding_sections_{};
    std::array<Page*, detail::builtin_page_count> builtin_pages_{};
    Page* root_page_{};
    bool frozen_{false};
};

} // namespace erui
