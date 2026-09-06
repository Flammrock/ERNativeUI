#include "hooks.hpp"

#include "dialog_input_gate.hpp"
#include "native_dialog.hpp"
#include "native_menu.hpp"
#include "native_popup_choice.hpp"
#include "native_text_input.hpp"
#include "native_text_result.hpp"
#include "native_title_bridge.hpp"
#include "pagination.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"
#include "root_button_text_override.hpp"
#include "submenu_navigation.hpp"
#include "submenu_runtime.hpp"
#include "color_picker.hpp"

#include <Windows.h>
#include <MinHook.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace erui::native {
namespace {

constexpr std::uint32_t kCustomTextFallbackId = 0x1B20A;
constexpr std::uint64_t kNativeBackAction = 3;
constexpr std::uint64_t kSubmenuRequestTimeoutMs = 2000;
constexpr std::uint32_t kSubmenuDiagnosticLogLimit = 192;
constexpr std::uint32_t kSubmenuFaultLogLimit = 32;
constexpr std::uint32_t kTitleFaultLogLimit = 4;

GameAddresses g_addresses{};
SafetyHookInline g_game_options_hook{};
SafetyHookInline g_camera_panel_materializer_hook{};
SafetyHookInline g_display_panel_materializer_hook{};
SafetyHookInline g_sound_panel_materializer_hook{};
SafetyHookInline g_network_panel_materializer_hook{};
SafetyHookInline g_keyboard_mouse_panel_materializer_hook{};
SafetyHookInline g_graphics_panel_materializer_hook{};
std::atomic<BuiltinPanelMaterializerFn> g_original_camera_panel_materializer{};
std::atomic<BuiltinPanelMaterializerFn> g_original_display_panel_materializer{};
std::atomic<BuiltinPanelMaterializerFn> g_original_sound_panel_materializer{};
std::atomic<BuiltinPanelMaterializerFn> g_original_network_panel_materializer{};
std::atomic<BuiltinPanelMaterializerFn>
    g_original_keyboard_mouse_panel_materializer{};
std::atomic<BuiltinPanelMaterializerFn> g_original_graphics_panel_materializer{};
std::atomic<std::uint16_t> g_builtin_panel_warning_mask{};
SafetyHookInline g_sub_hook{};
SafetyHookInline g_page_frame_gate_hook{};
SafetyHookInline g_dialog_back_gate_hook{};
std::atomic<PageFrameFn> g_original_page_frame{};
std::atomic<NativeBackFn> g_original_dialog_back{};
SafetyHookMid g_scaleform_path_resolver_hook{};
std::atomic<GameOptionsHandlerFn> g_original_game_options{nullptr};
std::atomic<SubHandlerFn> g_original_sub{nullptr};
std::atomic<TextResolverFn> g_original_text_resolver{nullptr};
void* g_text_hook_target{};
bool g_text_hook_created{};
bool g_text_hook_enabled{};

std::atomic<void*> g_last_page{};
// Exact physical page currently materialized by ERNativeUI. The native frame
// dispatcher is shared; modal gating every instance also suppresses the
// dialog's own confirmation work.
std::atomic<void*> g_dialog_input_page{};
std::atomic<std::uint64_t> g_last_injection_tick{};
std::atomic<std::uint32_t> g_game_options_entry_count{};
std::atomic<std::uint32_t> g_game_options_injection_count{};
std::atomic<std::uint32_t> g_text_entry_count{};
std::atomic<std::uint32_t> g_text_log_count{};
std::atomic<std::uint32_t> g_text_fault_count{};
std::atomic<std::uint32_t> g_sub_entry_count{};
std::atomic<std::uint32_t> g_sub_injection_count{};
std::atomic<std::uint32_t> g_title_fault_count{};
erui::detail::SubmenuNavigation g_submenu_navigation{};
TitleCaptureState g_title_capture{};

constexpr std::size_t kPanelVisualCapacityOffset = 0xB14;
constexpr std::size_t kPanelNativeCountOffset = 0x1AF0;
constexpr std::uintptr_t kOptionSettingDialogVtableRva = 0x2B14888;

struct BuiltinPanelSnapshot {
    void* vtable{};
    std::uint32_t visual_capacity{};
    std::uint64_t native_count{};
    bool valid{};
};

BuiltinPanelSnapshot inspect_builtin_panel(void* page) noexcept {
    BuiltinPanelSnapshot result{};
    if (!page) return result;
#if defined(_MSC_VER)
    __try {
#endif
        const auto* bytes = static_cast<const std::byte*>(page);
        result.vtable = *reinterpret_cast<void* const*>(bytes);
        result.visual_capacity = *reinterpret_cast<const std::uint32_t*>(
            bytes + kPanelVisualCapacityOffset);
        result.native_count = *reinterpret_cast<const std::uint64_t*>(
            bytes + kPanelNativeCountOffset);
        const std::uintptr_t image =
            reinterpret_cast<std::uintptr_t>(g_addresses.game_image_base);
        const void* expected_vtable = reinterpret_cast<const void*>(
            image + kOptionSettingDialogVtableRva);
        result.valid = image &&
            kOptionSettingDialogVtableRva < g_addresses.game_image_size &&
            result.vtable == expected_vtable &&
            result.visual_capacity >= 1 &&
            result.visual_capacity <=
                erui::detail::builtin_page_max_first_capacity &&
            result.native_count <= result.visual_capacity;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = {};
    }
#endif
    return result;
}

void inject_builtin_panel_rows(
    void* page,
    const char* panel_name,
    std::uint8_t native_category) noexcept {
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.options.enable_row_injection || !runtime.menu) return;
    const std::size_t logical_page_index =
        runtime.menu->builtin_page_index(native_category);
    if (logical_page_index >= runtime.menu->pages.size() ||
        runtime.menu->pages[logical_page_index].rows.empty()) {
        return;
    }

    const BuiltinPanelSnapshot before = inspect_builtin_panel(page);
    g_dialog_input_page.store(page, std::memory_order_release);
    if (!before.valid || before.native_count >= before.visual_capacity) {
        const std::uint16_t bit = static_cast<std::uint16_t>(
            1u << native_category);
        const bool first_warning =
            (g_builtin_panel_warning_mask.fetch_or(
                bit, std::memory_order_relaxed) & bit) == 0;
        if (!first_warning) return;
        erui::detail::logf(
            erui::LogLevel::warning,
            "Built-in %s rows disabled safely: page=%p nativeRows=%llu visualRows=%u valid=%d (the GFX panel has no free row capacity or its layout is unsupported)",
            panel_name,
            page,
            static_cast<unsigned long long>(before.native_count),
            static_cast<unsigned>(before.visual_capacity),
            before.valid ? 1 : 0);
        return;
    }

    const std::uint8_t free_capacity = static_cast<std::uint8_t>(
        before.visual_capacity - before.native_count);
    const erui::detail::PageRoute route =
        erui::detail::PageRoute::builtin_main(
            logical_page_index,
            native_category,
            free_capacity);
    const RowInjectionOutcome outcome = inject_page_route(
        page,
        route,
        g_addresses);
    const BuiltinPanelSnapshot after = inspect_builtin_panel(page);
    const bool committed = after.valid && outcome.faulted == 0 &&
        after.native_count == before.native_count + outcome.added;
    if (!committed || runtime.options.enable_diagnostics) {
        erui::detail::logf(
            committed ? erui::LogLevel::trace : erui::LogLevel::error,
            "built-in %s materializer: page=%p logicalPage=%zu before=%llu after=%llu visualRows=%u freeRows=%u slice=%zu/%zu attempted=%u added=%u faulted=%u committed=%d",
            panel_name,
            page,
            logical_page_index,
            static_cast<unsigned long long>(before.native_count),
            static_cast<unsigned long long>(after.native_count),
            static_cast<unsigned>(before.visual_capacity),
            static_cast<unsigned>(free_capacity),
            outcome.slice_index + 1,
            outcome.slice_count,
            static_cast<unsigned>(outcome.attempted),
            static_cast<unsigned>(outcome.added),
            static_cast<unsigned>(outcome.faulted),
            committed ? 1 : 0);
    }
}

