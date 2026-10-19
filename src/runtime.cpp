#include "runtime.hpp"

#include "addresses.hpp"
#include "game_build.hpp"
#include "hooks.hpp"
#include "module.hpp"
#include "runtime_log.hpp"
#include "runtime_requirements.hpp"
#include "runtime_state.hpp"
#include "native_input_bindings.hpp"
#include "native_tab_catalogue.hpp"
#include "native_tabs.hpp"
#include "native_text_input.hpp"

#include <Windows.h>

#include <algorithm>
#include <exception>
#include <utility>

namespace erui {
namespace {

// Installation and explicit teardown mutate one process-wide native hook
// graph. Serialize them so uninstall cannot pass a pre-exposure tab check and
// then race the first hook enable.
SRWLOCK g_runtime_lifecycle_lock = SRWLOCK_INIT;

class ScopedRuntimeLifecycleLock final {
public:
    ScopedRuntimeLifecycleLock() noexcept {
        AcquireSRWLockExclusive(&g_runtime_lifecycle_lock);
    }
    ~ScopedRuntimeLifecycleLock() {
        ReleaseSRWLockExclusive(&g_runtime_lifecycle_lock);
    }

    ScopedRuntimeLifecycleLock(const ScopedRuntimeLifecycleLock&) = delete;
    ScopedRuntimeLifecycleLock& operator=(
        const ScopedRuntimeLifecycleLock&) = delete;
};

InstallResult fail_install(InstallError error) noexcept {
    if (!erui::native::remove_native_tabs()) {
        // No fallible subsystem should be rolled back after carrier or
        // viewport hooks are exposed.
        // Preserve the complete pinned runtime instead of invalidating an
        // in-flight trampoline or freeing storage a callback could borrow.
        detail::logf(
            LogLevel::error,
            "Install failure could not hot-remove exposed API 1.3 carrier/viewport hooks; the pinned runtime was retained intact");
        return InstallResult{error};
    }
    erui::native::remove_native_input_bindings();
    erui::native::remove_native_menu_hooks();
    erui::native::reset_native_text_inputs();
    detail::runtime_state().tab_catalogue.reset();
    detail::runtime_state().menu.reset();
    detail::runtime_state().installed.store(false, std::memory_order_release);
    return InstallResult{error};
}

std::uint8_t normalize_slider(
    std::uint8_t value,
    const SliderSpec& range) noexcept {
    const auto minimum = static_cast<std::uint8_t>(range.minimum);
    const auto maximum = static_cast<std::uint8_t>(range.maximum);
    value = std::clamp(value, minimum, maximum);

    const unsigned step = range.step <= 0 ? 1u : static_cast<unsigned>(range.step);
    const unsigned offset = static_cast<unsigned>(value - minimum);
    const unsigned snapped = (offset / step) * step;
    const unsigned result = static_cast<unsigned>(minimum) + snapped;
    return static_cast<std::uint8_t>(std::min<unsigned>(result, maximum));
}

const char* page_kind_name(PageKind kind) noexcept {
    switch (kind) {
    case PageKind::root:
        return "root";
    case PageKind::submenu:
        return "submenu";
    case PageKind::tab:
        return "tab";
    }
    return "unknown";
}

const char* row_kind_name(RowKind kind) noexcept {
    switch (kind) {
    case RowKind::toggle:
        return "toggle";
    case RowKind::slider:
        return "slider";
    case RowKind::numeric_slider:
        return "numeric-slider";
    case RowKind::custom_slider:
        return "custom-slider";
    case RowKind::inline_choice:
        return "inline-choice";
    case RowKind::popup_choice:
        return "popup-choice";
    case RowKind::text_input:
        return "text-input";
    case RowKind::color_picker:
        return "color-picker";
    case RowKind::button:
        return "button";
    case RowKind::submenu:
        return "submenu";
    }
    return "unknown";
}

void log_page_plan(
    const char* name,
    const detail::PagePlan& plan) noexcept {
    detail::logf(
        LogLevel::trace,
        "paginationPlan %s logicalPage=%zu logicalRows=%zu physicalPages=%zu capacity=%zu",
        name,
        plan.logical_page_index,
        plan.slices.empty()
            ? 0
            : plan.slices.back().first_row + plan.slices.back().row_count,
        plan.slices.size(),
        plan.physical_capacity);
    for (const detail::PageSlice& slice : plan.slices) {
        detail::logf(
            LogLevel::trace,
            "paginationSlice %s logicalPage=%zu slice=%zu/%zu firstRow=%zu contentRows=%zu previous=%d next=%d nativeRows=%zu",
            name,
            slice.logical_page_index,
            slice.slice_index + 1,
            slice.slice_count,
            slice.first_row,
            slice.row_count,
            slice.has_previous ? 1 : 0,
            slice.has_next ? 1 : 0,
            slice.row_count + (slice.has_previous ? 1u : 0u) +
                (slice.has_next ? 1u : 0u));
    }
}

void log_compiled_menu_diagnostics(const detail::CompiledMenu& menu) noexcept {
    if (!detail::diagnostics_enabled()) {
        return;
    }

    detail::logf(
        LogLevel::trace,
        "diagnostic: runtimeApi=0x%08X RuntimeOptionsSize=%zu logSinkOffset=%zu rootPage=%zu pagination=%d gameOptionsCapacity=%u",
        static_cast<unsigned>(runtime_api_version),
        sizeof(RuntimeOptions),
        offsetof(RuntimeOptions, log_sink),
        menu.root_page_index,
        menu.pagination_required_for_capacity(
            detail::runtime_state().options.game_options_visual_capacity) ? 1 : 0,
        static_cast<unsigned>(detail::runtime_state().options.game_options_visual_capacity));

    for (std::size_t page_index = 0; page_index < menu.pages.size(); ++page_index) {
        const detail::CompiledPage& page = menu.pages[page_index];
        detail::logf(
            LogLevel::trace,
            "diagnostic: page[%zu] kind=%s titleId=0x%X helpId=0x%X rows=%zu",
            page_index,
            page_kind_name(page.kind),
            static_cast<unsigned>(page.title_id),
            static_cast<unsigned>(page.help_id),
            page.rows.size());

        for (std::size_t row_index = 0; row_index < page.rows.size(); ++row_index) {
            const detail::CompiledRow& row = page.rows[row_index];
            detail::logf(
                LogLevel::trace,
                "diagnostic: page[%zu].row[%zu] kind=%s labelId=0x%X helpId=0x%X value=%p enabled=%d range=%d..%d step=%d action=0x%llX userData=%p targetPage=%zu",
                page_index,
                row_index,
                row_kind_name(row.kind),
                static_cast<unsigned>(row.label_id),
                static_cast<unsigned>(row.help_id),
                const_cast<std::uint8_t*>(row.byte_value),
                row.enabled ? 1 : 0,
                row.slider.minimum,
                row.slider.maximum,
                row.slider.step,
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(row.action.callback)),
                row.action.user_data,
                row.target_page_index);
        }

        if (page_index != menu.root_page_index &&
            page_index < menu.page_plans.size()) {
            log_page_plan("submenu", menu.page_plans[page_index]);
        }
    }

