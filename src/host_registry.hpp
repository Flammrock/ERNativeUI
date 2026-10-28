#pragma once

#include <ernativeui/erui.h>
#include "host_storage.hpp"
#include "menu.hpp"
#include "scaleform_html.hpp"
#include "tab_configuration.hpp"

#include <Windows.h>

#include <array>
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

    // Custom tabs are advertised only after an authoritative native backend
    // has completed its compatibility checks. Availability is immutable once
    // provider registration opens, so every negotiated table observes one
    // stable contract for the lifetime of that registration epoch.
    [[nodiscard]] bool enable_custom_tabs_backend() noexcept;
    [[nodiscard]] bool custom_tabs_backend_available() const noexcept;
    // Title-screen text follows the same publication rule: advertise it only
    // after the validated GFX transform/open seam is ready, and never change
    // availability once registration has opened.
    [[nodiscard]] bool enable_title_screen_text_backend() noexcept;
    [[nodiscard]] bool title_screen_text_backend_available() const noexcept;
    // Runtime row updates are published only on exact game builds whose
    // native label/help presentation paths have been live validated.
    [[nodiscard]] bool enable_updates_backend() noexcept;
    [[nodiscard]] bool updates_backend_available() const noexcept;
    [[nodiscard]] bool enable_submenu_updates_backend() noexcept;
    [[nodiscard]] bool submenu_updates_backend_available() const noexcept;
    [[nodiscard]] bool enable_tab_updates_backend() noexcept;
    [[nodiscard]] bool tab_updates_backend_available() const noexcept;
    // Typed-row runtime interaction/appearance is advertised independently
    // from text updates and only after every admitted native controller and
    // Scaleform presentation seam passes exact-build validation.
    [[nodiscard]] bool enable_row_availability_backend() noexcept;
    [[nodiscard]] bool row_availability_backend_available() const noexcept;
    // Runtime layout projection is advertised separately because its
    // destructive in-place rebuild has a narrower exact-build matrix than
    // label/help or availability presentation.
    [[nodiscard]] bool enable_row_visibility_backend() noexcept;
    [[nodiscard]] bool row_visibility_backend_available() const noexcept;
    // Same-cardinality formatted-slider label replacement is advertised
    // independently so clients never infer it merely from generic updates.
    [[nodiscard]] bool enable_row_configuration_backend() noexcept;
    [[nodiscard]] bool row_configuration_backend_available() const noexcept;
    void open_registration() noexcept;
    void set_host_locale(erui::MenuLocalization pagination);
    void set_tab_configuration(erui::detail::TabConfiguration configuration);
    [[nodiscard]] erui::detail::TabDiagnosticsReport
        tab_diagnostics_report() const;
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
    ERUI_Result get_builtin_page(
        ERUI_ProviderHandle provider,
        ERUI_BuiltinPage builtin_page,
        ERUI_PageHandle* out_page) noexcept;
    ERUI_Result add_tab(
        ERUI_ProviderHandle provider,
        const ERUI_TabDesc* description,
        ERUI_PageHandle* out_page) noexcept;
    ERUI_Result add_title_screen_text(
        ERUI_ProviderHandle provider,
        const ERUI_TitleScreenTextDesc* description) noexcept;
    ERUI_Result add_input_section(
        ERUI_ProviderHandle provider,
        const ERUI_InputSectionDesc* description,
        ERUI_InputSectionHandle* out_section) noexcept;
    ERUI_Result add_input_action(
        ERUI_ProviderHandle provider,
        ERUI_InputSectionHandle section,
        const ERUI_InputActionDesc* description,
        ERUI_InputActionHandle* out_action) noexcept;
    ERUI_Result set_assignments_changed_handler(
        ERUI_ProviderHandle provider,
        const ERUI_AssignmentsChangedHandlerDesc* description) noexcept;
    ERUI_Result set_action_inputs(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        const ERUI_ActionInputs* inputs) noexcept;
    ERUI_Result get_action_inputs(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        ERUI_ActionInputs* out_inputs) noexcept;
    ERUI_Result get_action_default_inputs(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        ERUI_ActionInputs* out_inputs) noexcept;
    ERUI_Result reset_action_inputs(
        ERUI_ProviderHandle provider,
        ERUI_InputActionHandle action,
        ERUI_InputDevices devices) noexcept;
    ERUI_Result open_storage(
        ERUI_ProviderHandle provider,
        const ERUI_StorageDesc* description,
        ERUI_StorageHandle* out_storage) noexcept;
    ERUI_Result storage_load(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage) noexcept;
    ERUI_Result storage_save(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage) noexcept;
    ERUI_Result storage_get_utf8(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        char* output,
        std::uint32_t output_capacity,
        std::uint32_t* out_length) noexcept;
    ERUI_Result storage_set_utf8(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        const ERUI_StringView* value) noexcept;
    ERUI_Result storage_erase(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key) noexcept;
    ERUI_Result storage_get_action_inputs(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        ERUI_ActionInputs* out_inputs) noexcept;
    ERUI_Result storage_set_action_inputs(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StorageKey* key,
        const ERUI_ActionInputs* inputs) noexcept;
    ERUI_Result storage_apply_assignment_changes(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        const ERUI_StringView* section,
        const ERUI_AssignmentChange* changes,
        std::uint32_t change_count) noexcept;
    ERUI_Result storage_get_info(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        ERUI_StorageInfo* out_info) noexcept;
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
    ERUI_Result add_numeric_slider(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_NumericSliderDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result set_numeric_slider_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        const ERUI_DecimalValue* value) noexcept;
    ERUI_Result get_numeric_slider_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        ERUI_DecimalValue* out_value) noexcept;
    ERUI_Result add_custom_slider(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_CustomSliderDesc* description,
        ERUI_RowHandle* out_row) noexcept;
    ERUI_Result set_custom_slider_index(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        std::uint32_t index) noexcept;
    ERUI_Result get_custom_slider_index(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        std::uint32_t* out_index) noexcept;
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
    ERUI_Result add_color_picker(
        ERUI_ProviderHandle provider,
        ERUI_PageHandle page,
        const ERUI_ColorPickerDesc* description,
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
    ERUI_Result set_color_picker_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        const ERUI_Color* value) noexcept;
    ERUI_Result get_color_picker_value(
        ERUI_ProviderHandle provider,
        ERUI_RowHandle row,
        ERUI_Color* out_value) noexcept;
    ERUI_Result enqueue_alert(
        ERUI_ProviderHandle provider,
        const ERUI_AlertDesc* description) noexcept;
    ERUI_Result commit_updates(
        ERUI_ProviderHandle provider,
        const ERUI_UpdateBatchDesc* description) noexcept;

    void apply_pending_values() noexcept;

