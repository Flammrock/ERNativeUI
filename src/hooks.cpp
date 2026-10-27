#include "hooks.hpp"

#include "dialog_input_gate.hpp"
#include "game_build.hpp"
#include "native_dialog.hpp"
#include "native_menu.hpp"
#include "native_popup_choice.hpp"
#include "native_slider_presentation.hpp"
#include "native_row_presentation.hpp"
#include "native_row_visibility.hpp"
#include "native_text_input.hpp"
#include "native_text_result.hpp"
#include "native_title_bridge.hpp"
#include "pagination.hpp"
#include "runtime_log.hpp"
#include "runtime_requirements.hpp"
#include "runtime_state.hpp"
#include "row_presentation_state.hpp"
#include "row_update_state.hpp"
#include "root_button_text_override.hpp"
#include "submenu_navigation.hpp"
#include "submenu_runtime.hpp"
#include "submenu_update_state.hpp"
#include "tab_update_state.hpp"
#include "color_picker.hpp"

#include <Windows.h>
#include <MinHook.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
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
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
struct TabCarrierAllocatorTarget;
struct TabCarrierBuildProfile {
    EldenRingBuild build{};
    std::uint32_t timestamp{};
    std::size_t image_size{};
    std::uintptr_t category8_pre_rva{};
    std::uintptr_t category8_materializer_rva{};
    std::uintptr_t category8_extra_callback_rva{};
    std::uintptr_t composite_teardown_rva{};
    std::uintptr_t option_setting_dialog_vtable_rva{};
    std::uintptr_t scene_obj_proxy_vtable_rva{};
    std::uintptr_t scene_obj_proxy_get_value_rva{};
    std::uintptr_t carrier_manager_rva{};
    std::uintptr_t carrier_deactivate_rva{};
    std::uintptr_t composite_vtable_rva{};
    std::uintptr_t top_vtable_rva{};
    std::uintptr_t extra_callback_anchor_rva{};
    std::uintptr_t row_callback_anchor_rva{};
    std::uintptr_t builder_anchor_rva{};
    std::uintptr_t route_anchor_rva{};
    std::array<std::uint8_t, 16> manager_entry{};
    const TabCarrierAllocatorTarget* allocator_targets{};
    std::size_t allocator_target_count{};
};

SafetyHookMid g_tab_content_category8_pre_hook{};
SafetyHookInline g_tab_content_composite_teardown_hook{};
using TabCarrierCompositeTeardownFn = void(__fastcall*)(void*);
std::atomic<TabCarrierCompositeTeardownFn>
    g_original_tab_content_composite_teardown{};
std::atomic_bool g_tab_content_hooks_exposed{};
std::atomic_bool g_tab_content_callbacks_armed{};
std::atomic_uint64_t g_tab_content_teardown_generation{};
std::atomic_uint32_t g_tab_content_route_log_count{};
std::atomic_uint32_t g_tab_content_route_fault_log_count{};
const TabCarrierBuildProfile* g_tab_carrier_build_profile{};
// The first-page capacity is discovered from each live Graphics carrier. Use
// one row only when reserving pagination bindings: it is the conservative
// worst case and is not a claim about the authored GFX capacity.
constexpr std::uint8_t kTabCarrierMinimumPlanCapacity = 1;
#endif
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
constexpr std::array<std::uint8_t, 16> kScaleformPathResolverEntry{
    0x4C, 0x89, 0x44, 0x24, 0x18, 0x4C, 0x89, 0x4C,
    0x24, 0x20, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56};
std::mutex g_scaleform_path_bridge_mutex{};
void* g_scaleform_path_bridge_target{};
std::array<std::uint8_t, 16> g_scaleform_path_bridge_patch{};
bool g_scaleform_path_bridge_proven{};
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

// Only physically open submenu pages need a live title target. Keep this
// fixed-capacity and UI-thread-owned so opening or updating a submenu never
// allocates from a native callback.
struct ActiveSubmenuTitleBinding {
    void* page{};
    CapturedTitleTarget target{};
    erui::detail::PageRoute route{
        .logical_page_index = erui::detail::invalid_route_index,
    };
    std::size_t slice_count{};
    std::uint64_t applied_revision{};
};
constexpr std::size_t kActiveSubmenuTitleBindingCapacity = 64;
std::array<ActiveSubmenuTitleBinding,
    kActiveSubmenuTitleBindingCapacity> g_active_submenu_titles{};

void retire_active_submenu_title(void* page) noexcept {
    if (!page) return;
    for (ActiveSubmenuTitleBinding& binding : g_active_submenu_titles) {
        if (binding.page == page) {
            binding = {};
            return;
        }
    }
}

constexpr std::size_t kPanelVisualCapacityOffset = 0xB14;
constexpr std::size_t kPanelNativeCountOffset = 0x1AF0;
constexpr std::uintptr_t kOptionSettingDialogVtableRva = 0x2B14888;

[[nodiscard]] std::uintptr_t option_setting_dialog_vtable_rva() noexcept {
    if (g_addresses.game_image_base &&
        g_addresses.option_setting_dialog_vtable) {
        const auto base = reinterpret_cast<std::uintptr_t>(
            g_addresses.game_image_base);
        const auto value = reinterpret_cast<std::uintptr_t>(
            g_addresses.option_setting_dialog_vtable);
        if (value >= base && value - base < g_addresses.game_image_size) {
            return value - base;
        }
    }
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    if (g_tab_carrier_build_profile) {
        return g_tab_carrier_build_profile->option_setting_dialog_vtable_rva;
    }
#endif
    return kOptionSettingDialogVtableRva;
}

bool read_scaleform_path_entry(
    const void* target,
    std::array<std::uint8_t, 16>& bytes) noexcept {
    if (!target) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(bytes.data(), target, bytes.size());
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        bytes = {};
        return false;
    }
#endif
}

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
        const std::uintptr_t vtable_rva =
            option_setting_dialog_vtable_rva();
        const void* expected_vtable = reinterpret_cast<const void*>(
            image + vtable_rva);
        result.valid = image &&
            vtable_rva < g_addresses.game_image_size &&
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

#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
void __fastcall tab_content_category8_materializer(
    void* page,
    void* menu_option_data) noexcept;

constexpr std::size_t kPanelSceneProxyOffset = 0x1200;
constexpr std::size_t kSceneProxyWrapperFlagsOffset = 0x20;
constexpr std::size_t kSceneProxyWrapperPayloadOffset = 0x28;
struct TabCarrierIdentity {
    void* panel{};
    void* wrapper{};
    void* payload{};
    std::uint32_t wrapper_flags{};
    std::size_t logical_page_index{
        erui::detail::invalid_compiled_index};
    std::uint8_t category{};
    bool valid{};
    bool committed{};
};

std::uint64_t g_tab_carrier_epoch{};

TabCarrierIdentity inspect_tab_carrier(
    void* page,
    std::uint8_t category,
    std::size_t logical_page_index) noexcept {
    TabCarrierIdentity result{};
    result.panel = page;
    result.category = category;
    result.logical_page_index = logical_page_index;
    const TabCarrierBuildProfile* const profile =
        g_tab_carrier_build_profile;
    if (!page || !profile || !g_addresses.game_image_base ||
        profile->scene_obj_proxy_get_value_rva >=
            g_addresses.game_image_size ||
        profile->scene_obj_proxy_vtable_rva >=
            g_addresses.game_image_size) {
        return result;
    }
#if defined(_MSC_VER)
    __try {
#endif
        const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
            g_addresses.game_image_base);
        auto* const proxy = static_cast<std::byte*>(page) +
            kPanelSceneProxyOffset;
        void* const proxy_vtable = *reinterpret_cast<void**>(proxy);
        void* const expected_vtable = reinterpret_cast<void*>(
            image + profile->scene_obj_proxy_vtable_rva);
        void* const getter = proxy_vtable
            ? *reinterpret_cast<void**>(
                  static_cast<std::byte*>(proxy_vtable) + sizeof(void*))
            : nullptr;
        if (proxy_vtable != expected_vtable ||
            getter != reinterpret_cast<void*>(
                image + profile->scene_obj_proxy_get_value_rva)) {
            return result;
        }
        using GetterFn = void*(__fastcall*)(void*);
        result.wrapper = reinterpret_cast<GetterFn>(getter)(proxy);
        if (!result.wrapper) return result;
        result.wrapper_flags = *reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::byte*>(result.wrapper) +
                kSceneProxyWrapperFlagsOffset);
        result.payload = *reinterpret_cast<void* const*>(
            static_cast<const std::byte*>(result.wrapper) +
                kSceneProxyWrapperPayloadOffset);
        result.valid = result.payload != nullptr &&
            (result.wrapper_flags & 0x8Fu) >= 2u;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = {};
        result.panel = page;
        result.category = category;
        result.logical_page_index = logical_page_index;
    }
#endif
    return result;
}

// Graphics carrier V2. Category 8 is the sole recyclable
// rows-only carrier. Category 5 is used only as synchronous native parking.
TabCarrierIdentity g_tab_carrier{};
std::size_t g_reusable_route{erui::detail::invalid_compiled_index};
void* g_reusable_composite{};
std::uint64_t g_reusable_activation_frame{};
std::atomic_bool g_reusable_failed{};
using TabCarrierParkingSwitchFn = bool(__cdecl*)(void*, std::int32_t);
using TabCarrierQueueFn = void(__fastcall*)(void*, void*);

std::size_t custom_tab_route_count() noexcept {
    const auto& runtime = erui::detail::runtime_state();
    return runtime.menu ? runtime.menu->tab_page_indices.size() : 0;
}

std::size_t native_graphics_route() noexcept {
    return custom_tab_route_count();
}

std::size_t graphics_route_count() noexcept {
    const std::size_t custom_count = custom_tab_route_count();
    return custom_count == (std::numeric_limits<std::size_t>::max)()
        ? custom_count
        : custom_count + 1;
}

struct TabCarrierAllocatorTarget {
    std::uintptr_t vtable;
    std::uintptr_t method;
    std::uint64_t entry;
};