template <typename Function>
Function wait_for_panel_materializer(
    const std::atomic<Function>& slot) noexcept {
    Function original = slot.load(std::memory_order_acquire);
    for (unsigned attempt = 0; !original && attempt < 64; ++attempt) {
        Sleep(0);
        original = slot.load(std::memory_order_acquire);
    }
    return original;
}

void __fastcall camera_panel_materializer_detour(
    void* page,
    void* menu_option_data) noexcept {
    BuiltinPanelMaterializerFn original = wait_for_panel_materializer(
        g_original_camera_panel_materializer);
    if (!original) return;
    original(page, menu_option_data);
    inject_builtin_panel_rows(page, "Camera Options", 1);
}

void __fastcall display_panel_materializer_detour(
    void* page,
    void* menu_option_data) noexcept {
    BuiltinPanelMaterializerFn original = wait_for_panel_materializer(
        g_original_display_panel_materializer);
    if (!original) return;
    original(page, menu_option_data);
    inject_builtin_panel_rows(page, "Display", 2);
}

void __fastcall sound_panel_materializer_detour(
    void* page,
    void* menu_option_data) noexcept {
    BuiltinPanelMaterializerFn original = wait_for_panel_materializer(
        g_original_sound_panel_materializer);
    if (!original) return;
    original(page, menu_option_data);
    inject_builtin_panel_rows(page, "Sound", 3);
}

void __fastcall network_panel_materializer_detour(
    void* page,
    void* menu_option_data) noexcept {
    BuiltinPanelMaterializerFn original = wait_for_panel_materializer(
        g_original_network_panel_materializer);
    if (!original) return;
    original(page, menu_option_data);
    inject_builtin_panel_rows(page, "Network", 5);
}

void __fastcall keyboard_mouse_panel_materializer_detour(
    void* page,
    void* menu_option_data) noexcept {
    BuiltinPanelMaterializerFn original = wait_for_panel_materializer(
        g_original_keyboard_mouse_panel_materializer);
    if (!original) return;
    original(page, menu_option_data);
    inject_builtin_panel_rows(page, "Keyboard/Mouse", 7);
}

void __fastcall graphics_panel_materializer_detour(
    void* page,
    void* menu_option_data) noexcept {
    BuiltinPanelMaterializerFn original = wait_for_panel_materializer(
        g_original_graphics_panel_materializer);
    if (!original) return;
    original(page, menu_option_data);
    inject_builtin_panel_rows(page, "Graphics", 8);
}

bool copy_native_title_source_path(
    const char* source,
    char (&destination)[sizeof(native_menu_title_source_path)]) noexcept {
    if (!source) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0; index < sizeof(destination); ++index) {
            destination[index] = source[index];
        }
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

void observe_scaleform_path(safetyhook::Context& context) noexcept {
    if (context.r8 < 0x10000) {
        return;
    }

    char observed_path[sizeof(native_menu_title_source_path)]{};
    if (!copy_native_title_source_path(
            reinterpret_cast<const char*>(context.r8), observed_path)) {
        return;
    }

    // The native action producer hard-codes Widgets/Button. During one
    // matched color-row construction, bind that same native controller to
    // the standalone sibling instead. The TLS source check prevents another
    // row, thread, or unrelated resolver call from being redirected.
    if (const char* replacement = color_picker_widget_path_override(
            observed_path,
            reinterpret_cast<const void*>(context.rcx))) {
        context.r8 = reinterpret_cast<std::uintptr_t>(replacement);
        return;
    }

    // The native page constructor resolves MenuTitle/Text_0 into its own
    // persistent 0x60-byte result. Redirect only that exact construction call
    // while a custom physical route is pending. The page remains responsible
    // for the result's lifetime; we retain only non-owning pointers until the
    // immediately following subpage-handler callback.
    const char* replacement = g_title_capture.observe(
        observed_path,
        g_submenu_navigation.pending_route(),
        reinterpret_cast<void*>(context.rcx),
        reinterpret_cast<void*>(context.rdx));
    if (replacement) {
        context.r8 = reinterpret_cast<std::uintptr_t>(replacement);
    }
}

bool invoke_scaleform_text_setter(
    ScaleformTextSetterFn function,
    void* value,
    const wchar_t* text,
    unsigned long& seh_exception_code) noexcept {
    seh_exception_code = 0;
    if (!function || !value || !text || !*text) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        function(value, text);
        return true;
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    function(value, text);
    return true;
#endif
}

bool invoke_scaleform_path_resolver(
    ScaleformPathResolverFn function,
    void* movie_context,
    void* destination,
    const char* path,
    unsigned long& seh_exception_code) noexcept {
    seh_exception_code = 0;
    if (!function || !movie_context || !destination || !path || !*path) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        return function(movie_context, destination, path) == destination;
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    return function(movie_context, destination, path) == destination;
#endif
}

bool invoke_scaleform_result_destructor(
    ScaleformResultDestructorFn function,
    void* nested_member,
    unsigned long& seh_exception_code) noexcept {
    seh_exception_code = 0;
    if (!function || !nested_member) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        function(nested_member);
        return true;
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    function(nested_member);
    return true;
#endif
}

void log_title_fault(const char* operation, unsigned long exception_code) noexcept {
    const std::uint32_t fault =
        g_title_fault_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (fault <= kTitleFaultLogLimit) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Scaleform title bridge %s failed (fault=%u seh=0x%08lX); menu rows remain active",
            operation,
            static_cast<unsigned>(fault),
            exception_code);
    }
}

void apply_physical_page_titles(
    const CapturedTitleTarget& captured,
    const wchar_t* outer_title,
    const wchar_t* page_title) noexcept {
    if (!captured.valid() || !g_addresses.title_bridge_complete()) {
        return;
    }

    // Preserve the page-owned persistent result for the inner heading. The
    // outer heading is a one-shot assignment through a temporary native result
    // whose construction and cleanup mirror Elden Ring's own call sites.
    if (outer_title && *outer_title) {
        alignas(16) std::array<std::byte, scaleform_path_result_size> temporary{};
        unsigned long exception_code = 0;
        const bool resolved = invoke_scaleform_path_resolver(
            g_addresses.scaleform_path_resolver,
            captured.movie_context,
            temporary.data(),
            custom_outer_title_path,
            exception_code);
        if (!resolved) {
            log_title_fault("outer-path resolution", exception_code);
        } else {
            if (!invoke_scaleform_text_setter(
                    g_addresses.scaleform_text_setter,
                    temporary.data() + scaleform_text_value_offset,
                    outer_title,
                    exception_code)) {
                log_title_fault("outer-title assignment", exception_code);
            }
            if (!invoke_scaleform_result_destructor(
                    g_addresses.scaleform_result_destructor,
                    temporary.data() + scaleform_destructor_member_offset,
                    exception_code)) {
                log_title_fault("temporary-result cleanup", exception_code);
            }
        }
    }

    if (page_title && *page_title) {
        unsigned long exception_code = 0;
        if (!invoke_scaleform_text_setter(
                g_addresses.scaleform_text_setter,
                captured.text_value(),
                page_title,
                exception_code)) {
            log_title_fault("page-title assignment", exception_code);
        }
    }
}

const char* minhook_status_name(MH_STATUS status) noexcept {
    const char* name = MH_StatusToString(status);
    return name ? name : "MH_UNKNOWN";
}

const char* route_kind_name(erui::detail::PageRouteKind kind) noexcept {
    switch (kind) {
    case erui::detail::PageRouteKind::submenu:
        return "submenu";
    case erui::detail::PageRouteKind::root_continuation:
        return "root-continuation";
    case erui::detail::PageRouteKind::root_main:
        return "root-main";
    case erui::detail::PageRouteKind::builtin_continuation:
        return "built-in-continuation";
    case erui::detail::PageRouteKind::builtin_main:
        return "built-in-main";
    }
    return "unknown";
}