    for (const detail::RootPagePlan& root : menu.root_plans) {
        detail::logf(
            LogLevel::trace,
            "rootPagination capacity=%u vanillaRows=%zu customSlots=%zu logicalRows=%zu physicalPages=%zu",
            static_cast<unsigned>(root.native_capacity),
            root.vanilla_row_count,
            root.custom_capacity,
            menu.root_page().rows.size(),
            root.pages.slices.size());
        log_page_plan("root", root.pages);
    }
}

} // namespace

InstallResult install_impl(
    Menu& menu,
    RuntimeOptions options,
    const native::GameAddresses* resolved_addresses,
    const detail::TabConfiguration* tab_configuration) noexcept {
    const ScopedRuntimeLifecycleLock lifecycle_lock;
    detail::RuntimeState& runtime = detail::runtime_state();
    bool expected = false;
    if (!runtime.installed.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return InstallResult{InstallError::already_installed};
    }

    detail::set_diagnostic_logging(options.enable_diagnostics);
    detail::set_log_sink(options.log_sink);
    runtime.options = options;
    runtime.custom_text_hits.store(0, std::memory_order_release);
    runtime.diagnostic_value_logs.store(0, std::memory_order_release);
    runtime.button_action_hits.store(0, std::memory_order_release);
    runtime.diagnostic_button_action_logs.store(0, std::memory_order_release);
    runtime.button_action_faults.store(0, std::memory_order_release);
    runtime.submenu_open_requests.store(0, std::memory_order_release);
    runtime.submenu_page_hits.store(0, std::memory_order_release);
    runtime.submenu_open_faults.store(0, std::memory_order_release);
    runtime.diagnostic_submenu_logs.store(0, std::memory_order_release);
    runtime.pagination_next_hits.store(0, std::memory_order_release);
    runtime.pagination_previous_hits.store(0, std::memory_order_release);

    if (!options.enable_row_injection ||
        options.injection_cooldown_ms > 5000 ||
        options.game_options_visual_capacity <
             detail::game_options_vanilla_capacity ||
        options.game_options_visual_capacity >
             detail::game_options_max_visual_capacity) {
        detail::logf(LogLevel::error, "Invalid ERNativeUI runtime options");
        return fail_install(InstallError::invalid_options);
    }

    try {
        runtime.menu = detail::MenuCompiler::compile(menu);
    } catch (const std::exception& exception) {
        detail::logf(LogLevel::error, "Menu compilation failed: %s", exception.what());
        return fail_install(InstallError::menu_compile_failed);
    } catch (...) {
        detail::logf(LogLevel::error, "Menu compilation failed with an unknown exception");
        return fail_install(InstallError::menu_compile_failed);
    }

    if (tab_configuration) {
        detail::NativeTabCatalogueBuildResult catalogue =
            detail::build_native_tab_catalogue(
                *runtime.menu, *tab_configuration);
        if (!catalogue) {
            detail::logf(
                LogLevel::error,
                "Native-tab catalogue validation failed: error=%s tabIndex=%zu",
                detail::native_tab_catalogue_error_name(catalogue.error),
                catalogue.tab_index);
            return fail_install(InstallError::menu_compile_failed);
        }
        runtime.tab_catalogue = std::move(catalogue.catalogue);
    } else if (!runtime.menu->tab_page_indices.empty()) {
        // Custom tabs are a host-owned API 1.3 facility. Never install a menu
        // containing them without the exact player configuration used to
        // resolve their deterministic order.
        detail::logf(
            LogLevel::error,
            "Compiled menu contains custom tabs but no tab configuration was supplied");
        return fail_install(InstallError::invalid_options);
    }

    detail::logf(
        LogLevel::info,
        "Compiled menu: pages=%zu rootRows=%zu tabs=%zu modelButtons=%zu modelSubmenus=%zu popupChoices=%zu textInputs=%zu colorPickers=%zu numericSliders=%zu customSliders=%zu inputBindingSections=%zu inputBindings=%zu customTexts=%zu pagination=%d",
        runtime.menu->pages.size(),
        runtime.menu->root_page().rows.size(),
        runtime.menu->tab_page_indices.size(),
        runtime.menu->modeled_button_count,
        runtime.menu->modeled_submenu_count,
        runtime.menu->modeled_popup_choice_count,
        runtime.menu->modeled_text_input_count,
        runtime.menu->modeled_color_picker_count,
        runtime.menu->modeled_numeric_slider_count,
        runtime.menu->modeled_custom_slider_count,
        runtime.menu->input_binding_sections.size(),
        runtime.menu->modeled_input_binding_count,
        runtime.menu->texts.size(),
        runtime.menu->pagination_required_for_capacity(
            options.game_options_visual_capacity) ? 1 : 0);
    log_compiled_menu_diagnostics(*runtime.menu);

    erui::native::ModuleView game{};
    HMODULE game_module = GetModuleHandleW(nullptr);
    if (!game.initialize(game_module)) {
        detail::logf(LogLevel::error, "Failed to parse the main process PE image");
        return fail_install(InstallError::game_module_invalid);
    }

    detail::logf(
        LogLevel::info,
        "Game image base=%p size=0x%zX PE timestamp=0x%08X",
        game.base(),
        game.image_size(),
        static_cast<unsigned>(game.timestamp()));

    if (const erui::native::EldenRingBuildIdentity* const identity =
            erui::native::find_elden_ring_build(
                game.timestamp(), game.image_size())) {
        detail::logf(
            LogLevel::info,
            "Selected exact native profile: Elden Ring %.*s",
            static_cast<int>(identity->product_version.size()),
            identity->product_version.data());
    } else {
        detail::logf(
            LogLevel::warning,
            "No exact native profile for this game image; build-locked features will remain unavailable");
    }

    const bool active_pagination = options.enable_row_injection &&
        runtime.menu->pagination_required_for_capacity(
            detail::game_options_min_plan_capacity);
    bool has_non_game_builtin_rows = false;
    for (std::uint8_t native_category = 1;
         native_category < runtime.menu->builtin_page_indices.size();
         ++native_category) {
        const detail::CompiledPage* const builtin =
            runtime.menu->builtin_page(native_category);
        if (builtin && !builtin->rows.empty()) {
            has_non_game_builtin_rows = true;
            break;
        }
    }
    bool custom_tab_capacity_pagination = false;
    for (const std::size_t page_index : runtime.menu->tab_page_indices) {
        if (page_index < runtime.menu->pages.size() &&
            runtime.menu->pages[page_index].rows.size() >
                1u) {
            // The Graphics carrier's live capacity is read only after its GFX
            // object exists. Reserve continuation interfaces against the
            // conservative one-row lower bound during address resolution.
            custom_tab_capacity_pagination = true;
            break;
        }
    }
    // A built-in panel's free slots are a live GFX property. Even a page that
    // contains only toggles may need a native Next action and Back-enabled
    // continuation, so resolve those shared interfaces conservatively.
    const bool runtime_capacity_pagination =
        options.enable_row_injection && has_non_game_builtin_rows;
    // ColorPicker reuses Elden Ring's native action-row controller even when
    // its GFX presentation is redirected to the standalone color widget.
    // Resolve that controller for a color-only menu too; otherwise the host
    // would install successfully while silently omitting every ColorPicker.
    bool require_buttons = active_pagination || runtime_capacity_pagination ||
        custom_tab_capacity_pagination ||
        runtime.menu->modeled_color_picker_count != 0;
    // Every continuation page is constructed through the native subpage
    // interfaces, even when its logical owner is not an explicit Submenu row
    // (for example, an overflowing custom tab). Keep address resolution in
    // lockstep with hooks.cpp, which enables the same route machinery for
    // any active pagination plan.
    bool require_submenus = detail::pagination_requires_submenus(
        options.enable_row_injection,
        active_pagination,
        runtime.menu->root_plan(detail::game_options_min_plan_capacity)
                .pages.slices.size() > 1,
        runtime_capacity_pagination,
        custom_tab_capacity_pagination);
    const bool require_native_back =
        active_pagination || runtime_capacity_pagination ||
        custom_tab_capacity_pagination;
    const bool require_text_inputs = options.enable_row_injection &&
        runtime.menu->modeled_text_input_count != 0;
    const bool require_input_bindings =
        runtime.menu->modeled_input_binding_count != 0;
    const bool require_formatted_sliders =
        runtime.menu->modeled_numeric_slider_count != 0 ||
        runtime.menu->modeled_custom_slider_count != 0;
    if (options.enable_row_injection) {
        for (std::size_t page_index = 0;
             page_index < runtime.menu->pages.size(); ++page_index) {
            if (!runtime.menu->page_reachable(page_index)) continue;
            const detail::CompiledPage& page = runtime.menu->pages[page_index];
            for (const detail::CompiledRow& row : page.rows) {
                if (!row.enabled) {
                    continue;
                }
                if (row.kind == RowKind::button && row.action) {
                    require_buttons = true;
                } else if (row.kind == RowKind::submenu &&
                    row.target_page_index != detail::invalid_compiled_index) {
                    require_buttons = true;
                    require_submenus = true;
                }
            }
        }
    }

    erui::native::GameAddresses addresses{};
    if (resolved_addresses) {
        addresses = *resolved_addresses;
        if (!addresses.complete(
                options.enable_row_injection,
                require_buttons,
                require_submenus,
                runtime.menu->modeled_popup_choice_count != 0,
                require_text_inputs,
                options.enable_custom_text) ||
            (require_native_back && !addresses.native_back)) {
            detail::logf(
                LogLevel::error,
                "Pre-resolved native interfaces do not satisfy the compiled menu");
            return fail_install(InstallError::address_resolution_failed);
        }
        detail::logf(
            LogLevel::info,
            "Using native interfaces captured before third-party hook installation");
    } else {
        if (!erui::native::resolve_game_addresses(
                game,
                addresses,
                options.enable_row_injection,
                require_buttons,
                require_submenus,
                require_native_back,
                runtime.menu->modeled_popup_choice_count != 0,
                require_text_inputs,
                options.enable_custom_text)) {
            return fail_install(InstallError::address_resolution_failed);
        }
    }

    if (require_formatted_sliders &&
        (!addresses.slider_refresh || !addresses.title_bridge_complete())) {
        detail::logf(
            LogLevel::error,
            "Formatted sliders require SliderCtrl refresh and Scaleform text interfaces");
        return fail_install(InstallError::address_resolution_failed);
    }

    if (require_text_inputs &&
        !erui::native::prepare_native_text_inputs(*runtime.menu, addresses)) {
        detail::logf(
            LogLevel::error,
            "Failed to prepare native TextInput bindings");
        return fail_install(InstallError::address_resolution_failed);
    }

    if (require_input_bindings &&
        !erui::native::prepare_native_input_bindings(*runtime.menu)) {
        detail::logf(
            LogLevel::error,
            "Failed to prepare native input bindings");
        return fail_install(InstallError::address_resolution_failed);
    }

    const erui::native::HookInstallStatus hook_status =
        erui::native::install_native_menu_hooks(addresses);
    if (hook_status != erui::native::HookInstallStatus::success) {
        return fail_install(InstallError::hook_install_failed);
    }

    if (require_input_bindings &&
        !erui::native::install_native_input_bindings()) {
        detail::logf(
            LogLevel::error,
            "Failed to install native input-binding hooks");
        return fail_install(InstallError::hook_install_failed);
    }

    // Native tabs become process-lifetime once their hooks are installed.
    // Keep this as the terminal fallible install stage so a successful tab
    // install is never followed by rollback work.
    if (runtime.tab_catalogue &&
        runtime.tab_catalogue->custom_count() != 0 &&
        !erui::native::install_native_tabs(*runtime.tab_catalogue)) {
        detail::logf(
            LogLevel::error,
            "Failed to install the native custom-tab backend after menu hooks");
        return fail_install(InstallError::hook_install_failed);
    }

    detail::logf(LogLevel::info, "ERNativeUI install completed successfully");
    return InstallResult{};
}

