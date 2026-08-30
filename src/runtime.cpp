#include "runtime.hpp"

#include "addresses.hpp"
#include "hooks.hpp"
#include "module.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"

#include <Windows.h>

#include <algorithm>
#include <exception>

namespace erui {
namespace {

InstallResult fail_install(InstallError error) noexcept {
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
    case RowKind::inline_choice:
        return "inline-choice";
    case RowKind::popup_choice:
        return "popup-choice";
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
        "diagnostic: runtimeApi=0x%08X RuntimeOptionsSize=%zu logSinkOffset=%zu rootPage=%zu pagination=%d controllerCapacity=%u",
        static_cast<unsigned>(runtime_api_version),
        sizeof(RuntimeOptions),
        offsetof(RuntimeOptions, log_sink),
        menu.root_page_index,
        menu.pagination_required_for_capacity(
            detail::runtime_state().options.controller_visual_capacity) ? 1 : 0,
        static_cast<unsigned>(detail::runtime_state().options.controller_visual_capacity));

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
    const native::GameAddresses* resolved_addresses) noexcept {
    detail::RuntimeState& runtime = detail::runtime_state();
    bool expected = false;
    if (!runtime.installed.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return InstallResult{InstallError::already_installed};
    }

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
        options.controller_visual_capacity <
             detail::controller_vanilla_capacity ||
        options.controller_visual_capacity >
             detail::controller_max_visual_capacity) {
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

    detail::logf(
        LogLevel::info,
        "Compiled menu: pages=%zu rootRows=%zu tabs=%zu modelButtons=%zu modelSubmenus=%zu popupChoices=%zu customTexts=%zu pagination=%d",
        runtime.menu->pages.size(),
        runtime.menu->root_page().rows.size(),
        runtime.menu->tab_page_indices.size(),
        runtime.menu->modeled_button_count,
        runtime.menu->modeled_submenu_count,
        runtime.menu->modeled_popup_choice_count,
        runtime.menu->texts.size(),
        runtime.menu->pagination_required_for_capacity(
            options.controller_visual_capacity) ? 1 : 0);
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

    const bool active_pagination = options.enable_row_injection &&
        runtime.menu->pagination_required_for_capacity(
            options.controller_visual_capacity);
    bool require_buttons = active_pagination;
    bool require_submenus = options.enable_row_injection &&
        runtime.menu->root_plan(options.controller_visual_capacity)
            .pages.slices.size() > 1;
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
                options.enable_custom_text) ||
            (active_pagination && !addresses.native_back)) {
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
                active_pagination,
                runtime.menu->modeled_popup_choice_count != 0,
                options.enable_custom_text)) {
            return fail_install(InstallError::address_resolution_failed);
        }
    }

    const erui::native::HookInstallStatus hook_status =
        erui::native::install_native_menu_hooks(addresses);
    if (hook_status != erui::native::HookInstallStatus::success) {
        return fail_install(InstallError::hook_install_failed);
    }

    detail::logf(LogLevel::info, "ERNativeUI install completed successfully");
    return InstallResult{};
}

InstallResult install(Menu& menu, RuntimeOptions options) noexcept {
    return install_impl(menu, options, nullptr);
}

InstallResult install_with_resolved_addresses(
    Menu& menu,
    RuntimeOptions options,
    const native::GameAddresses& addresses) noexcept {
    return install_impl(menu, options, &addresses);
}

void uninstall() noexcept {
    detail::RuntimeState& runtime = detail::runtime_state();
    if (!runtime.installed.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    erui::native::remove_native_menu_hooks();
    runtime.button_action_hits.store(0, std::memory_order_release);
    runtime.diagnostic_button_action_logs.store(0, std::memory_order_release);
    runtime.button_action_faults.store(0, std::memory_order_release);
    runtime.submenu_open_requests.store(0, std::memory_order_release);
    runtime.submenu_page_hits.store(0, std::memory_order_release);
    runtime.submenu_open_faults.store(0, std::memory_order_release);
    runtime.diagnostic_submenu_logs.store(0, std::memory_order_release);
    runtime.pagination_next_hits.store(0, std::memory_order_release);
    runtime.pagination_previous_hits.store(0, std::memory_order_release);
    runtime.menu.reset();
    detail::set_log_sink(nullptr);
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

    std::size_t changes = 0;
    for (detail::CompiledPage& page : runtime.menu->pages) {
        for (detail::CompiledRow& row : page.rows) {
            if (!row.byte_value) {
                continue;
            }

            std::uint8_t current = *row.byte_value;
            if (row.kind == RowKind::toggle) {
                current = current == 0 ? 0 : 1;
            } else if (row.kind == RowKind::slider) {
                current = normalize_slider(current, row.slider);
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