const char* request_kind_name(NavigationRequestKind kind) noexcept {
    switch (kind) {
    case NavigationRequestKind::submenu:
        return "submenu";
    case NavigationRequestKind::pagination_next:
        return "next";
    case NavigationRequestKind::pagination_previous:
        return "previous";
    }
    return "unknown";
}

void* __fastcall text_resolver_detour(
    void* text_result,
    std::uintptr_t argument2,
    std::uint32_t message_id,
    std::uintptr_t argument4,
    std::uintptr_t argument5) noexcept {
    TextResolverFn original = g_original_text_resolver.load(std::memory_order_acquire);
    if (!original) {
        // MH_CreateHook publishes the trampoline before the queued hook is
        // enabled. This branch is defensive and should never be reached.
        return nullptr;
    }

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const std::uint32_t entry =
        g_text_entry_count.fetch_add(1, std::memory_order_relaxed) + 1;

    std::uint32_t lookup_id = message_id;
    if (root_button_text_override.active) {
        if (message_id == root_button_native_label_id) {
            lookup_id = root_button_text_override.label_id;
        } else if (message_id == root_button_native_help_id) {
            lookup_id = root_button_text_override.help_id;
        }
    }

    const wchar_t* replacement = runtime.options.enable_custom_text && runtime.menu
        ? runtime.menu->texts.lookup(static_cast<int>(lookup_id))
        : nullptr;
    if (!replacement && runtime.options.enable_custom_text) {
        replacement = lookup_native_dialog_text(message_id);
    }

    if (!replacement) {
        void* result = original(
            text_result,
            argument2,
            message_id,
            argument4,
            argument5);

        if (erui::detail::message_trace_logging() && entry <= 8) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "textResolver[%u] passthrough resultObject=%p a2=0x%llX id=%u/0x%X a4=0x%llX a5=0x%llX return=%p customText=%u",
                static_cast<unsigned>(entry),
                text_result,
                static_cast<unsigned long long>(argument2),
                static_cast<unsigned>(message_id),
                static_cast<unsigned>(message_id),
                static_cast<unsigned long long>(argument4),
                static_cast<unsigned long long>(argument5),
                result,
                runtime.options.enable_custom_text ? 1u : 0u);
        }
        return result;
    }

    // Match the native-UI technique observed in Solid Uncapper: ask Elden
    // Ring to build a valid result object using a known vanilla message, then
    // replace the result object's UTF-16 pointer/storage/length with our
    // stable registry string.
    void* result = original(
        text_result,
        argument2,
        kCustomTextFallbackId,
        argument4,
        argument5);

    const erui::detail::NativeTextPatchOutcome patch =
        erui::detail::patch_native_text_result(text_result, replacement);
    const std::uint64_t hit =
        runtime.custom_text_hits.fetch_add(1, std::memory_order_relaxed) + 1;

    if (patch.status == erui::detail::NativeTextPatchStatus::access_violation) {
        const std::uint32_t fault =
            g_text_fault_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (fault <= 16) {
            erui::detail::logf(
                erui::LogLevel::error,
                "custom text patch fault=%u hit=%llu id=0x%X resultObject=%p return=%p",
                static_cast<unsigned>(fault),
                static_cast<unsigned long long>(hit),
                static_cast<unsigned>(message_id),
                text_result,
                result);
        }
    }

    if (erui::detail::message_trace_logging()) {
        const std::uint32_t logged =
            g_text_log_count.fetch_add(1, std::memory_order_relaxed);
        if (logged < 128) {
            const char* mode = "invalid";
            switch (patch.status) {
            case erui::detail::NativeTextPatchStatus::direct_pointer_only:
                mode = "direct";
                break;
            case erui::detail::NativeTextPatchStatus::direct_pointer_and_buffer:
                mode = "direct+buffer";
                break;
            case erui::detail::NativeTextPatchStatus::access_violation:
                mode = "fault";
                break;
            case erui::detail::NativeTextPatchStatus::invalid_argument:
                break;
            }
            erui::detail::logf(
                erui::LogLevel::trace,
                "custom text hit=%llu id=0x%X object=%p return=%p patch=%s length=%zu capacity=%llu",
                static_cast<unsigned long long>(hit),
                static_cast<unsigned>(message_id),
                text_result,
                result,
                mode,
                patch.text_length,
                static_cast<unsigned long long>(patch.native_capacity));
        }
    }

    return result;
}

void __fastcall game_options_handler_detour(
    void* page,
    std::uintptr_t argument2,
    std::uintptr_t argument3,
    std::uintptr_t argument4,
    std::uintptr_t argument5,
    std::uintptr_t argument6) noexcept {
    GameOptionsHandlerFn original =
        g_original_game_options.load(std::memory_order_acquire);
    for (unsigned attempt = 0; !original && attempt < 64; ++attempt) {
        Sleep(0);
        original = g_original_game_options.load(std::memory_order_acquire);
    }
    if (!original) {
        return;
    }
    original(page, argument2, argument3, argument4, argument5, argument6);

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const std::uint32_t entry =
        g_game_options_entry_count.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool trace_this_call = erui::detail::trace_logging() && entry <= 64;
    if (trace_this_call) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "gameOptions[%u] original returned page=%p a2=0x%llX a3=0x%llX a4=0x%llX a5=0x%llX a6=0x%llX",
            static_cast<unsigned>(entry),
            page,
            static_cast<unsigned long long>(argument2),
            static_cast<unsigned long long>(argument3),
            static_cast<unsigned long long>(argument4),
            static_cast<unsigned long long>(argument5),
            static_cast<unsigned long long>(argument6));
    }

    if (!runtime.options.enable_row_injection || !runtime.menu || !page) {
        return;
    }

    // The shared root is a live native page too. Subpages publish this pointer
    // in sub_handler_detour, but a dialog opened directly from the initial
    // root never visits that path. Publish before the injection cooldown so
    // page-frame and Back gates can match root-origin alerts as well.
    g_dialog_input_page.store(page, std::memory_order_release);

    const std::uint64_t now = GetTickCount64();
    const void* previous_page = g_last_page.load(std::memory_order_relaxed);
    const std::uint64_t previous_tick =
        g_last_injection_tick.load(std::memory_order_relaxed);
    const std::uint64_t cooldown = runtime.options.injection_cooldown_ms;
    if (previous_page == page && now >= previous_tick && now - previous_tick < cooldown) {
        if (trace_this_call) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "gameOptions[%u] row injection skipped by cooldown",
                static_cast<unsigned>(entry));
        }
        return;
    }

    g_last_page.store(page, std::memory_order_relaxed);
    g_last_injection_tick.store(now, std::memory_order_relaxed);

    const std::uint32_t injection =
        g_game_options_injection_count.fetch_add(
            1,
            std::memory_order_relaxed) + 1;
    const RowInjectionOutcome outcome = inject_registered_rows(page, g_addresses);
    const std::uint32_t log_limit = erui::detail::diagnostics_enabled() ? 128u : 4u;
    if (injection <= log_limit || outcome.faulted != 0) {
        erui::detail::logf(
            outcome.faulted == 0 ? erui::LogLevel::info : erui::LogLevel::error,
            "injection[%u] page=%p route=%s logicalPage=%zu slice=%zu/%zu capacity=%u attempted=%u added=%u navigation=%u faulted=%u skipped=%u",
            static_cast<unsigned>(injection),
            page,
            route_kind_name(outcome.route.kind),
            outcome.route.logical_page_index,
            outcome.slice_index + 1,
            outcome.slice_count,
            static_cast<unsigned>(outcome.root_capacity),
            static_cast<unsigned>(outcome.attempted),
            static_cast<unsigned>(outcome.added),
            static_cast<unsigned>(outcome.navigation),
            static_cast<unsigned>(outcome.faulted),
            static_cast<unsigned>(outcome.skipped));
    }
}

