#pragma once

#include <ernativeui/erui.h>
#include "menu.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace erui::host {

enum class ApiState : std::uint8_t {
    initializing,
    accepting,
    runtime_ready,
    failed,
};

class Registry {
public:
    Registry();
    ~Registry();
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    void open_registration() noexcept;
    void set_host_locale(erui::MenuLocalization pagination);
    [[nodiscard]] std::unique_ptr<erui::Menu> freeze_and_build();
    [[nodiscard]] std::unique_ptr<erui::Menu> try_freeze_and_build(
        std::uint64_t opened_at,
        std::uint64_t quiet_ms,
        std::uint64_t maximum_wait_ms);
    [[nodiscard]] bool registration_open() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::size_t committed_provider_count() const noexcept;
    [[nodiscard]] std::size_t committed_root_row_count() const noexcept;

    ERUI_Result register_provider(
        std::uint32_t negotiated_api_version,
        const ERUI_ProviderDesc* description,
        ERUI_ProviderHandle* out_provider,
        ERUI_PageHandle* out_root_page) noexcept;
    ERUI_Result add_button(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ButtonDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result add_toggle(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ToggleDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result add_slider(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_SliderDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result add_inline_choice(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result add_popup_choice(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result add_text_input(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_TextInputDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result add_submenu(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle parent_page,
        const ERUI_SubmenuDesc* description,
        ERUI_PageHandle* out_child_page,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result set_page_presentation(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_PagePresentationDesc* description) noexcept;
    ERUI_Result commit_provider(ERUI_ProviderHandle provider) noexcept;
    ERUI_Result abort_provider(ERUI_ProviderHandle provider) noexcept;
    ERUI_Result set_row_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        std::uint8_t value) noexcept;
    ERUI_Result get_row_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        std::uint8_t* out_value) noexcept;
    ERUI_Result set_text_input_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        const ERUI_Utf16View* value) noexcept;
    ERUI_Result get_text_input_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        std::uint16_t* output,
        std::uint32_t output_capacity,
        std::uint32_t* out_length) noexcept;
    ERUI_Result enqueue_alert(
        ERUI_ProviderHandle provider,
        const ERUI_AlertDesc* description) noexcept;

    void apply_pending_values() noexcept;

private:
    enum class RowKind : std::uint8_t {
        button,
        toggle,
        slider,
        inline_choice,
        popup_choice,
        text_input,
        submenu,
    };

    struct Row {
        ERUI_RowHandle handle{};
        ERUI_ProviderHandle provider{};
        RowKind kind{RowKind::button};
        std::wstring label{};
        std::wstring help{};
        bool enabled{true};
        ERUI_ButtonCallback button_callback{};
        ERUI_ValueChangedCallback changed_callback{};
        ERUI_TextInputChangedCallback text_changed_callback{};
        void* user_data{};
        alignas(4) volatile std::uint8_t native_value{};
        std::atomic<std::uint8_t> public_value{};
        std::atomic<std::uint8_t> pending_value{};
        std::atomic_bool pending_write{false};
        erui::SliderSpec slider{};
        std::vector<std::wstring> choices{};
        std::unique_ptr<erui::detail::TextInputState> text_input_state{};
        ERUI_PageHandle child_page{};
    };

    struct Page {
        ERUI_PageHandle handle{};
        ERUI_ProviderHandle provider{};
        std::wstring title{};
        std::wstring help{};
        std::wstring menu_title{};
        std::wstring page_title{};
        ERUI_PageTitleFormatter title_formatter{};
        void* title_formatter_user_data{};
        std::atomic_bool formatter_fallback_logged{false};
        std::vector<Row*> rows{};
    };

    struct Provider {
        ERUI_ProviderHandle handle{};
        HMODULE owner_module{};
        HMODULE pinned_module{};
        std::uint32_t api_version{};
        std::string id{};
        std::wstring display_name{};
        std::int32_t priority{};
        std::uint64_t ordinal{};
        ERUI_PageHandle root_page{};
        bool committed{};
        std::vector<std::unique_ptr<Page>> pages{};
        std::vector<std::unique_ptr<Row>> rows{};
        std::unordered_map<ERUI_PageHandle, Page*> page_lookup{};
        std::unordered_map<ERUI_RowHandle, Row*> row_lookup{};
    };

    static void button_bridge(void* user_data) noexcept;
    static void value_bridge(std::uint8_t value, void* user_data) noexcept;
    static void text_input_bridge(
        std::wstring_view value,
        void* user_data) noexcept;
    static bool page_title_bridge(
        const erui::PageTitleFormatRequest& request,
        std::wstring& output,
        void* user_data) noexcept;

    [[nodiscard]] ERUI_Result validate_provider_and_page_locked(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        Provider*& out_provider,
        Page*& out_page) noexcept;
    [[nodiscard]] Page& create_page_locked(
        Provider& provider,
        std::wstring title,
        std::wstring help);
    [[nodiscard]] Row& create_row_locked(Provider& provider, Page& page);
    ERUI_Result add_choice(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ChoiceDesc* description,
        ERUI_RowHandle* out_row,
        RowKind kind) noexcept;
    void append_page_locked(
        const Provider& provider,
        const Page& source,
        erui::Page& destination);
    [[nodiscard]] std::unique_ptr<erui::Menu> freeze_and_build_locked();
    void note_activity_locked() noexcept;

    mutable std::mutex mutex_{};
    bool open_{};
    std::uint64_t generation_{};
    std::uint64_t last_activity_tick_{};
    std::uint64_t next_handle_{1};
    std::uint64_t next_ordinal_{};
    std::unordered_map<ERUI_ProviderHandle, std::unique_ptr<Provider>> providers_{};
    erui::MenuLocalization pagination_locale_{};
};

Registry& registry() noexcept;
void set_api_state(ApiState state) noexcept;
[[nodiscard]] ApiState api_state() noexcept;

} // namespace erui::host