// Exact DLAllocator descendants validated for Configuration panels in each
// admitted executable. No unlisted vtable/queue pair is called.
constexpr std::array<TabCarrierAllocatorTarget, 26>
    kTabCarrierAllocatorTargets271{{
        {0x29D54F0,0x2657530,0xFF864788E9CA8B48},
        {0x2B7D418,0xB97EC0,0x74894808245C8948},
        {0x2BDE708,0xE14B60,0x8B4820EC83485340},
        {0x2BDE8F0,0xE156B0,0xC74840EC83485740},
        {0x2BDECA0,0xE16260,0x28C1834808498B48},
        {0x2BDEA48,0xE15BA0,0x28C1834808498B48},
        {0x2BDED78,0xE166D0,0xC74840EC83485740},
        {0x2BDE818,0xE150C0,0xC74840EC83485740},
        {0x2BDEB20,0xE15F30,0x28C1834808498B48},
        {0x2BDEE50,0xE16CA0,0xC74840EC83485740},
        {0x2BE1870,0xE1F250,0x8B4820EC83485340},
        {0x2BE1A20,0xE1F7B0,0x8B4820EC83485340},
        {0x2F26FA8,0x1C59C30,0x00262088E9CA8B48},
        {0x2F273D0,0x1AF6110,0x5FA5EF08900000C2},
        {0x30357D8,0x1B44400,0x28C1834808498B48},
        {0x3051240,0x1C29FE0,0xA33CC7EC900000C2},
        {0x30B0298,0x1EBCE90,0xEB17900000065BE9},
        {0x30C0228,0x1F38D90,0xC74840EC83485740},
        {0x30CA908,0x1F61F50,0x48018B4808498B48},
        {0x30CAE70,0x1F64450,0xC74830EC83485740},
        {0x30CFE80,0x1F81AA0,0x3176CC030E302AE9},
        {0x30CFF58,0x1F81AB0,0x35A19002D12495E9},
        {0x3278EA0,0x26398D0,0xC74830EC83485740},
        {0x30C74A0,0x1A4900,0x28C1834808498B48},
        {0x2BBFF38,0x1538B0,0xC74840EC83485740},
        {0x2BE0F30,0x1F1A8D0,0x48018B4808498B48},
    }};

constexpr std::array<TabCarrierAllocatorTarget, 26>
    kTabCarrierAllocatorTargets272{{
        {0x29D74F0,0x2658C60,0xFF864748E9CA8B48},
        {0x2B7F4A0,0xB981C0,0x83485308244C8948},
        {0x2BE09B8,0xE16250,0x8B4820EC83485340},
        {0x2BE0BA0,0xE16DA0,0xC74840EC83485740},
        {0x2BE0F50,0xE17950,0x28C1834808498B48},
        {0x2BE0CF8,0xE17290,0x28C1834808498B48},
        {0x2BE1028,0xE17DC0,0xC74840EC83485740},
        {0x2BE0AC8,0xE167B0,0xC74840EC83485740},
        {0x2BE0DD0,0xE17620,0x28C1834808498B48},
        {0x2BE1100,0xE18390,0xC74840EC83485740},
        {0x2BE3B20,0xE20940,0x8B4820EC83485340},
        {0x2BE3CD0,0xE20EA0,0x8B4820EC83485340},
        {0x2F29248,0x1C5B320,0x00262088E9CA8B48},
        {0x2F29670,0x1AF7800,0x2745C748CC0000C2},
        {0x3037A78,0x1B45AF0,0x28C1834808498B48},
        {0x30534E0,0x1C2B6D0,0xFF418D48CC0000C2},
        {0x30B2558,0x1EBE580,0xE872CC0000065BE9},
        {0x30C24E8,0x1F3A480,0xC74840EC83485740},
        {0x30CCBC8,0x1F63640,0x48018B4808498B48},
        {0x30CD130,0x1F65B40,0xC74830EC83485740},
        {0x30D2140,0x1F83190,0x8B41CC02D5E587E9},
        {0x30D2218,0x1F831A0,0x8B489003B215C8E9},
        {0x327B160,0x263B000,0xC74830EC83485740},
        {0x30C9760,0x1A4900,0x28C1834808498B48},
        {0x2BC21E8,0x1538B0,0xC74840EC83485740},
        {0x2BE31E0,0x1F1BFC0,0x48018B4808498B48},
    }};

constexpr std::array<TabCarrierBuildProfile, 2>
    kTabCarrierBuildProfiles{{
        {
            EldenRingBuild::v2_7_1_0,
            0x6A96B418,
            0x5E0DA00,
            0x93CF89,
            0x95C050,
            0x95A820,
            0x93C1A0,
            0x2B14888,
            0x2A97AF0,
            0x74C930,
            0xE1C080,
            0x953520,
            0x2B0C950,
            0x2B16B48,
            0x93CF20,
            0x93CF32,
            0x93CF58,
            0x93CF84,
            {
                0x4C,0x63,0x05,0x81,0xF3,0xF6,0x02,0x33,
                0xD2,0x4D,0x85,0xC0,0x7E,0x1E,0x48,0x8D,
            },
            kTabCarrierAllocatorTargets271.data(),
            kTabCarrierAllocatorTargets271.size(),
        },
        {
            EldenRingBuild::v2_7_2_0,
            0x6AABF7E7,
            0x5DFD600,
            0x93D439,
            0x95C500,
            0x95ACD0,
            0x93C650,
            0x2B16978,
            0x2A99BE0,
            0x74CDE0,
            0xE1D770,
            0x9539D0,
            0x2B0EA40,
            0x2B18C38,
            0x93D3D0,
            0x93D3E2,
            0x93D408,
            0x93D434,
            {
                0x4C,0x63,0x05,0xB1,0x0C,0xF7,0x02,0x33,
                0xD2,0x4D,0x85,0xC0,0x7E,0x1E,0x48,0x8D,
            },
            kTabCarrierAllocatorTargets272.data(),
            kTabCarrierAllocatorTargets272.size(),
        },
    }};

[[nodiscard]] constexpr const TabCarrierBuildProfile*
find_tab_carrier_build_profile(
    std::uint32_t timestamp,
    std::size_t image_size) noexcept {
    for (const TabCarrierBuildProfile& profile :
         kTabCarrierBuildProfiles) {
        if (profile.timestamp == timestamp &&
            profile.image_size == image_size) {
            return &profile;
        }
    }
    return nullptr;
}

bool reusable_fault(const char* reason) noexcept {
    g_reusable_failed.store(true, std::memory_order_release);
    erui::detail::logf(erui::LogLevel::error,
        "Native-tab Graphics carrier FAILED CLOSED: %s; custom transitions frozen, native input remains live",
        reason);
    return false;
}

bool exact_allocator_target(
    std::uintptr_t image,
    void* manager_vtable,
    void* queue) noexcept {
    const TabCarrierBuildProfile* const profile =
        g_tab_carrier_build_profile;
    if (!profile || !profile->allocator_targets ||
        profile->allocator_target_count == 0) return false;
    for (std::size_t index = 0;
         index < profile->allocator_target_count;
         ++index) {
        const TabCarrierAllocatorTarget& target =
            profile->allocator_targets[index];
        if (manager_vtable == reinterpret_cast<void*>(
                image + target.vtable) &&
            queue == reinterpret_cast<void*>(image + target.method) &&
            std::memcmp(queue, &target.entry, sizeof(target.entry)) == 0) {
            return true;
        }
    }
    return false;
}

bool cached_identity_is_globally_unique(
    void** slots,
    std::size_t owner_slot,
    const TabCarrierIdentity& owner,
    const char* label) noexcept {
    if (!slots || owner_slot >= 10 || !owner.valid ||
        slots[owner_slot] != owner.panel || !owner.wrapper ||
        !owner.payload) {
        erui::detail::logf(erui::LogLevel::error,
            "Native-tab Graphics carrier identity audit rejected %s owner slot=%zu panel=%p wrapper=%p payload=%p valid=%d",
            label ? label : "unknown", owner_slot, owner.panel,
            owner.wrapper, owner.payload, owner.valid ? 1 : 0);
        return false;
    }
    for (std::size_t slot = 0; slot < 10; ++slot) {
        if (slot == owner_slot || !slots[slot]) continue;
        const auto other = inspect_tab_carrier(
            slots[slot], static_cast<std::uint8_t>(slot),
            erui::detail::invalid_compiled_index);
        const bool distinct = other.valid &&
            other.panel != owner.panel &&
            other.wrapper != owner.wrapper &&
            other.payload != owner.payload;
        if (!distinct) {
            erui::detail::logf(erui::LogLevel::error,
                "Native-tab Graphics carrier identity audit rejected %s ownerSlot=%zu owner=%p/%p/%p conflictingSlot=%zu other=%p/%p/%p valid=%d",
                label ? label : "unknown", owner_slot,
                owner.panel, owner.wrapper, owner.payload, slot,
                other.panel, other.wrapper, other.payload,
                other.valid ? 1 : 0);
            return false;
        }
    }
    return true;
}

void clear_retired_page_references(void* panel) noexcept {
    if (!panel) return;
    retire_active_submenu_title(panel);
    retire_native_row_visibility_page(panel);
    retire_native_row_presentation_page(panel, g_addresses);
    void* expected = panel;
    const bool dialog_cleared = g_dialog_input_page.compare_exchange_strong(
        expected, nullptr, std::memory_order_acq_rel,
        std::memory_order_acquire);
    expected = panel;
    const bool root_cleared = g_last_page.compare_exchange_strong(
        expected, nullptr, std::memory_order_acq_rel,
        std::memory_order_acquire);

    // A tab transition cannot legitimately overlap a pending subpage open.
    // Cancel the request before destroying its possible parent page. Existing
    // child bindings are left intact and are normally removed by native Back.
    const bool pending_subpage =
        g_submenu_navigation.pending_route().valid();
    if (pending_subpage) g_submenu_navigation.cancel_request();
    reset_color_picker_page_widgets(
        panel, ColorPickerWidgetHost::graphics);
    erui::detail::logf(erui::LogLevel::trace,
        "Native-tab Graphics carrier cleared retiring page references panel=%p dialog=%d root=%d pendingSubpageCancelled=%d",
        panel, dialog_cleared ? 1 : 0, root_cleared ? 1 : 0,
        pending_subpage ? 1 : 0);
}

std::uint32_t graphics_carrier_version_v2_impl() noexcept {
    return 2;
}