const char* submenu_resolution_name(
    erui::detail::SubmenuResolutionKind kind) noexcept {
    switch (kind) {
    case erui::detail::SubmenuResolutionKind::none:
        return "none";
    case erui::detail::SubmenuResolutionKind::pending_request:
        return "pending";
    case erui::detail::SubmenuResolutionKind::bound_page:
        return "bound";
    case erui::detail::SubmenuResolutionKind::expired_request:
        return "expired";
    }
    return "unknown";
}

void __fastcall sub_handler_detour(
    void* page,
    std::uintptr_t argument2,
    std::uintptr_t argument3,
    std::uintptr_t argument4) noexcept {
    SubHandlerFn original = g_original_sub.load(std::memory_order_acquire);
    for (unsigned attempt = 0; !original && attempt < 64; ++attempt) {
        Sleep(0);
        original = g_original_sub.load(std::memory_order_acquire);
    }
    if (!original) {
        return;
    }

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const std::uint32_t entry =
        g_sub_entry_count.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::uint64_t now = GetTickCount64();
    const erui::detail::SubmenuResolution resolution =
        g_submenu_navigation.resolve(page, now, kSubmenuRequestTimeoutMs);

    if (resolution.kind == erui::detail::SubmenuResolutionKind::expired_request) {
        g_title_capture.reset();
        if (runtime.options.enable_diagnostics && entry <= kSubmenuDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "diagnostic: subHandler[%u] expired pending route before page=%p; forwarding original",
                static_cast<unsigned>(entry),
                page);
        }
        reset_color_picker_page_widgets(
            page, ColorPickerWidgetHost::subpage);
        original(page, argument2, argument3, argument4);
        return;
    }

    const erui::detail::PageRoute route = resolution.route;
    const CapturedTitleTarget captured_title =
        resolution.kind == erui::detail::SubmenuResolutionKind::pending_request
        ? g_title_capture.consume(route)
        : (g_title_capture.reset(), CapturedTitleTarget{});
    erui::detail::PageSlice resolved_slice{};
    const bool slice_resolved = runtime.menu &&
        resolve_page_route_slice(*runtime.menu, route, resolved_slice);
    if (resolution.kind == erui::detail::SubmenuResolutionKind::none ||
        !runtime.options.enable_row_injection || !runtime.menu ||
        !slice_resolved) {
        if (runtime.options.enable_diagnostics && entry <= 32) {
            const erui::detail::PageRoute pending =
                g_submenu_navigation.pending_route();
            erui::detail::logf(
                erui::LogLevel::trace,
                "diagnostic: subHandler[%u] vanilla page=%p pendingKind=%s pendingLogical=%zu pendingSlice=%zu; forwarding original",
                static_cast<unsigned>(entry),
                page,
                route_kind_name(pending.kind),
                pending.logical_page_index,
                pending.slice_index);
        }
        reset_color_picker_page_widgets(
            page, ColorPickerWidgetHost::subpage);
        original(page, argument2, argument3, argument4);
        return;
    }

    g_dialog_input_page.store(page, std::memory_order_release);

    const erui::detail::CompiledPage& compiled_page =
        runtime.menu->pages[route.logical_page_index];
    const bool route_matches_page =
        (route.kind == erui::detail::PageRouteKind::submenu &&
            compiled_page.kind == erui::PageKind::submenu) ||
        (route.kind == erui::detail::PageRouteKind::root_continuation &&
            route.logical_page_index == runtime.menu->root_page_index) ||
        (route.kind ==
                erui::detail::PageRouteKind::builtin_continuation &&
            runtime.menu->builtin_page_index(route.builtin_category) ==
                route.logical_page_index);
    if (!route_matches_page) {
        erui::detail::logf(
            erui::LogLevel::error,
            "subHandler[%u] route kind=%s logicalPage=%zu does not match compiled page kind; forwarding original",
            static_cast<unsigned>(entry),
            route_kind_name(route.kind),
            route.logical_page_index);
        reset_color_picker_page_widgets(
            page, ColorPickerWidgetHost::subpage);
        original(page, argument2, argument3, argument4);
        return;
    }

    const std::uint64_t hit =
        runtime.submenu_page_hits.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_submenu_navigation.should_inject(
            route,
            page,
            now,
            runtime.options.injection_cooldown_ms)) {
        if (runtime.options.enable_diagnostics && hit <= kSubmenuDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "diagnostic: custom subHandler[%llu] page=%p route=%s logicalPage=%zu slice=%zu skipped by cooldown",
                static_cast<unsigned long long>(hit),
                page,
                route_kind_name(route.kind),
                route.logical_page_index,
                route.slice_index);
        }
        return;
    }

    const std::uint32_t injection =
        g_sub_injection_count.fetch_add(1, std::memory_order_relaxed) + 1;
    const RowInjectionOutcome outcome = inject_page_route(page, route, g_addresses);
    const erui::detail::ResolvedPagePresentation presentation =
        runtime.menu->resolve_presentation(route);
    const wchar_t* const outer_title = runtime.menu->texts.lookup(
        static_cast<int>(presentation.outer_title_id));
    const wchar_t* const page_title = runtime.menu->texts.lookup(
        static_cast<int>(presentation.page_title_id));
    apply_physical_page_titles(captured_title, outer_title, page_title);
    const std::uint32_t log_limit = runtime.options.enable_diagnostics ? 192u : 8u;
    if (injection <= log_limit || outcome.faulted != 0) {
        erui::detail::logf(
            outcome.faulted == 0 ? erui::LogLevel::info : erui::LogLevel::error,
            "physicalPageMaterialize[%u] page=%p source=%s route=%s logicalPage=%zu slice=%zu/%zu firstRow=%zu contentRows=%zu attempted=%u added=%u navigation=%u faulted=%u skipped=%u",
            static_cast<unsigned>(injection),
            page,
            submenu_resolution_name(resolution.kind),
            route_kind_name(route.kind),
            route.logical_page_index,
            outcome.slice_index + 1,
            outcome.slice_count,
            outcome.first_row,
            outcome.content_rows,
            static_cast<unsigned>(outcome.attempted),
            static_cast<unsigned>(outcome.added),
            static_cast<unsigned>(outcome.navigation),
            static_cast<unsigned>(outcome.faulted),
            static_cast<unsigned>(outcome.skipped));
    }
}

void __fastcall page_frame_gate_detour(
    void* page,
    float frame_value,
    std::uint8_t* input_enabled) noexcept {
    // TextInput setters may run on the host/client thread. Publish their
    // canonical values into the native MenuString shadow only from this UI
    // frame boundary, including while a native alert owns page input.
    synchronize_pending_text_inputs();
    synchronize_color_picker_previews(page);

    PageFrameFn original = g_original_page_frame.load(std::memory_order_acquire);
    if (!original) return;
    if (native_dialog_owns_menu_input() &&
        page == g_dialog_input_page.load(std::memory_order_acquire)) {
        return;
    }
    original(page, frame_value, input_enabled);
}

bool install_page_frame_gate(const GameAddresses& addresses) noexcept {
    if (!addresses.page_frame) return false;
    if (g_original_page_frame.load(std::memory_order_acquire)) {
        return true;
    }
    auto result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.page_frame),
        reinterpret_cast<void*>(&page_frame_gate_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        erui::detail::logf(erui::LogLevel::warning,
            "Page frame gate could not be prepared (SafetyHook error type %u)",
            static_cast<unsigned>(result.error().type));
        return false;
    }
    g_page_frame_gate_hook = std::move(*result);
    g_original_page_frame.store(
        g_page_frame_gate_hook.original<PageFrameFn>(), std::memory_order_release);
    if (auto enabled = g_page_frame_gate_hook.enable(); !enabled) {
        erui::detail::logf(erui::LogLevel::warning,
            "Page frame gate could not be enabled (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_page_frame_gate_hook.reset();
        g_original_page_frame.store(nullptr, std::memory_order_release);
        return false;
    }
    erui::detail::logf(erui::LogLevel::info,
        "Page-frame UI pump/input gate installed");
    return true;
}