InstallResult install(Menu& menu, RuntimeOptions options) noexcept {
    return install_impl(menu, options, nullptr, nullptr);
}

InstallResult install_with_resolved_addresses(
    Menu& menu,
    RuntimeOptions options,
    const native::GameAddresses& addresses) noexcept {
    return install_impl(menu, options, &addresses, nullptr);
}

InstallResult install_with_tab_configuration(
    Menu& menu,
    RuntimeOptions options,
    const detail::TabConfiguration& tab_configuration) noexcept {
    return install_impl(menu, options, nullptr, &tab_configuration);
}

InstallResult install_with_resolved_addresses_and_tab_configuration(
    Menu& menu,
    RuntimeOptions options,
    const native::GameAddresses& addresses,
    const detail::TabConfiguration& tab_configuration) noexcept {
    return install_impl(
        menu, options, &addresses, &tab_configuration);
}

void uninstall() noexcept {
    const ScopedRuntimeLifecycleLock lifecycle_lock;
    detail::RuntimeState& runtime = detail::runtime_state();
    if (!runtime.installed.load(std::memory_order_acquire)) {
        return;
    }
    // Run the tab lifetime gate before changing any other subsystem. A game
    // callback can borrow catalogue/image storage as soon as the hooks are
    // installed, so partial teardown would leave dangling native pointers;
    // the pinned runtime must remain intact.
    if (!erui::native::remove_native_tabs()) {
        detail::logf(
            LogLevel::warning,
            "ERNativeUI uninstall was refused after API 1.3 carrier/viewport hook exposure; close the process to release the pinned runtime safely");
        return;
    }
    if (!runtime.installed.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    erui::native::remove_native_input_bindings();
    erui::native::remove_native_menu_hooks();
    erui::native::reset_native_text_inputs();
    runtime.button_action_hits.store(0, std::memory_order_release);
    runtime.diagnostic_button_action_logs.store(0, std::memory_order_release);
    runtime.button_action_faults.store(0, std::memory_order_release);
    runtime.submenu_open_requests.store(0, std::memory_order_release);
    runtime.submenu_page_hits.store(0, std::memory_order_release);
    runtime.submenu_open_faults.store(0, std::memory_order_release);
    runtime.diagnostic_submenu_logs.store(0, std::memory_order_release);
    runtime.pagination_next_hits.store(0, std::memory_order_release);
    runtime.pagination_previous_hits.store(0, std::memory_order_release);
    runtime.tab_catalogue.reset();
    runtime.menu.reset();
    detail::set_log_sink(nullptr);
    detail::set_diagnostic_logging(false);
}

bool installed() noexcept {
    return detail::runtime_state().installed.load(std::memory_order_acquire);
}

Capabilities capabilities() noexcept {
    return {};
}

std::uint64_t custom_text_hit_count() noexcept {
    return detail::runtime_state().custom_text_hits.load(std::memory_order_acquire);
}

std::uint64_t button_action_hit_count() noexcept {
    return detail::runtime_state().button_action_hits.load(std::memory_order_acquire);
}

std::uint64_t submenu_open_request_count() noexcept {
    return detail::runtime_state().submenu_open_requests.load(std::memory_order_acquire);
}

std::uint64_t submenu_page_hit_count() noexcept {
    return detail::runtime_state().submenu_page_hits.load(std::memory_order_acquire);
}

std::uint64_t pagination_next_hit_count() noexcept {
    return detail::runtime_state().pagination_next_hits.load(std::memory_order_acquire);
}

std::uint64_t pagination_previous_hit_count() noexcept {
    return detail::runtime_state().pagination_previous_hits.load(std::memory_order_acquire);
}

std::size_t poll_changes() noexcept {
    detail::RuntimeState& runtime = detail::runtime_state();
    if (!runtime.installed.load(std::memory_order_acquire) || !runtime.menu) {
        return 0;
    }

    native::dispatch_native_input_binding_events();

    std::size_t changes = 0;
    for (detail::CompiledPage& page : runtime.menu->pages) {
        for (detail::CompiledRow& row : page.rows) {
            if (row.kind == RowKind::text_input) {
                if (!row.text_input_state) continue;
                std::wstring value{};
                while (row.text_input_state->pop_committed_change(value)) {
                    ++changes;
                    row.text_action.invoke(value);
                }
                continue;
            }
            if (!row.byte_value) {
                continue;
            }

            std::uint8_t current = *row.byte_value;
            if (row.kind == RowKind::toggle) {
                current = current == 0 ? 0 : 1;
            } else if (row.kind == RowKind::slider) {
                current = normalize_slider(current, row.slider);
            } else if (row.kind == RowKind::numeric_slider ||
                row.kind == RowKind::custom_slider) {
                if (row.slider_value_texts.size() <
                        ERUI_SLIDER_MIN_VALUE_COUNT ||
                    row.slider_value_texts.size() >
                        ERUI_SLIDER_MAX_VALUE_COUNT) {
                    continue;
                }
                if (current >= row.slider_value_texts.size()) current = 0;
            } else if (row.kind == RowKind::inline_choice) {
                if (row.choice_ids.empty()) continue;
                if (current >= row.choice_ids.size()) current = 0;
            } else if (row.kind == RowKind::popup_choice) {
                if (row.choice_ids.empty()) continue;
                current = detail::synchronize_popup_choice_state(
                    *row.byte_value,
                    row.popup_choice_state,
                    row.last_observed_value,
                    row.choice_ids.size());
            } else {
                continue;
            }
            *row.byte_value = current;

            if (current == row.last_observed_value) {
                continue;
            }
            row.last_observed_value = current;
            ++changes;
            if (runtime.options.enable_diagnostics) {
                const std::uint32_t diagnostic_index =
                    runtime.diagnostic_value_logs.fetch_add(1, std::memory_order_relaxed);
                if (diagnostic_index < 256) {
                    detail::logf(
                        LogLevel::trace,
                        "diagnostic: value change[%u] kind=%s valuePtr=%p value=%u labelId=0x%X",
                        static_cast<unsigned>(diagnostic_index + 1),
                        row_kind_name(row.kind),
                        const_cast<std::uint8_t*>(row.byte_value),
                        static_cast<unsigned>(current),
                        static_cast<unsigned>(row.label_id));
                } else if (diagnostic_index == 256) {
                    detail::logf(
                        LogLevel::warning,
                        "diagnostic: further value-change traces are suppressed after 256 entries");
                }
            }
            row.value_action.invoke(current);
        }
    }
    return changes;
}

} // namespace erui