bool prepare_graphics_carrier_v2_impl(
    void* composite,
    std::size_t route,
    std::uint64_t completed_frame,
    std::int32_t* category,
    TabCarrierParkingSwitchFn park_native) noexcept {
    if (g_reusable_failed.load(std::memory_order_acquire)) return false;
    const auto& runtime = erui::detail::runtime_state();
    const std::size_t custom_count = custom_tab_route_count();
    if (!composite || !category || !park_native || !runtime.menu ||
        custom_count == 0 || route >= graphics_route_count() ||
        (g_reusable_composite && composite != g_reusable_composite)) {
        return reusable_fault(
            "invalid composite, catalogue, route or output");
    }
    const auto image = reinterpret_cast<std::uintptr_t>(
        g_addresses.game_image_base);
    const TabCarrierBuildProfile* const profile =
        g_tab_carrier_build_profile;
    if (!image || !profile ||
        g_addresses.game_image_size != profile->image_size) {
        return reusable_fault(
            "retirement requires an exact admitted Elden Ring image");
    }

#if defined(_MSC_VER)
    __try {
#endif
        constexpr std::array<std::uint8_t, 16> deactivate_entry{
            0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,
            0xEC,0x20,0x8B,0xDA,0x48,0x8B,0xF9,0xE8};
        if (std::memcmp(
                reinterpret_cast<const void*>(
                    image + profile->carrier_manager_rva),
                profile->manager_entry.data(),
                profile->manager_entry.size()) != 0 ||
            std::memcmp(reinterpret_cast<const void*>(
                    image + profile->carrier_deactivate_rva),
                deactivate_entry.data(), deactivate_entry.size()) != 0) {
            return reusable_fault(
                "retirement manager/deactivate bytes differed");
        }

        auto* const bytes = static_cast<std::byte*>(composite);
        if (*reinterpret_cast<void**>(bytes) !=
                reinterpret_cast<void*>(
                    image + profile->composite_vtable_rva) ||
            *reinterpret_cast<void**>(bytes - 0x1768) !=
                reinterpret_cast<void*>(image + profile->top_vtable_rva)) {
            return reusable_fault(
                "composite/top vtable differed from validated objects");
        }

        auto** const slots = reinterpret_cast<void**>(bytes + 0x68);
        void* current = *reinterpret_cast<void**>(bytes + 0xB8);
        g_reusable_composite = composite;
        constexpr std::int32_t target = 8;
        constexpr std::int32_t parking = 5;

        if (g_reusable_route == route && slots[target]) {
            const auto observed = inspect_tab_carrier(
                slots[target], target,
                g_tab_carrier.logical_page_index);
            if (!g_tab_carrier.committed || !observed.valid ||
                slots[target] != g_tab_carrier.panel ||
                observed.wrapper != g_tab_carrier.wrapper ||
                observed.payload != g_tab_carrier.payload ||
                !cached_identity_is_globally_unique(
                    slots, target, observed,
                    "same-route Graphics carrier")) {
                return reusable_fault(
                    "cached route lost its exact committed Graphics carrier");
            }
            *category = target;
            g_reusable_activation_frame = completed_frame;
            return true;
        }

        void* const old_panel = slots[target];
        if (old_panel) {
            if (completed_frame <= g_reusable_activation_frame) {
                return reusable_fault(
                    "Graphics carrier retirement requires one completed frame");
            }
            const auto observed = inspect_tab_carrier(
                old_panel, target,
                g_tab_carrier.logical_page_index);
            const auto panel = inspect_builtin_panel(old_panel);
            if (!g_tab_carrier.committed || !observed.valid ||
                !panel.valid || g_reusable_route >= graphics_route_count() ||
                observed.panel != g_tab_carrier.panel ||
                observed.wrapper != g_tab_carrier.wrapper ||
                observed.payload != g_tab_carrier.payload ||
                !cached_identity_is_globally_unique(
                    slots, target, observed,
                    "pre-retirement Graphics carrier")) {
                return reusable_fault(
                    "Graphics identity failed before native retirement");
            }

            using ManagerFn = void*(__fastcall*)(void*);
            using DeactivateFn = void(__fastcall*)(void*, bool);
            void* const manager =
                reinterpret_cast<ManagerFn>(
                    image + profile->carrier_manager_rva)(old_panel);
            if (!manager) return reusable_fault("retirement manager was null");
            void* const deactivate = *reinterpret_cast<void**>(
                static_cast<std::byte*>(panel.vtable) + 0x08);
            void* const manager_vtable = *reinterpret_cast<void**>(manager);
            void* const queue = manager_vtable
                ? *reinterpret_cast<void**>(
                      static_cast<std::byte*>(manager_vtable) + 0x68)
                : nullptr;
            if (deactivate != reinterpret_cast<void*>(
                    image + profile->carrier_deactivate_rva) ||
                !manager_vtable || !queue ||
                !exact_allocator_target(image, manager_vtable, queue)) {
                return reusable_fault(
                    "retirement allocator/deactivate target was not exact");
            }

            if (old_panel == current) {
                void* const parking_before = slots[parking];
                const auto parking_identity_before = inspect_tab_carrier(
                    parking_before, parking,
                    erui::detail::invalid_compiled_index);
                if (parking_before &&
                    (!inspect_builtin_panel(parking_before).valid ||
                        !parking_identity_before.valid ||
                        parking_before == old_panel ||
                        parking_identity_before.wrapper == observed.wrapper ||
                        parking_identity_before.payload == observed.payload ||
                        !cached_identity_is_globally_unique(
                            slots, parking, parking_identity_before,
                            "cached Network parking panel"))) {
                    return reusable_fault(
                        "cached Network parking identity was invalid or aliased");
                }
                if (!park_native(composite, parking)) {
                    return reusable_fault(
                        "native Network parking switch/state isolation failed");
                }
                void* const parked_panel = slots[parking];
                const auto parked = inspect_tab_carrier(
                    parked_panel, parking,
                    erui::detail::invalid_compiled_index);
                const auto old_after = inspect_tab_carrier(
                    old_panel, target,
                    g_tab_carrier.logical_page_index);
                if (!parked_panel || !parked.valid ||
                    !inspect_builtin_panel(parked_panel).valid ||
                    *reinterpret_cast<void**>(bytes + 0xB8) != parked_panel ||
                    parked_panel == old_panel ||
                    parked.wrapper == observed.wrapper ||
                    parked.payload == observed.payload ||
                    slots[target] != old_panel || !old_after.valid ||
                    old_after.wrapper != observed.wrapper ||
                    old_after.payload != observed.payload ||
                    !cached_identity_is_globally_unique(
                        slots, target, old_after,
                        "parked Graphics carrier") ||
                    !cached_identity_is_globally_unique(
                        slots, parking, parked,
                        "active Network parking panel") ||
                    (parking_before &&
                        (parked_panel != parking_before ||
                            parked.wrapper != parking_identity_before.wrapper ||
                            parked.payload != parking_identity_before.payload))) {
                    return reusable_fault(
                        "Network parking did not leave Graphics intact/inactive");
                }
                current = parked_panel;
                erui::detail::logf(erui::LogLevel::trace,
                    "Native-tab Graphics carrier parked frame=%llu route=%zu category=5 panel=%p wrapper=%p payload=%p cached=%d currentExact=1 graphicsInactive=1 stateCopySuppressed=1 synchronous=1",
                    static_cast<unsigned long long>(completed_frame), route,
                    parked_panel, parked.wrapper, parked.payload,
                    parking_before != nullptr);
            }

            if (old_panel == *reinterpret_cast<void**>(bytes + 0xB8) ||
                *reinterpret_cast<void**>(bytes + 0xB8) != current) {
                return reusable_fault(
                    "refused retirement because Graphics was current/current changed");
            }
            void* const parking_pre_retire = slots[parking];
            const auto parking_pre_identity = inspect_tab_carrier(
                parking_pre_retire, parking,
                erui::detail::invalid_compiled_index);
            if (parking_pre_retire &&
                (!parking_pre_identity.valid ||
                    !cached_identity_is_globally_unique(
                        slots, parking, parking_pre_identity,
                        "pre-retirement Network parking panel"))) {
                return reusable_fault(
                    "Network parking identity was not globally unique before retirement");
            }
            clear_retired_page_references(old_panel);
            reinterpret_cast<DeactivateFn>(deactivate)(old_panel, false);
            if (*reinterpret_cast<void**>(bytes + 0xB8) != current ||
                slots[target] != old_panel) {
                return reusable_fault(
                    "current/cache changed during Graphics deactivation");
            }
            {
                bool queue_returned{};
#if defined(_MSC_VER)
                __try {
#endif
                    reinterpret_cast<TabCarrierQueueFn>(queue)(
                        manager, old_panel);
                    queue_returned = true;
#if defined(_MSC_VER)
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // Ownership may already have transferred. Clear the sole
                    // cache owner before failing so native teardown cannot
                    // queue the same panel a second time.
                    slots[target] = nullptr;
                }
#endif
                const bool post_current_unchanged =
                    *reinterpret_cast<void**>(bytes + 0xB8) == current;
                const bool post_graphics_pointer_unchanged =
                    slots[target] == old_panel;
                const bool post_network_pointer_unchanged =
                    slots[parking] == parking_pre_retire;
                // From this point the allocator owns old_panel. Never inspect
                // or dereference it again through slot 8.
                slots[target] = nullptr;
                g_tab_carrier = {};
                ++g_tab_carrier_epoch;
                if (g_tab_carrier_epoch == 0) g_tab_carrier_epoch = 1;
                if (!queue_returned || !post_current_unchanged ||
                    !post_graphics_pointer_unchanged ||
                    !post_network_pointer_unchanged) {
                    return reusable_fault(
                        "post-queue raw cache/current invariants differed; slot 8 was cleared to prevent double retirement");
                }
                erui::detail::logf(erui::LogLevel::trace,
                    "Native-tab Graphics carrier retired frame=%llu activationFrame=%llu ageFrames=%llu priorRoute=%zu nextRoute=%zu old=%p/%p/%p active=%p manager=%p managerVtable=%p deactivate=%p queue=%p allocatorExact=1 slot8Null=%d currentUntouched=%d networkUntouched=1",
                    static_cast<unsigned long long>(completed_frame),
                    static_cast<unsigned long long>(
                        g_reusable_activation_frame),
                    static_cast<unsigned long long>(
                        completed_frame - g_reusable_activation_frame),
                    g_reusable_route, route, old_panel, observed.wrapper,
                    observed.payload, current, manager, manager_vtable,
                    deactivate, queue, slots[target] == nullptr,
                    post_current_unchanged);
            }
        } else if (g_tab_carrier.committed) {
            return reusable_fault(
                "empty Graphics cache retained committed identity");
        }

        g_reusable_route = route;
        g_reusable_activation_frame = completed_frame;
        *category = target;
        erui::detail::logf(erui::LogLevel::trace,
            "Native-tab Graphics carrier prepared frame=%llu route=%zu kind=%s current=%p slot8Empty=%d",
            static_cast<unsigned long long>(completed_frame), route,
            route == native_graphics_route() ? "native-graphics" : "custom",
            current, slots[target] == nullptr);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return reusable_fault(
            "SEH during Graphics staging/retirement");
    }
#endif
}