void __fastcall dialog_back_gate_detour(
    void* page,
    std::uint64_t action) noexcept {
    NativeBackFn original = g_original_dialog_back.load(std::memory_order_acquire);
    if (!original) return;
    // Action 3 is both menu Back and the right/secondary action of a native
    // two-button dialog. The popup action is emitted synchronously from the
    // owned CSPopupMenu update; suppress action 3 everywhere else so it cannot
    // navigate the page underneath the dialog.
    const bool owns_dialog = native_dialog_owns_menu_input();
    const bool popup_scope = native_dialog_dispatching_popup_input();
    const bool suppress = should_suppress_native_back(
        owns_dialog, popup_scope, action);
    if (suppress) {
        return;
    }
    void* const restored_parent = action == kNativeBackAction
        ? g_submenu_navigation.parent_page(page)
        : nullptr;
    original(page, action);
    if (restored_parent) {
        g_submenu_navigation.unbind_native_page(page);
        g_dialog_input_page.store(restored_parent, std::memory_order_release);
    }
}

bool install_dialog_back_gate(const GameAddresses& addresses) noexcept {
    if (!addresses.native_back) return false;
    auto result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.native_back),
        reinterpret_cast<void*>(&dialog_back_gate_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        erui::detail::logf(erui::LogLevel::warning,
            "Dialog Back gate could not be prepared (SafetyHook error type %u)",
            static_cast<unsigned>(result.error().type));
        return false;
    }
    g_dialog_back_gate_hook = std::move(*result);
    g_original_dialog_back.store(
        g_dialog_back_gate_hook.original<NativeBackFn>(), std::memory_order_release);
    if (auto enabled = g_dialog_back_gate_hook.enable(); !enabled) {
        erui::detail::logf(erui::LogLevel::warning,
            "Dialog Back gate could not be enabled (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_dialog_back_gate_hook.reset();
        g_original_dialog_back.store(nullptr, std::memory_order_release);
        return false;
    }
    erui::detail::logf(erui::LogLevel::info,
        "Dialog native Back input gate installed");
    return true;
}

void reset_dialog_back_gate() noexcept {
    g_dialog_back_gate_hook.reset();
    g_original_dialog_back.store(nullptr, std::memory_order_release);
}

void reset_page_frame_gate() noexcept {
    g_page_frame_gate_hook.reset();
    g_original_page_frame.store(nullptr, std::memory_order_release);
}

void reset_dialog_input_gates(bool preserve_page_frame) noexcept {
    reset_dialog_back_gate();
    if (!preserve_page_frame) {
        reset_page_frame_gate();
    }
}

bool install_dialog_input_gates(
    const GameAddresses& addresses,
    bool preserve_page_frame) noexcept {
    reset_dialog_back_gate();
    if (!addresses.page_frame || !addresses.native_back) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert input ownership unavailable: page-frame or Back interface unresolved");
        if (!preserve_page_frame) {
            reset_page_frame_gate();
        }
        return false;
    }
    if (!install_page_frame_gate(addresses) ||
        !install_dialog_back_gate(addresses)) {
        reset_dialog_input_gates(preserve_page_frame);
        return false;
    }
    return true;
}

bool install_game_options_hook(const GameAddresses& addresses) noexcept {
    if (erui::detail::diagnostics_enabled()) {
        const auto* code = reinterpret_cast<const unsigned char*>(
            addresses.game_options_handler);
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: Game Options entry before hook=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7],
            code[8], code[9], code[10], code[11], code[12], code[13], code[14], code[15]);
    }
    auto game_options_result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.game_options_handler),
        reinterpret_cast<void*>(&game_options_handler_detour),
        SafetyHookInline::StartDisabled);
    if (!game_options_result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "SafetyHook failed to prepare Game Options handler hook (error type %u)",
            static_cast<unsigned>(game_options_result.error().type));
        return false;
    }

    g_game_options_hook = std::move(*game_options_result);
    g_original_game_options.store(
        g_game_options_hook.original<GameOptionsHandlerFn>(),
        std::memory_order_release);
    if (auto enable_result = g_game_options_hook.enable(); !enable_result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "SafetyHook failed to enable Game Options handler hook (error type %u)",
            static_cast<unsigned>(enable_result.error().type));
        return false;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Hook installed: Game Options handler (six forwarded ABI slots)");
    if (erui::detail::diagnostics_enabled()) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: Game Options hook target=%p detour=%p trampoline=%p",
            reinterpret_cast<void*>(addresses.game_options_handler),
            reinterpret_cast<void*>(&game_options_handler_detour),
            reinterpret_cast<void*>(
                g_original_game_options.load(std::memory_order_acquire)));
    }
    return true;
}

template <typename Function>
bool install_panel_materializer_hook(
    const char* panel_name,
    std::uint8_t native_category,
    Function target,
    void* detour,
    SafetyHookInline& hook,
    std::atomic<Function>& original) noexcept {
    const erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const erui::detail::CompiledPage* const page = runtime.menu
        ? runtime.menu->builtin_page(native_category)
        : nullptr;
    if (!page || page->rows.empty()) return true;

    if (!target) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Built-in %s rows disabled: native row materializer was not resolved",
            panel_name);
        return false;
    }

    auto result = SafetyHookInline::create(
        reinterpret_cast<void*>(target),
        detour,
        SafetyHookInline::StartDisabled);
    if (!result) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Built-in %s materializer hook creation failed (SafetyHook error type %u)",
            panel_name,
            static_cast<unsigned>(result.error().type));
        return false;
    }

    hook = std::move(*result);
    original.store(hook.original<Function>(), std::memory_order_release);
    if (auto enabled = hook.enable(); !enabled) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Built-in %s materializer hook enable failed (SafetyHook error type %u)",
            panel_name,
            static_cast<unsigned>(enabled.error().type));
        hook.reset();
        original.store(nullptr, std::memory_order_release);
        return false;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Hook installed: built-in %s row materializer (native category %u, logical rows=%zu, handler=%p)",
        panel_name,
        static_cast<unsigned>(native_category),
        page->rows.size(),
        reinterpret_cast<void*>(target));
    return true;
}

void install_builtin_panel_materializers(
    const GameAddresses& addresses) noexcept {
    // Every target is optional and isolated. One incompatible third-party
    // detour or game-update signature disables only that native destination.
    (void)install_panel_materializer_hook(
        "Camera Options",
        1,
        addresses.camera_panel_materializer,
        reinterpret_cast<void*>(&camera_panel_materializer_detour),
        g_camera_panel_materializer_hook,
        g_original_camera_panel_materializer);
    (void)install_panel_materializer_hook(
        "Display",
        2,
        addresses.display_panel_materializer,
        reinterpret_cast<void*>(&display_panel_materializer_detour),
        g_display_panel_materializer_hook,
        g_original_display_panel_materializer);
    (void)install_panel_materializer_hook(
        "Sound",
        3,
        addresses.sound_panel_materializer,
        reinterpret_cast<void*>(&sound_panel_materializer_detour),
        g_sound_panel_materializer_hook,
        g_original_sound_panel_materializer);
    (void)install_panel_materializer_hook(
        "Network",
        5,
        addresses.network_panel_materializer,
        reinterpret_cast<void*>(&network_panel_materializer_detour),
        g_network_panel_materializer_hook,
        g_original_network_panel_materializer);
    (void)install_panel_materializer_hook(
        "Keyboard/Mouse",
        7,
        addresses.keyboard_mouse_panel_materializer,
        reinterpret_cast<void*>(&keyboard_mouse_panel_materializer_detour),
        g_keyboard_mouse_panel_materializer_hook,
        g_original_keyboard_mouse_panel_materializer);
    (void)install_panel_materializer_hook(
        "Graphics",
        8,
        addresses.graphics_panel_materializer,
        reinterpret_cast<void*>(&graphics_panel_materializer_detour),
        g_graphics_panel_materializer_hook,
        g_original_graphics_panel_materializer);
}

