#include "native_row_visibility.hpp"

#include "addresses.hpp"
#include "color_picker.hpp"
#include "game_build.hpp"
#include "game_build_profiles.hpp"
#include "hooks.hpp"
#include "native_dialog.hpp"
#include "native_menu.hpp"
#include "native_row_presentation.hpp"
#include "native_title_bridge.hpp"
#include "module.hpp"
#include "row_visibility_state.hpp"
#include "row_update_state.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace erui::native {
namespace {

constexpr std::size_t kVisualCapacityOffset = 0xB14;
constexpr std::size_t kNativeCountOffset = 0x1AF0;
constexpr std::size_t kSelectorOffset = 0xA38;
constexpr std::size_t kSelectorActiveOffset = 0xD0;
constexpr std::size_t kSelectorSelectedOffset = 0xD4;
constexpr std::size_t kSelectorAxis0Offset = 0xD8;
constexpr std::size_t kSelectorAxis1Offset = 0xDC;
constexpr std::size_t kSelectorAdapterOffset = 0x698;
constexpr std::size_t kSelectorBindingOffset = 0x720;
constexpr std::size_t kSelectorBindingSize = 0x40;
constexpr std::size_t kRowOwnerOffset = 0x1260;
constexpr std::size_t kRowStorageOffset = 0x1268;
constexpr std::size_t kRowStride = 0x88;
constexpr std::size_t kRowControllerOffset = 0x78;
constexpr std::size_t kRowEnabledOffset = 0x80;
constexpr std::size_t kActiveJobOffset = 0x10;
constexpr std::size_t kQueuedJobsOffset = 0x40;
constexpr std::size_t kComponentCountOffset = 0x98;
constexpr std::size_t kScrollbarControllerOffset = 0xBE0;
constexpr std::size_t kMaximumRows = 32;
constexpr std::size_t kMaximumPages = 64;

constexpr std::array<std::uint8_t, 16> kSetActivePrefix{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x85};
constexpr std::array<std::uint8_t, 16> kRowDestructorPrefix{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x8B, 0xDA, 0x48, 0x8B, 0xF9, 0xE8};
constexpr std::array<std::uint8_t, 24> kFullRefreshPrefix{
    0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81, 0xEC, 0x90,
    0x00, 0x00, 0x00, 0x48, 0xC7, 0x40, 0x88, 0xFE,
    0xFF, 0xFF, 0xFF, 0x48, 0x89, 0x58, 0x10, 0x48};
constexpr std::array<std::uint8_t, 16> kComponentStateSyncPrefix{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xEC, 0x20, 0x48, 0x8B, 0xF9, 0x48, 0x83, 0xC1};
constexpr std::array<std::uint8_t, 16> kScrollbarRefreshPrefix{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
    0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x8B};

using SelectorSetActiveFn = void(__fastcall*)(void*, std::uint32_t);
using RowDeletingDestructorFn = void*(__fastcall*)(void*, std::uint32_t);
using PageFullRefreshFn = void(__fastcall*)(void*, std::uint32_t);
using ComponentStateSyncFn = void(__fastcall*)(void*);
using ScrollbarRefreshFn = void(__fastcall*)(void*);

struct NativeProfile {
    EldenRingBuild build{};
    std::uint32_t timestamp{};
    std::size_t image_size{};
    std::uintptr_t set_active_rva{};
    std::uintptr_t full_refresh_rva{};
    std::uintptr_t row_destructor_rva{};
    std::uintptr_t component_state_sync_rva{};
    std::uintptr_t scrollbar_refresh_rva{};
    std::uintptr_t owner_vtable_rva{};
    std::uintptr_t selector_vtable_rva{};
};

// Destructive row retirement and in-place selector refresh have been live
// proved only on 2.7.2.0. Similar-looking 2.7.1.0 entries are deliberately
// not admitted until that complete lifecycle receives its own live matrix.
constexpr std::array kProfiles{
    NativeProfile{
        EldenRingBuild::v2_7_2_0,
        0x6AABF7E7,
        0x5DFD600,
        0x73A0C0,
        0x977F10,
        0x86BCC0,
        0x736D20,
        0x74EFC0,
        0x2B06EC0,
        0x2A96528,
    },
};

struct NativeEntries {
    const NativeProfile* profile{};
    SelectorSetActiveFn set_active{};
    PageFullRefreshFn full_refresh{};
    RowDeletingDestructorFn row_destructor{};
    ComponentStateSyncFn component_state_sync{};
    ScrollbarRefreshFn scrollbar_refresh{};
};

struct Snapshot {
    void* page_vtable{};
    void* owner_vtable{};
    void* selector_vtable{};
    void* selector_adapter{};
    std::array<std::byte, kSelectorBindingSize> selector_binding{};
    std::array<void*, kMaximumRows> row_vtables{};
    std::array<void*, kMaximumRows> row_controllers{};
    void* active_job{};
    std::uint64_t queued_jobs{};
    std::uint64_t component_count{};
    std::uint64_t count{};
    std::uint32_t capacity{};
    std::int32_t active{-1};
    std::int32_t selected{-1};
    std::int32_t axis0{-1};
    std::int32_t axis1{-1};
    unsigned long seh{};
    bool valid{};
};

enum class Phase : std::uint8_t {
    idle,
    settling,
    rebuilding,
    verifying,
    faulted,
};

struct PageBinding {
    void* page{};
    detail::PageRoute route{};
    std::uint64_t prefix_count{};
    std::uint64_t count_after{};
    std::uint64_t applied_revision{};
    std::uint64_t target_revision{};
    std::size_t selected_source{detail::invalid_compiled_index};
    std::size_t selected_projected{};
    std::size_t selected_prefix{detail::invalid_compiled_index};
    std::uint32_t stable_frames{};
    Phase phase{Phase::idle};
    Snapshot settle{};
    Snapshot expected{};
    CapturedTitleTarget title_target{};
    bool main_route{};
    bool active{};
};

std::recursive_mutex g_mutex{};
std::array<PageBinding, kMaximumPages> g_pages{};
thread_local void* g_internal_rebuild_page{};
thread_local bool g_internal_materialization_exact{};
std::uint32_t g_fault_logs{};
bool g_backend_faulted{};

#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)

// Observation-only API 1.4 research fixture. Nothing in this block writes a
// Scaleform property or a native selector field. The probe exists to identify
// the first lifecycle boundary at which GraphicOption/ScrollBarV and the
// selector-owned scroll range diverge during an in-place subpage rebuild.
constexpr std::size_t kProbeMovieContextOffset = 0x120;
constexpr std::size_t kProbeScrollbarPositionOffset = 0xD80;
constexpr std::size_t kProbeScrollbarTotalOffset = 0xD84;
constexpr std::size_t kProbeScrollbarPageSizeOffset = 0xD88;
constexpr std::size_t kProbeComponentValidCacheOffset = 0x68;
constexpr std::size_t kProbeComponentVisibleCacheOffset = 0x69;
constexpr std::size_t kProbePathResultSize = 0x60;
constexpr std::size_t kProbeMemberResultSize = 0x38;
constexpr std::size_t kProbeSceneProxyValueOffset = 0x28;
constexpr std::size_t kProbeScaleformInterfaceOffset = 0x18;
constexpr std::size_t kProbeScaleformFlagsOffset = 0x20;
constexpr std::size_t kProbeScaleformPayloadOffset = 0x28;
constexpr std::uint32_t kProbeScaleformKindMask = 0x8F;
constexpr std::uint32_t kProbeScaleformKindBoolean = 0x02;
constexpr std::uint32_t kProbeScaleformKindNumber = 0x05;
constexpr std::uint32_t kProbeScaleformKindDisplayObject = 0x0A;
constexpr std::uint32_t kProbeConvertBoolean = 0x82;
constexpr std::uint32_t kProbeConvertNumber = 0x85;
constexpr std::uintptr_t kProbeValueConstructorRva = 0xD82B90;
constexpr std::uintptr_t kProbeSceneProxyVtableRva = 0x2A99BE0;
constexpr std::uintptr_t kProbeSceneProxyGetterRva = 0x74CDE0;
constexpr std::uintptr_t kProbeSceneValueVtableRva = 0x2A99BC8;
constexpr std::uintptr_t kProbeAs3InterfaceVtableRva = 0x2CC1C38;
constexpr std::uintptr_t kProbeAs3GetMemberRva = 0x10DF400;
constexpr char kProbeScrollbarPath[] = "GraphicOption/ScrollBarV";
constexpr char kProbeVisibleMember[] = "visible";
constexpr char kProbeAlphaMember[] = "alpha";
constexpr std::size_t kProbeRecordCapacity = 16;
constexpr std::uint32_t kProbeTraceLimit = 8;

using ProbeValueConstructorFn = void*(__fastcall*)(void* destination);
using ProbeGetMemberFn = bool(__fastcall*)(
    void* value_interface,
    void* value_payload,
    const char* member_name,
    void* destination);

enum class ScrollbarProbeStage : std::uint8_t {
    baseline,
    selector_reset,
    rows_retired,
    materialized,
    availability,
    full_refresh,
    component_state_sync,
    scrollbar_refresh,
    presentations,
    next_frame,
};

struct ScrollbarProbeRecord {
    ScrollbarProbeStage stage{};
    void* page{};
    void* movie_owner{};
    void* value_interface{};
    void* value_payload{};
    std::uint64_t native_count{};
    std::int32_t active{-1};
    std::int32_t selected{-1};
    std::int32_t axis0{-1};
    std::int32_t axis1{-1};
    std::int32_t scroll_position{-1};
    std::int32_t scroll_total{-1};
    std::int32_t scroll_page_size{-1};
    std::uint32_t visible_flags{};
    std::uint32_t alpha_flags{};
    double alpha{};
    std::uint8_t component_valid_cache{};
    std::uint8_t component_visible_cache{};
    unsigned long seh{};
    bool scalars_valid{};
    bool component_cache_read{};
    bool path_constructed{};
    bool display_valid{};
    bool visible_read{};
    bool visible{};
    bool alpha_read{};
};

struct ScrollbarProbeTrace {
    std::array<ScrollbarProbeRecord, kProbeRecordCapacity> records{};
    void* page{};
    std::uint64_t revision{};
    std::uint64_t before_count{};
    std::size_t expected_count{};
    std::size_t projected_count{};
    std::size_t record_count{};
    std::uint32_t serial{};
    bool armed{};
    bool overflow{};
};

ScrollbarProbeTrace g_scrollbar_probe{};
std::uint32_t g_scrollbar_probe_completed{};

[[nodiscard]] const char* scrollbar_probe_stage_name(
    ScrollbarProbeStage stage) noexcept {
    switch (stage) {
    case ScrollbarProbeStage::baseline:
        return "baseline";
    case ScrollbarProbeStage::selector_reset:
        return "selector_reset";
    case ScrollbarProbeStage::rows_retired:
        return "rows_retired_count_written";
    case ScrollbarProbeStage::materialized:
        return "materialized";
    case ScrollbarProbeStage::availability:
        return "availability";
    case ScrollbarProbeStage::full_refresh:
        return "full_refresh";
    case ScrollbarProbeStage::component_state_sync:
        return "component_state_sync";
    case ScrollbarProbeStage::scrollbar_refresh:
        return "scrollbar_refresh";
    case ScrollbarProbeStage::presentations:
        return "presentations_title";
    case ScrollbarProbeStage::next_frame:
        return "next_native_frame";
    }
    return "unknown";
}

[[nodiscard]] bool read_scrollbar_probe_member(
    const GameAddresses& addresses,
    void* value_interface,
    void* value_payload,
    ProbeGetMemberFn get_member,
    const char* name,
    std::uint32_t conversion,
    std::uint32_t expected_kind,
    std::uint32_t& out_flags,
    double& out_value,
    unsigned long& out_seh) noexcept {
    out_flags = 0;
    out_value = 0.0;
    if (!addresses.game_image_base || !addresses.scaleform_result_destructor ||
        !value_interface || !value_payload || !get_member || !name) {
        return false;
    }

    alignas(16) std::array<std::byte, kProbeMemberResultSize> result{};
    bool constructed{};
    bool read{};
#if defined(_MSC_VER)
    __try {
#endif
        void* const constructor_result =
            reinterpret_cast<ProbeValueConstructorFn>(
                addresses.game_image_base + kProbeValueConstructorRva)(
                    result.data());
        if (constructor_result == result.data()) {
            constructed = true;
            std::memcpy(
                result.data() + kProbeScaleformFlagsOffset,
                &conversion,
                sizeof(conversion));
            read = get_member(
                value_interface,
                value_payload,
                name,
                result.data() + 0x08);
            std::memcpy(
                &out_flags,
                result.data() + kProbeScaleformFlagsOffset,
                sizeof(out_flags));
            if (read &&
                (out_flags & kProbeScaleformKindMask) == expected_kind) {
                if (expected_kind == kProbeScaleformKindBoolean) {
                    std::uint8_t boolean{};
                    std::memcpy(
                        &boolean,
                        result.data() + kProbeScaleformPayloadOffset,
                        sizeof(boolean));
                    out_value = boolean != 0 ? 1.0 : 0.0;
                } else {
                    std::memcpy(
                        &out_value,
                        result.data() + kProbeScaleformPayloadOffset,
                        sizeof(out_value));
                    read = std::isfinite(out_value);
                }
            } else {
                read = false;
            }
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    if (!constructed) return false;
#if defined(_MSC_VER)
    __try {
#endif
        addresses.scaleform_result_destructor(result.data());
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return read;
}

void observe_scrollbar_display(
    const GameAddresses& addresses,
    ScrollbarProbeRecord& record) noexcept {
    if (!addresses.game_image_base || !addresses.scaleform_path_resolver ||
        !addresses.scaleform_result_destructor || !record.page) {
        return;
    }

    alignas(16) std::array<std::byte, kProbePathResultSize> proxy{};
    bool constructed{};
#if defined(_MSC_VER)
    __try {
#endif
        void* const result = addresses.scaleform_path_resolver(
            static_cast<std::byte*>(record.page) +
                kProbeMovieContextOffset,
            proxy.data(),
            kProbeScrollbarPath);
        constructed = result == proxy.data();
        record.path_constructed = constructed;
        if (constructed) {
            void* proxy_vtable{};
            void* proxy_getter{};
            void* self0{};
            void* self1{};
            void* self2{};
            void* value_vtable{};
            void* interface_vtable{};
            void* get_member_pointer{};
            std::uint32_t value_flags{};
            const auto* const value =
                proxy.data() + kProbeSceneProxyValueOffset;
            std::memcpy(&proxy_vtable, proxy.data(), sizeof(proxy_vtable));
            if (proxy_vtable) {
                std::memcpy(
                    &proxy_getter,
                    static_cast<const std::byte*>(proxy_vtable) +
                        sizeof(void*),
                    sizeof(proxy_getter));
            }
            std::memcpy(&self0, proxy.data() + 0x08, sizeof(self0));
            std::memcpy(&self1, proxy.data() + 0x10, sizeof(self1));
            std::memcpy(&self2, proxy.data() + 0x18, sizeof(self2));
            std::memcpy(
                &record.movie_owner,
                proxy.data() + 0x20,
                sizeof(record.movie_owner));
            std::memcpy(&value_vtable, value, sizeof(value_vtable));
            std::memcpy(
                &record.value_interface,
                value + kProbeScaleformInterfaceOffset,
                sizeof(record.value_interface));
            std::memcpy(
                &value_flags,
                value + kProbeScaleformFlagsOffset,
                sizeof(value_flags));
            std::memcpy(
                &record.value_payload,
                value + kProbeScaleformPayloadOffset,
                sizeof(record.value_payload));
            if (record.value_interface) {
                std::memcpy(
                    &interface_vtable,
                    record.value_interface,
                    sizeof(interface_vtable));
            }
            if (interface_vtable == addresses.game_image_base +
                    kProbeAs3InterfaceVtableRva) {
                std::memcpy(
                    &get_member_pointer,
                    static_cast<const std::byte*>(interface_vtable) +
                        4u * sizeof(void*),
                    sizeof(get_member_pointer));
            }
            record.display_valid =
                proxy_vtable == addresses.game_image_base +
                    kProbeSceneProxyVtableRva &&
                proxy_getter == addresses.game_image_base +
                    kProbeSceneProxyGetterRva &&
                self0 == proxy.data() && self1 == proxy.data() &&
                self2 == proxy.data() && record.movie_owner != nullptr &&
                value_vtable == addresses.game_image_base +
                    kProbeSceneValueVtableRva &&
                interface_vtable == addresses.game_image_base +
                    kProbeAs3InterfaceVtableRva &&
                get_member_pointer == addresses.game_image_base +
                    kProbeAs3GetMemberRva &&
                record.value_payload != nullptr &&
                (value_flags & kProbeScaleformKindMask) ==
                    kProbeScaleformKindDisplayObject;
            if (record.display_valid) {
                const auto get_member =
                    reinterpret_cast<ProbeGetMemberFn>(get_member_pointer);
                double visible{};
                unsigned long member_seh{};
                record.visible_read = read_scrollbar_probe_member(
                    addresses,
                    record.value_interface,
                    record.value_payload,
                    get_member,
                    kProbeVisibleMember,
                    kProbeConvertBoolean,
                    kProbeScaleformKindBoolean,
                    record.visible_flags,
                    visible,
                    member_seh);
                record.visible = visible != 0.0;
                if (member_seh != 0 && record.seh == 0) {
                    record.seh = member_seh;
                }
                member_seh = 0;
                record.alpha_read = read_scrollbar_probe_member(
                    addresses,
                    record.value_interface,
                    record.value_payload,
                    get_member,
                    kProbeAlphaMember,
                    kProbeConvertNumber,
                    kProbeScaleformKindNumber,
                    record.alpha_flags,
                    record.alpha,
                    member_seh);
                if (member_seh != 0 && record.seh == 0) {
                    record.seh = member_seh;
                }
            }
        }
#if defined(_MSC_VER)
    } __except (record.seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        // The ownership state of a partially resolved proxy is ambiguous
        // after SEH. Do not attempt cleanup through an unproven value.
        record.path_constructed = false;
        return;
    }
#endif
    if (!record.path_constructed) return;
#if defined(_MSC_VER)
    __try {
#endif
        addresses.scaleform_result_destructor(
            proxy.data() + kProbeSceneProxyValueOffset);
#if defined(_MSC_VER)
    } __except (record.seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
#endif
}

void record_scrollbar_probe(
    ScrollbarProbeStage stage,
    const GameAddresses& addresses) noexcept {
    if (!g_scrollbar_probe.armed || !g_scrollbar_probe.page) return;
    if (g_scrollbar_probe.record_count >=
        g_scrollbar_probe.records.size()) {
        g_scrollbar_probe.overflow = true;
        return;
    }

    ScrollbarProbeRecord& record =
        g_scrollbar_probe.records[g_scrollbar_probe.record_count++];
    record.stage = stage;
    record.page = g_scrollbar_probe.page;
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const bytes = static_cast<const std::byte*>(record.page);
        const auto* const selector = bytes + kSelectorOffset;
        std::memcpy(
            &record.native_count,
            bytes + kNativeCountOffset,
            sizeof(record.native_count));
        std::memcpy(
            &record.active,
            selector + kSelectorActiveOffset,
            sizeof(record.active));
        std::memcpy(
            &record.selected,
            selector + kSelectorSelectedOffset,
            sizeof(record.selected));
        std::memcpy(
            &record.axis0,
            selector + kSelectorAxis0Offset,
            sizeof(record.axis0));
        std::memcpy(
            &record.axis1,
            selector + kSelectorAxis1Offset,
            sizeof(record.axis1));
        std::memcpy(
            &record.scroll_position,
            bytes + kProbeScrollbarPositionOffset,
            sizeof(record.scroll_position));
        std::memcpy(
            &record.scroll_total,
            bytes + kProbeScrollbarTotalOffset,
            sizeof(record.scroll_total));
        std::memcpy(
            &record.scroll_page_size,
            bytes + kProbeScrollbarPageSizeOffset,
            sizeof(record.scroll_page_size));
        std::memcpy(
            &record.component_valid_cache,
            bytes + kScrollbarControllerOffset +
                kProbeComponentValidCacheOffset,
            sizeof(record.component_valid_cache));
        std::memcpy(
            &record.component_visible_cache,
            bytes + kScrollbarControllerOffset +
                kProbeComponentVisibleCacheOffset,
            sizeof(record.component_visible_cache));
        record.component_cache_read = true;
        record.scalars_valid = true;
#if defined(_MSC_VER)
    } __except (record.seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        record.scalars_valid = false;
    }
#endif
    observe_scrollbar_display(addresses, record);
}

void arm_scrollbar_probe(
    const PageBinding& binding,
    const Snapshot& before,
    const detail::PageSlice& target_slice,
    std::size_t expected_count,
    const GameAddresses& addresses) noexcept {
    if (g_scrollbar_probe.armed ||
        g_scrollbar_probe_completed >= kProbeTraceLimit ||
        binding.route.kind != detail::PageRouteKind::submenu ||
        target_slice.has_next || before.count <= expected_count) {
        return;
    }
    g_scrollbar_probe = {};
    g_scrollbar_probe.page = binding.page;
    g_scrollbar_probe.revision = binding.target_revision;
    g_scrollbar_probe.before_count = before.count;
    g_scrollbar_probe.expected_count = expected_count;
    g_scrollbar_probe.projected_count = target_slice.row_count;
    g_scrollbar_probe.serial = g_scrollbar_probe_completed + 1;
    g_scrollbar_probe.armed = true;
    record_scrollbar_probe(ScrollbarProbeStage::baseline, addresses);
}

void finish_scrollbar_probe(bool success, const char* outcome) noexcept {
    if (!g_scrollbar_probe.armed) return;
    detail::logf(
        LogLevel::info,
        "SCROLLBAR_PROBE_BEGIN serial=%u page=%p revision=%llu before=%llu expected=%zu projected=%zu records=%zu overflow=%u success=%u outcome=%s",
        static_cast<unsigned>(g_scrollbar_probe.serial),
        g_scrollbar_probe.page,
        static_cast<unsigned long long>(g_scrollbar_probe.revision),
        static_cast<unsigned long long>(g_scrollbar_probe.before_count),
        g_scrollbar_probe.expected_count,
        g_scrollbar_probe.projected_count,
        g_scrollbar_probe.record_count,
        g_scrollbar_probe.overflow ? 1u : 0u,
        success ? 1u : 0u,
        outcome ? outcome : "unknown");
    for (std::size_t index = 0;
         index < g_scrollbar_probe.record_count; ++index) {
        const ScrollbarProbeRecord& record =
            g_scrollbar_probe.records[index];
        detail::logf(
            LogLevel::info,
            "SCROLLBAR_PROBE_RECORD serial=%u index=%zu stage=%s page=%p native={count:%llu active:%d selected:%d axis0:%d axis1:%d} range={position:%d total:%d pageSize:%d} controller={cacheRead:%u valid:%u visible:%u} display={constructed:%u valid:%u visibleRead:%u visible:%u visibleFlags:0x%X alphaRead:%u alpha:%.6f alphaFlags:0x%X owner:%p interface:%p payload:%p} scalarValid=%u seh=0x%08lX",
            static_cast<unsigned>(g_scrollbar_probe.serial),
            index,
            scrollbar_probe_stage_name(record.stage),
            record.page,
            static_cast<unsigned long long>(record.native_count),
            record.active,
            record.selected,
            record.axis0,
            record.axis1,
            record.scroll_position,
            record.scroll_total,
            record.scroll_page_size,
            record.component_cache_read ? 1u : 0u,
            static_cast<unsigned>(record.component_valid_cache),
            static_cast<unsigned>(record.component_visible_cache),
            record.path_constructed ? 1u : 0u,
            record.display_valid ? 1u : 0u,
            record.visible_read ? 1u : 0u,
            record.visible ? 1u : 0u,
            static_cast<unsigned>(record.visible_flags),
            record.alpha_read ? 1u : 0u,
            record.alpha,
            static_cast<unsigned>(record.alpha_flags),
            record.movie_owner,
            record.value_interface,
            record.value_payload,
            record.scalars_valid ? 1u : 0u,
            record.seh);
    }
    detail::logf(
        LogLevel::info,
        "SCROLLBAR_PROBE_END serial=%u",
        static_cast<unsigned>(g_scrollbar_probe.serial));
    ++g_scrollbar_probe_completed;
    g_scrollbar_probe = {};
}

#endif

[[nodiscard]] bool main_route(detail::PageRoute route) noexcept {
    return route.slice_index == 0 &&
        (route.kind == detail::PageRouteKind::root_main ||
         route.kind == detail::PageRouteKind::builtin_main ||
         route.kind == detail::PageRouteKind::tab_main ||
         route.kind == detail::PageRouteKind::submenu);
}

[[nodiscard]] bool tab_carrier_exact(
    void* page,
    detail::PageRoute route) noexcept {
    return route.kind != detail::PageRouteKind::tab_main ||
        validate_native_row_visibility_tab_carrier(
            page, route.logical_page_index);
}

void log_fault(const char* stage, void* page, const char* reason) noexcept {
    if (++g_fault_logs > 24) return;
    detail::logf(
        LogLevel::error,
        "row visibility native lifecycle fault[%u] stage=%s page=%p reason=%s",
        static_cast<unsigned>(g_fault_logs),
        stage ? stage : "unknown", page,
        reason ? reason : "unknown");
}

template <std::size_t Size>
[[nodiscard]] bool entry_matches(
    const GameAddresses& addresses,
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& prefix) noexcept {
    return addresses.game_image_base &&
        rva + prefix.size() <= addresses.game_image_size &&
        std::memcmp(addresses.game_image_base + rva,
            prefix.data(), prefix.size()) == 0;
}

template <std::size_t Size>
[[nodiscard]] bool entry_matches(
    const ModuleView& game,
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& prefix) noexcept {
    const std::uint8_t* const entry = game.at_rva(rva);
    return entry && game.contains(entry, prefix.size()) &&
        std::memcmp(entry, prefix.data(), prefix.size()) == 0;
}

[[nodiscard]] bool resolve_entries(
    const GameAddresses& addresses,
    NativeEntries& output) noexcept {
    output = {};
    if (!addresses.game_image_base || !addresses.edit_property_vtable) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(
            addresses.game_image_base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
            static_cast<std::size_t>(dos->e_lfanew) +
                    sizeof(IMAGE_NT_HEADERS64) >
                addresses.game_image_size) {
            return false;
        }
        const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            addresses.game_image_base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        const NativeProfile* profile{};
        for (const NativeProfile& candidate : kProfiles) {
            if (candidate.timestamp == nt->FileHeader.TimeDateStamp &&
                candidate.image_size == nt->OptionalHeader.SizeOfImage &&
                candidate.image_size == addresses.game_image_size) {
                profile = &candidate;
                break;
            }
        }
        if (!profile ||
            !entry_matches(addresses, profile->set_active_rva,
                kSetActivePrefix) ||
            !entry_matches(addresses, profile->row_destructor_rva,
                kRowDestructorPrefix) ||
            !entry_matches(addresses, profile->full_refresh_rva,
                kFullRefreshPrefix)) {
            return false;
        }
        if (!entry_matches(addresses, profile->component_state_sync_rva,
                kComponentStateSyncPrefix) ||
            !entry_matches(addresses, profile->scrollbar_refresh_rva,
                kScrollbarRefreshPrefix)) {
            return false;
        }
        output.profile = profile;
        output.set_active = reinterpret_cast<SelectorSetActiveFn>(
            addresses.game_image_base + profile->set_active_rva);
        output.full_refresh = reinterpret_cast<PageFullRefreshFn>(
            addresses.game_image_base + profile->full_refresh_rva);
        output.row_destructor = reinterpret_cast<RowDeletingDestructorFn>(
            addresses.game_image_base + profile->row_destructor_rva);
        output.component_state_sync =
            reinterpret_cast<ComponentStateSyncFn>(
                addresses.game_image_base +
                    profile->component_state_sync_rva);
        output.scrollbar_refresh = reinterpret_cast<ScrollbarRefreshFn>(
            addresses.game_image_base + profile->scrollbar_refresh_rva);
        void* vtable_destructor{};
        std::memcpy(&vtable_destructor, addresses.edit_property_vtable,
            sizeof(vtable_destructor));
        return output.set_active && output.full_refresh &&
            output.row_destructor &&
            output.component_state_sync && output.scrollbar_refresh &&
            vtable_destructor == reinterpret_cast<void*>(
                output.row_destructor);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        output = {};
        return false;
    }
#endif
}

[[nodiscard]] Snapshot inspect(
    void* page,
    const GameAddresses& addresses,
    const NativeEntries& entries,
    bool require_quiescent = true) noexcept {
    Snapshot result{};
    if (!page || !entries.profile || !addresses.game_image_base ||
        !addresses.option_setting_dialog_vtable ||
        !addresses.edit_property_vtable) {
        return result;
    }
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const bytes = static_cast<const std::byte*>(page);
        const auto* const selector = bytes + kSelectorOffset;
        std::memcpy(&result.page_vtable, bytes,
            sizeof(result.page_vtable));
        std::memcpy(&result.owner_vtable, bytes + kRowOwnerOffset,
            sizeof(result.owner_vtable));
        std::memcpy(&result.selector_vtable, selector,
            sizeof(result.selector_vtable));
        std::memcpy(&result.selector_adapter,
            selector + kSelectorAdapterOffset,
            sizeof(result.selector_adapter));
        std::memcpy(result.selector_binding.data(),
            selector + kSelectorBindingOffset,
            result.selector_binding.size());
        std::memcpy(&result.capacity, bytes + kVisualCapacityOffset,
            sizeof(result.capacity));
        std::memcpy(&result.count, bytes + kNativeCountOffset,
            sizeof(result.count));
        std::memcpy(&result.active,
            selector + kSelectorActiveOffset, sizeof(result.active));
        std::memcpy(&result.selected,
            selector + kSelectorSelectedOffset, sizeof(result.selected));
        std::memcpy(&result.axis0,
            selector + kSelectorAxis0Offset, sizeof(result.axis0));
        std::memcpy(&result.axis1,
            selector + kSelectorAxis1Offset, sizeof(result.axis1));
        std::memcpy(&result.active_job, bytes + kActiveJobOffset,
            sizeof(result.active_job));
        std::memcpy(&result.queued_jobs, bytes + kQueuedJobsOffset,
            sizeof(result.queued_jobs));
        std::memcpy(&result.component_count,
            bytes + kComponentCountOffset,
            sizeof(result.component_count));

        bool rows_exact = result.count != 0 &&
            result.count <= kMaximumRows;
        for (std::size_t index = 0;
             rows_exact && index < static_cast<std::size_t>(result.count);
             ++index) {
            const std::byte* const row =
                bytes + kRowStorageOffset + index * kRowStride;
            std::memcpy(&result.row_vtables[index], row,
                sizeof(result.row_vtables[index]));
            std::memcpy(&result.row_controllers[index],
                row + kRowControllerOffset,
                sizeof(result.row_controllers[index]));
            rows_exact =
                result.row_vtables[index] == addresses.edit_property_vtable &&
                result.row_controllers[index] != nullptr;
            for (std::size_t previous = 0;
                 rows_exact && previous < index; ++previous) {
                rows_exact = result.row_controllers[previous] !=
                    result.row_controllers[index];
            }
        }
        result.valid =
            result.page_vtable == addresses.option_setting_dialog_vtable &&
            result.owner_vtable == addresses.game_image_base +
                entries.profile->owner_vtable_rva &&
            result.selector_vtable == addresses.game_image_base +
                entries.profile->selector_vtable_rva &&
            result.selector_adapter != nullptr &&
            result.capacity >= 1 && result.capacity <= kMaximumRows &&
            result.count <= result.capacity && rows_exact &&
            result.active == static_cast<std::int32_t>(result.count) &&
            result.selected >= 0 &&
            static_cast<std::uint64_t>(result.selected) < result.count &&
            result.axis0 == 1 &&
            result.axis1 == static_cast<std::int32_t>(result.capacity) &&
            (!require_quiescent ||
                (result.active_job == nullptr &&
                 result.queued_jobs == 0 && result.component_count < 8));
#if defined(_MSC_VER)
    } __except (
        result.seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        result.valid = false;
    }
#endif
    return result;
}

[[nodiscard]] bool same_snapshot(
    const Snapshot& left,
    const Snapshot& right) noexcept {
    if (!left.valid || !right.valid ||
        left.page_vtable != right.page_vtable ||
        left.owner_vtable != right.owner_vtable ||
        left.selector_vtable != right.selector_vtable ||
        left.selector_adapter != right.selector_adapter ||
        left.selector_binding != right.selector_binding ||
        left.count != right.count || left.capacity != right.capacity ||
        left.active != right.active || left.selected != right.selected ||
        left.axis0 != right.axis0 || left.axis1 != right.axis1 ||
        left.active_job != right.active_job ||
        left.queued_jobs != right.queued_jobs ||
        left.component_count != right.component_count) {
        return false;
    }
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(left.count); ++index) {
        if (left.row_vtables[index] != right.row_vtables[index] ||
            left.row_controllers[index] != right.row_controllers[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] PageBinding* find_page(void* page) noexcept {
    for (PageBinding& binding : g_pages) {
        if (binding.active && binding.page == page) return &binding;
    }
    return nullptr;
}

[[nodiscard]] PageBinding* allocate_page(void* page) noexcept {
    if (PageBinding* existing = find_page(page)) return existing;
    for (PageBinding& binding : g_pages) {
        if (!binding.active) return &binding;
    }
    return nullptr;
}

[[nodiscard]] bool read_count(
    void* page,
    const GameAddresses& addresses,
    std::uint64_t& count) noexcept {
    count = 0;
    if (!page || !addresses.option_setting_dialog_vtable) return false;
#if defined(_MSC_VER)
    __try {
#endif
        void* vtable{};
        std::uint32_t capacity{};
        std::memcpy(&vtable, page, sizeof(vtable));
        const auto* bytes = static_cast<const std::byte*>(page);
        std::memcpy(&capacity, bytes + kVisualCapacityOffset,
            sizeof(capacity));
        std::memcpy(&count, bytes + kNativeCountOffset, sizeof(count));
        return vtable == addresses.option_setting_dialog_vtable &&
            capacity >= 1 && capacity <= kMaximumRows && count <= capacity;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        count = 0;
        return false;
    }
#endif
}

[[nodiscard]] bool write_count(void* page, std::uint64_t count) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(static_cast<std::byte*>(page) + kNativeCountOffset,
            &count, sizeof(count));
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

[[nodiscard]] bool row_enabled(void* page, std::size_t index) noexcept {
    if (!page || index >= kMaximumRows) return false;
    bool enabled{};
#if defined(_MSC_VER)
    __try {
#endif
        const auto* row = static_cast<const std::byte*>(page) +
            kRowStorageOffset + index * kRowStride;
        void* controller{};
        std::memcpy(&controller, row + kRowControllerOffset,
            sizeof(controller));
        std::uint8_t byte{};
        std::memcpy(&byte, row + kRowEnabledOffset, sizeof(byte));
        enabled = controller != nullptr && byte != 0;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        enabled = false;
    }
#endif
    return enabled;
}

[[nodiscard]] std::size_t nearest_enabled(
    void* page,
    std::size_t count,
    std::size_t preferred) noexcept {
    if (count == 0) return detail::invalid_compiled_index;
    preferred = std::min(preferred, count - 1);
    if (row_enabled(page, preferred)) return preferred;
    for (std::size_t distance = 1; distance < count; ++distance) {
        if (preferred >= distance &&
            row_enabled(page, preferred - distance)) {
            return preferred - distance;
        }
        if (preferred + distance < count &&
            row_enabled(page, preferred + distance)) {
            return preferred + distance;
        }
    }
    return detail::invalid_compiled_index;
}

[[nodiscard]] bool prefix_identity_equal(
    const Snapshot& before,
    const Snapshot& after,
    std::size_t prefix) noexcept {
    if (prefix > before.count || prefix > after.count) return false;
    for (std::size_t index = 0; index < prefix; ++index) {
        if (before.row_vtables[index] != after.row_vtables[index] ||
            before.row_controllers[index] != after.row_controllers[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool execute_rebuild(
    PageBinding& binding,
    const GameAddresses& addresses,
    const NativeEntries& entries,
    const Snapshot& before,
    Snapshot& rebuilt) noexcept {
    rebuilt = {};
    detail::RuntimeState& runtime = detail::runtime_state();
    if (!runtime.menu || !binding.main_route ||
        binding.route.logical_page_index >= runtime.menu->pages.size()) {
        return false;
    }
    if (!tab_carrier_exact(binding.page, binding.route)) return false;
    detail::PageSlice target_slice{};
    if (!resolve_page_route_slice(
            *runtime.menu, binding.route, target_slice) ||
        target_slice.slice_index != 0) {
        return false;
    }
    const std::size_t navigation = target_slice.has_next ? 1u : 0u;
    const std::size_t target_injected = target_slice.row_count + navigation;
    const std::size_t expected_count =
        static_cast<std::size_t>(binding.prefix_count) + target_injected;
    if (target_slice.has_previous || expected_count == 0 ||
        expected_count > before.capacity ||
        binding.prefix_count > before.count ||
        before.count != binding.count_after) {
        return false;
    }

    std::size_t preferred{};
    if (binding.selected_prefix != detail::invalid_compiled_index &&
        binding.selected_prefix < binding.prefix_count) {
        preferred = binding.selected_prefix;
    } else {
        std::size_t projected{};
        const bool mapped =
            binding.selected_source != detail::invalid_compiled_index &&
            detail::map_source_row_to_projected(
                binding.route.logical_page_index,
                binding.target_revision,
                binding.selected_source,
                projected) &&
            projected >= target_slice.first_row &&
            projected < target_slice.first_row + target_slice.row_count;
        const std::size_t local = mapped
            ? projected - target_slice.first_row
            : (target_slice.row_count == 0
                ? 0
                : std::min(binding.selected_projected,
                    target_slice.row_count - 1));
        preferred = static_cast<std::size_t>(binding.prefix_count) + local;
        if (target_slice.row_count == 0) {
            preferred = binding.prefix_count != 0
                ? static_cast<std::size_t>(binding.prefix_count - 1)
                : 0;
        }
    }

#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    arm_scrollbar_probe(
        binding, before, target_slice, expected_count, addresses);
#endif

    retire_native_row_presentation_page(binding.page, addresses);
    reset_color_picker_page_widgets(
        binding.page,
        color_picker_widget_host_for_route(binding.route));

    bool mutation_started{};
#if defined(_MSC_VER)
    __try {
#endif
        mutation_started = true;
        entries.set_active(
            static_cast<std::byte*>(binding.page) + kSelectorOffset, 0u);
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
        record_scrollbar_probe(
            ScrollbarProbeStage::selector_reset, addresses);
#endif
        for (std::size_t index =
                static_cast<std::size_t>(binding.prefix_count);
             index < static_cast<std::size_t>(before.count); ++index) {
            void* row = static_cast<std::byte*>(binding.page) +
                kRowStorageOffset + index * kRowStride;
            (void)entries.row_destructor(row, 0u);
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        (void)mutation_started;
        return false;
    }
#endif
    if (!write_count(binding.page, binding.prefix_count)) return false;
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    record_scrollbar_probe(ScrollbarProbeStage::rows_retired, addresses);
#endif

    g_internal_rebuild_page = binding.page;
    g_internal_materialization_exact = false;
    const RowInjectionOutcome outcome = inject_page_route(
        binding.page, binding.route, addresses);
    const bool materialization_exact = g_internal_materialization_exact &&
        outcome.attempted == target_injected &&
        outcome.added == target_injected &&
        outcome.content_rows == target_slice.row_count &&
        outcome.navigation == navigation &&
        outcome.faulted == 0 && outcome.skipped == 0;
    g_internal_rebuild_page = nullptr;
    g_internal_materialization_exact = false;
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    record_scrollbar_probe(ScrollbarProbeStage::materialized, addresses);
#endif
    if (!materialization_exact) return false;

    apply_native_row_availability_pre_frame(binding.page, addresses);
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    record_scrollbar_probe(ScrollbarProbeStage::availability, addresses);
#endif
    preferred = nearest_enabled(binding.page, expected_count, preferred);
    if (preferred == detail::invalid_compiled_index) return false;
#if defined(_MSC_VER)
    __try {
#endif
        entries.full_refresh(
            binding.page, static_cast<std::uint32_t>(preferred));
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
        record_scrollbar_probe(ScrollbarProbeStage::full_refresh, addresses);
#endif
        // Destructive row retirement can restore the authored ScrollBarV
        // visibility without updating its native controller's cached state.
        // First ask the controller to observe its live Scaleform component,
        // then let Elden Ring derive visibility and thumb geometry from the
        // already-refreshed range. This does not force a GFX property.
        if (binding.route.kind == detail::PageRouteKind::submenu) {
            void* const scrollbar =
                static_cast<std::byte*>(binding.page) +
                kScrollbarControllerOffset;
            entries.component_state_sync(scrollbar);
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
            record_scrollbar_probe(
                ScrollbarProbeStage::component_state_sync, addresses);
#endif
            entries.scrollbar_refresh(scrollbar);
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
            record_scrollbar_probe(
                ScrollbarProbeStage::scrollbar_refresh, addresses);
#endif
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    (void)apply_native_row_presentations(binding.page, addresses);
    // Only submenu/continuation constructors expose a validated, page-owned
    // Scaleform title result. The physical page survives this row-only
    // transaction, so reapply the title variant selected by the rebuilt
    // visible slice count after the native refresh has finished.
    reapply_native_row_visibility_page_title(
        binding.page,
        binding.title_target,
        binding.route,
        outcome.slice_count);
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    record_scrollbar_probe(ScrollbarProbeStage::presentations, addresses);
#endif

    rebuilt = inspect(binding.page, addresses, entries);
    const bool exact = rebuilt.valid &&
        rebuilt.count == expected_count &&
        rebuilt.selected == static_cast<std::int32_t>(preferred) &&
        before.page_vtable == rebuilt.page_vtable &&
        before.owner_vtable == rebuilt.owner_vtable &&
        before.selector_vtable == rebuilt.selector_vtable &&
        before.selector_adapter == rebuilt.selector_adapter &&
        before.selector_binding == rebuilt.selector_binding &&
        before.capacity == rebuilt.capacity &&
        before.axis0 == rebuilt.axis0 && before.axis1 == rebuilt.axis1 &&
        before.active_job == rebuilt.active_job &&
        before.queued_jobs == rebuilt.queued_jobs &&
        before.component_count == rebuilt.component_count &&
        prefix_identity_equal(
            before, rebuilt,
            static_cast<std::size_t>(binding.prefix_count)) &&
        tab_carrier_exact(binding.page, binding.route);
    return exact;
}

} // namespace

bool native_row_visibility_supported(
    const GameAddresses& addresses) noexcept {
    NativeEntries entries{};
    return resolve_entries(addresses, entries);
}

bool native_row_visibility_build_supported(
    const ModuleView& game) noexcept {
    if (!game.base()) return false;
    for (const NativeProfile& profile : kProfiles) {
        const TextInputAddressProfile* const text_profile =
            find_text_input_address_profile(profile.build);
        const std::uint8_t* const edit_vtable = text_profile
            ? game.at_rva(text_profile->edit_property_vtable_rva)
            : nullptr;
        void* deleting_destructor{};
        if (!text_profile || !edit_vtable ||
            !game.contains(edit_vtable, sizeof(deleting_destructor))) {
            continue;
        }
        std::memcpy(&deleting_destructor, edit_vtable,
            sizeof(deleting_destructor));
        if (profile.timestamp == game.timestamp() &&
            profile.image_size == game.image_size() &&
            entry_matches(game, profile.set_active_rva, kSetActivePrefix) &&
            entry_matches(
                game, profile.row_destructor_rva, kRowDestructorPrefix) &&
            entry_matches(
                game, profile.full_refresh_rva, kFullRefreshPrefix) &&
            entry_matches(game, profile.component_state_sync_rva,
                kComponentStateSyncPrefix) &&
            entry_matches(game, profile.scrollbar_refresh_rva,
                kScrollbarRefreshPrefix) &&
            deleting_destructor == game.at_rva(
                profile.row_destructor_rva)) {
            return true;
        }
    }
    return false;
}

bool native_row_visibility_internal_materialization(
    const void* page) noexcept {
    return page != nullptr && page == g_internal_rebuild_page;
}

RowVisibilityMaterializationTicket
begin_native_row_visibility_materialization(
    void* page,
    detail::PageRoute route,
    const GameAddresses& addresses) noexcept {
    std::lock_guard lock(g_mutex);
    RowVisibilityMaterializationTicket ticket{};
    NativeEntries entries{};
    std::uint64_t count{};
    const bool internal_rebuild = page == g_internal_rebuild_page;
    const detail::RowVisibilityProjection projection =
        detail::row_visibility_projection(route.logical_page_index);
    if (g_backend_faulted || !resolve_entries(addresses, entries) ||
        !projection || !read_count(page, addresses, count)) {
        // An ordinary constructor attempt supersedes any older binding for
        // this address. If the new materialization cannot be proven, never
        // let a stale route/count/title target survive page-address reuse.
        // Internal reconstruction retains its binding until the enclosing
        // transaction records the failure and faults the backend.
        if (!internal_rebuild) {
            if (PageBinding* stale = find_page(page)) *stale = {};
        }
        return ticket;
    }
    ticket.page = page;
    ticket.route = route;
    ticket.count_before = count;
    ticket.projection_revision = projection.revision;
    ticket.internal_rebuild = internal_rebuild;
    ticket.armed = true;
    return ticket;
}

void complete_native_row_visibility_materialization(
    const GameAddresses& addresses,
    RowVisibilityMaterializationTicket ticket,
    const RowInjectionOutcome& outcome) noexcept {
    std::lock_guard lock(g_mutex);
    if (!ticket.armed) return;
    std::uint64_t count{};
    const detail::RowVisibilityProjection projection =
        detail::row_visibility_projection(ticket.route.logical_page_index);
    const bool exact = read_count(ticket.page, addresses, count) && projection &&
        projection.revision == ticket.projection_revision &&
        outcome.route == ticket.route && outcome.faulted == 0 &&
        outcome.skipped == 0 && outcome.attempted == outcome.added &&
        count == ticket.count_before + outcome.added;
    if (ticket.internal_rebuild) {
        g_internal_materialization_exact = exact;
        return;
    }
    PageBinding* binding = find_page(ticket.page);
    CapturedTitleTarget retained_title{};
    if (exact && binding && binding->route == ticket.route &&
        binding->title_target.owned_by(ticket.page)) {
        retained_title = binding->title_target;
    }
    if (!exact) {
        if (binding) *binding = {};
        log_fault("materialize", ticket.page,
            "inexact");
        return;
    }
    if (!binding) binding = allocate_page(ticket.page);
    if (!binding) {
        log_fault("materialize", ticket.page, "tracking_capacity");
        return;
    }
    *binding = {
        .page = ticket.page,
        .route = ticket.route,
        .prefix_count = ticket.count_before,
        .count_after = count,
        .applied_revision = ticket.projection_revision,
        .target_revision = ticket.projection_revision,
        .title_target = retained_title,
        .main_route = main_route(ticket.route),
        .active = true,
    };
}

void bind_native_row_visibility_title_target(
    void* page,
    const CapturedTitleTarget& captured) noexcept {
    std::lock_guard lock(g_mutex);
    PageBinding* const binding = find_page(page);
    if (!binding || !captured.owned_by(page) ||
        captured.route != binding->route) {
        return;
    }
    binding->title_target = captured;
}

bool prepare_native_row_visibility_page_frame(
    void* page,
    bool native_input_enabled,
    const GameAddresses& addresses) noexcept {
    std::lock_guard lock(g_mutex);
    PageBinding* const binding = find_page(page);
    if (!binding || g_backend_faulted || binding->phase == Phase::faulted) {
        return false;
    }
    NativeEntries entries{};
    if (!resolve_entries(addresses, entries)) {
        binding->phase = Phase::faulted;
        g_backend_faulted = true;
        return false;
    }

    // A continuation can disappear when the projected page count shrinks.
    // That native Back-stack rewrite has not been proven. Keep its current
    // projection alive and publish the queued generation after returning to a
    // rebuildable main page (or on the next fresh materialization).
    if (detail::row_visibility_updates_pending()) {
        if (!binding->main_route || !native_input_enabled ||
            native_dialog_owns_menu_input() ||
            color_picker_session_active() ||
            binding->phase != Phase::idle) {
            return binding->phase != Phase::idle;
        }
        // The reusable Graphics carrier can change while old panels still
        // receive a frame. Never publish the batch from an aliased/stale
        // custom-tab panel; wait for the exact committed carrier instead.
        if (!tab_carrier_exact(page, binding->route)) return false;

        const Snapshot before = inspect(page, addresses, entries, false);
        if (!before.valid || before.count != binding->count_after) {
            return false;
        }
        binding->selected_source = detail::invalid_compiled_index;
        binding->selected_prefix = detail::invalid_compiled_index;
        binding->selected_projected = 0;
        if (before.selected >= 0) {
            const std::size_t selected =
                static_cast<std::size_t>(before.selected);
            if (selected < binding->prefix_count) {
                binding->selected_prefix = selected;
            } else {
                detail::PageSlice old_slice{};
                if (resolve_page_route_slice(
                        *detail::runtime_state().menu,
                        binding->route,
                        old_slice)) {
                    const std::size_t local = selected -
                        static_cast<std::size_t>(binding->prefix_count);
                    if (local < old_slice.row_count) {
                        binding->selected_projected =
                            old_slice.first_row + local;
                        (void)detail::map_projected_row_to_source(
                            binding->route.logical_page_index,
                            binding->applied_revision,
                            binding->selected_projected,
                            binding->selected_source);
                    }
                }
            }
        }
        detail::synchronize_pending_row_updates();
    }

    const detail::RowVisibilityProjection projection =
        detail::row_visibility_projection(binding->route.logical_page_index);
    if (!projection || projection.revision == binding->applied_revision) {
        return binding->phase != Phase::idle;
    }
    if (!binding->main_route) {
        // Another active main page may have published a multi-page batch.
        // Do not mutate a continuation whose route may no longer exist.
        log_fault("continuation", page, "active_slice_changed_deferred");
        binding->phase = Phase::faulted;
        return false;
    }
    if (binding->phase == Phase::idle) {
        // Another active page may have published a multi-page generation.
        // Hidden parent panels continue receiving frames with input disabled;
        // do not start their destructive settle/rebuild until the page is the
        // active input owner and no native editor owns the menu.
        if (!native_input_enabled || native_dialog_owns_menu_input() ||
            color_picker_session_active() ||
            !tab_carrier_exact(page, binding->route)) {
            return false;
        }
        // A single atomic batch may affect several already-materialized
        // pages. Only the page that published it could map through the old
        // projection; a later inactive page still preserves its physical
        // ordinal (or native prefix selection) when it next becomes active.
        if (binding->selected_source == detail::invalid_compiled_index &&
            binding->selected_prefix == detail::invalid_compiled_index) {
            const Snapshot before = inspect(page, addresses, entries, false);
            if (before.valid && before.selected >= 0) {
                const std::size_t selected =
                    static_cast<std::size_t>(before.selected);
                if (selected < binding->prefix_count) {
                    binding->selected_prefix = selected;
                } else {
                    binding->selected_projected = selected -
                        static_cast<std::size_t>(binding->prefix_count);
                }
            }
        }
        binding->target_revision = projection.revision;
        binding->stable_frames = 0;
        binding->settle = {};
        binding->phase = Phase::settling;
    }
    return true;
}

void observe_native_row_visibility_page_frame(
    void* page,
    const GameAddresses& addresses) noexcept {
    std::lock_guard lock(g_mutex);
    PageBinding* binding = find_page(page);
    if (!binding || g_backend_faulted || binding->phase == Phase::idle ||
        binding->phase == Phase::faulted) {
        return;
    }
    NativeEntries entries{};
    if (!resolve_entries(addresses, entries)) {
        binding->phase = Phase::faulted;
        g_backend_faulted = true;
        return;
    }
    if (binding->phase == Phase::verifying) {
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
        record_scrollbar_probe(ScrollbarProbeStage::next_frame, addresses);
#endif
        const Snapshot current = inspect(page, addresses, entries);
        if (!same_snapshot(binding->expected, current)) {
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
            finish_scrollbar_probe(
                false, "post_rebuild_identity_changed");
#endif
            log_fault("verify", page, "post_rebuild_identity_changed");
            binding->phase = Phase::faulted;
            g_backend_faulted = true;
            return;
        }
        binding->applied_revision = binding->target_revision;
        binding->count_after = current.count;
        binding->selected_source = detail::invalid_compiled_index;
        binding->selected_prefix = detail::invalid_compiled_index;
        binding->selected_projected = 0;
        binding->phase = Phase::idle;
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
        finish_scrollbar_probe(true, "verified");
#endif
        return;
    }
    if (binding->phase != Phase::settling ||
        native_dialog_owns_menu_input() || color_picker_session_active()) {
        return;
    }
    const Snapshot current = inspect(page, addresses, entries);
    if (!current.valid || current.count != binding->count_after) {
        binding->stable_frames = 0;
        binding->settle = {};
        return;
    }
    if (binding->stable_frames != 0 &&
        same_snapshot(binding->settle, current)) {
        ++binding->stable_frames;
    } else {
        binding->settle = current;
        binding->stable_frames = 1;
    }
    if (binding->stable_frames < 2) return;

    binding->phase = Phase::rebuilding;
    Snapshot rebuilt{};
    if (!execute_rebuild(
            *binding, addresses, entries, current, rebuilt)) {
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
        finish_scrollbar_probe(false, "transaction_failed");
#endif
        log_fault("rebuild", page, "transaction_failed");
        binding = find_page(page);
        if (binding) binding->phase = Phase::faulted;
        g_backend_faulted = true;
        return;
    }
    binding = find_page(page);
    if (!binding) {
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
        finish_scrollbar_probe(false, "binding_lost");
#endif
        return;
    }
    binding->expected = rebuilt;
    binding->count_after = rebuilt.count;
    binding->phase = Phase::verifying;
}

void retire_native_row_visibility_page(void* page) noexcept {
    std::lock_guard lock(g_mutex);
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    if (g_scrollbar_probe.armed && g_scrollbar_probe.page == page) {
        finish_scrollbar_probe(false, "page_retired");
    }
#endif
    if (PageBinding* binding = find_page(page)) *binding = {};
}

void reset_native_row_visibility() noexcept {
    std::lock_guard lock(g_mutex);
    for (PageBinding& binding : g_pages) binding = {};
    g_internal_rebuild_page = nullptr;
    g_internal_materialization_exact = false;
    g_fault_logs = 0;
    g_backend_faulted = false;
#if defined(ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE)
    g_scrollbar_probe = {};
    g_scrollbar_probe_completed = 0;
#endif
}

} // namespace erui::native