bool verify_graphics_carrier_v2_impl(
    void* composite,
    std::size_t route,
    std::int32_t category) noexcept {
    if (g_reusable_failed.load(std::memory_order_acquire) ||
        composite != g_reusable_composite ||
        route >= graphics_route_count() || category != 8) return false;
#if defined(_MSC_VER)
    __try {
#endif
        const auto& identity = g_tab_carrier;
        const auto observed = inspect_tab_carrier(
            identity.panel, 8, identity.logical_page_index);
        const auto panel = inspect_builtin_panel(identity.panel);
        auto* const bytes = static_cast<std::byte*>(composite);
        const bool exact = identity.committed &&
            g_reusable_route == route && observed.valid && panel.valid &&
            observed.wrapper == identity.wrapper &&
            observed.payload == identity.payload &&
            *reinterpret_cast<void**>(bytes + 0xB8) == identity.panel &&
            *reinterpret_cast<void**>(bytes + 0x68 + 8 * 8) ==
                identity.panel &&
            cached_identity_is_globally_unique(
                reinterpret_cast<void**>(bytes + 0x68), 8, observed,
                "activated Graphics carrier");
        erui::detail::logf(
            exact ? erui::LogLevel::trace : erui::LogLevel::error,
            "Native-tab Graphics carrier activated route=%zu kind=%s category=8 panel=%p wrapper=%p payload=%p rows=%llu committed=%d currentExact=%d",
            route,
            route == native_graphics_route() ? "native-graphics" : "custom",
            identity.panel, identity.wrapper, identity.payload,
            static_cast<unsigned long long>(panel.native_count),
            identity.committed, exact);
        return exact || reusable_fault(
            "activation did not commit requested Graphics route");
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return reusable_fault("SEH during Graphics activation verification");
    }
#endif
}


void log_tab_content_route_fault(const char* message) noexcept {
    const std::uint32_t event =
        g_tab_content_route_fault_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event <= 8) {
        erui::detail::logf(erui::LogLevel::error,
            "Native-tab Graphics-carrier route fault[%u]: %s",
            static_cast<unsigned>(event), message);
    }
}

void begin_tab_content_category8_route(
    safetyhook::Context& context) noexcept {
    if (!g_tab_content_callbacks_armed.load(std::memory_order_acquire) ||
        g_reusable_failed.load(std::memory_order_acquire)) {
        // A sticky carrier fault must degrade to Elden Ring's original
        // Graphics construction. Replacing the callback and then letting the
        // host materializer reject it would otherwise create an empty page.
        return;
    }
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
        g_addresses.game_image_base);
    const TabCarrierBuildProfile* const profile =
        g_tab_carrier_build_profile;
    if (!image || !profile || profile->category8_materializer_rva >=
            g_addresses.game_image_size) {
        log_tab_content_route_fault("invalid image bounds");
        return;
    }
    bool replaced{};
    bool secondary_zeroed{};
    std::uint8_t observed_selector{};
    std::uintptr_t observed_primary_slot{};
    std::uintptr_t observed_secondary_slot{};
    std::uint8_t observed_enabled{};
    std::uint8_t observed_reserved{};
    std::uintptr_t observed_primary_callback{};
    std::uint32_t observed_primary_metadata{};
    std::uintptr_t observed_secondary_callback{};
    std::uint32_t observed_secondary_metadata{};
#if defined(_MSC_VER)
    __try {
#endif
        auto* const stack = reinterpret_cast<std::byte*>(context.rsp);
        observed_selector =
            *reinterpret_cast<const std::uint8_t*>(stack + 0x20);
        observed_primary_slot =
            *reinterpret_cast<const std::uintptr_t*>(stack + 0x28);
        observed_secondary_slot =
            *reinterpret_cast<const std::uintptr_t*>(stack + 0x30);
        observed_enabled =
            *reinterpret_cast<const std::uint8_t*>(stack + 0x38);
        observed_reserved =
            *reinterpret_cast<const std::uint8_t*>(stack + 0x40);
        auto* const secondary_callback =
            reinterpret_cast<std::uintptr_t*>(
            stack + 0x50);
        auto* const secondary_metadata =
            reinterpret_cast<std::uint32_t*>(stack + 0x58);
        auto* const primary_callback =
            reinterpret_cast<std::uintptr_t*>(
            stack + 0x60);
        auto* const primary_metadata =
            reinterpret_cast<std::uint32_t*>(stack + 0x68);
        observed_primary_callback = *primary_callback;
        observed_primary_metadata = *primary_metadata;
        observed_secondary_callback = *secondary_callback;
        observed_secondary_metadata = *secondary_metadata;
        // Both callback descriptors occupy 16 stack bytes, but the wrapper
        // initializes only an eight-byte callback and four-byte metadata
        // field. The final four bytes are padding and must neither be read as
        // metadata nor mutated.
        const bool exact = observed_selector == 11 &&
            observed_primary_slot == context.rsp + 0x60 &&
            observed_secondary_slot == context.rsp + 0x50 &&
            observed_enabled == 1 && observed_reserved == 0 &&
            observed_primary_callback ==
                image + profile->category8_materializer_rva &&
            observed_primary_metadata == 0 &&
            observed_secondary_callback ==
                image + profile->category8_extra_callback_rva &&
            observed_secondary_metadata == 0;
        if (exact) {
            *primary_callback = reinterpret_cast<std::uintptr_t>(
                &tab_content_category8_materializer);
            if (g_reusable_route != native_graphics_route()) {
                // Selector 11 invokes this callback after row materialization.
                // A custom route must use the generic post-init branch.
                *secondary_callback = 0;
                *secondary_metadata = 0;
                secondary_zeroed = true;
            }
            replaced = true;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_tab_content_route_fault("SEH reading callback locals");
        return;
    }
#endif
    if (!replaced) {
        const std::uint32_t event =
            g_tab_content_route_fault_log_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        if (event <= 8) {
            erui::detail::logf(erui::LogLevel::error,
                "Native-tab Graphics-carrier route fault[%u]: unexpected selector/callback stack-local contract observed={selector:%u primarySlot:%p secondarySlot:%p enabled:%u reserved:%u primary:%p/0x%08X secondary:%p/0x%08X}",
                static_cast<unsigned>(event),
                static_cast<unsigned>(observed_selector),
                reinterpret_cast<void*>(observed_primary_slot),
                reinterpret_cast<void*>(observed_secondary_slot),
                static_cast<unsigned>(observed_enabled),
                static_cast<unsigned>(observed_reserved),
                reinterpret_cast<void*>(observed_primary_callback),
                static_cast<unsigned>(observed_primary_metadata),
                reinterpret_cast<void*>(observed_secondary_callback),
                static_cast<unsigned>(observed_secondary_metadata));
        }
        return;
    }
    const std::uint32_t event =
        g_tab_content_route_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event <= 32) {
        erui::detail::logf(erui::LogLevel::trace,
            "Native-tab Graphics route begin[%u]: route=%zu kind=%s selector=11 primary=host-thunk secondary=%s",
            static_cast<unsigned>(event), g_reusable_route,
            g_reusable_route == native_graphics_route()
                ? "native-graphics" : "custom",
            secondary_zeroed ? "zero/generic-post-init" :
                "native-authored");
    }
}

void materialize_native_graphics(void* page) noexcept {
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
        g_addresses.game_image_base);
    const TabCarrierBuildProfile* const profile =
        g_tab_carrier_build_profile;
    if (!page || !image || !profile ||
        profile->category8_materializer_rva >=
            g_addresses.game_image_size) return;
    reinterpret_cast<BuiltinPanelMaterializerFn>(
        image + profile->category8_materializer_rva)(
            page, static_cast<std::byte*>(page) + 0x1B38);
}