bool install_sub_hook(const GameAddresses& addresses) noexcept {
    if (erui::detail::diagnostics_enabled()) {
        const auto* code = reinterpret_cast<const unsigned char*>(addresses.sub_handler);
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: subpage entry before hook=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7],
            code[8], code[9], code[10], code[11], code[12], code[13], code[14], code[15]);
    }
    auto sub_result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.sub_handler),
        reinterpret_cast<void*>(&sub_handler_detour),
        SafetyHookInline::StartDisabled);
    if (!sub_result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "SafetyHook failed to prepare Game Options subpage handler hook (error type %u)",
            static_cast<unsigned>(sub_result.error().type));
        return false;
    }

    g_sub_hook = std::move(*sub_result);
    g_original_sub.store(g_sub_hook.original<SubHandlerFn>(), std::memory_order_release);
    if (auto enable_result = g_sub_hook.enable(); !enable_result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "SafetyHook failed to enable Game Options subpage handler hook (error type %u)",
            static_cast<unsigned>(enable_result.error().type));
        return false;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Hook installed: Game Options subpage handler (four forwarded ABI slots)");
    if (erui::detail::diagnostics_enabled()) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: subpage hook target=%p detour=%p trampoline=%p openSubPage=%p",
            reinterpret_cast<void*>(addresses.sub_handler),
            reinterpret_cast<void*>(&sub_handler_detour),
            reinterpret_cast<void*>(g_original_sub.load(std::memory_order_acquire)),
            reinterpret_cast<void*>(addresses.open_sub_page));
    }
    return true;
}

bool invoke_open_sub_page(
    OpenSubPageFn function,
    void** parent_page_slot,
    unsigned long& seh_exception_code) noexcept {
    seh_exception_code = 0;
    if (!function || !parent_page_slot) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        function(parent_page_slot);
        return true;
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    function(parent_page_slot);
    return true;
#endif
}

bool invoke_native_back(
    NativeBackFn function,
    void* page,
    unsigned long& seh_exception_code) noexcept {
    seh_exception_code = 0;
    if (!function || !page) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        // The concrete page's real Back path constructs the packed action
        // {kind=3, flags=0}. Use that exact payload for synthetic Previous.
        function(page, kNativeBackAction);
        return true;
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    function(page, kNativeBackAction);
    return true;
#endif
}

enum class TextHookInstallStatus : std::uint8_t {
    success,
    hook_failed,
};

TextHookInstallStatus install_text_hook(const GameAddresses& addresses) noexcept {
    const MH_STATUS initialize_status = MH_Initialize();
    if (initialize_status != MH_OK && initialize_status != MH_ERROR_ALREADY_INITIALIZED) {
        erui::detail::logf(
            erui::LogLevel::error,
            "MinHook initialization failed: %s (%d)",
            minhook_status_name(initialize_status),
            static_cast<int>(initialize_status));
        return TextHookInstallStatus::hook_failed;
    }

    g_text_hook_target = reinterpret_cast<void*>(addresses.text_resolver);
    void* trampoline = nullptr;
    const MH_STATUS create_status = MH_CreateHook(
        g_text_hook_target,
        reinterpret_cast<void*>(&text_resolver_detour),
        &trampoline);
    if (create_status != MH_OK) {
        erui::detail::logf(
            erui::LogLevel::error,
            "MinHook failed to create native text-resolver hook: %s (%d)",
            minhook_status_name(create_status),
            static_cast<int>(create_status));
        g_text_hook_target = nullptr;
        return TextHookInstallStatus::hook_failed;
    }

    g_text_hook_created = true;
    g_original_text_resolver.store(
        reinterpret_cast<TextResolverFn>(trampoline),
        std::memory_order_release);

    const MH_STATUS queue_status = MH_QueueEnableHook(g_text_hook_target);
    if (queue_status != MH_OK) {
        erui::detail::logf(
            erui::LogLevel::error,
            "MinHook failed to queue native text-resolver hook: %s (%d)",
            minhook_status_name(queue_status),
            static_cast<int>(queue_status));
        return TextHookInstallStatus::hook_failed;
    }

    const MH_STATUS apply_status = MH_ApplyQueued();
    if (apply_status != MH_OK) {
        erui::detail::logf(
            erui::LogLevel::error,
            "MinHook failed to enable native text-resolver hook: %s (%d)",
            minhook_status_name(apply_status),
            static_cast<int>(apply_status));
        return TextHookInstallStatus::hook_failed;
    }

    g_text_hook_enabled = true;
    erui::detail::logf(
        erui::LogLevel::info,
        "Hook installed: native UI text resolver via MinHook (custom IDs enabled, five-argument result-object ABI)");
    if (erui::detail::diagnostics_enabled()) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: text hook target=%p detour=%p trampoline=%p",
            g_text_hook_target,
            reinterpret_cast<void*>(&text_resolver_detour),
            trampoline);
    }
    return TextHookInstallStatus::success;
}

void reset_hooks() noexcept {
    set_color_picker_widget_path_bridge_available(false);
    g_scaleform_path_resolver_hook.reset();
    reset_dialog_input_gates(false);
    remove_native_popup_choice_bridge();
    g_sub_hook.reset();
    g_graphics_panel_materializer_hook.reset();
    g_original_graphics_panel_materializer.store(
        nullptr,
        std::memory_order_release);
    g_keyboard_mouse_panel_materializer_hook.reset();
    g_original_keyboard_mouse_panel_materializer.store(
        nullptr,
        std::memory_order_release);
    g_network_panel_materializer_hook.reset();
    g_original_network_panel_materializer.store(
        nullptr,
        std::memory_order_release);
    g_sound_panel_materializer_hook.reset();
    g_original_sound_panel_materializer.store(
        nullptr,
        std::memory_order_release);
    g_display_panel_materializer_hook.reset();
    g_original_display_panel_materializer.store(
        nullptr,
        std::memory_order_release);
    g_camera_panel_materializer_hook.reset();
    g_original_camera_panel_materializer.store(
        nullptr,
        std::memory_order_release);
    g_builtin_panel_warning_mask.store(0, std::memory_order_release);
    g_game_options_hook.reset();

    if (g_text_hook_target && g_text_hook_enabled) {
        const MH_STATUS disable_status = MH_DisableHook(g_text_hook_target);
        if (disable_status != MH_OK && disable_status != MH_ERROR_DISABLED) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "MinHook could not disable native UI text resolver during cleanup: %s (%d)",
                minhook_status_name(disable_status),
                static_cast<int>(disable_status));
        }
    }
    if (g_text_hook_target && g_text_hook_created) {
        const MH_STATUS remove_status = MH_RemoveHook(g_text_hook_target);
        if (remove_status != MH_OK && remove_status != MH_ERROR_NOT_CREATED) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "MinHook could not remove native UI text resolver during cleanup: %s (%d)",
                minhook_status_name(remove_status),
                static_cast<int>(remove_status));
        }
    }

    g_text_hook_target = nullptr;
    g_text_hook_created = false;
    g_text_hook_enabled = false;
    g_original_game_options.store(nullptr, std::memory_order_release);
    g_original_sub.store(nullptr, std::memory_order_release);
    g_original_text_resolver.store(nullptr, std::memory_order_release);
    g_title_capture.reset();
    g_submenu_navigation.reset();
}

} // namespace