private:
    enum class RowKind : std::uint8_t {
        button,
        toggle,
        slider,
        numeric_slider,
        custom_slider,
        inline_choice,
        popup_choice,
        text_input,
        color_picker,
        submenu,
    };

    struct Row {
        ERUI_RowHandle handle{};
        ERUI_ProviderHandle provider{};
        ERUI_PageHandle page{};
        RowKind kind{RowKind::button};
        std::wstring label{};
        std::wstring help{};
        std::wstring caption{};
        std::vector<erui::detail::ScaleformTextStyleSpan> label_styles{};
        std::vector<erui::detail::ScaleformTextStyleSpan> help_styles{};
        std::vector<erui::detail::ScaleformTextStyleSpan> caption_styles{};
        bool caption_overridden{};
        bool enabled{true};
        bool interactive{true};
        bool visible{true};
        ERUI_AvailabilityAppearance availability_appearance{
            ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY};
        ERUI_ButtonCallback button_callback{};
        ERUI_ValueChangedCallback changed_callback{};
        ERUI_NumericSliderChangedCallback numeric_slider_changed_callback{};
        ERUI_CustomSliderChangedCallback custom_slider_changed_callback{};
        ERUI_TextInputChangedCallback text_changed_callback{};
        ERUI_ColorPickerChangedCallback color_changed_callback{};
        void* user_data{};
        alignas(4) volatile std::uint8_t native_value{};
        std::atomic<std::uint8_t> public_value{};
        std::atomic<std::uint8_t> pending_value{};
        std::atomic_bool pending_write{false};
        std::atomic<std::uint8_t> silent_native_value{};
        std::atomic_bool silent_native_transition{false};
        erui::SliderSpec slider{};
        struct NumericSliderData {
            std::int64_t minimum_scaled_value{};
            std::int64_t maximum_scaled_value{};
            std::int64_t step_scaled_value{};
            std::uint32_t decimal_places{};
            std::uint32_t value_count{};
        } numeric_slider{};
        std::vector<std::wstring> slider_value_texts{};
        std::vector<std::wstring> choices{};
        std::unique_ptr<erui::detail::TextInputState> text_input_state{};
        std::unique_ptr<erui::detail::ColorPickerState> color_picker_state{};
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
        ERUI_RowHandle submenu_opener{ERUI_INVALID_ROW};
        erui::detail::BuiltinPage builtin_page{
            erui::detail::BuiltinPage::count};
        bool custom_tab{};
        bool tab_visible{true};
        std::string tab_id{};
        std::int32_t preferred_tab_order{};
        erui::TabIconSpec tab_icon{};
        std::vector<Row*> rows{};
    };

    struct Binding;

    struct BindingSection {
        ERUI_InputSectionHandle handle{};
        ERUI_ProviderHandle provider{};
        std::wstring label{};
        std::vector<Binding*> bindings{};
    };

    struct Binding {
        ERUI_InputActionHandle handle{};
        ERUI_ProviderHandle provider{};
        ERUI_InputSectionHandle section{};
        std::string id{};
        std::wstring label{};
        ERUI_ActionInputs default_inputs{};
        ERUI_ActionInputs current_inputs{};
        ERUI_InputActionActivatedCallback activated_callback{};
        void* user_data{};
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
        std::array<ERUI_PageHandle, erui::detail::builtin_page_count>
            builtin_pages{};
        ERUI_AssignmentsChangedCallback assignments_changed_callback{};
        void* assignments_changed_user_data{};
        ERUI_StorageHandle storage_handle{};
        std::shared_ptr<ProviderStorage> storage{};
        bool committed{};
        std::vector<std::unique_ptr<Page>> pages{};
        std::vector<std::unique_ptr<Row>> rows{};
        std::vector<std::unique_ptr<BindingSection>> binding_sections{};
        std::vector<std::unique_ptr<Binding>> bindings{};
        std::vector<erui::TitleScreenTextSpec> title_screen_texts{};
        std::unordered_map<ERUI_PageHandle, Page*> page_lookup{};
        std::unordered_map<std::string, Page*> tab_id_lookup{};
        std::unordered_map<ERUI_RowHandle, Row*> row_lookup{};
        std::unordered_map<ERUI_InputSectionHandle, BindingSection*>
            binding_section_lookup{};
        std::unordered_map<ERUI_InputActionHandle, Binding*> binding_lookup{};
        std::unordered_map<std::string, Binding*> binding_id_lookup{};
    };

    static void button_bridge(void* user_data) noexcept;
    static void value_bridge(std::uint8_t value, void* user_data) noexcept;
    static void text_input_bridge(
        std::wstring_view value,
        void* user_data) noexcept;
    static void color_picker_bridge(
        erui::detail::RgbColor value,
        void* user_data) noexcept;
    static void input_action_bridge(
        std::uint32_t devices,
        void* user_data) noexcept;
    static void input_assignments_bridge(
        const ERUI_ActionInputs& previous,
        const ERUI_ActionInputs& current,
        ERUI_AssignmentChangeReason reason,
        ERUI_InputDevices changed_devices,
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
    [[nodiscard]] ERUI_Result require_provider_api_version(
        ERUI_ProviderHandle provider,
        std::uint32_t minimum_version) const noexcept;
    [[nodiscard]] ERUI_Result find_storage_locked(
        ERUI_ProviderHandle provider,
        ERUI_StorageHandle storage,
        std::shared_ptr<ProviderStorage>& output) const noexcept;
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
    bool custom_tabs_backend_available_{};
    bool title_screen_text_backend_available_{};
    bool updates_backend_available_{};
    bool submenu_updates_backend_available_{};
    bool tab_updates_backend_available_{};
    bool row_availability_backend_available_{};
    bool row_visibility_backend_available_{};
    bool row_configuration_backend_available_{};
    bool open_{};
    bool frozen_{};
    std::uint64_t generation_{};
    std::uint64_t last_activity_tick_{};
    std::uint64_t next_handle_{1};
    std::uint64_t next_ordinal_{};
    std::unordered_map<ERUI_ProviderHandle, std::unique_ptr<Provider>> providers_{};
    erui::MenuLocalization pagination_locale_{};
    erui::detail::TabConfiguration tab_configuration_{};
    erui::detail::TabDiagnosticsReport tab_diagnostics_report_{};
};

Registry& registry() noexcept;
void set_api_state(ApiState state) noexcept;
[[nodiscard]] ApiState api_state() noexcept;

} // namespace erui::host