void __fastcall tab_content_category8_materializer(
    void* page,
    void* untrusted_argument2) noexcept {
    (void)untrusted_argument2;
    if (!page || g_reusable_failed.load(std::memory_order_acquire) ||
        g_tab_carrier.committed) {
        reusable_fault(
            "materialization occurred without exact prior Graphics retirement");
        return;
    }
    if (g_tab_carrier_epoch == 0) g_tab_carrier_epoch = 1;

    const BuiltinPanelSnapshot before = inspect_builtin_panel(page);
    const TabCarrierIdentity initial = inspect_tab_carrier(
        page, 8, erui::detail::invalid_compiled_index);
    if (!before.valid || before.native_count != 0 || !initial.valid) {
        reusable_fault("fresh Graphics panel failed pre-mutation identity");
        return;
    }

    if (g_reusable_route == native_graphics_route()) {
        materialize_native_graphics(page);
        const BuiltinPanelSnapshot after = inspect_builtin_panel(page);
        TabCarrierIdentity identity = inspect_tab_carrier(
            page, 8, erui::detail::invalid_compiled_index);
        const bool committed = after.valid && after.native_count > 0 &&
            after.native_count <= after.visual_capacity && identity.valid &&
            identity.wrapper == initial.wrapper &&
            identity.payload == initial.payload;
        if (committed) {
            identity.committed = true;
            g_tab_carrier = identity;
        }
        erui::detail::logf(
            committed ? erui::LogLevel::trace : erui::LogLevel::error,
            "Native-tab Graphics carrier construct: epoch=%llu route=%zu kind=native-graphics page=%p wrapper=%p payload=%p before=%llu after=%llu visualRows=%u committed=%d extraCallback=native",
            static_cast<unsigned long long>(g_tab_carrier_epoch),
            g_reusable_route,
            page, identity.wrapper, identity.payload,
            static_cast<unsigned long long>(before.native_count),
            static_cast<unsigned long long>(after.native_count),
            static_cast<unsigned>(after.visual_capacity),
            committed ? 1 : 0);
        if (!committed) reusable_fault("native Graphics restoration failed");
        return;
    }

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.options.enable_row_injection || !runtime.menu ||
        runtime.menu->tab_page_indices.empty() ||
        g_reusable_route >= runtime.menu->tab_page_indices.size()) {
        reusable_fault("custom Graphics route had no compiled tab page");
        return;
    }
    const std::size_t logical_page_index =
        runtime.menu->tab_page_indices[g_reusable_route];
    if (logical_page_index >= runtime.menu->pages.size() ||
        runtime.menu->pages[logical_page_index].kind != erui::PageKind::tab) {
        reusable_fault("custom Graphics route resolved invalid tab page");
        return;
    }
    const std::size_t logical_rows =
        runtime.menu->pages[logical_page_index].rows.size();
    const erui::detail::PageRoute route =
        erui::detail::PageRoute::tab_main(
            logical_page_index,
            static_cast<std::uint8_t>(before.visual_capacity));
    erui::detail::PageSlice expected_slice{};
    if (!resolve_page_route_slice(*runtime.menu, route, expected_slice)) {
        reusable_fault("custom Graphics route slice could not resolve");
        return;
    }
    const std::size_t expected_native_rows = expected_slice.row_count +
        (expected_slice.has_previous ? 1u : 0u) +
        (expected_slice.has_next ? 1u : 0u);
    if (expected_native_rows > before.visual_capacity) {
        reusable_fault("custom rows exceeded Graphics capacity");
        return;
    }
    const RowInjectionOutcome outcome = inject_page_route(
        page, route, g_addresses);
    const BuiltinPanelSnapshot after = inspect_builtin_panel(page);
    TabCarrierIdentity identity = inspect_tab_carrier(
        page, 8, logical_page_index);
    const bool identity_stable = identity.valid &&
        identity.wrapper == initial.wrapper &&
        identity.payload == initial.payload;
    const bool committed = after.valid && outcome.faulted == 0 &&
        outcome.skipped == 0 && outcome.attempted == expected_native_rows &&
        outcome.added == expected_native_rows &&
        after.native_count == expected_native_rows &&
        after.native_count <= after.visual_capacity && identity_stable;
    if (committed) {
        identity.committed = true;
        g_tab_carrier = identity;
        // Publish dialog/Back ownership only after the carrier identity and
        // complete row injection have committed. A failed materialization
        // must never leave input routed to an unowned category-8 page.
        g_dialog_input_page.store(page, std::memory_order_release);
    }
    erui::detail::logf(
        committed ? erui::LogLevel::trace : erui::LogLevel::error,
        "Native-tab Graphics carrier construct: epoch=%llu route=%zu kind=custom page=%p wrapper=%p payload=%p logicalPage=%zu before=%llu after=%llu visualRows=%u logicalRows=%zu attempted=%u added=%u faulted=%u skipped=%u identityStable=%d committed=%d extraCallback=zero/generic-post-init",
        static_cast<unsigned long long>(g_tab_carrier_epoch),
        g_reusable_route, page, identity.wrapper, identity.payload,
        logical_page_index,
        static_cast<unsigned long long>(before.native_count),
        static_cast<unsigned long long>(after.native_count),
        static_cast<unsigned>(before.visual_capacity), logical_rows,
        static_cast<unsigned>(outcome.attempted),
        static_cast<unsigned>(outcome.added),
        static_cast<unsigned>(outcome.faulted),
        static_cast<unsigned>(outcome.skipped),
        identity_stable ? 1 : 0, committed ? 1 : 0);
    if (!committed) reusable_fault("custom Graphics materialization failed");
}

void __fastcall tab_content_composite_teardown_detour(
    void* composite) noexcept {
    const auto original = g_original_tab_content_composite_teardown.load(
        std::memory_order_acquire);
    if (!original) return;
    if (!g_tab_content_callbacks_armed.load(std::memory_order_acquire)) {
        original(composite);
        return;
    }
    TabCarrierIdentity category8{};
    bool slot_read{};
    if (composite) {
#if defined(_MSC_VER)
        __try {
#endif
            void* const panel = *reinterpret_cast<void**>(
                static_cast<std::byte*>(composite) + 0x68 + 8 * 8);
            category8 = inspect_tab_carrier(
                panel,
                8, g_tab_carrier.logical_page_index);
            slot_read = true;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    }
    const bool tracked = g_tab_carrier.committed;
    const bool exact_tracked = tracked && slot_read && category8.valid &&
        composite == g_reusable_composite &&
        category8.panel == g_tab_carrier.panel &&
        category8.wrapper == g_tab_carrier.wrapper &&
        category8.payload == g_tab_carrier.payload;
    const bool clean_empty = !tracked && slot_read && !category8.panel &&
        (!g_reusable_composite || composite == g_reusable_composite);
    if (category8.panel) {
        clear_retired_page_references(category8.panel);
    }
    original(composite);
    bool cleared{};
#if defined(_MSC_VER)
    __try {
#endif
        cleared = composite && !*reinterpret_cast<void**>(
            static_cast<std::byte*>(composite) + 0x68 + 8 * 8);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    const bool exact_teardown = cleared &&
        (exact_tracked || clean_empty);
    erui::detail::logf(
        exact_teardown ? erui::LogLevel::trace : erui::LogLevel::error,
        "Native-tab Graphics carrier native teardown: epoch=%llu composite=%p route=%zu category8=%p/%p/%p tracked=%d exactTracked=%d cleanEmpty=%d slot8Null=%d networkRetiredByHost=0",
        static_cast<unsigned long long>(g_tab_carrier_epoch),
        composite, g_reusable_route, category8.panel, category8.wrapper,
        category8.payload, tracked ? 1 : 0, exact_tracked ? 1 : 0,
        clean_empty ? 1 : 0, cleared ? 1 : 0);
    if (exact_teardown) {
        g_tab_carrier = {};
        g_reusable_route = native_graphics_route();
        g_reusable_composite = nullptr;
        g_reusable_activation_frame = 0;
        // Exact native destruction is the sole recovery boundary for a
        // sticky carrier fault. Until this point category 8 may still cache a
        // custom or partially constructed panel and must not be treated as
        // native Graphics.
        g_reusable_failed.store(false, std::memory_order_release);
        g_tab_content_teardown_generation.fetch_add(
            1, std::memory_order_acq_rel);
        ++g_tab_carrier_epoch;
        if (g_tab_carrier_epoch == 0) {
            g_tab_carrier_epoch = 1;
        }
    } else {
        (void)reusable_fault(
            "native teardown did not match the tracked Graphics identity; retained diagnostics were not reset");
    }
}

#endif

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

bool resolve_submenu_title_override(
    const erui::detail::PageRoute& route,
    std::size_t slice_count,
    std::array<wchar_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY>& buffer,
    std::uint64_t& revision) noexcept {
    revision = 0;
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.menu ||
        route.logical_page_index >= runtime.menu->pages.size()) {
        return false;
    }
    if (route.kind == erui::detail::PageRouteKind::tab_main ||
        route.kind == erui::detail::PageRouteKind::tab_continuation) {
        return erui::detail::format_applied_tab_page_title(
            route.logical_page_index,
            route.slice_index,
            slice_count,
            buffer.data(),
            buffer.size(),
            revision);
    }
    if (route.kind != erui::detail::PageRouteKind::submenu) return false;
    const erui::detail::CompiledPage& page =
        runtime.menu->pages[route.logical_page_index];
    if (page.submenu_update_slot == erui::detail::invalid_compiled_index) {
        return false;
    }
    return erui::detail::format_submenu_page_title(
        page.submenu_update_slot,
        route.slice_index,
        slice_count,
        buffer.data(),
        buffer.size(),
        revision);
}

ActiveSubmenuTitleBinding* active_submenu_title_binding(
    void* page) noexcept {
    if (!page) return nullptr;
    for (ActiveSubmenuTitleBinding& binding : g_active_submenu_titles) {
        if (binding.page == page) return &binding;
    }
    return nullptr;
}

void bind_active_submenu_title(
    void* page,
    const CapturedTitleTarget& target,
    const erui::detail::PageRoute& route,
    std::size_t slice_count,
    std::uint64_t applied_revision) noexcept {
    const bool live_title_route =
        route.kind == erui::detail::PageRouteKind::submenu ||
        route.kind == erui::detail::PageRouteKind::tab_main ||
        route.kind == erui::detail::PageRouteKind::tab_continuation;
    if (!page || !live_title_route ||
        slice_count == 0) {
        retire_active_submenu_title(page);
        return;
    }
    if (!target.owned_by(page)) {
        // A bound page may revisit subHandler without another construction-
        // time path capture. Preserve the validated page-owned result from
        // its first materialization and only refresh route topology.
        if (ActiveSubmenuTitleBinding* existing =
                active_submenu_title_binding(page)) {
            existing->route = route;
            existing->slice_count = slice_count;
        }
        return;
    }
    ActiveSubmenuTitleBinding* destination =
        active_submenu_title_binding(page);
    if (!destination) {
        for (ActiveSubmenuTitleBinding& candidate : g_active_submenu_titles) {
            if (!candidate.page) {
                destination = &candidate;
                break;
            }
        }
    }
    if (!destination) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Live submenu-title binding capacity exhausted; current page keeps its construction title");
        return;
    }
    *destination = {
        .page = page,
        .target = target,
        .route = route,
        .slice_count = slice_count,
        .applied_revision = applied_revision,
    };
}