void request_navigate_page(
    void** parent_page_slot,
    erui::detail::PageRoute target,
    NavigationRequestKind kind) noexcept {
    // Retained button thunks already reject input while a native alert owns
    // the modal slot. Keep the navigation boundary defensive as well so a new
    // internal caller cannot bypass modal ownership accidentally.
    if (native_dialog_owns_menu_input()) return;

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    void* parent_page = parent_page_slot ? *parent_page_slot : nullptr;

    if (kind == NavigationRequestKind::pagination_next) {
        runtime.pagination_next_hits.fetch_add(1, std::memory_order_relaxed);
    } else if (kind == NavigationRequestKind::pagination_previous) {
        runtime.pagination_previous_hits.fetch_add(1, std::memory_order_relaxed);
    }
    g_title_capture.reset();

    if (kind == NavigationRequestKind::pagination_previous) {
        const erui::detail::PageRoute current =
            g_submenu_navigation.route_for_native_page(parent_page);
        if (!parent_page || !g_addresses.native_back ||
            !erui::detail::is_previous_route_target(current, target)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "paginationPrevious rejected page=%p currentKind=%s currentLogical=%zu currentSlice=%zu targetKind=%s targetLogical=%zu targetSlice=%zu nativeBack=%p",
                parent_page,
                route_kind_name(current.kind),
                current.logical_page_index,
                current.slice_index + 1,
                route_kind_name(target.kind),
                target.logical_page_index,
                target.slice_index + 1,
                reinterpret_cast<void*>(g_addresses.native_back));
            return;
        }
        // Native Back restores an already-constructed parent and therefore
        // does not re-enter the subpage handler that normally publishes the
        // page protected by the dialog input gates. Resolve that parent while
        // its binding is still live, then publish it only after Back succeeds.
        void* const restored_page =
            target.kind == erui::detail::PageRouteKind::root_main
                ? g_last_page.load(std::memory_order_acquire)
                : target.kind == erui::detail::PageRouteKind::builtin_main
                ? g_submenu_navigation.parent_page(parent_page)
                : g_submenu_navigation.native_page(target);
        unsigned long seh_exception_code = 0;
        const bool invoked = invoke_native_back(
            g_addresses.native_back, parent_page, seh_exception_code);
        if (invoked) {
            g_submenu_navigation.unbind_native_page(parent_page);
            if (restored_page) {
                g_dialog_input_page.store(
                    restored_page, std::memory_order_release);
            }
        }
        erui::detail::logf(
            invoked ? erui::LogLevel::info : erui::LogLevel::error,
            "paginationPrevious page=%p currentKind=%s currentLogical=%zu currentSlice=%zu targetKind=%s targetLogical=%zu targetSlice=%zu nativeBack=%p invoked=%d seh=0x%08lX",
            parent_page,
            route_kind_name(current.kind),
            current.logical_page_index,
            current.slice_index + 1,
            route_kind_name(target.kind),
            target.logical_page_index,
            target.slice_index + 1,
            reinterpret_cast<void*>(g_addresses.native_back),
            invoked ? 1 : 0,
            seh_exception_code);
        return;
    }

    erui::detail::PageSlice target_slice{};
    const bool target_resolved = runtime.menu &&
        resolve_page_route_slice(*runtime.menu, target, target_slice);
    if (!parent_page_slot || !parent_page || !target_resolved) {
        erui::detail::logf(
            erui::LogLevel::error,
            "navigation request rejected kind=%s parentSlot=%p parentPage=%p targetKind=%s logicalPage=%zu slice=%zu capacity=%u",
            request_kind_name(kind),
            reinterpret_cast<void*>(parent_page_slot),
            parent_page,
            route_kind_name(target.kind),
            target.logical_page_index,
            target.slice_index,
            static_cast<unsigned>(target.root_capacity));
        return;
    }

    const std::uint64_t request =
        runtime.submenu_open_requests.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_addresses.open_sub_page) {
        erui::detail::logf(
            erui::LogLevel::error,
            "submenu open request[%llu] has no openSubPage function",
            static_cast<unsigned long long>(request));
        return;
    }

    const std::uint64_t now = GetTickCount64();
    if (!g_submenu_navigation.begin_request(target, now, parent_page)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "submenu open request[%llu] could not arm route kind=%s logicalPage=%zu slice=%zu",
            static_cast<unsigned long long>(request),
            route_kind_name(target.kind),
            target.logical_page_index,
            target.slice_index);
        return;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "submenuOpen[%llu] kind=%s parent=%p targetKind=%s logicalPage=%zu slice=%zu/%zu",
        static_cast<unsigned long long>(request),
        request_kind_name(kind),
        parent_page,
        route_kind_name(target.kind),
        target.logical_page_index,
        target.slice_index + 1,
        target_slice.slice_count);

    if (runtime.options.enable_diagnostics) {
        const std::uint32_t diagnostic_index =
            runtime.diagnostic_submenu_logs.fetch_add(1, std::memory_order_relaxed);
        if (diagnostic_index < kSubmenuDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "diagnostic: submenu open request[%llu] parentSlot=%p parentPage=%p targetKind=%s logicalPage=%zu slice=%zu tick=%llu openSubPage=%p",
                static_cast<unsigned long long>(request),
                reinterpret_cast<void*>(parent_page_slot),
                parent_page,
                route_kind_name(target.kind),
                target.logical_page_index,
                target.slice_index,
                static_cast<unsigned long long>(now),
                reinterpret_cast<void*>(g_addresses.open_sub_page));
        }
    }

    unsigned long seh_exception_code = 0;
    const bool opened = invoke_open_sub_page(
            g_addresses.open_sub_page,
            parent_page_slot,
            seh_exception_code);
    if (!opened) {
        g_submenu_navigation.cancel_request();
        g_title_capture.reset();
        const std::uint32_t fault =
            runtime.submenu_open_faults.fetch_add(1, std::memory_order_relaxed) + 1;
        if (fault <= kSubmenuFaultLogLimit) {
            erui::detail::logf(
                erui::LogLevel::error,
                "submenu open request[%llu] failed with SEH exception 0x%08lX (fault=%u parentPage=%p targetKind=%s logicalPage=%zu slice=%zu)",
                static_cast<unsigned long long>(request),
                seh_exception_code,
                static_cast<unsigned>(fault),
                parent_page,
                route_kind_name(target.kind),
                target.logical_page_index,
                target.slice_index);
        }
        return;
    }

    if (runtime.options.enable_diagnostics && request <= kSubmenuDiagnosticLogLimit) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: submenu open request[%llu] returned pendingKind=%s pendingLogical=%zu pendingSlice=%zu boundNativePage=%p",
            static_cast<unsigned long long>(request),
            route_kind_name(g_submenu_navigation.pending_route().kind),
            g_submenu_navigation.pending_route().logical_page_index,
            g_submenu_navigation.pending_route().slice_index,
            g_submenu_navigation.native_page(target));
    }
}