void apply_active_submenu_title(void* page) noexcept {
    ActiveSubmenuTitleBinding* const binding =
        active_submenu_title_binding(page);
    if (!binding || !binding->target.owned_by(page) ||
        binding->slice_count == 0 || !g_addresses.scaleform_text_setter) {
        return;
    }
    std::array<wchar_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY> title{};
    std::uint64_t revision{};
    if (!resolve_submenu_title_override(
            binding->route, binding->slice_count, title, revision) ||
        revision == 0 || revision == binding->applied_revision) {
        return;
    }
    unsigned long exception_code = 0;
    if (!invoke_scaleform_text_setter(
            g_addresses.scaleform_text_setter,
            binding->target.text_value(),
            title.data(),
            exception_code)) {
        log_title_fault("live submenu-title assignment", exception_code);
        return;
    }
    binding->applied_revision = revision;
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
    case erui::detail::PageRouteKind::tab_continuation:
        return "tab-continuation";
    case erui::detail::PageRouteKind::tab_main:
        return "tab-main";
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

    if (previous_page && previous_page != page) {
        retire_active_submenu_title(const_cast<void*>(previous_page));
        retire_native_row_visibility_page(
            const_cast<void*>(previous_page));
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
            outcome.faulted == 0 ? erui::LogLevel::trace : erui::LogLevel::error,
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
                route.logical_page_index) ||
        (route.kind ==
                erui::detail::PageRouteKind::tab_continuation &&
            compiled_page.kind == erui::PageKind::tab);
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
        runtime.menu->resolve_presentation(route, outcome.slice_count);
    const wchar_t* const outer_title = runtime.menu->texts.lookup(
        static_cast<int>(presentation.outer_title_id));
    const wchar_t* page_title = runtime.menu->texts.lookup(
        static_cast<int>(presentation.page_title_id));
    std::array<wchar_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY>
        updated_page_title{};
    std::uint64_t submenu_title_revision{};
    if (resolve_submenu_title_override(
            route,
            outcome.slice_count,
            updated_page_title,
            submenu_title_revision)) {
        page_title = updated_page_title.data();
    }
    apply_physical_page_titles(captured_title, outer_title, page_title);
    bind_active_submenu_title(
        page,
        captured_title,
        route,
        outcome.slice_count,
        submenu_title_revision);
    bind_native_row_visibility_title_target(page, captured_title);
    const std::uint32_t log_limit = runtime.options.enable_diagnostics ? 192u : 8u;
    if (injection <= log_limit || outcome.faulted != 0) {
        erui::detail::logf(
            outcome.faulted == 0 ? erui::LogLevel::trace : erui::LogLevel::error,
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

    std::uint8_t visibility_suppressed_input{};
    const bool visibility_rebuild_pending =
        prepare_native_row_visibility_page_frame(
            page,
            input_enabled && *input_enabled != 0,
            g_addresses);
    // A visibility batch racing the prepare call remains wholly queued; an
    // ordinary presentation-only batch can still publish on this frame.
    (void)erui::detail::synchronize_pending_row_updates_if_visibility_idle();
    apply_native_row_availability_pre_frame(page, g_addresses);
    std::uint8_t* const effective_input = visibility_rebuild_pending
        ? &visibility_suppressed_input
        : input_enabled;

    PageFrameFn original = g_original_page_frame.load(std::memory_order_acquire);
    if (!original) return;
    if (native_dialog_owns_menu_input() &&
        page == g_dialog_input_page.load(std::memory_order_acquire)) {
        // The native page frame is intentionally paused while the dialog owns
        // input, so there is no native presentation pass to overwrite us.
        (void)apply_native_row_presentations(page, g_addresses);
        apply_active_submenu_title(page);
        return;
    }
    original(page, frame_value, effective_input);

    // Action controllers republish their cached caption during the native
    // frame pass. Apply client presentation afterward so Button/Submenu-style
    // rows cannot immediately overwrite an accepted label edit. Every target
    // is identity-validated again inside the bridge before it is touched.
    const SelectedHelpRefreshTicket selected_help =
        apply_native_row_presentations(page, g_addresses);
    apply_active_submenu_title(page);
    observe_native_row_visibility_page_frame(page, g_addresses);
    if (!visibility_rebuild_pending) {
        publish_native_selected_help(selected_help, g_addresses);
    }
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
    if (restored_parent) {
        retire_active_submenu_title(page);
        retire_native_row_visibility_page(page);
    }
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

bool revalidate_shared_menu_entry_before_hook(
    const ModuleView* game,
    const PreservedSharedMenuAddresses* preserved,
    PreservedSharedMenuEntry entry,
    const char* name) noexcept {
    if (!preserved) return true;
    if (game && revalidate_preserved_shared_menu_entry(
            *game, *preserved, entry)) {
        return true;
    }
    erui::detail::logf(
        erui::LogLevel::error,
        "Approved shared menu entry '%s' changed immediately before hook creation",
        name ? name : "unknown");
    return false;
}

bool install_game_options_hook(
    const GameAddresses& addresses,
    const ModuleView* game,
    const PreservedSharedMenuAddresses* preserved) noexcept {
    if (erui::detail::diagnostics_enabled()) {
        const auto* code = reinterpret_cast<const unsigned char*>(
            addresses.game_options_handler);
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: Game Options entry before hook=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7],
            code[8], code[9], code[10], code[11], code[12], code[13], code[14], code[15]);
    }
    if (!revalidate_shared_menu_entry_before_hook(
            game,
            preserved,
            PreservedSharedMenuEntry::game_options,
            "Game Options")) {
        return false;
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

#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
template <std::size_t Size>
bool tab_content_bytes_match(
    const std::uint8_t* address,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    if (!address) return false;
#if defined(_MSC_VER)
    __try {
#endif
        return std::memcmp(address, expected.data(), expected.size()) == 0;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool tab_carrier_signatures_match(
    const GameAddresses& addresses,
    const TabCarrierBuildProfile** matched_profile = nullptr) noexcept {
    if (matched_profile) {
        *matched_profile = nullptr;
    }
    // Category 8 (Graphics). Native Graphics preserves
    // both callbacks. Each 16-byte descriptor contains an eight-byte callback,
    // four-byte metadata field and four bytes of padding. A validated custom
    // route substitutes the callback at stack+0x60 and clears only the
    // authored callback/metadata fields at stack+0x50.
    constexpr std::array<std::uint8_t, 7> extra_callback{
        0x48, 0x8D, 0x05, 0xF9, 0xD8, 0x01, 0x00};
    constexpr std::array<std::uint8_t, 7> row_callback{
        0x48, 0x8D, 0x05, 0x17, 0xF1, 0x01, 0x00};
    constexpr std::array<std::uint8_t, 13> builder{
        0x48, 0x8D, 0x8C, 0x24, 0x80, 0x00, 0x00, 0x00,
        0xE8, 0xCB, 0x04, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 23> route{
        0xC6, 0x44, 0x24, 0x20, 0x0B,
        0x44, 0x0F, 0xB6, 0xCF, 0x4C, 0x8B, 0xC3,
        0x48, 0x8B, 0xD0, 0x48, 0x8B, 0xCE,
        0xE8, 0x95, 0x2D, 0x02, 0x00};
    constexpr std::array<std::uint8_t, 16> teardown_entry{
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57,
        0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x30};
    constexpr std::array<std::uint8_t, 16> deactivate_entry{
        0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,
        0xEC,0x20,0x8B,0xDA,0x48,0x8B,0xF9,0xE8};
    if (!addresses.game_image_base) {
        return false;
    }

    bool exact_image{};
    const TabCarrierBuildProfile* profile{};
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(
            addresses.game_image_base);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 &&
            static_cast<std::size_t>(dos->e_lfanew) +
                    sizeof(IMAGE_NT_HEADERS64) <= addresses.game_image_size) {
            const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
                addresses.game_image_base + dos->e_lfanew);
            const bool valid_pe = nt->Signature == IMAGE_NT_SIGNATURE &&
                nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
                nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
                nt->OptionalHeader.SizeOfImage == addresses.game_image_size;
            if (valid_pe) {
                profile = find_tab_carrier_build_profile(
                    nt->FileHeader.TimeDateStamp,
                    nt->OptionalHeader.SizeOfImage);
                exact_image = profile != nullptr;
            }
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        exact_image = false;
    }
#endif
    if (!exact_image || !profile ||
        reinterpret_cast<void*>(addresses.graphics_panel_materializer) !=
            addresses.game_image_base +
                profile->category8_materializer_rva ||
        !tab_content_bytes_match(
            addresses.game_image_base +
                profile->extra_callback_anchor_rva,
            extra_callback) ||
        !tab_content_bytes_match(
            addresses.game_image_base + profile->row_callback_anchor_rva,
            row_callback) ||
        !tab_content_bytes_match(
            addresses.game_image_base + profile->builder_anchor_rva,
            builder) ||
        !tab_content_bytes_match(
            addresses.game_image_base + profile->route_anchor_rva,
            route) ||
        !tab_content_bytes_match(
            addresses.game_image_base + profile->composite_teardown_rva,
            teardown_entry) ||
        !tab_content_bytes_match(
            addresses.game_image_base + profile->carrier_manager_rva,
            profile->manager_entry) ||
        !tab_content_bytes_match(
            addresses.game_image_base + profile->carrier_deactivate_rva,
            deactivate_entry)) {
        return false;
    }
    if (matched_profile) {
        *matched_profile = profile;
    }
    return true;
}

bool install_tab_content_carrier_hooks(
    const GameAddresses& addresses) noexcept {
    if (g_tab_content_hooks_exposed.load(std::memory_order_acquire)) {
        erui::detail::logf(erui::LogLevel::error,
            "Native-tab Graphics-carrier hooks were already exposed; refusing a second installation attempt");
        return false;
    }
    const TabCarrierBuildProfile* profile{};
    if (!tab_carrier_signatures_match(addresses, &profile)) {
        erui::detail::logf(erui::LogLevel::error,
            "Native-tab Graphics-carrier build/signatures differed; no carrier hooks installed");
        return false;
    }
    if (!profile) return false;

    auto pre8 = SafetyHookMid::create(
        addresses.game_image_base + profile->category8_pre_rva,
        &begin_tab_content_category8_route,
        SafetyHookMid::StartDisabled);
    if (!pre8) return false;
    g_tab_content_category8_pre_hook = std::move(*pre8);

    auto teardown = SafetyHookInline::create(
        addresses.game_image_base + profile->composite_teardown_rva,
        reinterpret_cast<void*>(&tab_content_composite_teardown_detour),
        SafetyHookInline::StartDisabled);
    if (!teardown) {
        g_tab_content_category8_pre_hook.reset();
        return false;
    }
    g_tab_content_composite_teardown_hook = std::move(*teardown);
    g_original_tab_content_composite_teardown.store(
        g_tab_content_composite_teardown_hook
            .original<TabCarrierCompositeTeardownFn>(),
        std::memory_order_release);

    // A callback may enter as soon as the first patch is published. From this
    // point forward the hook objects and original trampoline are retained for
    // the process lifetime. Until both patches are known enabled, the inline
    // detour is exact pass-through and the mid hook is a no-op.
    g_tab_carrier_build_profile = profile;
    g_tab_content_callbacks_armed.store(false, std::memory_order_release);
    g_tab_content_hooks_exposed.store(true, std::memory_order_release);
    if (!g_tab_content_composite_teardown_hook.enable()) {
        erui::detail::logf(erui::LogLevel::error,
            "Native-tab Graphics-carrier teardown hook could not be enabled; retaining the unarmed process-lifetime trampoline");
        return false;
    }
    if (!g_tab_content_category8_pre_hook.enable()) {
        erui::detail::logf(erui::LogLevel::error,
            "Native-tab Graphics-carrier pre-hook could not be enabled; retaining unarmed process-lifetime hooks");
        return false;
    }
    g_tab_content_callbacks_armed.store(true, std::memory_order_release);

    erui::detail::logf(erui::LogLevel::info,
        "Native-tab Graphics carrier installed category=8 selector=11 pre=+0x%llX nativeRows=+0x%llX extraCallback=+0x%llX policy={native:preserve custom:validated-zero} teardown=+0x%llX parking=stock-category5 synchronous=1",
        static_cast<unsigned long long>(profile->category8_pre_rva),
        static_cast<unsigned long long>(
            profile->category8_materializer_rva),
        static_cast<unsigned long long>(
            profile->category8_extra_callback_rva),
        static_cast<unsigned long long>(
            profile->composite_teardown_rva));
    return true;
}

#endif

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

bool install_sub_hook(
    const GameAddresses& addresses,
    const ModuleView* game,
    const PreservedSharedMenuAddresses* preserved) noexcept {
    if (erui::detail::diagnostics_enabled()) {
        const auto* code = reinterpret_cast<const unsigned char*>(addresses.sub_handler);
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: subpage entry before hook=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7],
            code[8], code[9], code[10], code[11], code[12], code[13], code[14], code[15]);
    }
    if (!revalidate_shared_menu_entry_before_hook(
            game,
            preserved,
            PreservedSharedMenuEntry::sub_handler,
            "Game Options subpage")) {
        return false;
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

TextHookInstallStatus install_text_hook(
    const GameAddresses& addresses,
    const ModuleView* game,
    const PreservedSharedMenuAddresses* preserved) noexcept {
    const MH_STATUS initialize_status = MH_Initialize();
    if (initialize_status != MH_OK && initialize_status != MH_ERROR_ALREADY_INITIALIZED) {
        erui::detail::logf(
            erui::LogLevel::error,
            "MinHook initialization failed: %s (%d)",
            minhook_status_name(initialize_status),
            static_cast<int>(initialize_status));
        return TextHookInstallStatus::hook_failed;
    }

    if (!revalidate_shared_menu_entry_before_hook(
            game,
            preserved,
            PreservedSharedMenuEntry::text_resolver,
            "native text resolver")) {
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
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    if (g_tab_content_hooks_exposed.load(std::memory_order_acquire)) {
        // Game callbacks do not participate in a host-side quiescence
        // protocol. Resetting any part of this interdependent hook graph after
        // carrier exposure could invalidate an in-flight trampoline or state
        // it can reach, so the complete runtime remains pinned.
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native UI hook reset refused after Graphics-carrier exposure; the complete runtime remains process-lifetime");
        return;
    }
#endif
    remove_native_slider_presentation();
    reset_native_row_visibility();
    reset_native_row_presentations(g_addresses);
    set_color_picker_widget_path_bridge_available(false);
    {
        std::scoped_lock lock{g_scaleform_path_bridge_mutex};
        // Invalidate the ownership proof before restoring executable bytes so
        // a concurrent validator can never mistake a tearing-down hook for a
        // live host-owned bridge.
        g_scaleform_path_bridge_proven = false;
        g_scaleform_path_bridge_target = nullptr;
        g_scaleform_path_bridge_patch = {};
        g_scaleform_path_resolver_hook.reset();
    }
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    g_tab_content_category8_pre_hook.reset();
    g_tab_content_composite_teardown_hook.reset();
    g_original_tab_content_composite_teardown.store(
        nullptr, std::memory_order_release);
    g_tab_carrier = {};
    g_reusable_route = erui::detail::invalid_compiled_index;
    g_reusable_composite = nullptr;
    g_reusable_activation_frame = 0;
    g_reusable_failed.store(false, std::memory_order_release);
    g_tab_carrier_epoch = 0;
    g_tab_content_route_log_count.store(0, std::memory_order_release);
    g_tab_content_route_fault_log_count.store(0, std::memory_order_release);
    g_tab_content_callbacks_armed.store(false, std::memory_order_release);
    g_tab_carrier_build_profile = nullptr;
#endif
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
    g_active_submenu_titles = {};
}

} // namespace

std::uint32_t native_tab_graphics_carrier_version() noexcept {
    return graphics_carrier_version_v2_impl();
}

bool prepare_native_tab_graphics_carrier(
    void* composite,
    std::size_t route,
    std::uint64_t completed_frame,
    std::int32_t* category,
    NativeTabParkingSwitchFn park_native) noexcept {
    return prepare_graphics_carrier_v2_impl(
        composite, route, completed_frame, category, park_native);
}

bool verify_native_tab_graphics_carrier(
    void* composite,
    std::size_t route,
    std::int32_t category) noexcept {
    return verify_graphics_carrier_v2_impl(composite, route, category);
}

#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)
// The standalone viewport research DLL predates the in-process production
// bridge and resolves these names dynamically. Keep that compatibility only
// in an explicitly opted-in research host; release hosts do not export them.
extern "C" __declspec(dllexport) std::uint32_t __cdecl
ERUI_ResearchGraphicsCarrierVersionV2() noexcept {
    return native_tab_graphics_carrier_version();
}

extern "C" __declspec(dllexport) bool __cdecl
ERUI_ResearchPrepareGraphicsCarrierV2(
    void* composite,
    std::size_t route,
    std::uint64_t completed_frame,
    std::int32_t* category,
    NativeTabParkingSwitchFn park_native) noexcept {
    return prepare_native_tab_graphics_carrier(
        composite, route, completed_frame, category, park_native);
}

extern "C" __declspec(dllexport) bool __cdecl
ERUI_ResearchVerifyGraphicsCarrierV2(
    void* composite,
    std::size_t route,
    std::int32_t category) noexcept {
    return verify_native_tab_graphics_carrier(composite, route, category);
}
#endif

bool preflight_native_tab_carrier(const GameAddresses& addresses) noexcept {
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    const TabCarrierBuildProfile* matched_profile{};
    return tab_carrier_signatures_match(addresses, &matched_profile) &&
        matched_profile != nullptr;
#else
    (void)addresses;
    return false;
#endif
}

bool native_tab_graphics_carrier_installed() noexcept {
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    return g_tab_content_callbacks_armed.load(std::memory_order_acquire) &&
        !g_reusable_failed.load(std::memory_order_acquire) &&
        g_tab_content_category8_pre_hook.enabled() &&
        g_tab_content_composite_teardown_hook.enabled() &&
        g_original_tab_content_composite_teardown.load(
            std::memory_order_acquire) != nullptr;
#else
    return false;
#endif
}

bool validate_native_row_visibility_tab_carrier(
    void* page,
    std::size_t logical_page_index) noexcept {
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    if (!page || !g_tab_carrier.committed ||
        g_tab_carrier.logical_page_index != logical_page_index ||
        g_tab_carrier.panel != page) {
        return false;
    }
    const TabCarrierIdentity current = inspect_tab_carrier(
        page, 8, logical_page_index);
    return current.valid &&
        current.panel == g_tab_carrier.panel &&
        current.wrapper == g_tab_carrier.wrapper &&
        current.payload == g_tab_carrier.payload;
#else
    (void)page;
    (void)logical_page_index;
    return false;
#endif
}

void reapply_native_row_visibility_page_title(
    void* page,
    const CapturedTitleTarget& captured,
    const erui::detail::PageRoute& route,
    std::size_t slice_count) noexcept {
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!captured.owned_by(page) || captured.route != route ||
        !runtime.menu || slice_count == 0 ||
        !g_addresses.scaleform_text_setter) {
        return;
    }
    const erui::detail::ResolvedPagePresentation presentation =
        runtime.menu->resolve_presentation(route, slice_count);
    const wchar_t* page_title = runtime.menu->texts.lookup(
        static_cast<int>(presentation.page_title_id));
    std::array<wchar_t, ERUI_PAGE_TITLE_BUFFER_CAPACITY>
        updated_page_title{};
    std::uint64_t submenu_title_revision{};
    if (resolve_submenu_title_override(
            route,
            slice_count,
            updated_page_title,
            submenu_title_revision)) {
        page_title = updated_page_title.data();
    }
    if (!page_title || !*page_title) return;

    // The outer title is invariant across visible slice counts. Reusing only
    // the exact page-owned persistent result avoids retaining or dereferencing
    // the construction-time movie context after the handler returns.
    unsigned long exception_code = 0;
    if (!invoke_scaleform_text_setter(
            g_addresses.scaleform_text_setter,
            captured.text_value(),
            page_title,
            exception_code)) {
        log_title_fault("visibility page-title assignment", exception_code);
        return;
    }
    if (ActiveSubmenuTitleBinding* binding =
            active_submenu_title_binding(page)) {
        binding->route = route;
        binding->slice_count = slice_count;
        binding->applied_revision = submenu_title_revision;
    }
}

bool native_scaleform_path_bridge_installed_at(
    const void* target) noexcept {
    std::scoped_lock lock{g_scaleform_path_bridge_mutex};
    if (!target || !g_scaleform_path_bridge_proven ||
        g_scaleform_path_bridge_target != target ||
        !g_scaleform_path_resolver_hook.enabled() ||
        g_scaleform_path_resolver_hook.target() != target ||
        g_scaleform_path_resolver_hook.destination() !=
            &observe_scaleform_path) {
        return false;
    }

    const auto& original =
        g_scaleform_path_resolver_hook.original_bytes();
    if (original.empty() ||
        original.size() > kScaleformPathResolverEntry.size() ||
        !std::equal(
            original.begin(), original.end(),
            kScaleformPathResolverEntry.begin())) {
        return false;
    }

    std::array<std::uint8_t, 16> live_patch{};
    return read_scaleform_path_entry(target, live_patch) &&
        live_patch == g_scaleform_path_bridge_patch;
}

bool native_tab_graphics_carrier_exposed() noexcept {
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    return g_tab_content_hooks_exposed.load(std::memory_order_acquire);
#else
    return false;
#endif
}

std::uint64_t native_tab_graphics_carrier_teardown_generation() noexcept {
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    return g_tab_content_teardown_generation.load(
        std::memory_order_acquire);
#else
    return 0;
#endif
}

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
        void* restored_page{};
        if (target.kind == erui::detail::PageRouteKind::root_main) {
            restored_page = g_last_page.load(std::memory_order_acquire);
        } else if (
            target.kind == erui::detail::PageRouteKind::builtin_main ||
            target.kind == erui::detail::PageRouteKind::tab_main) {
            restored_page = g_submenu_navigation.parent_page(parent_page);
        } else {
            restored_page = g_submenu_navigation.native_page(target);
        }
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
            invoked ? erui::LogLevel::trace : erui::LogLevel::error,
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
        erui::LogLevel::trace,
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

HookInstallStatus install_native_menu_hooks(
    const GameAddresses& addresses,
    const PreservedSharedMenuAddresses* preserved_shared,
    bool enable_row_presentations) noexcept {
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

    ModuleView game{};
    if (preserved_shared &&
        !game.initialize(GetModuleHandleW(nullptr))) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Could not initialize the game image for per-entry shared-menu revalidation");
        return HookInstallStatus::hook_failed;
    }

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const bool enable_rows = runtime.options.enable_row_injection;
    const bool enable_text = runtime.options.enable_custom_text;
    const bool root_overflow = runtime.menu &&
        runtime.menu->root_plan(erui::detail::game_options_min_plan_capacity)
            .pages.slices.size() > 1;
    bool has_builtin_rows = false;
    bool has_tab_overflow = false;
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
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
        for (const std::size_t page_index :
             runtime.menu->tab_page_indices) {
            if (page_index < runtime.menu->pages.size() &&
                runtime.menu->pages[page_index].rows.size() >
                    kTabCarrierMinimumPlanCapacity) {
                has_tab_overflow = true;
                break;
            }
        }
#endif
    }
    const bool enable_submenus = enable_rows && runtime.menu &&
        (runtime.menu->modeled_submenu_count != 0 || root_overflow ||
            has_builtin_rows || has_tab_overflow);
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
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
        // Custom-tab main panels already exist in the Configuration cache;
        // reserve bindings only for their native continuation subpages.
        for (const std::size_t page_index :
             runtime.menu->tab_page_indices) {
            if (page_index >= runtime.menu->pages.size()) continue;
            erui::detail::PageSlice first{};
            if (erui::detail::resolve_paginated_slice(
                    page_index,
                    runtime.menu->pages[page_index].rows.size(),
                    kTabCarrierMinimumPlanCapacity,
                    erui::detail::native_subpage_capacity,
                    0,
                    first) && first.slice_count > 1) {
                binding_capacity += first.slice_count - 1;
            }
        }
#endif
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
            !install_game_options_hook(
                addresses, &game, preserved_shared)) {
            reset_hooks();
            return HookInstallStatus::hook_failed;
        }
        // Install the ordinary Graphics materializer first. The reusable
        // carrier deliberately invokes the exact native Graphics entry, so a
        // registered provider's Graphics rows must already be in that chain.
        install_builtin_panel_materializers(addresses);
        if (enable_submenus) {
            if (!addresses.sub_handler || !addresses.open_sub_page ||
                !install_sub_hook(addresses, &game, preserved_shared)) {
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
            install_text_hook(addresses, &game, preserved_shared) !=
                TextHookInstallStatus::success) {
            reset_hooks();
            return HookInstallStatus::hook_failed;
        }
    } else {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Custom text hook disabled; rows use a vanilla fallback message ID");
    }

    if (!install_native_slider_presentation(
            addresses, &game, preserved_shared)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted-slider presentation could not be installed");
        reset_hooks();
        return HookInstallStatus::hook_failed;
    }

    // TextInput state may be changed from a client callback or the host
    // worker. Its native MenuString shadow is therefore synchronized at the
    // game's page-frame boundary. This frame hook is required independently
    // of the optional native-alert transport, which merely shares it as an
    // input gate when available.
    const bool require_row_presentations =
        enable_row_presentations &&
        erui::detail::editable_row_presentation_count() != 0;
    bool require_page_frame = enable_text_inputs || require_row_presentations;
    if (require_row_presentations &&
        !addresses.row_presentation_complete()) {
        erui::detail::logf(
            erui::LogLevel::error,
            "API 1.4 row updates require the validated OptionSetting and Scaleform text interfaces");
        reset_hooks();
        return HookInstallStatus::hook_failed;
    }
    if (require_page_frame && !install_page_frame_gate(addresses)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "TextInput/API 1.4 row updates require the page-frame UI pump");
        reset_hooks();
        return HookInstallStatus::hook_failed;
    }

    const bool dialog_input_ready = enable_text &&
        install_dialog_input_gates(addresses, require_page_frame);
    if (!enable_text) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport unavailable: custom text support is disabled");
    } else if (!dialog_input_ready) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport unavailable: modal input gates were not installed");
    } else if (!game.base() &&
        !game.initialize(GetModuleHandleW(nullptr))) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport unavailable: main executable could not be parsed");
        reset_dialog_input_gates(require_page_frame);
    } else if (!install_native_dialog_transport(game)) {
        // Alerts are an optional native route. A game update may invalidate
        // their semantic signatures without disabling registered menu rows.
        erui::detail::logf(
            erui::LogLevel::warning,
            "Native alert transport was not installed; menu rows remain active");
        reset_dialog_input_gates(require_page_frame);
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
    bool exact_path_ownership_required{};
#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    exact_path_ownership_required = runtime.menu &&
        !runtime.menu->tab_page_indices.empty();
#endif
    if (title_path_bridge_requested || color_widget_path_bridge_requested) {
        void* path_hook_target = reinterpret_cast<void*>(
            addresses.scaleform_path_resolver);
        if (!path_hook_target && color_picker_installed) {
            path_hook_target = color_picker_widget_path_hook_target();
        }
        if (path_hook_target) {
            std::array<std::uint8_t, 16> entry_before_hook{};
            const bool pristine_entry =
                read_scaleform_path_entry(
                    path_hook_target, entry_before_hook) &&
                entry_before_hook == kScaleformPathResolverEntry;
            if (exact_path_ownership_required && !pristine_entry) {
                erui::detail::logf(
                    erui::LogLevel::error,
                    "Shared Scaleform path target changed after native-tab preflight; refusing to chain or expose process-lifetime custom-tab hooks");
                reset_hooks();
                remove_native_dialog_transport();
                return HookInstallStatus::hook_failed;
            }
            auto path_result = SafetyHookMid::create(
                path_hook_target,
                &observe_scaleform_path);
            if (path_result) {
                bool exact_ownership{};
                {
                    std::scoped_lock lock{
                        g_scaleform_path_bridge_mutex};
                    g_scaleform_path_bridge_proven = false;
                    g_scaleform_path_bridge_target = nullptr;
                    g_scaleform_path_bridge_patch = {};
                    g_scaleform_path_resolver_hook =
                        std::move(*path_result);

                    const auto& original =
                        g_scaleform_path_resolver_hook.original_bytes();
                    std::array<std::uint8_t, 16> installed_patch{};
                    const bool original_matches = !original.empty() &&
                        original.size() <= entry_before_hook.size() &&
                        std::equal(
                            original.begin(), original.end(),
                            entry_before_hook.begin());
                    const bool patch_captured =
                        read_scaleform_path_entry(
                            path_hook_target, installed_patch) &&
                        installed_patch != entry_before_hook;
                    exact_ownership = pristine_entry &&
                        g_scaleform_path_resolver_hook.enabled() &&
                        g_scaleform_path_resolver_hook.target() ==
                            static_cast<std::uint8_t*>(path_hook_target) &&
                        g_scaleform_path_resolver_hook.destination() ==
                            &observe_scaleform_path &&
                        original_matches && patch_captured;
                    if (exact_ownership) {
                        g_scaleform_path_bridge_target = path_hook_target;
                        g_scaleform_path_bridge_patch = installed_patch;
                        g_scaleform_path_bridge_proven = true;
                    } else if (exact_path_ownership_required) {
                        // We are still before process-lifetime carrier
                        // exposure. Restore the pristine entry immediately;
                        // never pin an unproven shared hook.
                        g_scaleform_path_resolver_hook.reset();
                    }
                }
                if (exact_path_ownership_required && !exact_ownership) {
                    set_color_picker_widget_path_bridge_available(false);
                    erui::detail::logf(
                        erui::LogLevel::error,
                        "Shared Scaleform path bridge could not prove exact target ownership; custom-tab hooks were not exposed");
                    reset_hooks();
                    remove_native_dialog_transport();
                    return HookInstallStatus::hook_failed;
                }
                set_color_picker_widget_path_bridge_available(
                    color_picker_installed);
                erui::detail::logf(
                    erui::LogLevel::info,
                    "Shared Scaleform path bridge installed (titles=%d colorWidget=%d exactOwnership=%d)",
                    title_path_bridge_requested ? 1 : 0,
                    color_widget_path_bridge_requested ? 1 : 0,
                    exact_ownership ? 1 : 0);
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

#if defined(ER_NATIVE_UI_CUSTOM_TABS_BACKEND)
    // Expose the process-lifetime Graphics carrier only after every other
    // fallible menu-hook stage has completed. If either carrier enable is
    // ambiguous, its unarmed pass-through hooks and the complete runtime are
    // retained rather than rolling back executable trampolines in flight.
    if (runtime.menu && !runtime.menu->tab_page_indices.empty()) {
        g_reusable_route = native_graphics_route();
        if (!install_tab_content_carrier_hooks(addresses)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "Custom tabs require the validated category-8 selector-11 Graphics carrier");
            reset_hooks();
            return HookInstallStatus::hook_failed;
        }
    }
#endif

    erui::detail::logf(
        erui::LogLevel::info,
        "Native UI runtime active (rows=%d submenus=%d pagination=%d customText=%d diagnostics=%d cooldown=%u ms)",
        enable_rows ? 1 : 0,
        enable_submenus ? 1 : 0,
        runtime.menu && runtime.menu->pagination_required_for_capacity(
            erui::detail::game_options_min_plan_capacity) ? 1 : 0,
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