HookInstallStatus install_native_menu_hooks(const GameAddresses& addresses) noexcept {
    g_addresses = addresses;
    g_last_page.store(nullptr, std::memory_order_release);
    g_dialog_input_page.store(nullptr, std::memory_order_release);
    g_last_injection_tick.store(0, std::memory_order_release);
    g_game_options_entry_count.store(0, std::memory_order_release);
    g_game_options_injection_count.store(0, std::memory_order_release);
    g_builtin_panel_warning_mask.store(0, std::memory_order_release);
    g_text_entry_count.store(0, std::memory_order_release);
    g_text_log_count.store(0, std::memory_order_release);
    g_text_fault_count.store(0, std::memory_order_release);
    g_sub_entry_count.store(0, std::memory_order_release);
    g_sub_injection_count.store(0, std::memory_order_release);
    g_title_fault_count.store(0, std::memory_order_release);
    g_title_capture.reset();

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const bool enable_rows = runtime.options.enable_row_injection;
    const bool enable_text = runtime.options.enable_custom_text;
    const bool root_overflow = runtime.menu &&
        runtime.menu->root_plan(runtime.options.game_options_visual_capacity)
            .pages.slices.size() > 1;
    bool has_builtin_rows = false;
    if (runtime.menu) {
        for (std::uint8_t native_category = 1;
             native_category < runtime.menu->builtin_page_indices.size();
             ++native_category) {
            const erui::detail::CompiledPage* const builtin =
                runtime.menu->builtin_page(native_category);
            if (builtin && !builtin->rows.empty()) {
                has_builtin_rows = true;
                break;
            }
        }
    }
    const bool enable_submenus = enable_rows && runtime.menu &&
        (runtime.menu->modeled_submenu_count != 0 || root_overflow ||
            has_builtin_rows);
    const bool enable_popup_choices = enable_rows && runtime.menu &&
        runtime.menu->modeled_popup_choice_count != 0;
    const bool enable_text_inputs = enable_rows && runtime.menu &&
        runtime.menu->modeled_text_input_count != 0;

    if (enable_submenus) {
        std::size_t binding_capacity = 0;
        for (std::size_t page_index = 0;
             page_index < runtime.menu->page_plans.size(); ++page_index) {
            if (runtime.menu->page_reachable(page_index)) {
                binding_capacity += runtime.menu->page_plans[page_index].slices.size();
            }
        }
        std::size_t maximum_root_slices = 0;
        for (const erui::detail::RootPagePlan& plan : runtime.menu->root_plans) {
            maximum_root_slices =
                std::max(maximum_root_slices, plan.pages.slices.size());
        }
        binding_capacity += maximum_root_slices;
        // Built-in first-page capacity is discovered only after native rows
        // exist. Reserve against the worst useful case (one free slot, which
        // becomes a Next-only main page on overflow) so the UI callback never
        // allocates or exhausts bindings for continuation pages.
        for (std::uint8_t native_category = 1;
             native_category < runtime.menu->builtin_page_indices.size();
             ++native_category) {
            const erui::detail::CompiledPage* const builtin =
                runtime.menu->builtin_page(native_category);
            if (!builtin || builtin->rows.empty()) continue;
            erui::detail::PageSlice first{};
            if (erui::detail::resolve_paginated_slice(
                    runtime.menu->builtin_page_index(native_category),
                    builtin->rows.size(),
                    1,
                    erui::detail::native_subpage_capacity,
                    0,
                    first) && first.slice_count > 1) {
                binding_capacity += first.slice_count - 1;
            }
        }
        binding_capacity = std::max<std::size_t>(binding_capacity + 8, 16);
        if (!g_submenu_navigation.initialize(
                runtime.menu->pages.size(),
                binding_capacity)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "Failed to allocate native submenu page bindings for %zu logical pages (%zu physical bindings)",
                runtime.menu->pages.size(),
                binding_capacity);
            return HookInstallStatus::hook_failed;
        }
    } else {
        g_submenu_navigation.reset();
    }

    if (!enable_rows) {
        erui::detail::logf(erui::LogLevel::error, "row injection is disabled");
        return HookInstallStatus::hook_failed;
    }

    if (enable_popup_choices &&
        !install_native_popup_choice_bridge(addresses)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Popup-choice rows require the native popup-choice bridge");
        reset_hooks();
        return HookInstallStatus::hook_failed;
    }

    if (enable_rows) {
        if (!addresses.game_options_handler ||
            !install_game_options_hook(addresses)) {
            reset_hooks();
            return HookInstallStatus::hook_failed;
        }
        install_builtin_panel_materializers(addresses);
        if (enable_submenus) {
            if (!addresses.sub_handler || !addresses.open_sub_page ||
                !install_sub_hook(addresses)) {
                erui::detail::logf(
                    erui::LogLevel::error,
                    "pagination/submenu routes require resolved subpage interfaces");
                reset_hooks();
                return HookInstallStatus::hook_failed;
            }
        }
    }


    if (enable_text) {
        if (!addresses.text_resolver ||
            install_text_hook(addresses) != TextHookInstallStatus::success) {
            reset_hooks();
            return HookInstallStatus::hook_failed;
        }
    } else {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Custom text hook disabled; rows use a vanilla fallback message ID");
    }

    // TextInput state may be changed from a client callback or the host
    // worker. Its native MenuString shadow is therefore synchronized at the
    // game's page-frame boundary. This frame hook is required independently
    // of the optional native-alert transport, which merely shares it as an
    // input gate when available.
    if (enable_text_inputs && !install_page_frame_gate(addresses)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "TextInput rows require the page-frame UI pump");
        reset_hooks();
        return HookInstallStatus::hook_failed;
    }

    ModuleView game{};
    const bool dialog_input_ready = enable_text &&
        install_dialog_input_gates(addresses, enable_text_inputs);
    if (!enable_text) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport unavailable: custom text support is disabled");
    } else if (!dialog_input_ready) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport unavailable: modal input gates were not installed");
    } else if (!game.initialize(GetModuleHandleW(nullptr))) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport unavailable: main executable could not be parsed");
        reset_dialog_input_gates(enable_text_inputs);
    } else if (!install_native_dialog_transport(game)) {
        // Alerts are an optional native route. A game update may invalidate
        // their semantic signatures without disabling registered menu rows.
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport was not installed; menu rows remain active");
        reset_dialog_input_gates(enable_text_inputs);
    }

    bool color_picker_installed = false;
    const bool color_picker_requested = runtime.menu &&
        runtime.menu->modeled_color_picker_count != 0;
    // Resolve and validate ColorPicker before either our title bridge
    // patches Scaleform_ResolvePath or the optional observation trace detours
    // ColorPalette/ColorControl entries.
    if (!color_picker_requested) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "Color Picker backend not requested by the compiled menu");
    } else if (!native_dialog_available()) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Color Picker unavailable: native dialog transport was not installed");
    } else if (!install_color_picker(game)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Color Picker was requested but its native backend could not be installed");
    } else {
        color_picker_installed = true;
    }
    if (color_picker_requested && !color_picker_installed) {
        reset_hooks();
        remove_native_dialog_transport();
        return HookInstallStatus::hook_failed;
    }

    const bool title_path_bridge_requested =
        enable_submenus && addresses.title_bridge_complete();
    const bool color_widget_path_bridge_requested = color_picker_installed;
    if (title_path_bridge_requested || color_widget_path_bridge_requested) {
        void* path_hook_target = reinterpret_cast<void*>(
            addresses.scaleform_path_resolver);
        if (!path_hook_target && color_picker_installed) {
            path_hook_target = color_picker_widget_path_hook_target();
        }
        if (path_hook_target) {
            auto path_result = SafetyHookMid::create(
                path_hook_target,
                &observe_scaleform_path);
            if (path_result) {
                g_scaleform_path_resolver_hook = std::move(*path_result);
                set_color_picker_widget_path_bridge_available(
                    color_picker_installed);
                erui::detail::logf(
                    erui::LogLevel::info,
                    "Shared Scaleform path bridge installed (titles=%d colorWidget=%d)",
                    title_path_bridge_requested ? 1 : 0,
                    color_widget_path_bridge_requested ? 1 : 0);
            } else {
                set_color_picker_widget_path_bridge_available(false);
                erui::detail::logf(
                    erui::LogLevel::warning,
                    "Shared Scaleform path bridge could not be installed (SafetyHook error type %u); dependent presentation features are disabled",
                    static_cast<unsigned>(path_result.error().type));
            }
        } else {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Shared Scaleform path target is unavailable; dependent presentation features are disabled");
        }
    } else if (enable_submenus) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Scaleform title interfaces are incomplete; custom page titles are disabled while menu rows remain active");
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Native UI runtime active (rows=%d submenus=%d pagination=%d customText=%d diagnostics=%d cooldown=%u ms)",
        enable_rows ? 1 : 0,
        enable_submenus ? 1 : 0,
        runtime.menu && runtime.menu->pagination_required_for_capacity(
            runtime.options.game_options_visual_capacity) ? 1 : 0,
        enable_text ? 1 : 0,
        runtime.options.enable_diagnostics ? 1 : 0,
        static_cast<unsigned>(runtime.options.injection_cooldown_ms));
    return HookInstallStatus::success;
}

void remove_native_menu_hooks() noexcept {
    remove_color_picker();
    remove_native_dialog_transport();
    reset_hooks();
}

} // namespace erui::native
