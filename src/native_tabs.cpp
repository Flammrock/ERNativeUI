#include "tab_viewport.hpp"
#include "tab_initial_presentation_refresh.hpp"
#include "tab_icon_retirement_policy.hpp"
#include "tab_pointer_dwell.hpp"
#include "tab_record_epoch.hpp"
#include "tab_transition_input_gate.hpp"
#include "native_tab_runtime_state.hpp"
#include "native_tab_slot_growth_policy.hpp"

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
#include "addresses.hpp"
#include "default_tab_icon.hpp"
#include "hooks.hpp"
#include "native_tab_catalogue.hpp"
#include "native_tabs.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"
#include "row_update_state.hpp"
#include "tab_update_state.hpp"
#endif

#include <Windows.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace erui::native {
struct PreservedSharedMenuAddresses;
}

namespace {

using erui::detail::TabBoundaryBehavior;
using erui::detail::TabDirection;
using erui::detail::DeferredTabPresentationRefresh;
using erui::detail::DeferredTabPresentationRefreshPoll;
using erui::detail::Rgba8HostRemovalPlan;
using erui::detail::Rgba8RapidEpochHandoffState;
using erui::detail::Rgba8RetirementLimits;
using erui::detail::TabOverflowIndicator;
using erui::detail::TabPointerDwell;
using erui::detail::TabPointerDwellEdge;
using erui::detail::TabPointerDwellPhase;
using erui::detail::TabPointerDwellTiming;
using erui::detail::TabTransitionCommitResult;
using erui::detail::TabTransitionInputAction;
using erui::detail::TabTransitionSelectorAnchor;
using erui::detail::TabViewport;
using erui::detail::TabViewportChange;
using erui::detail::NativeTabSlotGrowthPlan;
using erui::detail::invalid_tab_index;
using erui::detail::native_tab_active_slot_target;
using erui::detail::plan_rgba8_host_removal;
using erui::detail::plan_rgba8_retirement_reconciliation;
using erui::detail::plan_native_tab_slot_growth;
using erui::detail::can_begin_vector_only_epoch_during_rgba8_retirement;
using erui::detail::should_cancel_pointer_dwell_for_composite_input;
using erui::detail::should_use_transparent_initial_raster_placeholder;
using erui::detail::tab_overflow_indicator;
using erui::detail::tab_overflow_indicators_changed;
using erui::detail::tab_transition_input_action;
using erui::detail::tab_transition_selector_anchor;

// Every native tab coordinate is selected from an exact executable identity.
// Do not derive a new build from an older one by a blanket RVA delta: the
// 2.7.2.0 image has several independently shifted code, vtable, and data
// regions. Keeping the complete audited coordinates together makes support
// additive and leaves the proven 2.7.1.0 path byte-for-byte address stable.
struct NativeTabsBuildProfile {
    const char* version{};
    std::uint32_t timestamp{};
    std::size_t image_size{};

    std::uintptr_t selection_callback{};
    std::uintptr_t composite_frame{};
    std::uintptr_t selector_select{};
    std::uintptr_t input_dispatch_frame_begin{};
    std::uintptr_t pointer_dispatch_result{};
    std::uintptr_t horizontal_navigation_commit{};
    std::uintptr_t selection_dispatch{};
    std::uintptr_t grid_hit_test{};
    std::uintptr_t get_pointer_position{};
    std::uintptr_t native_pointer_select{};
    std::uintptr_t pointer_cell_sample{};
    std::uintptr_t switch_panel{};
    std::uintptr_t switch_panel_state_copy_signature{};
    std::uintptr_t switch_panel_state_copy{};
    std::uintptr_t switch_panel_state_copy_resume{};
    std::uintptr_t switch_panel_visibility_signature{};
    std::uintptr_t switch_panel_visibility_decision{};
    std::uintptr_t refresh_title{};
    std::uintptr_t resolve_path{};
    std::uintptr_t value_exists{};
    std::uintptr_t present_tab{};
    std::uintptr_t make_icon_descriptor{};
    std::uintptr_t texture_lookup_call{};
    std::uintptr_t image_bind_call{};
    std::uintptr_t texture_lookup{};
    std::uintptr_t image_bind{};

    std::uintptr_t scaleform_constructor{};
    std::uintptr_t scaleform_get_member{};
    std::uintptr_t scaleform_set_position{};
    std::uintptr_t scaleform_set_visible{};
    std::uintptr_t scaleform_destructor{};
    std::uintptr_t raw_image_create{};
    std::uintptr_t raw_image_get_data{};
    std::uintptr_t image_data_clear{};
    std::uintptr_t raw_image_release{};
    std::uintptr_t raw_image_vtable{};
    std::uintptr_t raw_image_get_type{};
    std::uintptr_t image_resource_constructor{};
    std::uintptr_t image_resource_add_ref{};
    std::uintptr_t image_resource_release{};
    std::uintptr_t image_resource_deleting_destructor{};
    std::uintptr_t image_resource_allocation_call{};
    std::uintptr_t image_resource_vtable{};
    std::uintptr_t image_delegate_vtable{};
    std::uintptr_t scaleform_global_heap{};

    std::uintptr_t top_vtable{};
    std::uintptr_t category_record_vtable{};
    std::uintptr_t selector_vtable{};
    std::uintptr_t option_setting_panel_vtable{};
    std::uintptr_t scene_obj_proxy_vtable{};
    std::uintptr_t scene_obj_proxy_get_value{};

    std::uintptr_t as3_value_interface_vtable{};
    std::uintptr_t as3_value_get_member{};
    std::uintptr_t as3_value_set_member{};
    std::uintptr_t as3_value_invoke{};
    std::uintptr_t as3_value_get_movie{};
    std::uintptr_t as3_movie_vtable{};
    std::uintptr_t as3_movie_create_object{};
    std::uintptr_t static_shape_vtable{};
    std::uintptr_t alternate_authored_icon_vtable{};
    std::uintptr_t dynamic_shape_vtable{};

    std::uintptr_t graphics_panel_materializer{};

    // Configuration construction seams used only when the immutable player
    // configuration requests more than the eight stock physical slots.  Keep
    // the complete ownership-sensitive profile beside the viewport profile:
    // no coordinate is inferred from a neighbouring executable build.
    std::uintptr_t top_constructor{};
    std::uintptr_t early_slot_growth{};
    std::uintptr_t first_count_publication{};
    std::uintptr_t selector_geometry_dispatch{};
    std::uintptr_t post_geometry_observation{};
    std::uintptr_t count_propagation_dispatch{};
    std::uintptr_t post_constructor_observation{};
    std::uintptr_t category_append{};
    std::uintptr_t category_count_getter{};
    std::uintptr_t selector_set_active_count{};
    std::uintptr_t selector_geometry_enumerator{};
    std::uintptr_t category_list_vtable{};
    std::uintptr_t scene_obj_proxy_value_vtable{};
};

constexpr std::array<NativeTabsBuildProfile, 2>
    kNativeTabsBuildProfiles{{
        {
            "2.7.1.0", 0x6A96B418, 0x5E0DA00,
            0x968510, 0x93D5E0, 0x739B90, 0x73A196, 0x73A1C7,
            0x73A358, 0x73A394, 0x737AE0, 0x758940, 0x73CA60,
            0x73B880, 0x93C900, 0x93C932, 0x93C93C, 0x93C963,
            0x93C977, 0x93C989, 0x968C40, 0x74B140, 0x733FA0,
            0x967DE0, 0x73E230, 0x74CCDF, 0x74CDC7, 0xD65C00,
            0xD833F0,
            0xD814A0, 0xD816F0, 0xD85BD0, 0xD86280, 0xD81600,
            0x114A820, 0x114AA40, 0x1146AD0, 0x112D640, 0x2CC27E0,
            0x114B9C0, 0xD61C90, 0x113D9E0, 0x113DA00, 0xD62340,
            0xD65DA5, 0x2BBAAF8, 0x2BBA9E8, 0x45972D0,
            0x2B16B48, 0x2AD6F20, 0x2A94438, 0x2B14888, 0x2A97AF0,
            0x74C930,
            0x2CBF988, 0x10DDD10, 0x10DE0D0, 0x10DE510, 0x10E26D0,
            0x2CBE5C0, 0x10D4460, 0x2CBD4E0, 0x2CC6268, 0x2CBD158,
            0x95C050,
            0x9672C0, 0x96773F, 0x967757, 0x967935, 0x96793E,
            0x96795B, 0x8074A7, 0x968CF0, 0x968DA0, 0x739C10,
            0x738AB0, 0x2B16B10, 0x2A97AD8,
        },
        {
            "2.7.2.0", 0x6AABF7E7, 0x5DFD600,
            0x9689C0, 0x93DA90, 0x73A040, 0x73A646, 0x73A677,
            0x73A808, 0x73A844, 0x737F90, 0x758DF0, 0x73CF10,
            0x73BD30, 0x93CDB0, 0x93CDE2, 0x93CDEC, 0x93CE13,
            0x93CE27, 0x93CE39, 0x9690F0, 0x74B5F0, 0x734450,
            0x968290, 0x73E6E0, 0x74D18F, 0x74D277, 0xD672F0,
            0xD84AE0,
            0xD82B90, 0xD82DE0, 0xD872C0, 0xD87970, 0xD82CF0,
            0x114BF10, 0x114C130, 0x11481C0, 0x112ED30, 0x2CC4A90,
            0x114D0B0, 0xD63380, 0x113F0D0, 0x113F0F0, 0xD63A30,
            0xD67495, 0x2BBCDA8, 0x2BBCC98, 0x459A2F0,
            0x2B18C38, 0x2AD9010, 0x2A96528, 0x2B16978, 0x2A99BE0,
            0x74CDE0,
            0x2CC1C38, 0x10DF400, 0x10DF7C0, 0x10DFC00, 0x10E3DC0,
            0x2CC0870, 0x10D5B50, 0x2CBF790, 0x2CC8518, 0x2CBF408,
            0x95C500,
            0x967770, 0x967BEF, 0x967C07, 0x967DE5, 0x967DEE,
            0x967E0B, 0x807957, 0x9691A0, 0x969250, 0x73A0C0,
            0x738F60, 0x2B18C00, 0x2A99BC8,
        },
    }};
static_assert(
    kNativeTabsBuildProfiles[0].timestamp == 0x6A96B418 &&
    kNativeTabsBuildProfiles[0].selection_callback == 0x968510 &&
    kNativeTabsBuildProfiles[0].image_bind == 0xD833F0 &&
    kNativeTabsBuildProfiles[0].raw_image_vtable == 0x2CC27E0 &&
    kNativeTabsBuildProfiles[0].scaleform_global_heap == 0x45972D0 &&
    kNativeTabsBuildProfiles[0].top_vtable == 0x2B16B48 &&
    kNativeTabsBuildProfiles[0].as3_value_interface_vtable == 0x2CBF988 &&
    kNativeTabsBuildProfiles[0].graphics_panel_materializer == 0x95C050 &&
    kNativeTabsBuildProfiles[0].early_slot_growth == 0x96773F &&
    kNativeTabsBuildProfiles[0].category_append == 0x968CF0);
static_assert(
    kNativeTabsBuildProfiles[1].timestamp == 0x6AABF7E7 &&
    kNativeTabsBuildProfiles[1].selection_callback == 0x9689C0 &&
    kNativeTabsBuildProfiles[1].image_bind == 0xD84AE0 &&
    kNativeTabsBuildProfiles[1].raw_image_vtable == 0x2CC4A90 &&
    kNativeTabsBuildProfiles[1].scaleform_global_heap == 0x459A2F0 &&
    kNativeTabsBuildProfiles[1].top_vtable == 0x2B18C38 &&
    kNativeTabsBuildProfiles[1].as3_value_interface_vtable == 0x2CC1C38 &&
    kNativeTabsBuildProfiles[1].graphics_panel_materializer == 0x95C500 &&
    kNativeTabsBuildProfiles[1].early_slot_growth == 0x967BEF &&
    kNativeTabsBuildProfiles[1].category_append == 0x9691A0);

const NativeTabsBuildProfile* g_native_tabs_build_profile{};

[[nodiscard]] const NativeTabsBuildProfile* find_native_tabs_build_profile(
    std::uint32_t timestamp,
    std::size_t image_size) noexcept {
    for (const NativeTabsBuildProfile& profile : kNativeTabsBuildProfiles) {
        if (profile.timestamp == timestamp &&
            profile.image_size == image_size) {
            return &profile;
        }
    }
    return nullptr;
}

[[nodiscard]] const NativeTabsBuildProfile& tabs_build() noexcept {
    return *g_native_tabs_build_profile;
}

// A typed RVA handle keeps the call sites readable while making every value
// come from the one active, exact-build profile above. It deliberately has no
// fallback value: callers select a profile before any native validation or
// callback can run.
struct NativeTabsRva {
    std::uintptr_t NativeTabsBuildProfile::*member{};

    [[nodiscard]] operator std::uintptr_t() const noexcept {
        return tabs_build().*member;
    }
};

constexpr NativeTabsRva kSelectionCallbackRva{
    &NativeTabsBuildProfile::selection_callback};
constexpr NativeTabsRva kCompositeFrameRva{
    &NativeTabsBuildProfile::composite_frame};
constexpr NativeTabsRva kSelectorSelectRva{
    &NativeTabsBuildProfile::selector_select};
constexpr NativeTabsRva kInputDispatchFrameBeginRva{
    &NativeTabsBuildProfile::input_dispatch_frame_begin};
constexpr NativeTabsRva kPointerDispatchResultRva{
    &NativeTabsBuildProfile::pointer_dispatch_result};
constexpr NativeTabsRva kHorizontalNavigationCommitRva{
    &NativeTabsBuildProfile::horizontal_navigation_commit};
constexpr NativeTabsRva kSelectionDispatchRva{
    &NativeTabsBuildProfile::selection_dispatch};
constexpr NativeTabsRva kGridHitTestRva{
    &NativeTabsBuildProfile::grid_hit_test};
constexpr NativeTabsRva kGetPointerPositionRva{
    &NativeTabsBuildProfile::get_pointer_position};
constexpr NativeTabsRva kNativePointerSelectRva{
    &NativeTabsBuildProfile::native_pointer_select};
constexpr NativeTabsRva kPointerCellSampleRva{
    &NativeTabsBuildProfile::pointer_cell_sample};
constexpr NativeTabsRva kSwitchPanelRva{
    &NativeTabsBuildProfile::switch_panel};
constexpr NativeTabsRva kSwitchPanelStateCopyRva{
    &NativeTabsBuildProfile::switch_panel_state_copy};
constexpr NativeTabsRva kSwitchPanelStateCopyResumeRva{
    &NativeTabsBuildProfile::switch_panel_state_copy_resume};
constexpr NativeTabsRva kSwitchPanelVisibilityDecisionRva{
    &NativeTabsBuildProfile::switch_panel_visibility_decision};
constexpr NativeTabsRva kRefreshTitleRva{
    &NativeTabsBuildProfile::refresh_title};
constexpr NativeTabsRva kResolvePathRva{
    &NativeTabsBuildProfile::resolve_path};
constexpr NativeTabsRva kValueExistsRva{
    &NativeTabsBuildProfile::value_exists};
constexpr NativeTabsRva kPresentTabRva{
    &NativeTabsBuildProfile::present_tab};
constexpr NativeTabsRva kMakeIconDescriptorRva{
    &NativeTabsBuildProfile::make_icon_descriptor};
constexpr NativeTabsRva kTextureLookupCallRva{
    &NativeTabsBuildProfile::texture_lookup_call};
constexpr NativeTabsRva kImageBindCallRva{
    &NativeTabsBuildProfile::image_bind_call};
constexpr NativeTabsRva kTextureLookup271Rva{
    &NativeTabsBuildProfile::texture_lookup};
constexpr NativeTabsRva kImageBind271Rva{
    &NativeTabsBuildProfile::image_bind};
constexpr NativeTabsRva kScaleformConstructor271Rva{
    &NativeTabsBuildProfile::scaleform_constructor};
constexpr NativeTabsRva kScaleformGetMember271Rva{
    &NativeTabsBuildProfile::scaleform_get_member};
constexpr NativeTabsRva kScaleformSetPosition271Rva{
    &NativeTabsBuildProfile::scaleform_set_position};
constexpr NativeTabsRva kScaleformSetVisible271Rva{
    &NativeTabsBuildProfile::scaleform_set_visible};
constexpr NativeTabsRva kScaleformDestructor271Rva{
    &NativeTabsBuildProfile::scaleform_destructor};
constexpr NativeTabsRva kRawImageCreate271Rva{
    &NativeTabsBuildProfile::raw_image_create};
constexpr NativeTabsRva kRawImageGetData271Rva{
    &NativeTabsBuildProfile::raw_image_get_data};
constexpr NativeTabsRva kImageDataClear271Rva{
    &NativeTabsBuildProfile::image_data_clear};
constexpr NativeTabsRva kRawImageRelease271Rva{
    &NativeTabsBuildProfile::raw_image_release};
constexpr NativeTabsRva kRawImageVtableRva{
    &NativeTabsBuildProfile::raw_image_vtable};
constexpr NativeTabsRva kRawImageGetType271Rva{
    &NativeTabsBuildProfile::raw_image_get_type};
constexpr NativeTabsRva kScaleformImageResourceCtor271Rva{
    &NativeTabsBuildProfile::image_resource_constructor};
constexpr NativeTabsRva kScaleformImageResourceAddRef271Rva{
    &NativeTabsBuildProfile::image_resource_add_ref};
constexpr NativeTabsRva kScaleformImageResourceRelease271Rva{
    &NativeTabsBuildProfile::image_resource_release};
constexpr NativeTabsRva kScaleformImageResourceDeletingDestructor271Rva{
    &NativeTabsBuildProfile::image_resource_deleting_destructor};
constexpr NativeTabsRva kScaleformImageResourceAllocationCall271Rva{
    &NativeTabsBuildProfile::image_resource_allocation_call};
constexpr NativeTabsRva kScaleformImageResourceVtable271Rva{
    &NativeTabsBuildProfile::image_resource_vtable};
constexpr NativeTabsRva kScaleformImageDelegateVtable271Rva{
    &NativeTabsBuildProfile::image_delegate_vtable};
constexpr NativeTabsRva kScaleformGlobalHeap271Rva{
    &NativeTabsBuildProfile::scaleform_global_heap};
constexpr NativeTabsRva kTopVtableRva{
    &NativeTabsBuildProfile::top_vtable};
constexpr NativeTabsRva kCategoryRecordVtableRva{
    &NativeTabsBuildProfile::category_record_vtable};
constexpr NativeTabsRva kSelectorVtableRva{
    &NativeTabsBuildProfile::selector_vtable};
constexpr NativeTabsRva kOptionSettingPanelVtableRva{
    &NativeTabsBuildProfile::option_setting_panel_vtable};
constexpr NativeTabsRva kSceneObjProxyVtableRva{
    &NativeTabsBuildProfile::scene_obj_proxy_vtable};
constexpr NativeTabsRva kSceneObjProxyGetValueRva{
    &NativeTabsBuildProfile::scene_obj_proxy_get_value};
constexpr NativeTabsRva kAs3ValueInterfaceVtable271Rva{
    &NativeTabsBuildProfile::as3_value_interface_vtable};
constexpr NativeTabsRva kAs3ValueGetMember271Rva{
    &NativeTabsBuildProfile::as3_value_get_member};
constexpr NativeTabsRva kAs3ValueSetMember271Rva{
    &NativeTabsBuildProfile::as3_value_set_member};
constexpr NativeTabsRva kAs3ValueInvoke271Rva{
    &NativeTabsBuildProfile::as3_value_invoke};
constexpr NativeTabsRva kAs3ValueGetMovie271Rva{
    &NativeTabsBuildProfile::as3_value_get_movie};
constexpr NativeTabsRva kAs3MovieVtable271Rva{
    &NativeTabsBuildProfile::as3_movie_vtable};
constexpr NativeTabsRva kAs3MovieCreateObject271Rva{
    &NativeTabsBuildProfile::as3_movie_create_object};
constexpr NativeTabsRva kStaticShapeVtable271Rva{
    &NativeTabsBuildProfile::static_shape_vtable};
constexpr NativeTabsRva kAlternateAuthoredIconVtable271Rva{
    &NativeTabsBuildProfile::alternate_authored_icon_vtable};
constexpr NativeTabsRva kDynamicShapeVtable271Rva{
    &NativeTabsBuildProfile::dynamic_shape_vtable};
constexpr NativeTabsRva kTopConstructorRva{
    &NativeTabsBuildProfile::top_constructor};
constexpr NativeTabsRva kEarlySlotGrowthRva{
    &NativeTabsBuildProfile::early_slot_growth};
constexpr NativeTabsRva kFirstCountPublicationRva{
    &NativeTabsBuildProfile::first_count_publication};
constexpr NativeTabsRva kSelectorGeometryDispatchRva{
    &NativeTabsBuildProfile::selector_geometry_dispatch};
constexpr NativeTabsRva kPostGeometryObservationRva{
    &NativeTabsBuildProfile::post_geometry_observation};
constexpr NativeTabsRva kCountPropagationDispatchRva{
    &NativeTabsBuildProfile::count_propagation_dispatch};
constexpr NativeTabsRva kPostConstructorObservationRva{
    &NativeTabsBuildProfile::post_constructor_observation};
constexpr NativeTabsRva kCategoryAppendRva{
    &NativeTabsBuildProfile::category_append};
constexpr NativeTabsRva kCategoryCountGetterRva{
    &NativeTabsBuildProfile::category_count_getter};
constexpr NativeTabsRva kSelectorSetActiveCountRva{
    &NativeTabsBuildProfile::selector_set_active_count};
constexpr NativeTabsRva kSelectorGeometryEnumeratorRva{
    &NativeTabsBuildProfile::selector_geometry_enumerator};
constexpr NativeTabsRva kCategoryListVtableRva{
    &NativeTabsBuildProfile::category_list_vtable};
constexpr NativeTabsRva kSceneObjProxyValueVtableRva{
    &NativeTabsBuildProfile::scene_obj_proxy_value_vtable};

// The GridControl frame clears provenance before it enters input dispatch.
// Pointer selection is published by the earlier input helper. Non-pointer
// horizontal navigation converges at one validated commit seam carrying a
// signed x delta. These seams carry exact provenance into the ordinary
// selection callback without guessing from the resulting index or timing.
// Every native input path has finished choosing a physical tab when execution
// reaches this comparison. R12D is the physical selection from frame entry,
// RDI is the selector, and [RDI+0xD4] is the final selection. A timed pointer
// dwell uses the same native cell setter as the ordinary pointer path just
// before this comparison; Elden Ring then owns its normal change dispatch.
// The GridControl pointer path has already converted the native cursor
// coordinates and completed its per-cell hit test at this seam. RSI is the
// selector, signed EAX is the hovered physical cell (or -1), and the two
// floats at RSP+0x20/RSP+0x24 are the exact coordinates used by that hit test.
// During an armed dwell, rewriting EAX to -1 takes the native no-hit branch
// and suppresses only repaint-induced reselection under a stationary cursor.
// Scaleform::Render::RawImage is a system-memory image and does not require
// the DrawableImage provider that this Configuration movie lacks. The first
// RawImage gate first creates, inspects, fills, and verifies an immutable
// system-memory image. The current visual follow-up wraps that exact image in
// the native ScaleformImageResource used by the game's texture repository,
// keeps one process-lifetime owner, and returns ordinary temporary references
// through the validated texture-lookup contract. Elden Ring's unchanged image
// binder then paints a runtime-created flash.display.Shape owned by the same
// tab-icon parent; the authored static Shape is retained only as a fallback.
constexpr std::size_t kScaleformImageResourceSize = 0x98;

constexpr std::size_t kMovieContextOffset = 0x120;
constexpr std::size_t kSelectorOffset = 0x0A38;
constexpr std::size_t kSelectorActiveCountOffset = 0x00D0;
constexpr std::size_t kSelectorSelectedIndexOffset = 0x00D4;
constexpr std::size_t kSelectorHorizontalExtentOffset = 0x00D8;
constexpr std::size_t kSelectorVerticalExtentOffset = 0x00DC;
constexpr std::size_t kCategoryListOffset = 0x1200;
constexpr std::size_t kCategoryStorageOffset = 0x1208;
constexpr std::size_t kCategoryCountOffset = 0x1760;
constexpr std::size_t kCompositeOffset = 0x1768;
constexpr std::size_t kTopControllerOffset = 0x1870;
constexpr std::size_t kCompositePanelCacheOffset = 0x68;
constexpr std::size_t kCompositeCurrentPanelOffset = 0xB8;
constexpr std::size_t kCategoryRecordSize = 0x88;
constexpr std::size_t kTitleOffset = 0x08;
constexpr std::size_t kCategoryIdOffset = 0x40;
constexpr std::size_t kPresentationOffset = 0x48;
constexpr std::size_t kPresentationSize = 0x40;
constexpr std::size_t kSceneObjProxySize = 0x60;
constexpr std::size_t kSceneObjProxyValueOffset = 0x28;
constexpr std::size_t kPanelVisualCapacityOffset = 0xB14;
constexpr std::size_t kPanelNativeRowCountOffset = 0x1AF0;

// Elden Ring contributes eight stock logical routes. This is deliberately
// distinct from the live number of selectable physical cells exposed by the
// current Configuration movie. The native category container has room for at
// most ten records; selector + 0xD0 decides how many are active per epoch.
constexpr std::size_t kStockLogicalTabCount = 8;
constexpr std::size_t kPhysicalTabCapacity = 10;
constexpr std::size_t kTabHostCapacity = kPhysicalTabCapacity * 2U;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
static_assert(
    kStockLogicalTabCount == erui::detail::native_tab_stock_slot_count);
static_assert(
    kPhysicalTabCapacity ==
        erui::detail::native_tab_physical_slot_capacity);
static_assert(
    kPhysicalTabCapacity == erui::detail::native_tab_growth_capacity);
static_assert(
    kTabHostCapacity == erui::detail::native_tab_physical_host_capacity);
#endif
using TabHostMask = std::uint32_t;
using AtomicTabHostMask = std::atomic_uint32_t;
// Native Graphics and each registered custom route sequentially reuse
// category 8's selector-11 rows-only carrier. Stock Network is synchronous
// parking. The standalone research fixture below remains fixed at 50 routes.
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
constexpr std::size_t kCustomTabCount = ERUI_CUSTOM_TAB_MAX_COUNT;
#else
constexpr std::size_t kCustomTabCount = 50;
#endif
constexpr std::size_t kLogicalTabCount =
    kStockLogicalTabCount + kCustomTabCount;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
constexpr bool kStressQuietSuccessLogging = false;
#else
constexpr bool kStressQuietSuccessLogging = kCustomTabCount >= 40;
#endif
constexpr std::size_t kTitleCapacity = 96;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
static_assert(
    kTitleCapacity == ERUI_CUSTOM_TAB_TITLE_MAX_CODE_UNITS + 1U,
    "native tab title storage must match the public API 1.3 contract");
#endif
constexpr std::size_t kNavigationMargin = 1;
static_assert(
    kTabHostCapacity <= std::numeric_limits<TabHostMask>::digits);
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
static_assert(kCustomTabCount <=
    std::numeric_limits<std::uint8_t>::max());
static_assert(kCustomTabCount < std::numeric_limits<std::uint64_t>::digits);
static_assert(kCustomTabCount <= 99U);
#endif
constexpr std::uint64_t kInPlaceRebindRetirementTimeoutFrames = 120;
constexpr std::uint64_t kPointerDwellInitialDelayMs = 250;
constexpr std::uint64_t kPointerDwellRepeatDelayMs = 200;
constexpr std::uint64_t kPointerDwellDecayMs = 20;
constexpr std::uint64_t kPointerDwellMinimumDelayMs = 50;
constexpr std::array<std::int32_t, kStockLogicalTabCount>
    kExpectedStockCategories{0, 1, 2, 3, 5, 6, 7, 8};
constexpr bool is_research_custom_category(std::int32_t category) noexcept {
    return category == 8;
}
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
extern const erui::detail::NativeTabCatalogue* g_native_tab_catalogue;
extern std::unique_ptr<erui::detail::NativeTabRuntimeState>
    g_native_tab_runtime;
extern std::shared_ptr<const erui::detail::TabRuntimeGeneration>
    g_tab_projection_generation;
extern thread_local const erui::detail::TabRuntimeGeneration*
    g_rgba8_staging_generation;
#endif
[[nodiscard]] std::size_t active_custom_tab_count() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    return g_native_tab_runtime ? g_native_tab_runtime->size() : 0U;
#else
    return kCustomTabCount;
#endif
}
[[nodiscard]] std::size_t active_visible_custom_tab_count() noexcept;
[[nodiscard]] std::size_t active_logical_tab_count() noexcept {
    return kStockLogicalTabCount + active_visible_custom_tab_count();
}
[[nodiscard]] std::size_t resource_logical_tab_count() noexcept {
    return kStockLogicalTabCount + active_custom_tab_count();
}

[[nodiscard]] std::size_t active_physical_tab_count() noexcept;
[[nodiscard]] std::size_t active_tab_host_count() noexcept {
    return active_physical_tab_count() * 2U;
}
[[nodiscard]] TabHostMask active_tab_host_mask() noexcept {
    const std::size_t count = active_tab_host_count();
    return count == 0U
        ? TabHostMask{0}
        : (TabHostMask{1} << count) - TabHostMask{1};
}
constexpr std::uint32_t kMaximumTransitionLogs = 512;
constexpr std::uint32_t kMaximumFailureLogs = 16;
constexpr std::uint32_t kMaximumRecordMismatchLogs = 8;
constexpr std::uint32_t kMaximumWrapDiagnosticLogs = 36;
constexpr std::uint32_t kMaximumAliasLogs = 128;
constexpr std::uint32_t kMaximumImageBindLogs = 1024;
constexpr std::uint32_t kMaximumNativeImageResourceLogs = 8;
constexpr std::uint32_t kMaximumShapeAttachLogs = 1024;
constexpr std::uint32_t kMaximumStateIsolationLogs = 64;
constexpr std::uint32_t kMaximumAliasVisibilityLogs = 64;
constexpr std::uint32_t kMaximumCarrierNormalizationLogs = 32;
constexpr std::uint32_t kMaximumCarrierSelectionLogs = 64;
constexpr std::uint32_t kMaximumPointerSampleLogs = 192;
constexpr std::uint32_t kMaximumPointerDwellLogs = 1024;
constexpr std::uint64_t kPointerPollReportMs = 1000;
constexpr std::int32_t kProbeBitmapWidth = 32;
constexpr std::int32_t kProbeBitmapHeight = 32;
// A single post-native-composite BitmapData.setPixel32 call deterministically
// blackened the renderer even though Invoke returned true, no SEH was raised,
// and the exact rooted object survived the following frame. Keep the failed
// gate in the research source, but make deployed builds return to the proven
// ownership-only baseline until a different pixel-upload boundary is isolated.
constexpr bool kEnableBitmapDataPixelMutationProbe = false;
// The ownership-only BitmapData lifecycle is proven, and the passive VM probe
// found both its DrawableImage context and provider null. Keep that experiment
// available as research history, but do not construct another BitmapData while
// the next gate characterizes a known-good immutable native image resource.
constexpr bool kEnableBitmapDataOwnershipProbe = false;
constexpr bool kEnableRawImageLifecycleProbe = true;
constexpr bool kEnableNativeImageResourceLifecycleProbe = true;

constexpr std::uint32_t kTabContentResearchMarker = 0x54414231;
constexpr char kTabContentResearchMarkerExport[] =
    "ERUI_TabContentResearchProbeMarker";

// The live probe checks every AS3 pointer against the active exact-build
// profile before making its one harmless Invoke.
// Network and Keyboard/Mouse can retain this second exact authored-icon
// display-object type when a synthetic presentation is painted directly into
// their physical cells. Static comparison shows that it shares 33 of its
// first 40 virtual entries with the already-proven StaticShape target. The
// probe never calls either native vtable; it operates through the separately
// validated AS3 bridge and uses this RVA only as a narrow target allowlist.

constexpr std::int32_t kCrossIconId = 90001;
constexpr std::int32_t kOverflowIconId = 90002;
constexpr std::int32_t kFirstRgba8IconId = 91000;

template <std::size_t Count>
constexpr std::array<std::int32_t, Count> make_rgba8_icon_ids() noexcept {
    std::array<std::int32_t, Count> result{};
    for (std::size_t index = 0; index < Count; ++index) {
        result[index] = kFirstRgba8IconId +
            static_cast<std::int32_t>(index);
    }
    return result;
}

template <std::size_t Capacity, std::size_t PrefixSize>
constexpr void make_fixed_decimal_name(
    std::array<wchar_t, Capacity>& output,
    const wchar_t (&prefix)[PrefixSize],
    std::uint32_t value,
    std::size_t digits) noexcept {
    static_assert(PrefixSize > 0);
    const std::size_t prefix_length = PrefixSize - 1U;
    for (std::size_t index = 0; index < prefix_length; ++index) {
        output[index] = prefix[index];
    }
    for (std::size_t digit = 0; digit < digits; ++digit) {
        const std::size_t reverse = digits - 1U - digit;
        output[prefix_length + reverse] =
            static_cast<wchar_t>(L'0' + value % 10U);
        value /= 10U;
    }
    output[prefix_length + digits] = L'\0';
}

template <std::size_t Count>
constexpr std::array<std::array<wchar_t, 32>, Count>
make_rgba8_icon_resource_names() noexcept {
    std::array<std::array<wchar_t, 32>, Count> result{};
    for (std::size_t index = 0; index < Count; ++index) {
        make_fixed_decimal_name(
            result[index],
            L"MENU_ItemIcon_",
            static_cast<std::uint32_t>(kFirstRgba8IconId + index),
            5U);
    }
    return result;
}

template <std::size_t Count>
constexpr std::array<std::array<wchar_t, 48>, Count>
make_raw_image_resource_names() noexcept {
    std::array<std::array<wchar_t, 48>, Count> result{};
    for (std::size_t index = 0; index < Count; ++index) {
        make_fixed_decimal_name(
            result[index],
            L"ERNativeUI.TabProbe.RawRGBA8.",
            static_cast<std::uint32_t>(index + 1U),
            2U);
    }
    return result;
}

#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
constexpr auto kRgba8IconIds = make_rgba8_icon_ids<kCustomTabCount>();
constexpr auto kRgba8IconResourceNames =
    make_rgba8_icon_resource_names<kCustomTabCount>();
constexpr auto kRawImageResourceProbeNames =
    make_raw_image_resource_names<kCustomTabCount>();
static_assert(kRgba8IconIds.front() == 91000);
static_assert(kRgba8IconIds.back() == 91049);
static_assert(kRgba8IconIds.front() != kCrossIconId);
static_assert(kRgba8IconIds.front() != kOverflowIconId);
static_assert(kRgba8IconResourceNames.front()[14] == L'9');
static_assert(kRawImageResourceProbeNames.front()[29] == L'0');
#endif
// The forward-only path proves that Elden Ring's Graphics icon bounds are a
// stable canvas for every synthetic cross. A direct reverse wrap can otherwise
// expose the old Network, Controller, or Keyboard/Mouse bounds in each cell,
// which makes identical percentage-based drawings appear at different scales.
constexpr double kCrossCanvasLeft = -27.200;
constexpr double kCrossCanvasTop = -23.050;
constexpr double kCrossCanvasWidth = 54.599;
constexpr double kCrossCanvasHeight = 48.100;
constexpr wchar_t kCrossIconResource[] = L"MENU_ItemIcon_90001";
constexpr wchar_t kOverflowIconResource[] = L"MENU_ItemIcon_90002";
// Use a known-native property icon for the first alias experiment. No client
// bytes or custom texture allocation are involved yet.
constexpr wchar_t kAliasedNativeIconResource[] = L"MENU_PropertyIcon_30050";
// Runtime Shapes use two parent-owned property banks.  A remap paints a
// changed target into the inactive bank while the active Shape remains in the
// display list, then removes the old bank only after the target has been
// attached and verified.  Scaleform therefore never gets a render boundary
// at which only the authored slot icon is visible.
constexpr char kInjectedIconProperties[2][48]{
    "__ernativeui_tab_viewport_probe_icon_a",
    "__ernativeui_tab_viewport_probe_icon_b",
};
constexpr char kInjectedBitmapDataProperty[] =
    "__ernativeui_tab_viewport_probe_bitmap_data";
constexpr char kAuthoredIconProperties[2][56]{
    "__ernativeui_tab_viewport_probe_authored_icon_a",
    "__ernativeui_tab_viewport_probe_authored_icon_b",
};

constexpr std::uint8_t kIconPropertyBankA = 0;
constexpr std::uint8_t kIconPropertyBankB = 1;

constexpr const char* injected_icon_property(
    std::uint8_t bank) noexcept {
    return kInjectedIconProperties[bank == kIconPropertyBankB ? 1 : 0];
}

constexpr const char* authored_icon_property(
    std::uint8_t bank) noexcept {
    return kAuthoredIconProperties[bank == kIconPropertyBankB ? 1 : 0];
}

// Non-viewport research probes remain pinned to bank A.  The production-path
// stress transaction below selects a bank explicitly for every tab host.
constexpr const char* kInjectedIconProperty = kInjectedIconProperties[0];
constexpr const char* kAuthoredIconProperty = kAuthoredIconProperties[0];

using SelectionCallbackFn = void(__fastcall*)(
    void* callable,
    const std::int32_t* physical_index);
using CompositeFrameFn = void(__fastcall*)(
    void* composite,
    float frame_value,
    const std::uint8_t* input_enabled);
using SelectorSelectFn = bool(__fastcall*)(
    void* selector,
    std::uint32_t physical_index,
    std::uint8_t suppress_selection_callbacks);
using SelectorSetActiveCountFn = void(__fastcall*)(
    void* selector,
    std::int32_t active_count);
struct PointerPosition {
    float x{};
    float y{};
};
static_assert(sizeof(PointerPosition) == 8);
using GetPointerPositionFn = PointerPosition*(__fastcall*)(
    const std::uint8_t* input_gate,
    PointerPosition* output);
using GridHitTestFn = std::int32_t(__fastcall*)(
    void* selector,
    const PointerPosition* point);
using NativePointerSelectFn = std::int32_t(__fastcall*)(
    void* selector,
    std::uint32_t physical_index);
using SwitchPanelFn = void(__fastcall*)(
    void* composite,
    std::int32_t category);
using ResearchParkingSwitchFn = bool(__cdecl*)(void*, std::int32_t);
using ResearchPrepareCarrierFn = bool(__cdecl*)(void*, std::size_t, std::uint64_t, std::int32_t*, ResearchParkingSwitchFn);
using ResearchVerifyCarrierFn = bool(__cdecl*)(void*, std::size_t, std::int32_t);
using ResearchCarrierVersionFn = std::uint32_t(__cdecl*)();
ResearchPrepareCarrierFn g_research_prepare_carrier{};
ResearchVerifyCarrierFn g_research_verify_carrier{};
SafetyHookInline g_research_switch_hook{};
std::atomic<SwitchPanelFn> g_original_research_switch{};
bool g_research_explicit_switch{};
bool g_research_switch_succeeded{};
bool g_research_failed{};
std::uint64_t g_research_carrier_teardown_generation{};
std::uint64_t g_research_completed_composite_frames{};
using RefreshTitleFn = void(__fastcall*)(void* top);
using ResolvePathFn = void*(__fastcall*)(
    void* movie_context,
    void* destination,
    const char* path_format,
    ...);
using ValueExistsFn = bool(__fastcall*)(void* scene_obj_proxy);
using PresentTabFn = void(__fastcall*)(
    void* top_reference,
    void* scene_obj_proxy,
    std::uint32_t physical_index);
using MakeIconDescriptorFn = void*(__fastcall*)(
    void* descriptor,
    std::int32_t icon_id);
using TextureLookupFn = void**(__fastcall*)(
    void* repository,
    void** out_resource,
    const wchar_t* resource_name);
using ImageBindFn = bool(__fastcall*)(
    void* scaleform_value,
    void** resource,
    const float* rectangle);
using ValueInvokeFn = bool(__fastcall*)(
    void* object_interface,
    void* object_payload,
    void* result,
    const char* method_name,
    const void* arguments,
    std::size_t argument_count,
    bool is_display_object);
using ValueSetMemberFn = bool(__fastcall*)(
    void* object_interface,
    void* object_payload,
    const char* member_name,
    const void* value,
    bool is_display_object);
using ValueGetMovieFn = void*(__fastcall*)(void* object_interface);
using MovieCreateObjectFn = void(__fastcall*)(
    void* movie_root,
    void* out_value,
    const char* class_name,
    const void* arguments,
    std::uint32_t argument_count);
using ScaleformConstructorFn = void*(__fastcall*)(void* nested_value);
using ScaleformGetMemberFn = void*(__fastcall*)(
    void* source_value,
    void* destination_value,
    const char* const* member_name);
using ScaleformSetPositionFn = void*(__fastcall*)(
    void* scaleform_value,
    float x,
    float y);
using ScaleformSetVisibleFn = void*(__fastcall*)(
    void* scaleform_value,
    std::uint8_t visible);
using ScaleformDestructorFn = void(__fastcall*)(void* nested_value);
using RawImageCreateFn = void*(__fastcall*)(
    std::uint32_t format,
    std::uint32_t mip_levels,
    const void* size,
    std::uint32_t use,
    void* heap,
    void* update_sync);
using RawImageGetDataFn = bool(__fastcall*)(void* image, void* image_data);
using ImageDataClearFn = void(__fastcall*)(void* image_data);
using RawImageReleaseFn = void(__fastcall*)(void* image);
using NativeHeapAllocFn = void*(__fastcall*)(
    void* heap,
    std::size_t size,
    void* allocation_info);
using ScaleformImageResourceCtorFn = void*(__fastcall*)(
    void* storage,
    const wchar_t* name,
    void* image,
    std::uint32_t kind);
using ScaleformImageResourceAddRefFn = void(__fastcall*)(void* resource);
using ScaleformImageResourceReleaseFn = void(__fastcall*)(void* resource);
using CategoryAppendFn = void(__fastcall*)(
    void* storage,
    const void* source_record);

struct RawImageSize {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct RawImagePlane {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uintptr_t pitch{};
    std::uintptr_t data_size{};
    std::byte* data{};
};

struct RawImageData {
    std::uint32_t format{};
    std::uint32_t use{};
    std::uint8_t flags{};
    std::uint8_t level_count{};
    std::uint16_t raw_plane_count{};
    std::uint32_t padding{};
    RawImagePlane* planes{};
    void* palette{};
    RawImagePlane plane0{};
};

struct Rgba8Pixel {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{};
};

[[nodiscard]] Rgba8Pixel rgba8_probe_pixel(
    std::size_t icon,
    std::uint32_t x,
    std::uint32_t y) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const erui::detail::TabRuntimeGeneration* const generation =
        g_rgba8_staging_generation
        ? g_rgba8_staging_generation
        : g_tab_projection_generation.get();
    if (!generation || icon >= active_custom_tab_count() ||
        x >= ERUI_TAB_ICON_WIDTH || y >= ERUI_TAB_ICON_HEIGHT) {
        return {0xFFU, 0x00U, 0xFFU, 0xFFU};
    }
    if (icon >= generation->entries.size()) {
        return {0xFFU, 0x00U, 0xFFU, 0xFFU};
    }
    const auto& specification = generation->entries[icon].icon;
    const std::uint8_t* pixels{};
    if (specification.source == erui::TabIconSource::fallback) {
        if (!specification.rgba8_pixels.empty()) {
            return {0xFFU, 0x00U, 0xFFU, 0xFFU};
        }
        pixels = erui::native::default_custom_tab_icon_rgba8.data();
    } else if (specification.source == erui::TabIconSource::rgba8 &&
               specification.rgba8_pixels.size() ==
                   ERUI_TAB_ICON_RGBA8_BYTE_COUNT) {
        pixels = specification.rgba8_pixels.data();
    } else {
        return {0xFFU, 0x00U, 0xFFU, 0xFFU};
    }
    const std::size_t offset =
        (static_cast<std::size_t>(y) * ERUI_TAB_ICON_WIDTH + x) * 4U;
    Rgba8Pixel pixel{
        pixels[offset + 0U],
        pixels[offset + 1U],
        pixels[offset + 2U],
        pixels[offset + 3U],
    };
    // Scaleform's RawImage path consumes premultiplied colour. Keep that
    // conversion private so clients always submit ordinary RGBA bytes.
    pixel.red = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(pixel.red) * pixel.alpha + 127U) / 255U);
    pixel.green = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(pixel.green) * pixel.alpha + 127U) / 255U);
    pixel.blue = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(pixel.blue) * pixel.alpha + 127U) / 255U);
    return pixel;
#else
    // Human-readable 01..50 labels make a long stress run auditable by eye.
    // The background is also an explicit alpha-semantics diagnostic. Its top
    // half stores straight RGB while its bottom half stores the same colour
    // premultiplied by a left-to-right 0..255 alpha ramp. Whichever half
    // blends smoothly identifies the convention consumed by Elden Ring's
    // Scaleform upload path. Opaque white digits remain readable in either
    // case, and alpha-zero pixels retain nonzero RGB in the straight half so
    // an ignored-alpha failure becomes an unmistakable rectangle.
    constexpr std::array<std::uint8_t, 10> kSevenSegmentMasks{
        0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
        0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU};
    if (icon >= kCustomTabCount) {
        return {0xFFU, 0x00U, 0xFFU, 0xFFU};
    }

    const std::uint32_t label = static_cast<std::uint32_t>(icon + 1U);
    const std::array<std::uint32_t, 2> digits{
        (label / 10U) % 10U,
        label % 10U};
    bool lit{};
    for (std::size_t digit_index = 0; digit_index < digits.size();
         ++digit_index) {
        const std::uint32_t left = digit_index == 0 ? 3U : 17U;
        const std::uint8_t segments =
            kSevenSegmentMasks[digits[digit_index]];
        const bool top = (segments & 0x01U) != 0 &&
            x >= left + 2U && x <= left + 9U && y >= 3U && y <= 5U;
        const bool upper_right = (segments & 0x02U) != 0 &&
            x >= left + 9U && x <= left + 11U && y >= 5U && y <= 14U;
        const bool lower_right = (segments & 0x04U) != 0 &&
            x >= left + 9U && x <= left + 11U && y >= 16U && y <= 25U;
        const bool bottom = (segments & 0x08U) != 0 &&
            x >= left + 2U && x <= left + 9U && y >= 25U && y <= 27U;
        const bool lower_left = (segments & 0x10U) != 0 &&
            x >= left && x <= left + 2U && y >= 16U && y <= 25U;
        const bool upper_left = (segments & 0x20U) != 0 &&
            x >= left && x <= left + 2U && y >= 5U && y <= 14U;
        const bool middle = (segments & 0x40U) != 0 &&
            x >= left + 2U && x <= left + 9U && y >= 14U && y <= 16U;
        lit = lit || top || upper_right || lower_right || bottom ||
            lower_left || upper_left || middle;
    }

    const std::uint8_t red = static_cast<std::uint8_t>(
        80U + (icon * 97U) % 176U);
    const std::uint8_t green = static_cast<std::uint8_t>(
        80U + (icon * 57U) % 176U);
    const std::uint8_t blue = static_cast<std::uint8_t>(
        80U + (icon * 137U) % 176U);
    if (lit) return {0xFFU, 0xFFU, 0xFFU, 0xFFU};

    const std::uint8_t alpha = static_cast<std::uint8_t>(
        (x * 255U) / 31U);
    if (y < 16U) return {red, green, blue, alpha};
    return {
        static_cast<std::uint8_t>((red * alpha + 127U) / 255U),
        static_cast<std::uint8_t>((green * alpha + 127U) / 255U),
        static_cast<std::uint8_t>((blue * alpha + 127U) / 255U),
        alpha};
#endif
}

[[nodiscard]] bool rgba8_probe_icon_has_alpha_profile(
    std::size_t icon) noexcept {
    const Rgba8Pixel straight_zero = rgba8_probe_pixel(icon, 0U, 0U);
    const Rgba8Pixel premultiplied_zero =
        rgba8_probe_pixel(icon, 0U, 31U);
    const Rgba8Pixel straight_opaque =
        rgba8_probe_pixel(icon, 31U, 0U);
    const Rgba8Pixel premultiplied_opaque =
        rgba8_probe_pixel(icon, 31U, 31U);
    const bool exact_discriminator = straight_zero.alpha == 0U &&
        (straight_zero.red != 0U || straight_zero.green != 0U ||
            straight_zero.blue != 0U) &&
        premultiplied_zero.red == 0U &&
        premultiplied_zero.green == 0U &&
        premultiplied_zero.blue == 0U &&
        premultiplied_zero.alpha == 0U &&
        straight_opaque.red == premultiplied_opaque.red &&
        straight_opaque.green == premultiplied_opaque.green &&
        straight_opaque.blue == premultiplied_opaque.blue &&
        straight_opaque.alpha == 0xFFU &&
        premultiplied_opaque.alpha == 0xFFU;
    if (!exact_discriminator) return false;

    bool transparent{};
    bool partial{};
    bool opaque{};
    for (std::uint32_t y = 0; y < 32U; ++y) {
        for (std::uint32_t x = 0; x < 32U; ++x) {
            const std::uint8_t alpha = rgba8_probe_pixel(icon, x, y).alpha;
            transparent = transparent || alpha == 0U;
            partial = partial || (alpha > 0U && alpha < 0xFFU);
            opaque = opaque || alpha == 0xFFU;
        }
    }
    return transparent && partial && opaque;
}

[[nodiscard]] std::uint64_t rgba8_probe_icon_checksum(
    std::size_t icon) noexcept {
    std::uint64_t checksum = 1469598103934665603ULL;
    for (std::uint32_t y = 0; y < 32U; ++y) {
        for (std::uint32_t x = 0; x < 32U; ++x) {
            const Rgba8Pixel pixel = rgba8_probe_pixel(icon, x, y);
            const std::array<std::uint8_t, 4> bytes{
                pixel.red, pixel.green, pixel.blue, pixel.alpha};
            for (const std::uint8_t value : bytes) {
                checksum ^= value;
                checksum *= 1099511628211ULL;
            }
        }
    }
    return checksum;
}

[[nodiscard]] bool catalogue_icon_requires_raster(
    std::size_t icon) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const erui::detail::TabRuntimeGeneration* const generation =
        g_rgba8_staging_generation
        ? g_rgba8_staging_generation
        : g_tab_projection_generation.get();
    return generation && icon < generation->entries.size() &&
        erui::native::custom_tab_icon_requests_raster(
            generation->entries[icon].icon.source);
#else
    return icon < kCustomTabCount;
#endif
}

template <std::size_t Count>
[[nodiscard]] std::array<std::uint64_t, Count>
make_rgba8_probe_icon_checksums() noexcept {
    std::array<std::uint64_t, Count> result{};
    for (std::size_t icon = 0; icon < Count; ++icon) {
        result[icon] = rgba8_probe_icon_checksum(icon);
    }
    return result;
}

[[nodiscard]] bool rgba8_probe_catalogue_is_unique_and_alpha_profiled(
    std::span<const std::uint64_t> checksums) noexcept {
    for (std::size_t icon = 0; icon < checksums.size(); ++icon) {
        if (!rgba8_probe_icon_has_alpha_profile(icon) ||
            checksums[icon] == 0) {
            return false;
        }
        for (std::size_t other = 0; other < icon; ++other) {
            if (checksums[icon] == checksums[other]) {
                return false;
            }
        }
    }
    return true;
}

static_assert(sizeof(RawImageSize) == 0x08);
static_assert(sizeof(RawImagePlane) == 0x20);
static_assert(offsetof(RawImagePlane, pitch) == 0x08);
static_assert(offsetof(RawImagePlane, data_size) == 0x10);
static_assert(offsetof(RawImagePlane, data) == 0x18);
static_assert(sizeof(RawImageData) == 0x40);
static_assert(offsetof(RawImageData, flags) == 0x08);
static_assert(offsetof(RawImageData, level_count) == 0x09);
static_assert(offsetof(RawImageData, raw_plane_count) == 0x0A);
static_assert(offsetof(RawImageData, planes) == 0x10);
static_assert(offsetof(RawImageData, palette) == 0x18);
static_assert(offsetof(RawImageData, plane0) == 0x20);

struct GameImage {
    std::uint8_t* base{};
    std::size_t size{};
    std::uint32_t timestamp{};
};

struct ShapeAttachProbeResult {
    bool resource_consumed{};
    bool reused_existing_shape{};
    bool created_new_shape{};
    bool custom_visible{};
    bool cleanup_exact{};
    bool bound{};
};

struct Rgba8ShapeAttachProbeResult {
    bool resource_consumed{};
    bool raster_bind_attempted{};
    bool raster_bound{};
    bool pre_bind_graphics_clear_required{};
    bool pre_bind_graphics_clear_attempted{};
    bool pre_bind_graphics_cleared{};
    bool pre_bind_old_raw_before_read{};
    std::uint32_t pre_bind_old_raw_before{};
    bool pre_bind_old_raw_after_read{};
    std::uint32_t pre_bind_old_raw_after{};
    bool pre_bind_old_raw_released_exact{};
    bool reference_before_read{};
    std::uint32_t reference_before{};
    bool reference_after_read{};
    std::uint32_t reference_after{};
    bool exact_reference_transition{};
    bool vector_fallback_visible{};
    bool custom_visible{};
    bool host_retained{};
    bool reused_existing_shape{};
    bool created_new_shape{};
    bool same_asset_bind_skipped{};
    bool cleanup_exact{};
    bool bound{};
    unsigned long graphics_clear_seh{};
    unsigned long consume_seh{};
};

struct RetainedRgba8Resource {
    void* image{};
    void* resource{};
    std::uint64_t checksum{};
    bool ready{};
};

// Every GFx wrapper participates in an intrusive ObjectInterface list while it
// is constructed. Keep the RGBA probe's wrappers in stable heap storage so a
// guarded native fault can retain that storage instead of returning with an
// intrusive link into a dead stack frame.
struct alignas(16) StableScaleformWrapper {
    std::array<std::byte, 0x38> value{};
    std::array<std::byte, 0x08> alignment_padding{};
};

static_assert(sizeof(StableScaleformWrapper) == 0x40);
static_assert(alignof(StableScaleformWrapper) == 16);

struct alignas(16) Rgba8ShapeWrapperArena {
    StableScaleformWrapper parent{};
    StableScaleformWrapper existing{};
    StableScaleformWrapper created{};
    StableScaleformWrapper graphics{};
    StableScaleformWrapper verified_icon{};
    StableScaleformWrapper verified_authored{};
    StableScaleformWrapper undefined{};
};

static_assert(alignof(Rgba8ShapeWrapperArena) == 16);

struct alignas(16) RawGfxValue {
    std::array<std::byte, 0x18> prefix{};
    std::uint32_t type{};
    std::uint32_t padding{};
    std::array<std::byte, 0x10> payload{};
};

static_assert(sizeof(RawGfxValue) == 0x30);

enum class TabLayer : std::uint8_t {
    none,
    front,
    back,
};

enum class VectorIconKind : std::uint8_t {
    none,
    cross,
    ellipsis,
};

enum class TabSelectionSource : std::uint8_t {
    unknown,
    pointer,
    navigation_previous,
    navigation_next,
};

struct RepaintContext {
    bool active{};
    void* top{};
    TabLayer layer{TabLayer::none};
    std::size_t physical{};
    std::size_t logical{invalid_tab_index};
};

struct PendingAliasedIcon {
    void* resource{};
    void* image{};
    VectorIconKind kind{VectorIconKind::none};
    RepaintContext context{};
    bool direct_rgba8{};
    bool transparent_vector_placeholder{};
    std::size_t rgba8_index{invalid_tab_index};
    std::size_t logical{invalid_tab_index};
    std::uint64_t checksum{};
    std::uint64_t presentation_epoch{};
};

struct LogicalTab {
    std::array<wchar_t, kTitleCapacity> title{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // Projection changes repaint a complete inactive icon/title mapping
    // before retiring the visible Shape bank. Keep a second process-owned
    // title buffer so a mixed title+order transaction never has to mutate the
    // string still borrowed by the source native records.
    std::array<wchar_t, kTitleCapacity> alternate_title{};
    bool alternate_title_active{};
#endif
    std::int32_t category{-1};
    std::array<std::byte, kPresentationSize> presentation{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const erui::detail::NativeTabCatalogueEntry* source{};
    erui::detail::NativeTabRuntimeEntry* custom_runtime{};
#endif
};

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
struct PinnedLogicalTabCatalogue {
    std::vector<LogicalTab> tabs{};
};
#endif

struct RecordFields {
    const wchar_t* title{};
    std::int32_t category{-1};
    std::array<std::byte, kPresentationSize> presentation{};
};

struct alignas(16) ResolvedTabProxy {
    std::array<std::byte, kSceneObjProxySize> storage{};
    bool constructed{};
    std::array<std::byte, 15> alignment_padding{};
};

static_assert(alignof(ResolvedTabProxy) == 16);
static_assert(sizeof(ResolvedTabProxy) == 0x70);

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
constexpr std::size_t kSlotGrowthProbeCellCount = 6U;

struct SlotGrowthMovieFingerprint final {
    void* proxy_owner{};
    void* object_interface{};
    std::uint32_t flags{};
    void* object_payload{};
    void* auxiliary{};
    void* owner_link{};
    void* movie_root{};
};

struct SlotGrowthCellProof final {
    SlotGrowthMovieFingerprint movie{};
    bool resolver_returned_destination{};
    bool existence_call_exact{};
    bool exists{};
    bool fingerprint_exact{};
    bool lineage_exact{};
    bool cleanup_exact{};
    unsigned long seh_code{};
};

struct SlotGrowthCellStorage final {
    ResolvedTabProxy proxy{};
    bool poisoned{};
};

struct SlotGrowthConstructionContext final {
    void* top{};
    void* scene_proxy{};
    SlotGrowthMovieFingerprint movie{};
    bool active{};
    bool title_menu{};
    bool early_seam_seen{};
    bool late_seam_seen{};
    bool mutation_attempted{};
    bool mutation_committed{};
    bool early_proof_exact{};
    bool late_proof_exact{};
    bool live_cells_symmetric{};
    std::size_t configured_minimum{kStockLogicalTabCount};
    std::size_t discovered_capacity{kStockLogicalTabCount};
    std::size_t active_target{kStockLogicalTabCount};
    std::size_t append_checkpoints{};
    std::uint64_t count_before{};
    std::int32_t active_before{-1};
};
#endif

struct ResolvedTabLayers {
    std::array<ResolvedTabProxy, kPhysicalTabCapacity> tab_list{};
    std::array<ResolvedTabProxy, kPhysicalTabCapacity> back_tab_list{};
};

struct alignas(16) InjectedIconDetachArena {
    ResolvedTabProxy parent{};
    StableScaleformWrapper injected{};
    StableScaleformWrapper authored{};
    StableScaleformWrapper graphics{};
    StableScaleformWrapper undefined{};
    StableScaleformWrapper verify_injected{};
    StableScaleformWrapper verify_authored{};
};

static_assert(alignof(InjectedIconDetachArena) == 16);

struct alignas(16) InPlaceRebindPreflightArena {
    ResolvedTabProxy parent{};
    StableScaleformWrapper injected{};
    StableScaleformWrapper authored{};
};

static_assert(alignof(InPlaceRebindPreflightArena) == 16);

struct alignas(16) InPlaceRebindClearArena {
    ResolvedTabProxy parent{};
    StableScaleformWrapper injected{};
    StableScaleformWrapper authored{};
    StableScaleformWrapper graphics{};
};

static_assert(alignof(InPlaceRebindClearArena) == 16);

enum class InPlaceRebindHostExpectation : std::uint8_t {
    untracked,
    vector_shape,
    rgba8_shape,
};

enum class InPlaceRgba8RebindStage : std::uint8_t {
    none,
    clearing,
    waiting_retirement,
    ready_to_repaint,
    repainting,
};

struct Rgba8ReplacementPlan {
    std::uint64_t transition_sequence{};
    std::size_t source_window{};
    std::size_t target_window{};
    TabHostMask source_injected_mask{};
    TabHostMask source_rgba_mask{};
    TabHostMask source_vector_mask{};
    TabHostMask raster_detach_mask{};
    TabHostMask vector_detach_mask{};
    TabHostMask expected_detach_mask{};
    TabHostMask retired_injected_mask{};
    TabHostMask target_injected_mask{};
    TabHostMask target_rgba_mask{};
    TabHostMask target_vector_mask{};
    TabHostMask fresh_host_mask{};
    TabHostMask preserved_host_mask{};
    TabHostMask audit_mask{};
    std::array<std::size_t, kTabHostCapacity> source_assets{};
    std::array<std::size_t, kTabHostCapacity> target_assets{};
    std::array<void*, kTabHostCapacity> source_resources{};
    std::array<void*, kTabHostCapacity> target_resources{};
};

struct InjectedHostLayout {
    TabHostMask injected_mask{};
    TabHostMask rgba_mask{};
    TabHostMask vector_mask{};
    std::array<std::size_t, kTabHostCapacity> assets{};
    std::array<void*, kTabHostCapacity> resources{};
};

using PendingTransitionCommitResult = TabTransitionCommitResult;

struct PendingTransition {
    bool active{};
    bool diagnose_wrap{};
    // A retained RGBA8 catalogue is published after the first native
    // composite. If that publication changes a currently visible custom icon
    // from its vector fallback to its real raster, schedule the same
    // ownership-safe Shape handoff as a viewport remap without pretending
    // that the logical selection or record window changed.
    bool presentation_refresh{};
    void* top{};
    TabViewport candidate{};
    std::uint32_t native_physical{};
    std::uint32_t desired_physical{};
    std::size_t provisional_logical{invalid_tab_index};
    std::size_t desired_logical{invalid_tab_index};
    std::size_t previous_logical{invalid_tab_index};
    std::int32_t provisional_category{-1};
    std::int32_t desired_category{-1};
    std::uint64_t pointer_dwell_ticket{};
    std::uint64_t requested_qpc{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // A projection refresh changes stable-custom -> visible-logical mapping
    // without changing any immutable icon resource. The source generation
    // remains globally authoritative until the inactive target Shape bank is
    // complete and the old bank has retired.
    bool projection_refresh{};
    bool icon_resource_refresh{};
    bool projection_selected_hidden{};
    std::shared_ptr<const erui::detail::TabRuntimeGeneration>
        projection_generation{};
#endif
};

struct PointerDwellNaturalIntent {
    bool active{};
    void* top{};
    TabPointerDwellEdge edge{TabPointerDwellEdge::none};
    std::uint32_t physical{};
    std::uint32_t x_bits{};
    std::uint32_t y_bits{};
};

struct PointerDwellTimedIntent {
    bool active{};
    void* top{};
    std::uint32_t physical{};
    std::size_t logical{invalid_tab_index};
};

struct PointerDwellAdapter {
    TabPointerDwell policy{TabPointerDwellTiming{
        kPointerDwellInitialDelayMs,
        kPointerDwellRepeatDelayMs,
        kPointerDwellDecayMs,
        kPointerDwellMinimumDelayMs,
    }};
    void* top{};
    TabDirection direction{TabDirection::next};
    std::uint64_t ticket_counter{};
    std::uint64_t transaction_ticket{};
    PointerDwellNaturalIntent natural_intent{};
    PointerDwellTimedIntent timed_intent{};
};

class ScopedBooleanTrue {
public:
    explicit ScopedBooleanTrue(bool& value) noexcept
        : value_(value), previous_(value) {
        value_ = true;
    }

    ~ScopedBooleanTrue() noexcept { value_ = previous_; }

    ScopedBooleanTrue(const ScopedBooleanTrue&) = delete;
    ScopedBooleanTrue& operator=(const ScopedBooleanTrue&) = delete;

private:
    bool& value_;
    bool previous_{};
};

enum class PanelStateIsolationReason : std::uint8_t {
    none,
    provisional,
    corrective,
};

struct PanelStateIsolationExpectation {
    bool active{};
    bool consumed{};
    void* composite{};
    void* source{};
    std::int32_t target_category{-1};
    PanelStateIsolationReason reason{PanelStateIsolationReason::none};
};

struct PanelSnapshot {
    void* panel{};
    void* vtable{};
    std::uint32_t visual_capacity{};
    std::uint64_t native_row_count{};
    bool pointer_read{};
    bool vtable_read{};
    bool option_setting{};
    bool fields_read{};
};

struct PanelCarrierSnapshot {
    void* proxy_vtable{};
    void* getter{};
    void* wrapper{};
    void* payload{};
    std::uint32_t wrapper_flags{};
    bool valid{};
};

struct RuntimeState {
    bool active{};
    void* top{};
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
    std::array<LogicalTab, kLogicalTabCount> catalogue{};
#endif
    std::array<std::byte, kPresentationSize> overflow_presentation{};
    TabViewport viewport{};
    PendingTransition pending{};
    DeferredTabPresentationRefresh catalogue_presentation_refresh{};
    std::uint64_t presentation_epoch{};
    std::uint64_t completed_shape_replacement_transitions{};
    bool destructive_transition_failed{};
    bool frozen_projection_retained_target{};
};

struct InjectedMovieFingerprint {
    void* object_interface{};
    void* owner_link{};
    void* movie_root{};
    std::uint64_t presentation_epoch{};
};

struct InPlaceRgba8RebindTransaction {
    bool active{};
    bool failed{};
    std::uint64_t transition_sequence{};
    InPlaceRgba8RebindStage stage{InPlaceRgba8RebindStage::none};
    void* top{};
    std::uint64_t presentation_epoch{};
    std::uint64_t source_generation{};
    std::uint64_t target_generation{};
    std::size_t source_window{};
    std::size_t target_window{};
    std::uint64_t clear_frame{};
    std::uint64_t requested_qpc{};
    std::uint64_t phase_a_started_qpc{};
    std::uint64_t detach_completed_qpc{};
    std::uint64_t retirement_completed_qpc{};
    std::uint64_t repaint_started_qpc{};
    std::uint64_t retirement_wait_frames{};
    TabHostMask expected_clear_mask{};
    TabHostMask completed_clear_mask{};
    TabHostMask expected_raster_clear_mask{};
    TabHostMask expected_vector_clear_mask{};
    TabHostMask source_injected_mask{};
    TabHostMask source_rgba_mask{};
    TabHostMask retired_injected_mask{};
    TabHostMask target_injected_mask{};
    TabHostMask target_rgba_mask{};
    TabHostMask audit_mask{};
    TabHostMask expected_host_mask{};
    TabHostMask completed_host_mask{};
    TabHostMask expected_rgba_presentation_mask{};
    TabHostMask completed_rgba_presentation_mask{};
    TabHostMask expected_vector_host_mask{};
    TabHostMask completed_vector_host_mask{};
    TabHostMask preserved_host_mask{};
    std::array<std::size_t, kTabHostCapacity> source_assets{};
    std::array<std::size_t, kTabHostCapacity> target_assets{};
    std::array<void*, kTabHostCapacity> source_resources{};
    std::array<void*, kTabHostCapacity> target_resources{};
};

struct PointerCellSampleTrace {
    bool initialized{};
    bool seen_this_dispatch{};
    std::int32_t natural_hit{-1};
    std::uint32_t natural_x_bits{};
    std::uint32_t natural_y_bits{};
    std::int32_t last_hit{-1};
    std::uint32_t last_x_bits{};
    std::uint32_t last_y_bits{};
    std::uint64_t last_position_change_ms{};
    std::uint64_t last_report_ms{};
    std::uint32_t dispatch_frames{};
    std::uint32_t successful_polls{};
    std::uint32_t natural_frames{};
    std::uint32_t fallback_frames{};
    std::uint32_t unavailable_frames{};
    std::uint32_t coordinate_changes{};
    std::uint32_t hit_changes{};
    std::uint32_t comparison_frames{};
    std::uint32_t coordinate_mismatches{};
    std::uint32_t hit_mismatches{};
    std::uint32_t poll_faults{};
    unsigned long last_poll_seh{};
};

struct NativePointerSample {
    std::int32_t hit{-1};
    std::uint32_t x_bits{};
    std::uint32_t y_bits{};
    unsigned long seh_code{};
};

struct DeferredBitmapDataProbe {
    bool scheduled{};
    void* top{};
    TabLayer layer{TabLayer::none};
    std::size_t physical{invalid_tab_index};
    std::size_t expected_logical{invalid_tab_index};
    std::uint64_t mapping_generation{};
    std::uint64_t scheduled_after_frame{};
    std::uint32_t scheduling_thread{};
};

enum class BitmapDataPropertyVerificationPhase : std::uint8_t {
    none = 0,
    pre_mutation_identity,
    post_mutation_identity,
};

const char* bitmapdata_property_verification_phase_name(
    BitmapDataPropertyVerificationPhase phase) noexcept {
    switch (phase) {
    case BitmapDataPropertyVerificationPhase::pre_mutation_identity:
        return "pre-mutation-identity";
    case BitmapDataPropertyVerificationPhase::post_mutation_identity:
        return "post-mutation-identity";
    default:
        return "none";
    }
}

struct BitmapDataPropertyVerification {
    BitmapDataPropertyVerificationPhase phase{
        BitmapDataPropertyVerificationPhase::none};
    void* top{};
    TabLayer layer{TabLayer::none};
    std::size_t physical{invalid_tab_index};
    std::size_t expected_logical{invalid_tab_index};
    std::uint64_t mapping_generation{};
    std::uint64_t created_frame{};
    std::uint64_t phase_armed_frame{};
    std::uint32_t creation_thread{};
    void* expected_payload{};
    // Passive diagnostics only. These are scalar addresses sampled from the
    // exact AS3 MovieRoot at creation; no VM-owned wrapper crosses the frame.
    std::uintptr_t vm_address{};
    std::uintptr_t drawable_image_context_address{};
    std::uintptr_t drawable_image_provider_address{};
};

struct BitmapDataVmContextSnapshot {
    std::uintptr_t vm_address{};
    std::uintptr_t drawable_image_context_address{};
    std::uintptr_t drawable_image_provider_address{};
    bool vm_read{};
    bool drawable_image_context_read{};
    bool drawable_image_provider_read{};
};

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
SRWLOCK g_log_lock = SRWLOCK_INIT;
GameImage g_game{};
RuntimeState g_runtime{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
const erui::detail::NativeTabCatalogue* g_native_tab_catalogue{};
std::unique_ptr<erui::detail::NativeTabRuntimeState> g_native_tab_runtime{};
std::unique_ptr<PinnedLogicalTabCatalogue> g_logical_tab_catalogue{};
std::shared_ptr<const erui::detail::TabRuntimeGeneration>
    g_tab_projection_generation{};
thread_local const erui::detail::TabRuntimeGeneration*
    g_rgba8_staging_generation{};
// Allocated once before hooks are exposed. Native image construction reuses
// this exact-sized scratch for both fresh-movie and same-movie transactions and
// never asks the C++ heap for storage from a UI hook.
std::vector<std::uint64_t> g_rgba8_expected_checksums_scratch{};
std::vector<RetainedRgba8Resource> g_rgba8_staged_resources_scratch{};
std::vector<std::uint8_t> g_rgba8_staged_changed_scratch{};
std::vector<RetainedRgba8Resource> g_rgba8_retired_resources_scratch{};
std::vector<std::uint32_t> g_rgba8_retired_debts_scratch{};
std::shared_ptr<const erui::detail::TabRuntimeGeneration>
    g_rgba8_staged_tab_generation{};
bool g_rgba8_staged_bank_ready{};
bool g_rgba8_retired_bank_active{};
std::uint64_t g_rgba8_staging_failed_epoch{};
std::uint64_t g_rgba8_retirement_wait_logged_epoch{};
std::uint64_t g_rgba8_retirement_wait_logged_revision{};
std::size_t g_navigation_margin{kNavigationMargin};
bool g_mouse_dwell_enabled{true};
TabPointerDwellTiming g_pointer_dwell_timing{
    kPointerDwellInitialDelayMs,
    kPointerDwellRepeatDelayMs,
    kPointerDwellDecayMs,
    kPointerDwellMinimumDelayMs,
};
std::atomic_bool g_native_tabs_installed{};
std::size_t g_minimum_tab_slots{kStockLogicalTabCount};
#else
constexpr std::size_t g_custom_tab_count{kCustomTabCount};
constexpr std::size_t g_navigation_margin{kNavigationMargin};
constexpr bool g_mouse_dwell_enabled{true};
constexpr TabPointerDwellTiming g_pointer_dwell_timing{
    kPointerDwellInitialDelayMs,
    kPointerDwellRepeatDelayMs,
    kPointerDwellDecayMs,
    kPointerDwellMinimumDelayMs,
};
#endif
// The first exact stock snapshot is copied into process-stable storage. A
// Scaleform movie can be recreated at the same native Configuration address
// while the live records still contain our mapped titles/descriptors; those
// records must be restored from this pristine catalogue, never reinterpreted
// as a fresh stock snapshot.
std::array<LogicalTab, kStockLogicalTabCount> g_preserved_stock_catalogue{};
bool g_preserved_stock_catalogue_ready{};

SafetyHookInline g_selection_hook{};
SafetyHookInline g_composite_frame_hook{};
SafetyHookInline g_texture_lookup_hook{};
SafetyHookInline g_image_bind_hook{};
SafetyHookMid g_panel_state_copy_hook{};
SafetyHookMid g_panel_alias_visibility_hook{};
SafetyHookMid g_input_dispatch_frame_begin_hook{};
SafetyHookMid g_pointer_dispatch_source_hook{};
SafetyHookMid g_horizontal_navigation_source_hook{};
SafetyHookMid g_selection_dispatch_hook{};
SafetyHookMid g_pointer_cell_sample_hook{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
SafetyHookMid g_slot_growth_early_hook{};
SafetyHookMid g_slot_growth_late_hook{};
SafetyHookMid g_slot_growth_post_constructor_hook{};
std::atomic_bool g_slot_growth_callbacks_armed{};
CategoryAppendFn g_category_append{};
thread_local bool g_inside_slot_growth_append{};
thread_local SlotGrowthConstructionContext g_slot_growth_construction{};
thread_local std::array<
    SlotGrowthCellStorage,
    kSlotGrowthProbeCellCount> g_slot_growth_cell_storage{};
#endif
// Hook creation is transactional only until the first target is exposed. Once
// enable() has been attempted, hook objects and all borrowed runtime storage
// remain process-lifetime. Every callback stays inert until the complete set
// is armed, so a partially enabled set is an exact native pass-through.
std::atomic_bool g_viewport_hooks_exposed{};
std::atomic_bool g_viewport_callbacks_armed{};
std::atomic<SelectionCallbackFn> g_original_selection{};
std::atomic<CompositeFrameFn> g_original_composite_frame{};
std::atomic<TextureLookupFn> g_original_texture_lookup{};
std::atomic<ImageBindFn> g_original_image_bind{};

SelectorSelectFn g_selector_select{};
SelectorSetActiveCountFn g_selector_set_active_count{};
GetPointerPositionFn g_get_pointer_position{};
GridHitTestFn g_grid_hit_test{};
NativePointerSelectFn g_native_pointer_select{};
SwitchPanelFn g_switch_panel{};
RefreshTitleFn g_refresh_title{};
ResolvePathFn g_resolve_path{};
ValueExistsFn g_value_exists{};
PresentTabFn g_present_tab{};
MakeIconDescriptorFn g_make_icon_descriptor{};
ScaleformConstructorFn g_construct_scaleform_value{};
ScaleformGetMemberFn g_get_scaleform_member{};
ScaleformSetPositionFn g_set_scaleform_position{};
ScaleformSetVisibleFn g_set_scaleform_visible{};
ScaleformDestructorFn g_destroy_scaleform_value{};

std::atomic_uint32_t g_transition_log_count{};
std::atomic_uint32_t g_serialized_selection_log_count{};
std::atomic_uint32_t g_failure_log_count{};
std::atomic_uint32_t g_record_mismatch_log_count{};
std::atomic_uint32_t g_wrap_diagnostic_log_count{};
std::atomic_uint32_t g_alias_log_count{};
std::atomic_uint32_t g_image_bind_log_count{};
std::atomic_uint32_t g_native_image_resource_log_count{};
std::atomic_uint32_t g_shape_attach_attempt_count{};
std::atomic_uint32_t g_state_isolation_log_count{};
std::atomic_uint32_t g_alias_visibility_log_count{};
std::atomic_uint32_t g_carrier_normalization_log_count{};
std::atomic_uint32_t g_carrier_selection_log_count{};
std::atomic_uint32_t g_pointer_sample_log_count{};
std::atomic_uint32_t g_pointer_dwell_log_count{};
std::atomic_uint32_t g_rgba8_lookup_log_count{};
std::atomic_uint32_t g_rgba8_bind_log_count{};
std::atomic_uint32_t g_rgba8_lifecycle_log_count{};
std::atomic_bool g_value_invoke_probe_attempted{};
std::atomic_bool g_value_invoke_probe_validated{};
std::atomic_bool g_bitmapdata_constructor_probe_attempted{};
std::atomic_bool g_bitmapdata_pixel_mutation_probe_attempted{};
std::atomic_bool g_raw_image_lifecycle_probe_attempted{};
std::atomic_bool g_rgba8_publication_disabled{};
// Readers may run from native presentation callbacks. Individual pointer
// stores are never sufficient evidence that the complete registered
// catalogue exists; this release/acquire flag is the single publication
// point after every image/resource/checksum identity has been installed.
std::atomic_bool g_rgba8_catalogue_ready{};
// Each immutable, movie-independent native image resource is intentionally
// retained until process exit. The DLL is pinned, and no game code is called
// from DllMain to release it under the loader lock.
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
std::array<std::atomic<void*>, kCustomTabCount> g_retained_rgba8_images{};
std::array<std::atomic<void*>, kCustomTabCount> g_retained_rgba8_resources{};
std::array<std::atomic_uint64_t, kCustomTabCount>
    g_retained_rgba8_checksums{};
// Readiness is explicit: every 4096-byte RGBA8 payload is valid, including a
// payload whose diagnostic FNV checksum happens to be zero.
std::array<std::atomic_bool, kCustomTabCount> g_retained_rgba8_ready{};
#endif
std::atomic_uint64_t g_presentation_epoch_counter{};
std::atomic_uint64_t g_rgba8_successful_attach_count{};
std::atomic_uint64_t g_rgba8_successful_detach_count{};
// The attachment ledger outlives RuntimeState. Resetting an invalidated or
// failed viewport must never erase ownership that still belongs to a live
// Scaleform movie; only an exact detach/absence proof clears a bit.
AtomicTabHostMask g_injected_icon_hosts{};
AtomicTabHostMask g_rgba8_icon_hosts{};
// Zero means no retained RGBA Shape; otherwise value-1 is the immutable asset
// index currently bound to that front/back physical host.
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
std::array<std::atomic_size_t, kTabHostCapacity>
    g_rgba8_host_assets{};
#endif
// A changed target is first attached in the host's inactive property bank.
// These ledgers describe that short, same-callback overlap.  They are never
// published as the active mapping until the old bank has been removed exactly.
AtomicTabHostMask g_staged_injected_icon_hosts{};
AtomicTabHostMask g_staged_rgba8_icon_hosts{};
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
std::array<std::atomic_size_t, kTabHostCapacity>
    g_staged_rgba8_host_assets{};
#endif
// One bit per front/back host; set means bank B is the active property bank.
AtomicTabHostMask g_icon_bank_b_hosts{};
// A native Shape can release its previous RawImage on a later renderer/trace
// boundary even though the in-place binder already returned successfully.
// Keep those old, no-longer-active references separate from the physical-host
// map. The debt is bounded by the total number of tab-strip hosts and may only
// reconcile downward after an observed RawImage refcount drop.
constexpr std::uint32_t kMaximumRgba8RetirementDebt =
    static_cast<std::uint32_t>(kTabHostCapacity);
constexpr Rgba8RetirementLimits kRgba8RetirementLimits{
    // Active and inactive property banks may coexist during one atomic
    // handoff, so the policy bounds both complete host sets.
    static_cast<std::uint32_t>(kTabHostCapacity * 2U),
    kMaximumRgba8RetirementDebt,
};
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
std::array<std::atomic_uint32_t, kCustomTabCount>
    g_rgba8_retirement_debt{};
#endif
InjectedMovieFingerprint g_injected_movie_fingerprint{};
// A managed GFx::Value is linked into its ObjectInterface while alive. Every
// wrapper used by this fault-isolation gate therefore lives in process-heap
// storage. A fully successful operation destroys and frees it; an uncertain
// native failure retains at most these three blocks for the process lifetime
// rather than leaving an intrusive-list link in dead stack storage.
void* g_retained_bitmapdata_wrapper{};
void* g_retained_bitmapdata_same_frame_wrapper{};
void* g_retained_bitmapdata_next_frame_wrapper{};
thread_local bool g_mutating_viewport{};
thread_local TabSelectionSource g_tab_selection_source{
    TabSelectionSource::unknown};
thread_local PointerCellSampleTrace g_pointer_cell_sample_trace{};
thread_local PointerDwellAdapter g_pointer_dwell{};
thread_local bool g_blocking_composite_input{};
thread_local PanelStateIsolationExpectation g_state_isolation_expectation{};
thread_local bool g_resolving_icon_alias{};
thread_local RepaintContext g_repaint_context{};
// True only while the desired cells of a new Configuration epoch are being
// presented. Rollback repainting deliberately runs outside this scope so a
// failed initial publication always restores the visible vector fallback.
thread_local bool g_initial_target_repaint{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
// Set only around synchronous target-layout construction/repaint. Native
// source validation outside that scope always resolves the applied generation.
thread_local const erui::detail::TabRuntimeGeneration*
    g_target_tab_projection_generation{};
#endif
// The alias lookup and image binding are adjacent calls in the validated
// presenter. Mark the exact resource, icon kind, and repaint cell, then
// consume the marker at the next image-bind call on the same UI thread.
thread_local PendingAliasedIcon g_pending_aliased_icon{};
thread_local InPlaceRgba8RebindTransaction
    g_in_place_rgba8_rebind_transaction{};
thread_local DeferredBitmapDataProbe g_deferred_bitmapdata_probe{};
thread_local BitmapDataPropertyVerification
    g_bitmapdata_property_verification{};
thread_local std::uint64_t g_composite_frame_sequence{};

constexpr std::size_t kNoRgba8Asset = invalid_tab_index;

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
[[nodiscard]] bool staged_rgba8_bank_publishable() noexcept;
[[nodiscard]] bool publish_staged_rgba8_bank_after_handoff() noexcept;
void finish_staged_rgba8_bank_publication() noexcept;
#endif

[[nodiscard]] std::size_t active_physical_tab_count() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (g_native_tab_runtime) {
        const std::size_t count =
            g_native_tab_runtime->active_physical_slot_count();
        if (count != 0U) return count;
    }
#endif
    // Before a live movie epoch has been validated, only the eight stock
    // logical records are safe to address. Research builds retain their
    // original fixed-eight contract.
    return kStockLogicalTabCount;
}

[[nodiscard]] std::size_t active_visible_custom_tab_count() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const erui::detail::TabRuntimeGeneration* const generation =
        g_target_tab_projection_generation
        ? g_target_tab_projection_generation
        : g_tab_projection_generation.get();
    return generation ? generation->visible_order.size() : 0U;
#else
    return active_custom_tab_count();
#endif
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
[[nodiscard]] const erui::detail::TabRuntimeGeneration*
effective_tab_projection_generation() noexcept {
    return g_target_tab_projection_generation
        ? g_target_tab_projection_generation
        : g_tab_projection_generation.get();
}

class ScopedTargetTabProjectionGeneration final {
public:
    explicit ScopedTargetTabProjectionGeneration(
        const erui::detail::TabRuntimeGeneration* generation) noexcept
        : previous_(g_target_tab_projection_generation) {
        g_target_tab_projection_generation = generation;
    }

    ~ScopedTargetTabProjectionGeneration() noexcept {
        g_target_tab_projection_generation = previous_;
    }

    ScopedTargetTabProjectionGeneration(
        const ScopedTargetTabProjectionGeneration&) = delete;
    ScopedTargetTabProjectionGeneration& operator=(
        const ScopedTargetTabProjectionGeneration&) = delete;

private:
    const erui::detail::TabRuntimeGeneration* previous_{};
};

[[nodiscard]] std::size_t stable_custom_index_for_logical(
    std::size_t logical) noexcept {
    if (!g_logical_tab_catalogue ||
        logical < kStockLogicalTabCount) {
        return invalid_tab_index;
    }
    const erui::detail::TabRuntimeGeneration* const generation =
        effective_tab_projection_generation();
    const std::size_t ordinal = logical - kStockLogicalTabCount;
    if (!generation || ordinal >= generation->visible_order.size()) {
        return invalid_tab_index;
    }
    const std::size_t stable = generation->visible_order[ordinal];
    return stable < active_custom_tab_count()
        ? stable
        : invalid_tab_index;
}

[[nodiscard]] std::size_t logical_for_stable_custom_index(
    std::size_t stable) noexcept {
    if (!g_logical_tab_catalogue ||
        stable >= active_custom_tab_count()) {
        return invalid_tab_index;
    }
    const erui::detail::TabRuntimeGeneration* const generation =
        effective_tab_projection_generation();
    if (!generation) return invalid_tab_index;
    const auto found = std::find(
        generation->visible_order.begin(),
        generation->visible_order.end(),
        stable);
    return found == generation->visible_order.end()
        ? invalid_tab_index
        : kStockLogicalTabCount + static_cast<std::size_t>(
              found - generation->visible_order.begin());
}

[[nodiscard]] const wchar_t* active_logical_title(
    const LogicalTab& tab) noexcept {
    return tab.alternate_title_active
        ? tab.alternate_title.data()
        : tab.title.data();
}

[[nodiscard]] std::array<wchar_t, kTitleCapacity>&
active_logical_title_storage(LogicalTab& tab) noexcept {
    return tab.alternate_title_active ? tab.alternate_title : tab.title;
}

[[nodiscard]] std::array<wchar_t, kTitleCapacity>&
inactive_logical_title(LogicalTab& tab) noexcept {
    return tab.alternate_title_active ? tab.title : tab.alternate_title;
}

[[nodiscard]] const wchar_t* inactive_logical_title(
    const LogicalTab& tab) noexcept {
    return tab.alternate_title_active
        ? tab.title.data()
        : tab.alternate_title.data();
}

[[nodiscard]] const wchar_t* mapping_logical_title(
    const LogicalTab& tab,
    std::size_t logical) noexcept {
    return g_target_tab_projection_generation &&
            logical >= kStockLogicalTabCount
        ? inactive_logical_title(tab)
        : active_logical_title(tab);
}
#endif

[[nodiscard]] LogicalTab* runtime_logical_tab(std::size_t logical) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_logical_tab_catalogue) return nullptr;
    if (logical < kStockLogicalTabCount) {
        return logical < g_logical_tab_catalogue->tabs.size()
            ? &g_logical_tab_catalogue->tabs[logical]
            : nullptr;
    }
    const std::size_t stable = stable_custom_index_for_logical(logical);
    const std::size_t storage = stable == invalid_tab_index
        ? invalid_tab_index
        : kStockLogicalTabCount + stable;
    return storage < g_logical_tab_catalogue->tabs.size()
        ? &g_logical_tab_catalogue->tabs[storage]
        : nullptr;
#else
    return logical < g_runtime.catalogue.size()
        ? &g_runtime.catalogue[logical]
        : nullptr;
#endif
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
[[nodiscard]] LogicalTab* runtime_stable_custom_logical_tab(
    std::size_t stable) noexcept {
    const std::size_t storage = kStockLogicalTabCount + stable;
    return g_logical_tab_catalogue &&
            stable < active_custom_tab_count() &&
            storage < g_logical_tab_catalogue->tabs.size()
        ? &g_logical_tab_catalogue->tabs[storage]
        : nullptr;
}

[[nodiscard]] erui::detail::NativeTabRuntimeEntry* runtime_custom_tab(
    std::size_t custom) noexcept {
    return g_native_tab_runtime ? g_native_tab_runtime->tab(custom) : nullptr;
}
#endif

[[nodiscard]] std::int32_t rgba8_icon_id(std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->icon().resource_id : -1;
#else
    return custom < kRgba8IconIds.size() ? kRgba8IconIds[custom] : -1;
#endif
}

[[nodiscard]] const wchar_t* rgba8_icon_resource_name(
    std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->icon().resource_name.data() : nullptr;
#else
    return custom < kRgba8IconResourceNames.size()
        ? kRgba8IconResourceNames[custom].data()
        : nullptr;
#endif
}

[[nodiscard]] const wchar_t* raw_image_resource_name(
    std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->icon().raw_resource_name.data() : nullptr;
#else
    return custom < kRawImageResourceProbeNames.size()
        ? kRawImageResourceProbeNames[custom].data()
        : nullptr;
#endif
}

[[nodiscard]] void* retained_rgba8_image(std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->icon().image.load(std::memory_order_acquire) : nullptr;
#else
    return custom < g_retained_rgba8_images.size()
        ? g_retained_rgba8_images[custom].load(std::memory_order_acquire)
        : nullptr;
#endif
}

[[nodiscard]] void* retained_rgba8_resource(std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->icon().resource.load(std::memory_order_acquire) : nullptr;
#else
    return custom < g_retained_rgba8_resources.size()
        ? g_retained_rgba8_resources[custom].load(std::memory_order_acquire)
        : nullptr;
#endif
}

[[nodiscard]] std::uint64_t retained_rgba8_checksum(
    std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->icon().checksum.load(std::memory_order_acquire) : 0U;
#else
    return custom < g_retained_rgba8_checksums.size()
        ? g_retained_rgba8_checksums[custom].load(std::memory_order_acquire)
        : 0U;
#endif
}

[[nodiscard]] bool retained_rgba8_ready(std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab && tab->icon().ready.load(std::memory_order_acquire);
#else
    return custom < g_retained_rgba8_ready.size() &&
        g_retained_rgba8_ready[custom].load(std::memory_order_acquire);
#endif
}

[[nodiscard]] RetainedRgba8Resource current_retained_rgba8(
    std::size_t custom) noexcept {
    return {
        .image = retained_rgba8_image(custom),
        .resource = retained_rgba8_resource(custom),
        .checksum = retained_rgba8_checksum(custom),
        .ready = retained_rgba8_ready(custom),
    };
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
[[nodiscard]] bool target_rgba8_bank_is_active() noexcept {
    return g_rgba8_staged_bank_ready &&
        g_rgba8_staged_tab_generation &&
        g_target_tab_projection_generation ==
            g_rgba8_staged_tab_generation.get() &&
        g_rgba8_staged_resources_scratch.size() ==
            active_custom_tab_count();
}

[[nodiscard]] RetainedRgba8Resource effective_retained_rgba8(
    std::size_t custom) noexcept {
    if (target_rgba8_bank_is_active() &&
        custom < g_rgba8_staged_resources_scratch.size()) {
        return g_rgba8_staged_resources_scratch[custom];
    }
    return current_retained_rgba8(custom);
}
#else
[[nodiscard]] RetainedRgba8Resource effective_retained_rgba8(
    std::size_t custom) noexcept {
    return current_retained_rgba8(custom);
}
#endif

void publish_retained_rgba8(
    std::size_t custom,
    const RetainedRgba8Resource& retained) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    auto* const tab = runtime_custom_tab(custom);
    if (!tab) return;
    tab->icon().image.store(retained.image, std::memory_order_relaxed);
    tab->icon().checksum.store(retained.checksum, std::memory_order_relaxed);
    tab->icon().resource.store(retained.resource, std::memory_order_relaxed);
    tab->icon().ready.store(retained.ready, std::memory_order_release);
#else
    if (custom >= kCustomTabCount) return;
    g_retained_rgba8_images[custom].store(
        retained.image, std::memory_order_relaxed);
    g_retained_rgba8_checksums[custom].store(
        retained.checksum, std::memory_order_relaxed);
    g_retained_rgba8_resources[custom].store(
        retained.resource, std::memory_order_relaxed);
    g_retained_rgba8_ready[custom].store(
        retained.ready, std::memory_order_release);
#endif
}

[[nodiscard]] std::uint32_t rgba8_retirement_debt(
    std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const tab = runtime_custom_tab(custom);
    return tab ? tab->retirement_debt() : 0U;
#else
    return custom < g_rgba8_retirement_debt.size()
        ? g_rgba8_retirement_debt[custom].load(std::memory_order_acquire)
        : 0U;
#endif
}

[[nodiscard]] bool set_rgba8_retirement_debt(
    std::size_t custom,
    std::uint32_t debt) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    return g_native_tab_runtime &&
        g_native_tab_runtime->set_retirement_debt(custom, debt);
#else
    if (custom >= g_rgba8_retirement_debt.size()) return false;
    g_rgba8_retirement_debt[custom].store(debt, std::memory_order_release);
    return true;
#endif
}

[[nodiscard]] std::size_t active_host_asset(std::size_t host) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const state = g_native_tab_runtime
        ? g_native_tab_runtime->host(host)
        : nullptr;
    return state
        ? state->active_tab_index.load(std::memory_order_acquire)
        : kNoRgba8Asset;
#else
    return host < g_rgba8_host_assets.size()
        ? g_rgba8_host_assets[host].load(std::memory_order_acquire)
        : kNoRgba8Asset;
#endif
}

[[nodiscard]] std::size_t staged_host_asset(std::size_t host) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const auto* const state = g_native_tab_runtime
        ? g_native_tab_runtime->host(host)
        : nullptr;
    return state
        ? state->staged_tab_index.load(std::memory_order_acquire)
        : kNoRgba8Asset;
#else
    return host < g_staged_rgba8_host_assets.size()
        ? g_staged_rgba8_host_assets[host].load(std::memory_order_acquire)
        : kNoRgba8Asset;
#endif
}

[[nodiscard]] bool set_active_host_asset(
    std::size_t host,
    std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_native_tab_runtime) return false;
    return custom == kNoRgba8Asset
        ? g_native_tab_runtime->clear_active_host(host)
        : g_native_tab_runtime->set_active_host(host, custom);
#else
    if (host >= g_rgba8_host_assets.size() ||
        (custom != kNoRgba8Asset && custom >= active_custom_tab_count())) {
        return false;
    }
    g_rgba8_host_assets[host].store(custom, std::memory_order_release);
    return true;
#endif
}

[[nodiscard]] bool set_staged_host_asset(
    std::size_t host,
    std::size_t custom) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_native_tab_runtime) return false;
    return custom == kNoRgba8Asset
        ? g_native_tab_runtime->clear_staged_host(host)
        : g_native_tab_runtime->set_staged_host(host, custom);
#else
    if (host >= g_staged_rgba8_host_assets.size() ||
        (custom != kNoRgba8Asset && custom >= active_custom_tab_count())) {
        return false;
    }
    g_staged_rgba8_host_assets[host].store(
        custom, std::memory_order_release);
    return true;
#endif
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
// Compatibility views keep the proven hook/lifecycle code readable while the
// actual ownership is consolidated in NativeTabRuntimeState. These objects do
// not contain per-tab storage: every access resolves tabs[index] and therefore
// remains exact-sized and stable for the process lifetime.
template <auto Member>
class RuntimeIconAtomicView final {
public:
    [[nodiscard]] std::size_t size() const noexcept {
        return active_custom_tab_count();
    }

    [[nodiscard]] decltype(auto) operator[](
        std::size_t custom) const noexcept {
        return (runtime_custom_tab(custom)->icon().*Member);
    }
};

class RuntimeRetirementDebtReference final {
public:
    explicit RuntimeRetirementDebtReference(std::size_t custom) noexcept
        : custom_(custom) {}

    [[nodiscard]] std::uint32_t load(
        std::memory_order = std::memory_order_seq_cst) const noexcept {
        return rgba8_retirement_debt(custom_);
    }

    void store(
        std::uint32_t debt,
        std::memory_order = std::memory_order_seq_cst) const noexcept {
        (void)set_rgba8_retirement_debt(custom_, debt);
    }

private:
    std::size_t custom_{};
};

class RuntimeRetirementDebtView final {
public:
    [[nodiscard]] std::size_t size() const noexcept {
        return active_custom_tab_count();
    }

    [[nodiscard]] RuntimeRetirementDebtReference operator[](
        std::size_t custom) const noexcept {
        return RuntimeRetirementDebtReference(custom);
    }
};

class EncodedHostAssetReference final {
public:
    EncodedHostAssetReference(
        std::size_t host,
        bool staged) noexcept
        : host_(host), staged_(staged) {}

    // Legacy transition code uses zero as empty and index+1 as occupied. The
    // pinned runtime itself stores the real size_t tab index; this boundary is
    // only an adapter and is intentionally not an 8-bit value.
    [[nodiscard]] std::size_t load(
        std::memory_order = std::memory_order_seq_cst) const noexcept {
        const std::size_t custom = staged_
            ? staged_host_asset(host_)
            : active_host_asset(host_);
        return custom == kNoRgba8Asset ? 0U : custom + 1U;
    }

    void store(
        std::size_t encoded,
        std::memory_order = std::memory_order_seq_cst) const noexcept {
        const std::size_t custom = encoded == 0U
            ? kNoRgba8Asset
            : encoded - 1U;
        if (staged_) {
            (void)set_staged_host_asset(host_, custom);
        } else {
            (void)set_active_host_asset(host_, custom);
        }
    }

private:
    std::size_t host_{};
    bool staged_{};
};

class EncodedHostAssetView final {
public:
    explicit EncodedHostAssetView(bool staged) noexcept : staged_(staged) {}

    [[nodiscard]] std::size_t size() const noexcept {
        return active_tab_host_count();
    }

    [[nodiscard]] EncodedHostAssetReference operator[](
        std::size_t host) const noexcept {
        return EncodedHostAssetReference(host, staged_);
    }

private:
    bool staged_{};
};

class RuntimeIconIdView final {
public:
    [[nodiscard]] std::size_t size() const noexcept {
        return active_custom_tab_count();
    }

    [[nodiscard]] std::int32_t operator[](
        std::size_t custom) const noexcept {
        return rgba8_icon_id(custom);
    }
};

class RuntimeIconResourceNameView final {
public:
    [[nodiscard]] std::size_t size() const noexcept {
        return active_custom_tab_count();
    }

    [[nodiscard]] const std::array<wchar_t, 32>& operator[](
        std::size_t custom) const noexcept {
        return runtime_custom_tab(custom)->icon().resource_name;
    }
};

class RuntimeRawResourceNameView final {
public:
    [[nodiscard]] std::size_t size() const noexcept {
        return active_custom_tab_count();
    }

    [[nodiscard]] const std::array<wchar_t, 48>& operator[](
        std::size_t custom) const noexcept {
        return runtime_custom_tab(custom)->icon().raw_resource_name;
    }
};

RuntimeIconAtomicView<&erui::detail::NativeTabIconRuntimeState::image>
    g_retained_rgba8_images{};
RuntimeIconAtomicView<&erui::detail::NativeTabIconRuntimeState::resource>
    g_retained_rgba8_resources{};
RuntimeIconAtomicView<&erui::detail::NativeTabIconRuntimeState::checksum>
    g_retained_rgba8_checksums{};
RuntimeIconAtomicView<&erui::detail::NativeTabIconRuntimeState::ready>
    g_retained_rgba8_ready{};
RuntimeRetirementDebtView g_rgba8_retirement_debt{};
EncodedHostAssetView g_rgba8_host_assets{false};
EncodedHostAssetView g_staged_rgba8_host_assets{true};
RuntimeIconIdView kRgba8IconIds{};
RuntimeIconResourceNameView kRgba8IconResourceNames{};
RuntimeRawResourceNameView kRawImageResourceProbeNames{};
#endif

[[nodiscard]] bool all_rgba8_retirement_debts_zero() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    return (!g_native_tab_runtime ||
        g_native_tab_runtime->retiring_tab_indices().empty()) &&
        !g_rgba8_retired_bank_active;
#else
    for (std::size_t custom = 0;
         custom < active_custom_tab_count();
         ++custom) {
        if (rgba8_retirement_debt(custom) != 0U) return false;
    }
    return true;
#endif
}

void clear_all_rgba8_retirement_debts() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    while (g_native_tab_runtime &&
           !g_native_tab_runtime->retiring_tab_indices().empty()) {
        const std::size_t custom =
            g_native_tab_runtime->retiring_tab_indices().back();
        if (!set_rgba8_retirement_debt(custom, 0U)) break;
    }
#else
    for (std::size_t custom = 0;
         custom < active_custom_tab_count();
         ++custom) {
        (void)set_rgba8_retirement_debt(custom, 0U);
    }
#endif
}

// Requested raster ownership and effective raster publication are distinct.
// A provider may request RGBA8, yet the all-or-nothing native resource
// catalogue can fail safely and leave that tab on the vector fallback.  Only
// successfully published resources may enter a target host layout.  The
// requested predicate remains the authority for auditing resources and host
// ledgers that were already published before a later fail-closed condition.
[[nodiscard]] bool catalogue_icon_has_retained_raster(
    std::size_t icon) noexcept {
    if (!catalogue_icon_requires_raster(icon) ||
        !g_rgba8_catalogue_ready.load(std::memory_order_acquire)) {
        return false;
    }
    const RetainedRgba8Resource retained =
        effective_retained_rgba8(icon);
    return retained.ready && retained.resource && retained.image;
}

[[nodiscard]] bool catalogue_icon_has_effective_raster(
    std::size_t icon) noexcept {
    return !g_rgba8_publication_disabled.load(
               std::memory_order_acquire) &&
        catalogue_icon_has_retained_raster(icon);
}

std::size_t repaint_host_bit() noexcept {
    if (!g_repaint_context.active ||
        g_repaint_context.layer == TabLayer::none ||
        g_repaint_context.physical >= active_physical_tab_count()) {
        return invalid_tab_index;
    }
    return g_repaint_context.physical +
        (g_repaint_context.layer == TabLayer::back
            ? active_physical_tab_count()
            : 0);
}

std::uint8_t active_icon_property_bank(std::size_t bit) noexcept {
    if (bit >= active_tab_host_count()) return kIconPropertyBankA;
    const TabHostMask mask = TabHostMask{1} << bit;
    return (g_icon_bank_b_hosts.load(std::memory_order_acquire) & mask) != 0
        ? kIconPropertyBankB
        : kIconPropertyBankA;
}

bool repaint_stages_fresh_host(std::size_t bit) noexcept {
    if (bit >= active_tab_host_count()) return false;
    const auto& transaction = g_in_place_rgba8_rebind_transaction;
    const TabHostMask mask = TabHostMask{1} << bit;
    return transaction.active && !transaction.failed &&
        transaction.stage == InPlaceRgba8RebindStage::repainting &&
        (transaction.expected_host_mask & mask) != 0;
}

std::uint8_t repaint_icon_property_bank() noexcept {
    const std::size_t bit = repaint_host_bit();
    const std::uint8_t active = active_icon_property_bank(bit);
    return repaint_stages_fresh_host(bit)
        ? static_cast<std::uint8_t>(active ^ 1U)
        : active;
}

void reset_pointer_dwell(const char* reason) noexcept;
void run_deferred_bitmapdata_constructor_probe(
    void* top,
    std::uint64_t frame_sequence) noexcept;
void run_bitmapdata_property_survival_verification(
    void* top,
    std::uint64_t frame_sequence) noexcept;
void run_raw_image_lifecycle_probe(void* top) noexcept;
bool validate_top(void* top) noexcept;
bool guarded_add_ref_scaleform_image_resource(
    ScaleformImageResourceAddRefFn add_ref,
    void* resource,
    unsigned long& seh_code) noexcept;
bool guarded_release_scaleform_image_resource(
    ScaleformImageResourceReleaseFn release,
    void* resource,
    unsigned long& seh_code) noexcept;

std::uint64_t query_performance_ticks() noexcept {
    LARGE_INTEGER ticks{};
    if (!QueryPerformanceCounter(&ticks) || ticks.QuadPart <= 0) return 0;
    return static_cast<std::uint64_t>(ticks.QuadPart);
}

std::uint64_t query_performance_frequency() noexcept {
    static const std::uint64_t frequency = []() noexcept {
        LARGE_INTEGER value{};
        if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0) {
            return std::uint64_t{};
        }
        return static_cast<std::uint64_t>(value.QuadPart);
    }();
    return frequency;
}

std::uint64_t elapsed_qpc_microseconds(
    std::uint64_t begin,
    std::uint64_t end) noexcept {
    const std::uint64_t frequency = query_performance_frequency();
    if (begin == 0 || end < begin || frequency == 0) return 0;
    const std::uint64_t delta = end - begin;
    constexpr std::uint64_t microseconds_per_second = 1'000'000;
    return (delta / frequency) * microseconds_per_second +
        ((delta % frequency) * microseconds_per_second) / frequency;
}

bool guarded_copy(
    void* destination,
    const void* source,
    std::size_t size) noexcept {
    if (!destination || !source || size == 0) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(destination, source, size);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

template <typename T>
bool guarded_read(const void* source, T& value) noexcept {
    return guarded_copy(&value, source, sizeof(value));
}

void log_line(const char* format, ...) noexcept;

struct GamePointerLocation {
    bool in_image{};
    std::uintptr_t rva{};
};

GamePointerLocation locate_game_pointer(const void* pointer) noexcept {
    GamePointerLocation result{};
    if (!pointer || !g_game.base || g_game.size == 0) return result;

    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(g_game.base);
    if (address < base) return result;
    const std::uintptr_t delta = address - base;
    if (delta >= g_game.size) return result;
    result.in_image = true;
    result.rva = delta;
    return result;
}

// Snapshot only scalar fields from the known-good resource that Elden Ring is
// about to bind. This deliberately performs no virtual call, AddRef, Release,
// mutation, or cross-frame retention. The original binder consumes the lookup
// reference immediately after this function returns.
void log_native_image_resource_snapshot(
    void* resource,
    void* image) noexcept {
    const std::uint32_t event =
        g_native_image_resource_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event > kMaximumNativeImageResourceLogs || !resource || !image) {
        return;
    }

    const auto* const resource_bytes =
        static_cast<const std::byte*>(resource);
    void* resource_vtable{};
    std::uint32_t resource_refcount{};
    void* delegate_vtable{};
    std::uint32_t delegate_refcount{};
    void* delegate_underlying_image{};
    std::uint32_t resource_kind{};
    const bool resource_vtable_read = guarded_read(
        resource_bytes + 0x00, resource_vtable);
    const bool resource_refcount_read = guarded_read(
        resource_bytes + 0x08, resource_refcount);
    const bool delegate_vtable_read = guarded_read(
        resource_bytes + 0x20, delegate_vtable);
    const bool delegate_refcount_read = guarded_read(
        resource_bytes + 0x28, delegate_refcount);
    const bool delegate_underlying_read = guarded_read(
        resource_bytes + 0x48, delegate_underlying_image);
    const bool resource_kind_read = guarded_read(
        resource_bytes + 0x60, resource_kind);

    const auto* const image_bytes = static_cast<const std::byte*>(image);
    void* image_vtable{};
    std::uint32_t image_refcount{};
    const bool image_vtable_read = guarded_read(
        image_bytes + 0x00, image_vtable);
    const bool image_refcount_read = guarded_read(
        image_bytes + 0x08, image_refcount);

    void* image_type_slot{};
    void* image_format_slot{};
    void* image_size_slot{};
    bool image_slots_read{};
    const GamePointerLocation image_vtable_location =
        locate_game_pointer(image_vtable);
    if (image_vtable_read && image_vtable_location.in_image) {
        const auto* const vtable_bytes =
            static_cast<const std::byte*>(image_vtable);
        image_slots_read =
            guarded_read(vtable_bytes + 0x18, image_type_slot) &&
            guarded_read(vtable_bytes + 0x20, image_format_slot) &&
            guarded_read(vtable_bytes + 0x28, image_size_slot);
    }

    std::uint32_t field_28{};
    std::uint32_t field_2c{};
    std::uint32_t field_30{};
    std::uint32_t field_34{};
    std::uint8_t field_70{};
    std::uint32_t field_74{};
    std::uint32_t field_78{};
    std::uint32_t field_7c{};
    std::uint32_t field_80{};
    std::uint32_t field_84{};
    std::uint32_t field_88{};
    const bool image_fields_read =
        guarded_read(image_bytes + 0x28, field_28) &&
        guarded_read(image_bytes + 0x2C, field_2c) &&
        guarded_read(image_bytes + 0x30, field_30) &&
        guarded_read(image_bytes + 0x34, field_34) &&
        guarded_read(image_bytes + 0x70, field_70) &&
        guarded_read(image_bytes + 0x74, field_74) &&
        guarded_read(image_bytes + 0x78, field_78) &&
        guarded_read(image_bytes + 0x7C, field_7c) &&
        guarded_read(image_bytes + 0x80, field_80) &&
        guarded_read(image_bytes + 0x84, field_84) &&
        guarded_read(image_bytes + 0x88, field_88);

    const GamePointerLocation resource_vtable_location =
        locate_game_pointer(resource_vtable);
    const GamePointerLocation delegate_vtable_location =
        locate_game_pointer(delegate_vtable);
    const GamePointerLocation image_type_location =
        locate_game_pointer(image_type_slot);
    const GamePointerLocation image_format_location =
        locate_game_pointer(image_format_slot);
    const GamePointerLocation image_size_location =
        locate_game_pointer(image_size_slot);
    const void* const inline_delegate = resource_bytes + 0x20;

    log_line(
        "nativeImageResource[%u] passiveOnly=1 resource=%p reads={vtable:%u ref:%u delegateVtable:%u delegateRef:%u underlying:%u kind:%u} vtable=%p{game:%u rva:0x%llX} ref=%u exposedImage=%p inlineDelegate=%p exposedIsInline=%u delegate={vtable:%p game:%u rva:0x%llX ref:%u underlying:%p} kind=0x%08X",
        static_cast<unsigned>(event),
        resource,
        resource_vtable_read ? 1U : 0U,
        resource_refcount_read ? 1U : 0U,
        delegate_vtable_read ? 1U : 0U,
        delegate_refcount_read ? 1U : 0U,
        delegate_underlying_read ? 1U : 0U,
        resource_kind_read ? 1U : 0U,
        resource_vtable,
        resource_vtable_location.in_image ? 1U : 0U,
        static_cast<unsigned long long>(resource_vtable_location.rva),
        static_cast<unsigned>(resource_refcount),
        image,
        inline_delegate,
        image == inline_delegate ? 1U : 0U,
        delegate_vtable,
        delegate_vtable_location.in_image ? 1U : 0U,
        static_cast<unsigned long long>(delegate_vtable_location.rva),
        static_cast<unsigned>(delegate_refcount),
        delegate_underlying_image,
        static_cast<unsigned>(resource_kind));
    log_line(
        "nativeImageResource[%u] image=%p reads={vtable:%u ref:%u slots:%u fields:%u} vtable=%p{game:%u rva:0x%llX} ref=%u slots={type:%p/0x%llX format:%p/0x%llX size:%p/0x%llX} raw={28:0x%08X 2C:%u 30:%u 34:0x%08X 70:%u 74:0x%08X 78:0x%08X 7C:0x%08X 80:0x%08X 84:%u 88:%u}",
        static_cast<unsigned>(event),
        image,
        image_vtable_read ? 1U : 0U,
        image_refcount_read ? 1U : 0U,
        image_slots_read ? 1U : 0U,
        image_fields_read ? 1U : 0U,
        image_vtable,
        image_vtable_location.in_image ? 1U : 0U,
        static_cast<unsigned long long>(image_vtable_location.rva),
        static_cast<unsigned>(image_refcount),
        image_type_slot,
        static_cast<unsigned long long>(image_type_location.rva),
        image_format_slot,
        static_cast<unsigned long long>(image_format_location.rva),
        image_size_slot,
        static_cast<unsigned long long>(image_size_location.rva),
        static_cast<unsigned>(field_28),
        static_cast<unsigned>(field_2c),
        static_cast<unsigned>(field_30),
        static_cast<unsigned>(field_34),
        static_cast<unsigned>(field_70),
        static_cast<unsigned>(field_74),
        static_cast<unsigned>(field_78),
        static_cast<unsigned>(field_7c),
        static_cast<unsigned>(field_80),
        static_cast<unsigned>(field_84),
        static_cast<unsigned>(field_88));
    if (event == kMaximumNativeImageResourceLogs) {
        log_line(
            "Further passive native-image resource snapshots are suppressed");
    }
}

bool retain_stress_log_format(const char* format) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!format) return false;
    if (erui::detail::trace_logging()) return true;
    // Routine per-cell and per-frame proof records are available through the
    // existing trace-level diagnostic switch. Always retain safety failures
    // without putting a one-tab production install into the old 50-tab probe
    // logging mode.
    return std::strncmp(format, "ERROR:", 6) == 0 ||
        std::strncmp(format, "WARNING:", 8) == 0;
#else
    if (!kStressQuietSuccessLogging || !format) return format != nullptr;
    const auto starts_with = [format](const char* prefix) noexcept {
        return std::strncmp(format, prefix, std::strlen(prefix)) == 0;
    };
    return format[0] == '=' || starts_with("ERROR:") ||
        starts_with("WARNING:") || starts_with("STRESS ") ||
        starts_with("pointerDwell[") ||
        starts_with("pointerPoll[") ||
        starts_with("selection[") ||
        starts_with("Configuration-tab viewport mutation probe starting") ||
        starts_with("Game image base=") ||
        starts_with("Viewport probe hooks installed:") ||
        starts_with("Further viewport-probe warnings are suppressed");
#endif
}

void log_line(const char* format, ...) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!format || !retain_stress_log_format(format)) return;
    std::array<char, 4096> message{};
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf_s(
        message.data(), message.size(), _TRUNCATE, format, arguments);
    va_end(arguments);
    if (result < 0 && message[0] == '\0') return;
    erui::LogLevel level = erui::LogLevel::trace;
    if (std::strncmp(message.data(), "ERROR:", 6) == 0) {
        level = erui::LogLevel::error;
    } else if (std::strncmp(message.data(), "WARNING:", 8) == 0) {
        level = erui::LogLevel::warning;
    }
    erui::detail::logf(level, "NativeTabs: %s", message.data());
#else
    if (g_log_file == INVALID_HANDLE_VALUE || !format) return;
    if (!retain_stress_log_format(format)) return;

    std::array<char, 4096> message{};
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf_s(
        message.data(), message.size(), _TRUNCATE, format, arguments);
    va_end(arguments);
    if (result < 0 && message[0] == '\0') return;

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::array<char, 4608> line{};
    const int line_length = snprintf(
        line.data(),
        line.size(),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [TabViewportProbe] %s\r\n",
        static_cast<unsigned>(time.wYear),
        static_cast<unsigned>(time.wMonth),
        static_cast<unsigned>(time.wDay),
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        static_cast<unsigned long>(GetCurrentThreadId()),
        message.data());
    if (line_length <= 0) return;

    const DWORD bytes_to_write = static_cast<DWORD>((std::min)(
        static_cast<std::size_t>(line_length), line.size() - 1));
    AcquireSRWLockExclusive(&g_log_lock);
    DWORD bytes_written{};
    (void)WriteFile(
        g_log_file, line.data(), bytes_to_write, &bytes_written, nullptr);
    ReleaseSRWLockExclusive(&g_log_lock);
#endif
}

void log_failure(const char* format, ...) noexcept {
    const std::uint32_t failure =
        g_failure_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (failure > kMaximumFailureLogs) return;

    std::array<char, 2048> message{};
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf_s(
        message.data(), message.size(), _TRUNCATE, format, arguments);
    va_end(arguments);
    if (result >= 0 || message[0] != '\0') {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        log_line("ERROR: %s", message.data());
#else
        log_line("WARNING: %s", message.data());
#endif
    }
    if (failure == kMaximumFailureLogs) {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        log_line("ERROR: Further native-tab failures are suppressed");
#else
        log_line("Further viewport-probe warnings are suppressed");
#endif
    }
}

template <std::size_t Size>
bool exact_resource_name(
    const wchar_t* candidate,
    const wchar_t (&expected)[Size]) noexcept {
    if (!candidate) return false;
#if defined(_MSC_VER)
    __try {
#endif
        // Size includes the terminator, so a matching prefix with trailing
        // characters is intentionally rejected.
        for (std::size_t index = 0; index < Size; ++index) {
            if (candidate[index] != expected[index]) return false;
        }
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool exact_resource_name(
    const wchar_t* candidate,
    const wchar_t* expected) noexcept {
    if (!candidate || !expected) return false;
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0;; ++index) {
            if (candidate[index] != expected[index]) return false;
            if (expected[index] == L'\0') return true;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool parse_rgba8_icon_resource_name(
    const wchar_t* candidate,
    std::size_t& icon_index) noexcept {
    icon_index = invalid_tab_index;
    if (!candidate) return false;
    constexpr wchar_t kPrefix[] = L"MENU_ItemIcon_";
    constexpr std::size_t kPrefixLength = std::size(kPrefix) - 1U;
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0; index < kPrefixLength; ++index) {
            if (candidate[index] != kPrefix[index]) return false;
        }
        std::uint32_t id{};
        for (std::size_t digit = 0; digit < 5U; ++digit) {
            const wchar_t value = candidate[kPrefixLength + digit];
            if (value < L'0' || value > L'9') return false;
            id = id * 10U + static_cast<std::uint32_t>(value - L'0');
        }
        if (candidate[kPrefixLength + 5U] != L'\0' ||
            id < static_cast<std::uint32_t>(kFirstRgba8IconId) ||
            id >= static_cast<std::uint32_t>(
                kFirstRgba8IconId + active_custom_tab_count())) {
            return false;
        }
        icon_index = static_cast<std::size_t>(
            id - static_cast<std::uint32_t>(kFirstRgba8IconId));
        return exact_resource_name(
            candidate, kRgba8IconResourceNames[icon_index].data());
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        icon_index = invalid_tab_index;
        return false;
    }
#endif
}

struct DisplayValueView {
    std::uint32_t flags{};
    void* object_interface{};
    void* object_interface_vtable{};
    void* get_member_slot{};
    void* set_member_slot{};
    void* invoke_slot{};
    void* object_payload{};
    void* native_display_object{};
    void* native_display_vtable{};
};

struct ObjectValueView {
    std::uint32_t flags{};
    void* object_interface{};
    void* object_interface_vtable{};
    void* get_member_slot{};
    void* set_member_slot{};
    void* invoke_slot{};
    void* object_payload{};
};

template <typename Value>
RawGfxValue make_primitive_value(
    std::uint32_t type,
    const Value& value) noexcept {
    static_assert(sizeof(Value) <= 8);
    RawGfxValue result{};
    result.type = type;
    std::memcpy(result.payload.data(), &value, sizeof(value));
    return result;
}

RawGfxValue make_bool_value(bool value) noexcept {
    const std::uint8_t encoded = value ? 1U : 0U;
    return make_primitive_value(2U, encoded);
}

RawGfxValue make_int_value(std::int32_t value) noexcept {
    return make_primitive_value(3U, value);
}

RawGfxValue make_uint_value(std::uint32_t value) noexcept {
    return make_primitive_value(4U, value);
}

RawGfxValue make_number_value(double value) noexcept {
    return make_primitive_value(5U, value);
}

bool inspect_object_value(
    const void* scaleform_wrapper,
    ObjectValueView& view) noexcept {
    view = {};
    if (!scaleform_wrapper ||
        !guarded_read(
            static_cast<const std::byte*>(scaleform_wrapper) + 0x18,
            view.object_interface) ||
        !view.object_interface ||
        !guarded_read(
            view.object_interface,
            view.object_interface_vtable) ||
        !view.object_interface_vtable ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface_vtable) + 0x20,
            view.get_member_slot) ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface_vtable) + 0x28,
            view.set_member_slot) ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface_vtable) + 0x30,
            view.invoke_slot) ||
        !guarded_read(
            static_cast<const std::byte*>(scaleform_wrapper) + 0x20,
            view.flags) ||
        !guarded_read(
            static_cast<const std::byte*>(scaleform_wrapper) + 0x28,
            view.object_payload) ||
        !view.object_payload) {
        return false;
    }
    return (view.flags & 0x8FU) == 0x08U;
}

bool exact_as3_object_bridge(const ObjectValueView& view) noexcept {
    return view.object_interface_vtable ==
            g_game.base + kAs3ValueInterfaceVtable271Rva &&
        view.get_member_slot == g_game.base + kAs3ValueGetMember271Rva &&
        view.set_member_slot == g_game.base + kAs3ValueSetMember271Rva &&
        view.invoke_slot == g_game.base + kAs3ValueInvoke271Rva;
}

bool invoke_object_method(
    const ObjectValueView& object,
    const char* method,
    const RawGfxValue* arguments,
    std::size_t argument_count,
    unsigned long& seh_code) noexcept {
    if (!method || !exact_as3_object_bridge(object)) return false;

    bool invoked{};
#if defined(_MSC_VER)
    __try {
#endif
        invoked = reinterpret_cast<ValueInvokeFn>(object.invoke_slot)(
            object.object_interface,
            object.object_payload,
            nullptr,
            method,
            arguments,
            argument_count,
            false);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (seh_code == 0) {
            seh_code = static_cast<unsigned long>(GetExceptionCode());
        }
    }
#endif
    return invoked;
}

bool draw_graphics_rectangle(
    const ObjectValueView& graphics,
    std::uint32_t color,
    double alpha,
    double x,
    double y,
    double width,
    double height,
    unsigned long& seh_code) noexcept {
    const std::array<RawGfxValue, 2> fill_arguments{
        make_uint_value(color),
        make_number_value(alpha),
    };
    const bool fill_begun = invoke_object_method(
        graphics,
        "beginFill",
        fill_arguments.data(),
        fill_arguments.size(),
        seh_code);
    if (!fill_begun) return false;

    const std::array<RawGfxValue, 4> rectangle_arguments{
        make_number_value(x),
        make_number_value(y),
        make_number_value(width),
        make_number_value(height),
    };
    const bool rectangle_drawn = invoke_object_method(
        graphics,
        "drawRect",
        rectangle_arguments.data(),
        rectangle_arguments.size(),
        seh_code);

    // Balance every successful beginFill even when drawRect returns false or
    // faults. This keeps the fresh Shape's Graphics state well formed.
    const bool fill_ended = invoke_object_method(
        graphics, "endFill", nullptr, 0, seh_code);
    return rectangle_drawn && fill_ended;
}

bool inspect_display_value(
    const void* scaleform_wrapper,
    DisplayValueView& view) noexcept {
    view = {};
    if (!scaleform_wrapper ||
        !guarded_read(
            static_cast<const std::byte*>(scaleform_wrapper) + 0x18,
            view.object_interface) ||
        !view.object_interface ||
        !guarded_read(
            view.object_interface,
            view.object_interface_vtable) ||
        !view.object_interface_vtable ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface_vtable) + 0x20,
            view.get_member_slot) ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface_vtable) + 0x28,
            view.set_member_slot) ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface_vtable) + 0x30,
            view.invoke_slot) ||
        !guarded_read(
            static_cast<const std::byte*>(scaleform_wrapper) + 0x20,
            view.flags) ||
        !guarded_read(
            static_cast<const std::byte*>(scaleform_wrapper) + 0x28,
            view.object_payload) ||
        !view.object_payload ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_payload) + 0x88,
            view.native_display_object) ||
        !view.native_display_object ||
        !guarded_read(
            view.native_display_object,
            view.native_display_vtable)) {
        return false;
    }
    return (view.flags & 0x8FU) == 0x0AU;
}

bool exact_as3_display_bridge(const DisplayValueView& view) noexcept {
    return view.object_interface_vtable ==
            g_game.base + kAs3ValueInterfaceVtable271Rva &&
        view.get_member_slot == g_game.base + kAs3ValueGetMember271Rva &&
        view.set_member_slot == g_game.base + kAs3ValueSetMember271Rva &&
        view.invoke_slot == g_game.base + kAs3ValueInvoke271Rva;
}

bool validated_authored_icon_target_vtable(void* vtable) noexcept {
    return g_native_tabs_build_profile && g_game.base &&
        (vtable == g_game.base + kStaticShapeVtable271Rva ||
            vtable ==
                g_game.base + kAlternateAuthoredIconVtable271Rva);
}

bool destroy_local_scaleform_value(
    std::array<std::byte, 0x38>& wrapper,
    bool& constructed,
    const char* label) noexcept {
    if (!constructed) return true;
    if (!g_destroy_scaleform_value) return false;
#if defined(_MSC_VER)
    __try {
#endif
        g_destroy_scaleform_value(wrapper.data());
        constructed = false;
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_failure(
            "%s cleanup faulted (seh=0x%08lX)",
            label ? label : "Scaleform value",
            static_cast<unsigned long>(GetExceptionCode()));
        return false;
    }
#endif
}

std::uint32_t count_tracked_icon_hosts(TabHostMask bits) noexcept {
    std::uint32_t count{};
    while (bits != 0) {
        count += bits & 1U;
        bits >>= 1U;
    }
    return count;
}

std::uint32_t count_active_rgba8_asset_hosts(
    std::size_t asset_index,
    std::size_t override_host = invalid_tab_index,
    std::size_t override_asset = 0) noexcept {
    if (asset_index >= active_custom_tab_count()) return 0;
    const std::size_t encoded_asset = asset_index + 1U;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    void* const current_resource = retained_rgba8_resource(asset_index);
#endif
    std::uint32_t count{};
    for (std::size_t host = 0; host < g_rgba8_host_assets.size(); ++host) {
        const std::size_t encoded = host == override_host
            ? override_asset
            : g_rgba8_host_assets[host].load(std::memory_order_acquire);
        if (encoded == encoded_asset) ++count;
        const std::size_t staged =
            g_staged_rgba8_host_assets[host].load(
                std::memory_order_acquire);
        if (staged == encoded_asset) {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            // A live immutable-icon handoff can temporarily expose the old
            // and target resources under the same stable tab index.  Only a
            // staged host that retained the exact current resource belongs in
            // the current RawImage reference count; a changed target resource
            // is audited independently during the overlap.
            void* staged_resource = current_resource;
            if (g_rgba8_staged_bank_ready &&
                asset_index < g_rgba8_staged_resources_scratch.size()) {
                staged_resource =
                    g_rgba8_staged_resources_scratch[asset_index].resource;
            }
            if (staged_resource == current_resource) ++count;
#else
            ++count;
#endif
        }
    }
    return count;
}

struct Rgba8ReferenceSnapshot {
    bool resource_read{};
    std::uint32_t resource{};
    bool raw_read{};
    std::uint32_t raw{};
};

Rgba8ReferenceSnapshot snapshot_rgba8_references(
    std::size_t asset_index) noexcept {
    Rgba8ReferenceSnapshot snapshot{};
    if (asset_index >= active_custom_tab_count()) return snapshot;
    void* const resource = g_retained_rgba8_resources[asset_index].load(
        std::memory_order_acquire);
    void* const image = g_retained_rgba8_images[asset_index].load(
        std::memory_order_acquire);
    snapshot.resource_read = resource && guarded_read(
        static_cast<const std::byte*>(resource) + 0x08,
        snapshot.resource);
    snapshot.raw_read = image && guarded_read(
        static_cast<const std::byte*>(image) + 0x08,
        snapshot.raw);
    return snapshot;
}

Rgba8ReferenceSnapshot snapshot_rgba8_references(
    const RetainedRgba8Resource& retained) noexcept {
    Rgba8ReferenceSnapshot snapshot{};
    snapshot.resource_read = retained.resource && guarded_read(
        static_cast<const std::byte*>(retained.resource) + 0x08,
        snapshot.resource);
    snapshot.raw_read = retained.image && guarded_read(
        static_cast<const std::byte*>(retained.image) + 0x08,
        snapshot.raw);
    return snapshot;
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
[[nodiscard]] std::uint32_t count_rgba8_resource_hosts(
    void* resource) noexcept {
    if (!resource) return 0U;
    std::uint32_t count{};
    for (std::size_t host = 0; host < active_tab_host_count(); ++host) {
        const std::size_t active = g_rgba8_host_assets[host].load(
            std::memory_order_acquire);
        if (active != 0U &&
            retained_rgba8_resource(active - 1U) == resource) {
            ++count;
        }
        const std::size_t staged = g_staged_rgba8_host_assets[host].load(
            std::memory_order_acquire);
        if (staged != 0U) {
            void* staged_resource = retained_rgba8_resource(staged - 1U);
            if (g_rgba8_staged_bank_ready &&
                staged - 1U < g_rgba8_staged_resources_scratch.size()) {
                staged_resource =
                    g_rgba8_staged_resources_scratch[staged - 1U].resource;
            }
            if (staged_resource == resource) ++count;
        }
    }
    return count;
}

[[nodiscard]] std::uint32_t rgba8_debt_for_resource(
    std::size_t asset_index,
    void* resource) noexcept {
    return asset_index < active_custom_tab_count() &&
            retained_rgba8_resource(asset_index) == resource
        ? rgba8_retirement_debt(asset_index)
        : 0U;
}
#else
[[nodiscard]] std::uint32_t count_rgba8_resource_hosts(
    void* resource) noexcept {
    if (!resource) return 0U;
    std::uint32_t count{};
    for (std::size_t asset = 0; asset < active_custom_tab_count(); ++asset) {
        if (retained_rgba8_resource(asset) == resource) {
            count += count_active_rgba8_asset_hosts(asset);
        }
    }
    return count;
}

[[nodiscard]] std::uint32_t rgba8_debt_for_resource(
    std::size_t asset_index,
    void* resource) noexcept {
    return asset_index < active_custom_tab_count() &&
            retained_rgba8_resource(asset_index) == resource
        ? rgba8_retirement_debt(asset_index)
        : 0U;
}
#endif

[[nodiscard]] bool reconcile_retired_rgba8_bank(
    const char* phase) noexcept {
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
    (void)phase;
    return true;
#else
    if (!g_rgba8_retired_bank_active) return true;
    if (g_rgba8_retired_resources_scratch.size() !=
            active_custom_tab_count() ||
        g_rgba8_retired_debts_scratch.size() !=
            active_custom_tab_count()) {
        return false;
    }
    bool exact{true};
    bool any_pending{};
    for (std::size_t index = 0;
         index < g_rgba8_retired_resources_scratch.size();
         ++index) {
        RetainedRgba8Resource& retired =
            g_rgba8_retired_resources_scratch[index];
        std::uint32_t& debt = g_rgba8_retired_debts_scratch[index];
        if (!retired.ready) continue;
        const Rgba8ReferenceSnapshot refs =
            snapshot_rgba8_references(retired);
        const auto retirement = plan_rgba8_retirement_reconciliation(
            0U, debt, refs.raw, kRgba8RetirementLimits);
        const bool entry_exact = refs.resource_read && refs.raw_read &&
            refs.resource == 1U && retirement.accepted();
        if (!entry_exact) {
            exact = false;
            any_pending = true;
            log_failure(
                "retired RGBA8 bank reconciliation phase=%s asset=%zu refs={resource:%u raw:%u} debt=%u policy=%u",
                phase ? phase : "unknown",
                index,
                static_cast<unsigned>(refs.resource),
                static_cast<unsigned>(refs.raw),
                static_cast<unsigned>(debt),
                static_cast<unsigned>(retirement.status));
            continue;
        }
        debt = retirement.retirement_debt_after;
        if (debt == 0U && refs.raw == 1U) {
            retired = {};
        } else {
            any_pending = true;
        }
    }
    if (exact && !any_pending) {
        g_rgba8_retired_bank_active = false;
        log_line(
            "retired RGBA8 resource bank reached exact root baseline phase=%s; next live icon generation may proceed",
            phase ? phase : "unknown");
    }
    return exact;
#endif
}

bool reconcile_rgba8_retirement_debt(
    const char* phase) noexcept {
    if (!g_rgba8_catalogue_ready.load(std::memory_order_acquire)) {
        return true;
    }

    const auto reconcile_asset = [phase](
        std::size_t asset,
        bool& removed_from_worklist) noexcept {
        removed_from_worklist = false;
        const std::uint32_t active =
            count_active_rgba8_asset_hosts(asset);
        const std::uint32_t prior_debt = rgba8_retirement_debt(asset);
        if (!catalogue_icon_requires_raster(asset)) {
            if (active != 0 || prior_debt != 0) {
                log_failure(
                    "RGBA8 retirement reconciliation phase=%s found ownership for non-raster asset=%zu active=%u debt=%u",
                    phase ? phase : "unknown",
                    asset,
                    static_cast<unsigned>(active),
                    static_cast<unsigned>(prior_debt));
                return false;
            }
            return true;
        }

        const Rgba8ReferenceSnapshot refs =
            snapshot_rgba8_references(asset);
        const auto retirement = plan_rgba8_retirement_reconciliation(
            active,
            prior_debt,
            refs.raw,
            kRgba8RetirementLimits);
        if (!refs.resource_read || !refs.raw_read || refs.resource != 1U ||
            !retirement.accepted()) {
            log_failure(
                "RGBA8 retirement reconciliation phase=%s asset=%zu refs={resourceRead:%u resource:%u rawRead:%u raw:%u allowed:%u..%u} active=%u debt=%u policy=%u",
                phase ? phase : "unknown",
                asset,
                refs.resource_read ? 1U : 0U,
                static_cast<unsigned>(refs.resource),
                refs.raw_read ? 1U : 0U,
                static_cast<unsigned>(refs.raw),
                static_cast<unsigned>(retirement.minimum_raw),
                static_cast<unsigned>(retirement.maximum_raw),
                static_cast<unsigned>(active),
                static_cast<unsigned>(prior_debt),
                static_cast<unsigned>(retirement.status));
            return false;
        }
        const std::uint32_t observed_debt =
            retirement.retirement_debt_after;
        if (observed_debt < prior_debt) {
            if (!set_rgba8_retirement_debt(asset, observed_debt)) {
                log_failure(
                    "RGBA8 retirement reconciliation phase=%s could not update asset=%zu debt=%u->%u",
                    phase ? phase : "unknown",
                    asset,
                    static_cast<unsigned>(prior_debt),
                    static_cast<unsigned>(observed_debt));
                return false;
            }
            removed_from_worklist = observed_debt == 0U;
            log_line(
                "rgba8Retirement reconcile phase=%s asset=%zu active=%u raw=%u debt=%u->%u",
                phase ? phase : "unknown",
                asset,
                static_cast<unsigned>(active),
                static_cast<unsigned>(refs.raw),
                static_cast<unsigned>(prior_debt),
                static_cast<unsigned>(observed_debt));
        }
        return true;
    };

    bool exact{true};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // A frame can reference at most twenty active and twenty staged logical
    // assets. Audit that bounded visible set together with the sparse deferred
    // retirement list, so correctness follows live work rather than the API
    // ceiling. Each asset is sampled once because the renderer may decrement
    // its RawImage reference asynchronously between two reads.
    std::array<std::size_t, kTabHostCapacity * 2U> visible_assets{};
    std::array<bool, kTabHostCapacity * 2U> visible_assets_audited{};
    std::size_t visible_asset_count{};
    const auto remember_visible_asset = [
        &visible_assets,
        &visible_asset_count,
        &exact,
        phase](std::size_t encoded) noexcept {
        if (encoded == 0U) return;
        const std::size_t asset = encoded - 1U;
        if (asset >= active_custom_tab_count() ||
            !catalogue_icon_requires_raster(asset)) {
            exact = false;
            log_failure(
                "RGBA8 retirement reconciliation phase=%s found invalid visible asset encoding=%zu",
                phase ? phase : "unknown",
                encoded);
            return;
        }
        for (std::size_t entry = 0;
             entry < visible_asset_count;
             ++entry) {
            if (visible_assets[entry] == asset) return;
        }
        if (visible_asset_count >= visible_assets.size()) {
            exact = false;
            log_failure(
                "RGBA8 retirement reconciliation phase=%s exceeded the bounded visible asset set",
                phase ? phase : "unknown");
            return;
        }
        visible_assets[visible_asset_count++] = asset;
    };
    for (std::size_t host = 0; host < active_tab_host_count(); ++host) {
        remember_visible_asset(g_rgba8_host_assets[host].load(
            std::memory_order_acquire));
        remember_visible_asset(g_staged_rgba8_host_assets[host].load(
            std::memory_order_acquire));
    }

    // The sparse list is render-thread-owned; clearing an entry swaps another
    // index into the same cursor, so that cursor is deliberately revisited.
    std::size_t cursor{};
    while (g_native_tab_runtime &&
           cursor < g_native_tab_runtime->retiring_tab_indices().size()) {
        const std::size_t asset =
            g_native_tab_runtime->retiring_tab_indices()[cursor];
        bool removed{};
        exact = reconcile_asset(asset, removed) && exact;
        for (std::size_t entry = 0;
             entry < visible_asset_count;
             ++entry) {
            if (visible_assets[entry] == asset) {
                visible_assets_audited[entry] = true;
                break;
            }
        }
        if (!removed) ++cursor;
    }
    for (std::size_t entry = 0;
         entry < visible_asset_count;
         ++entry) {
        if (visible_assets_audited[entry]) continue;
        bool removed{};
        exact = reconcile_asset(visible_assets[entry], removed) && exact;
    }
#else
    for (std::size_t asset = 0;
         asset < active_custom_tab_count();
         ++asset) {
        bool removed{};
        exact = reconcile_asset(asset, removed) && exact;
    }
#endif
    return reconcile_retired_rgba8_bank(phase) && exact;
}

bool rgba8_resources_at_root_baseline() noexcept {
    if (!g_rgba8_catalogue_ready.load(std::memory_order_acquire)) {
        return false;
    }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!reconcile_retired_rgba8_bank("epoch-root-baseline") ||
        g_rgba8_retired_bank_active) {
        return false;
    }
#endif

    // This predicate is used only at an epoch boundary after the current
    // display object reports a different Movie fingerprint. The old Movie has
    // already torn down its Shapes by then, so the live-host ledger still
    // describes the previous epoch and must not participate in this audit.
    // Prove the literal process-owned roots first; only after every asset is at
    // 1/1 may stale renderer-retirement debt be discarded and the caller
    // clear the old host ledger.
    for (std::size_t icon = 0;
         icon < active_custom_tab_count();
         ++icon) {
        if (!catalogue_icon_requires_raster(icon)) {
            const std::uint32_t debt =
                g_rgba8_retirement_debt[icon].load(
                    std::memory_order_acquire);
            if (debt != 0) {
                log_failure(
                    "RGBA8 epoch root-baseline audit found debt for non-raster asset=%zu debt=%u",
                    icon,
                    static_cast<unsigned>(debt));
                return false;
            }
            continue;
        }
        void* const resource = g_retained_rgba8_resources[icon].load(
            std::memory_order_acquire);
        void* const image = g_retained_rgba8_images[icon].load(
            std::memory_order_acquire);
        std::uint32_t resource_ref{};
        std::uint32_t raw_ref{};
        if (!resource || !image ||
            !guarded_read(
                static_cast<const std::byte*>(resource) + 0x08,
                resource_ref) ||
            !guarded_read(
                static_cast<const std::byte*>(image) + 0x08,
                raw_ref) ||
            resource_ref != 1U || raw_ref != 1U) {
            log_failure(
                "RGBA8 epoch root-baseline audit failed asset=%zu resource=%p ref=%u image=%p rawRef=%u",
                icon,
                resource,
                static_cast<unsigned>(resource_ref),
                image,
                static_cast<unsigned>(raw_ref));
            return false;
        }
    }
    clear_all_rgba8_retirement_debts();
    return true;
}

bool rgba8_references_match_tracked_hosts(
    const char* phase,
    std::size_t override_host = invalid_tab_index,
    std::size_t override_asset = 0,
    bool override_staged = false) noexcept {
    const TabHostMask rgba_hosts = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask staged_rgba_hosts =
        g_staged_rgba8_icon_hosts.load(std::memory_order_acquire);
    if (!g_rgba8_catalogue_ready.load(std::memory_order_acquire)) {
        const bool empty = rgba_hosts == 0 && staged_rgba_hosts == 0;
        if (!empty) {
            log_failure(
                "RGBA8 ref audit phase=%s found tracked hosts before the all-ready catalogue publication",
                phase ? phase : "unknown");
        }
        return empty;
    }

    // At most twenty front/back physical hosts can contribute references.
    // Store those sparse counts by logical index instead of allocating a
    // maximum-sized parallel array.
    std::array<std::pair<std::size_t, std::uint32_t>, kTabHostCapacity * 2U>
        expected_hosts{};
    std::size_t expected_host_entry_count{};
    bool expected_host_overflow{};
    const auto increment_expected_host = [&expected_hosts,
                                          &expected_host_entry_count,
                                          &expected_host_overflow](
        std::size_t custom) noexcept {
        for (std::size_t entry = 0;
             entry < expected_host_entry_count;
             ++entry) {
            if (expected_hosts[entry].first == custom) {
                ++expected_hosts[entry].second;
                return;
            }
        }
        if (expected_host_entry_count < expected_hosts.size()) {
            expected_hosts[expected_host_entry_count++] = {custom, 1U};
        } else {
            expected_host_overflow = true;
        }
    };
    const auto expected_host_count = [&expected_hosts,
                                      &expected_host_entry_count](
        std::size_t custom) noexcept -> std::uint32_t {
        for (std::size_t entry = 0;
             entry < expected_host_entry_count;
             ++entry) {
            if (expected_hosts[entry].first == custom) {
                return expected_hosts[entry].second;
            }
        }
        return 0U;
    };
    bool map_exact{true};
    const auto encoded_asset_is_current = [](std::size_t encoded) noexcept {
        if (encoded == 0) return true;
        const std::size_t asset = static_cast<std::size_t>(encoded - 1U);
        return asset < active_custom_tab_count() &&
            catalogue_icon_requires_raster(asset);
    };
    for (std::size_t host = 0; host < active_tab_host_count(); ++host) {
        const TabHostMask mask = TabHostMask{1} << host;
        const std::size_t encoded = host == override_host &&
                !override_staged
            ? override_asset
            : g_rgba8_host_assets[host].load(std::memory_order_acquire);
        const bool bit_set = host == override_host && !override_staged
            ? encoded != 0
            : (rgba_hosts & mask) != 0;
        if ((encoded != 0) != bit_set ||
            !encoded_asset_is_current(encoded)) {
            map_exact = false;
            continue;
        }
        if (encoded != 0) increment_expected_host(encoded - 1U);

        const std::size_t staged_encoded = host == override_host &&
                override_staged
            ? override_asset
            : g_staged_rgba8_host_assets[host].load(
                std::memory_order_acquire);
        const bool staged_bit_set = host == override_host && override_staged
            ? staged_encoded != 0
            : (staged_rgba_hosts & mask) != 0;
        if ((staged_encoded != 0) != staged_bit_set ||
            !encoded_asset_is_current(staged_encoded)) {
            map_exact = false;
            continue;
        }
        if (staged_encoded != 0) {
            increment_expected_host(staged_encoded - 1U);
        }
    }

    const auto audit_icon = [phase, &map_exact, &expected_host_count](
        std::size_t icon) noexcept {
        const std::uint32_t tracked_hosts = expected_host_count(icon);
        const std::uint32_t retirement_debt = rgba8_retirement_debt(icon);
        if (!catalogue_icon_requires_raster(icon)) {
            const bool entry_exact = tracked_hosts == 0 &&
                retirement_debt == 0;
            if (!entry_exact) {
                log_failure(
                    "RGBA8 ref audit phase=%s found tracked ownership for non-raster asset=%zu trackedHosts=%u retirementDebt=%u mapExact=%u",
                    phase ? phase : "unknown",
                    icon,
                    static_cast<unsigned>(tracked_hosts),
                    static_cast<unsigned>(retirement_debt),
                    map_exact ? 1U : 0U);
            }
            return entry_exact;
        }
        void* const resource = g_retained_rgba8_resources[icon].load(
            std::memory_order_acquire);
        void* const image = g_retained_rgba8_images[icon].load(
            std::memory_order_acquire);
        const std::uint64_t checksum =
            g_retained_rgba8_checksums[icon].load(
                std::memory_order_acquire);
        std::uint32_t resource_ref{};
        std::uint32_t raw_ref{};
        const bool resource_read = resource && guarded_read(
            static_cast<const std::byte*>(resource) + 0x08,
            resource_ref);
        const bool raw_read = image && guarded_read(
            static_cast<const std::byte*>(image) + 0x08,
            raw_ref);
        const std::uint32_t expected_raw =
            1U + tracked_hosts + retirement_debt;
        const bool entry_exact =
            g_retained_rgba8_ready[icon].load(
                std::memory_order_acquire) &&
            resource && image &&
            resource_read && raw_read && resource_ref == 1U &&
            raw_ref == expected_raw;
        if (!entry_exact) {
            log_failure(
                "RGBA8 ref audit phase=%s asset=%zu id=%d checksum=0x%016llX refs={resourceRead:%u resource:%u expected:1 rawRead:%u raw:%u expected:%u} trackedHosts=%u retirementDebt=%u mapExact=%u",
                phase ? phase : "unknown",
                icon,
                kRgba8IconIds[icon],
                static_cast<unsigned long long>(checksum),
                resource_read ? 1U : 0U,
                static_cast<unsigned>(resource_ref),
                raw_read ? 1U : 0U,
                static_cast<unsigned>(raw_ref),
                static_cast<unsigned>(expected_raw),
                static_cast<unsigned>(tracked_hosts),
                static_cast<unsigned>(retirement_debt),
                map_exact ? 1U : 0U);
        }
        return entry_exact;
    };

    if (expected_host_overflow) {
        map_exact = false;
        log_failure(
            "RGBA8 ref audit phase=%s exceeded the bounded active/staged host identity set",
            phase ? phase : "unknown");
    }
    bool exact = map_exact;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // A transition can affect only the active physical hosts plus resources
    // that still carry deferred renderer references. Immutable inactive roots
    // were proven at catalogue publication and are rechecked at epoch reset;
    // scanning every registered tab on every repaint would make navigation
    // scale with the public API ceiling instead of visible work.
    for (std::size_t entry = 0;
         entry < expected_host_entry_count;
         ++entry) {
        exact = audit_icon(expected_hosts[entry].first) && exact;
    }
    if (g_native_tab_runtime) {
        for (const std::size_t icon :
             g_native_tab_runtime->retiring_tab_indices()) {
            if (expected_host_count(icon) == 0U) {
                exact = audit_icon(icon) && exact;
            }
        }
    }
#else
    for (std::size_t icon = 0;
         icon < active_custom_tab_count();
         ++icon) {
        exact = audit_icon(icon) && exact;
    }
#endif
    if (!map_exact) {
        log_failure(
            "RGBA8 ref audit phase=%s found a host-bit/resource-index ledger mismatch (activeBits=0x%04X stagedBits=0x%04X overrideHost=%zu overrideAsset=%u overrideStaged=%u)",
            phase ? phase : "unknown",
            static_cast<unsigned>(rgba_hosts),
            static_cast<unsigned>(staged_rgba_hosts),
            override_host,
            static_cast<unsigned>(override_asset),
            override_staged ? 1U : 0U);
    }
    return exact;
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
// During a live immutable-icon handoff the active and inactive property banks
// can deliberately contain two different resources for the same stable tab.
// The ordinary audit is indexed by that stable tab and is therefore valid only
// outside this short overlap.  Audit the exact resource/image identities here
// until the target bank is published and the ordinary single-bank invariant is
// restored.
[[nodiscard]] bool rgba8_rebind_overlap_references_match(
    const char* phase) noexcept {
    const InPlaceRgba8RebindTransaction& transaction =
        g_in_place_rgba8_rebind_transaction;
    if (!transaction.active || !g_rgba8_staged_bank_ready ||
        !g_rgba8_staged_tab_generation ||
        g_rgba8_staged_resources_scratch.size() !=
            active_custom_tab_count()) {
        return false;
    }

    struct ExactResourceAuditEntry final {
        RetainedRgba8Resource retained{};
        std::size_t asset{invalid_tab_index};
        std::uint32_t hosts{};
        std::uint32_t debt{};
    };
    std::array<ExactResourceAuditEntry, kTabHostCapacity * 2U> entries{};
    std::size_t entry_count{};
    bool exact{true};

    const auto add_host = [&entries, &entry_count, &exact](
        std::size_t asset,
        const RetainedRgba8Resource& retained,
        std::uint32_t debt) noexcept {
        if (asset >= active_custom_tab_count() || !retained.ready ||
            !retained.resource || !retained.image) {
            exact = false;
            return;
        }
        for (std::size_t entry = 0; entry < entry_count; ++entry) {
            if (entries[entry].retained.resource != retained.resource) {
                continue;
            }
            exact = exact &&
                entries[entry].retained.image == retained.image &&
                entries[entry].retained.checksum == retained.checksum &&
                entries[entry].debt == debt;
            ++entries[entry].hosts;
            return;
        }
        if (entry_count >= entries.size()) {
            exact = false;
            return;
        }
        entries[entry_count++] = {
            .retained = retained,
            .asset = asset,
            .hosts = 1U,
            .debt = debt,
        };
    };

    const TabHostMask active_bits = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask staged_bits = g_staged_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    exact = exact &&
        (active_bits & ~transaction.source_rgba_mask) == 0U &&
        (staged_bits & ~transaction.target_rgba_mask) == 0U;

    for (std::size_t host = 0; host < active_tab_host_count(); ++host) {
        const TabHostMask mask = TabHostMask{1} << host;
        const std::size_t active_encoded = g_rgba8_host_assets[host].load(
            std::memory_order_acquire);
        const bool active_bit = (active_bits & mask) != 0;
        const bool source_raster_was_retired =
            (transaction.source_rgba_mask & mask) != 0U &&
            (transaction.completed_clear_mask & mask) != 0U;
        if ((active_encoded != 0U) != active_bit ||
            (active_encoded == 0U &&
                transaction.source_assets[host] != 0U &&
                !source_raster_was_retired) ||
            (active_encoded != 0U &&
                active_encoded != transaction.source_assets[host])) {
            exact = false;
        } else if (active_encoded != 0U) {
            const std::size_t asset = active_encoded - 1U;
            const RetainedRgba8Resource retained =
                current_retained_rgba8(asset);
            if (retained.resource != transaction.source_resources[host]) {
                exact = false;
            }
            add_host(
                asset,
                retained,
                rgba8_debt_for_resource(asset, retained.resource));
        }

        const std::size_t staged_encoded =
            g_staged_rgba8_host_assets[host].load(
                std::memory_order_acquire);
        const bool staged_bit = (staged_bits & mask) != 0;
        if ((staged_encoded != 0U) != staged_bit) {
            exact = false;
        } else if (staged_encoded != 0U) {
            const std::size_t asset = staged_encoded - 1U;
            if (asset >= g_rgba8_staged_resources_scratch.size() ||
                staged_encoded != transaction.target_assets[host]) {
                exact = false;
                continue;
            }
            const RetainedRgba8Resource& retained =
                g_rgba8_staged_resources_scratch[asset];
            if (retained.resource != transaction.target_resources[host]) {
                exact = false;
            }
            add_host(asset, retained, 0U);
        }
    }

    for (std::size_t entry = 0; entry < entry_count; ++entry) {
        const ExactResourceAuditEntry& expected = entries[entry];
        const Rgba8ReferenceSnapshot refs =
            snapshot_rgba8_references(expected.retained);
        const std::uint32_t expected_raw =
            1U + expected.hosts + expected.debt;
        const bool entry_exact = refs.resource_read && refs.resource == 1U &&
            refs.raw_read && refs.raw == expected_raw;
        exact = exact && entry_exact;
        if (!entry_exact) {
            log_failure(
                "RGBA8 overlap ref audit phase=%s asset=%zu resource=%p image=%p refs={resource:%u/%u expected:1 raw:%u/%u expected:%u} hosts=%u debt=%u",
                phase ? phase : "unknown",
                expected.asset,
                expected.retained.resource,
                expected.retained.image,
                refs.resource_read ? 1U : 0U,
                static_cast<unsigned>(refs.resource),
                refs.raw_read ? 1U : 0U,
                static_cast<unsigned>(refs.raw),
                static_cast<unsigned>(expected_raw),
                static_cast<unsigned>(expected.hosts),
                static_cast<unsigned>(expected.debt));
        }
    }
    if (!exact) {
        log_failure(
            "RGBA8 overlap ledger audit phase=%s failed (active=0x%04X/0x%04X staged=0x%04X target=0x%04X resources=%zu)",
            phase ? phase : "unknown",
            static_cast<unsigned>(active_bits),
            static_cast<unsigned>(transaction.source_rgba_mask),
            static_cast<unsigned>(staged_bits),
            static_cast<unsigned>(transaction.target_rgba_mask),
            entry_count);
    }
    return exact;
}
#else
[[nodiscard]] bool rgba8_rebind_overlap_references_match(
    const char* phase) noexcept {
    return rgba8_references_match_tracked_hosts(phase);
}
#endif

void log_rgba8_lifecycle_snapshot(
    const char* phase,
    void* top) noexcept {
    // The full snapshot walks every retained asset and was invaluable while
    // proving ownership. In the 50-tab timing fixture it would perform that
    // diagnostic walk after every painted cell, so omit it from successful
    // stress runs. Transaction pre/postflight reference gates remain active.
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!erui::detail::trace_logging()) return;
#else
    if (kStressQuietSuccessLogging) return;
#endif

    constexpr std::uint32_t kMaximumLifecycleLogs = 512;
    const std::uint32_t event = g_rgba8_lifecycle_log_count.fetch_add(
        1, std::memory_order_relaxed) + 1;
    if (event > kMaximumLifecycleLogs) return;

    const TabHostMask hosts = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask rgba_hosts = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask staged_hosts =
        g_staged_injected_icon_hosts.load(std::memory_order_acquire);
    const TabHostMask staged_rgba_hosts =
        g_staged_rgba8_icon_hosts.load(std::memory_order_acquire);
    log_line(
        "rgba8Lifecycle[%u] phase=%s epoch=%llu top=%p catalogueReady=%u fingerprint={epoch:%llu interface:%p ownerLink:%p movieRoot:%p} hosts={activeBits:0x%04X activeLive:%u activeRgbaBits:0x%04X activeRgbaLive:%u stagedBits:0x%04X stagedLive:%u stagedRgbaBits:0x%04X stagedRgbaLive:%u} rgbaEvents={attach:%llu detach:%llu}",
        static_cast<unsigned>(event),
        phase ? phase : "unknown",
        static_cast<unsigned long long>(g_runtime.presentation_epoch),
        top,
        g_rgba8_catalogue_ready.load(std::memory_order_acquire) ? 1U : 0U,
        static_cast<unsigned long long>(
            g_injected_movie_fingerprint.presentation_epoch),
        g_injected_movie_fingerprint.object_interface,
        g_injected_movie_fingerprint.owner_link,
        g_injected_movie_fingerprint.movie_root,
        static_cast<unsigned>(hosts),
        static_cast<unsigned>(count_tracked_icon_hosts(hosts)),
        static_cast<unsigned>(rgba_hosts),
        static_cast<unsigned>(count_tracked_icon_hosts(rgba_hosts)),
        static_cast<unsigned>(staged_hosts),
        static_cast<unsigned>(count_tracked_icon_hosts(staged_hosts)),
        static_cast<unsigned>(staged_rgba_hosts),
        static_cast<unsigned>(count_tracked_icon_hosts(staged_rgba_hosts)),
        static_cast<unsigned long long>(
            g_rgba8_successful_attach_count.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_rgba8_successful_detach_count.load(std::memory_order_relaxed)));

    for (std::size_t host = 0; host < active_tab_host_count(); ++host) {
        const std::size_t encoded = g_rgba8_host_assets[host].load(
            std::memory_order_acquire);
        if (encoded == 0 || encoded > active_custom_tab_count()) continue;
        const std::size_t icon = encoded - 1U;
        log_line(
            "rgba8Lifecycle[%u] host={layer:%s physical:%zu} asset=%zu id=%d checksum=0x%016llX",
            static_cast<unsigned>(event),
            host < active_physical_tab_count() ? "front" : "back",
            host % active_physical_tab_count(),
            icon,
            kRgba8IconIds[icon],
            static_cast<unsigned long long>(
                g_retained_rgba8_checksums[icon].load(
                    std::memory_order_acquire)));
    }

    for (std::size_t icon = 0;
         icon < active_custom_tab_count();
         ++icon) {
        if (!catalogue_icon_requires_raster(icon)) continue;
        const std::uint32_t live_asset_hosts =
            count_active_rgba8_asset_hosts(icon);
        void* const resource = g_retained_rgba8_resources[icon].load(
            std::memory_order_acquire);
        void* const image = g_retained_rgba8_images[icon].load(
            std::memory_order_acquire);
        const std::uint64_t checksum =
            g_retained_rgba8_checksums[icon].load(
                std::memory_order_acquire);
        const std::uint32_t retirement_debt =
            g_rgba8_retirement_debt[icon].load(
                std::memory_order_acquire);
        std::uint32_t resource_ref{};
        std::uint32_t raw_ref{};
        // Passive, opaque RawImage scalar samples. Static analysis suggests
        // +0x10 participates in renderer realization, but this probe does not
        // treat that interpretation (or any sampled value) as a safety gate.
        void* image_opaque_10{};
        void* image_auxiliary_18{};
        void* image_auxiliary_20{};
        const bool resource_ref_read = resource && guarded_read(
            static_cast<const std::byte*>(resource) + 0x08,
            resource_ref);
        const bool raw_ref_read = image && guarded_read(
            static_cast<const std::byte*>(image) + 0x08,
            raw_ref);
        const bool renderer_fields_read = image &&
            guarded_read(
                static_cast<const std::byte*>(image) + 0x10,
                image_opaque_10) &&
            guarded_read(
                static_cast<const std::byte*>(image) + 0x18,
                image_auxiliary_18) &&
            guarded_read(
                static_cast<const std::byte*>(image) + 0x20,
                image_auxiliary_20);
        log_line(
            "rgba8Lifecycle[%u] asset=%zu id=%d sourceAsset=%zu resource=%p image=%p checksum=0x%016llX liveHosts=%u retirementDebt=%u refs={resourceRead:%u resource:%u rootExpected:1 rawRead:%u raw:%u rawExpected:%u} imageState={read:%u opaque10:%p opaque18:%p opaque20:%p}",
            static_cast<unsigned>(event),
            icon,
            kRgba8IconIds[icon],
            icon,
            resource,
            image,
            static_cast<unsigned long long>(checksum),
            static_cast<unsigned>(live_asset_hosts),
            static_cast<unsigned>(retirement_debt),
            resource_ref_read ? 1U : 0U,
            static_cast<unsigned>(resource_ref),
            raw_ref_read ? 1U : 0U,
            static_cast<unsigned>(raw_ref),
            static_cast<unsigned>(
                1U + live_asset_hosts + retirement_debt),
            renderer_fields_read ? 1U : 0U,
            image_opaque_10,
            image_auxiliary_18,
            image_auxiliary_20);
    }
    if (event == kMaximumLifecycleLogs) {
        log_line("Further RGBA8 lifecycle snapshots are suppressed");
    }
}

void** __fastcall texture_lookup_detour(
    void* repository,
    void** out_resource,
    const wchar_t* resource_name) noexcept {
    TextureLookupFn const original =
        g_original_texture_lookup.load(std::memory_order_acquire);
    if (!original) {
        if (out_resource) *out_resource = nullptr;
        return out_resource;
    }
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) {
        return original(repository, out_resource, resource_name);
    }

    VectorIconKind kind{VectorIconKind::none};
    const wchar_t* synthetic_resource{};
    std::size_t rgba8_index{invalid_tab_index};
    if (parse_rgba8_icon_resource_name(resource_name, rgba8_index)) {
        kind = VectorIconKind::cross;
        synthetic_resource =
            kRgba8IconResourceNames[rgba8_index].data();
    }
    if (exact_resource_name(resource_name, kOverflowIconResource)) {
        kind = VectorIconKind::ellipsis;
        synthetic_resource = kOverflowIconResource;
    }
    if (g_resolving_icon_alias || kind == VectorIconKind::none) {
        return original(repository, out_resource, resource_name);
    }
    g_pending_aliased_icon = {};

    // The retained RGBA8 resource follows the same ownership contract as a
    // repository hit: publish exactly one owned lookup reference. The native
    // presenter creates and consumes its own binder-local reference, then
    // releases this lookup reference after the bind returns. Our permanent
    // root therefore remains at refcount 1 between presentations.
    const bool exact_repaint_coordinate = g_repaint_context.active &&
        g_repaint_context.top &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.layer != TabLayer::none &&
        g_repaint_context.physical < active_physical_tab_count();
    const bool exact_repaint_context = exact_repaint_coordinate &&
        g_runtime.active;
    const std::size_t logical = g_repaint_context.logical;
    const bool logical_resolved = exact_repaint_context &&
        logical < active_logical_tab_count();
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const std::size_t logical_custom = logical_resolved
        ? stable_custom_index_for_logical(logical)
        : invalid_tab_index;
#else
    const std::size_t logical_custom = logical >= kStockLogicalTabCount
        ? logical - kStockLogicalTabCount
        : invalid_tab_index;
#endif
    const bool exact_catalogue_icon =
        rgba8_index < active_custom_tab_count() &&
        logical_resolved && logical_custom == rgba8_index;
    const bool effective_raster = exact_catalogue_icon &&
        catalogue_icon_has_effective_raster(rgba8_index);
    const bool exact_initial_logical_raster =
        kind == VectorIconKind::cross && exact_repaint_coordinate &&
        rgba8_index < active_custom_tab_count() &&
        logical < active_logical_tab_count() &&
        logical_custom == rgba8_index &&
        catalogue_icon_requires_raster(rgba8_index);
    const bool transparent_vector_placeholder =
        should_use_transparent_initial_raster_placeholder({
            g_initial_target_repaint,
            g_runtime.active,
            g_repaint_context.top == g_runtime.top,
            exact_repaint_coordinate,
            exact_initial_logical_raster,
            exact_initial_logical_raster &&
                catalogue_icon_has_effective_raster(rgba8_index),
        });
    const RetainedRgba8Resource retained =
        rgba8_index < active_custom_tab_count()
        ? effective_retained_rgba8(rgba8_index)
        : RetainedRgba8Resource{};
    void* const retained_resource = retained.resource;
    void* const retained_image = retained.image;
    const std::uint64_t retained_checksum = retained.checksum;
    if (kind == VectorIconKind::cross && effective_raster &&
        retained_resource && retained_image && out_resource &&
        !g_rgba8_publication_disabled.load(std::memory_order_acquire)) {
        void* resource_vtable{};
        std::uint32_t resource_ref_before{};
        void* exposed_image{};
        void* delegate_vtable{};
        std::uint32_t delegate_ref{};
        void* delegated_image{};
        std::uint32_t resource_kind{};
        void* image_vtable{};
        std::uint32_t image_ref_before{};
        void* prior_output{};
        const bool fields_read =
            guarded_read(retained_resource, resource_vtable) &&
            guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x08,
                resource_ref_before) &&
            guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x18,
                exposed_image) &&
            guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x20,
                delegate_vtable) &&
            guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x28,
                delegate_ref) &&
            guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x48,
                delegated_image) &&
            guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x60,
                resource_kind) &&
            guarded_read(retained_image, image_vtable) &&
            guarded_read(
                static_cast<const std::byte*>(retained_image) + 0x08,
                image_ref_before) &&
            guarded_read(out_resource, prior_output);
        // The native lookup treats this as a pure output parameter and writes
        // it unconditionally on every path. The presenter intentionally does
        // not initialize its stack slot, so prior_output is diagnostic only.
        const bool exact_resource = fields_read &&
            g_native_tabs_build_profile &&
            resource_vtable ==
                g_game.base + kScaleformImageResourceVtable271Rva &&
            exposed_image == retained_image &&
            delegate_vtable ==
                g_game.base + kScaleformImageDelegateVtable271Rva &&
            delegate_ref == 1U && delegated_image == nullptr &&
            resource_kind == 1U &&
            image_vtable == g_game.base + kRawImageVtableRva &&
            resource_ref_before == 1U && image_ref_before >= 1U;

        unsigned long add_ref_seh{};
        bool add_ref_completed{};
        std::uint32_t resource_ref_after{};
        bool ref_after_read{};
        bool output_written{};
        bool output_verified{};
        if (exact_resource) {
            add_ref_completed =
                guarded_add_ref_scaleform_image_resource(
                    reinterpret_cast<ScaleformImageResourceAddRefFn>(
                        g_game.base +
                        kScaleformImageResourceAddRef271Rva),
                    retained_resource,
                    add_ref_seh);
            ref_after_read = add_ref_completed && guarded_read(
                static_cast<const std::byte*>(retained_resource) + 0x08,
                resource_ref_after);
            const bool exact_add_ref = ref_after_read &&
                resource_ref_after == resource_ref_before + 1U;
            if (exact_add_ref) {
                output_written = guarded_copy(
                    out_resource,
                    &retained_resource,
                    sizeof(retained_resource));
                void* published{};
                output_verified = output_written &&
                    guarded_read(out_resource, published) &&
                    published == retained_resource;
            }
        }

        const std::uint32_t event =
            g_rgba8_lookup_log_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        log_line(
            "rgba8Lookup[%u] epoch=%llu icon=%zu logical=%zu checksum=0x%016llX context={layer:%u physical:%zu} resource=%p image=%p fieldsRead=%u exact=%u layout={delegate:%p delegateRef:%u delegated:%p kind:%u priorOutputIgnored:%p} addRef=%u seh=0x%08lX refs={before:%u afterRead:%u after:%u} imageRef=%u output={written:%u verified:%u}",
            static_cast<unsigned>(event),
            static_cast<unsigned long long>(
                g_runtime.presentation_epoch),
            rgba8_index,
            logical,
            static_cast<unsigned long long>(retained_checksum),
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            retained_resource,
            retained_image,
            fields_read ? 1U : 0U,
            exact_resource ? 1U : 0U,
            delegate_vtable,
            static_cast<unsigned>(delegate_ref),
            delegated_image,
            static_cast<unsigned>(resource_kind),
            prior_output,
            add_ref_completed ? 1U : 0U,
            add_ref_seh,
            static_cast<unsigned>(resource_ref_before),
            ref_after_read ? 1U : 0U,
            static_cast<unsigned>(resource_ref_after),
            static_cast<unsigned>(image_ref_before),
            output_written ? 1U : 0U,
            output_verified ? 1U : 0U);

        // A successful guarded write is the ownership handoff. The readback is
        // diagnostic only; never Release a reference after its pointer has
        // become caller-visible, even if that diagnostic read unexpectedly
        // fails.
        if (output_written) {
            g_pending_aliased_icon = {
                retained_resource,
                retained_image,
                kind,
                g_repaint_context,
                true,
                false,
                rgba8_index,
                logical,
                retained_checksum,
                g_runtime.presentation_epoch,
            };
            return out_resource;
        }

        // AddRef succeeded but publication did not. Balance only the reference
        // owned by this failed lookup attempt; the process root stays alive.
        if (add_ref_completed) {
            unsigned long rollback_seh{};
            const bool rolled_back =
                guarded_release_scaleform_image_resource(
                    reinterpret_cast<ScaleformImageResourceReleaseFn>(
                        g_game.base +
                        kScaleformImageResourceRelease271Rva),
                    retained_resource,
                    rollback_seh);
            log_line(
                "rgba8Lookup[%u] rollback completed=%u seh=0x%08lX",
                static_cast<unsigned>(event),
                rolled_back ? 1U : 0U,
                rollback_seh);
        }
        g_rgba8_publication_disabled.store(true, std::memory_order_release);
        log_failure(
            "direct RGBA8 lookup failed closed; later custom icons use the proven vector fallback");
    }

    // Call the SafetyHook trampoline with a different, known-native name. The
    // guard also makes this safe if the lookup implementation gains an
    // indirect recursive path during later experiments.
    g_resolving_icon_alias = true;
    void** const result = original(
        repository, out_resource, kAliasedNativeIconResource);
    g_resolving_icon_alias = false;

    void* resolved{};
    (void)guarded_read(out_resource, resolved);
    if (resolved) {
        g_pending_aliased_icon = {
            resolved,
            nullptr,
            kind,
            g_repaint_context,
            false,
            transparent_vector_placeholder,
            rgba8_index,
            logical,
            retained_checksum,
            g_runtime.presentation_epoch,
        };
    }
    const std::uint32_t event =
        g_alias_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumAliasLogs) {
        log_line(
            "iconAlias[%u] epoch=%llu kind=%u rgbaIcon=%zu logical=%zu exactCatalogue=%u transparentPlaceholder=%u context={active:%u layer:%u physical:%zu} %ls -> %ls resource=%p",
            static_cast<unsigned>(event),
            static_cast<unsigned long long>(
                g_runtime.presentation_epoch),
            static_cast<unsigned>(kind),
            rgba8_index,
            logical,
            exact_catalogue_icon ? 1U : 0U,
            transparent_vector_placeholder ? 1U : 0U,
            g_repaint_context.active ? 1U : 0U,
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            synthetic_resource,
            kAliasedNativeIconResource,
            resolved);
    }
    if (event == kMaximumAliasLogs) {
        log_line("Further synthetic-icon alias events are suppressed");
    }
    return result;
}

#if 0
// Retained only as research history. Never compile or call this path:
// BitmapData work from the image-binding callback corrupts Scaleform's
// renderer state even when no Bitmap is attached to the display list.
ShapeAttachProbeResult probe_dynamic_bitmap_attachment(
    std::uint32_t attempt,
    void* target_value,
    void* movie_root,
    void* create_object_slot,
    ImageBindFn image_binder,
    void** resource,
    const std::array<float, 4>& rectangle) noexcept {
    ShapeAttachProbeResult result{};

    // Preserve the native ImageResource ownership contract without asking the
    // authored static Shape to accept a runtime drawing context (it cannot).
    // A short-lived dynamic Shape consumes the resource through the already
    // proven binder, but is never attached to the display list. The authored
    // GFX artwork therefore remains the visual fallback if Bitmap creation
    // fails later.
    alignas(16) std::array<std::byte, 0x38> resource_shape_wrapper{};
    bool resource_shape_constructed{};
    bool resource_shape_created{};
    unsigned long resource_shape_seh{};
    if (movie_root && create_object_slot && g_construct_scaleform_value) {
#if defined(_MSC_VER)
        __try {
#endif
            resource_shape_constructed = g_construct_scaleform_value(
                resource_shape_wrapper.data()) ==
                resource_shape_wrapper.data();
            if (resource_shape_constructed) {
                reinterpret_cast<MovieCreateObjectFn>(create_object_slot)(
                    movie_root,
                    resource_shape_wrapper.data() + 0x08,
                    "flash.display.Shape",
                    nullptr,
                    0);
                DisplayValueView resource_shape_view{};
                resource_shape_created = inspect_display_value(
                        resource_shape_wrapper.data(), resource_shape_view) &&
                    exact_as3_display_bridge(resource_shape_view) &&
                    resource_shape_view.native_display_vtable ==
                        g_game.base + kDynamicShapeVtable271Rva;
            }
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            resource_shape_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool fallback_bound{};
    unsigned long fallback_seh{};
    if (resource_shape_created) {
        result.resource_consumed = true;
#if defined(_MSC_VER)
        __try {
#endif
            fallback_bound = image_binder && image_binder(
                resource_shape_wrapper.data(), resource, rectangle.data());
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fallback_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    result.bound = fallback_bound;
    if (resource_shape_seh == 0 && fallback_seh == 0) {
        destroy_local_scaleform_value(
            resource_shape_wrapper,
            resource_shape_constructed,
            "temporary resource Shape");
    }
    if (!resource_shape_created || !fallback_bound ||
        resource_shape_seh != 0 || fallback_seh != 0) {
        if (attempt <= kMaximumShapeAttachLogs) {
            log_line(
                "bitmapAttach[%u] resourceShape={created:%u seh:0x%08lX} fallback=%u fallbackSeh=0x%08lX consumed=%u final=0",
                static_cast<unsigned>(attempt),
                resource_shape_created ? 1U : 0U,
                resource_shape_seh,
                fallback_bound ? 1U : 0U,
                fallback_seh,
                result.resource_consumed ? 1U : 0U);
        }
        return result;
    }

    alignas(16) std::array<std::byte, 0x38> parent_wrapper{};
    const char* parent_name = "parent";
    bool parent_constructed{};
    unsigned long parent_seh{};
    if (target_value && g_get_scaleform_member) {
#if defined(_MSC_VER)
        __try {
#endif
            parent_constructed = g_get_scaleform_member(
                target_value, parent_wrapper.data(), &parent_name) ==
                parent_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            parent_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    DisplayValueView parent_view{};
    DisplayValueView target_view{};
    const bool exact_parent = parent_seh == 0 && parent_constructed &&
        inspect_display_value(parent_wrapper.data(), parent_view) &&
        exact_as3_display_bridge(parent_view);
    const bool exact_target = inspect_display_value(target_value, target_view) &&
        exact_as3_display_bridge(target_view) &&
        validated_authored_icon_target_vtable(
            target_view.native_display_vtable);

    // A repaint can reuse the pair already retained by the GFX parent. The
    // Bitmap owns the visual object; the explicit BitmapData property keeps
    // the backing pixels alive under the same symmetric cleanup boundary.
    alignas(16) std::array<std::byte, 0x38> existing_bitmap_wrapper{};
    alignas(16) std::array<std::byte, 0x38> existing_data_wrapper{};
    bool existing_bitmap_constructed{};
    bool existing_data_constructed{};
    if (exact_parent && g_get_scaleform_member) {
        const char* bitmap_property = kInjectedIconProperty;
        const char* data_property = kInjectedBitmapDataProperty;
#if defined(_MSC_VER)
        __try {
#endif
            existing_bitmap_constructed = g_get_scaleform_member(
                parent_wrapper.data(),
                existing_bitmap_wrapper.data(),
                &bitmap_property) == existing_bitmap_wrapper.data();
            existing_data_constructed = g_get_scaleform_member(
                parent_wrapper.data(),
                existing_data_wrapper.data(),
                &data_property) == existing_data_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            existing_bitmap_constructed = false;
            existing_data_constructed = false;
        }
#endif
    }
    DisplayValueView existing_bitmap_view{};
    ObjectValueView existing_data_view{};
    const bool reusable_bitmap = existing_bitmap_constructed &&
        existing_data_constructed &&
        inspect_display_value(
            existing_bitmap_wrapper.data(), existing_bitmap_view) &&
        exact_as3_display_bridge(existing_bitmap_view) &&
        inspect_object_value(existing_data_wrapper.data(), existing_data_view) &&
        exact_as3_object_bridge(existing_data_view);

    alignas(16) std::array<std::byte, 0x38> created_data_wrapper{};
    alignas(16) std::array<std::byte, 0x38> created_bitmap_wrapper{};
    bool created_data_constructed{};
    bool created_bitmap_constructed{};
    bool data_created{};
    bool pixels_written{};
    bool bitmap_created{};
    unsigned long create_seh{};

    ObjectValueView created_data_view{};
    DisplayValueView created_bitmap_view{};
    if (exact_parent && exact_target && !reusable_bitmap &&
        movie_root && create_object_slot && g_construct_scaleform_value) {
#if defined(_MSC_VER)
        __try {
#endif
            created_data_constructed = g_construct_scaleform_value(
                created_data_wrapper.data()) == created_data_wrapper.data();
            if (created_data_constructed) {
                const std::array<RawGfxValue, 4> arguments{
                    make_int_value(kProbeBitmapWidth),
                    make_int_value(kProbeBitmapHeight),
                    make_bool_value(true),
                    // Keep every untouched pixel transparent. A bad transform
                    // must remain a harmless overlay instead of covering the
                    // Configuration movie with an opaque test surface.
                    make_uint_value(0x00000000U),
                };
                reinterpret_cast<MovieCreateObjectFn>(create_object_slot)(
                    movie_root,
                    created_data_wrapper.data() + 0x08,
                    "flash.display.BitmapData",
                    arguments.data(),
                    static_cast<std::uint32_t>(arguments.size()));
                data_created = inspect_object_value(
                        created_data_wrapper.data(), created_data_view) &&
                    exact_as3_object_bridge(created_data_view);
            }

            // Draw a two-color X. Sixty-four calls are enough to prove
            // caller-supplied ARGB pixels without making the UI hook perform
            // a full image upload before the bridge itself is validated.
            pixels_written = data_created;
            for (std::int32_t y = 0;
                 pixels_written && y < kProbeBitmapHeight;
                 ++y) {
                std::array<RawGfxValue, 3> pixel_arguments{
                    make_int_value(y),
                    make_int_value(y),
                    make_uint_value(0xC042E8F5U),
                };
                pixels_written = reinterpret_cast<ValueInvokeFn>(
                    created_data_view.invoke_slot)(
                    created_data_view.object_interface,
                    created_data_view.object_payload,
                    nullptr,
                    "setPixel32",
                    pixel_arguments.data(),
                    pixel_arguments.size(),
                    false);
                const std::int32_t opposite =
                    kProbeBitmapWidth - 1 - y;
                if (pixels_written && opposite != y) {
                    pixel_arguments[0] = make_int_value(opposite);
                    pixel_arguments[2] = make_uint_value(0xC0FFC857U);
                    pixels_written = reinterpret_cast<ValueInvokeFn>(
                        created_data_view.invoke_slot)(
                        created_data_view.object_interface,
                        created_data_view.object_payload,
                        nullptr,
                        "setPixel32",
                        pixel_arguments.data(),
                        pixel_arguments.size(),
                        false);
                }
            }

            if (pixels_written) {
                created_bitmap_constructed = g_construct_scaleform_value(
                    created_bitmap_wrapper.data()) ==
                    created_bitmap_wrapper.data();
            }
            if (created_bitmap_constructed) {
                reinterpret_cast<MovieCreateObjectFn>(create_object_slot)(
                    movie_root,
                    created_bitmap_wrapper.data() + 0x08,
                    "flash.display.Bitmap",
                    created_data_wrapper.data() + 0x08,
                    1);
                bitmap_created = inspect_display_value(
                        created_bitmap_wrapper.data(), created_bitmap_view) &&
                    exact_as3_display_bridge(created_bitmap_view);
            }
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            create_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    void* const bitmap_wrapper = reusable_bitmap
        ? static_cast<void*>(existing_bitmap_wrapper.data())
        : static_cast<void*>(created_bitmap_wrapper.data());
    void* const data_wrapper = reusable_bitmap
        ? static_cast<void*>(existing_data_wrapper.data())
        : static_cast<void*>(created_data_wrapper.data());
    const DisplayValueView& bitmap_view = reusable_bitmap
        ? existing_bitmap_view
        : created_bitmap_view;
    const bool exact_bitmap = reusable_bitmap ||
        (create_seh == 0 && data_created && pixels_written && bitmap_created);

    bool natural_size_used{};
    bool positioned{};
    unsigned long presentation_seh{};
    if (exact_bitmap && rectangle[2] > rectangle[0] &&
        rectangle[3] > rectangle[1]) {
#if defined(_MSC_VER)
        __try {
#endif
            // The previous probe assigned ActionScript width/height before
            // attachment and produced a full-screen opaque surface. Isolate
            // Bitmap rendering first: retain its natural 32x32 dimensions and
            // use only the already-proven native x/y DisplayInfo update.
            natural_size_used = true;
            positioned = g_set_scaleform_position &&
                g_set_scaleform_position(
                    bitmap_wrapper, rectangle[0], rectangle[1]) ==
                    bitmap_wrapper;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            presentation_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool bitmap_property_set{};
    bool data_property_set{};
    bool authored_property_set{};
    bool add_child_attempted{};
    bool attached{};
    bool authored_hidden{};
    unsigned long attach_seh{};
    if (positioned && exact_parent && exact_target) {
#if defined(_MSC_VER)
        __try {
#endif
            const auto set_member = reinterpret_cast<ValueSetMemberFn>(
                parent_view.set_member_slot);
            bitmap_property_set = set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                kInjectedIconProperty,
                static_cast<const std::byte*>(bitmap_wrapper) + 0x08,
                true);
            data_property_set = bitmap_property_set && set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                kInjectedBitmapDataProperty,
                static_cast<const std::byte*>(data_wrapper) + 0x08,
                true);
            authored_property_set = data_property_set && set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                kAuthoredIconProperty,
                static_cast<const std::byte*>(target_value) + 0x08,
                true);
            if (authored_property_set) {
                add_child_attempted = true;
                attached = reinterpret_cast<ValueInvokeFn>(
                    parent_view.invoke_slot)(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    nullptr,
                    "addChild",
                    static_cast<const std::byte*>(bitmap_wrapper) + 0x08,
                    1,
                    true);
            }
            // Keep the authored icon visible during this isolation run. The
            // transparent Bitmap is intentionally an overlay, so even an
            // unexpected transform cannot remove the known-good fallback.
            authored_hidden = false;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            attach_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    const bool custom_attached = attach_seh == 0 && attached;

    bool rollback_detached{};
    bool rollback_hidden{};
    bool rollback_visible{};
    bool properties_cleared{};
    bool rollback_retained{};
    unsigned long rollback_seh{};
    alignas(16) std::array<std::byte, 0x38> undefined_wrapper{};
    bool undefined_constructed{};
    if (!custom_attached &&
        (bitmap_property_set || data_property_set || authored_property_set)) {
        const bool attachment_uncertain =
            attach_seh != 0 && add_child_attempted;
        // Assume a display-list reference must be retained until a normal
        // removeChild return proves otherwise. This remains true even if the
        // rollback call itself raises a native exception.
        rollback_retained = attached || attachment_uncertain;
#if defined(_MSC_VER)
        __try {
#endif
            if (attached || attachment_uncertain) {
                rollback_detached = reinterpret_cast<ValueInvokeFn>(
                    parent_view.invoke_slot)(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    nullptr,
                    "removeChild",
                    static_cast<const std::byte*>(bitmap_wrapper) + 0x08,
                    1,
                    true);
            } else {
                rollback_detached = true;
            }
            if (rollback_detached) rollback_retained = false;
            if (!rollback_detached && g_set_scaleform_visible) {
                rollback_hidden = g_set_scaleform_visible(
                    bitmap_wrapper, 0) == bitmap_wrapper;
            }
            rollback_visible = !authored_hidden ||
                (g_set_scaleform_visible &&
                    g_set_scaleform_visible(target_value, 1) == target_value);
            undefined_constructed = rollback_detached &&
                g_construct_scaleform_value &&
                g_construct_scaleform_value(undefined_wrapper.data()) ==
                    undefined_wrapper.data();
            if (undefined_constructed) {
                const auto set_member = reinterpret_cast<ValueSetMemberFn>(
                    parent_view.set_member_slot);
                const bool bitmap_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    kInjectedIconProperty,
                    undefined_wrapper.data() + 0x08,
                    true);
                const bool data_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    kInjectedBitmapDataProperty,
                    undefined_wrapper.data() + 0x08,
                    true);
                const bool authored_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    kAuthoredIconProperty,
                    undefined_wrapper.data() + 0x08,
                    true);
                properties_cleared = bitmap_cleared && data_cleared &&
                    authored_cleared;
            }
            // If removal failed or an Invoke fault made attachment state
            // uncertain, keep every parent-owned reference and track this
            // cell. A later viewport cleanup can safely retry removeChild.
            rollback_retained = !rollback_detached;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            rollback_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    if ((custom_attached || rollback_retained) &&
        g_repaint_context.active &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.physical < active_physical_tab_count() &&
        g_repaint_context.layer != TabLayer::none) {
        const std::size_t bit_index = g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0);
        g_injected_icon_hosts.fetch_or(
            TabHostMask{1} << bit_index,
            std::memory_order_acq_rel);
    }

    if (attempt <= kMaximumShapeAttachLogs) {
        log_line(
            "bitmapAttach[%u] context=%u layer=%u physical=%zu fallback=%u reused=%u dataCreated=%u pixelsWritten=%u bitmapCreated=%u bitmapFlags=0x%08X bitmapNativeVtable=%p natural=%dx%d sizePropertiesWritten=0 positioned=%u properties={%u,%u,%u} attached=%u hideAttempted=0 hidden=%u custom=%u",
            static_cast<unsigned>(attempt),
            g_repaint_context.active ? 1U : 0U,
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            fallback_bound ? 1U : 0U,
            reusable_bitmap ? 1U : 0U,
            data_created ? 1U : 0U,
            pixels_written ? 1U : 0U,
            bitmap_created ? 1U : 0U,
            static_cast<unsigned>(bitmap_view.flags),
            bitmap_view.native_display_vtable,
            natural_size_used ? kProbeBitmapWidth : 0,
            natural_size_used ? kProbeBitmapHeight : 0,
            positioned ? 1U : 0U,
            bitmap_property_set ? 1U : 0U,
            data_property_set ? 1U : 0U,
            authored_property_set ? 1U : 0U,
            attached ? 1U : 0U,
            authored_hidden ? 1U : 0U,
            custom_attached ? 1U : 0U);
        log_line(
            "bitmapAttach[%u] seh={fallback:0x%08lX parent:0x%08lX create:0x%08lX presentation:0x%08lX attach:0x%08lX rollback:0x%08lX} rollback={detached:%u hidden:%u visible:%u cleared:%u retained:%u} final=%u",
            static_cast<unsigned>(attempt),
            fallback_seh,
            parent_seh,
            create_seh,
            presentation_seh,
            attach_seh,
            rollback_seh,
            rollback_detached ? 1U : 0U,
            rollback_hidden ? 1U : 0U,
            rollback_visible ? 1U : 0U,
            properties_cleared ? 1U : 0U,
            rollback_retained ? 1U : 0U,
            result.bound ? 1U : 0U);
    }

    const bool cleanup_safe = fallback_seh == 0 && parent_seh == 0 &&
        create_seh == 0 && presentation_seh == 0 && attach_seh == 0 &&
        rollback_seh == 0;
    if (cleanup_safe) {
        destroy_local_scaleform_value(
            undefined_wrapper,
            undefined_constructed,
            "bitmap rollback undefined value");
        destroy_local_scaleform_value(
            created_bitmap_wrapper,
            created_bitmap_constructed,
            "created Bitmap");
        destroy_local_scaleform_value(
            created_data_wrapper,
            created_data_constructed,
            "created BitmapData");
        destroy_local_scaleform_value(
            existing_data_wrapper,
            existing_data_constructed,
            "existing BitmapData");
        destroy_local_scaleform_value(
            existing_bitmap_wrapper,
            existing_bitmap_constructed,
            "existing Bitmap");
        destroy_local_scaleform_value(
            parent_wrapper,
            parent_constructed,
            "Bitmap parent");
    } else {
        log_failure(
            "bitmapAttach[%u] leaked temporary wrappers after a guarded native fault",
            static_cast<unsigned>(attempt));
    }
    return result;
}
#endif

Rgba8ShapeAttachProbeResult probe_dynamic_rgba8_shape_attachment(
    std::uint32_t attempt,
    void* target_value,
    void* movie_root,
    void* create_object_slot,
    ImageBindFn image_binder,
    void** resource,
    const std::array<float, 4>& rectangle,
    bool require_reusable_shape,
    bool skip_same_asset_bind,
    bool clear_before_bind) noexcept {
    Rgba8ShapeAttachProbeResult result{};
    const std::uint8_t property_bank = repaint_icon_property_bank();
    const char* const injected_property =
        injected_icon_property(property_bank);
    const char* const authored_property =
        authored_icon_property(property_bank);
    // Only the fixed, forward-proven Graphics canvas is used for custom RGBA
    // content; retain the authored rectangle parameter for call-site symmetry.
    (void)rectangle;

    void* resource_value{};
    const bool resource_read = guarded_read(resource, resource_value) &&
        resource_value;
    if (resource_read) {
        result.reference_before_read = guarded_read(
            static_cast<const std::byte*>(resource_value) + 0x08,
            result.reference_before);
    }
    const bool exact_incoming_reference =
        result.reference_before_read && result.reference_before == 3U;

    void* const arena_storage = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Rgba8ShapeWrapperArena));
    auto* const wrappers = arena_storage
        ? ::new (arena_storage) Rgba8ShapeWrapperArena{}
        : nullptr;
    if (!wrappers) {
        // Without stable wrapper storage there is no safe Shape probe. Balance
        // only a positively identified binder-local reference; on any other
        // count, leave ownership untouched and fail closed.
        if (resource_read && exact_incoming_reference) {
            result.resource_consumed =
                guarded_release_scaleform_image_resource(
                    reinterpret_cast<ScaleformImageResourceReleaseFn>(
                        g_game.base +
                        kScaleformImageResourceRelease271Rva),
                    resource_value,
                    result.consume_seh);
            result.reference_after_read = guarded_read(
                static_cast<const std::byte*>(resource_value) + 0x08,
                result.reference_after);
            result.exact_reference_transition =
                result.resource_consumed && result.reference_after_read &&
                result.reference_after == 2U;
        }
        log_failure(
            "rgba8Shape[%u] wrapper arena allocation failed; consumed=%u seh=0x%08lX refs=%u/%u->%u/%u exact=%u",
            static_cast<unsigned>(attempt),
            result.resource_consumed ? 1U : 0U,
            result.consume_seh,
            result.reference_before_read ? 1U : 0U,
            static_cast<unsigned>(result.reference_before),
            result.reference_after_read ? 1U : 0U,
            static_cast<unsigned>(result.reference_after),
            result.exact_reference_transition ? 1U : 0U);
        return result;
    }

    // Resolve the same ActionScript parent that owns the already-proven vector
    // overlay. The authored timeline Shape cannot accept an image fill on this
    // movie (its native drawing-context slot is null), so the RGBA resource is
    // never forwarded to that object.
    auto& parent_wrapper = wrappers->parent.value;
    const char* parent_name = "parent";
    bool parent_constructed{};
    unsigned long parent_seh{};
    if (target_value && g_get_scaleform_member) {
#if defined(_MSC_VER)
        __try {
#endif
            parent_constructed = g_get_scaleform_member(
                target_value, parent_wrapper.data(), &parent_name) ==
                parent_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            parent_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    DisplayValueView parent_view{};
    DisplayValueView target_view{};
    const bool exact_parent = parent_seh == 0 && parent_constructed &&
        inspect_display_value(parent_wrapper.data(), parent_view) &&
        exact_as3_display_bridge(parent_view);
    const bool exact_target = inspect_display_value(target_value, target_view) &&
        exact_as3_display_bridge(target_view) &&
        validated_authored_icon_target_vtable(
            target_view.native_display_vtable);

    auto& existing_wrapper = wrappers->existing.value;
    bool existing_constructed{};
    unsigned long existing_get_seh{};
    if (exact_parent && g_get_scaleform_member) {
        const char* property_name = injected_property;
#if defined(_MSC_VER)
        __try {
#endif
            existing_constructed = g_get_scaleform_member(
                parent_wrapper.data(), existing_wrapper.data(),
                &property_name) == existing_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            existing_get_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool tracked_cell{};
    if (g_repaint_context.active &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.physical < active_physical_tab_count() &&
        g_repaint_context.layer != TabLayer::none) {
        const std::size_t bit_index = g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0);
        const AtomicTabHostMask& host_ledger =
            repaint_stages_fresh_host(bit_index)
            ? g_staged_injected_icon_hosts
            : g_injected_icon_hosts;
        tracked_cell = (host_ledger.load(std::memory_order_acquire) &
            (TabHostMask{1} << bit_index)) != 0;
    }
    DisplayValueView existing_view{};
    const bool reusable_shape = existing_get_seh == 0 && tracked_cell &&
        existing_constructed &&
        inspect_display_value(existing_wrapper.data(), existing_view) &&
        exact_as3_display_bridge(existing_view) &&
        existing_view.native_display_vtable ==
            g_game.base + kDynamicShapeVtable271Rva;

    auto& created_wrapper = wrappers->created.value;
    bool created_constructed{};
    void* constructor_result{};
    unsigned long constructor_seh{};
    if (exact_parent && exact_target && !reusable_shape &&
        !require_reusable_shape &&
        g_construct_scaleform_value) {
#if defined(_MSC_VER)
        __try {
#endif
            constructor_result =
                g_construct_scaleform_value(created_wrapper.data());
            created_constructed = true;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            constructor_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    unsigned long create_seh{};
    if (created_constructed && movie_root && create_object_slot) {
#if defined(_MSC_VER)
        __try {
#endif
            reinterpret_cast<MovieCreateObjectFn>(create_object_slot)(
                movie_root,
                created_wrapper.data() + 0x08,
                "flash.display.Shape",
                nullptr,
                0);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            create_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    DisplayValueView created_view{};
    const bool created_shape = created_constructed && create_seh == 0 &&
        constructor_result == created_wrapper.data() &&
        inspect_display_value(created_wrapper.data(), created_view) &&
        exact_as3_display_bridge(created_view) &&
        created_view.native_display_vtable ==
            g_game.base + kDynamicShapeVtable271Rva;
    void* const shape_wrapper = reusable_shape
        ? static_cast<void*>(existing_wrapper.data())
        : static_cast<void*>(created_wrapper.data());
    const DisplayValueView& shape_view = reusable_shape
        ? existing_view
        : created_view;
    const bool exact_dynamic_shape = reusable_shape || created_shape;
    result.reused_existing_shape = reusable_shape;
    result.created_new_shape = created_shape;
    result.pre_bind_graphics_clear_required = clear_before_bind;

    // Scaleform's image binder succeeds when called a second time on the same
    // Shape, but runtime proof showed that its cached drawing command can then
    // render as an untextured white quad. Clear the existing Graphics command
    // list before the one allowed asset-0 -> asset-1 rebind. Graphics.clear()
    // is already proven by the detach path and also releases the old RawImage
    // fill synchronously when the renderer permits it.
    auto& graphics_wrapper = wrappers->graphics.value;
    bool graphics_constructed{};
    unsigned long graphics_get_seh{};
    ObjectValueView graphics_view{};
    bool exact_graphics{};
    Rgba8ReferenceSnapshot pre_clear_old_refs{};
    Rgba8ReferenceSnapshot post_clear_old_refs{};
    if (result.pre_bind_graphics_clear_required && reusable_shape &&
        g_get_scaleform_member) {
        const char* graphics_name = "graphics";
#if defined(_MSC_VER)
        __try {
#endif
            graphics_constructed = g_get_scaleform_member(
                shape_wrapper,
                graphics_wrapper.data(),
                &graphics_name) == graphics_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            graphics_get_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
        exact_graphics = graphics_get_seh == 0 && graphics_constructed &&
            inspect_object_value(graphics_wrapper.data(), graphics_view) &&
            exact_as3_object_bridge(graphics_view);
        if (exact_graphics) {
            pre_clear_old_refs = snapshot_rgba8_references(0);
            result.pre_bind_old_raw_before_read =
                pre_clear_old_refs.raw_read;
            result.pre_bind_old_raw_before = pre_clear_old_refs.raw;
            result.pre_bind_graphics_clear_attempted = true;
            result.pre_bind_graphics_cleared = invoke_object_method(
                graphics_view,
                "clear",
                nullptr,
                0,
                result.graphics_clear_seh);
            post_clear_old_refs = snapshot_rgba8_references(0);
            result.pre_bind_old_raw_after_read = post_clear_old_refs.raw_read;
            result.pre_bind_old_raw_after = post_clear_old_refs.raw;
            result.pre_bind_old_raw_released_exact =
                pre_clear_old_refs.resource_read &&
                pre_clear_old_refs.resource == 1U &&
                pre_clear_old_refs.raw_read &&
                post_clear_old_refs.resource_read &&
                post_clear_old_refs.resource == 1U &&
                post_clear_old_refs.raw_read &&
                pre_clear_old_refs.raw > 0U &&
                post_clear_old_refs.raw == pre_clear_old_refs.raw - 1U;
        }
    }
    const bool pre_bind_graphics_ready =
        !result.pre_bind_graphics_clear_required ||
        (result.pre_bind_graphics_clear_attempted &&
            result.pre_bind_graphics_cleared &&
            result.graphics_clear_seh == 0 &&
            result.pre_bind_old_raw_released_exact);
    if (attempt <= kMaximumShapeAttachLogs &&
        result.pre_bind_graphics_clear_required) {
        log_line(
            "rgba8Shape[%u] preBindClear={required:1 graphicsConstructed:%u graphicsExact:%u attempted:%u cleared:%u seh:0x%08lX oldResource:%u/%u->%u/%u oldRaw:%u/%u->%u/%u releasedExact:%u binderAllowed:%u}",
            static_cast<unsigned>(attempt),
            graphics_constructed ? 1U : 0U,
            exact_graphics ? 1U : 0U,
            result.pre_bind_graphics_clear_attempted ? 1U : 0U,
            result.pre_bind_graphics_cleared ? 1U : 0U,
            result.graphics_clear_seh,
            pre_clear_old_refs.resource_read ? 1U : 0U,
            static_cast<unsigned>(pre_clear_old_refs.resource),
            post_clear_old_refs.resource_read ? 1U : 0U,
            static_cast<unsigned>(post_clear_old_refs.resource),
            result.pre_bind_old_raw_before_read ? 1U : 0U,
            static_cast<unsigned>(result.pre_bind_old_raw_before),
            result.pre_bind_old_raw_after_read ? 1U : 0U,
            static_cast<unsigned>(result.pre_bind_old_raw_after),
            result.pre_bind_old_raw_released_exact ? 1U : 0U,
            pre_bind_graphics_ready ? 1U : 0U);
    }
    constexpr std::array<float, 4> kFixedCanvasRectangle{
        static_cast<float>(kCrossCanvasLeft),
        static_cast<float>(kCrossCanvasTop),
        static_cast<float>(kCrossCanvasLeft + kCrossCanvasWidth),
        static_cast<float>(kCrossCanvasTop + kCrossCanvasHeight),
    };

    // The presenter owns one binder-local reference in addition to the lookup
    // reference. Consume that reference exactly once on every branch. The
    // normal path gives it to the unchanged image binder on a dynamic Shape;
    // a pre-bind validation failure balances it directly with Release. Never
    // retry the resource on the authored target after either operation.
    bool release_completed{};
    if (skip_same_asset_bind && reusable_shape && resource_read &&
        exact_incoming_reference) {
        release_completed = guarded_release_scaleform_image_resource(
            reinterpret_cast<ScaleformImageResourceReleaseFn>(
                g_game.base + kScaleformImageResourceRelease271Rva),
            resource_value,
            result.consume_seh);
        result.resource_consumed = release_completed;
        result.same_asset_bind_skipped = release_completed;
    } else if (exact_dynamic_shape && pre_bind_graphics_ready && image_binder &&
        resource_read && exact_incoming_reference) {
        result.resource_consumed = true;
        result.raster_bind_attempted = true;
#if defined(_MSC_VER)
        __try {
#endif
            result.raster_bound = image_binder(
                shape_wrapper, resource, kFixedCanvasRectangle.data());
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.consume_seh =
                static_cast<unsigned long>(GetExceptionCode());
            result.raster_bound = false;
        }
#endif
    } else if (resource_read && exact_incoming_reference) {
        release_completed = guarded_release_scaleform_image_resource(
            reinterpret_cast<ScaleformImageResourceReleaseFn>(
                g_game.base + kScaleformImageResourceRelease271Rva),
            resource_value,
            result.consume_seh);
        result.resource_consumed = release_completed;
    }

    if (resource_read) {
        result.reference_after_read = guarded_read(
            static_cast<const std::byte*>(resource_value) + 0x08,
            result.reference_after);
    }
    result.exact_reference_transition =
        result.reference_before_read && result.reference_after_read &&
        result.reference_before == 3U && result.reference_after == 2U;
    const bool raster_ready =
        ((result.raster_bind_attempted && result.raster_bound) ||
            result.same_asset_bind_skipped) &&
        result.consume_seh == 0 && result.exact_reference_transition;

    // A false return from the dynamic-Shape binder still has a proven 3 -> 2
    // ownership transition. In that narrow case, paint the known-good vector
    // cross into this same Shape so the active front/back layer never flashes
    // the unrelated Graphics icon while publication is disabled for later
    // repaints. No second bind or resource operation is performed.
    const bool can_paint_fallback = result.raster_bind_attempted &&
        !result.raster_bound && result.consume_seh == 0 &&
        result.exact_reference_transition && exact_dynamic_shape;
    if (can_paint_fallback && !graphics_constructed &&
        g_get_scaleform_member) {
        const char* graphics_name = "graphics";
#if defined(_MSC_VER)
        __try {
#endif
            graphics_constructed = g_get_scaleform_member(
                shape_wrapper,
                graphics_wrapper.data(),
                &graphics_name) == graphics_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            graphics_get_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    if (can_paint_fallback && !exact_graphics) {
        exact_graphics = graphics_get_seh == 0 && graphics_constructed &&
            inspect_object_value(graphics_wrapper.data(), graphics_view) &&
            exact_as3_object_bridge(graphics_view);
    }
    constexpr double local_width = kCrossCanvasWidth;
    constexpr double local_height = kCrossCanvasHeight;
    constexpr bool sane_extents = local_width > 0.0 &&
        local_width <= 256.0 && local_height > 0.0 &&
        local_height <= 256.0;
    unsigned long paint_seh{};
    std::uint32_t painted_rectangles{};
    if (exact_graphics && sane_extents && can_paint_fallback) {
        if (draw_graphics_rectangle(
                graphics_view,
                0x42E8F5U,
                0.92,
                local_width * 0.42,
                local_height * 0.08,
                local_width * 0.16,
                local_height * 0.84,
                paint_seh)) {
            ++painted_rectangles;
        }
        if (painted_rectangles == 1 && draw_graphics_rectangle(
                graphics_view,
                0xFFC857U,
                0.92,
                local_width * 0.12,
                local_height * 0.42,
                local_width * 0.76,
                local_height * 0.16,
                paint_seh)) {
            ++painted_rectangles;
        }
        if (painted_rectangles == 2 && draw_graphics_rectangle(
                graphics_view,
                0xD95CFFU,
                0.96,
                local_width * 0.36,
                local_height * 0.36,
                local_width * 0.28,
                local_height * 0.28,
                paint_seh)) {
            ++painted_rectangles;
        }
    }
    const bool vector_fallback_painted =
        painted_rectangles == 3 && paint_seh == 0;
    const bool drawable_shape = exact_dynamic_shape &&
        (raster_ready || vector_fallback_painted);

    bool icon_property_set{};
    bool authored_property_set{};
    unsigned long property_set_seh{};
    if (exact_parent && exact_target && drawable_shape) {
#if defined(_MSC_VER)
        __try {
#endif
            const auto set_member = reinterpret_cast<ValueSetMemberFn>(
                parent_view.set_member_slot);
            icon_property_set = set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                injected_property,
                static_cast<const std::byte*>(shape_wrapper) + 0x08,
                true);
            authored_property_set = icon_property_set && set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                authored_property,
                static_cast<const std::byte*>(target_value) + 0x08,
                true);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            property_set_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    auto& verified_icon_wrapper = wrappers->verified_icon.value;
    auto& verified_authored_wrapper = wrappers->verified_authored.value;
    bool verified_icon_constructed{};
    bool verified_authored_constructed{};
    unsigned long property_verify_seh{};
    if (icon_property_set && authored_property_set &&
        g_get_scaleform_member) {
        const char* icon_property = injected_property;
#if defined(_MSC_VER)
        __try {
#endif
            verified_icon_constructed = g_get_scaleform_member(
                parent_wrapper.data(),
                verified_icon_wrapper.data(),
                &icon_property) == verified_icon_wrapper.data();
            verified_authored_constructed = g_get_scaleform_member(
                parent_wrapper.data(),
                verified_authored_wrapper.data(),
                &authored_property) == verified_authored_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            property_verify_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    DisplayValueView verified_icon_view{};
    DisplayValueView verified_authored_view{};
    const bool properties_verified = property_verify_seh == 0 &&
        verified_icon_constructed && verified_authored_constructed &&
        inspect_display_value(
            verified_icon_wrapper.data(), verified_icon_view) &&
        inspect_display_value(
            verified_authored_wrapper.data(), verified_authored_view) &&
        exact_as3_display_bridge(verified_icon_view) &&
        exact_as3_display_bridge(verified_authored_view) &&
        verified_icon_view.object_payload == shape_view.object_payload &&
        verified_icon_view.native_display_vtable ==
            g_game.base + kDynamicShapeVtable271Rva &&
        verified_authored_view.object_payload == target_view.object_payload;

    // The native binder emits a zero-based image quad and derives only scale
    // from the supplied rectangle's width/height. Both raster and immediate
    // vector fallback therefore use the fixed, forward-proven Graphics canvas
    // and its matching DisplayInfo translation. This deliberately ignores the
    // remapped authored icon's varying reverse-wrap bounds.
    constexpr float position_x = static_cast<float>(kCrossCanvasLeft);
    constexpr float position_y = static_cast<float>(kCrossCanvasTop);
    void* position_result{};
    unsigned long position_seh{};
    if (properties_verified && drawable_shape && g_set_scaleform_position) {
#if defined(_MSC_VER)
        __try {
#endif
            position_result = g_set_scaleform_position(
                shape_wrapper, position_x, position_y);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            position_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    const bool positioned = position_seh == 0 &&
        position_result == shape_wrapper;

    bool shape_visible{};
    unsigned long visible_seh{};
    if (positioned && g_set_scaleform_visible) {
#if defined(_MSC_VER)
        __try {
#endif
            shape_visible = g_set_scaleform_visible(shape_wrapper, 1) ==
                shape_wrapper;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            visible_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool add_child_attempted{};
    bool attached{};
    unsigned long attach_seh{};
    if (shape_visible) {
#if defined(_MSC_VER)
        __try {
#endif
            add_child_attempted = true;
            attached = reinterpret_cast<ValueInvokeFn>(
                parent_view.invoke_slot)(
                parent_view.object_interface,
                parent_view.object_payload,
                nullptr,
                "addChild",
                static_cast<const std::byte*>(shape_wrapper) + 0x08,
                1,
                true);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            attach_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool authored_hidden{};
    unsigned long hide_seh{};
    if (attached && g_set_scaleform_visible) {
#if defined(_MSC_VER)
        __try {
#endif
            authored_hidden = g_set_scaleform_visible(target_value, 0) ==
                target_value;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hide_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    result.custom_visible = drawable_shape && positioned && shape_visible &&
        attached && authored_hidden;
    result.vector_fallback_visible =
        vector_fallback_painted && result.custom_visible;
    result.bound = result.custom_visible;
    const bool attachment_uncertain =
        attach_seh != 0 && add_child_attempted;

    bool rollback_detached{};
    bool rollback_hidden{};
    bool rollback_visible{};
    bool properties_cleared{};
    bool rollback_retained{};
    unsigned long rollback_seh{};
    auto& undefined_wrapper = wrappers->undefined.value;
    bool undefined_constructed{};
    if (!result.custom_visible &&
        (icon_property_set || authored_property_set)) {
        // A faulting Invoke may have attached the child before raising. Keep
        // the parent-owned properties and tracked-host bit until removeChild
        // returns normally; never infer detachment from a missing return value.
        rollback_retained = attached || reusable_shape ||
            attachment_uncertain;
#if defined(_MSC_VER)
        __try {
#endif
            if (attached || reusable_shape || attachment_uncertain) {
                rollback_detached = reinterpret_cast<ValueInvokeFn>(
                    parent_view.invoke_slot)(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    nullptr,
                    "removeChild",
                    static_cast<const std::byte*>(shape_wrapper) + 0x08,
                    1,
                    true);
            } else {
                rollback_detached = true;
            }
            if (rollback_detached) rollback_retained = false;
            if (!rollback_detached && g_set_scaleform_visible) {
                rollback_hidden = g_set_scaleform_visible(
                    shape_wrapper, 0) == shape_wrapper;
            }
            rollback_visible = !authored_hidden ||
                (g_set_scaleform_visible &&
                    g_set_scaleform_visible(target_value, 1) == target_value);
            if (rollback_detached && g_construct_scaleform_value &&
                g_construct_scaleform_value(undefined_wrapper.data()) ==
                    undefined_wrapper.data()) {
                undefined_constructed = true;
                const auto set_member = reinterpret_cast<ValueSetMemberFn>(
                    parent_view.set_member_slot);
                const bool icon_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    injected_property,
                    undefined_wrapper.data() + 0x08,
                    true);
                const bool authored_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    authored_property,
                    undefined_wrapper.data() + 0x08,
                    true);
                properties_cleared = icon_cleared && authored_cleared;
            }
            if (!rollback_detached) rollback_retained = true;
            // removeChild proves only display-list detachment. Either private
            // parent property may still own the Shape (and therefore its
            // RawImage fill) when a SetMember fails or when its success has
            // not been independently read back as Undefined. Retain the host
            // ledger conservatively so a later exact cleanup/epoch baseline
            // can account for that ownership; never infer release here.
            if (icon_property_set || authored_property_set) {
                rollback_retained = true;
            }
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            rollback_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    if ((result.custom_visible || rollback_retained) &&
        g_repaint_context.active &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.physical < active_physical_tab_count() &&
        g_repaint_context.layer != TabLayer::none) {
        const std::size_t bit_index = g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0);
        const TabHostMask mask = TabHostMask{1} << bit_index;
        AtomicTabHostMask& host_ledger = repaint_stages_fresh_host(bit_index)
            ? g_staged_injected_icon_hosts
            : g_injected_icon_hosts;
        const TabHostMask prior = host_ledger.fetch_or(
            mask, std::memory_order_acq_rel);
        (void)prior;
        result.host_retained = true;
    }

    if (attempt <= kMaximumShapeAttachLogs) {
        log_line(
            "rgba8Shape[%u] context={layer:%u physical:%zu} parent=%u target=%u tracked=%u reused=%u requiredReuse=%u created=%u resource={read:%u consumed:%u bindAttempted:%u rasterBound:%u sameAssetSkipped:%u release:%u seh:0x%08lX refs:%u/%u->%u/%u exact:%u}",
            static_cast<unsigned>(attempt),
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            exact_parent ? 1U : 0U,
            exact_target ? 1U : 0U,
            tracked_cell ? 1U : 0U,
            reusable_shape ? 1U : 0U,
            require_reusable_shape ? 1U : 0U,
            created_shape ? 1U : 0U,
            resource_read ? 1U : 0U,
            result.resource_consumed ? 1U : 0U,
            result.raster_bind_attempted ? 1U : 0U,
            result.raster_bound ? 1U : 0U,
            result.same_asset_bind_skipped ? 1U : 0U,
            release_completed ? 1U : 0U,
            result.consume_seh,
            result.reference_before_read ? 1U : 0U,
            static_cast<unsigned>(result.reference_before),
            result.reference_after_read ? 1U : 0U,
            static_cast<unsigned>(result.reference_after),
            result.exact_reference_transition ? 1U : 0U);
        log_line(
            "rgba8Shape[%u] fallback={eligible:%u graphics:%u rects:%u visible:%u seh:0x%08lX} properties={set:%u/%u verified:%u} presentation={positioned:%u at:%.3f,%.3f visible:%u addAttempted:%u attached:%u attachUncertain:%u authoredHidden:%u customVisible:%u} rollback={detached:%u hidden:%u visible:%u cleared:%u retained:%u seh:0x%08lX}",
            static_cast<unsigned>(attempt),
            can_paint_fallback ? 1U : 0U,
            exact_graphics ? 1U : 0U,
            static_cast<unsigned>(painted_rectangles),
            result.vector_fallback_visible ? 1U : 0U,
            paint_seh,
            icon_property_set ? 1U : 0U,
            authored_property_set ? 1U : 0U,
            properties_verified ? 1U : 0U,
            positioned ? 1U : 0U,
            static_cast<double>(position_x),
            static_cast<double>(position_y),
            shape_visible ? 1U : 0U,
            add_child_attempted ? 1U : 0U,
            attached ? 1U : 0U,
            attachment_uncertain ? 1U : 0U,
            authored_hidden ? 1U : 0U,
            result.custom_visible ? 1U : 0U,
            rollback_detached ? 1U : 0U,
            rollback_hidden ? 1U : 0U,
            rollback_visible ? 1U : 0U,
            properties_cleared ? 1U : 0U,
            rollback_retained ? 1U : 0U,
            rollback_seh);
    }

    const bool cleanup_safe = parent_seh == 0 && existing_get_seh == 0 &&
        constructor_seh == 0 && create_seh == 0 &&
        result.consume_seh == 0 && graphics_get_seh == 0 &&
        result.graphics_clear_seh == 0 && paint_seh == 0 &&
        property_set_seh == 0 && property_verify_seh == 0 &&
        position_seh == 0 && visible_seh == 0 && attach_seh == 0 &&
        hide_seh == 0 && rollback_seh == 0;
    bool wrapper_destruction_exact = cleanup_safe;
    if (cleanup_safe) {
        wrapper_destruction_exact = destroy_local_scaleform_value(
            undefined_wrapper,
            undefined_constructed,
            "RGBA8 rollback undefined value") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            verified_authored_wrapper,
            verified_authored_constructed,
            "RGBA8 verified authored icon") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            verified_icon_wrapper,
            verified_icon_constructed,
            "RGBA8 verified injected icon") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            graphics_wrapper,
            graphics_constructed,
            "RGBA8 fallback graphics") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            created_wrapper,
            created_constructed,
            "RGBA8 created Shape") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            existing_wrapper,
            existing_constructed,
            "RGBA8 existing Shape") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            parent_wrapper,
            parent_constructed,
            "RGBA8 Shape parent") &&
            wrapper_destruction_exact;
    }
    const bool wrappers_released = !undefined_constructed &&
        !verified_authored_constructed && !verified_icon_constructed &&
        !graphics_constructed && !created_constructed &&
        !existing_constructed && !parent_constructed;
    result.cleanup_exact = cleanup_safe && wrapper_destruction_exact &&
        wrappers_released;
    if (result.cleanup_exact) {
        wrappers->~Rgba8ShapeWrapperArena();
        (void)HeapFree(GetProcessHeap(), 0, wrappers);
    } else {
        log_failure(
            "rgba8Shape[%u] retained heap wrapper arena after guarded fault/cleanup uncertainty (cleanupSafe=%u wrappersReleased=%u)",
            static_cast<unsigned>(attempt),
            cleanup_safe ? 1U : 0U,
            wrappers_released ? 1U : 0U);
    }
    if (attempt <= kMaximumShapeAttachLogs) {
        log_line(
            "rgba8Shape[%u] cleanup={preconditions:%u destructors:%u wrappersReleased:%u exact:%u}",
            static_cast<unsigned>(attempt),
            cleanup_safe ? 1U : 0U,
            wrapper_destruction_exact ? 1U : 0U,
            wrappers_released ? 1U : 0U,
            result.cleanup_exact ? 1U : 0U);
    }
    return result;
}

ShapeAttachProbeResult probe_dynamic_shape_attachment(
    std::uint32_t attempt,
    VectorIconKind icon_kind,
    bool transparent_cross,
    void* target_value,
    void* movie_root,
    void* create_object_slot,
    ImageBindFn image_binder,
    void** resource,
    const std::array<float, 4>& rectangle) noexcept {
    ShapeAttachProbeResult result{};
    const std::uint8_t property_bank = repaint_icon_property_bank();
    const char* const injected_property =
        injected_icon_property(property_bank);
    const char* const authored_property =
        authored_icon_property(property_bank);

    // The aliased native resource belongs to this binder call. Consume it on
    // the authored target first so every failure below can safely leave the
    // normal Graphics icon visible. The custom vector Shape is an overlay; it
    // never owns or forwards this native renderer resource.
    bool fallback_bound{};
    unsigned long bind_seh{};
    if (image_binder && resource) {
        result.resource_consumed = true;
#if defined(_MSC_VER)
        __try {
#endif
            fallback_bound = image_binder(
                target_value, resource, rectangle.data());
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            bind_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    result.bound = fallback_bound;
    if (!result.resource_consumed || bind_seh != 0) {
        if (attempt <= kMaximumShapeAttachLogs) {
            log_line(
                "shapeAttach[%u] kind=%u fallbackBound=%u resourceConsumed=%u bindSeh=0x%08lX customSkipped=1",
                static_cast<unsigned>(attempt),
                static_cast<unsigned>(icon_kind),
                fallback_bound ? 1U : 0U,
                result.resource_consumed ? 1U : 0U,
                bind_seh);
        }
        return result;
    }

    // Resolve target.parent through the game's tracked wrapper. All persistent
    // ownership stays inside that ActionScript parent: no GFx wrapper or raw
    // movie object is retained after this synchronous repaint call.
    alignas(16) std::array<std::byte, 0x38> parent_wrapper{};
    const char* parent_name = "parent";
    void* parent_result{};
    bool parent_constructed{};
    unsigned long parent_seh{};
    if (target_value && g_get_scaleform_member) {
#if defined(_MSC_VER)
        __try {
#endif
            parent_result = g_get_scaleform_member(
                target_value, parent_wrapper.data(), &parent_name);
            parent_constructed = parent_result == parent_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            parent_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    DisplayValueView parent_view{};
    const bool exact_parent = parent_seh == 0 && parent_constructed &&
        inspect_display_value(parent_wrapper.data(), parent_view) &&
        exact_as3_display_bridge(parent_view);

    DisplayValueView target_view{};
    const bool exact_target = inspect_display_value(
            target_value, target_view) &&
        exact_as3_display_bridge(target_view) &&
        validated_authored_icon_target_vtable(
            target_view.native_display_vtable);

    // Reuse a child already owned through the reserved parent property if the
    // same cell is repainted before a viewport remap. GetMember constructs its
    // destination, including the undefined-value case.
    alignas(16) std::array<std::byte, 0x38> existing_wrapper{};
    bool existing_constructed{};
    void* existing_result{};
    if (exact_parent && g_get_scaleform_member) {
        const char* property_name = injected_property;
#if defined(_MSC_VER)
        __try {
#endif
            existing_result = g_get_scaleform_member(
                parent_wrapper.data(), existing_wrapper.data(), &property_name);
            existing_constructed =
                existing_result == existing_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            existing_result = nullptr;
        }
#endif
    }
    DisplayValueView existing_view{};
    bool tracked_cell{};
    if (g_repaint_context.active &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.physical < active_physical_tab_count() &&
        g_repaint_context.layer != TabLayer::none) {
        const std::size_t bit_index = g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0);
        const AtomicTabHostMask& host_ledger =
            repaint_stages_fresh_host(bit_index)
            ? g_staged_injected_icon_hosts
            : g_injected_icon_hosts;
        tracked_cell = (host_ledger.load(std::memory_order_acquire) &
            (TabHostMask{1} << bit_index)) != 0;
    }
    // A failed parent-property clear can leave a dormant reference after the
    // Shape was successfully detached. Reuse only objects that the runtime
    // still tracks as attached to this exact cell; otherwise create a fresh
    // Shape and overwrite the stale property.
    const bool reusable_shape = tracked_cell && existing_constructed &&
        inspect_display_value(existing_wrapper.data(), existing_view) &&
        exact_as3_display_bridge(existing_view) &&
        existing_view.native_display_vtable ==
            g_game.base + kDynamicShapeVtable271Rva;

    // A CSScaleformValue is a 0x38-byte tracked wrapper whose public
    // GFx::Value begins at +0x08. Create a Shape only when the parent does not
    // already own our validated child.
    alignas(16) std::array<std::byte, 0x38> created_wrapper{};
    bool wrapper_constructed{};
    void* constructor_result{};
    unsigned long constructor_seh{};
    if (exact_parent && exact_target && !reusable_shape &&
        g_construct_scaleform_value) {
#if defined(_MSC_VER)
        __try {
#endif
            constructor_result =
                g_construct_scaleform_value(created_wrapper.data());
            wrapper_constructed = true;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            constructor_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    unsigned long create_seh{};
    if (wrapper_constructed && movie_root && create_object_slot) {
#if defined(_MSC_VER)
        __try {
#endif
            reinterpret_cast<MovieCreateObjectFn>(create_object_slot)(
                movie_root,
                created_wrapper.data() + 0x08,
                "flash.display.Shape",
                nullptr,
                0);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            create_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    DisplayValueView created_view{};
    const bool created_shape = wrapper_constructed && create_seh == 0 &&
        constructor_result == created_wrapper.data() &&
        inspect_display_value(created_wrapper.data(), created_view) &&
        exact_as3_display_bridge(created_view) &&
        created_view.native_display_vtable ==
            g_game.base + kDynamicShapeVtable271Rva;
    void* const shape_wrapper = reusable_shape
        ? static_cast<void*>(existing_wrapper.data())
        : static_cast<void*>(created_wrapper.data());
    const DisplayValueView& shape_view = reusable_shape
        ? existing_view
        : created_view;
    const bool exact_dynamic_shape = reusable_shape || created_shape;
    result.reused_existing_shape = reusable_shape;
    result.created_new_shape = created_shape;

    // Paint only a newly-created Shape. A reused Shape is already retained by
    // the GFX parent and already contains this vector probe. Calling the public
    // ActionScript Graphics methods is important: they perform Scaleform's
    // own twip conversion and display invalidation without creating a renderer
    // texture or entering the private DrawingContext ABI.
    alignas(16) std::array<std::byte, 0x38> graphics_wrapper{};
    bool graphics_constructed{};
    void* graphics_result{};
    unsigned long graphics_get_seh{};
    if (created_shape && g_get_scaleform_member) {
        const char* graphics_name = "graphics";
#if defined(_MSC_VER)
        __try {
#endif
            graphics_result = g_get_scaleform_member(
                created_wrapper.data(),
                graphics_wrapper.data(),
                &graphics_name);
            graphics_constructed =
                graphics_result == graphics_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            graphics_get_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    ObjectValueView graphics_view{};
    const bool exact_graphics = graphics_get_seh == 0 &&
        graphics_constructed &&
        inspect_object_value(graphics_wrapper.data(), graphics_view) &&
        exact_as3_object_bridge(graphics_view);
    const double local_width =
        static_cast<double>(rectangle[2] - rectangle[0]);
    const double local_height =
        static_cast<double>(rectangle[3] - rectangle[1]);
    const bool sane_extents = local_width > 0.0 && local_width <= 256.0 &&
        local_height > 0.0 && local_height <= 256.0;
    const double overlay_left = icon_kind == VectorIconKind::cross
        ? kCrossCanvasLeft
        : static_cast<double>(rectangle[0]);
    const double overlay_top = icon_kind == VectorIconKind::cross
        ? kCrossCanvasTop
        : static_cast<double>(rectangle[1]);
    const double overlay_width = icon_kind == VectorIconKind::cross
        ? kCrossCanvasWidth
        : local_width;
    const double overlay_height = icon_kind == VectorIconKind::cross
        ? kCrossCanvasHeight
        : local_height;

    unsigned long paint_seh{};
    std::uint32_t painted_rectangles{};
    const double primary_cross_alpha = transparent_cross ? 0.0 : 0.92;
    const double center_cross_alpha = transparent_cross ? 0.0 : 0.96;
    if (exact_graphics && sane_extents &&
        icon_kind == VectorIconKind::cross) {
        if (draw_graphics_rectangle(
                graphics_view,
                0x42E8F5U,
                primary_cross_alpha,
                overlay_width * 0.42,
                overlay_height * 0.08,
                overlay_width * 0.16,
                overlay_height * 0.84,
                paint_seh)) {
            ++painted_rectangles;
        }
        if (painted_rectangles == 1 && draw_graphics_rectangle(
                graphics_view,
                0xFFC857U,
                primary_cross_alpha,
                overlay_width * 0.12,
                overlay_height * 0.42,
                overlay_width * 0.76,
                overlay_height * 0.16,
                paint_seh)) {
            ++painted_rectangles;
        }
        if (painted_rectangles == 2 && draw_graphics_rectangle(
                graphics_view,
                0xD95CFFU,
                center_cross_alpha,
                overlay_width * 0.36,
                overlay_height * 0.36,
                overlay_width * 0.28,
                overlay_height * 0.28,
                paint_seh)) {
            ++painted_rectangles;
        }
    } else if (exact_graphics && sane_extents &&
        icon_kind == VectorIconKind::ellipsis) {
        const double dot_size =
            (std::min)(local_width, local_height) * 0.11;
        const double dot_y = local_height * 0.50 - dot_size * 0.50;
        constexpr std::array<double, 3> kDotCenters{0.25, 0.50, 0.75};
        for (double center : kDotCenters) {
            if (!draw_graphics_rectangle(
                    graphics_view,
                    0xC8BE9FU,
                    0.96,
                    local_width * center - dot_size * 0.50,
                    dot_y,
                    dot_size,
                    dot_size,
                    paint_seh)) {
                break;
            }
            ++painted_rectangles;
        }
    }
    const bool painted = reusable_shape ||
        (created_shape && painted_rectangles == 3 && paint_seh == 0);
    const bool exact_drawable_shape = exact_dynamic_shape && painted;

    bool icon_property_set{};
    bool authored_property_set{};
    unsigned long property_set_seh{};
    if (exact_parent && exact_target && exact_drawable_shape) {
#if defined(_MSC_VER)
        __try {
#endif
            const auto set_member = reinterpret_cast<ValueSetMemberFn>(
                parent_view.set_member_slot);
            icon_property_set = set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                injected_property,
                static_cast<const std::byte*>(shape_wrapper) + 0x08,
                true);
            authored_property_set = icon_property_set && set_member(
                parent_view.object_interface,
                parent_view.object_payload,
                authored_property,
                static_cast<const std::byte*>(target_value) + 0x08,
                true);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            property_set_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    alignas(16) std::array<std::byte, 0x38> verified_icon_wrapper{};
    alignas(16) std::array<std::byte, 0x38> verified_authored_wrapper{};
    bool verified_icon_constructed{};
    bool verified_authored_constructed{};
    unsigned long property_verify_seh{};
    if (icon_property_set && authored_property_set &&
        g_get_scaleform_member) {
        const char* icon_property = injected_property;
#if defined(_MSC_VER)
        __try {
#endif
            verified_icon_constructed = g_get_scaleform_member(
                parent_wrapper.data(),
                verified_icon_wrapper.data(),
                &icon_property) == verified_icon_wrapper.data();
            verified_authored_constructed = g_get_scaleform_member(
                parent_wrapper.data(),
                verified_authored_wrapper.data(),
                &authored_property) == verified_authored_wrapper.data();
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            property_verify_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    DisplayValueView verified_icon_view{};
    DisplayValueView verified_authored_view{};
    const bool properties_verified = property_verify_seh == 0 &&
        verified_icon_constructed && verified_authored_constructed &&
        inspect_display_value(
            verified_icon_wrapper.data(), verified_icon_view) &&
        inspect_display_value(
            verified_authored_wrapper.data(), verified_authored_view) &&
        exact_as3_display_bridge(verified_icon_view) &&
        exact_as3_display_bridge(verified_authored_view) &&
        verified_icon_view.object_payload == shape_view.object_payload &&
        verified_icon_view.native_display_vtable ==
            g_game.base + kDynamicShapeVtable271Rva &&
        verified_authored_view.object_payload == target_view.object_payload;

    void* position_result{};
    unsigned long position_seh{};
    if (properties_verified && painted && g_set_scaleform_position) {
#if defined(_MSC_VER)
        __try {
#endif
            position_result = g_set_scaleform_position(
                shape_wrapper,
                static_cast<float>(overlay_left),
                static_cast<float>(overlay_top));
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            position_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }
    const bool positioned = position_seh == 0 &&
        position_result == shape_wrapper;

    bool shape_visible{};
    unsigned long shape_visible_seh{};
    if (painted && positioned && g_set_scaleform_visible) {
#if defined(_MSC_VER)
        __try {
#endif
            shape_visible = g_set_scaleform_visible(shape_wrapper, 1) ==
                shape_wrapper;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            shape_visible_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool attached{};
    unsigned long attach_seh{};
    if (painted && positioned && shape_visible) {
#if defined(_MSC_VER)
        __try {
#endif
            attached = reinterpret_cast<ValueInvokeFn>(
                parent_view.invoke_slot)(
                parent_view.object_interface,
                parent_view.object_payload,
                nullptr,
                "addChild",
                static_cast<const std::byte*>(shape_wrapper) + 0x08,
                1,
                true);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            attach_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    bool authored_hidden{};
    unsigned long hide_seh{};
    if (painted && attached && g_set_scaleform_visible) {
#if defined(_MSC_VER)
        __try {
#endif
            authored_hidden = g_set_scaleform_visible(target_value, 0) ==
                target_value;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hide_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    const bool custom_visible =
        painted && positioned && shape_visible && attached && authored_hidden;
    result.custom_visible = custom_visible;
    result.bound = fallback_bound || custom_visible;

    // If any post-registration step failed, detach the child if necessary,
    // restore the native icon, and clear both parent-owned references. A fresh
    // undefined tracked value is the safest SetMember source for clearing.
    bool rollback_detached{};
    bool rollback_hidden{};
    bool rollback_visible{};
    bool properties_cleared{};
    bool rollback_retained{};
    unsigned long rollback_seh{};
    alignas(16) std::array<std::byte, 0x38> undefined_wrapper{};
    bool undefined_constructed{};
    if (!custom_visible && (icon_property_set || authored_property_set)) {
#if defined(_MSC_VER)
        __try {
#endif
            if (attached || reusable_shape) {
                rollback_detached = reinterpret_cast<ValueInvokeFn>(
                    parent_view.invoke_slot)(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    nullptr,
                    "removeChild",
                    static_cast<const std::byte*>(shape_wrapper) + 0x08,
                    1,
                    true);
            } else {
                rollback_detached = true;
            }
            if (!rollback_detached && g_set_scaleform_visible) {
                rollback_hidden = g_set_scaleform_visible(
                    shape_wrapper, 0) == shape_wrapper;
            }
            rollback_visible = !authored_hidden ||
                (g_set_scaleform_visible &&
                    g_set_scaleform_visible(target_value, 1) == target_value);
            if (rollback_detached && g_construct_scaleform_value &&
                g_construct_scaleform_value(undefined_wrapper.data()) ==
                    undefined_wrapper.data()) {
                undefined_constructed = true;
                const auto set_member = reinterpret_cast<ValueSetMemberFn>(
                    parent_view.set_member_slot);
                const bool icon_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    injected_property,
                    undefined_wrapper.data() + 0x08,
                    true);
                const bool authored_cleared = set_member(
                    parent_view.object_interface,
                    parent_view.object_payload,
                    authored_property,
                    undefined_wrapper.data() + 0x08,
                    true);
                properties_cleared = icon_cleared && authored_cleared;
            }
            rollback_retained = !rollback_detached && rollback_hidden;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            rollback_seh =
                static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    if ((custom_visible || rollback_retained) && g_repaint_context.active &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.physical < active_physical_tab_count() &&
        g_repaint_context.layer != TabLayer::none) {
        const std::size_t bit_index = g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0);
        const TabHostMask mask = TabHostMask{1} << bit_index;
        AtomicTabHostMask& host_ledger = repaint_stages_fresh_host(bit_index)
            ? g_staged_injected_icon_hosts
            : g_injected_icon_hosts;
        host_ledger.fetch_or(mask, std::memory_order_acq_rel);
    }

    if (attempt <= kMaximumShapeAttachLogs) {
        log_line(
            "shapeAttach[%u] kind=%u context=%u layer=%u physical=%zu transparentPlaceholder=%u exactParent=%u exactTarget=%u reused=%u ctor=%u ctorSeh=0x%08lX createSeh=0x%08lX exactDynamic=%u graphics={constructed:%u exact:%u seh:0x%08lX} sourceRect={%.3f,%.3f,%.3f,%.3f sane:%u} overlayRect={%.3f,%.3f,%.3f,%.3f} painted={rects:%u final:%u seh:0x%08lX}",
            static_cast<unsigned>(attempt),
            static_cast<unsigned>(icon_kind),
            g_repaint_context.active ? 1U : 0U,
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            transparent_cross ? 1U : 0U,
            exact_parent ? 1U : 0U,
            exact_target ? 1U : 0U,
            reusable_shape ? 1U : 0U,
            wrapper_constructed ? 1U : 0U,
            constructor_seh,
            create_seh,
            exact_dynamic_shape ? 1U : 0U,
            graphics_constructed ? 1U : 0U,
            exact_graphics ? 1U : 0U,
            graphics_get_seh,
            static_cast<double>(rectangle[0]),
            static_cast<double>(rectangle[1]),
            static_cast<double>(rectangle[2]),
            static_cast<double>(rectangle[3]),
            sane_extents ? 1U : 0U,
            overlay_left,
            overlay_top,
            overlay_left + overlay_width,
            overlay_top + overlay_height,
            static_cast<unsigned>(painted_rectangles),
            painted ? 1U : 0U,
            paint_seh);
        log_line(
            "shapeAttach[%u] exactDrawable=%u propertySet={%u,%u} propertySetSeh=0x%08lX propertyVerified=%u propertyVerifySeh=0x%08lX",
            static_cast<unsigned>(attempt),
            exact_drawable_shape ? 1U : 0U,
            icon_property_set ? 1U : 0U,
            authored_property_set ? 1U : 0U,
            property_set_seh,
            properties_verified ? 1U : 0U,
            property_verify_seh);
        log_line(
            "shapeAttach[%u] positioned=%u position={%.3f,%.3f} positionSeh=0x%08lX shapeVisible=%u shapeVisibleSeh=0x%08lX attached=%u attachSeh=0x%08lX resourceConsumed=%u fallbackBound=%u customVisible=%u bindSeh=0x%08lX authoredHidden=%u hideSeh=0x%08lX rollback={detached:%u hidden:%u visible:%u cleared:%u retained:%u seh:0x%08lX}",
            static_cast<unsigned>(attempt),
            positioned ? 1U : 0U,
            overlay_left,
            overlay_top,
            position_seh,
            shape_visible ? 1U : 0U,
            shape_visible_seh,
            attached ? 1U : 0U,
            attach_seh,
            result.resource_consumed ? 1U : 0U,
            fallback_bound ? 1U : 0U,
            custom_visible ? 1U : 0U,
            bind_seh,
            authored_hidden ? 1U : 0U,
            hide_seh,
            rollback_detached ? 1U : 0U,
            rollback_hidden ? 1U : 0U,
            rollback_visible ? 1U : 0U,
            properties_cleared ? 1U : 0U,
            rollback_retained ? 1U : 0U,
            rollback_seh);
    }

    const bool cleanup_safe = parent_seh == 0 && constructor_seh == 0 &&
        create_seh == 0 && graphics_get_seh == 0 && paint_seh == 0 &&
        property_set_seh == 0 && property_verify_seh == 0 && bind_seh == 0 &&
        position_seh == 0 && shape_visible_seh == 0 && attach_seh == 0 &&
        hide_seh == 0 && rollback_seh == 0;
    bool wrapper_destruction_exact = cleanup_safe;
    if (cleanup_safe) {
        wrapper_destruction_exact = destroy_local_scaleform_value(
            undefined_wrapper,
            undefined_constructed,
            "undefined property value") && wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            verified_authored_wrapper,
            verified_authored_constructed,
            "verified authored icon") && wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            verified_icon_wrapper,
            verified_icon_constructed,
            "verified injected icon") && wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            graphics_wrapper, graphics_constructed, "Shape graphics") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            created_wrapper, wrapper_constructed, "created Shape") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            existing_wrapper, existing_constructed, "existing Shape") &&
            wrapper_destruction_exact;
        wrapper_destruction_exact = destroy_local_scaleform_value(
            parent_wrapper, parent_constructed, "Shape parent") &&
            wrapper_destruction_exact;
    } else {
        log_failure(
            "shapeAttach[%u] leaked temporary wrappers after a guarded native fault",
            static_cast<unsigned>(attempt));
    }
    result.cleanup_exact = wrapper_destruction_exact &&
        !undefined_constructed && !verified_authored_constructed &&
        !verified_icon_constructed && !graphics_constructed &&
        !wrapper_constructed && !existing_constructed &&
        !parent_constructed;
    return result;
}

bool destroy_and_free_heap_scaleform_value(
    void*& wrapper,
    bool& constructed,
    const char* label) noexcept {
    if (!wrapper) return true;
    if (constructed) {
        if (!g_destroy_scaleform_value) {
            log_failure(
                "%s cannot be released because the Scaleform destructor is unavailable",
                label ? label : "heap Scaleform value");
            return false;
        }
        unsigned long destroy_seh{};
#if defined(_MSC_VER)
        __try {
#endif
            g_destroy_scaleform_value(wrapper);
            constructed = false;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            destroy_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
        if (destroy_seh != 0) {
            log_failure(
                "%s retained after cleanup faulted (seh=0x%08lX)",
                label ? label : "heap Scaleform value",
                destroy_seh);
            return false;
        }
    }
    if (!HeapFree(GetProcessHeap(), 0, wrapper)) {
        log_failure(
            "%s retained because HeapFree failed (error=%lu)",
            label ? label : "heap Scaleform value",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }
    wrapper = nullptr;
    return true;
}

// Read-only snapshot of the VM services associated with an already validated
// Exact-build AS3 MovieRoot. The offsets come from the static boundary
// analysis shared by both audited profiles.
// No pointer is dereferenced outside guarded_read, and callers retain only the
// resulting scalar addresses for comparison on a later frame.
BitmapDataVmContextSnapshot read_bitmapdata_vm_context_snapshot(
    void* exact_movie_root) noexcept {
    BitmapDataVmContextSnapshot result{};
    void* vm{};
    result.vm_read = exact_movie_root && guarded_read(
        static_cast<const std::byte*>(exact_movie_root) + 0x88, vm);
    result.vm_address = reinterpret_cast<std::uintptr_t>(vm);
    if (!result.vm_read || !vm) return result;

    void* drawable_image_context{};
    result.drawable_image_context_read = guarded_read(
        static_cast<const std::byte*>(vm) + 0x5470,
        drawable_image_context);
    result.drawable_image_context_address =
        reinterpret_cast<std::uintptr_t>(drawable_image_context);

    void* drawable_image_provider{};
    result.drawable_image_provider_read = guarded_read(
        static_cast<const std::byte*>(vm) + 0x5478,
        drawable_image_provider);
    result.drawable_image_provider_address =
        reinterpret_cast<std::uintptr_t>(drawable_image_provider);
    return result;
}

// This property-lifecycle operation must never run from the image-binding
// callback. Its caller invokes it only after the native Configuration-panel
// frame virtual returns and after freshly validating the parent and Movie.
// It creates one transparent 2x2 BitmapData and roots it in a private property
// on that movie-owned parent. The later single-pixel experiment remains in the
// source but is disabled after it deterministically blackened the renderer.
// This creation operation deliberately performs no pixel writes,
// Bitmap construction, display-list change, explicit dispose, or property
// clear. Any uncertain native failure retains its heap wrapper for the rest of
// the process rather than risking an intrusive-list link into dead storage.
void probe_bitmapdata_movie_property_ownership(
    void* parent_wrapper,
    const DisplayValueView& parent_view,
    void* movie_root,
    void* create_object_slot,
    const DeferredBitmapDataProbe& scheduled,
    std::uint64_t frame_sequence) noexcept {
    constexpr std::int32_t diagnostic_width = 2;
    constexpr std::int32_t diagnostic_height = 2;

    void* data_wrapper = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
    void* verification_wrapper = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
    g_retained_bitmapdata_wrapper = data_wrapper;
    g_retained_bitmapdata_same_frame_wrapper = verification_wrapper;

    bool data_constructed{};
    bool data_created{};
    bool property_set{};
    bool verification_constructed{};
    bool same_frame_identity{};
    unsigned long constructor_seh{};
    unsigned long create_seh{};
    unsigned long set_member_seh{};
    unsigned long get_member_seh{};
    ObjectValueView data_view{};
    ObjectValueView verification_view{};

    if (data_wrapper && verification_wrapper && parent_wrapper &&
        exact_as3_display_bridge(parent_view) && movie_root &&
        create_object_slot && g_construct_scaleform_value &&
        g_get_scaleform_member) {
#if defined(_MSC_VER)
        __try {
#endif
            data_constructed = g_construct_scaleform_value(data_wrapper) ==
                data_wrapper;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            constructor_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    if (data_constructed && constructor_seh == 0) {
        const std::array<RawGfxValue, 4> arguments{
            make_int_value(diagnostic_width),
            make_int_value(diagnostic_height),
            make_bool_value(true),
            make_uint_value(0x00000000U),
        };
#if defined(_MSC_VER)
        __try {
#endif
            reinterpret_cast<MovieCreateObjectFn>(create_object_slot)(
                movie_root,
                static_cast<std::byte*>(data_wrapper) + 0x08,
                "flash.display.BitmapData",
                arguments.data(),
                static_cast<std::uint32_t>(arguments.size()));
            data_created = inspect_object_value(data_wrapper, data_view) &&
                exact_as3_object_bridge(data_view);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            create_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    if (data_created && create_seh == 0) {
#if defined(_MSC_VER)
        __try {
#endif
            property_set = reinterpret_cast<ValueSetMemberFn>(
                parent_view.set_member_slot)(
                parent_view.object_interface,
                parent_view.object_payload,
                kInjectedBitmapDataProperty,
                static_cast<const std::byte*>(data_wrapper) + 0x08,
                true);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            set_member_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    if (property_set && set_member_seh == 0) {
        const char* property_name = kInjectedBitmapDataProperty;
#if defined(_MSC_VER)
        __try {
#endif
            void* const result = g_get_scaleform_member(
                parent_wrapper, verification_wrapper, &property_name);
            verification_constructed = result == verification_wrapper;
            same_frame_identity = verification_constructed &&
                inspect_object_value(
                    verification_wrapper, verification_view) &&
                exact_as3_object_bridge(verification_view) &&
                verification_view.object_payload == data_view.object_payload;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            get_member_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    const bool operation_succeeded = data_created && property_set &&
        same_frame_identity && constructor_seh == 0 && create_seh == 0 &&
        set_member_seh == 0 && get_member_seh == 0;
    log_line(
        "bitmapDataProperty create phase=post-native-composite frame=%llu requested=2x2 wrapper={value:%p aligned16:%u constructed:%u created:%u payload:%p} setMember=%u sameFrame={wrapper:%p constructed:%u payload:%p identity:%u} seh={constructor:0x%08lX create:0x%08lX set:0x%08lX get:0x%08lX} operationSucceeded=%u",
        static_cast<unsigned long long>(frame_sequence),
        data_wrapper,
        data_wrapper &&
                (reinterpret_cast<std::uintptr_t>(data_wrapper) & 0xFU) == 0
            ? 1U
            : 0U,
        data_constructed ? 1U : 0U,
        data_created ? 1U : 0U,
        data_view.object_payload,
        property_set ? 1U : 0U,
        verification_wrapper,
        verification_constructed ? 1U : 0U,
        verification_view.object_payload,
        same_frame_identity ? 1U : 0U,
        constructor_seh,
        create_seh,
        set_member_seh,
        get_member_seh,
        operation_succeeded ? 1U : 0U);

    if (!operation_succeeded) {
        log_failure(
            "BitmapData movie-property operation failed closed; heap wrappers and any established parent property were retained");
        return;
    }

    const BitmapDataVmContextSnapshot vm_context =
        read_bitmapdata_vm_context_snapshot(movie_root);
    log_line(
        "bitmapDataVmContext creation frame=%llu movieRoot=%p vm={read:%u address:%p} drawableImageContext={read:%u address:%p} drawableImageProvider={read:%u address:%p} passiveOnly=1",
        static_cast<unsigned long long>(frame_sequence),
        movie_root,
        vm_context.vm_read ? 1U : 0U,
        reinterpret_cast<void*>(vm_context.vm_address),
        vm_context.drawable_image_context_read ? 1U : 0U,
        reinterpret_cast<void*>(
            vm_context.drawable_image_context_address),
        vm_context.drawable_image_provider_read ? 1U : 0U,
        reinterpret_cast<void*>(
            vm_context.drawable_image_provider_address));

    void* const expected_payload = data_view.object_payload;
    const bool verification_released =
        destroy_and_free_heap_scaleform_value(
            g_retained_bitmapdata_same_frame_wrapper,
            verification_constructed,
            "same-frame BitmapData verification wrapper");
    bool creator_released{};
    if (verification_released) {
        creator_released = destroy_and_free_heap_scaleform_value(
            g_retained_bitmapdata_wrapper,
            data_constructed,
            "created BitmapData wrapper");
    }
    if (!verification_released || !creator_released) {
        log_failure(
            "BitmapData movie-property cleanup failed closed; next-frame verification was not armed");
        return;
    }

    g_bitmapdata_property_verification = {
        BitmapDataPropertyVerificationPhase::pre_mutation_identity,
        scheduled.top,
        scheduled.layer,
        scheduled.physical,
        scheduled.expected_logical,
        scheduled.mapping_generation,
        frame_sequence,
        frame_sequence,
        scheduled.scheduling_thread,
        expected_payload,
        vm_context.vm_address,
        vm_context.drawable_image_context_address,
        vm_context.drawable_image_provider_address,
    };
    log_line(
        "bitmapDataProperty rooted payload=%p localWrappersReleased=1 phase=%s nextFrameVerificationArmed=1 propertyCleared=0 disposed=0",
        expected_payload,
        bitmapdata_property_verification_phase_name(
            g_bitmapdata_property_verification.phase));
}

void schedule_deferred_bitmapdata_constructor_probe(
    void* top,
    TabLayer layer,
    std::size_t physical) noexcept {
    std::size_t expected_logical{invalid_tab_index};
    const bool pending_mapping = g_runtime.pending.active &&
        g_runtime.pending.top == top;
    const TabViewport& expected_viewport = pending_mapping
        ? g_runtime.pending.candidate
        : g_runtime.viewport;
    if (!top || layer == TabLayer::none ||
        physical >= active_physical_tab_count() ||
        !g_runtime.active || g_runtime.top != top ||
        !expected_viewport.logical_for_physical(
            physical, expected_logical) ||
        g_bitmapdata_constructor_probe_attempted.load(
            std::memory_order_acquire) ||
        g_deferred_bitmapdata_probe.scheduled) {
        return;
    }

    g_deferred_bitmapdata_probe = DeferredBitmapDataProbe{
        true,
        top,
        layer,
        physical,
        expected_logical,
        expected_viewport.mapping_generation(),
        g_composite_frame_sequence,
        GetCurrentThreadId(),
    };
    log_line(
        "bitmapDataDeferred scheduled source=%s top=%p layer=%u physical=%zu logical=%zu generation=%llu afterFrame=%llu thread=%lu",
        pending_mapping ? "pending-candidate" : "active-viewport",
        top,
        static_cast<unsigned>(layer),
        physical,
        expected_logical,
        static_cast<unsigned long long>(
            expected_viewport.mapping_generation()),
        static_cast<unsigned long long>(g_composite_frame_sequence),
        static_cast<unsigned long>(GetCurrentThreadId()));
}

bool __fastcall image_bind_detour(
    void* scaleform_value,
    void** resource,
    const float* rectangle) noexcept {
    ImageBindFn const original =
        g_original_image_bind.load(std::memory_order_acquire);
    if (!original) return false;
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) {
        return original(scaleform_value, resource, rectangle);
    }

    void* resource_value{};
    (void)guarded_read(resource, resource_value);

    // Treat the marker as immediate: the validated presenter performs this
    // bind directly after its successful texture lookup. Consuming it at the
    // next bind prevents a stale pointer from correlating with unrelated UI.
    const PendingAliasedIcon pending = std::exchange(
        g_pending_aliased_icon, PendingAliasedIcon{});
    const bool correlated = pending.resource != nullptr &&
        pending.kind != VectorIconKind::none &&
        resource_value == pending.resource &&
        pending.presentation_epoch == g_runtime.presentation_epoch &&
        pending.context.active && g_repaint_context.active &&
        pending.context.top == g_repaint_context.top &&
        pending.context.layer == g_repaint_context.layer &&
        pending.context.physical == g_repaint_context.physical &&
        pending.context.logical == g_repaint_context.logical;
    if (!correlated) {
        return original(scaleform_value, resource, rectangle);
    }

    std::uint32_t target_flags{};
    void* target_object_interface{};
    void* target_object_interface_vtable{};
    void* target_get_member_slot{};
    void* target_invoke_slot{};
    void* target_get_movie_slot{};
    void* target_interface_owner_link{};
    void* target_movie_root{};
    void* target_movie_root_vtable{};
    void* target_movie_root_create_object_slot{};
    void* target_vm{};
    void* target_object{};
    void* native_display_object{};
    void* native_display_vtable{};
    void* resource_image{};
    std::array<float, 4> rectangle_values{};
    const auto* const target_bytes =
        static_cast<const std::byte*>(scaleform_value);
    const bool object_interface_read = target_bytes &&
        guarded_read(target_bytes + 0x18, target_object_interface);
    const bool object_interface_vtable_read = target_object_interface &&
        guarded_read(
            target_object_interface,
            target_object_interface_vtable);
    const auto* const object_interface_vtable_bytes =
        static_cast<const std::byte*>(target_object_interface_vtable);
    const bool get_member_slot_read = object_interface_vtable_bytes &&
        guarded_read(
            object_interface_vtable_bytes + 0x20,
            target_get_member_slot);
    const bool invoke_slot_read = object_interface_vtable_bytes &&
        guarded_read(
            object_interface_vtable_bytes + 0x30,
            target_invoke_slot);
    const bool get_movie_slot_read = object_interface_vtable_bytes &&
        guarded_read(
            object_interface_vtable_bytes + 0x178,
            target_get_movie_slot);
    const auto* const object_interface_bytes =
        static_cast<const std::byte*>(target_object_interface);
    const bool interface_owner_link_read = object_interface_bytes &&
        guarded_read(
            object_interface_bytes + 0x08,
            target_interface_owner_link);
    const auto* const interface_owner_link_bytes =
        static_cast<const std::byte*>(target_interface_owner_link);
    const bool movie_root_read = interface_owner_link_bytes &&
        guarded_read(
            interface_owner_link_bytes + 0x18,
            target_movie_root);
    const bool movie_root_vtable_read = target_movie_root &&
        guarded_read(target_movie_root, target_movie_root_vtable);
    const auto* const movie_root_vtable_bytes =
        static_cast<const std::byte*>(target_movie_root_vtable);
    const bool movie_root_create_object_slot_read =
        movie_root_vtable_bytes &&
        guarded_read(
            movie_root_vtable_bytes + 0x170,
            target_movie_root_create_object_slot);
    const auto* const movie_root_bytes =
        static_cast<const std::byte*>(target_movie_root);
    const bool vm_read = movie_root_bytes &&
        guarded_read(movie_root_bytes + 0x88, target_vm);
    const bool flags_read = target_bytes &&
        guarded_read(target_bytes + 0x20, target_flags);
    const bool object_read = target_bytes &&
        guarded_read(target_bytes + 0x28, target_object);
    const auto* const object_bytes =
        static_cast<const std::byte*>(target_object);
    const bool native_object_read = object_bytes &&
        guarded_read(object_bytes + 0x88, native_display_object);
    const bool native_vtable_read = native_display_object &&
        guarded_read(native_display_object, native_display_vtable);
    const auto* const resource_bytes =
        static_cast<const std::byte*>(resource_value);
    const bool resource_image_read = resource_bytes &&
        guarded_read(resource_bytes + 0x18, resource_image);
    const bool rectangle_read = rectangle && guarded_copy(
        rectangle_values.data(), rectangle, sizeof(rectangle_values));
    const bool exact_pending_asset = !pending.direct_rgba8 ||
        (pending.rgba8_index < active_custom_tab_count() &&
            pending.resource && pending.image &&
            resource_value == pending.resource &&
            resource_image_read && resource_image == pending.image);

    bool fingerprint_consistent{};
    if (object_interface_read &&
        interface_owner_link_read && movie_root_read) {
        const bool unset =
            g_injected_movie_fingerprint.object_interface == nullptr &&
            g_injected_movie_fingerprint.owner_link == nullptr &&
            g_injected_movie_fingerprint.movie_root == nullptr;
        if (unset) {
            g_injected_movie_fingerprint.object_interface =
                target_object_interface;
            g_injected_movie_fingerprint.owner_link =
                target_interface_owner_link;
            g_injected_movie_fingerprint.movie_root = target_movie_root;
            g_injected_movie_fingerprint.presentation_epoch =
                g_runtime.presentation_epoch;
        }
        fingerprint_consistent =
            g_injected_movie_fingerprint.object_interface ==
                target_object_interface &&
            g_injected_movie_fingerprint.owner_link ==
                target_interface_owner_link &&
            g_injected_movie_fingerprint.movie_root == target_movie_root &&
            g_injected_movie_fingerprint.presentation_epoch ==
                g_runtime.presentation_epoch;
    }

    // The lookup reference still owns this known-native resource and the
    // original binder below will consume it. Observe scalar fields only while
    // that ownership is unquestionably live.
    if (resource_image_read) {
        log_native_image_resource_snapshot(resource_value, resource_image);
    }

    const std::uint32_t event =
        g_image_bind_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumImageBindLogs) {
        log_line(
            "iconBind[%u] epoch=%llu icon=%zu logical=%zu checksum=0x%016llX fingerprintConsistent=%u interfaceRead=%u interface=%p interfaceVtableRead=%u interfaceVtable=%p getMemberRead=%u getMember=%p invokeRead=%u invoke=%p getMovieRead=%u getMovie=%p ownerLinkRead=%u ownerLink=%p movieRootRead=%u movieRoot=%p movieRootVtableRead=%u movieRootVtable=%p rootCreateObjectRead=%u rootCreateObject=%p vmRead=%u vm=%p",
            static_cast<unsigned>(event),
            static_cast<unsigned long long>(pending.presentation_epoch),
            pending.rgba8_index,
            pending.logical,
            static_cast<unsigned long long>(pending.checksum),
            fingerprint_consistent ? 1U : 0U,
            object_interface_read ? 1U : 0U,
            target_object_interface,
            object_interface_vtable_read ? 1U : 0U,
            target_object_interface_vtable,
            get_member_slot_read ? 1U : 0U,
            target_get_member_slot,
            invoke_slot_read ? 1U : 0U,
            target_invoke_slot,
            get_movie_slot_read ? 1U : 0U,
            target_get_movie_slot,
            interface_owner_link_read ? 1U : 0U,
            target_interface_owner_link,
            movie_root_read ? 1U : 0U,
            target_movie_root,
            movie_root_vtable_read ? 1U : 0U,
            target_movie_root_vtable,
            movie_root_create_object_slot_read ? 1U : 0U,
            target_movie_root_create_object_slot,
            vm_read ? 1U : 0U,
            target_vm);
        log_line(
            "iconBind[%u] pre target=%p flagsRead=%u flags=0x%08X objectRead=%u object=%p nativeObjectRead=%u nativeObject=%p nativeVtableRead=%u nativeVtable=%p resourceArg=%p resource=%p imageRead=%u image=%p rectRead=%u rect={%.3f,%.3f,%.3f,%.3f}",
            static_cast<unsigned>(event),
            scaleform_value,
            flags_read ? 1U : 0U,
            static_cast<unsigned>(target_flags),
            object_read ? 1U : 0U,
            target_object,
            native_object_read ? 1U : 0U,
            native_display_object,
            native_vtable_read ? 1U : 0U,
            native_display_vtable,
            static_cast<void*>(resource),
            resource_value,
            resource_image_read ? 1U : 0U,
            resource_image,
            rectangle_read ? 1U : 0U,
            static_cast<double>(rectangle_values[0]),
            static_cast<double>(rectangle_values[1]),
            static_cast<double>(rectangle_values[2]),
            static_cast<double>(rectangle_values[3]));
    }

    // Validate the recovered ObjectInterface::Invoke ABI with an inherited,
    // side-effect-free method before any later probe creates a display object.
    // This is deliberately limited to the exact bridge selected above.
    if (!g_value_invoke_probe_attempted.exchange(
            true, std::memory_order_relaxed)) {
        const bool exact_get_movie =
            g_native_tabs_build_profile &&
            target_object_interface_vtable ==
                g_game.base + kAs3ValueInterfaceVtable271Rva &&
            target_get_movie_slot ==
                g_game.base + kAs3ValueGetMovie271Rva;
        void* public_movie{};
        void* public_movie_vtable{};
        void* public_movie_slot_170{};
        unsigned long get_movie_seh{};
        if (exact_get_movie) {
#if defined(_MSC_VER)
            __try {
#endif
                public_movie = reinterpret_cast<ValueGetMovieFn>(
                    target_get_movie_slot)(target_object_interface);
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                get_movie_seh =
                    static_cast<unsigned long>(GetExceptionCode());
            }
#endif
        }
        const bool public_movie_vtable_read = public_movie &&
            guarded_read(public_movie, public_movie_vtable);
        const auto* const public_movie_vtable_bytes =
            static_cast<const std::byte*>(public_movie_vtable);
        const bool public_movie_slot_170_read =
            public_movie_vtable_bytes && guarded_read(
                public_movie_vtable_bytes + 0x170,
                public_movie_slot_170);
        log_line(
            "valueGetMovie validation exact=%u movie=%p movieVtableRead=%u movieVtable=%p slot170Read=%u slot170=%p seh=0x%08lX",
            exact_get_movie ? 1U : 0U,
            public_movie,
            public_movie_vtable_read ? 1U : 0U,
            public_movie_vtable,
            public_movie_slot_170_read ? 1U : 0U,
            public_movie_slot_170,
            get_movie_seh);

        const bool exact_interface =
            g_native_tabs_build_profile &&
            target_object_interface_vtable ==
                g_game.base + kAs3ValueInterfaceVtable271Rva &&
            target_invoke_slot == g_game.base + kAs3ValueInvoke271Rva &&
            target_movie_root_vtable ==
                g_game.base + kAs3MovieVtable271Rva &&
            target_movie_root_create_object_slot ==
                g_game.base + kAs3MovieCreateObject271Rva &&
            validated_authored_icon_target_vtable(
                native_display_vtable) &&
            (target_flags & 0x8FU) == 0x0AU && target_object;
        bool invoked{};
        unsigned long seh_code{};
        if (exact_interface) {
#if defined(_MSC_VER)
            __try {
#endif
                invoked = reinterpret_cast<ValueInvokeFn>(
                    target_invoke_slot)(
                    target_object_interface,
                    target_object,
                    nullptr,
                    "toString",
                    nullptr,
                    0,
                    true);
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                seh_code =
                    static_cast<unsigned long>(GetExceptionCode());
            }
#endif
        }
        log_line(
            "valueInvoke validation exact=%u result=%u seh=0x%08lX method=toString",
            exact_interface ? 1U : 0U,
            invoked ? 1U : 0U,
            seh_code);
        if (exact_interface && invoked && seh_code == 0) {
            g_value_invoke_probe_validated.store(
                true, std::memory_order_release);
        }
    }

    const bool exact_attachment_bridge =
        g_native_tabs_build_profile &&
        g_repaint_context.active &&
        g_repaint_context.top == g_runtime.top &&
        g_repaint_context.layer != TabLayer::none &&
        g_repaint_context.physical < active_physical_tab_count() &&
        g_value_invoke_probe_validated.load(std::memory_order_acquire) &&
        target_object_interface_vtable ==
            g_game.base + kAs3ValueInterfaceVtable271Rva &&
        target_get_member_slot ==
            g_game.base + kAs3ValueGetMember271Rva &&
        target_invoke_slot == g_game.base + kAs3ValueInvoke271Rva &&
        target_movie_root_vtable ==
            g_game.base + kAs3MovieVtable271Rva &&
        target_movie_root_create_object_slot ==
            g_game.base + kAs3MovieCreateObject271Rva &&
        validated_authored_icon_target_vtable(
            native_display_vtable) &&
        flags_read && (target_flags & 0x8FU) == 0x0AU && target_object &&
        rectangle_read && g_construct_scaleform_value &&
        g_get_scaleform_member && g_set_scaleform_position &&
        g_set_scaleform_visible && g_destroy_scaleform_value;

    if (pending.direct_rgba8) {
        const std::uint32_t attach_attempt =
            g_shape_attach_attempt_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        const bool exact_host_coordinate =
            g_repaint_context.layer != TabLayer::none &&
            g_repaint_context.physical < active_physical_tab_count() &&
            pending.rgba8_index < active_custom_tab_count();
        const std::size_t host_bit = exact_host_coordinate
            ? g_repaint_context.physical +
                (g_repaint_context.layer == TabLayer::back
                    ? active_physical_tab_count()
                    : 0)
            : invalid_tab_index;
        const TabHostMask host_mask = exact_host_coordinate
            ? TabHostMask{1} << host_bit
            : 0;
        const std::size_t desired_asset = exact_host_coordinate
            ? pending.rgba8_index + 1U
            : 0;
        const bool staging_fresh_host = exact_host_coordinate &&
            repaint_stages_fresh_host(host_bit);
        const TabHostMask prior_rgba_hosts = staging_fresh_host
            ? g_staged_rgba8_icon_hosts.load(std::memory_order_acquire)
            : g_rgba8_icon_hosts.load(std::memory_order_acquire);
        const TabHostMask prior_injected_hosts = staging_fresh_host
            ? g_staged_injected_icon_hosts.load(std::memory_order_acquire)
            : g_injected_icon_hosts.load(std::memory_order_acquire);
        const std::size_t prior_asset = exact_host_coordinate
            ? (staging_fresh_host
                ? g_staged_rgba8_host_assets[host_bit].load(
                    std::memory_order_acquire)
                : g_rgba8_host_assets[host_bit].load(
                    std::memory_order_acquire))
            : 0;
        const bool prior_host_consistent = exact_host_coordinate &&
            (((prior_rgba_hosts & host_mask) != 0) ==
                (prior_asset != 0));
        const InPlaceRgba8RebindTransaction& rebind_transaction =
            g_in_place_rgba8_rebind_transaction;
        const bool matching_rebind_transaction =
            rebind_transaction.active &&
            g_runtime.pending.active &&
            rebind_transaction.top == g_repaint_context.top &&
            rebind_transaction.top == g_runtime.top &&
            rebind_transaction.presentation_epoch ==
                g_runtime.presentation_epoch &&
            rebind_transaction.source_window ==
                g_runtime.viewport.window_start() &&
            rebind_transaction.target_window ==
                g_runtime.pending.candidate.window_start() &&
            g_runtime.pending.top == g_runtime.top &&
            g_runtime.pending.candidate.mapping_generation() ==
                rebind_transaction.target_generation;
        const bool exact_rebind_transaction =
            matching_rebind_transaction && !rebind_transaction.failed &&
            rebind_transaction.stage ==
                InPlaceRgba8RebindStage::repainting;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        const bool pending_matches_stable_asset =
            stable_custom_index_for_logical(pending.logical) ==
                pending.rgba8_index;
#else
        const bool pending_matches_stable_asset = pending.logical ==
            kStockLogicalTabCount + pending.rgba8_index;
#endif
        const bool exact_shape_replacement =
            exact_rebind_transaction && exact_host_coordinate &&
            (rebind_transaction.expected_host_mask & host_mask) != 0 &&
            staging_fresh_host &&
            rebind_transaction.target_assets[host_bit] == desired_asset &&
            rebind_transaction.target_resources[host_bit] ==
                pending.resource &&
            (prior_injected_hosts & host_mask) == 0 &&
            (prior_rgba_hosts & host_mask) == 0 && prior_asset == 0U &&
            desired_asset > 0U &&
            desired_asset <= active_custom_tab_count() &&
            pending_matches_stable_asset;
        const bool exact_same_asset_repaint =
            exact_rebind_transaction && exact_host_coordinate &&
            !staging_fresh_host && prior_asset != 0 &&
            prior_asset == desired_asset &&
            rebind_transaction.source_resources[host_bit] ==
                pending.resource;
        const bool host_transition_compatible = prior_host_consistent &&
            (prior_asset == 0 || prior_asset == desired_asset ||
                exact_shape_replacement);
        if (!host_transition_compatible) {
            if (g_in_place_rgba8_rebind_transaction.active) {
                g_in_place_rgba8_rebind_transaction.failed = true;
            } else {
                g_rgba8_publication_disabled.store(
                    true, std::memory_order_release);
            }
            log_failure(
                "RGBA8 bind refused before Shape mutation because host ownership was incompatible (layer=%u physical=%zu priorAsset=%u desiredAsset=%u rgbaBits=0x%04X)",
                static_cast<unsigned>(g_repaint_context.layer),
                g_repaint_context.physical,
                static_cast<unsigned>(prior_asset),
                static_cast<unsigned>(desired_asset),
                static_cast<unsigned>(prior_rgba_hosts));
        }

        Rgba8ReferenceSnapshot fresh_refs_before{};
        bool rebind_pre_refs_exact{true};
        if (exact_shape_replacement) {
            const std::size_t fresh_asset_index = desired_asset - 1U;
            const RetainedRgba8Resource fresh_resource{
                .image = pending.image,
                .resource = pending.resource,
                .checksum = pending.checksum,
                .ready = true,
            };
            fresh_refs_before = snapshot_rgba8_references(fresh_resource);
            const std::uint32_t fresh_expected =
                1U + count_rgba8_resource_hosts(pending.resource) +
                rgba8_debt_for_resource(
                    fresh_asset_index, pending.resource);
            rebind_pre_refs_exact = fresh_refs_before.resource_read &&
                fresh_refs_before.resource == 3U &&
                fresh_refs_before.raw_read &&
                fresh_refs_before.raw == fresh_expected &&
                rgba8_debt_for_resource(
                    fresh_asset_index, pending.resource) == 0U;
            log_line(
                "rgba8Replace pre transition=%llu layer=%u physical=%zu freshAsset=%u refs={resource:%u/%u raw:%u/%u expected:%u} exact=%u",
                static_cast<unsigned long long>(
                    rebind_transaction.transition_sequence),
                static_cast<unsigned>(g_repaint_context.layer),
                g_repaint_context.physical,
                static_cast<unsigned>(desired_asset),
                fresh_refs_before.resource_read ? 1U : 0U,
                static_cast<unsigned>(fresh_refs_before.resource),
                fresh_refs_before.raw_read ? 1U : 0U,
                static_cast<unsigned>(fresh_refs_before.raw),
                static_cast<unsigned>(fresh_expected),
                rebind_pre_refs_exact ? 1U : 0U);
            if (!rebind_pre_refs_exact) {
                g_in_place_rgba8_rebind_transaction.failed = true;
            }
        }
        Rgba8ShapeAttachProbeResult attachment{};
        if (exact_attachment_bridge && fingerprint_consistent &&
            exact_pending_asset && host_transition_compatible &&
            rebind_pre_refs_exact) {
            attachment = probe_dynamic_rgba8_shape_attachment(
                attach_attempt,
                scaleform_value,
                target_movie_root,
                target_movie_root_create_object_slot,
                original,
                resource,
                rectangle_values,
                exact_same_asset_repaint,
                exact_same_asset_repaint,
                false);
        } else {
            // The presenter has already created a binder-local reference. If
            // the exact dynamic-Shape bridge is unavailable, balance that one
            // reference directly; forwarding this custom resource to the
            // authored static Shape is the disproven route.
            attachment.reference_before_read = guarded_read(
                static_cast<const std::byte*>(resource_value) + 0x08,
                attachment.reference_before);
            const bool exact_incoming_reference =
                attachment.reference_before_read &&
                attachment.reference_before == 3U;
            if (exact_incoming_reference) {
                attachment.resource_consumed =
                    guarded_release_scaleform_image_resource(
                        reinterpret_cast<ScaleformImageResourceReleaseFn>(
                            g_game.base +
                            kScaleformImageResourceRelease271Rva),
                        resource_value,
                        attachment.consume_seh);
                attachment.reference_after_read = guarded_read(
                    static_cast<const std::byte*>(resource_value) + 0x08,
                    attachment.reference_after);
            }
            attachment.exact_reference_transition =
                attachment.resource_consumed &&
                attachment.reference_before_read &&
                attachment.reference_after_read &&
                attachment.reference_before == 3U &&
                attachment.reference_after == 2U;
        }

        Rgba8ReferenceSnapshot fresh_refs_after{};
        bool exact_rebind_refs{};
        if (exact_shape_replacement && attachment.resource_consumed &&
            attachment.raster_bind_attempted && attachment.raster_bound &&
            attachment.created_new_shape &&
            !attachment.reused_existing_shape && attachment.cleanup_exact &&
            attachment.consume_seh == 0) {
            const std::size_t fresh_asset_index = desired_asset - 1U;
            fresh_refs_after = snapshot_rgba8_references({
                .image = pending.image,
                .resource = pending.resource,
                .checksum = pending.checksum,
                .ready = true,
            });
            exact_rebind_refs = fresh_refs_after.resource_read &&
                fresh_refs_after.resource == 2U &&
                fresh_refs_after.raw_read && fresh_refs_before.raw_read &&
                fresh_refs_before.raw <
                    std::numeric_limits<std::uint32_t>::max() &&
                fresh_refs_after.raw == fresh_refs_before.raw + 1U &&
                !attachment.pre_bind_graphics_clear_required &&
                !attachment.pre_bind_graphics_clear_attempted &&
                attachment.graphics_clear_seh == 0;
            log_line(
                "rgba8Replace attach transition=%llu layer=%u physical=%zu freshAsset=%u created=%u reused=%u binder={attempted:%u bound:%u seh:0x%08lX} refs={resource:%u/%u raw:%u/%u before:%u} debt=%u exact=%u",
                static_cast<unsigned long long>(
                    rebind_transaction.transition_sequence),
                static_cast<unsigned>(g_repaint_context.layer),
                g_repaint_context.physical,
                static_cast<unsigned>(desired_asset),
                attachment.created_new_shape ? 1U : 0U,
                attachment.reused_existing_shape ? 1U : 0U,
                attachment.raster_bind_attempted ? 1U : 0U,
                attachment.raster_bound ? 1U : 0U,
                attachment.consume_seh,
                fresh_refs_after.resource_read ? 1U : 0U,
                static_cast<unsigned>(fresh_refs_after.resource),
                fresh_refs_after.raw_read ? 1U : 0U,
                static_cast<unsigned>(fresh_refs_after.raw),
                static_cast<unsigned>(fresh_refs_before.raw),
                static_cast<unsigned>(
                    rgba8_debt_for_resource(
                        fresh_asset_index, pending.resource)),
                exact_rebind_refs ? 1U : 0U);
            if (!exact_rebind_refs) {
                g_in_place_rgba8_rebind_transaction.failed = true;
            }
        }

        const bool presentation_bind_exact = attachment.raster_bound ||
            attachment.same_asset_bind_skipped;
        bool host_asset_recorded{};
        if (attachment.host_retained && presentation_bind_exact &&
            pending.rgba8_index < active_custom_tab_count() &&
            g_repaint_context.layer != TabLayer::none &&
            g_repaint_context.physical < active_physical_tab_count()) {
            const std::size_t bit = host_bit;
            const TabHostMask mask = host_mask;
            const std::size_t recorded_prior_asset =
                staging_fresh_host
                ? g_staged_rgba8_host_assets[bit].load(
                    std::memory_order_acquire)
                : g_rgba8_host_assets[bit].load(
                    std::memory_order_acquire);
            AtomicTabHostMask& rgba_host_ledger = staging_fresh_host
                ? g_staged_rgba8_icon_hosts
                : g_rgba8_icon_hosts;
            auto& asset_ledger = staging_fresh_host
                ? g_staged_rgba8_host_assets
                : g_rgba8_host_assets;
            const TabHostMask prior_hosts =
                rgba_host_ledger.load(std::memory_order_acquire);
            const bool prior_consistent =
                ((prior_hosts & mask) != 0) ==
                (recorded_prior_asset != 0);
            const bool compatible_asset = recorded_prior_asset == 0 ||
                recorded_prior_asset == desired_asset ||
                (exact_shape_replacement && exact_rebind_refs &&
                    recorded_prior_asset == 0U);
            if (prior_consistent && compatible_asset) {
                const TabHostMask prior = rgba_host_ledger.fetch_or(
                    mask, std::memory_order_acq_rel);
                asset_ledger[bit].store(
                    desired_asset, std::memory_order_release);
                host_asset_recorded = true;
                if (exact_shape_replacement && exact_rebind_refs) {
                    g_in_place_rgba8_rebind_transaction.completed_host_mask =
                        g_in_place_rgba8_rebind_transaction
                            .completed_host_mask | mask;
                }
                if ((prior & mask) == 0) {
                    g_rgba8_successful_attach_count.fetch_add(
                        1, std::memory_order_relaxed);
                }
            } else {
                if (g_in_place_rgba8_rebind_transaction.active) {
                    g_in_place_rgba8_rebind_transaction.failed = true;
                } else {
                    g_rgba8_publication_disabled.store(
                        true, std::memory_order_release);
                }
                log_failure(
                    "RGBA8 host asset transition refused without a prior detach proof (layer=%u physical=%zu priorAsset=%u desiredAsset=%u priorBits=0x%04X)",
                    static_cast<unsigned>(g_repaint_context.layer),
                    g_repaint_context.physical,
                    static_cast<unsigned>(recorded_prior_asset),
                    static_cast<unsigned>(desired_asset),
                    static_cast<unsigned>(prior_hosts));
            }
        } else if (attachment.host_retained &&
            g_repaint_context.layer != TabLayer::none &&
            g_repaint_context.physical < active_physical_tab_count()) {
            const std::size_t bit = g_repaint_context.physical +
                (g_repaint_context.layer == TabLayer::back
                    ? active_physical_tab_count()
                    : 0);
            const TabHostMask mask = TabHostMask{1} << bit;
            const TabHostMask prior = g_rgba8_icon_hosts.load(
                std::memory_order_acquire);
            if ((prior & mask) != 0) {
                // A vector fallback can repaint the same Shape, but it is not
                // proof that Scaleform released the previous raster fill.
                // Preserve the old asset association until exact Shape
                // removal or an epoch/root-refcount proof clears it.
                log_line(
                    "RGBA8 fallback preserved retained asset ownership pending detach proof (layer=%u physical=%zu asset=%u)",
                    static_cast<unsigned>(g_repaint_context.layer),
                    g_repaint_context.physical,
                    static_cast<unsigned>(
                        g_rgba8_host_assets[bit].load(
                            std::memory_order_acquire)));
            }
        }

        const bool exact_dynamic_raster = exact_attachment_bridge &&
            fingerprint_consistent &&
            exact_pending_asset &&
            host_transition_compatible &&
            pending.rgba8_index < active_custom_tab_count() &&
            pending_matches_stable_asset &&
            attachment.resource_consumed &&
            ((attachment.raster_bind_attempted &&
                 attachment.raster_bound) ||
                attachment.same_asset_bind_skipped) &&
            host_asset_recorded &&
            (!exact_shape_replacement || exact_rebind_refs) &&
            (!exact_shape_replacement ||
                (!attachment.pre_bind_graphics_clear_required &&
                    !attachment.pre_bind_graphics_clear_attempted &&
                    attachment.graphics_clear_seh == 0)) &&
            (!exact_same_asset_repaint ||
                attachment.reused_existing_shape) &&
            attachment.exact_reference_transition &&
            attachment.cleanup_exact &&
            attachment.custom_visible && attachment.consume_seh == 0;
        const bool expected_transaction_rgba_cell =
            matching_rebind_transaction && exact_host_coordinate &&
            (rebind_transaction.expected_rgba_presentation_mask &
                host_mask) != 0;
        if (expected_transaction_rgba_cell) {
            if (exact_dynamic_raster && !rebind_transaction.failed) {
                g_in_place_rgba8_rebind_transaction
                    .completed_rgba_presentation_mask =
                    g_in_place_rgba8_rebind_transaction
                        .completed_rgba_presentation_mask | host_mask;
            } else {
                g_in_place_rgba8_rebind_transaction.failed = true;
            }
            log_line(
                "rgba8Replace presentation layer=%u physical=%zu asset=%u exact=%u completed=0x%04X/0x%04X",
                static_cast<unsigned>(g_repaint_context.layer),
                g_repaint_context.physical,
                static_cast<unsigned>(desired_asset),
                exact_dynamic_raster ? 1U : 0U,
                static_cast<unsigned>(
                    g_in_place_rgba8_rebind_transaction
                        .completed_rgba_presentation_mask),
                static_cast<unsigned>(
                    rebind_transaction.expected_rgba_presentation_mask));
        }
        const std::uint32_t rgba_event =
            g_rgba8_bind_log_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        log_line(
            "rgba8Bind[%u] epoch=%llu icon=%zu logical=%zu checksum=0x%016llX context={layer:%u physical:%zu} fingerprintConsistent=%u exactAsset=%u correlated=1 resource=%p image=%p rectRead=%u bridge=%u consumed=%u bindAttempted=%u sameAssetSkipped=%u created=%u reused=%u freshReplacement=%u replacementRefs=%u preClear={required:%u attempted:%u cleared:%u oldReleased:%u seh:0x%08lX} refs={beforeRead:%u before:%u afterRead:%u after:%u exact:%u} rasterBound=%u customVisible=%u vectorFallback=%u cleanupExact=%u result=%u seh=0x%08lX authoredTarget=0 dynamicShape=1",
            static_cast<unsigned>(rgba_event),
            static_cast<unsigned long long>(pending.presentation_epoch),
            pending.rgba8_index,
            pending.logical,
            static_cast<unsigned long long>(pending.checksum),
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            fingerprint_consistent ? 1U : 0U,
            exact_pending_asset ? 1U : 0U,
            resource_value,
            resource_image,
            rectangle_read ? 1U : 0U,
            exact_attachment_bridge ? 1U : 0U,
            attachment.resource_consumed ? 1U : 0U,
            attachment.raster_bind_attempted ? 1U : 0U,
            attachment.same_asset_bind_skipped ? 1U : 0U,
            attachment.created_new_shape ? 1U : 0U,
            attachment.reused_existing_shape ? 1U : 0U,
            exact_shape_replacement ? 1U : 0U,
            exact_rebind_refs ? 1U : 0U,
            attachment.pre_bind_graphics_clear_required ? 1U : 0U,
            attachment.pre_bind_graphics_clear_attempted ? 1U : 0U,
            attachment.pre_bind_graphics_cleared ? 1U : 0U,
            attachment.pre_bind_old_raw_released_exact ? 1U : 0U,
            attachment.graphics_clear_seh,
            attachment.reference_before_read ? 1U : 0U,
            static_cast<unsigned>(attachment.reference_before),
            attachment.reference_after_read ? 1U : 0U,
            static_cast<unsigned>(attachment.reference_after),
            attachment.exact_reference_transition ? 1U : 0U,
            attachment.raster_bound ? 1U : 0U,
            attachment.custom_visible ? 1U : 0U,
            attachment.vector_fallback_visible ? 1U : 0U,
            attachment.cleanup_exact ? 1U : 0U,
            attachment.bound ? 1U : 0U,
            attachment.consume_seh);
        if (!exact_dynamic_raster) {
            if (g_in_place_rgba8_rebind_transaction.active) {
                g_in_place_rgba8_rebind_transaction.failed = true;
            } else {
                g_rgba8_publication_disabled.store(
                    true, std::memory_order_release);
            }
            log_failure(
                "dynamic-Shape RGBA8 bind failed its exact presentation/ownership gate; later custom icons use the proven vector fallback");
        }
        if (event <= kMaximumImageBindLogs) {
            log_line(
                "iconBind[%u] result=%u target=%p resource=%p route=dynamic-shape-rgba8 fallback=%u",
                static_cast<unsigned>(event),
                attachment.bound ? 1U : 0U,
                scaleform_value,
                resource_value,
                attachment.vector_fallback_visible ? 1U : 0U);
        }
        return attachment.bound;
    }

    const std::uint32_t attach_attempt =
        g_shape_attach_attempt_count.fetch_add(1, std::memory_order_relaxed) +
        1;
    ShapeAttachProbeResult attachment{};
    if (exact_attachment_bridge) {
        if (pending.kind == VectorIconKind::cross &&
            kEnableBitmapDataOwnershipProbe) {
            // Capture only stable logical coordinates here. The later frame
            // reacquires a fresh parent wrapper and derives its current Movie;
            // no raw Scaleform object or movie pointer crosses frame boundaries.
            schedule_deferred_bitmapdata_constructor_probe(
                g_repaint_context.top,
                g_repaint_context.layer,
                g_repaint_context.physical);
        }
        // BitmapData creation is deliberately excluded here. Runtime tests
        // proved that even constructor-only BitmapData work performed from
        // this image-binding callback can poison Scaleform presentation for
        // the whole game. Keep this path on the proven native Shape binder.
        attachment = probe_dynamic_shape_attachment(
            attach_attempt,
            pending.kind,
            pending.transparent_vector_placeholder,
            scaleform_value,
            target_movie_root,
            target_movie_root_create_object_slot,
            original,
            resource,
            rectangle_values);
    } else if (attach_attempt <= kMaximumShapeAttachLogs) {
        log_line(
            "shapeAttach[%u] kind=%u skipped exactBridge=%u context=%u layer=%u physical=%zu invokeValidated=%u targetVtable=%p targetSupported=%u rectRead=%u ctorReady=%u memberReady=%u positionReady=%u visibleReady=%u dtorReady=%u",
            static_cast<unsigned>(attach_attempt),
            static_cast<unsigned>(pending.kind),
            exact_attachment_bridge ? 1U : 0U,
            g_repaint_context.active ? 1U : 0U,
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            g_value_invoke_probe_validated.load(
                std::memory_order_acquire) ? 1U : 0U,
            native_display_vtable,
            validated_authored_icon_target_vtable(
                native_display_vtable) ? 1U : 0U,
            rectangle_read ? 1U : 0U,
            g_construct_scaleform_value ? 1U : 0U,
            g_get_scaleform_member ? 1U : 0U,
            g_set_scaleform_position ? 1U : 0U,
            g_set_scaleform_visible ? 1U : 0U,
            g_destroy_scaleform_value ? 1U : 0U);
    }

    const bool bound = attachment.resource_consumed
        ? attachment.bound
        : original(scaleform_value, resource, rectangle);
    const std::size_t vector_host_bit =
        g_repaint_context.layer != TabLayer::none &&
            g_repaint_context.physical < active_physical_tab_count()
        ? g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0)
        : invalid_tab_index;
    const TabHostMask vector_host_mask =
        vector_host_bit < active_tab_host_count()
        ? TabHostMask{1} << vector_host_bit
        : 0;
    const InPlaceRgba8RebindTransaction& vector_transaction =
        g_in_place_rgba8_rebind_transaction;
    const bool matching_vector_transaction = vector_transaction.active &&
        g_runtime.pending.active &&
        vector_transaction.top == g_repaint_context.top &&
        vector_transaction.top == g_runtime.top &&
        vector_transaction.presentation_epoch ==
            g_runtime.presentation_epoch &&
        vector_transaction.source_window ==
            g_runtime.viewport.window_start() &&
        vector_transaction.target_window ==
            g_runtime.pending.candidate.window_start() &&
        g_runtime.pending.top == g_runtime.top &&
        g_runtime.pending.candidate.mapping_generation() ==
            vector_transaction.target_generation;
    const bool expected_fresh_vector =
        matching_vector_transaction &&
        !vector_transaction.failed &&
        vector_transaction.stage == InPlaceRgba8RebindStage::repainting &&
        (vector_transaction.expected_vector_host_mask &
            vector_host_mask) != 0;
    if (expected_fresh_vector) {
        const bool exact_vector_creation =
            (pending.kind == VectorIconKind::ellipsis ||
                pending.kind == VectorIconKind::cross) &&
            attachment.resource_consumed &&
            attachment.created_new_shape &&
            !attachment.reused_existing_shape &&
            attachment.custom_visible && attachment.cleanup_exact && bound;
        if (exact_vector_creation) {
            g_in_place_rgba8_rebind_transaction
                .completed_vector_host_mask =
                g_in_place_rgba8_rebind_transaction
                    .completed_vector_host_mask | vector_host_mask;
            g_in_place_rgba8_rebind_transaction.completed_host_mask =
                g_in_place_rgba8_rebind_transaction
                    .completed_host_mask | vector_host_mask;
        } else {
            g_in_place_rgba8_rebind_transaction.failed = true;
        }
        log_line(
            "rgba8Rebind fresh-vector layer=%u physical=%zu created=%u reused=%u visible=%u cleanup=%u bound=%u exact=%u completed=0x%04X/0x%04X",
            static_cast<unsigned>(g_repaint_context.layer),
            g_repaint_context.physical,
            attachment.created_new_shape ? 1U : 0U,
            attachment.reused_existing_shape ? 1U : 0U,
            attachment.custom_visible ? 1U : 0U,
            attachment.cleanup_exact ? 1U : 0U,
            bound ? 1U : 0U,
            exact_vector_creation ? 1U : 0U,
            static_cast<unsigned>(
                g_in_place_rgba8_rebind_transaction
                    .completed_vector_host_mask),
            static_cast<unsigned>(
                vector_transaction.expected_vector_host_mask));
    }
    if (bound && g_repaint_context.layer != TabLayer::none &&
        g_repaint_context.physical < active_physical_tab_count()) {
        const std::size_t bit = g_repaint_context.physical +
            (g_repaint_context.layer == TabLayer::back
                ? active_physical_tab_count()
                : 0);
        const TabHostMask mask = TabHostMask{1} << bit;
        const TabHostMask prior = g_rgba8_icon_hosts.load(
            std::memory_order_acquire);
        if ((prior & mask) != 0) {
            // The vector presenter may reuse our dynamic Shape. A successful
            // paint does not prove its old image fill released a RawImage
            // reference, so keep the RGBA ownership ledger until exact
            // removeChild/property cleanup (or a movie-boundary ref proof).
            log_line(
                "vector bind preserved RGBA8 ownership pending detach proof (layer=%u physical=%zu asset=%u)",
                static_cast<unsigned>(g_repaint_context.layer),
                g_repaint_context.physical,
                static_cast<unsigned>(
                    g_rgba8_host_assets[bit].load(
                        std::memory_order_acquire)));
        }
    }
    if (event <= kMaximumImageBindLogs) {
        log_line(
            "iconBind[%u] result=%u target=%p resource=%p",
            static_cast<unsigned>(event),
            bound ? 1U : 0U,
            scaleform_value,
            resource_value);
    }
    if (event == kMaximumImageBindLogs) {
        log_line("Further synthetic-icon bind events are suppressed");
    }
    return bound;
}

bool initialize_log() noexcept {
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(
        g_module, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || static_cast<std::size_t>(length) >= module_path.size()) {
        return false;
    }

    wchar_t* const separator = wcsrchr(module_path.data(), L'\\');
    if (!separator) return false;
    *(separator + 1) = L'\0';
    constexpr wchar_t kLogName[] = L"ERNativeUITabViewportProbe.log";
    const std::size_t prefix_length = wcslen(module_path.data());
    if (prefix_length + _countof(kLogName) > module_path.size()) return false;
    wcscat_s(module_path.data(), module_path.size(), kLogName);

    g_log_file = CreateFileW(
        module_path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return g_log_file != INVALID_HANDLE_VALUE;
}

bool inspect_game_image(GameImage& image) noexcept {
    image = {};
    auto* const base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!base) return false;

#if defined(_MSC_VER)
    __try {
#endif
        const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
            dos->e_lfanew > 0x100000) {
            return false;
        }
        const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return false;
        }
        image.base = base;
        image.size = nt->OptionalHeader.SizeOfImage;
        image.timestamp = nt->FileHeader.TimeDateStamp;
        return image.size >= 0x1000;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        image = {};
        return false;
    }
#endif
}

bool supported_game_image(const GameImage& image) noexcept {
    return find_native_tabs_build_profile(image.timestamp, image.size) !=
        nullptr;
}

bool image_contains(std::uintptr_t rva, std::size_t size) noexcept {
    return g_game.base && rva < g_game.size && size <= g_game.size - rva;
}

template <std::size_t Size>
bool entry_matches(
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    if (!image_contains(rva, Size)) return false;
    std::array<std::uint8_t, Size> actual{};
    return guarded_copy(actual.data(), g_game.base + rva, Size) &&
        actual == expected;
}

bool resolve_rip_relative_data_target(
    std::uintptr_t instruction_rva,
    std::uintptr_t& target_rva) noexcept {
    target_rva = 0;
    constexpr std::size_t kInstructionSize = 7;
    if (!image_contains(instruction_rva, kInstructionSize)) return false;

    std::array<std::uint8_t, 3> opcode{};
    std::int32_t displacement{};
    if (!guarded_copy(
            opcode.data(),
            g_game.base + instruction_rva,
            opcode.size()) ||
        opcode != std::array<std::uint8_t, 3>{0x48, 0x8B, 0x0D} ||
        !guarded_read(
            g_game.base + instruction_rva + opcode.size(),
            displacement)) {
        return false;
    }

    const std::int64_t target =
        static_cast<std::int64_t>(instruction_rva) +
        static_cast<std::int64_t>(kInstructionSize) + displacement;
    if (target < 0 ||
        static_cast<std::uint64_t>(target) >= g_game.size) {
        return false;
    }
    target_rva = static_cast<std::uintptr_t>(target);
    return true;
}

bool guarded_create_raw_image(
    RawImageCreateFn create,
    const RawImageSize& size,
    void*& image,
    unsigned long& seh_code) noexcept {
    image = nullptr;
    seh_code = 0;
    if (!create) return false;
#if defined(_MSC_VER)
    __try {
#endif
        image = create(
            1U, // Scaleform::Render::Image_R8G8B8A8
            1U, // one actual top-level mip level
            &size,
            0U, // immutable system-memory image
            nullptr,
            nullptr);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
        image = nullptr;
    }
#endif
    return image != nullptr && seh_code == 0;
}

bool guarded_get_raw_image_data(
    RawImageGetDataFn get_data,
    void* image,
    RawImageData& data,
    bool& returned,
    unsigned long& seh_code) noexcept {
    returned = false;
    seh_code = 0;
    if (!get_data || !image) return false;
#if defined(_MSC_VER)
    __try {
#endif
        returned = get_data(image, &data);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
        returned = false;
    }
#endif
    return seh_code == 0;
}

bool guarded_clear_raw_image_data(
    ImageDataClearFn clear,
    RawImageData& data,
    unsigned long& seh_code) noexcept {
    seh_code = 0;
    if (!clear) return false;
#if defined(_MSC_VER)
    __try {
#endif
        clear(&data);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
    }
#endif
    return seh_code == 0;
}

bool guarded_release_raw_image(
    RawImageReleaseFn release,
    void* image,
    unsigned long& seh_code) noexcept {
    seh_code = 0;
    if (!release || !image) return false;
#if defined(_MSC_VER)
    __try {
#endif
        release(image);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
    }
#endif
    return seh_code == 0;
}

bool guarded_allocate_scaleform_image_resource(
    NativeHeapAllocFn allocate,
    void* heap,
    void*& storage,
    unsigned long& seh_code) noexcept {
    storage = nullptr;
    seh_code = 0;
    if (!allocate || !heap) return false;
#if defined(_MSC_VER)
    __try {
#endif
        storage = allocate(
            heap,
            kScaleformImageResourceSize,
            nullptr);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
        storage = nullptr;
    }
#endif
    return storage != nullptr && seh_code == 0;
}

bool guarded_construct_scaleform_image_resource(
    ScaleformImageResourceCtorFn construct,
    void* storage,
    void* image,
    const wchar_t* name,
    void*& result,
    unsigned long& seh_code) noexcept {
    result = nullptr;
    seh_code = 0;
    if (!construct || !storage || !image || !name) return false;
#if defined(_MSC_VER)
    __try {
#endif
        result = construct(
            storage,
            name,
            image,
            1U);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
        result = nullptr;
    }
#endif
    return result == storage && seh_code == 0;
}

bool guarded_add_ref_scaleform_image_resource(
    ScaleformImageResourceAddRefFn add_ref,
    void* resource,
    unsigned long& seh_code) noexcept {
    seh_code = 0;
    if (!add_ref || !resource) return false;
#if defined(_MSC_VER)
    __try {
#endif
        add_ref(resource);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
    }
#endif
    return seh_code == 0;
}

bool guarded_release_scaleform_image_resource(
    ScaleformImageResourceReleaseFn release,
    void* resource,
    unsigned long& seh_code) noexcept {
    seh_code = 0;
    if (!release || !resource) return false;
#if defined(_MSC_VER)
    __try {
#endif
        release(resource);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = static_cast<unsigned long>(GetExceptionCode());
    }
#endif
    return seh_code == 0;
}

void run_raw_image_lifecycle_probe_one(
    void* top,
    std::size_t icon_index,
    RetainedRgba8Resource& retained) noexcept {
    retained = {};
    const bool staging_fresh_generation =
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        g_rgba8_staging_generation != nullptr;
#else
        false;
#endif
    const bool valid_context = staging_fresh_generation
        ? top && validate_top(top)
        : top && g_runtime.active && g_runtime.top == top &&
            validate_top(top);
    if (!kEnableRawImageLifecycleProbe || !valid_context ||
        icon_index >= active_custom_tab_count() ||
        !catalogue_icon_requires_raster(icon_index)) {
        return;
    }

    constexpr std::array<std::uint8_t, 20> kRawImageCreateEntry{
        0x89, 0x54, 0x24, 0x10, 0x89, 0x4C, 0x24, 0x08,
        0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x57,
        0x48, 0x83, 0xEC, 0x40};
    constexpr std::array<std::uint8_t, 26> kRawImageGetDataEntry{
        0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0xC2, 0x48,
        0x8D, 0x51, 0x28, 0x48, 0x8B, 0xC8, 0xE8, 0x1D,
        0xBF, 0xFF, 0xFF, 0xB0, 0x01, 0x48, 0x83, 0xC4,
        0x28, 0xC3};
    constexpr std::array<std::uint8_t, 20> kImageDataClearEntry{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
        0x48, 0x83, 0xEC, 0x20};
    constexpr std::array<std::uint8_t, 16> kRawImageReleaseEntry{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
        0xD9, 0x83, 0xCA, 0xFF, 0x48, 0x83, 0xC1, 0x08};

    if (!g_native_tabs_build_profile) return;
    const std::uintptr_t create_rva = kRawImageCreate271Rva;
    const std::uintptr_t get_data_rva = kRawImageGetData271Rva;
    const std::uintptr_t clear_rva = kImageDataClear271Rva;
    const std::uintptr_t expected_release_rva = kRawImageRelease271Rva;
    const bool exact_interfaces =
        entry_matches(create_rva, kRawImageCreateEntry) &&
        entry_matches(get_data_rva, kRawImageGetDataEntry) &&
        entry_matches(clear_rva, kImageDataClearEntry) &&
        entry_matches(expected_release_rva, kRawImageReleaseEntry) &&
        image_contains(kRawImageVtableRva, 0x18) &&
        image_contains(expected_release_rva, kRawImageReleaseEntry.size());
    log_line(
        "rawImageLifecycle begin icon=%zu id=%d thread=%lu profile=%s interfacesExact=%u exposed=0 bound=0",
        icon_index,
        kRgba8IconIds[icon_index],
        static_cast<unsigned long>(GetCurrentThreadId()),
        tabs_build().version,
        exact_interfaces ? 1U : 0U);
    if (!exact_interfaces) {
        log_failure(
            "RawImage lifecycle interfaces differ from the validated executable; no native call was made");
        return;
    }

    RawImageCreateFn const create = reinterpret_cast<RawImageCreateFn>(
        g_game.base + create_rva);
    RawImageGetDataFn const get_data =
        reinterpret_cast<RawImageGetDataFn>(g_game.base + get_data_rva);
    ImageDataClearFn const clear = reinterpret_cast<ImageDataClearFn>(
        g_game.base + clear_rva);
    constexpr RawImageSize kSize{
        static_cast<std::uint32_t>(kProbeBitmapWidth),
        static_cast<std::uint32_t>(kProbeBitmapHeight)};

    void* image{};
    unsigned long create_seh{};
    const bool created = guarded_create_raw_image(
        create, kSize, image, create_seh);
    if (!created) {
        log_failure(
            "RawImage::Create failed closed (image=%p seh=0x%08lX); nothing was exposed",
            image,
            create_seh);
        return;
    }

    void* vtable{};
    std::uint32_t refcount{};
    void* release_slot{};
    const bool object_fields_read =
        guarded_read(image, vtable) &&
        guarded_read(
            static_cast<const std::byte*>(image) + 0x08,
            refcount) &&
        vtable &&
        guarded_read(
            static_cast<const std::byte*>(vtable) + 0x10,
            release_slot);
    const bool exact_object = object_fields_read &&
        vtable == g_game.base + kRawImageVtableRva &&
        release_slot == g_game.base + expected_release_rva &&
        refcount == 1U;
    log_line(
        "rawImageLifecycle object image=%p reads=%u exact=%u vtable=%p rva=0x%llX ref=%u release=%p rva=0x%llX createSeh=0x%08lX",
        image,
        object_fields_read ? 1U : 0U,
        exact_object ? 1U : 0U,
        vtable,
        static_cast<unsigned long long>(
            locate_game_pointer(vtable).rva),
        static_cast<unsigned>(refcount),
        release_slot,
        static_cast<unsigned long long>(
            locate_game_pointer(release_slot).rva),
        create_seh);
    if (!exact_object) {
        log_failure(
            "RawImage object identity was unexpected; the unexposed allocation is intentionally retained rather than calling an unproven release slot");
        return;
    }

    RawImageData view{};
    view.raw_plane_count = 1;
    view.planes = &view.plane0;
    bool get_data_returned{};
    unsigned long get_data_seh{};
    const bool get_data_call_completed = guarded_get_raw_image_data(
        get_data,
        image,
        view,
        get_data_returned,
        get_data_seh);

    RawImagePlane plane{};
    bool plane_read{};
    if (get_data_call_completed && get_data_returned && view.planes) {
        plane_read = guarded_read(view.planes, plane);
    }
    constexpr std::uintptr_t kRowBytes =
        static_cast<std::uintptr_t>(kProbeBitmapWidth) * 4U;
    const bool plane_size_multiplication_safe = plane.pitch == 0 ||
        static_cast<std::uintptr_t>(kProbeBitmapHeight) <=
            (std::numeric_limits<std::uintptr_t>::max)() / plane.pitch;
    const std::uintptr_t required_data_size =
        plane_size_multiplication_safe
        ? plane.pitch * static_cast<std::uintptr_t>(kProbeBitmapHeight)
        : 0;
    const std::uintptr_t data_address =
        reinterpret_cast<std::uintptr_t>(plane.data);
    const bool plane_address_addition_safe = plane.data != nullptr &&
        required_data_size <=
            (std::numeric_limits<std::uintptr_t>::max)() - data_address;
    const bool exact_data = get_data_call_completed &&
        get_data_returned && get_data_seh == 0 &&
        view.format == 1U && view.use == 0U &&
        view.flags == 0U && view.level_count == 1U &&
        view.raw_plane_count == 1U &&
        view.planes == &view.plane0 && view.palette == nullptr &&
        plane_read &&
        plane.width == kSize.width && plane.height == kSize.height &&
        plane.pitch >= kRowBytes && plane_size_multiplication_safe &&
        plane.data_size >= required_data_size &&
        plane_address_addition_safe;
    log_line(
        "rawImageLifecycle data call=%u returned=%u seh=0x%08lX exact=%u format=%u use=0x%08X flags=0x%02X levels=%u planes=%u planeTable=%p inlinePlane=%p palette=%p planeRead=%u plane={width:%u height:%u pitch:%llu size:%llu data:%p}",
        get_data_call_completed ? 1U : 0U,
        get_data_returned ? 1U : 0U,
        get_data_seh,
        exact_data ? 1U : 0U,
        static_cast<unsigned>(view.format),
        static_cast<unsigned>(view.use),
        static_cast<unsigned>(view.flags),
        static_cast<unsigned>(view.level_count),
        static_cast<unsigned>(view.raw_plane_count),
        view.planes,
        static_cast<void*>(&view.plane0),
        view.palette,
        plane_read ? 1U : 0U,
        static_cast<unsigned>(plane.width),
        static_cast<unsigned>(plane.height),
        static_cast<unsigned long long>(plane.pitch),
        static_cast<unsigned long long>(plane.data_size),
        plane.data);

    constexpr std::size_t kProbeRowBytes =
        static_cast<std::size_t>(kProbeBitmapWidth) * 4U;
    std::array<std::byte, kProbeRowBytes> expected_row{};
    std::array<std::byte, kProbeRowBytes> observed_row{};
    bool pixels_written = exact_data;
    bool pixels_verified = exact_data;
    std::uint64_t expected_checksum = 1469598103934665603ULL;
    std::uint64_t observed_checksum = 1469598103934665603ULL;
    for (std::uint32_t y = 0;
         exact_data && y < kSize.height;
         ++y) {
        for (std::uint32_t x = 0; x < kSize.width; ++x) {
            const Rgba8Pixel pixel = rgba8_probe_pixel(icon_index, x, y);
            const std::size_t offset = static_cast<std::size_t>(x) * 4U;
            expected_row[offset + 0] = static_cast<std::byte>(pixel.red);
            expected_row[offset + 1] = static_cast<std::byte>(pixel.green);
            expected_row[offset + 2] = static_cast<std::byte>(pixel.blue);
            expected_row[offset + 3] = static_cast<std::byte>(pixel.alpha);
        }
        for (const std::byte value : expected_row) {
            expected_checksum ^= static_cast<std::uint8_t>(value);
            expected_checksum *= 1099511628211ULL;
        }
        std::byte* const destination = plane.data +
            static_cast<std::uintptr_t>(y) * plane.pitch;
        if (!guarded_copy(
                destination, expected_row.data(), expected_row.size())) {
            pixels_written = false;
            pixels_verified = false;
            break;
        }
        observed_row.fill(std::byte{});
        if (!guarded_copy(
                observed_row.data(), destination, observed_row.size())) {
            pixels_verified = false;
            break;
        }
        if (observed_row != expected_row) {
            pixels_verified = false;
            break;
        }
        for (const std::byte value : observed_row) {
            observed_checksum ^= static_cast<std::uint8_t>(value);
            observed_checksum *= 1099511628211ULL;
        }
    }
    pixels_verified = pixels_verified &&
        expected_checksum == observed_checksum;
    log_line(
        "rawImageLifecycle pixels icon=%zu id=%d written=%u verified=%u rows=%u rowBytes=%zu checksum={expected:0x%016llX observed:0x%016llX} alpha=diagnostic-ramp exposed=0 bound=0",
        icon_index,
        kRgba8IconIds[icon_index],
        pixels_written ? 1U : 0U,
        pixels_verified ? 1U : 0U,
        static_cast<unsigned>(kSize.height),
        kProbeRowBytes,
        static_cast<unsigned long long>(expected_checksum),
        static_cast<unsigned long long>(observed_checksum));

    bool view_cleared{};
    unsigned long clear_seh{};
    if (get_data_call_completed) {
        view_cleared = guarded_clear_raw_image_data(
            clear, view, clear_seh);
    }

    bool image_released{};
    unsigned long release_seh{};
    bool resource_probe_attempted{};
    bool resource_probe_completed{};
    bool visual_resource_retained{};
    bool uncertain_objects_retained{};
    if (kEnableNativeImageResourceLifecycleProbe &&
        exact_data && pixels_written && pixels_verified && view_cleared) {
        resource_probe_attempted = true;

        constexpr std::array<std::uint8_t, 6> kRawImageGetTypeEntry{
            0xB8, 0x02, 0x00, 0x00, 0x00, 0xC3};
        constexpr std::array<std::uint8_t, 33>
            kScaleformImageResourceCtorEntry{
                0x48, 0x89, 0x4C, 0x24, 0x08, 0x57, 0x48, 0x83,
                0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE,
                0xFF, 0xFF, 0xFF, 0x48, 0x89, 0x5C, 0x24, 0x48,
                0x49, 0x8B, 0xC0, 0x48, 0x8B, 0xDA, 0x48, 0x8B,
                0xF9};
        constexpr std::array<std::uint8_t, 14>
            kScaleformImageResourceAddRefEntry{
                0x48, 0x83, 0xC1, 0x08, 0xBA, 0x01, 0x00,
                0x00, 0x00, 0xE9, 0x32, 0x50, 0x00, 0x00};
        constexpr std::array<std::uint8_t, 16>
            kScaleformImageResourceReleaseEntry{
                0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
                0xD9, 0x83, 0xCA, 0xFF, 0x48, 0x83, 0xC1, 0x08};
        constexpr std::array<std::uint8_t, 25>
            kScaleformImageResourceDeletingDestructorEntry{
                0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
                0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48,
                0x89, 0x5C, 0x24, 0x40, 0x8B, 0xFA, 0x48, 0x8B,
                0xD9};
        // The RIP displacement necessarily changes when the global-heap data
        // slot moves. Validate the instruction semantically against the
        // profile-selected slot, then validate its unchanged allocation tail.
        constexpr std::array<std::uint8_t, 14>
            kScaleformImageResourceAllocationTail{
                0x48, 0x8B, 0x01, 0x45, 0x33, 0xC0, 0xBA,
                0x98, 0x00, 0x00, 0x00, 0xFF, 0x50, 0x50};

        void* raw_type_slot{};
        void* resource_deleting_destructor{};
        const bool static_fields_read =
            guarded_read(
                static_cast<const std::byte*>(vtable) + 0x18,
                raw_type_slot) &&
            guarded_read(
                g_game.base + kScaleformImageResourceVtable271Rva,
                resource_deleting_destructor);
        std::uintptr_t allocation_heap_rva{};
        const bool exact_allocation_call =
            resolve_rip_relative_data_target(
                kScaleformImageResourceAllocationCall271Rva,
                allocation_heap_rva) &&
            allocation_heap_rva ==
                static_cast<std::uintptr_t>(kScaleformGlobalHeap271Rva) &&
            entry_matches(
                static_cast<std::uintptr_t>(
                    kScaleformImageResourceAllocationCall271Rva) + 7U,
                kScaleformImageResourceAllocationTail);
        const bool exact_resource_interfaces = static_fields_read &&
            raw_type_slot == g_game.base + kRawImageGetType271Rva &&
            resource_deleting_destructor ==
                g_game.base +
                    kScaleformImageResourceDeletingDestructor271Rva &&
            entry_matches(
                kRawImageGetType271Rva,
                kRawImageGetTypeEntry) &&
            entry_matches(
                kScaleformImageResourceCtor271Rva,
                kScaleformImageResourceCtorEntry) &&
            entry_matches(
                kScaleformImageResourceAddRef271Rva,
                kScaleformImageResourceAddRefEntry) &&
            entry_matches(
                kScaleformImageResourceRelease271Rva,
                kScaleformImageResourceReleaseEntry) &&
            entry_matches(
                kScaleformImageResourceDeletingDestructor271Rva,
                kScaleformImageResourceDeletingDestructorEntry) &&
            exact_allocation_call &&
            image_contains(kScaleformImageResourceVtable271Rva, 0x08) &&
            image_contains(kScaleformImageDelegateVtable271Rva, 0x08) &&
            image_contains(kScaleformGlobalHeap271Rva, sizeof(void*));
        log_line(
            "nativeImageResourceLifecycle begin thread=%lu profile=%s interfacesExact=%u raw=%p rawType=%p/0x%llX deletingDestructor=%p/0x%llX exposed=0 bound=0",
            static_cast<unsigned long>(GetCurrentThreadId()),
            tabs_build().version,
            exact_resource_interfaces ? 1U : 0U,
            image,
            raw_type_slot,
            static_cast<unsigned long long>(
                locate_game_pointer(raw_type_slot).rva),
            resource_deleting_destructor,
            static_cast<unsigned long long>(
                locate_game_pointer(resource_deleting_destructor).rva));

        if (exact_resource_interfaces) {
            void* heap{};
            void* heap_vtable{};
            void* allocation_slot{};
            const bool heap_fields_read =
                guarded_read(
                    g_game.base + kScaleformGlobalHeap271Rva,
                    heap) &&
                guarded_read(heap, heap_vtable) &&
                guarded_read(
                    static_cast<const std::byte*>(heap_vtable) + 0x50,
                    allocation_slot);
            const GamePointerLocation allocation_location =
                locate_game_pointer(allocation_slot);
            const bool exact_heap = heap_fields_read && heap &&
                heap_vtable && allocation_slot &&
                allocation_location.in_image;
            log_line(
                "nativeImageResourceLifecycle heap reads=%u exact=%u heap=%p vtable=%p allocate=%p/0x%llX size=0x%zX arg3=null",
                heap_fields_read ? 1U : 0U,
                exact_heap ? 1U : 0U,
                heap,
                heap_vtable,
                allocation_slot,
                static_cast<unsigned long long>(allocation_location.rva),
                kScaleformImageResourceSize);

            if (exact_heap) {
                void* storage{};
                unsigned long allocation_seh{};
                const bool allocated =
                    guarded_allocate_scaleform_image_resource(
                        reinterpret_cast<NativeHeapAllocFn>(allocation_slot),
                        heap,
                        storage,
                        allocation_seh);
                log_line(
                    "nativeImageResourceLifecycle allocation completed=%u storage=%p seh=0x%08lX",
                    allocated ? 1U : 0U,
                    storage,
                    allocation_seh);

                if (allocated) {
                    void* constructed_resource{};
                    unsigned long constructor_seh{};
                    const bool constructed =
                        guarded_construct_scaleform_image_resource(
                            reinterpret_cast<
                                ScaleformImageResourceCtorFn>(
                                    g_game.base +
                                    kScaleformImageResourceCtor271Rva),
                            storage,
                            image,
                            kRawImageResourceProbeNames[icon_index].data(),
                            constructed_resource,
                            constructor_seh);

                    void* resource_vtable{};
                    std::uint32_t resource_refcount{};
                    void* exposed_image{};
                    void* delegate_vtable{};
                    std::uint32_t delegate_refcount{};
                    void* delegated_image{};
                    std::uint32_t resource_kind{};
                    std::uint32_t raw_refcount_after_constructor{};
                    const bool resource_fields_read = constructed &&
                        guarded_read(
                            constructed_resource,
                            resource_vtable) &&
                        guarded_read(
                            static_cast<const std::byte*>(
                                constructed_resource) + 0x08,
                            resource_refcount) &&
                        guarded_read(
                            static_cast<const std::byte*>(
                                constructed_resource) + 0x18,
                            exposed_image) &&
                        guarded_read(
                            static_cast<const std::byte*>(
                                constructed_resource) + 0x20,
                            delegate_vtable) &&
                        guarded_read(
                            static_cast<const std::byte*>(
                                constructed_resource) + 0x28,
                            delegate_refcount) &&
                        guarded_read(
                            static_cast<const std::byte*>(
                                constructed_resource) + 0x48,
                            delegated_image) &&
                        guarded_read(
                            static_cast<const std::byte*>(
                                constructed_resource) + 0x60,
                            resource_kind) &&
                        guarded_read(
                            static_cast<const std::byte*>(image) + 0x08,
                            raw_refcount_after_constructor);
                    const bool exact_resource = resource_fields_read &&
                        constructed_resource == storage &&
                        resource_vtable ==
                            g_game.base +
                                kScaleformImageResourceVtable271Rva &&
                        resource_refcount == 1U &&
                        exposed_image == image &&
                        delegate_vtable ==
                            g_game.base +
                                kScaleformImageDelegateVtable271Rva &&
                        delegate_refcount == 1U &&
                        delegated_image == nullptr &&
                        resource_kind == 1U &&
                        raw_refcount_after_constructor == 2U;
                    log_line(
                        "nativeImageResourceLifecycle constructor completed=%u result=%p storage=%p seh=0x%08lX fieldsRead=%u exact=%u resource={vtable:%p/0x%llX ref:%u exposed:%p delegateVtable:%p/0x%llX delegateRef:%u delegated:%p kind:%u} rawRef=%u exposedToUi=0 bound=0",
                        constructed ? 1U : 0U,
                        constructed_resource,
                        storage,
                        constructor_seh,
                        resource_fields_read ? 1U : 0U,
                        exact_resource ? 1U : 0U,
                        resource_vtable,
                        static_cast<unsigned long long>(
                            locate_game_pointer(resource_vtable).rva),
                        static_cast<unsigned>(resource_refcount),
                        exposed_image,
                        delegate_vtable,
                        static_cast<unsigned long long>(
                            locate_game_pointer(delegate_vtable).rva),
                        static_cast<unsigned>(delegate_refcount),
                        delegated_image,
                        static_cast<unsigned>(resource_kind),
                        static_cast<unsigned>(
                            raw_refcount_after_constructor));

                    if (exact_resource) {
                        unsigned long creator_release_seh{};
                        const bool creator_released =
                            guarded_release_raw_image(
                                reinterpret_cast<RawImageReleaseFn>(
                                    release_slot),
                                image,
                                creator_release_seh);
                        std::uint32_t raw_refcount_after_creator_release{};
                        const bool remaining_ref_read = creator_released &&
                            guarded_read(
                                static_cast<const std::byte*>(image) +
                                    0x08,
                                raw_refcount_after_creator_release);
                        const bool ownership_handoff_exact =
                            remaining_ref_read &&
                            raw_refcount_after_creator_release == 1U;
                        log_line(
                            "nativeImageResourceLifecycle ownership creatorReleased=%u seh=0x%08lX remainingRefRead=%u rawRef=%u exact=%u",
                            creator_released ? 1U : 0U,
                            creator_release_seh,
                            remaining_ref_read ? 1U : 0U,
                            static_cast<unsigned>(
                                raw_refcount_after_creator_release),
                            ownership_handoff_exact ? 1U : 0U);

                        if (ownership_handoff_exact) {
                            // Stage this exact root. The outer catalogue gate
                            // publishes none of the resources until every
                            // requested entry has passed this same
                            // ownership handoff.
                            retained.image = image;
                            retained.resource = constructed_resource;
                            retained.checksum = observed_checksum;
                            retained.ready = true;
                            visual_resource_retained = true;
                            resource_probe_completed = true;
                            log_line(
                                "rgba8Resource staged icon=%zu id=%d resource=%p image=%p resourceRef=1 rawRef=1 checksum=0x%016llX intentionalProcessLifetime=1 exposed=0 bound=0",
                                icon_index,
                                kRgba8IconIds[icon_index],
                                constructed_resource,
                                image,
                                static_cast<unsigned long long>(
                                    observed_checksum));
                        } else {
                            uncertain_objects_retained = true;
                        }
                    } else {
                        // Construction may have partially transferred
                        // ownership even if it faulted or a postcondition did
                        // not match. Retain both bounded one-shot allocations
                        // rather than guessing which destructor is safe.
                        uncertain_objects_retained = true;
                    }
                }
            }
        }
    }

    // If the wrapper experiment never obtained storage, RawImage ownership is
    // still exactly the already-proven single creator reference. Preserve the
    // original cleanup path. An uncertain constructed object deliberately
    // blocks this fallback to avoid a double Release.
    if (get_data_call_completed && view_cleared &&
        !image_released && !visual_resource_retained &&
        !uncertain_objects_retained) {
        image_released = guarded_release_raw_image(
            reinterpret_cast<RawImageReleaseFn>(release_slot),
            image,
            release_seh);
    }
    const bool completed = exact_data && pixels_written &&
        pixels_verified && view_cleared &&
        (image_released || visual_resource_retained) &&
        (!resource_probe_attempted || resource_probe_completed);
    log_line(
        "rawImageLifecycle cleanup viewCleared=%u clearSeh=0x%08lX imageReleased=%u fallbackReleaseSeh=0x%08lX resourceProbe={attempted:%u completed:%u} visualCacheRetained=%u uncertainRetained=%u completed=%u exposed=0 bound=0",
        view_cleared ? 1U : 0U,
        clear_seh,
        image_released ? 1U : 0U,
        release_seh,
        resource_probe_attempted ? 1U : 0U,
        resource_probe_completed ? 1U : 0U,
        visual_resource_retained ? 1U : 0U,
        uncertain_objects_retained ? 1U : 0U,
        completed ? 1U : 0U);
    if (!completed) {
        log_failure(
            "RawImage/ImageResource lifecycle gate did not complete; renderer publication remains disabled");
    }
}

[[nodiscard]] bool construct_rgba8_catalogue(
    void* top,
    bool require_active_runtime,
    bool publish_ready,
    bool disable_on_failure) noexcept {
    const bool valid_context = require_active_runtime
        ? top && g_runtime.active && g_runtime.top == top &&
            validate_top(top)
        : top && validate_top(top);
    if (!kEnableRawImageLifecycleProbe || !valid_context) return false;

    const std::size_t custom_count = active_custom_tab_count();
    // These two vectors are private construction-transaction scratch, not
    // another runtime catalogue. Their exact registered size lets us retain
    // the all-or-nothing publication gate without imposing the public ceiling
    // on every process. All persistent state is owned by tabs[index].
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    auto& expected_checksums = g_rgba8_expected_checksums_scratch;
    auto& staged = g_rgba8_staged_resources_scratch;
    if (expected_checksums.size() != custom_count ||
        staged.size() != custom_count) {
        if (disable_on_failure) {
            g_rgba8_catalogue_ready.store(false, std::memory_order_release);
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
        }
        log_failure(
            "RGBA8 catalogue scratch does not match the registered tab count (%zu/%zu expected=%zu); publication refused without allocating on the UI thread",
            expected_checksums.size(), staged.size(), custom_count);
        return false;
    }
    std::fill(expected_checksums.begin(), expected_checksums.end(), 0U);
    std::fill(staged.begin(), staged.end(), RetainedRgba8Resource{});
#else
    std::vector<std::uint64_t> expected_checksums;
    std::vector<RetainedRgba8Resource> staged;
    try {
        expected_checksums.resize(custom_count);
        staged.resize(custom_count);
    } catch (...) {
        if (disable_on_failure) {
            g_rgba8_catalogue_ready.store(false, std::memory_order_release);
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
        }
        log_failure(
            "RGBA8 catalogue could not allocate an exact-sized %zu-tab construction transaction; raster publication remains disabled",
            custom_count);
        return false;
    }
#endif

    const std::uint64_t catalogue_started_qpc = query_performance_ticks();
    std::uint64_t slowest_icon_microseconds{};
    std::size_t slowest_icon{};
    std::size_t ready_count{};
    std::size_t raster_count{};
    bool procedural_catalogue_valid{true};
    for (std::size_t icon = 0;
         icon < custom_count && procedural_catalogue_valid;
         ++icon) {
        if (!catalogue_icon_requires_raster(icon)) continue;
        expected_checksums[icon] = rgba8_probe_icon_checksum(icon);
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
        procedural_catalogue_valid = expected_checksums[icon] != 0;
#endif
        ++raster_count;
    }
#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
    procedural_catalogue_valid = procedural_catalogue_valid &&
        rgba8_probe_catalogue_is_unique_and_alpha_profiled(
            expected_checksums);
#endif
    bool all_ready{procedural_catalogue_valid};
    for (std::size_t icon = 0;
         icon < custom_count;
         ++icon) {
        if (!catalogue_icon_requires_raster(icon)) continue;
        const std::uint64_t icon_started_qpc = query_performance_ticks();
        run_raw_image_lifecycle_probe_one(top, icon, staged[icon]);
        const std::uint64_t icon_microseconds = elapsed_qpc_microseconds(
            icon_started_qpc, query_performance_ticks());
        if (icon_microseconds > slowest_icon_microseconds) {
            slowest_icon_microseconds = icon_microseconds;
            slowest_icon = icon;
        }
        const bool entry_ready = staged[icon].ready &&
            staged[icon].image && staged[icon].resource &&
            staged[icon].checksum == expected_checksums[icon];
        all_ready = all_ready && entry_ready;
        if (entry_ready) {
            ++ready_count;
        }
    }

    bool identities_unique{true};
    for (std::size_t icon = 0;
         icon < custom_count;
         ++icon) {
        if (!catalogue_icon_requires_raster(icon)) continue;
        for (std::size_t other = 0; other < icon; ++other) {
            if (!catalogue_icon_requires_raster(other)) continue;
            identities_unique = identities_unique &&
                staged[icon].image != staged[other].image &&
                staged[icon].resource != staged[other].resource;
        }
    }
    all_ready = all_ready && identities_unique;
    std::uint64_t catalogue_microseconds = elapsed_qpc_microseconds(
        catalogue_started_qpc, query_performance_ticks());
    std::uint64_t average_icon_microseconds = raster_count == 0
        ? 0
        : catalogue_microseconds / raster_count;

    if (!all_ready) {
        // Successfully staged entries retain their exact process-lifetime root
        // rather than guessing teardown after a sibling failed. None becomes
        // visible to the lookup detour, so the whole catalogue falls back to
        // the proven vector icon as one atomic feature gate.
        if (disable_on_failure) {
            g_rgba8_catalogue_ready.store(false, std::memory_order_release);
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
        }
        for (std::size_t icon = 0;
             icon < custom_count;
             ++icon) {
            if (!catalogue_icon_requires_raster(icon)) continue;
            log_line(
                "rgba8Catalogue stage icon=%zu id=%d ready=%u resource=%p image=%p checksum=0x%016llX published=0",
                icon,
                kRgba8IconIds[icon],
                staged[icon].ready ? 1U : 0U,
                staged[icon].resource,
                staged[icon].image,
                static_cast<unsigned long long>(staged[icon].checksum));
        }
        log_failure(
            "RGBA8 catalogue did not pass its all-ready/identity gate (procedural=%u ready=%zu/%zu unique=%u timingUs={total:%llu average:%llu slowest:%llu icon:%zu}); no custom raster resource was published",
            procedural_catalogue_valid ? 1U : 0U,
            ready_count,
            raster_count,
            identities_unique ? 1U : 0U,
            static_cast<unsigned long long>(catalogue_microseconds),
            static_cast<unsigned long long>(average_icon_microseconds),
            static_cast<unsigned long long>(slowest_icon_microseconds),
            slowest_icon);
        return false;
    }

    // Publish every identity first, then release one all-ready flag. No
    // arbitrary presentation reader can observe a partially published table.
    for (std::size_t icon = 0;
         icon < custom_count;
         ++icon) {
        if (!catalogue_icon_requires_raster(icon)) continue;
        publish_retained_rgba8(icon, staged[icon]);
        log_line(
            "rgba8Catalogue publish slot=%zu id=%d resource=%p image=%p resourceRef=1 rawRef=1 checksum=0x%016llX identity=unique",
            icon,
            kRgba8IconIds[icon],
            staged[icon].resource,
            staged[icon].image,
            static_cast<unsigned long long>(staged[icon].checksum));
    }
    if (publish_ready) {
        g_rgba8_catalogue_ready.store(true, std::memory_order_release);
    }
    catalogue_microseconds = elapsed_qpc_microseconds(
        catalogue_started_qpc, query_performance_ticks());
    average_icon_microseconds = raster_count == 0
        ? 0
        : catalogue_microseconds / raster_count;
    log_line(
        "STRESS catalogue ready count=%zu ready=%zu alpha=straight-vs-premultiplied-ramp publication=atomic identity=one-to-one unique=1 timingUs={total:%llu average:%llu slowest:%llu icon:%zu}",
        raster_count,
        ready_count,
        static_cast<unsigned long long>(catalogue_microseconds),
        static_cast<unsigned long long>(average_icon_microseconds),
        static_cast<unsigned long long>(slowest_icon_microseconds),
        slowest_icon);
    log_rgba8_lifecycle_snapshot("catalogue-published", top);
    return true;
}

void run_raw_image_lifecycle_probe(void* top) noexcept {
    if (!kEnableRawImageLifecycleProbe || !top || !g_runtime.active ||
        g_runtime.top != top || !validate_top(top)) {
        return;
    }
    if (g_raw_image_lifecycle_probe_attempted.exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    (void)construct_rgba8_catalogue(top, true, true, true);
}

bool resolve_relative_call_target(
    std::uintptr_t call_rva,
    std::uintptr_t& target_rva) noexcept {
    target_rva = 0;
    if (!image_contains(call_rva, 5)) return false;

    std::uint8_t opcode{};
    std::int32_t displacement{};
    if (!guarded_read(g_game.base + call_rva, opcode) || opcode != 0xE8 ||
        !guarded_read(g_game.base + call_rva + 1, displacement)) {
        return false;
    }
    const std::int64_t target = static_cast<std::int64_t>(call_rva) + 5 +
        static_cast<std::int64_t>(displacement);
    if (target < 0 ||
        static_cast<std::uint64_t>(target) >= g_game.size) {
        return false;
    }
    target_rva = static_cast<std::uintptr_t>(target);
    return true;
}

bool validate_official_host() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    g_research_prepare_carrier =
        &erui::native::prepare_native_tab_graphics_carrier;
    g_research_verify_carrier =
        &erui::native::verify_native_tab_graphics_carrier;
    if (erui::native::native_tab_graphics_carrier_version() != 2) {
        log_line("ERROR: native Graphics carrier V2 bridge unavailable");
        return false;
    }
    return true;
#else
    std::array<wchar_t, 32768> host_path{};
    HMODULE const host = GetModuleHandleW(L"ERNativeUI.dll");
    const DWORD length = host
        ? GetModuleFileNameW(
              host,
              host_path.data(),
              static_cast<DWORD>(host_path.size()))
        : 0;
    if (length == 0 || static_cast<std::size_t>(length) >= host_path.size()) {
        log_line("ERROR: loaded ERNativeUI host path unavailable");
        return false;
    }

    std::array<char, 32768> utf8_path{};
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        host_path.data(),
        static_cast<int>(length),
        utf8_path.data(),
        static_cast<int>(utf8_path.size() - 1),
        nullptr,
        nullptr);
    if (converted <= 0) {
        log_line("ERROR: loaded ERNativeUI host path conversion failed");
        return false;
    }
    utf8_path[static_cast<std::size_t>(converted)] = '\0';
    log_line("Loaded ERNativeUI host path=%s", utf8_path.data());

    constexpr wchar_t kExpectedSuffix[] =
        L"\\DemoConfigMenu\\ERNativeUI.dll";
    const std::size_t path_length = static_cast<std::size_t>(length);
    const std::size_t suffix_length = _countof(kExpectedSuffix) - 1;
    if (path_length < suffix_length ||
        _wcsicmp(
            host_path.data() + path_length - suffix_length,
            kExpectedSuffix) != 0) {
        log_line(
            "ERROR: viewport probe requires the validated official ERNativeUI host under DemoConfigMenu; no hooks installed");
        return false;
    }

    using ResearchMarkerFn = std::uint32_t(__cdecl*)();
    ResearchMarkerFn const marker = reinterpret_cast<ResearchMarkerFn>(
        GetProcAddress(host, kTabContentResearchMarkerExport));
    if (!marker || marker() != kTabContentResearchMarker) {
        log_line(
            "ERROR: viewport content proof requires the exact research host marker; no hooks installed");
        return false;
    }
    log_line("Validated ERNativeUI tab-content research host marker");
    g_research_prepare_carrier = reinterpret_cast<ResearchPrepareCarrierFn>(
        GetProcAddress(host, "ERUI_ResearchPrepareGraphicsCarrierV2"));
    g_research_verify_carrier = reinterpret_cast<ResearchVerifyCarrierFn>(
        GetProcAddress(host, "ERUI_ResearchVerifyGraphicsCarrierV2"));
    ResearchCarrierVersionFn const carrier_version =
        reinterpret_cast<ResearchCarrierVersionFn>(GetProcAddress(
            host, "ERUI_ResearchGraphicsCarrierVersionV2"));
    if (!g_research_prepare_carrier || !g_research_verify_carrier ||
        !carrier_version || carrier_version() != 2) {
        log_line("ERROR: exact Graphics carrier V2 exports missing; no hooks installed");
        return false;
    }
    log_line("Validated Graphics carrier research protocol V2");
    return true;
#endif
}

bool copy_title(
    const wchar_t* source,
    std::array<wchar_t, kTitleCapacity>& destination) noexcept;

std::byte* record_at(void* top, std::size_t physical) noexcept {
    if (!top || physical >= kPhysicalTabCapacity) return nullptr;
    return static_cast<std::byte*>(top) + kCategoryStorageOffset +
        physical * kCategoryRecordSize;
}

struct LiveTabGeometry final {
    std::size_t records{};
    std::size_t active{};
    std::size_t authored{};
    std::size_t selected{};
};

bool read_live_tab_geometry(
    void* top,
    LiveTabGeometry& geometry) noexcept {
    geometry = {};
    if (!top) return false;
    std::uint64_t record_count{};
    std::int32_t active{-1};
    std::int32_t selected{-1};
    std::int32_t horizontal{-1};
    std::int32_t vertical{-1};
    const auto* const selector =
        static_cast<const std::byte*>(top) + kSelectorOffset;
    void* selector_vtable{};
    if (!guarded_read(selector, selector_vtable) ||
        selector_vtable != g_game.base + kSelectorVtableRva ||
        !guarded_read(
            static_cast<const std::byte*>(top) + kCategoryCountOffset,
            record_count) ||
        !guarded_read(selector + kSelectorActiveCountOffset, active) ||
        !guarded_read(selector + kSelectorSelectedIndexOffset, selected) ||
        !guarded_read(
            selector + kSelectorHorizontalExtentOffset, horizontal) ||
        !guarded_read(selector + kSelectorVerticalExtentOffset, vertical)) {
        return false;
    }
    erui::detail::NativeTabLiveGeometry validated{};
    if (!erui::detail::validate_native_tab_live_geometry(
            record_count,
            active,
            selected,
            horizontal,
            vertical,
            validated)) {
        return false;
    }
    geometry.active = validated.active_slot_count;
    geometry.records = static_cast<std::size_t>(record_count);
    geometry.authored = validated.authored_cell_count;
    geometry.selected = validated.selected_slot;
    return true;
}

bool extra_records_are_graphics_carriers(
    void* top,
    std::size_t active) noexcept {
    if (!top || active < kStockLogicalTabCount ||
        active > kPhysicalTabCapacity) {
        return false;
    }
    if (active == kStockLogicalTabCount) return true;

    std::int32_t graphics_category{-1};
    const wchar_t* graphics_title{};
    std::array<std::byte, kPresentationSize> graphics_presentation{};
    const std::byte* const graphics = record_at(
        top, kStockLogicalTabCount - 1U);
    if (!guarded_read(graphics + kTitleOffset, graphics_title) ||
        !graphics_title ||
        !guarded_read(graphics + kCategoryIdOffset, graphics_category) ||
        graphics_category != kExpectedStockCategories.back() ||
        !guarded_copy(
            graphics_presentation.data(),
            graphics + kPresentationOffset,
            graphics_presentation.size())) {
        return false;
    }

    std::array<wchar_t, kTitleCapacity> expected_title{};
    if (!copy_title(graphics_title, expected_title)) return false;
    for (std::size_t physical = kStockLogicalTabCount;
         physical < active;
         ++physical) {
        const std::byte* const record = record_at(top, physical);
        const wchar_t* title{};
        std::int32_t category{-1};
        std::array<std::byte, kPresentationSize> presentation{};
        std::array<wchar_t, kTitleCapacity> title_copy{};
        if (!guarded_read(record + kTitleOffset, title) || !title ||
            !copy_title(title, title_copy) || title_copy != expected_title ||
            !guarded_read(record + kCategoryIdOffset, category) ||
            category != graphics_category ||
            !guarded_copy(
                presentation.data(),
                record + kPresentationOffset,
                presentation.size()) ||
            presentation != graphics_presentation) {
            return false;
        }
    }
    return true;
}

bool validate_top(void* top) noexcept {
    if (!top) return false;
    void* vtable{};
    LiveTabGeometry geometry{};
    if (!guarded_read(top, vtable) ||
        vtable != g_game.base + kTopVtableRva ||
        !read_live_tab_geometry(top, geometry)) {
        return false;
    }

    for (std::size_t physical = 0; physical < geometry.active; ++physical) {
        void* record_vtable{};
        if (!guarded_read(record_at(top, physical), record_vtable) ||
            record_vtable != g_game.base + kCategoryRecordVtableRva) {
            return false;
        }
    }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_native_tab_runtime) return false;
    const std::size_t epoch_count =
        g_native_tab_runtime->active_physical_slot_count();
    // The transformed Scaleform movie and its selector layout are immutable
    // for this process. A different D0 while the prior epoch is still bound is
    // therefore an identity/lifecycle mismatch, never an in-place resize.
    if (epoch_count != 0U) return epoch_count == geometry.active;
    if (!extra_records_are_graphics_carriers(top, geometry.active)) {
        log_failure(
            "live Configuration exposes %zu active tabs, but records beyond the eight stock routes are not exact Graphics carriers; custom-tab mutation remains disabled for this movie epoch",
            geometry.active);
        return false;
    }
#else
    if (geometry.active != kStockLogicalTabCount) return false;
#endif
    return true;
}

bool copy_title(
    const wchar_t* source,
    std::array<wchar_t, kTitleCapacity>& destination) noexcept {
    destination.fill(L'\0');
    if (!source) return false;
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0; index + 1 < destination.size(); ++index) {
            destination[index] = source[index];
            if (source[index] == L'\0') return index != 0;
        }
        destination.back() = L'\0';
        return false;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        destination.fill(L'\0');
        return false;
    }
#endif
}

bool make_synthetic_icon_descriptor(
    std::array<std::byte, kPresentationSize>& descriptor,
    std::int32_t icon_id) noexcept {
    if (!g_make_icon_descriptor) return false;
    descriptor.fill(std::byte{});

    void* result{};
#if defined(_MSC_VER)
    __try {
#endif
        result = g_make_icon_descriptor(
            descriptor.data(), icon_id);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = nullptr;
    }
#endif
    std::uint8_t resource_family{};
    std::int32_t resource_id{-1};
    std::memcpy(
        &resource_family,
        descriptor.data() + 0x38,
        sizeof(resource_family));
    std::memcpy(
        &resource_id,
        descriptor.data() + 0x3C,
        sizeof(resource_id));
    return result == descriptor.data() && resource_family == 0 &&
        resource_id == icon_id;
}

bool snapshot_record_fields(
    void* top,
    std::array<RecordFields, kPhysicalTabCapacity>& fields) noexcept {
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        const std::byte* const record = record_at(top, physical);
        if (!guarded_read(record + kTitleOffset, fields[physical].title) ||
            !guarded_read(
                record + kCategoryIdOffset,
                fields[physical].category) ||
            !guarded_copy(
                fields[physical].presentation.data(),
                record + kPresentationOffset,
                kPresentationSize)) {
            return false;
        }
    }
    return true;
}

bool write_record_fields(
    void* top,
    const std::array<RecordFields, kPhysicalTabCapacity>& fields) noexcept {
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        std::byte* const record = record_at(top, physical);
        if (!guarded_copy(
                record + kTitleOffset,
                &fields[physical].title,
                sizeof(fields[physical].title)) ||
            !guarded_copy(
                record + kCategoryIdOffset,
                &fields[physical].category,
                sizeof(fields[physical].category)) ||
            !guarded_copy(
                record + kPresentationOffset,
                fields[physical].presentation.data(),
                kPresentationSize)) {
            return false;
        }
    }
    return true;
}

bool build_mapping_fields(
    const TabViewport& viewport,
    std::array<RecordFields, kPhysicalTabCapacity>& fields) noexcept {
    if (!viewport.initialized() ||
        viewport.visible_slot_count() != active_physical_tab_count()) {
        return false;
    }
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        std::size_t logical{};
        if (!viewport.logical_for_physical(physical, logical) ||
            logical >= active_logical_tab_count()) {
            return false;
        }
        const LogicalTab* const tab = runtime_logical_tab(logical);
        if (!tab) return false;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        fields[physical].title = mapping_logical_title(*tab, logical);
#else
        fields[physical].title = tab->title.data();
#endif
        fields[physical].category = tab->category;
        fields[physical].presentation = tab->presentation;
        if (tab_overflow_indicator(viewport, physical) !=
            TabOverflowIndicator::none) {
            fields[physical].presentation =
                g_runtime.overflow_presentation;
        }
    }
    return true;
}

bool records_match_mapping(void* top, const TabViewport& viewport) noexcept {
    std::array<RecordFields, kPhysicalTabCapacity> expected{};
    std::array<RecordFields, kPhysicalTabCapacity> actual{};
    if (!build_mapping_fields(viewport, expected) ||
        !snapshot_record_fields(top, actual)) {
        return false;
    }
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        if (actual[physical].title != expected[physical].title ||
            actual[physical].category != expected[physical].category ||
            actual[physical].presentation != expected[physical].presentation) {
            return false;
        }
    }
    return true;
}

const char* stock_epoch_match_name(
    erui::detail::StockTabEpochMatch match) noexcept {
    using erui::detail::StockTabEpochMatch;
    switch (match) {
    case StockTabEpochMatch::exact: return "exact";
    case StockTabEpochMatch::missing_title_pointer:
        return "missing-title-pointer";
    case StockTabEpochMatch::borrowed_title_pointer:
        return "borrowed-title-pointer";
    case StockTabEpochMatch::incomplete_title:
        return "incomplete-title";
    case StockTabEpochMatch::category_mismatch:
        return "category-mismatch";
    case StockTabEpochMatch::title_mismatch: return "title-mismatch";
    case StockTabEpochMatch::presentation_mismatch:
        return "presentation-mismatch";
    }
    return "unknown";
}

bool exact_stock_layout_republished(void* top) noexcept {
    static_assert(
        kStockLogicalTabCount == erui::detail::tab_record_epoch_slot_count);
    static_assert(
        kTitleCapacity == erui::detail::tab_record_epoch_title_capacity);
    static_assert(
        kPresentationSize ==
            erui::detail::tab_record_epoch_presentation_size);

    std::array<RecordFields, kPhysicalTabCapacity> actual{};
    if (!snapshot_record_fields(top, actual) ||
        !extra_records_are_graphics_carriers(
            top, active_physical_tab_count())) {
        return false;
    }

    std::array<
        erui::detail::TabRecordObservation,
        kStockLogicalTabCount> observations{};
    std::array<
        erui::detail::StockTabRecordFingerprint,
        kStockLogicalTabCount> stock{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_logical_tab_catalogue ||
        g_logical_tab_catalogue->tabs.size() !=
            resource_logical_tab_count()) {
        return false;
    }
#else
    std::array<std::uintptr_t, kLogicalTabCount> owned_title_storage{};
    for (std::size_t logical = 0;
         logical < active_logical_tab_count();
         ++logical) {
        const LogicalTab* const tab = runtime_logical_tab(logical);
        if (!tab) return false;
        owned_title_storage[logical] = reinterpret_cast<std::uintptr_t>(
            tab->title.data());
    }
    const std::span<const std::uintptr_t> owned_titles(
        owned_title_storage);
#endif
    for (std::size_t physical = 0;
         physical < kStockLogicalTabCount;
         ++physical) {
        observations[physical].source_title =
            reinterpret_cast<std::uintptr_t>(actual[physical].title);
        observations[physical].title_complete = copy_title(
            actual[physical].title, observations[physical].title);
        observations[physical].category = actual[physical].category;
        observations[physical].presentation =
            actual[physical].presentation;

        const LogicalTab* const tab = runtime_logical_tab(physical);
        if (!tab) return false;
        stock[physical].title = tab->title;
        stock[physical].category = kExpectedStockCategories[physical];
        stock[physical].presentation = tab->presentation;
    }

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const erui::detail::StockTabEpochResult result =
        erui::detail::classify_stock_tab_epoch_with_predicate(
            observations,
            stock,
            [](std::uintptr_t candidate) noexcept {
                for (const LogicalTab& tab :
                     g_logical_tab_catalogue->tabs) {
                    if (candidate == reinterpret_cast<std::uintptr_t>(
                            tab.title.data()) ||
                        candidate == reinterpret_cast<std::uintptr_t>(
                            tab.alternate_title.data())) {
                        return true;
                    }
                }
                return false;
            });
#else
    const erui::detail::StockTabEpochResult result =
        erui::detail::classify_stock_tab_epoch(
            observations, stock, owned_titles);
#endif
    if (result.exact()) return true;

    const std::uint32_t mismatch =
        g_record_mismatch_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (mismatch <= kMaximumRecordMismatchLogs &&
        result.slot < kStockLogicalTabCount) {
        const std::size_t physical = result.slot;
        std::uint32_t actual_token{};
        std::int32_t actual_resource{-1};
        std::uint32_t expected_token{};
        std::int32_t expected_resource{-1};
        std::memcpy(
            &actual_token,
            observations[physical].presentation.data(),
            sizeof(actual_token));
        std::memcpy(
            &actual_resource,
            observations[physical].presentation.data() + 0x3C,
            sizeof(actual_resource));
        std::memcpy(
            &expected_token,
            stock[physical].presentation.data(),
            sizeof(expected_token));
        std::memcpy(
            &expected_resource,
            stock[physical].presentation.data() + 0x3C,
            sizeof(expected_resource));
        log_line(
            "WARNING: same-address record mismatch is not an exact stock reset: reason=%s physical=%zu titleComplete=%u actualTitle=%ls expectedTitle=%ls category=%d/%d presentationMatch=%u iconToken=%u/%u resourceId=%d/%d",
            stock_epoch_match_name(result.match),
            physical,
            observations[physical].title_complete ? 1u : 0u,
            observations[physical].title_complete
                ? observations[physical].title.data()
                : L"<unreadable-or-truncated>",
            stock[physical].title.data(),
            observations[physical].category,
            stock[physical].category,
            observations[physical].presentation ==
                    stock[physical].presentation
                ? 1u : 0u,
            static_cast<unsigned>(actual_token),
            static_cast<unsigned>(expected_token),
            actual_resource,
            expected_resource);
    }
    return false;
}

bool destroy_proxy(ResolvedTabProxy& proxy) noexcept {
    if (!proxy.constructed) return true;
    if (!g_destroy_scaleform_value) return false;
#if defined(_MSC_VER)
    __try {
#endif
        g_destroy_scaleform_value(
            proxy.storage.data() + kSceneObjProxyValueOffset);
        proxy.constructed = false;
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_failure(
            "Scaleform proxy cleanup faulted (seh=0x%08lX)",
            static_cast<unsigned long>(GetExceptionCode()));
        return false;
    }
#endif
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
bool read_slot_growth_movie_fingerprint(
    const void* scene_proxy,
    SlotGrowthMovieFingerprint& fingerprint) noexcept {
    fingerprint = {};
    if (!scene_proxy) return false;

    const auto* const proxy = static_cast<const std::byte*>(scene_proxy);
    const auto* const wrapper = proxy + kSceneObjProxyValueOffset;
    void* proxy_vtable{};
    void* proxy_getter{};
    void* proxy_self0{};
    void* proxy_self1{};
    void* proxy_self2{};
    void* wrapper_vtable{};
    void* owner_link{};
    void* movie_root{};
    void* movie_vtable{};
    void* create_object{};
    DisplayValueView view{};

    if (!guarded_read(proxy, proxy_vtable) ||
        proxy_vtable != g_game.base + kSceneObjProxyVtableRva ||
        !guarded_read(
            static_cast<const std::byte*>(proxy_vtable) + sizeof(void*),
            proxy_getter) ||
        proxy_getter != g_game.base + kSceneObjProxyGetValueRva ||
        !guarded_read(proxy + 0x08, proxy_self0) ||
        !guarded_read(proxy + 0x10, proxy_self1) ||
        !guarded_read(proxy + 0x18, proxy_self2) ||
        proxy_self0 != scene_proxy || proxy_self1 != scene_proxy ||
        proxy_self2 != scene_proxy ||
        !guarded_read(proxy + 0x20, fingerprint.proxy_owner) ||
        !fingerprint.proxy_owner ||
        !guarded_read(wrapper, wrapper_vtable) ||
        wrapper_vtable != g_game.base + kSceneObjProxyValueVtableRva ||
        !inspect_display_value(wrapper, view) ||
        !exact_as3_display_bridge(view) ||
        !guarded_read(wrapper + 0x30, fingerprint.auxiliary) ||
        !guarded_read(
            static_cast<const std::byte*>(view.object_interface) + 0x08,
            owner_link) ||
        !owner_link ||
        !guarded_read(
            static_cast<const std::byte*>(owner_link) + 0x18,
            movie_root) ||
        !movie_root ||
        !guarded_read(movie_root, movie_vtable) ||
        movie_vtable != g_game.base + kAs3MovieVtable271Rva ||
        !guarded_read(
            static_cast<const std::byte*>(movie_vtable) + 0x170,
            create_object) ||
        create_object != g_game.base + kAs3MovieCreateObject271Rva) {
        fingerprint = {};
        return false;
    }

    fingerprint.object_interface = view.object_interface;
    fingerprint.flags = view.flags;
    fingerprint.object_payload = view.object_payload;
    fingerprint.owner_link = owner_link;
    fingerprint.movie_root = movie_root;
    return true;
}

bool same_slot_growth_movie(
    const SlotGrowthMovieFingerprint& left,
    const SlotGrowthMovieFingerprint& right) noexcept {
    return left.proxy_owner && left.object_interface && left.object_payload &&
        left.owner_link && left.movie_root &&
        left.proxy_owner == right.proxy_owner &&
        left.object_interface == right.object_interface &&
        left.flags == right.flags &&
        left.object_payload == right.object_payload &&
        left.auxiliary == right.auxiliary &&
        left.owner_link == right.owner_link &&
        left.movie_root == right.movie_root;
}

bool same_slot_growth_movie_lineage(
    const SlotGrowthMovieFingerprint& child,
    const SlotGrowthMovieFingerprint& root) noexcept {
    return child.object_interface && child.owner_link && child.movie_root &&
        child.object_interface == root.object_interface &&
        child.owner_link == root.owner_link &&
        child.movie_root == root.movie_root;
}

SlotGrowthCellProof probe_slot_growth_cell(
    void* movie_context,
    const char* path,
    const SlotGrowthMovieFingerprint& root,
    SlotGrowthCellStorage& storage) noexcept {
    SlotGrowthCellProof proof{};
    if (!movie_context || !path || storage.poisoned ||
        storage.proxy.constructed || !g_resolve_path || !g_value_exists ||
        !g_destroy_scaleform_value) {
        return proof;
    }

    storage.proxy.storage.fill(std::byte{});
    void* result{};
#if defined(_MSC_VER)
    __try {
#endif
        result = g_resolve_path(
            movie_context, storage.proxy.storage.data(), path);
#if defined(_MSC_VER)
    } __except (proof.seh_code = GetExceptionCode(),
               EXCEPTION_EXECUTE_HANDLER) {
        result = nullptr;
    }
#endif
    if (result != storage.proxy.storage.data() || proof.seh_code != 0) {
        // ResolvePath constructs an owning value. A mismatched return leaves
        // ownership ambiguous, so retain this process-lifetime TLS storage and
        // never attempt to destroy or reuse it.
        storage.poisoned = true;
        return proof;
    }

    storage.proxy.constructed = true;
    proof.resolver_returned_destination = true;
#if defined(_MSC_VER)
    __try {
#endif
        proof.exists = g_value_exists(storage.proxy.storage.data());
        proof.existence_call_exact = true;
#if defined(_MSC_VER)
    } __except (proof.seh_code = GetExceptionCode(),
               EXCEPTION_EXECUTE_HANDLER) {
        proof.existence_call_exact = false;
    }
#endif
    if (proof.exists && proof.seh_code == 0) {
        proof.fingerprint_exact = read_slot_growth_movie_fingerprint(
            storage.proxy.storage.data(), proof.movie);
        proof.lineage_exact = proof.fingerprint_exact &&
            same_slot_growth_movie_lineage(proof.movie, root);
    }

    proof.cleanup_exact = destroy_proxy(storage.proxy);
    if (proof.cleanup_exact) {
        storage.proxy.storage.fill(std::byte{});
    } else {
        storage.poisoned = true;
    }
    return proof;
}

bool exact_slot_growth_cell_proof(
    const SlotGrowthCellProof& proof) noexcept {
    if (!proof.resolver_returned_destination ||
        !proof.existence_call_exact || !proof.cleanup_exact ||
        proof.seh_code != 0) {
        return false;
    }
    return !proof.exists ||
        (proof.fingerprint_exact && proof.lineage_exact &&
            proof.movie.object_payload);
}

bool discover_slot_growth_target(
    void* top,
    const SlotGrowthMovieFingerprint& root,
    std::size_t configured_minimum,
    NativeTabSlotGrowthPlan& plan,
    bool& symmetric) noexcept {
    plan = {};
    symmetric = false;
    if (!top) return false;

    std::array<SlotGrowthCellProof, kSlotGrowthProbeCellCount> proofs{};
    std::size_t proof_index{};
    constexpr std::array<const char*, 2> kStripNames{
        "TabList", "BackTabList"};
    for (const char* strip : kStripNames) {
        for (std::size_t cell = 8U; cell <= 10U; ++cell) {
            char path[64]{};
            const int written = _snprintf_s(
                path,
                std::size(path),
                _TRUNCATE,
                "%s/Item_0_%zu",
                strip,
                cell);
            if (written <= 0 || proof_index >= proofs.size()) return false;
            proofs[proof_index] = probe_slot_growth_cell(
                static_cast<std::byte*>(top) + kMovieContextOffset,
                path,
                root,
                g_slot_growth_cell_storage[proof_index]);
            ++proof_index;
        }
    }

    if (proof_index != proofs.size()) return false;
    for (const SlotGrowthCellProof& proof : proofs) {
        if (!exact_slot_growth_cell_proof(proof)) return false;
    }

    const std::array<bool, 3> front{
        proofs[0].exists, proofs[1].exists, proofs[2].exists};
    const std::array<bool, 3> back{
        proofs[3].exists, proofs[4].exists, proofs[5].exists};
    symmetric = front == back;
    plan = plan_native_tab_slot_growth(front, back, configured_minimum);
    if (!plan.structure_exact()) return false;

    std::array<void*, kSlotGrowthProbeCellCount> payloads{};
    std::size_t payload_count{};
    for (const SlotGrowthCellProof& proof : proofs) {
        if (!proof.exists) continue;
        if (proof.movie.object_payload == root.object_payload) return false;
        for (std::size_t earlier = 0; earlier < payload_count; ++earlier) {
            if (payloads[earlier] == proof.movie.object_payload) return false;
        }
        payloads[payload_count++] = proof.movie.object_payload;
    }

    return plan.discovered_slot_count >= kStockLogicalTabCount &&
        plan.discovered_slot_count <= kPhysicalTabCapacity;
}

bool read_slot_growth_record(
    void* top,
    std::size_t index,
    RecordFields& fields) noexcept {
    const std::byte* const record = record_at(top, index);
    void* vtable{};
    return record && guarded_read(record, vtable) &&
        vtable == g_game.base + kCategoryRecordVtableRva &&
        guarded_read(record + kTitleOffset, fields.title) && fields.title &&
        guarded_read(record + kCategoryIdOffset, fields.category) &&
        guarded_copy(
            fields.presentation.data(),
            record + kPresentationOffset,
            fields.presentation.size());
}

bool slot_growth_records_exact(
    void* top,
    std::size_t count,
    const RecordFields* graphics_snapshot = nullptr) noexcept {
    if (!top || count < kStockLogicalTabCount ||
        count > kPhysicalTabCapacity) {
        return false;
    }
    RecordFields graphics{};
    for (std::size_t index = 0; index < count; ++index) {
        RecordFields current{};
        if (!read_slot_growth_record(top, index, current)) return false;
        const std::int32_t expected = index < kExpectedStockCategories.size()
            ? kExpectedStockCategories[index]
            : kExpectedStockCategories.back();
        if (current.category != expected) return false;
        if (index == kStockLogicalTabCount - 1U) graphics = current;
        if (index >= kStockLogicalTabCount &&
            (current.title != graphics.title ||
                current.presentation != graphics.presentation)) {
            return false;
        }
    }
    return !graphics_snapshot ||
        (graphics.title == graphics_snapshot->title &&
            graphics.category == graphics_snapshot->category &&
            graphics.presentation == graphics_snapshot->presentation);
}

bool slot_growth_geometry_supports_capacity(
    void* top,
    std::size_t required_capacity) noexcept {
    if (!top || required_capacity < kStockLogicalTabCount ||
        required_capacity > kPhysicalTabCapacity) {
        return false;
    }
    const auto* const selector =
        static_cast<const std::byte*>(top) + kSelectorOffset;
    void* selector_vtable{};
    std::int32_t horizontal{};
    std::int32_t vertical{};
    if (!guarded_read(selector, selector_vtable) ||
        selector_vtable != g_game.base + kSelectorVtableRva ||
        !guarded_read(
            selector + kSelectorHorizontalExtentOffset, horizontal) ||
        !guarded_read(
            selector + kSelectorVerticalExtentOffset, vertical) ||
        horizontal <= 0 || vertical <= 0 ||
        horizontal > 32 || vertical > 64 ||
        (horizontal != 1 && vertical != 1)) {
        return false;
    }
    // A third-party movie may author additional dormant cells beyond the
    // native ten-record ceiling. Keep them untouched and require only the
    // complete prefix needed by the capped native capacity.
    return static_cast<std::int64_t>(horizontal) * vertical >=
        static_cast<std::int64_t>(required_capacity + 1U);
}

[[noreturn]] void fail_fast_after_slot_growth(
    const char* stage,
    const SlotGrowthConstructionContext& context) noexcept {
    erui::detail::logf(
        erui::LogLevel::fatal,
        "NativeTabs slot growth lost exact state after owned append "
        "(stage=%s configuredMinimum=%zu discoveredCapacity=%zu "
        "activeTarget=%zu "
        "checkpoints=%zu); terminating before the mutated Configuration "
        "object can escape",
        stage ? stage : "unknown",
        context.configured_minimum,
        context.discovered_capacity,
        context.active_target,
        context.append_checkpoints);
    RaiseFailFastException(nullptr, nullptr, 0);
    std::terminate();
}

void append_graphics_records_at_construction_seam(
    safetyhook::Context& cpu) noexcept {
    if (!g_slot_growth_callbacks_armed.load(std::memory_order_acquire) ||
        g_inside_slot_growth_append) {
        return;
    }

    SlotGrowthConstructionContext& context = g_slot_growth_construction;
    void* const top = reinterpret_cast<void*>(cpu.rdi);
    if (!top || context.active) return;
    context = {};
    context.active = true;
    context.top = top;
    context.early_seam_seen = true;
    context.title_menu = cpu.r15 != 0;
    context.configured_minimum = g_minimum_tab_slots;
    if (context.title_menu) {
        // The same constructor also serves the title-menu options movie.
        // Physical custom tabs belong only to the in-game Configuration
        // movie, so leave that unrelated object completely untouched.
        context = {};
        return;
    }

    auto* const storage = static_cast<std::byte*>(top) +
        kCategoryStorageOffset;
    auto* const list = static_cast<std::byte*>(top) + kCategoryListOffset;
    auto* const selector = static_cast<std::byte*>(top) + kSelectorOffset;
    auto* const composite = static_cast<std::byte*>(top) + kCompositeOffset;
    auto* const controller = static_cast<std::byte*>(top) +
        kTopControllerOffset;
    std::byte* const graphics = record_at(
        top, kStockLogicalTabCount - 1U);

    void* scene_proxy{};
    void* frame_top{};
    void* top_vtable{};
    void* list_vtable{};
    void* selector_vtable{};
    void* geometry_enumerator{};
    SlotGrowthMovieFingerprint caller_movie{};
    SlotGrowthMovieFingerprint top_movie{};
    RecordFields graphics_snapshot{};
    const bool scene_proxy_read = guarded_read(
        reinterpret_cast<const void*>(cpu.rsp + 0x50), scene_proxy);
    const bool caller_movie_exact = scene_proxy_read &&
        read_slot_growth_movie_fingerprint(scene_proxy, caller_movie);
    const bool top_movie_exact = read_slot_growth_movie_fingerprint(
        static_cast<std::byte*>(top) + kMovieContextOffset, top_movie);
    const bool movie_exact = caller_movie_exact && top_movie_exact &&
        same_slot_growth_movie(caller_movie, top_movie);
    const bool frame_top_read = cpu.rbp >= 0x58 && guarded_read(
        reinterpret_cast<const void*>(cpu.rbp - 0x58), frame_top);
    const bool register_frame_exact =
        cpu.rbx == reinterpret_cast<std::uintptr_t>(storage) &&
        cpu.r13 == reinterpret_cast<std::uintptr_t>(list) &&
        cpu.r14 == reinterpret_cast<std::uintptr_t>(selector) &&
        cpu.r12 == reinterpret_cast<std::uintptr_t>(composite) &&
        cpu.rsi == reinterpret_cast<std::uintptr_t>(controller) &&
        cpu.r15 == 0 && frame_top_read && frame_top == top;
    const bool identities_exact =
        guarded_read(top, top_vtable) &&
        top_vtable == g_game.base + kTopVtableRva &&
        guarded_read(list, list_vtable) &&
        list_vtable == g_game.base + kCategoryListVtableRva &&
        guarded_read(selector, selector_vtable) &&
        selector_vtable == g_game.base + kSelectorVtableRva &&
        guarded_read(
            static_cast<const std::byte*>(selector_vtable) +
                3U * sizeof(void*),
            geometry_enumerator) &&
        geometry_enumerator ==
            g_game.base + kSelectorGeometryEnumeratorRva;
    const bool count_exact = guarded_read(
        static_cast<std::byte*>(top) + kCategoryCountOffset,
        context.count_before) &&
        context.count_before == kStockLogicalTabCount;
    const bool active_readable = guarded_read(
        selector + kSelectorActiveCountOffset,
        context.active_before);
    const bool stock_exact = slot_growth_records_exact(
        top, kStockLogicalTabCount);
    const bool snapshot_exact = read_slot_growth_record(
        top, kStockLogicalTabCount - 1U, graphics_snapshot);
    constexpr std::array<std::uint8_t, 16> kCategoryAppendEntry{
        0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
        0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48};
    const bool append_pristine = entry_matches(
        kCategoryAppendRva, kCategoryAppendEntry);
    bool symmetric{};
    NativeTabSlotGrowthPlan plan{};
    const bool cells_exact = movie_exact && discover_slot_growth_target(
        top,
        caller_movie,
        context.configured_minimum,
        plan,
        symmetric);
    context.scene_proxy = scene_proxy;
    context.movie = caller_movie;
    context.discovered_capacity = plan.discovered_slot_count;
    context.active_target = native_tab_active_slot_target(
        context.discovered_capacity, resource_logical_tab_count());
    context.live_cells_symmetric = symmetric;

    const bool competing_native_tweaks =
        GetModuleHandleW(L"NativeTweaks.dll") != nullptr;
    context.early_proof_exact = !competing_native_tweaks &&
        g_category_append && register_frame_exact && movie_exact &&
        identities_exact && count_exact && active_readable && stock_exact &&
        snapshot_exact && append_pristine && cells_exact;
    if (!context.early_proof_exact) {
        if (competing_native_tweaks) {
            if (context.configured_minimum > kStockLogicalTabCount) {
                log_line(
                    "WARNING: physical tab growth was left to NativeTweaks.dll because its competing constructor mutation path is loaded (configuredMinimum=%zu authoredCapacity=%zu)",
                    context.configured_minimum,
                    context.discovered_capacity);
            }
        } else {
            log_line(
                "ERROR: physical tab growth refused before mutation: exact same-construction proof failed (configuredMinimum=%zu discovered=%zu symmetric=%u)",
                context.configured_minimum,
                context.discovered_capacity,
                context.live_cells_symmetric ? 1U : 0U);
        }
        return;
    }
    if (!plan.minimum_satisfied()) {
        log_line(
            "ERROR: physical tab growth refused before mutation: current GFX exposes %zu compatible slots, below configured MinimumTabSlots=%zu",
            context.discovered_capacity,
            context.configured_minimum);
        return;
    }
    if (context.active_target < kStockLogicalTabCount ||
        context.active_target > context.discovered_capacity) {
        log_line(
            "ERROR: physical tab growth refused before mutation: invalid active target %zu for authored capacity %zu and logical count %zu",
            context.active_target,
            context.discovered_capacity,
            resource_logical_tab_count());
        return;
    }
    if (context.active_target == kStockLogicalTabCount) return;

    context.mutation_attempted = true;
    for (std::size_t expected_count = kStockLogicalTabCount + 1U;
         expected_count <= context.active_target;
         ++expected_count) {
        g_inside_slot_growth_append = true;
        // The helper increments the owned-vector count before copy
        // construction. Do not catch native faults: no safe rollback exists
        // for a partially constructed counted record.
        g_category_append(storage, graphics);
        g_inside_slot_growth_append = false;

        std::uint64_t count_after{};
        std::int32_t active_after{-1};
        const bool checkpoint_exact = guarded_read(
                static_cast<std::byte*>(top) + kCategoryCountOffset,
                count_after) &&
            count_after == expected_count &&
            guarded_read(
                selector + kSelectorActiveCountOffset, active_after) &&
            active_after == context.active_before &&
            slot_growth_records_exact(
                top, expected_count, &graphics_snapshot);
        if (!checkpoint_exact) {
            fail_fast_after_slot_growth("append-checkpoint", context);
        }
        ++context.append_checkpoints;
    }
    context.mutation_committed =
        context.append_checkpoints ==
            context.active_target - kStockLogicalTabCount &&
        slot_growth_records_exact(
            top, context.active_target, &graphics_snapshot);
    if (!context.mutation_committed) {
        fail_fast_after_slot_growth("append-commit", context);
    }
}

void observe_slot_growth_post_geometry(safetyhook::Context& cpu) noexcept {
    if (!g_slot_growth_callbacks_armed.load(std::memory_order_acquire) ||
        g_inside_slot_growth_append) {
        return;
    }
    SlotGrowthConstructionContext& context = g_slot_growth_construction;
    void* const top = reinterpret_cast<void*>(cpu.rdi);
    if (!context.active || !context.mutation_attempted ||
        context.top != top || context.late_seam_seen) {
        return;
    }
    context.late_seam_seen = true;

    void* scene_proxy{};
    void* frame_top{};
    SlotGrowthMovieFingerprint caller_movie{};
    SlotGrowthMovieFingerprint top_movie{};
    std::uint64_t count{};
    std::int32_t active{-1};
    const bool exact = guarded_read(
            reinterpret_cast<const void*>(cpu.rsp + 0x50), scene_proxy) &&
        scene_proxy == context.scene_proxy &&
        read_slot_growth_movie_fingerprint(scene_proxy, caller_movie) &&
        read_slot_growth_movie_fingerprint(
            static_cast<std::byte*>(top) + kMovieContextOffset, top_movie) &&
        same_slot_growth_movie(caller_movie, context.movie) &&
        same_slot_growth_movie(top_movie, context.movie) &&
        cpu.rbp >= 0x58 && guarded_read(
            reinterpret_cast<const void*>(cpu.rbp - 0x58), frame_top) &&
        frame_top == top &&
        cpu.r13 == reinterpret_cast<std::uintptr_t>(
            static_cast<std::byte*>(top) + kCategoryListOffset) &&
        cpu.r14 == reinterpret_cast<std::uintptr_t>(
            static_cast<std::byte*>(top) + kSelectorOffset) &&
        cpu.r12 == reinterpret_cast<std::uintptr_t>(
            static_cast<std::byte*>(top) + kCompositeOffset) &&
        cpu.rsi == reinterpret_cast<std::uintptr_t>(
            static_cast<std::byte*>(top) + kTopControllerOffset) &&
        cpu.r15 == 0 &&
        guarded_read(
            static_cast<std::byte*>(top) + kCategoryCountOffset, count) &&
        count == context.active_target &&
        guarded_read(
            static_cast<std::byte*>(top) + kSelectorOffset +
                kSelectorActiveCountOffset,
            active) &&
        active == context.active_before &&
        slot_growth_records_exact(top, context.active_target) &&
        slot_growth_geometry_supports_capacity(
            top, context.discovered_capacity);
    context.late_proof_exact = exact;
}

void observe_slot_growth_post_constructor(
    safetyhook::Context& cpu) noexcept {
    if (!g_slot_growth_callbacks_armed.load(std::memory_order_acquire)) return;
    const SlotGrowthConstructionContext pending = g_slot_growth_construction;
    if (!pending.active || !pending.top) return;
    g_slot_growth_construction = {};
    if (!pending.mutation_attempted) return;

    void* top_vtable{};
    void* list_vtable{};
    void* count_getter{};
    void* selector_vtable{};
    std::uint64_t count{};
    std::int32_t active{-1};
    std::int32_t selected{-2};
    SlotGrowthMovieFingerprint caller_movie{};
    SlotGrowthMovieFingerprint top_movie{};
    const auto* const list = static_cast<const std::byte*>(pending.top) +
        kCategoryListOffset;
    const auto* const selector = static_cast<const std::byte*>(pending.top) +
        kSelectorOffset;
    const bool exact = pending.early_seam_seen && pending.late_seam_seen &&
        pending.early_proof_exact && pending.late_proof_exact &&
        pending.mutation_committed &&
        pending.append_checkpoints ==
            pending.active_target - kStockLogicalTabCount &&
        reinterpret_cast<void*>(cpu.rax) == pending.top &&
        reinterpret_cast<void*>(cpu.rdi) == pending.top &&
        read_slot_growth_movie_fingerprint(
            pending.scene_proxy, caller_movie) &&
        read_slot_growth_movie_fingerprint(
            static_cast<std::byte*>(pending.top) + kMovieContextOffset,
            top_movie) &&
        same_slot_growth_movie(caller_movie, pending.movie) &&
        same_slot_growth_movie(top_movie, pending.movie) &&
        guarded_read(pending.top, top_vtable) &&
        top_vtable == g_game.base + kTopVtableRva &&
        guarded_read(list, list_vtable) &&
        list_vtable == g_game.base + kCategoryListVtableRva &&
        guarded_read(
            static_cast<const std::byte*>(list_vtable) + sizeof(void*),
            count_getter) &&
        count_getter == g_game.base + kCategoryCountGetterRva &&
        guarded_read(selector, selector_vtable) &&
        selector_vtable == g_game.base + kSelectorVtableRva &&
        guarded_read(
            static_cast<const std::byte*>(pending.top) +
                kCategoryCountOffset,
            count) &&
        count == pending.active_target &&
        guarded_read(selector + kSelectorActiveCountOffset, active) &&
        active == static_cast<std::int32_t>(pending.active_target) &&
        guarded_read(selector + kSelectorSelectedIndexOffset, selected) &&
        selected >= -1 && selected < active &&
        slot_growth_geometry_supports_capacity(
            pending.top, pending.discovered_capacity) &&
        slot_growth_records_exact(pending.top, pending.active_target);
    if (!exact) fail_fast_after_slot_growth("post-constructor", pending);

    erui::detail::logf(
        erui::LogLevel::info,
        "NativeTabs physical tab growth committed: configuredMinimum=%zu authoredCapacity=%zu activeTarget=%zu nativeCount=%llu selectorActive=%d",
        pending.configured_minimum,
        pending.discovered_capacity,
        pending.active_target,
        static_cast<unsigned long long>(count),
        active);
}
#endif

bool destroy_proxies(
    std::array<ResolvedTabProxy, kPhysicalTabCapacity>& proxies) noexcept {
    bool exact{true};
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        exact = destroy_proxy(proxies[physical]) && exact;
    }
    return exact;
}

bool destroy_tab_layers(ResolvedTabLayers& layers) noexcept {
    const bool front_exact = destroy_proxies(layers.tab_list);
    const bool back_exact = destroy_proxies(layers.back_tab_list);
    return front_exact && back_exact;
}

bool resolve_tab_cell(
    void* top,
    const char* layer_name,
    std::size_t physical,
    ResolvedTabProxy& proxy) noexcept {
    if (!top || !layer_name || physical >= active_physical_tab_count() ||
        !g_resolve_path || !g_value_exists || !g_destroy_scaleform_value) {
        return false;
    }

    char path[64]{};
    const int written = _snprintf_s(
        path,
        std::size(path),
        _TRUNCATE,
        "%s/Item_0_%zu",
        layer_name,
        physical);
    if (written <= 0) return false;

    bool resolved{};
#if defined(_MSC_VER)
    __try {
#endif
        void* const movie_context =
            static_cast<std::byte*>(top) + kMovieContextOffset;
        void* const result = g_resolve_path(
            movie_context, proxy.storage.data(), path);
        proxy.constructed = result == proxy.storage.data();
        resolved = proxy.constructed &&
            g_value_exists(proxy.storage.data());
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        resolved = false;
    }
#endif
    if (!resolved) {
        log_failure("could not resolve existing tab cell '%s'", path);
        destroy_proxy(proxy);
    }
    return resolved;
}

bool resolve_tab_layer(
    void* top,
    const char* layer_name,
    std::array<ResolvedTabProxy, kPhysicalTabCapacity>& proxies) noexcept {
    if (!top || !layer_name || !g_resolve_path || !g_value_exists ||
        !g_destroy_scaleform_value) {
        return false;
    }
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        if (!resolve_tab_cell(
                top, layer_name, physical, proxies[physical])) {
            destroy_proxies(proxies);
            return false;
        }
    }
    return true;
}

bool resolve_tab_layers(
    void* top,
    ResolvedTabLayers& layers) noexcept {
    // TabList and BackTabList are coincident presentation layers. Resolve the
    // complete pair before any record mutation so a missing cell cannot leave
    // one layer showing a different logical window from the other.
    if (!resolve_tab_layer(top, "TabList", layers.tab_list)) {
        log_failure("could not resolve the complete TabList layer");
        destroy_tab_layers(layers);
        return false;
    }
    if (!resolve_tab_layer(top, "BackTabList", layers.back_tab_list)) {
        log_failure("could not resolve the complete BackTabList layer");
        destroy_tab_layers(layers);
        return false;
    }
    return true;
}

bool resolve_injected_icon_parent(
    void* top,
    TabLayer layer,
    std::size_t physical,
    ResolvedTabProxy& proxy) noexcept {
    if (!top || physical >= active_physical_tab_count() ||
        (layer != TabLayer::front && layer != TabLayer::back) ||
        !g_resolve_path || !g_value_exists || !g_destroy_scaleform_value) {
        return false;
    }

    char path[96]{};
    const char* const format = layer == TabLayer::front
        ? "TabList/Item_0_%zu/Cursor/ItemIcon"
        : "BackTabList/Item_0_%zu/ItemIcon";
    const int written = _snprintf_s(
        path, std::size(path), _TRUNCATE, format, physical);
    if (written <= 0) return false;

    bool resolved{};
#if defined(_MSC_VER)
    __try {
#endif
        void* const movie_context =
            static_cast<std::byte*>(top) + kMovieContextOffset;
        void* const result = g_resolve_path(
            movie_context, proxy.storage.data(), path);
        proxy.constructed = result == proxy.storage.data();
        resolved = proxy.constructed && g_value_exists(proxy.storage.data());
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        resolved = false;
    }
#endif
    if (!resolved) {
        log_failure("could not reacquire injected-icon parent '%s'", path);
        destroy_proxy(proxy);
    }
    return resolved;
}

constexpr bool is_undefined_gfx_value(std::uint32_t flags) noexcept;

bool preflight_in_place_rgba8_rebind_host(
    void* top,
    TabLayer layer,
    std::size_t physical,
    InPlaceRebindHostExpectation expectation,
    std::size_t expected_asset = 0,
    std::uint8_t property_bank = 0xFFU,
    bool enforce_ledger = true) noexcept {
    const std::size_t bit = physical +
        (layer == TabLayer::back ? active_physical_tab_count() : 0);
    const bool expects_tracked =
        expectation != InPlaceRebindHostExpectation::untracked;
    const bool expects_rgba =
        expectation == InPlaceRebindHostExpectation::rgba8_shape;
    if (!top || physical >= active_physical_tab_count() ||
        (layer != TabLayer::front && layer != TabLayer::back) ||
        bit >= g_rgba8_host_assets.size() ||
        (expects_rgba &&
            (expected_asset == 0 ||
                expected_asset > active_custom_tab_count())) ||
        (!expects_rgba && expected_asset != 0)) {
        return false;
    }

    const TabHostMask mask = TabHostMask{1} << bit;
    if (property_bank > kIconPropertyBankB) {
        property_bank = active_icon_property_bank(bit);
    }
    const TabHostMask injected = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask rgba = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const std::size_t asset = g_rgba8_host_assets[bit].load(
        std::memory_order_acquire);
    const bool ledger_exact = !enforce_ledger ||
        ((((injected & mask) != 0) == expects_tracked) &&
            (((rgba & mask) != 0) == expects_rgba) &&
            asset == expected_asset);

    void* const storage = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(InPlaceRebindPreflightArena));
    auto* const arena = storage
        ? ::new (storage) InPlaceRebindPreflightArena{}
        : nullptr;
    if (!arena) return false;

    bool injected_constructed{};
    bool authored_constructed{};
    bool fingerprint_exact{};
    bool properties_exact{};
    bool injected_flags_read{};
    bool authored_flags_read{};
    std::uint32_t injected_flags{};
    std::uint32_t authored_flags{};
    unsigned long seh_code{};
    if (ledger_exact && resolve_injected_icon_parent(
            top, layer, physical, arena->parent)) {
        void* const parent_wrapper =
            arena->parent.storage.data() + kSceneObjProxyValueOffset;
        DisplayValueView parent_view{};
        const bool parent_exact = inspect_display_value(
                parent_wrapper, parent_view) &&
            exact_as3_display_bridge(parent_view);
        void* owner_link{};
        void* movie_root{};
        fingerprint_exact = parent_exact && guarded_read(
                static_cast<const std::byte*>(parent_view.object_interface) +
                    0x08,
                owner_link) && owner_link && guarded_read(
                static_cast<const std::byte*>(owner_link) + 0x18,
                movie_root) && movie_root &&
            g_injected_movie_fingerprint.presentation_epoch ==
                g_runtime.presentation_epoch &&
            parent_view.object_interface ==
                g_injected_movie_fingerprint.object_interface &&
            owner_link == g_injected_movie_fingerprint.owner_link &&
            movie_root == g_injected_movie_fingerprint.movie_root;
        if (fingerprint_exact && g_get_scaleform_member) {
            const char* injected_property =
                injected_icon_property(property_bank);
            const char* authored_property =
                authored_icon_property(property_bank);
#if defined(_MSC_VER)
            __try {
#endif
                injected_constructed = g_get_scaleform_member(
                    parent_wrapper,
                    arena->injected.value.data(),
                    &injected_property) == arena->injected.value.data();
                authored_constructed = g_get_scaleform_member(
                    parent_wrapper,
                    arena->authored.value.data(),
                    &authored_property) == arena->authored.value.data();
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                seh_code = static_cast<unsigned long>(GetExceptionCode());
            }
#endif
        }
        if (seh_code == 0 && injected_constructed &&
            authored_constructed) {
            injected_flags_read = guarded_read(
                arena->injected.value.data() + 0x20, injected_flags);
            authored_flags_read = guarded_read(
                arena->authored.value.data() + 0x20, authored_flags);
            if (!expects_tracked) {
                properties_exact = injected_flags_read &&
                    authored_flags_read &&
                    is_undefined_gfx_value(injected_flags) &&
                    is_undefined_gfx_value(authored_flags);
            } else {
                DisplayValueView injected_view{};
                DisplayValueView authored_view{};
                properties_exact = inspect_display_value(
                        arena->injected.value.data(), injected_view) &&
                    inspect_display_value(
                        arena->authored.value.data(), authored_view) &&
                    exact_as3_display_bridge(injected_view) &&
                    exact_as3_display_bridge(authored_view) &&
                    injected_view.object_interface ==
                        parent_view.object_interface &&
                    authored_view.object_interface ==
                        parent_view.object_interface &&
                    injected_view.native_display_vtable ==
                        g_game.base + kDynamicShapeVtable271Rva &&
                    validated_authored_icon_target_vtable(
                        authored_view.native_display_vtable) &&
                    injected_view.object_payload !=
                        authored_view.object_payload;
            }
        }
    }

    bool cleanup_exact = seh_code == 0;
    if (cleanup_exact) {
        cleanup_exact = destroy_local_scaleform_value(
            arena->injected.value,
            injected_constructed,
            "in-place rebind preflight Shape") && cleanup_exact;
        cleanup_exact = destroy_local_scaleform_value(
            arena->authored.value,
            authored_constructed,
            "in-place rebind preflight authored icon") && cleanup_exact;
        cleanup_exact = destroy_proxy(arena->parent) && cleanup_exact;
    }
    const bool exact = ledger_exact && fingerprint_exact &&
        properties_exact && cleanup_exact;
    const char* const expectation_name = !expects_tracked
        ? "untracked"
        : (expects_rgba ? "rgba8" : "vector");
    log_line(
        "rgba8Rebind host preflight layer=%u physical=%zu expectation=%s asset=%u ledger=%u fingerprint=%u properties=%u flags={injected:%u/0x%08X authored:%u/0x%08X} cleanup=%u seh=0x%08lX result=%u",
        static_cast<unsigned>(layer),
        physical,
        expectation_name,
        static_cast<unsigned>(asset),
        ledger_exact ? 1U : 0U,
        fingerprint_exact ? 1U : 0U,
        properties_exact ? 1U : 0U,
        injected_flags_read ? 1U : 0U,
        static_cast<unsigned>(injected_flags),
        authored_flags_read ? 1U : 0U,
        static_cast<unsigned>(authored_flags),
        cleanup_exact ? 1U : 0U,
        seh_code,
        exact ? 1U : 0U);
    if (cleanup_exact) {
        arena->~InPlaceRebindPreflightArena();
        HeapFree(GetProcessHeap(), 0, arena);
    } else {
        log_failure(
            "fresh-Shape RGBA8 replacement preflight retained its stable wrapper arena after cleanup uncertainty");
    }
    return exact;
}

bool clear_in_place_rgba8_rebind_host(
    void* top,
    TabLayer layer,
    std::size_t physical,
    InPlaceRgba8RebindTransaction& transaction) noexcept {
    const std::size_t bit = physical +
        (layer == TabLayer::back ? active_physical_tab_count() : 0);
    if (!top || physical >= active_physical_tab_count() ||
        (layer != TabLayer::front && layer != TabLayer::back) ||
        bit >= g_rgba8_host_assets.size()) {
        return false;
    }

    const TabHostMask mask = TabHostMask{1} << bit;
    if ((transaction.expected_raster_clear_mask & mask) == 0 ||
        (transaction.completed_clear_mask & mask) != 0) {
        return false;
    }

    const bool reconciled_before = reconcile_rgba8_retirement_debt(
        "two-phase-clear-before");
    const TabHostMask injected = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask rgba = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const std::size_t encoded_asset = g_rgba8_host_assets[bit].load(
        std::memory_order_acquire);
    const std::uint32_t active_before =
        count_active_rgba8_asset_hosts(0U);
    const std::uint32_t debt_before =
        g_rgba8_retirement_debt[0].load(std::memory_order_acquire);
    const Rgba8ReferenceSnapshot refs_before =
        snapshot_rgba8_references(0U);
    const std::uint32_t raw_expected_before =
        1U + active_before + debt_before;
    const bool ownership_before_exact = reconciled_before &&
        (injected & mask) != 0 && (rgba & mask) != 0 &&
        encoded_asset == 1U && active_before > 0U &&
        debt_before < kMaximumRgba8RetirementDebt &&
        refs_before.resource_read && refs_before.resource == 1U &&
        refs_before.raw_read && refs_before.raw == raw_expected_before;

    void* const arena_storage = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(InPlaceRebindClearArena));
    auto* const arena = arena_storage
        ? ::new (arena_storage) InPlaceRebindClearArena{}
        : nullptr;
    if (!arena) return false;

    bool injected_constructed{};
    bool authored_constructed{};
    bool graphics_constructed{};
    bool fingerprint_exact{};
    bool shape_exact{};
    bool graphics_exact{};
    bool graphics_cleared{};
    unsigned long seh_code{};
    if (ownership_before_exact && resolve_injected_icon_parent(
            top, layer, physical, arena->parent)) {
        void* const parent_wrapper =
            arena->parent.storage.data() + kSceneObjProxyValueOffset;
        DisplayValueView parent_view{};
        const bool parent_exact = inspect_display_value(
                parent_wrapper, parent_view) &&
            exact_as3_display_bridge(parent_view);
        void* owner_link{};
        void* movie_root{};
        fingerprint_exact = parent_exact && guarded_read(
                static_cast<const std::byte*>(parent_view.object_interface) +
                    0x08,
                owner_link) && owner_link && guarded_read(
                static_cast<const std::byte*>(owner_link) + 0x18,
                movie_root) && movie_root &&
            g_injected_movie_fingerprint.presentation_epoch ==
                g_runtime.presentation_epoch &&
            parent_view.object_interface ==
                g_injected_movie_fingerprint.object_interface &&
            owner_link == g_injected_movie_fingerprint.owner_link &&
            movie_root == g_injected_movie_fingerprint.movie_root;
        if (fingerprint_exact && g_get_scaleform_member) {
            const char* injected_property = kInjectedIconProperty;
            const char* authored_property = kAuthoredIconProperty;
#if defined(_MSC_VER)
            __try {
#endif
                injected_constructed = g_get_scaleform_member(
                    parent_wrapper,
                    arena->injected.value.data(),
                    &injected_property) == arena->injected.value.data();
                authored_constructed = g_get_scaleform_member(
                    parent_wrapper,
                    arena->authored.value.data(),
                    &authored_property) == arena->authored.value.data();
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                seh_code = static_cast<unsigned long>(GetExceptionCode());
            }
#endif
        }

        DisplayValueView injected_view{};
        DisplayValueView authored_view{};
        shape_exact = seh_code == 0 && injected_constructed &&
            authored_constructed && inspect_display_value(
                arena->injected.value.data(), injected_view) &&
            inspect_display_value(
                arena->authored.value.data(), authored_view) &&
            exact_as3_display_bridge(injected_view) &&
            exact_as3_display_bridge(authored_view) &&
            injected_view.object_interface == parent_view.object_interface &&
            authored_view.object_interface == parent_view.object_interface &&
            injected_view.native_display_vtable ==
                g_game.base + kDynamicShapeVtable271Rva &&
            validated_authored_icon_target_vtable(
                authored_view.native_display_vtable) &&
            injected_view.object_payload != authored_view.object_payload;
        if (shape_exact && g_get_scaleform_member) {
            const char* graphics_property = "graphics";
#if defined(_MSC_VER)
            __try {
#endif
                graphics_constructed = g_get_scaleform_member(
                    arena->injected.value.data(),
                    arena->graphics.value.data(),
                    &graphics_property) == arena->graphics.value.data();
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                seh_code = static_cast<unsigned long>(GetExceptionCode());
            }
#endif
        }
        ObjectValueView graphics_view{};
        graphics_exact = seh_code == 0 && graphics_constructed &&
            inspect_object_value(
                arena->graphics.value.data(), graphics_view) &&
            exact_as3_object_bridge(graphics_view);
        if (graphics_exact) {
            graphics_cleared = invoke_object_method(
                graphics_view, "clear", nullptr, 0, seh_code);
        }
    }

    bool cleanup_exact = seh_code == 0;
    if (cleanup_exact) {
        cleanup_exact = destroy_local_scaleform_value(
            arena->graphics.value,
            graphics_constructed,
            "two-phase rebind clear Graphics") && cleanup_exact;
        cleanup_exact = destroy_local_scaleform_value(
            arena->authored.value,
            authored_constructed,
            "two-phase rebind clear authored icon") && cleanup_exact;
        cleanup_exact = destroy_local_scaleform_value(
            arena->injected.value,
            injected_constructed,
            "two-phase rebind clear Shape") && cleanup_exact;
        cleanup_exact = destroy_proxy(arena->parent) && cleanup_exact;
    }

    const Rgba8ReferenceSnapshot refs_after =
        snapshot_rgba8_references(0);
    const std::uint32_t active_after = active_before > 0U
        ? active_before - 1U
        : 0U;
    const std::uint32_t minimum_raw_after = 1U + active_after;
    const std::uint32_t maximum_debt_after = debt_before + 1U;
    const std::uint32_t maximum_raw_after =
        minimum_raw_after + maximum_debt_after;
    const bool clear_transition_exact = ownership_before_exact &&
        fingerprint_exact && shape_exact && graphics_exact &&
        graphics_cleared && cleanup_exact && seh_code == 0 &&
        refs_after.resource_read && refs_after.resource == 1U &&
        refs_after.raw_read && refs_after.raw >= minimum_raw_after &&
        refs_after.raw <= maximum_raw_after;

    bool ledger_exact{};
    std::uint32_t recorded_debt = debt_before;
    if (clear_transition_exact) {
        recorded_debt = refs_after.raw - minimum_raw_after;
        g_rgba8_icon_hosts.fetch_and(
            ~mask,
            std::memory_order_acq_rel);
        g_rgba8_host_assets[bit].store(0, std::memory_order_release);
        g_rgba8_retirement_debt[0].store(
            recorded_debt, std::memory_order_release);
        g_rgba8_successful_detach_count.fetch_add(
            1, std::memory_order_relaxed);
        transaction.completed_clear_mask |= mask;
        ledger_exact = reconcile_rgba8_retirement_debt(
            "two-phase-clear-after");
        recorded_debt = g_rgba8_retirement_debt[0].load(
            std::memory_order_acquire);
    }

    log_line(
        "rgba8Rebind clear layer=%u physical=%zu mask=0x%04X ownership=%u fingerprint=%u shape=%u graphics={exact:%u cleared:%u} refs={resource:%u/%u raw:%u/%u->%u/%u expectedBefore:%u allowedAfter:%u..%u} ledger={active:%u->%u debt:%u->%u completed:0x%04X/0x%04X exact:%u} cleanup=%u seh=0x%08lX",
        static_cast<unsigned>(layer),
        physical,
        static_cast<unsigned>(mask),
        ownership_before_exact ? 1U : 0U,
        fingerprint_exact ? 1U : 0U,
        shape_exact ? 1U : 0U,
        graphics_exact ? 1U : 0U,
        graphics_cleared ? 1U : 0U,
        refs_before.resource_read ? 1U : 0U,
        static_cast<unsigned>(refs_before.resource),
        refs_before.raw_read ? 1U : 0U,
        static_cast<unsigned>(refs_before.raw),
        refs_after.raw_read ? 1U : 0U,
        static_cast<unsigned>(refs_after.raw),
        static_cast<unsigned>(raw_expected_before),
        static_cast<unsigned>(minimum_raw_after),
        static_cast<unsigned>(maximum_raw_after),
        static_cast<unsigned>(active_before),
        static_cast<unsigned>(active_after),
        static_cast<unsigned>(debt_before),
        static_cast<unsigned>(recorded_debt),
        static_cast<unsigned>(transaction.completed_clear_mask),
        static_cast<unsigned>(transaction.expected_clear_mask),
        ledger_exact ? 1U : 0U,
        cleanup_exact ? 1U : 0U,
        seh_code);

    if (cleanup_exact) {
        arena->~InPlaceRebindClearArena();
        HeapFree(GetProcessHeap(), 0, arena);
    } else {
        log_failure(
            "two-phase RGBA8 clear retained its stable wrapper arena after cleanup uncertainty");
    }
    return clear_transition_exact && ledger_exact;
}

void run_deferred_bitmapdata_constructor_probe(
    void* top,
    std::uint64_t frame_sequence) noexcept {
    if (!g_deferred_bitmapdata_probe.scheduled ||
        frame_sequence <=
            g_deferred_bitmapdata_probe.scheduled_after_frame) {
        return;
    }

    if (g_bitmapdata_constructor_probe_attempted.load(
            std::memory_order_acquire)) {
        g_deferred_bitmapdata_probe = {};
        return;
    }

    const DeferredBitmapDataProbe scheduled =
        g_deferred_bitmapdata_probe;
    const std::uint32_t execution_thread = GetCurrentThreadId();
    const bool exact_epoch = top && scheduled.top == top &&
        g_runtime.active && g_runtime.top == top && validate_top(top);
    const bool exact_thread =
        scheduled.scheduling_thread == execution_thread;
    std::size_t current_logical{invalid_tab_index};
    const bool exact_mapping = exact_epoch &&
        g_runtime.viewport.mapping_generation() ==
            scheduled.mapping_generation &&
        g_runtime.viewport.logical_for_physical(
            scheduled.physical, current_logical) &&
        current_logical == scheduled.expected_logical;
    const bool quiescent = !g_mutating_viewport &&
        !g_blocking_composite_input && !g_repaint_context.active &&
        !g_resolving_icon_alias &&
        g_pending_aliased_icon.resource == nullptr &&
        !g_runtime.pending.active;
    if (!exact_epoch || !exact_thread || !exact_mapping) {
        g_deferred_bitmapdata_probe = {};
        log_line(
            "bitmapDataDeferred canceled exactEpoch=%u exactThread=%u exactMapping=%u scheduledTop=%p currentTop=%p scheduledLogical=%zu currentLogical=%zu generation=%llu->%llu scheduledThread=%lu currentThread=%lu frame=%llu",
            exact_epoch ? 1U : 0U,
            exact_thread ? 1U : 0U,
            exact_mapping ? 1U : 0U,
            scheduled.top,
            top,
            scheduled.expected_logical,
            current_logical,
            static_cast<unsigned long long>(scheduled.mapping_generation),
            static_cast<unsigned long long>(
                g_runtime.viewport.mapping_generation()),
            static_cast<unsigned long>(scheduled.scheduling_thread),
            static_cast<unsigned long>(execution_thread),
            static_cast<unsigned long long>(frame_sequence));
        return;
    }
    constexpr std::uint64_t maximum_quiescence_wait_frames = 120;
    if (!quiescent) {
        const std::uint64_t waited =
            frame_sequence - scheduled.scheduled_after_frame;
        if (waited <= maximum_quiescence_wait_frames) return;
        g_deferred_bitmapdata_probe = {};
        log_line(
            "bitmapDataDeferred canceled after waiting %llu frames for a quiescent post-composite boundary",
            static_cast<unsigned long long>(waited));
        return;
    }

    g_deferred_bitmapdata_probe = {};

    ResolvedTabProxy parent_proxy{};
    if (!resolve_injected_icon_parent(
            top, scheduled.layer, scheduled.physical, parent_proxy)) {
        log_line(
            "bitmapDataDeferred canceled because the live icon parent could not be reacquired");
        return;
    }

    void* const parent_wrapper =
        parent_proxy.storage.data() + kSceneObjProxyValueOffset;
    DisplayValueView parent_view{};
    const bool exact_parent = inspect_display_value(
            parent_wrapper, parent_view) &&
        exact_as3_display_bridge(parent_view);

    void* get_movie_slot{};
    if (exact_parent) {
        (void)guarded_read(
            static_cast<const std::byte*>(
                parent_view.object_interface_vtable) + 0x178,
            get_movie_slot);
    }

    void* public_movie{};
    unsigned long get_movie_seh{};
    const bool exact_get_movie = exact_parent &&
        get_movie_slot == g_game.base + kAs3ValueGetMovie271Rva;
    if (exact_get_movie) {
#if defined(_MSC_VER)
        __try {
#endif
            public_movie = reinterpret_cast<ValueGetMovieFn>(get_movie_slot)(
                parent_view.object_interface);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            get_movie_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    // GetMovie is present at the expected AS3 interface slot, but Elden Ring's
    // resolved display values return null from it. Reacquire the same validated
    // owner link used by the native image presenter instead. This traversal is
    // performed from a fresh wrapper in this frame; none of these pointers is
    // retained after the synchronous probe call.
    void* owner_link{};
    void* owner_movie{};
    const bool owner_link_read = exact_parent && guarded_read(
        static_cast<const std::byte*>(parent_view.object_interface) + 0x08,
        owner_link);
    const bool owner_movie_read = owner_link_read && owner_link && guarded_read(
        static_cast<const std::byte*>(owner_link) + 0x18,
        owner_movie);
    void* const movie_root = public_movie ? public_movie : owner_movie;

    void* movie_vtable{};
    void* create_object_slot{};
    const bool movie_vtable_read = movie_root &&
        guarded_read(movie_root, movie_vtable);
    const bool create_object_read = movie_vtable_read &&
        guarded_read(
            static_cast<const std::byte*>(movie_vtable) + 0x170,
            create_object_slot);
    const bool exact_movie = get_movie_seh == 0 &&
        movie_vtable_read && create_object_read &&
        movie_vtable == g_game.base + kAs3MovieVtable271Rva &&
        create_object_slot ==
            g_game.base + kAs3MovieCreateObject271Rva;

    log_line(
        "bitmapDataDeferred execute phase=post-native-composite frame=%llu waited=%llu layer=%u physical=%zu exactParent=%u exactGetMovie=%u publicMovie=%p ownerLink={read:%u value:%p} ownerMovie={read:%u value:%p} selectedMovie=%p movieVtable=%p createObject=%p exactMovie=%u seh=0x%08lX thread=%lu",
        static_cast<unsigned long long>(frame_sequence),
        static_cast<unsigned long long>(
            frame_sequence - scheduled.scheduled_after_frame),
        static_cast<unsigned>(scheduled.layer),
        scheduled.physical,
        exact_parent ? 1U : 0U,
        exact_get_movie ? 1U : 0U,
        public_movie,
        owner_link_read ? 1U : 0U,
        owner_link,
        owner_movie_read ? 1U : 0U,
        owner_movie,
        movie_root,
        movie_vtable,
        create_object_slot,
        exact_movie ? 1U : 0U,
        get_movie_seh,
        static_cast<unsigned long>(execution_thread));

    if (exact_movie &&
        !g_bitmapdata_constructor_probe_attempted.exchange(
            true, std::memory_order_acq_rel)) {
        probe_bitmapdata_movie_property_ownership(
            parent_wrapper,
            parent_view,
            movie_root,
            create_object_slot,
            scheduled,
            frame_sequence);
    }
    destroy_proxy(parent_proxy);
}

void run_bitmapdata_property_survival_verification(
    void* top,
    std::uint64_t frame_sequence) noexcept {
    const BitmapDataPropertyVerificationPhase phase =
        g_bitmapdata_property_verification.phase;
    if (phase == BitmapDataPropertyVerificationPhase::none ||
        frame_sequence <=
            g_bitmapdata_property_verification.phase_armed_frame) {
        return;
    }

    const BitmapDataPropertyVerification pending =
        g_bitmapdata_property_verification;
    const bool known_phase =
        phase ==
            BitmapDataPropertyVerificationPhase::pre_mutation_identity ||
        phase ==
            BitmapDataPropertyVerificationPhase::post_mutation_identity;
    if (!known_phase) {
        g_bitmapdata_property_verification = {};
        log_failure(
            "BitmapData property verification discarded an unknown phase=%u",
            static_cast<unsigned>(phase));
        return;
    }
    const std::uint32_t execution_thread = GetCurrentThreadId();
    const bool exact_epoch = top && pending.top == top &&
        g_runtime.active && g_runtime.top == top && validate_top(top);
    const bool exact_thread =
        pending.creation_thread == execution_thread;
    std::size_t current_logical{invalid_tab_index};
    const bool exact_mapping = exact_epoch &&
        g_runtime.viewport.mapping_generation() ==
            pending.mapping_generation &&
        g_runtime.viewport.logical_for_physical(
            pending.physical, current_logical) &&
        current_logical == pending.expected_logical;
    const bool quiescent = !g_mutating_viewport &&
        !g_blocking_composite_input && !g_repaint_context.active &&
        !g_resolving_icon_alias &&
        g_pending_aliased_icon.resource == nullptr &&
        !g_runtime.pending.active;

    if (!exact_epoch || !exact_thread || !exact_mapping) {
        g_bitmapdata_property_verification = {};
        log_line(
            "bitmapDataProperty verification canceled phase=%s exactEpoch=%u exactThread=%u exactMapping=%u scheduledTop=%p currentTop=%p scheduledLogical=%zu currentLogical=%zu generation=%llu->%llu creationThread=%lu currentThread=%lu frame=%llu",
            bitmapdata_property_verification_phase_name(phase),
            exact_epoch ? 1U : 0U,
            exact_thread ? 1U : 0U,
            exact_mapping ? 1U : 0U,
            pending.top,
            top,
            pending.expected_logical,
            current_logical,
            static_cast<unsigned long long>(pending.mapping_generation),
            static_cast<unsigned long long>(
                g_runtime.viewport.mapping_generation()),
            static_cast<unsigned long>(pending.creation_thread),
            static_cast<unsigned long>(execution_thread),
            static_cast<unsigned long long>(frame_sequence));
        return;
    }
    constexpr std::uint64_t maximum_quiescence_wait_frames = 120;
    if (!quiescent) {
        const std::uint64_t waited =
            frame_sequence - pending.phase_armed_frame;
        if (waited <= maximum_quiescence_wait_frames) return;
        g_bitmapdata_property_verification = {};
        log_line(
            "bitmapDataProperty verification canceled phase=%s after waiting %llu frames for a quiescent post-composite boundary",
            bitmapdata_property_verification_phase_name(phase),
            static_cast<unsigned long long>(waited));
        return;
    }

    // Consume this one-shot verification before crossing any native boundary.
    // An exception or mismatch must never trigger a retry against uncertain AS
    // ownership state.
    g_bitmapdata_property_verification = {};

    ResolvedTabProxy parent_proxy{};
    if (!resolve_injected_icon_parent(
            top, pending.layer, pending.physical, parent_proxy)) {
        log_failure(
            "BitmapData property verification phase=%s could not reacquire the movie-owned parent",
            bitmapdata_property_verification_phase_name(phase));
        return;
    }

    void* const parent_wrapper =
        parent_proxy.storage.data() + kSceneObjProxyValueOffset;
    DisplayValueView parent_view{};
    const bool exact_parent = inspect_display_value(
            parent_wrapper, parent_view) &&
        exact_as3_display_bridge(parent_view);

    // Reacquire the MovieRoot exclusively through the validated owner chain.
    // This is a passive diagnostic: failed reads do not alter the ownership
    // verification result or the wrapper cleanup path below.
    void* verification_owner_link{};
    void* verification_movie_root{};
    void* verification_movie_vtable{};
    const bool verification_owner_link_read = exact_parent && guarded_read(
        static_cast<const std::byte*>(parent_view.object_interface) + 0x08,
        verification_owner_link);
    const bool verification_movie_root_read =
        verification_owner_link_read && verification_owner_link &&
        guarded_read(
            static_cast<const std::byte*>(verification_owner_link) + 0x18,
            verification_movie_root);
    const bool verification_movie_vtable_read =
        verification_movie_root_read && verification_movie_root &&
        guarded_read(verification_movie_root, verification_movie_vtable);
    const bool exact_verification_movie = verification_movie_vtable_read &&
        verification_movie_vtable ==
            g_game.base + kAs3MovieVtable271Rva;
    const BitmapDataVmContextSnapshot current_vm_context =
        exact_verification_movie
        ? read_bitmapdata_vm_context_snapshot(verification_movie_root)
        : BitmapDataVmContextSnapshot{};

    void* verification_wrapper{};
    if (!g_retained_bitmapdata_next_frame_wrapper) {
        verification_wrapper = HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, 0x40);
        g_retained_bitmapdata_next_frame_wrapper = verification_wrapper;
    } else {
        log_failure(
            "BitmapData property verification phase=%s found an occupied retained-wrapper slot; no additional wrapper was allocated",
            bitmapdata_property_verification_phase_name(phase));
    }
    bool verification_constructed{};
    bool survived{};
    unsigned long get_member_seh{};
    ObjectValueView verification_view{};
    if (exact_parent && verification_wrapper && g_get_scaleform_member) {
        const char* property_name = kInjectedBitmapDataProperty;
#if defined(_MSC_VER)
        __try {
#endif
            void* const result = g_get_scaleform_member(
                parent_wrapper, verification_wrapper, &property_name);
            verification_constructed = result == verification_wrapper;
            survived = verification_constructed &&
                inspect_object_value(
                    verification_wrapper, verification_view) &&
                exact_as3_object_bridge(verification_view) &&
                verification_view.object_payload == pending.expected_payload;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            get_member_seh = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    log_line(
        "bitmapDataProperty verification phase=%s frame=%llu waitedPhase=%llu age=%llu exactParent=%u wrapper=%p constructed=%u expectedPayload=%p actualPayload=%p identity=%u seh=0x%08lX propertyCleared=0 disposed=0",
        bitmapdata_property_verification_phase_name(phase),
        static_cast<unsigned long long>(frame_sequence),
        static_cast<unsigned long long>(
            frame_sequence - pending.phase_armed_frame),
        static_cast<unsigned long long>(
            frame_sequence - pending.created_frame),
        exact_parent ? 1U : 0U,
        verification_wrapper,
        verification_constructed ? 1U : 0U,
        pending.expected_payload,
        verification_view.object_payload,
        survived ? 1U : 0U,
        get_member_seh);

    if (!survived || get_member_seh != 0) {
        log_failure(
            "BitmapData property verification phase=%s failed closed; its heap wrapper was retained",
            bitmapdata_property_verification_phase_name(phase));
        destroy_proxy(parent_proxy);
        return;
    }

    if (phase ==
        BitmapDataPropertyVerificationPhase::pre_mutation_identity) {
        if (!kEnableBitmapDataPixelMutationProbe) {
            const bool vm_changed = current_vm_context.vm_address !=
                pending.vm_address;
            const bool context_changed =
                current_vm_context.drawable_image_context_address !=
                pending.drawable_image_context_address;
            const bool provider_changed =
                current_vm_context.drawable_image_provider_address !=
                pending.drawable_image_provider_address;
            log_line(
                "bitmapDataVmContext verification frame=%llu ownerLink={read:%u address:%p} movieRoot={read:%u address:%p exact:%u} vm={read:%u initial:%p current:%p changed:%u} drawableImageContext={read:%u initial:%p current:%p changed:%u} drawableImageProvider={read:%u initial:%p current:%p changed:%u} passiveOnly=1 diagnosticFailureIsNonFatal=1",
                static_cast<unsigned long long>(frame_sequence),
                verification_owner_link_read ? 1U : 0U,
                verification_owner_link,
                verification_movie_root_read ? 1U : 0U,
                verification_movie_root,
                exact_verification_movie ? 1U : 0U,
                current_vm_context.vm_read ? 1U : 0U,
                reinterpret_cast<void*>(pending.vm_address),
                reinterpret_cast<void*>(current_vm_context.vm_address),
                vm_changed ? 1U : 0U,
                current_vm_context.drawable_image_context_read ? 1U : 0U,
                reinterpret_cast<void*>(
                    pending.drawable_image_context_address),
                reinterpret_cast<void*>(
                    current_vm_context.drawable_image_context_address),
                context_changed ? 1U : 0U,
                current_vm_context.drawable_image_provider_read ? 1U : 0U,
                reinterpret_cast<void*>(
                    pending.drawable_image_provider_address),
                reinterpret_cast<void*>(
                    current_vm_context.drawable_image_provider_address),
                provider_changed ? 1U : 0U);
            if (destroy_and_free_heap_scaleform_value(
                    g_retained_bitmapdata_next_frame_wrapper,
                    verification_constructed,
                    "ownership-only BitmapData verification wrapper")) {
                log_line(
                    "bitmapDataProperty ownershipOnlyBaselineVerified=1 verificationWrapperReleased=1 parentPropertyRetained=1 pixelMutationEnabled=0");
            }
            destroy_proxy(parent_proxy);
            return;
        }

        constexpr std::uint32_t diagnostic_argb = 0xA17BC3E5U;
        const bool mutation_guard_claimed =
            !g_bitmapdata_pixel_mutation_probe_attempted.exchange(
                true, std::memory_order_acq_rel);
        bool mutation_invoked{};
        bool mutation_succeeded{};
        unsigned long mutation_seh{};
        if (mutation_guard_claimed && verification_view.invoke_slot) {
            const std::array<RawGfxValue, 3> pixel_arguments{
                make_int_value(0),
                make_int_value(0),
                make_uint_value(diagnostic_argb),
            };
            mutation_invoked = true;
#if defined(_MSC_VER)
            __try {
#endif
                mutation_succeeded = reinterpret_cast<ValueInvokeFn>(
                    verification_view.invoke_slot)(
                    verification_view.object_interface,
                    verification_view.object_payload,
                    nullptr,
                    "setPixel32",
                    pixel_arguments.data(),
                    pixel_arguments.size(),
                    false);
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                mutation_seh =
                    static_cast<unsigned long>(GetExceptionCode());
            }
#endif
        }

        log_line(
            "bitmapDataProperty mutation phase=%s frame=%llu guardClaimed=%u invoked=%u method=setPixel32 arguments={x:0 y:0 argb:0x%08X} invokeResult=%u seh=0x%08lX identityBeforeMutation=1 propertyCleared=0 disposed=0",
            bitmapdata_property_verification_phase_name(phase),
            static_cast<unsigned long long>(frame_sequence),
            mutation_guard_claimed ? 1U : 0U,
            mutation_invoked ? 1U : 0U,
            static_cast<unsigned>(diagnostic_argb),
            mutation_succeeded ? 1U : 0U,
            mutation_seh);

        if (!mutation_guard_claimed || !mutation_invoked ||
            !mutation_succeeded || mutation_seh != 0) {
            log_failure(
                "BitmapData single-pixel mutation failed closed; its verified heap wrapper was retained and no post-mutation verification was armed");
            destroy_proxy(parent_proxy);
            return;
        }

        if (!destroy_and_free_heap_scaleform_value(
                g_retained_bitmapdata_next_frame_wrapper,
                verification_constructed,
                "pre-mutation BitmapData verification wrapper")) {
            log_failure(
                "BitmapData mutation succeeded but wrapper cleanup failed closed; no post-mutation verification was armed");
            destroy_proxy(parent_proxy);
            return;
        }

        BitmapDataPropertyVerification post_mutation = pending;
        post_mutation.phase =
            BitmapDataPropertyVerificationPhase::post_mutation_identity;
        post_mutation.phase_armed_frame = frame_sequence;
        g_bitmapdata_property_verification = post_mutation;
        log_line(
            "bitmapDataProperty mutationCommitted=1 verificationWrapperReleased=1 nextPhase=%s nextFrameIdentityOnlyArmed=1 expectedPayload=%p parentPropertyRetained=1",
            bitmapdata_property_verification_phase_name(
                post_mutation.phase),
            post_mutation.expected_payload);
        destroy_proxy(parent_proxy);
        return;
    }

    if (destroy_and_free_heap_scaleform_value(
            g_retained_bitmapdata_next_frame_wrapper,
            verification_constructed,
            "post-mutation BitmapData verification wrapper")) {
        log_line(
            "bitmapDataProperty postMutationSurvivalVerified=1 identityOnly=1 verificationWrapperReleased=1 parentPropertyRetained=1 pixelReadbackPerformed=0");
    }
    destroy_proxy(parent_proxy);
}

enum class InjectedIconDetachResult : std::uint8_t {
    failed,
    removed,
    already_absent,
    different_movie,
};

enum class InjectedIconsClearResult : std::uint8_t {
    failed,
    waiting_for_renderer_retirement,
    cleared,
};

constexpr bool is_undefined_gfx_value(std::uint32_t flags) noexcept {
    return (flags & 0x8FU) == 0;
}

static_assert(is_undefined_gfx_value(0));
static_assert(!is_undefined_gfx_value(0x08));
static_assert(!is_undefined_gfx_value(0x0A));

InjectedIconDetachResult clear_injected_icon_host(
    void* top,
    TabLayer layer,
    std::size_t physical,
    std::uint8_t property_bank = 0xFFU,
    bool restore_authored = true) noexcept {
    const std::size_t host_bit = physical +
        (layer == TabLayer::back ? active_physical_tab_count() : 0);
    if (property_bank > kIconPropertyBankB) {
        property_bank = active_icon_property_bank(host_bit);
    }
    const char* const injected_property_name =
        injected_icon_property(property_bank);
    const char* const authored_property_name =
        authored_icon_property(property_bank);
    void* const arena_storage = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(InjectedIconDetachArena));
    auto* const arena = arena_storage
        ? ::new (arena_storage) InjectedIconDetachArena{}
        : nullptr;
    if (!arena) {
        log_failure("iconDetach could not allocate stable wrapper arena");
        return InjectedIconDetachResult::failed;
    }

    if (!resolve_injected_icon_parent(
            top, layer, physical, arena->parent)) {
        const bool parent_clean = destroy_proxy(arena->parent);
        if (parent_clean) {
            arena->~InjectedIconDetachArena();
            HeapFree(GetProcessHeap(), 0, arena_storage);
        }
        return InjectedIconDetachResult::failed;
    }

    void* const parent_wrapper =
        arena->parent.storage.data() + kSceneObjProxyValueOffset;
    DisplayValueView parent_view{};
    const bool exact_parent = inspect_display_value(
            parent_wrapper, parent_view) &&
        exact_as3_display_bridge(parent_view);
    void* parent_owner_link{};
    void* parent_movie_root{};
    const bool fingerprint_read = exact_parent &&
        guarded_read(
            static_cast<const std::byte*>(parent_view.object_interface) +
                0x08,
            parent_owner_link) &&
        parent_owner_link &&
        guarded_read(
            static_cast<const std::byte*>(parent_owner_link) + 0x18,
            parent_movie_root) &&
        parent_movie_root;
    const bool fingerprint_known =
        g_injected_movie_fingerprint.object_interface &&
        g_injected_movie_fingerprint.owner_link &&
        g_injected_movie_fingerprint.movie_root;
    const bool same_movie = fingerprint_read && fingerprint_known &&
        parent_view.object_interface ==
            g_injected_movie_fingerprint.object_interface &&
        parent_owner_link == g_injected_movie_fingerprint.owner_link &&
        parent_movie_root == g_injected_movie_fingerprint.movie_root;

    auto& injected_wrapper = arena->injected.value;
    auto& authored_wrapper = arena->authored.value;
    auto& graphics_wrapper = arena->graphics.value;
    auto& undefined_wrapper = arena->undefined.value;
    auto& verify_injected_wrapper = arena->verify_injected.value;
    auto& verify_authored_wrapper = arena->verify_authored.value;
    bool injected_constructed{};
    bool authored_constructed{};
    bool graphics_constructed{};
    bool graphics_exact{};
    bool graphics_cleared{};
    bool undefined_constructed{};
    bool verify_injected_constructed{};
    bool verify_authored_constructed{};
    bool injected_flags_read{};
    bool authored_flags_read{};
    std::uint32_t injected_flags{};
    std::uint32_t authored_flags{};
    bool already_absent{};
    bool removed{};
    bool hidden_fallback{};
    bool authored_visible{};
    bool icon_property_cleared{};
    bool authored_property_cleared{};
    bool property_clear_verified{};
    unsigned long graphics_seh{};
    unsigned long seh_code{};

    if (same_movie && g_get_scaleform_member) {
        const char* icon_property = injected_property_name;
        const char* authored_property = authored_property_name;
#if defined(_MSC_VER)
        __try {
#endif
            injected_constructed = g_get_scaleform_member(
                parent_wrapper,
                injected_wrapper.data(),
                &icon_property) == injected_wrapper.data();
            authored_constructed = g_get_scaleform_member(
                parent_wrapper,
                authored_wrapper.data(),
                &authored_property) == authored_wrapper.data();

            injected_flags_read = injected_constructed && guarded_read(
                injected_wrapper.data() + 0x20, injected_flags);
            authored_flags_read = authored_constructed && guarded_read(
                authored_wrapper.data() + 0x20, authored_flags);
            already_absent = injected_flags_read && authored_flags_read &&
                is_undefined_gfx_value(injected_flags) &&
                is_undefined_gfx_value(authored_flags);

            DisplayValueView injected_view{};
            DisplayValueView authored_view{};
            const bool exact_injected = injected_constructed &&
                inspect_display_value(
                    injected_wrapper.data(), injected_view) &&
                exact_as3_display_bridge(injected_view);
            const bool exact_authored = authored_constructed &&
                inspect_display_value(
                    authored_wrapper.data(), authored_view) &&
                exact_as3_display_bridge(authored_view);
            const bool exact_shape_icon = exact_injected &&
                injected_view.native_display_vtable ==
                    g_game.base + kDynamicShapeVtable271Rva;

            if (exact_shape_icon && exact_authored &&
                g_construct_scaleform_value && g_set_scaleform_visible) {
                if (g_get_scaleform_member) {
                    const char* graphics_property = "graphics";
#if defined(_MSC_VER)
                    __try {
#endif
                        graphics_constructed = g_get_scaleform_member(
                            injected_wrapper.data(),
                            graphics_wrapper.data(),
                            &graphics_property) == graphics_wrapper.data();
#if defined(_MSC_VER)
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        graphics_seh = static_cast<unsigned long>(
                            GetExceptionCode());
                    }
#endif
                }
                ObjectValueView graphics_view{};
                graphics_exact = graphics_seh == 0 &&
                    graphics_constructed &&
                    inspect_object_value(
                        graphics_wrapper.data(), graphics_view) &&
                    exact_as3_object_bridge(graphics_view);
                if (graphics_exact) {
                    graphics_cleared = invoke_object_method(
                        graphics_view,
                        "clear",
                        nullptr,
                        0,
                        graphics_seh);
                }

                // A detached AS3 Shape remains alive until Scaleform's tracing
                // collector runs. Clear its Graphics command list first so a
                // bitmap fill releases its RawImage reference synchronously;
                // only then may the host ledger be checked and transferred.
                // Restoring the authored icon is an independent visual
                // fail-safe. Even when Graphics.clear() is unavailable, make
                // the native icon visible before hiding the injected Shape;
                // only exact Graphics cleanup permits ownership detachment.
                authored_visible = !restore_authored ||
                    g_set_scaleform_visible(
                        authored_wrapper.data(), 1) == authored_wrapper.data();
                if (graphics_cleared && graphics_seh == 0 &&
                    authored_visible) {
                    removed = reinterpret_cast<ValueInvokeFn>(
                        parent_view.invoke_slot)(
                        parent_view.object_interface,
                        parent_view.object_payload,
                        nullptr,
                        "removeChild",
                        injected_wrapper.data() + 0x08,
                        1,
                        true);
                }
                if (!removed) {
                    hidden_fallback = g_set_scaleform_visible(
                        injected_wrapper.data(), 0) == injected_wrapper.data();
                }

                // Keep parent-owned references when removeChild fails.
                // Hiding is only a visual fail-safe; it is not ownership
                // cleanup, and retaining the references lets a later remap
                // retry the exact removal without accumulating orphan icons.
                if (removed) {
                    undefined_constructed = g_construct_scaleform_value(
                        undefined_wrapper.data()) == undefined_wrapper.data();
                }
                if (removed && undefined_constructed) {
                    const auto set_member =
                        reinterpret_cast<ValueSetMemberFn>(
                            parent_view.set_member_slot);
                    icon_property_cleared = set_member(
                        parent_view.object_interface,
                        parent_view.object_payload,
                        injected_property_name,
                        undefined_wrapper.data() + 0x08,
                        true);
                    authored_property_cleared = set_member(
                        parent_view.object_interface,
                        parent_view.object_payload,
                        authored_property_name,
                        undefined_wrapper.data() + 0x08,
                        true);
                    if (icon_property_cleared &&
                        authored_property_cleared) {
                        verify_injected_constructed =
                            g_get_scaleform_member(
                                parent_wrapper,
                                verify_injected_wrapper.data(),
                                &icon_property) ==
                            verify_injected_wrapper.data();
                        verify_authored_constructed =
                            g_get_scaleform_member(
                                parent_wrapper,
                                verify_authored_wrapper.data(),
                                &authored_property) ==
                            verify_authored_wrapper.data();
                        std::uint32_t verify_injected_flags{};
                        std::uint32_t verify_authored_flags{};
                        property_clear_verified =
                            verify_injected_constructed &&
                            verify_authored_constructed &&
                            guarded_read(
                                verify_injected_wrapper.data() + 0x20,
                                verify_injected_flags) &&
                            guarded_read(
                                verify_authored_wrapper.data() + 0x20,
                                verify_authored_flags) &&
                            is_undefined_gfx_value(
                                verify_injected_flags) &&
                            is_undefined_gfx_value(
                                verify_authored_flags);
                    }
                }
            }
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            seh_code = static_cast<unsigned long>(GetExceptionCode());
        }
#endif
    }

    // Destroy every managed wrapper while its heap arena remains stable. Any
    // native fault retains the arena and all ledger bits for a later exact
    // epoch cleanup rather than leaving an ObjectInterface link into dead
    // stack storage or claiming an ownership transition that did not finish.
    bool wrappers_clean = seh_code == 0 && graphics_seh == 0;
    if (wrappers_clean) {
        wrappers_clean = destroy_local_scaleform_value(
            verify_authored_wrapper,
            verify_authored_constructed,
            "detach verify authored") && wrappers_clean;
        wrappers_clean = destroy_local_scaleform_value(
            verify_injected_wrapper,
            verify_injected_constructed,
            "detach verify injected") && wrappers_clean;
        wrappers_clean = destroy_local_scaleform_value(
            undefined_wrapper,
            undefined_constructed,
            "detach undefined value") && wrappers_clean;
        wrappers_clean = destroy_local_scaleform_value(
            authored_wrapper,
            authored_constructed,
            "detach authored icon") && wrappers_clean;
        wrappers_clean = destroy_local_scaleform_value(
            graphics_wrapper,
            graphics_constructed,
            "detach Shape graphics") && wrappers_clean;
        wrappers_clean = destroy_local_scaleform_value(
            injected_wrapper,
            injected_constructed,
            "detach Shape") && wrappers_clean;
    }
    const bool proxy_clean = wrappers_clean && destroy_proxy(arena->parent);
    const bool cleanup_clean = wrappers_clean && proxy_clean;

    const bool exact_removed = same_movie && graphics_exact &&
        graphics_cleared && graphics_seh == 0 && removed && authored_visible &&
        icon_property_cleared && authored_property_cleared &&
        property_clear_verified && seh_code == 0 && cleanup_clean;
    const bool exact_absent = same_movie && already_absent &&
        seh_code == 0 && cleanup_clean;
    const bool exact_different_movie = fingerprint_read &&
        fingerprint_known && !same_movie && cleanup_clean;
    const InjectedIconDetachResult result = exact_removed
        ? InjectedIconDetachResult::removed
        : (exact_absent
            ? InjectedIconDetachResult::already_absent
            : (exact_different_movie
                ? InjectedIconDetachResult::different_movie
                : InjectedIconDetachResult::failed));
    log_line(
        "iconDetach epoch=%llu layer=%u physical=%zu exactParent=%u fingerprint={read:%u known:%u same:%u interface:%p/%p owner:%p/%p root:%p/%p} propertiesConstructed={%u,%u} propertyFlags={read:%u,0x%08X;read:%u,0x%08X} graphics={constructed:%u exact:%u clear:%u seh:0x%08lX} alreadyAbsent=%u removed=%u hiddenFallback=%u authoredVisible=%u propertyCleared={%u,%u verified:%u} cleanup={wrappers:%u proxy:%u} seh=0x%08lX result=%u",
        static_cast<unsigned long long>(g_runtime.presentation_epoch),
        static_cast<unsigned>(layer),
        physical,
        exact_parent ? 1U : 0U,
        fingerprint_read ? 1U : 0U,
        fingerprint_known ? 1U : 0U,
        same_movie ? 1U : 0U,
        parent_view.object_interface,
        g_injected_movie_fingerprint.object_interface,
        parent_owner_link,
        g_injected_movie_fingerprint.owner_link,
        parent_movie_root,
        g_injected_movie_fingerprint.movie_root,
        injected_constructed ? 1U : 0U,
        authored_constructed ? 1U : 0U,
        injected_flags_read ? 1U : 0U,
        static_cast<unsigned>(injected_flags),
        authored_flags_read ? 1U : 0U,
        static_cast<unsigned>(authored_flags),
        graphics_constructed ? 1U : 0U,
        graphics_exact ? 1U : 0U,
        graphics_cleared ? 1U : 0U,
        graphics_seh,
        already_absent ? 1U : 0U,
        removed ? 1U : 0U,
        hidden_fallback ? 1U : 0U,
        authored_visible ? 1U : 0U,
        icon_property_cleared ? 1U : 0U,
        authored_property_cleared ? 1U : 0U,
        property_clear_verified ? 1U : 0U,
        wrappers_clean ? 1U : 0U,
        proxy_clean ? 1U : 0U,
        seh_code,
        static_cast<unsigned>(result));

    if (cleanup_clean) {
        arena->~InjectedIconDetachArena();
        HeapFree(GetProcessHeap(), 0, arena_storage);
    } else {
        log_failure(
            "iconDetach retained stable heap arena after guarded cleanup uncertainty");
    }
    return result;
}

// Retire one raster host without retaining its Shape. The generic per-cell
// detach helper performs the exact same-Movie Graphics.clear(), removeChild(),
// authored-icon restore, property-to-Undefined readback, and wrapper teardown
// already proven by ordinary viewport cleanup. This wrapper adds the
// renderer-debt accounting required when the RawImage reference retires on a
// later frame instead of synchronously during Graphics.clear().
bool detach_rgba8_host_for_replacement(
    void* top,
    TabLayer layer,
    std::size_t physical,
    InPlaceRgba8RebindTransaction& transaction,
    bool restore_authored = true) noexcept {
    const std::size_t bit = physical +
        (layer == TabLayer::back ? active_physical_tab_count() : 0);
    if (!top || physical >= active_physical_tab_count() ||
        (layer != TabLayer::front && layer != TabLayer::back) ||
        bit >= g_rgba8_host_assets.size()) {
        return false;
    }

    const TabHostMask mask = TabHostMask{1} << bit;
    if ((transaction.expected_raster_clear_mask & mask) == 0 ||
        (transaction.completed_clear_mask & mask) != 0) {
        return false;
    }

    const bool reconciled_before = reconcile_rgba8_retirement_debt(
        "shape-replacement-detach-before");
    const TabHostMask injected_before = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask rgba_before = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const std::size_t encoded_asset_before =
        g_rgba8_host_assets[bit].load(std::memory_order_acquire);
    const bool asset_index_exact = encoded_asset_before > 0U &&
        encoded_asset_before <= active_custom_tab_count() &&
        encoded_asset_before == transaction.source_assets[bit];
    const std::size_t asset_index = asset_index_exact
        ? static_cast<std::size_t>(encoded_asset_before - 1U)
        : invalid_tab_index;
    const RetainedRgba8Resource source_retained = asset_index_exact
        ? current_retained_rgba8(asset_index)
        : RetainedRgba8Resource{};
    void* const source_resource = transaction.source_resources[bit];
    const bool source_identity_exact = asset_index_exact &&
        source_resource && source_retained.ready &&
        source_retained.resource == source_resource &&
        source_retained.image;
    const std::uint32_t active_before = asset_index_exact
        ? count_rgba8_resource_hosts(source_resource)
        : 0U;
    const std::uint32_t debt_before = asset_index_exact
        ? rgba8_debt_for_resource(asset_index, source_resource)
        : 0U;
    const Rgba8ReferenceSnapshot refs_before = source_identity_exact
        ? snapshot_rgba8_references(source_retained)
        : Rgba8ReferenceSnapshot{};
    const auto source_retirement =
        plan_rgba8_retirement_reconciliation(
            active_before,
            debt_before,
            refs_before.raw,
            kRgba8RetirementLimits);
    const bool ownership_before_exact = reconciled_before &&
        (injected_before & mask) != 0 && (rgba_before & mask) != 0 &&
        source_identity_exact && active_before > 0U &&
        debt_before < kMaximumRgba8RetirementDebt &&
        refs_before.resource_read && refs_before.resource == 1U &&
        refs_before.raw_read && source_retirement.accepted() &&
        source_retirement.retirement_debt_after == debt_before;

    const InjectedIconDetachResult detach_result = ownership_before_exact
        ? clear_injected_icon_host(
            top,
            layer,
            physical,
            active_icon_property_bank(bit),
            restore_authored)
        : InjectedIconDetachResult::failed;
    const Rgba8ReferenceSnapshot refs_after =
        source_identity_exact
        ? snapshot_rgba8_references(source_retained)
        : Rgba8ReferenceSnapshot{};
    const Rgba8HostRemovalPlan retirement =
        plan_rgba8_host_removal(
            active_before,
            debt_before,
            refs_before.raw,
            refs_after.raw,
            kRgba8RetirementLimits);
    const bool detach_transition_exact = ownership_before_exact &&
        detach_result == InjectedIconDetachResult::removed &&
        refs_after.resource_read && refs_after.resource == 1U &&
        refs_after.raw_read && retirement.accepted();

    std::uint32_t recorded_debt = debt_before;
    bool ledger_exact{};
    if (detach_transition_exact) {
        recorded_debt = retirement.retirement_debt_after;
        const bool debt_published =
            retained_rgba8_resource(asset_index) == source_resource &&
            set_rgba8_retirement_debt(asset_index, recorded_debt);
        if (debt_published) {
            g_injected_icon_hosts.fetch_and(
                ~mask,
                std::memory_order_acq_rel);
            g_rgba8_icon_hosts.fetch_and(
                ~mask,
                std::memory_order_acq_rel);
            g_rgba8_host_assets[bit].store(0, std::memory_order_release);

            ledger_exact =
                (g_injected_icon_hosts.load(
                     std::memory_order_acquire) & mask) == 0 &&
                (g_rgba8_icon_hosts.load(
                     std::memory_order_acquire) & mask) == 0 &&
                g_rgba8_host_assets[bit].load(
                    std::memory_order_acquire) == 0 &&
                rgba8_retirement_debt(asset_index) == recorded_debt;
        }
        if (ledger_exact) {
            g_rgba8_successful_detach_count.fetch_add(
                1, std::memory_order_relaxed);
            transaction.completed_clear_mask =
                transaction.completed_clear_mask | mask;
        }
    }

    log_line(
        "rgba8Replace detach layer=%u physical=%zu mask=0x%04X asset=%u ownership=%u result=%u refs={resource:%u/%u raw:%u/%u->%u/%u expectedBefore:%u allowedAfter:%u..%u} ledger={active:%u->%u debt:%u->%u injected:0x%04X rgba:0x%04X assetAfter:%u completed:0x%04X/0x%04X exact:%u}",
        static_cast<unsigned>(layer),
        physical,
        static_cast<unsigned>(mask),
        static_cast<unsigned>(encoded_asset_before),
        ownership_before_exact ? 1U : 0U,
        static_cast<unsigned>(detach_result),
        refs_before.resource_read ? 1U : 0U,
        static_cast<unsigned>(refs_before.resource),
        refs_before.raw_read ? 1U : 0U,
        static_cast<unsigned>(refs_before.raw),
        refs_after.raw_read ? 1U : 0U,
        static_cast<unsigned>(refs_after.raw),
        static_cast<unsigned>(source_retirement.maximum_raw),
        static_cast<unsigned>(retirement.minimum_raw_after),
        static_cast<unsigned>(retirement.maximum_raw_after),
        static_cast<unsigned>(active_before),
        static_cast<unsigned>(retirement.active_hosts_after),
        static_cast<unsigned>(debt_before),
        static_cast<unsigned>(recorded_debt),
        static_cast<unsigned>(
            g_injected_icon_hosts.load(std::memory_order_acquire)),
        static_cast<unsigned>(
            g_rgba8_icon_hosts.load(std::memory_order_acquire)),
        static_cast<unsigned>(
            g_rgba8_host_assets[bit].load(std::memory_order_acquire)),
        static_cast<unsigned>(transaction.completed_clear_mask),
        static_cast<unsigned>(transaction.expected_clear_mask),
        ledger_exact ? 1U : 0U);
    return detach_transition_exact && ledger_exact;
}

// Retire one vector-only injected Shape. Unlike a raster host it owns no
// RawImage reference, so exact removal can update the injected-host ledger
// immediately and does not create renderer-retirement debt.
bool detach_vector_host_for_replacement(
    void* top,
    TabLayer layer,
    std::size_t physical,
    InPlaceRgba8RebindTransaction& transaction,
    bool restore_authored = true) noexcept {
    const std::size_t bit = physical +
        (layer == TabLayer::back ? active_physical_tab_count() : 0);
    if (!top || physical >= active_physical_tab_count() ||
        (layer != TabLayer::front && layer != TabLayer::back) ||
        bit >= g_rgba8_host_assets.size()) {
        return false;
    }

    const TabHostMask mask = TabHostMask{1} << bit;
    if ((transaction.expected_vector_clear_mask & mask) == 0 ||
        (transaction.completed_clear_mask & mask) != 0) {
        return false;
    }

    const TabHostMask injected_before = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask rgba_before = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    const std::size_t asset_before = g_rgba8_host_assets[bit].load(
        std::memory_order_acquire);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const bool immutable_bank_overlap = g_rgba8_staged_bank_ready;
#else
    constexpr bool immutable_bank_overlap = false;
#endif
    const bool refs_before_exact = immutable_bank_overlap
        ? rgba8_rebind_overlap_references_match(
            "shape-replacement-vector-detach-before")
        : rgba8_references_match_tracked_hosts(
            "shape-replacement-vector-detach-before");
    const bool ownership_before_exact =
        (injected_before & mask) != 0 && (rgba_before & mask) == 0 &&
        asset_before == 0 && refs_before_exact &&
        preflight_in_place_rgba8_rebind_host(
            top, layer, physical,
            InPlaceRebindHostExpectation::vector_shape);
    const InjectedIconDetachResult detach_result = ownership_before_exact
        ? clear_injected_icon_host(
            top,
            layer,
            physical,
            active_icon_property_bank(bit),
            restore_authored)
        : InjectedIconDetachResult::failed;

    bool ledger_exact{};
    if (detach_result == InjectedIconDetachResult::removed) {
        g_injected_icon_hosts.fetch_and(
            ~mask,
            std::memory_order_acq_rel);
        ledger_exact =
            (g_injected_icon_hosts.load(std::memory_order_acquire) & mask) ==
                0 &&
            (g_rgba8_icon_hosts.load(std::memory_order_acquire) & mask) == 0 &&
            g_rgba8_host_assets[bit].load(std::memory_order_acquire) == 0 &&
            (immutable_bank_overlap
                ? rgba8_rebind_overlap_references_match(
                    "shape-replacement-vector-detach-after")
                : rgba8_references_match_tracked_hosts(
                    "shape-replacement-vector-detach-after"));
        if (ledger_exact) {
            transaction.completed_clear_mask =
                transaction.completed_clear_mask | mask;
        }
    }

    log_line(
        "rgba8Replace vectorDetach layer=%u physical=%zu mask=0x%04X ownership=%u result=%u ledger={injected:0x%04X rgba:0x%04X asset:%u completed:0x%04X/0x%04X exact:%u}",
        static_cast<unsigned>(layer),
        physical,
        static_cast<unsigned>(mask),
        ownership_before_exact ? 1U : 0U,
        static_cast<unsigned>(detach_result),
        static_cast<unsigned>(
            g_injected_icon_hosts.load(std::memory_order_acquire)),
        static_cast<unsigned>(
            g_rgba8_icon_hosts.load(std::memory_order_acquire)),
        static_cast<unsigned>(
            g_rgba8_host_assets[bit].load(std::memory_order_acquire)),
        static_cast<unsigned>(transaction.completed_clear_mask),
        static_cast<unsigned>(transaction.expected_clear_mask),
        ledger_exact ? 1U : 0U);
    return ownership_before_exact &&
        detach_result == InjectedIconDetachResult::removed && ledger_exact;
}

InjectedIconsClearResult clear_injected_icons(
    void* top,
    bool allow_already_absent = false) noexcept {
    const TabHostMask active_tracked = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask staged_tracked =
        g_staged_injected_icon_hosts.load(std::memory_order_acquire);
    if ((active_tracked | staged_tracked) == 0) {
        log_rgba8_lifecycle_snapshot("clear-no-hosts", top);
        if (!reconcile_rgba8_retirement_debt(
                "epoch-clear-no-hosts")) {
            return InjectedIconsClearResult::failed;
        }
        if (!all_rgba8_retirement_debts_zero()) {
            return allow_already_absent
                ? InjectedIconsClearResult::waiting_for_renderer_retirement
                : InjectedIconsClearResult::failed;
        }
        g_injected_movie_fingerprint = {};
        g_icon_bank_b_hosts.store(0, std::memory_order_release);
        return InjectedIconsClearResult::cleared;
    }

    bool all_cleared{true};
    const auto clear_bank = [&](bool staged) noexcept {
        const TabHostMask tracked = staged
            ? g_staged_injected_icon_hosts.load(std::memory_order_acquire)
            : g_injected_icon_hosts.load(std::memory_order_acquire);
        for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
            const TabHostMask mask = TabHostMask{1} << bit;
            if ((tracked & mask) == 0) continue;

            const TabLayer layer = bit < active_physical_tab_count()
                ? TabLayer::front
                : TabLayer::back;
            const std::size_t physical = bit % active_physical_tab_count();
            const TabHostMask rgba_bits = staged
                ? g_staged_rgba8_icon_hosts.load(std::memory_order_acquire)
                : g_rgba8_icon_hosts.load(std::memory_order_acquire);
            const bool was_rgba = (rgba_bits & mask) != 0;
            const std::size_t prior_asset = staged
                ? g_staged_rgba8_host_assets[bit].load(
                    std::memory_order_acquire)
                : g_rgba8_host_assets[bit].load(
                    std::memory_order_acquire);
            const bool asset_index_exact = was_rgba && prior_asset > 0U &&
                prior_asset <= active_custom_tab_count() &&
                catalogue_icon_requires_raster(prior_asset - 1U);
            const std::size_t asset_index = asset_index_exact
                ? prior_asset - 1U
                : invalid_tab_index;
            RetainedRgba8Resource retained = asset_index_exact
                ? current_retained_rgba8(asset_index)
                : RetainedRgba8Resource{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            if (staged && asset_index_exact &&
                g_rgba8_staged_bank_ready &&
                asset_index < g_rgba8_staged_resources_scratch.size()) {
                retained = g_rgba8_staged_resources_scratch[asset_index];
            }
#endif
            const bool resource_identity_exact = asset_index_exact &&
                retained.ready && retained.resource && retained.image;
            const std::uint32_t active_before = resource_identity_exact
                ? count_rgba8_resource_hosts(retained.resource)
                : 0U;
            std::uint32_t debt_before = resource_identity_exact && !staged
                ? rgba8_debt_for_resource(asset_index, retained.resource)
                : 0U;
            const Rgba8ReferenceSnapshot refs_before =
                resource_identity_exact
                ? snapshot_rgba8_references(retained)
                : Rgba8ReferenceSnapshot{};
            bool epoch_source_exact = allow_already_absent &&
                resource_identity_exact && refs_before.resource_read &&
                refs_before.resource == 1U && refs_before.raw_read;
            if (epoch_source_exact) {
                const auto reconciliation =
                    plan_rgba8_retirement_reconciliation(
                        active_before,
                        debt_before,
                        refs_before.raw,
                        kRgba8RetirementLimits);
                epoch_source_exact = reconciliation.accepted();
                if (epoch_source_exact && !staged &&
                    reconciliation.retirement_debt_after != debt_before) {
                    epoch_source_exact = set_rgba8_retirement_debt(
                        asset_index,
                        reconciliation.retirement_debt_after);
                    if (epoch_source_exact) {
                        debt_before =
                            reconciliation.retirement_debt_after;
                    }
                }
            }
            const std::uint8_t property_bank = staged
                ? static_cast<std::uint8_t>(
                    active_icon_property_bank(bit) ^ 1U)
                : active_icon_property_bank(bit);
            const InjectedIconDetachResult result =
                clear_injected_icon_host(
                    top, layer, physical, property_bank);
            const Rgba8ReferenceSnapshot refs_after =
                resource_identity_exact &&
                    result == InjectedIconDetachResult::removed
                ? snapshot_rgba8_references(retained)
                : Rgba8ReferenceSnapshot{};
            const Rgba8HostRemovalPlan epoch_retirement =
                plan_rgba8_host_removal(
                    active_before,
                    debt_before,
                    refs_before.raw,
                    refs_after.raw,
                    kRgba8RetirementLimits);
            const bool exact_removed_resource = was_rgba &&
                resource_identity_exact &&
                result == InjectedIconDetachResult::removed &&
                refs_after.resource_read && refs_after.resource == 1U &&
                refs_after.raw_read && epoch_retirement.accepted();
            const bool retirement_debt_published = exact_removed_resource &&
                (staged
                    ? epoch_retirement.retirement_debt_after == 0U
                    : set_rgba8_retirement_debt(
                        asset_index,
                        epoch_retirement.retirement_debt_after));
            const bool epoch_removed_ref_proven =
                allow_already_absent && was_rgba && epoch_source_exact &&
                exact_removed_resource &&
                retirement_debt_published;
            const bool staged_removed_ref_proven = staged && was_rgba &&
                exact_removed_resource &&
                refs_before.resource_read && refs_before.resource == 1U &&
                refs_before.raw_read &&
                epoch_retirement.retirement_debt_after == 0U;
            const bool removed_ref_proven =
                result == InjectedIconDetachResult::removed &&
                (!was_rgba ||
                    (allow_already_absent
                        ? epoch_removed_ref_proven
                        : (staged
                            ? staged_removed_ref_proven
                            : rgba8_references_match_tracked_hosts(
                            staged
                                ? "post-remove-staged"
                                : "post-remove",
                            bit,
                            0,
                            staged))));
            const Rgba8ReferenceSnapshot boundary_refs =
                resource_identity_exact
                ? snapshot_rgba8_references(retained)
                : Rgba8ReferenceSnapshot{};
            const bool exact_resource_root =
                boundary_refs.resource_read &&
                boundary_refs.resource == 1U &&
                boundary_refs.raw_read && boundary_refs.raw == 1U;
            const bool absent_boundary_released =
                allow_already_absent &&
                result == InjectedIconDetachResult::already_absent &&
                (!was_rgba || (staged
                    ? exact_resource_root
                    : rgba8_resources_at_root_baseline()));
            const TabHostMask all_rgba_bits =
                g_rgba8_icon_hosts.load(std::memory_order_acquire) |
                g_staged_rgba8_icon_hosts.load(std::memory_order_acquire);
            const bool movie_boundary_released =
                allow_already_absent &&
                result == InjectedIconDetachResult::different_movie &&
                (!was_rgba || all_rgba_bits == 0 ||
                    (staged
                        ? exact_resource_root
                        : rgba8_resources_at_root_baseline()));
            if (removed_ref_proven || absent_boundary_released ||
                movie_boundary_released) {
                if (staged) {
                    g_staged_injected_icon_hosts.fetch_and(
                        ~mask,
                        std::memory_order_acq_rel);
                    g_staged_rgba8_icon_hosts.fetch_and(
                        ~mask,
                        std::memory_order_acq_rel);
                    g_staged_rgba8_host_assets[bit].store(
                        0, std::memory_order_release);
                } else {
                    g_injected_icon_hosts.fetch_and(
                        ~mask,
                        std::memory_order_acq_rel);
                    g_rgba8_icon_hosts.fetch_and(
                        ~mask,
                        std::memory_order_acq_rel);
                    g_rgba8_host_assets[bit].store(
                        0, std::memory_order_release);
                }
                const TabHostMask injected_after = staged
                    ? g_staged_injected_icon_hosts.load(
                        std::memory_order_acquire)
                    : g_injected_icon_hosts.load(
                        std::memory_order_acquire);
                const TabHostMask rgba_after = staged
                    ? g_staged_rgba8_icon_hosts.load(
                        std::memory_order_acquire)
                    : g_rgba8_icon_hosts.load(
                        std::memory_order_acquire);
                const std::size_t asset_after = staged
                    ? g_staged_rgba8_host_assets[bit].load(
                        std::memory_order_acquire)
                    : g_rgba8_host_assets[bit].load(
                        std::memory_order_acquire);
                const bool ledger_exact =
                    (injected_after & mask) == 0 &&
                    (rgba_after & mask) == 0 && asset_after == 0 &&
                    (staged || !epoch_removed_ref_proven ||
                        rgba8_retirement_debt(asset_index) ==
                            epoch_retirement.retirement_debt_after);
                if (was_rgba && ledger_exact) {
                    g_rgba8_successful_detach_count.fetch_add(
                        1, std::memory_order_relaxed);
                }
                log_line(
                    "iconDetach ownershipCleared=%u bank=%s layer=%u physical=%zu rgba=%u asset=%u result=%u refsProven=%u retirement={active:%u->%u debt:%u->%u policy:%u}",
                    ledger_exact ? 1U : 0U,
                    staged ? "staged" : "active",
                    static_cast<unsigned>(layer),
                    physical,
                    was_rgba ? 1U : 0U,
                    static_cast<unsigned>(prior_asset),
                    static_cast<unsigned>(result),
                    (!was_rgba || removed_ref_proven ||
                        absent_boundary_released || movie_boundary_released)
                        ? 1U
                        : 0U,
                    static_cast<unsigned>(active_before),
                    static_cast<unsigned>(
                        epoch_retirement.active_hosts_after),
                    static_cast<unsigned>(debt_before),
                    static_cast<unsigned>(
                        epoch_retirement.retirement_debt_after),
                    static_cast<unsigned>(epoch_retirement.status));
                if (!ledger_exact) {
                    log_failure(
                        "iconDetach ownership proof succeeded but the host/debt ledger did not publish exactly (bank=%s layer=%u physical=%zu rgba=%u)",
                        staged ? "staged" : "active",
                        static_cast<unsigned>(layer),
                        physical,
                        was_rgba ? 1U : 0U);
                    all_cleared = false;
                }
                if (absent_boundary_released && was_rgba) {
                    log_line(
                        "iconDetach accepted already-absent RGBA8 host at an epoch boundary only after every resource/raw image returned to root refcount 1 (bank=%s layer=%u physical=%zu)",
                        staged ? "staged" : "active",
                        static_cast<unsigned>(layer),
                        physical);
                }
                if (movie_boundary_released) {
                    log_line(
                        "iconDetach accepted different-movie epoch boundary after exact ownership proof (bank=%s layer=%u physical=%zu rgba=%u)",
                        staged ? "staged" : "active",
                        static_cast<unsigned>(layer),
                        physical,
                        was_rgba ? 1U : 0U);
                }
            } else {
                if (result == InjectedIconDetachResult::removed && was_rgba) {
                    log_failure(
                        "iconDetach removed a tracked RGBA8 Shape but its RawImage retirement transition was not exact; ownership ledger retained (bank=%s layer=%u physical=%zu policy=%u)",
                        staged ? "staged" : "active",
                        static_cast<unsigned>(layer),
                        physical,
                        static_cast<unsigned>(epoch_retirement.status));
                } else if (result ==
                    InjectedIconDetachResult::already_absent) {
                    log_failure(
                        "iconDetach found an already-absent tracked icon without both an allowed Configuration epoch boundary and RGBA root-refcount proof (bank=%s layer=%u physical=%zu rgba=%u)",
                        staged ? "staged" : "active",
                        static_cast<unsigned>(layer),
                        physical,
                        was_rgba ? 1U : 0U);
                }
                all_cleared = false;
            }
        }
    };

    // A failed atomic handoff may leave a partial inactive bank above the
    // intact source bank. Retire that staged bank first, then the active bank.
    clear_bank(true);
    clear_bank(false);
    const bool host_ledgers_empty =
        g_injected_icon_hosts.load(std::memory_order_acquire) == 0 &&
        g_staged_injected_icon_hosts.load(std::memory_order_acquire) == 0;
    const bool retirement_exact = all_cleared && host_ledgers_empty &&
        reconcile_rgba8_retirement_debt("epoch-clear-post-remove");
    const bool retirement_complete = retirement_exact &&
        all_rgba8_retirement_debts_zero();
    const InjectedIconsClearResult clear_result = !all_cleared ||
            !host_ledgers_empty || !retirement_exact
        ? InjectedIconsClearResult::failed
        : (retirement_complete
            ? InjectedIconsClearResult::cleared
            : (allow_already_absent
                ? InjectedIconsClearResult::waiting_for_renderer_retirement
                : InjectedIconsClearResult::failed));
    log_rgba8_lifecycle_snapshot(
        clear_result == InjectedIconsClearResult::cleared
            ? "post-clear"
            : (clear_result ==
                    InjectedIconsClearResult::waiting_for_renderer_retirement
                ? "post-clear-waiting-retirement"
                : "post-clear-failed"),
        top);
    if (clear_result == InjectedIconsClearResult::cleared) {
        g_injected_movie_fingerprint = {};
        g_icon_bank_b_hosts.store(0, std::memory_order_release);
    }
    return clear_result;
}

// Scaleform may publish the next Configuration movie while renderer-side
// RawImage references from the preceding movie are still retiring.  The old
// Shapes have already been detached at this point, so exact retirement debt
// is the only ownership allowed to cross that movie boundary.  A new movie may
// present vector-only placeholders during that short interval; attaching a
// retained RGBA resource remains blocked by arm_catalogue_presentation_refresh
// until every debt reaches zero.
[[nodiscard]] bool accept_vector_only_epoch_clear_result(
    void* top,
    InjectedIconsClearResult clear_result,
    const char* phase) noexcept {
    if (clear_result == InjectedIconsClearResult::cleared) return true;

    LiveTabGeometry geometry{};
    const bool geometry_read = read_live_tab_geometry(top, geometry);
    bool active_assets_empty = true;
    bool staged_assets_empty = true;
    if (g_rgba8_host_assets.size() !=
        g_staged_rgba8_host_assets.size()) {
        active_assets_empty = false;
        staged_assets_empty = false;
    } else {
        for (std::size_t bit = 0;
             bit < g_rgba8_host_assets.size();
             ++bit) {
            active_assets_empty = active_assets_empty &&
                g_rgba8_host_assets[bit].load(
                    std::memory_order_acquire) == 0U;
            staged_assets_empty = staged_assets_empty &&
                g_staged_rgba8_host_assets[bit].load(
                    std::memory_order_acquire) == 0U;
        }
    }

    const InPlaceRgba8RebindTransaction& transaction =
        g_in_place_rgba8_rebind_transaction;
    const Rgba8RapidEpochHandoffState proof{
        .exact_waiting_for_renderer_retirement =
            clear_result == InjectedIconsClearResult::
                waiting_for_renderer_retirement,
        .physical_geometry_matches = geometry_read &&
            geometry.active == active_physical_tab_count(),
        .active_injected_hosts_empty =
            g_injected_icon_hosts.load(std::memory_order_acquire) == 0U,
        .staged_injected_hosts_empty =
            g_staged_injected_icon_hosts.load(
                std::memory_order_acquire) == 0U,
        .active_rgba_hosts_empty =
            g_rgba8_icon_hosts.load(std::memory_order_acquire) == 0U,
        .staged_rgba_hosts_empty =
            g_staged_rgba8_icon_hosts.load(
                std::memory_order_acquire) == 0U,
        .active_asset_ledger_empty = active_assets_empty,
        .staged_asset_ledger_empty = staged_assets_empty,
        .transaction_active = transaction.active,
        .transaction_failed = transaction.failed,
        .destructive_runtime_failure =
            g_runtime.destructive_transition_failed,
    };
    if (!can_begin_vector_only_epoch_during_rgba8_retirement(proof)) {
        return false;
    }

    // No display object remains associated with the old movie fingerprint or
    // either property bank.  Retirement debt deliberately remains untouched:
    // it is the exact renderer ownership that the deferred RGBA handoff waits
    // to observe reaching zero.
    g_injected_movie_fingerprint = {};
    g_icon_bank_b_hosts.store(0, std::memory_order_release);
    log_line(
        "RGBA8 rapid epoch handoff accepted phase=%s active=%zu vectorOnly=1 retirementPending=1",
        phase ? phase : "unspecified",
        geometry.active);
    return true;
}

bool sample_current_movie_fingerprint(
    void* top,
    InjectedMovieFingerprint& fingerprint) noexcept {
    fingerprint = {};
    void* const storage = HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ResolvedTabProxy));
    auto* const proxy = storage
        ? ::new (storage) ResolvedTabProxy{}
        : nullptr;
    if (!proxy) {
        log_failure(
            "movie fingerprint sampling could not allocate a stable proxy");
        return false;
    }

    bool exact{};
    if (resolve_injected_icon_parent(
            top, TabLayer::front, 0, *proxy)) {
        void* const wrapper =
            proxy->storage.data() + kSceneObjProxyValueOffset;
        DisplayValueView view{};
        void* owner_link{};
        void* movie_root{};
        exact = inspect_display_value(wrapper, view) &&
            exact_as3_display_bridge(view) &&
            guarded_read(
                static_cast<const std::byte*>(view.object_interface) + 0x08,
                owner_link) && owner_link &&
            guarded_read(
                static_cast<const std::byte*>(owner_link) + 0x18,
                movie_root) && movie_root;
        if (exact) {
            fingerprint.object_interface = view.object_interface;
            fingerprint.owner_link = owner_link;
            fingerprint.movie_root = movie_root;
            fingerprint.presentation_epoch = g_runtime.presentation_epoch;
        }
    }

    const bool proxy_clean = destroy_proxy(*proxy);
    if (proxy_clean) {
        proxy->~ResolvedTabProxy();
        HeapFree(GetProcessHeap(), 0, storage);
    } else {
        log_failure(
            "movie fingerprint sampling retained its stable proxy after cleanup uncertainty");
    }
    return exact && proxy_clean;
}

bool same_movie_fingerprint(
    const InjectedMovieFingerprint& left,
    const InjectedMovieFingerprint& right) noexcept {
    return left.object_interface && left.owner_link && left.movie_root &&
        left.object_interface == right.object_interface &&
        left.owner_link == right.owner_link &&
        left.movie_root == right.movie_root;
}

bool repaint_tab_cell(
    void* top,
    const char* layer_name,
    std::size_t physical,
    const TabViewport& viewport,
    ResolvedTabProxy& proxy) noexcept {
    if (!top || !layer_name ||
        physical >= active_physical_tab_count() ||
        !g_present_tab || !proxy.constructed) {
        destroy_proxy(proxy);
        return false;
    }

    bool presented{};
    bool present_faulted{};
    const RepaintContext previous_context = g_repaint_context;
    TabLayer layer{TabLayer::none};
    if (std::strcmp(layer_name, "TabList") == 0) {
        layer = TabLayer::front;
    } else if (std::strcmp(layer_name, "BackTabList") == 0) {
        layer = TabLayer::back;
    }
    std::size_t logical{invalid_tab_index};
    (void)viewport.logical_for_physical(physical, logical);
    g_repaint_context = {true, top, layer, physical, logical};
#if defined(_MSC_VER)
    __try {
#endif
        void* top_reference = top;
        g_present_tab(
            &top_reference,
            proxy.storage.data(),
            static_cast<std::uint32_t>(physical));
        presented = true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        present_faulted = true;
        log_failure(
            "%s repaint faulted at physical=%zu (seh=0x%08lX)",
            layer_name,
            physical,
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
    if (presented) {
        const bool retirement_exact =
            reconcile_rgba8_retirement_debt("post-present");
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        const bool immutable_bank_overlap =
            g_in_place_rgba8_rebind_transaction.active &&
            g_rgba8_staged_bank_ready;
        const bool references_exact = immutable_bank_overlap
            ? rgba8_rebind_overlap_references_match("post-present-overlap")
            : rgba8_references_match_tracked_hosts("post-present");
#else
        const bool references_exact =
            rgba8_references_match_tracked_hosts("post-present");
#endif
        if (!retirement_exact || !references_exact) {
            if (g_in_place_rgba8_rebind_transaction.active) {
                g_in_place_rgba8_rebind_transaction.failed = true;
            } else {
                g_rgba8_publication_disabled.store(
                    true, std::memory_order_release);
            }
            log_failure(
                "post-present RGBA8 reference audit failed; active rebind transaction is frozen or later raster publication is disabled");
        }
        log_rgba8_lifecycle_snapshot("post-present", top);
    } else if (present_faulted) {
        // The presenter may have consumed either the lookup reference or the
        // binder-local reference before faulting. No normal return boundary
        // exists from which to infer ownership, so permanently stop raster
        // publication for this process and retain every existing ledger bit.
        g_rgba8_publication_disabled.store(
            true, std::memory_order_release);
        g_pending_aliased_icon = {};
        g_resolving_icon_alias = false;
        const bool refs_exact = rgba8_references_match_tracked_hosts(
            "present-fault");
        log_rgba8_lifecycle_snapshot("present-fault", top);
        log_failure(
            "%s repaint ownership became uncertain after presenter SEH (physical=%zu logical=%zu refsExact=%u); later RGBA8 publication is disabled",
            layer_name,
            physical,
            logical,
            refs_exact ? 1U : 0U);
    }
    g_repaint_context = previous_context;
    // PresentTab consumes the embedded Scaleform value. On an exception the
    // value's ownership is uncertain; deliberately leak this research-only
    // reference rather than risk a double release.
    proxy.constructed = false;
    return presented;
}

bool repaint_tab_layer(
    void* top,
    const char* layer_name,
    const TabViewport& viewport,
    std::array<ResolvedTabProxy, kPhysicalTabCapacity>& proxies) noexcept {
    if (!top || !layer_name || !g_present_tab) {
        destroy_proxies(proxies);
        return false;
    }
    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        if (g_in_place_rgba8_rebind_transaction.active &&
            g_in_place_rgba8_rebind_transaction.failed) {
            destroy_proxies(proxies);
            return false;
        }
        if (!repaint_tab_cell(
                top, layer_name, physical, viewport, proxies[physical])) {
            destroy_proxies(proxies);
            return false;
        }
        // A correlated image-bind failure can occur after the native
        // presenter has consumed this cell successfully. Stop before touching
        // the next cell: continuing would turn a fail-closed research result
        // into additional unproven display-tree mutations.
        if (g_in_place_rgba8_rebind_transaction.active &&
            g_in_place_rgba8_rebind_transaction.failed) {
            destroy_proxies(proxies);
            return false;
        }
    }
    return true;
}

bool repaint_tab_layers(
    void* top,
    const TabViewport& viewport,
    ResolvedTabLayers& layers) noexcept {
    if (!repaint_tab_layer(
            top, "TabList", viewport, layers.tab_list)) {
        destroy_proxies(layers.back_tab_list);
        return false;
    }
    if (!repaint_tab_layer(
            top, "BackTabList", viewport, layers.back_tab_list)) {
        return false;
    }
    return true;
}

bool invoke_selector_select(
    void* top,
    std::uint32_t physical) noexcept {
    if (!top || !g_selector_select ||
        physical >= active_physical_tab_count()) {
        return false;
    }
    bool selected{};
#if defined(_MSC_VER)
    __try {
#endif
        selected = g_selector_select(
            static_cast<std::byte*>(top) + kSelectorOffset,
            physical,
            1);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        selected = false;
        log_failure(
            "selector relocation faulted (seh=0x%08lX)",
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
    return selected;
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
bool invoke_selector_active_count(
    void* top,
    std::size_t active) noexcept {
    if (!top || !g_selector_set_active_count ||
        active < kStockLogicalTabCount ||
        active > kPhysicalTabCapacity ||
        active > static_cast<std::size_t>(
            (std::numeric_limits<std::int32_t>::max)())) {
        return false;
    }
    auto* const selector = static_cast<std::byte*>(top) + kSelectorOffset;
#if defined(_MSC_VER)
    __try {
#endif
        g_selector_set_active_count(
            selector, static_cast<std::int32_t>(active));
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_failure(
            "selector active-count update faulted (active=%zu seh=0x%08lX)",
            active,
            static_cast<unsigned long>(GetExceptionCode()));
        return false;
    }
#endif
    std::int32_t observed{-1};
    return guarded_read(
            selector + kSelectorActiveCountOffset, observed) &&
        observed == static_cast<std::int32_t>(active);
}

bool normalize_selector_active_count_for_projection(void* top) noexcept {
    if (!top) return false;
    std::uint64_t record_count{};
    std::int32_t active{-1};
    std::int32_t selected{-1};
    auto* const selector = static_cast<std::byte*>(top) + kSelectorOffset;
    if (!guarded_read(
            static_cast<std::byte*>(top) + kCategoryCountOffset,
            record_count) ||
        record_count < kStockLogicalTabCount ||
        record_count > kPhysicalTabCapacity ||
        !guarded_read(selector + kSelectorActiveCountOffset, active) ||
        !guarded_read(selector + kSelectorSelectedIndexOffset, selected)) {
        return false;
    }
    const std::size_t target = std::min(
        static_cast<std::size_t>(record_count),
        active_logical_tab_count());
    if (target < kStockLogicalTabCount) return false;
    if (active == static_cast<std::int32_t>(target) &&
        selected >= 0 && selected < active) {
        return true;
    }
    if (selected < 0 || selected >= static_cast<std::int32_t>(target)) {
        ScopedBooleanTrue mutating{g_mutating_viewport};
        if (!invoke_selector_select(top, 0U)) return false;
    }
    return invoke_selector_active_count(top, target);
}
#endif

bool begin_panel_state_isolation(
    void* top,
    std::int32_t target_category,
    PanelStateIsolationReason reason) noexcept;
const char* panel_state_isolation_reason_name(
    PanelStateIsolationReason reason) noexcept;
void end_panel_state_isolation(bool armed) noexcept;
PanelSnapshot inspect_panel_slot(const void* slot) noexcept;
PanelCarrierSnapshot inspect_panel_carrier(const PanelSnapshot& panel) noexcept;

constexpr std::size_t kNativeGraphicsLogical = 7;
[[nodiscard]] std::size_t native_graphics_route() noexcept {
    return active_custom_tab_count();
}

bool logical_uses_graphics_carrier(
    std::size_t logical) noexcept {
    return logical == kNativeGraphicsLogical ||
        (logical >= kStockLogicalTabCount &&
            logical < active_logical_tab_count());
}

bool is_custom_logical(std::size_t logical) noexcept {
    return logical >= kStockLogicalTabCount &&
        logical < active_logical_tab_count();
}

std::size_t graphics_route_for_logical(
    std::size_t logical) noexcept {
    if (logical == kNativeGraphicsLogical) {
        return native_graphics_route();
    }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    return stable_custom_index_for_logical(logical);
#else
    return logical - kStockLogicalTabCount;
#endif
}

// Called synchronously by the host only after exact identity, age and native
// retirement validation. Category 5 is parking; category 8 is the carrier.
bool __cdecl switch_research_carrier_isolated(
    void* composite,
    std::int32_t category) noexcept {
    const auto original = g_original_research_switch.load(
        std::memory_order_acquire);
    if (!original || !composite || (category != 5 && category != 8) ||
        !g_research_explicit_switch || !g_runtime.pending.active ||
        static_cast<std::byte*>(g_runtime.top) + kCompositeOffset !=
            composite) return false;
    auto* const bytes = static_cast<std::byte*>(composite);
    void* source{};
    void* cached_target{};
    if (!guarded_read(bytes + kCompositeCurrentPanelOffset, source) ||
        !guarded_read(
            bytes + kCompositePanelCacheOffset +
                category * sizeof(void*),
            cached_target)) return false;
    g_state_isolation_expectation = {};
    const bool true_custom_route_involved =
        is_custom_logical(g_runtime.pending.previous_logical) ||
        is_custom_logical(g_runtime.pending.desired_logical);
    const bool native_copy_would_run = source && source != cached_target;
    const bool suppress_copy = native_copy_would_run &&
        (category == 5 || true_custom_route_involved);
    const bool armed = suppress_copy && begin_panel_state_isolation(
        g_runtime.top, category,
        PanelStateIsolationReason::corrective);
    if (suppress_copy && !armed) return false;
    original(composite, category);
    const bool copy_policy_exact = !suppress_copy ||
        (g_state_isolation_expectation.active &&
            g_state_isolation_expectation.consumed);
    end_panel_state_isolation(armed);
    void* current{};
    void* target{};
    const bool exact = copy_policy_exact &&
        guarded_read(bytes + kCompositeCurrentPanelOffset, current) &&
        guarded_read(
            bytes + kCompositePanelCacheOffset +
                category * sizeof(void*),
            target) && current && current == target;
    log_line(
        "graphicsCarrier nativeSwitch category=%d source=%p target=%p currentExact=%u stateCopyPolicy=%s customBoundary=%u frame=%llu",
        category, source, target, exact ? 1u : 0u,
        suppress_copy ? "suppressed" : "native-preserved",
        true_custom_route_involved ? 1u : 0u,
        static_cast<unsigned long long>(
            g_research_completed_composite_frames));
    return exact;
}

void __fastcall research_switch_panel_detour(
    void* composite,
    std::int32_t category) noexcept {
    const auto original = g_original_research_switch.load(
        std::memory_order_acquire);
    if (!original) return;
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) {
        original(composite, category);
        return;
    }
    if (g_runtime.destructive_transition_failed && !g_research_failed) {
        // The viewport lifetime guard owns no further routing. Preserve
        // Elden Ring's native switching unless an independent carrier fault
        // has made category 8 ambiguous; that stricter fault remains frozen
        // until an exact native teardown proves a recovery boundary.
        original(composite, category);
        return;
    }
    void* const top = composite
        ? static_cast<std::byte*>(composite) - kCompositeOffset
        : nullptr;
    if (g_runtime.active && g_runtime.top == top &&
        g_runtime.pending.active && !g_research_explicit_switch) {
        // The frame transaction owns the final route; suppress only the
        // provisional tab callback switch, never ordinary stock navigation.
        g_state_isolation_expectation.consumed = true;
        return;
    }
    if (category != 8) {
        original(composite, category);
        g_research_switch_succeeded = true;
        return;
    }

    const std::size_t logical = g_runtime.pending.active
        ? g_runtime.pending.desired_logical
        : g_runtime.viewport.selected_logical();
    const std::size_t physical = g_runtime.pending.active
        ? g_runtime.pending.desired_physical
        : g_runtime.viewport.selected_physical();
    if (g_research_failed || !g_runtime.active || g_runtime.top != top ||
        !logical_uses_graphics_carrier(logical) ||
        physical >= active_physical_tab_count() ||
        !g_research_explicit_switch ||
        !g_runtime.pending.active) {
        g_research_switch_succeeded = false;
        if (!g_research_failed) {
            g_research_failed = true;
            log_failure(
                "Graphics carrier switch escaped validated pending frame; custom transitions frozen");
        }
        return;
    }

    const std::size_t route = graphics_route_for_logical(logical);
    const std::uint64_t transaction_frame =
        g_research_completed_composite_frames;
    if (!g_research_prepare_carrier(
            composite, route, transaction_frame, &category,
            &switch_research_carrier_isolated)) {
        g_research_failed = true;
        g_research_switch_succeeded = false;
        log_failure(
            "Graphics carrier prepare FAILED CLOSED logical=%zu route=%zu frame=%llu",
            logical, route,
            static_cast<unsigned long long>(transaction_frame));
        return;
    }

    auto* const bytes = static_cast<std::byte*>(composite);
    const PanelSnapshot network = inspect_panel_slot(
        bytes + kCompositePanelCacheOffset + 5 * sizeof(void*));
    const PanelCarrierSnapshot network_carrier =
        inspect_panel_carrier(network);
    const bool switched = category == 8 &&
        switch_research_carrier_isolated(composite, category);
    const PanelSnapshot network_after = inspect_panel_slot(
        bytes + kCompositePanelCacheOffset + 5 * sizeof(void*));
    const PanelCarrierSnapshot network_carrier_after =
        inspect_panel_carrier(network_after);
    const bool network_exact = network.panel == network_after.panel &&
        (!network.panel ||
            (network_carrier.valid && network_carrier_after.valid &&
                network_carrier.wrapper == network_carrier_after.wrapper &&
                network_carrier.payload ==
                    network_carrier_after.payload));
    std::int32_t record_category{-1};
    const bool record_exact = guarded_read(
        record_at(top, physical) + kCategoryIdOffset,
        record_category) && record_category == category;
    if (!switched || !network_exact ||
        transaction_frame != g_research_completed_composite_frames ||
        !g_research_verify_carrier(
            composite, route, category) || !record_exact) {
        g_research_failed = true;
        g_research_switch_succeeded = false;
        log_failure(
            "Graphics carrier activation FAILED CLOSED logical=%zu route=%zu category=%d recordCategory=%d recordWrite=0",
            logical, route, category, record_category);
        return;
    }
    LogicalTab* const tab = runtime_logical_tab(logical);
    if (!tab) {
        g_research_failed = true;
        g_research_switch_succeeded = false;
        return;
    }
    tab->category = category;
    g_runtime.pending.desired_category = category;
    g_research_switch_succeeded = true;
    log_line(
        "graphicsCarrier activated logical=%zu route=%zu kind=%s category=8 frame=%llu networkPreserved=1 noInterveningFrame=1 finalCurrentCategory8=1",
        logical, route,
        route == native_graphics_route() ? "native-graphics" : "custom",
        static_cast<unsigned long long>(transaction_frame));
}

void invoke_switch_panel(
    void* top,
    std::int32_t category,
    PanelStateIsolationReason isolation_reason =
        PanelStateIsolationReason::none) noexcept {
    if (!top || !g_switch_panel) return;
    const bool isolation_armed = begin_panel_state_isolation(
        top, category, isolation_reason);
#if defined(_MSC_VER)
    __try {
#endif
        g_research_explicit_switch = true;
        g_research_switch_succeeded = false;
        g_switch_panel(
            static_cast<std::byte*>(top) + kCompositeOffset,
            category);
        g_research_explicit_switch = false;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_research_explicit_switch = false;
        g_research_failed = true;
        log_failure(
            "panel correction faulted for category=%d (seh=0x%08lX)",
            category,
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
    end_panel_state_isolation(isolation_armed);
}

void invoke_refresh_title(void* top) noexcept {
    if (!top || !g_refresh_title) return;
#if defined(_MSC_VER)
    __try {
#endif
        g_refresh_title(top);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_failure(
            "title refresh faulted (seh=0x%08lX)",
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
}

bool restore_previous_mapping(
    void* top,
    const std::array<RecordFields, kPhysicalTabCapacity>& previous,
    bool allow_vector_only_epoch_during_retirement = false) noexcept {
    const InjectedIconsClearResult clear_result = clear_injected_icons(
        top, allow_vector_only_epoch_during_retirement);
    const bool overlays_cleared =
        allow_vector_only_epoch_during_retirement
        ? accept_vector_only_epoch_clear_result(
              top, clear_result, "initial-mapping-rollback")
        : clear_result == InjectedIconsClearResult::cleared;
    const bool records_restored = overlays_cleared &&
        write_record_fields(top, previous);
    ResolvedTabLayers rollback_layers{};
    const bool visuals_restored = records_restored &&
        resolve_tab_layers(top, rollback_layers) &&
        repaint_tab_layers(top, g_runtime.viewport, rollback_layers);
    if (!overlays_cleared || !records_restored || !visuals_restored) {
        destroy_tab_layers(rollback_layers);
        log_failure(
            "record/visual rollback after slot remapping could not be completed (overlays=%u records=%u visuals=%u)",
            overlays_cleared ? 1u : 0u,
            records_restored ? 1u : 0u,
            visuals_restored ? 1u : 0u);
        return false;
    }
    return true;
}

[[nodiscard]] bool derive_injected_host_layout(
    const TabViewport& viewport,
    InjectedHostLayout& output) noexcept {
    output = {};
    if (!viewport.initialized() ||
        viewport.visible_slot_count() != active_physical_tab_count()) {
        return false;
    }

    for (std::size_t physical = 0;
         physical < active_physical_tab_count();
         ++physical) {
        std::size_t logical{invalid_tab_index};
        if (!viewport.logical_for_physical(physical, logical) ||
            logical >= active_logical_tab_count()) {
            return false;
        }

        bool vector = tab_overflow_indicator(viewport, physical) !=
            TabOverflowIndicator::none;
        std::size_t asset{};
        if (!vector && logical >= kStockLogicalTabCount) {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            const std::size_t custom =
                stable_custom_index_for_logical(logical);
#else
            const std::size_t custom =
                logical - kStockLogicalTabCount;
#endif
            if (custom >= active_custom_tab_count()) {
                return false;
            }
            if (catalogue_icon_has_effective_raster(custom)) {
                // Zero remains the empty-host sentinel. The encoded value is
                // size_t so every public logical-tab index is representable.
                asset = custom + 1U;
                const RetainedRgba8Resource retained =
                    effective_retained_rgba8(custom);
                if (!retained.ready || !retained.resource ||
                    !retained.image) {
                    return false;
                }
                for (std::size_t layer = 0; layer < 2; ++layer) {
                    const std::size_t bit = physical +
                        layer * active_physical_tab_count();
                    output.resources[bit] = retained.resource;
                }
            } else {
                vector = true;
            }
        }

        for (std::size_t layer = 0; layer < 2; ++layer) {
            const std::size_t bit =
                physical + layer * active_physical_tab_count();
            const TabHostMask mask = TabHostMask{1} << bit;
            if (vector) {
                output.vector_mask |= mask;
                output.injected_mask |= mask;
            } else if (asset != 0) {
                output.rgba_mask |= mask;
                output.injected_mask |= mask;
                output.assets[bit] = asset;
            }
        }
    }
    return output.injected_mask ==
            (output.rgba_mask | output.vector_mask) &&
        (output.rgba_mask & output.vector_mask) == 0;
}

// Source ownership must come from the committed display-tree ledger, not by
// re-deriving the old viewport with the current publication gate.  Native
// RGBA8 resources are created after the initial tab-strip paint; readiness can
// therefore change while that old viewport is still showing vector fallbacks.
// Reclassifying those already-painted hosts as raster would make the first
// real remap disagree with the Shapes that actually exist.
[[nodiscard]] bool snapshot_injected_host_layout(
    InjectedHostLayout& output) noexcept {
    output = {};
    output.injected_mask = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    output.rgba_mask = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    if ((output.rgba_mask & ~output.injected_mask) != 0) return false;
    output.vector_mask = output.injected_mask & ~output.rgba_mask;

    for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
        const TabHostMask mask = TabHostMask{1} << bit;
        const std::size_t encoded = g_rgba8_host_assets[bit].load(
            std::memory_order_acquire);
        if ((output.rgba_mask & mask) == 0) {
            if (encoded != 0) return false;
            continue;
        }
        if (encoded == 0) return false;
        const std::size_t asset = static_cast<std::size_t>(encoded - 1U);
        if (asset >= active_custom_tab_count() ||
            !catalogue_icon_has_retained_raster(asset)) {
            return false;
        }
        output.assets[bit] = encoded;
        output.resources[bit] = retained_rgba8_resource(asset);
        if (!output.resources[bit]) return false;
    }
    return output.injected_mask ==
            (output.rgba_mask | output.vector_mask) &&
        (output.rgba_mask & output.vector_mask) == 0;
}

[[nodiscard]] bool same_injected_host_layout(
    const InjectedHostLayout& left,
    const InjectedHostLayout& right) noexcept {
    if (left.injected_mask != right.injected_mask ||
        left.rgba_mask != right.rgba_mask ||
        left.vector_mask != right.vector_mask) {
        return false;
    }
    for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
        if (left.assets[bit] != right.assets[bit] ||
            left.resources[bit] != right.resources[bit]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool exact_initial_catalogue_upgrade(
    const InjectedHostLayout& source,
    const InjectedHostLayout& target) noexcept {
    const bool topology_exact = source.rgba_mask == 0 &&
        target.rgba_mask != 0 &&
        source.injected_mask == target.injected_mask &&
        source.vector_mask ==
            (target.vector_mask | target.rgba_mask) &&
        (target.vector_mask & target.rgba_mask) == 0;
    if (!topology_exact) return false;

    for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
        const TabHostMask mask = TabHostMask{1} << bit;
        if (source.assets[bit] != 0 || source.resources[bit] != nullptr ||
            (((target.rgba_mask & mask) != 0) !=
                (target.assets[bit] != 0)) ||
            (((target.rgba_mask & mask) != 0) !=
                (target.resources[bit] != nullptr))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool pending_requests_catalogue_presentation_refresh(
    void* top,
    const TabViewport& viewport) noexcept {
    const PendingTransition& pending = g_runtime.pending;
    return pending.active && pending.presentation_refresh &&
        pending.top == top && g_runtime.active && g_runtime.top == top &&
        pending.candidate.mapping_generation() ==
            viewport.mapping_generation() &&
        pending.candidate.window_start() == viewport.window_start() &&
        pending.candidate.selected_logical() ==
            viewport.selected_logical();
}

[[nodiscard]] bool pending_requests_tab_projection_refresh(
    void* top,
    const TabViewport& viewport) noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const PendingTransition& pending = g_runtime.pending;
    return pending.active && pending.projection_refresh &&
        pending.projection_generation && pending.top == top &&
        g_runtime.active && g_runtime.top == top &&
        pending.candidate.mapping_generation() ==
            viewport.mapping_generation() &&
        pending.candidate.window_start() == viewport.window_start() &&
        pending.candidate.selected_logical() == viewport.selected_logical();
#else
    (void)top;
    (void)viewport;
    return false;
#endif
}

[[nodiscard]] bool shape_replacement_plan(
    void* top,
    const TabViewport& viewport,
    Rgba8ReplacementPlan& output) noexcept {
    const bool catalogue_presentation_refresh =
        pending_requests_catalogue_presentation_refresh(top, viewport);
    const bool tab_projection_refresh =
        pending_requests_tab_projection_refresh(top, viewport);
    const bool transition_required = g_runtime.active &&
        g_runtime.top == top &&
        (catalogue_presentation_refresh || tab_projection_refresh ||
            tab_overflow_indicators_changed(g_runtime.viewport, viewport) ||
            viewport.mapping_generation() !=
                g_runtime.viewport.mapping_generation());
    if (!transition_required ||
        g_runtime.completed_shape_replacement_transitions ==
            std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }

    InjectedHostLayout source{};
    InjectedHostLayout target{};
    if (!snapshot_injected_host_layout(source) ||
        !derive_injected_host_layout(viewport, target)) {
        return false;
    }

    if (catalogue_presentation_refresh) {
        // This deferred first-epoch repair is deliberately narrower than a
        // generic same-mapping repaint. Before catalogue publication every
        // injected host is a vector Shape. Afterwards, only visible custom
        // hosts may become RGBA8; overflow markers remain vectors. Refuse any
        // other ownership topology before touching the display tree.
        if (!exact_initial_catalogue_upgrade(source, target)) return false;
    }

    output = {};
    output.transition_sequence =
        g_runtime.completed_shape_replacement_transitions + 1U;
    output.source_window = g_runtime.viewport.window_start();
    output.target_window = viewport.window_start();
    output.source_injected_mask = source.injected_mask;
    output.source_rgba_mask = source.rgba_mask;
    output.source_vector_mask = source.vector_mask;
    output.target_injected_mask = target.injected_mask;
    output.target_rgba_mask = target.rgba_mask;
    output.target_vector_mask = target.vector_mask;
    // Preserve a Shape only when the same physical host keeps the exact same
    // presentation kind and, for raster content, the exact immutable asset.
    // Everything else is staged in the inactive property bank before its
    // source bank is retired.
    for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
        const TabHostMask mask = TabHostMask{1} << bit;
        const bool same_vector = (source.vector_mask & mask) != 0 &&
            (target.vector_mask & mask) != 0;
        const bool same_raster = (source.rgba_mask & mask) != 0 &&
            (target.rgba_mask & mask) != 0 &&
            source.assets[bit] != 0 &&
            source.assets[bit] == target.assets[bit] &&
            erui::detail::rgba8_resource_identity_preserved(
                source.resources[bit], target.resources[bit]);
        if (same_vector || same_raster) {
            output.preserved_host_mask |= mask;
        }
    }
    output.raster_detach_mask =
        source.rgba_mask & ~output.preserved_host_mask;
    output.vector_detach_mask =
        source.vector_mask & ~output.preserved_host_mask;
    output.expected_detach_mask =
        source.injected_mask & ~output.preserved_host_mask;
    output.retired_injected_mask = output.preserved_host_mask;
    output.fresh_host_mask =
        target.injected_mask & ~output.preserved_host_mask;
    output.audit_mask = active_tab_host_mask();
    output.source_assets = source.assets;
    output.target_assets = target.assets;
    output.source_resources = source.resources;
    output.target_resources = target.resources;
    return true;
}

bool apply_mapping(
    void* top,
    const TabViewport& viewport,
    ResolvedTabLayers& layers,
    bool allow_transparent_initial_raster_placeholder = false) noexcept {
    std::array<RecordFields, kPhysicalTabCapacity> previous{};
    std::array<RecordFields, kPhysicalTabCapacity> desired{};
    if (!snapshot_record_fields(top, previous) ||
        !build_mapping_fields(viewport, desired)) {
        destroy_tab_layers(layers);
        return false;
    }

    Rgba8ReplacementPlan replacement_plan{};
    const bool transition_required = g_runtime.active &&
        g_runtime.top == top &&
        (pending_requests_catalogue_presentation_refresh(top, viewport) ||
            pending_requests_tab_projection_refresh(top, viewport) ||
            tab_overflow_indicators_changed(g_runtime.viewport, viewport) ||
            viewport.mapping_generation() !=
                g_runtime.viewport.mapping_generation());
    const bool shape_replacement_transition =
        shape_replacement_plan(top, viewport, replacement_plan);
    if (transition_required && !shape_replacement_transition) {
        destroy_tab_layers(layers);
        log_failure(
            "viewport mutation required a fresh-Shape transaction, but no exact transition plan could be derived; no record or display mutation was attempted");
        return false;
    }
    if (shape_replacement_transition) {
        InPlaceRgba8RebindTransaction& transaction =
            g_in_place_rgba8_rebind_transaction;
        if (!transaction.active) {
            const std::uint64_t phase_a_started_qpc =
                query_performance_ticks();
            const TabHostMask injected = g_injected_icon_hosts.load(
                std::memory_order_acquire);
            const TabHostMask rgba = g_rgba8_icon_hosts.load(
                std::memory_order_acquire);
            bool assets_exact = true;
            for (std::size_t bit = 0;
                 bit < active_tab_host_count();
                 ++bit) {
                const std::size_t expected =
                    replacement_plan.source_assets[bit];
                assets_exact = assets_exact &&
                    g_rgba8_host_assets[bit].load(
                        std::memory_order_acquire) == expected;
                if (expected != 0U) {
                    assets_exact = assets_exact &&
                        retained_rgba8_resource(expected - 1U) ==
                            replacement_plan.source_resources[bit];
                }
            }
            bool debts_zero = all_rgba8_retirement_debts_zero();
            InjectedMovieFingerprint current_fingerprint{};
            const bool fingerprint_sampled = sample_current_movie_fingerprint(
                top, current_fingerprint);
            const bool fingerprint_unset =
                !g_injected_movie_fingerprint.object_interface &&
                !g_injected_movie_fingerprint.owner_link &&
                !g_injected_movie_fingerprint.movie_root;
            if (fingerprint_sampled && fingerprint_unset &&
                replacement_plan.source_injected_mask == 0) {
                g_injected_movie_fingerprint = current_fingerprint;
            }
            const bool fingerprint_exact = fingerprint_sampled &&
                same_movie_fingerprint(
                    current_fingerprint,
                    g_injected_movie_fingerprint);
            const bool refs_exact =
                reconcile_rgba8_retirement_debt(
                    "shape-replacement-source-preflight") &&
                rgba8_references_match_tracked_hosts(
                    "shape-replacement-source-preflight");
            debts_zero = all_rgba8_retirement_debts_zero();
            const bool staging_empty =
                g_staged_injected_icon_hosts.load(
                    std::memory_order_acquire) == 0 &&
                g_staged_rgba8_icon_hosts.load(
                    std::memory_order_acquire) == 0;
            bool staged_assets_empty = true;
            for (std::size_t bit = 0;
                 bit < active_tab_host_count();
                 ++bit) {
                staged_assets_empty = staged_assets_empty &&
                    g_staged_rgba8_host_assets[bit].load(
                        std::memory_order_acquire) == 0;
            }
            const bool publication_state_compatible =
                !g_rgba8_publication_disabled.load(
                    std::memory_order_acquire) ||
                (replacement_plan.source_rgba_mask |
                    replacement_plan.target_rgba_mask) == 0;
            const bool source_ledger_exact =
                injected == replacement_plan.source_injected_mask &&
                rgba == replacement_plan.source_rgba_mask && assets_exact &&
                staging_empty && staged_assets_empty &&
                fingerprint_exact && refs_exact &&
                publication_state_compatible;
            TabHostMask property_state_mask{};
            if (source_ledger_exact) {
                const auto preflight = [&property_state_mask, top](
                    TabLayer layer,
                    std::size_t physical,
                    InPlaceRebindHostExpectation expectation,
                    std::size_t expected_asset = 0) noexcept {
                    const std::size_t bit = physical +
                        (layer == TabLayer::back
                            ? active_physical_tab_count()
                            : 0);
                    const std::uint8_t active_bank =
                        active_icon_property_bank(bit);
                    const bool exact = preflight_in_place_rgba8_rebind_host(
                            top,
                            layer,
                            physical,
                            expectation,
                            expected_asset,
                            active_bank) &&
                        preflight_in_place_rgba8_rebind_host(
                            top,
                            layer,
                            physical,
                            InPlaceRebindHostExpectation::untracked,
                            0,
                            static_cast<std::uint8_t>(active_bank ^ 1U),
                            false);
                    if (exact) {
                        property_state_mask |= TabHostMask{1} << bit;
                    }
                };
                for (std::size_t bit = 0;
                     bit < active_tab_host_count();
                     ++bit) {
                    const TabHostMask mask = TabHostMask{1} << bit;
                    if ((replacement_plan.audit_mask & mask) == 0) continue;
                    const TabLayer layer = bit < active_physical_tab_count()
                        ? TabLayer::front
                        : TabLayer::back;
                    const std::size_t physical =
                        bit % active_physical_tab_count();
                    if ((replacement_plan.source_rgba_mask & mask) != 0) {
                        preflight(
                            layer,
                            physical,
                            InPlaceRebindHostExpectation::rgba8_shape,
                            replacement_plan.source_assets[bit]);
                    } else if ((replacement_plan.source_vector_mask & mask) !=
                        0) {
                        preflight(
                            layer,
                            physical,
                            InPlaceRebindHostExpectation::vector_shape);
                    } else {
                        preflight(
                            layer,
                            physical,
                            InPlaceRebindHostExpectation::untracked);
                    }
                }
            }
            const bool property_states_exact =
                property_state_mask == replacement_plan.audit_mask;
            if (!source_ledger_exact || !property_states_exact) {
                destroy_tab_layers(layers);
                log_failure(
                    "generic RGBA8 replacement transition %llu window %zu->%zu source preflight failed; no record or display mutation was attempted (injected=0x%04X expected=0x%04X rgba=0x%04X expected=0x%04X assets=%u stagingEmpty=%u debtsZero=%u fingerprint=%u refs=%u sourceLedger=%u propertyStates=0x%04X/0x%04X)",
                    static_cast<unsigned long long>(
                        replacement_plan.transition_sequence),
                    replacement_plan.source_window,
                    replacement_plan.target_window,
                    static_cast<unsigned>(injected),
                    static_cast<unsigned>(replacement_plan.source_injected_mask),
                    static_cast<unsigned>(rgba),
                    static_cast<unsigned>(replacement_plan.source_rgba_mask),
                    assets_exact ? 1U : 0U,
                    (staging_empty && staged_assets_empty) ? 1U : 0U,
                    debts_zero ? 1U : 0U,
                    fingerprint_exact ? 1U : 0U,
                    refs_exact ? 1U : 0U,
                    source_ledger_exact ? 1U : 0U,
                    static_cast<unsigned>(property_state_mask),
                    static_cast<unsigned>(replacement_plan.audit_mask));
                return false;
            }

            transaction = {};
            transaction.active = true;
            transaction.transition_sequence =
                replacement_plan.transition_sequence;
            transaction.stage = InPlaceRgba8RebindStage::clearing;
            transaction.top = top;
            transaction.presentation_epoch =
                g_runtime.presentation_epoch;
            transaction.source_generation =
                g_runtime.viewport.mapping_generation();
            transaction.target_generation = viewport.mapping_generation();
            transaction.source_window =
                g_runtime.viewport.window_start();
            transaction.target_window = viewport.window_start();
            transaction.clear_frame = g_composite_frame_sequence;
            transaction.requested_qpc = g_runtime.pending.requested_qpc != 0
                ? g_runtime.pending.requested_qpc
                : phase_a_started_qpc;
            transaction.phase_a_started_qpc = phase_a_started_qpc;
            transaction.expected_clear_mask =
                replacement_plan.expected_detach_mask;
            transaction.expected_raster_clear_mask =
                replacement_plan.raster_detach_mask;
            transaction.expected_vector_clear_mask =
                replacement_plan.vector_detach_mask;
            transaction.source_injected_mask =
                replacement_plan.source_injected_mask;
            transaction.source_rgba_mask = replacement_plan.source_rgba_mask;
            transaction.retired_injected_mask =
                replacement_plan.retired_injected_mask;
            transaction.target_injected_mask =
                replacement_plan.target_injected_mask;
            transaction.target_rgba_mask = replacement_plan.target_rgba_mask;
            transaction.audit_mask = replacement_plan.audit_mask;
            transaction.expected_host_mask =
                replacement_plan.fresh_host_mask;
            transaction.expected_rgba_presentation_mask =
                replacement_plan.target_rgba_mask;
            transaction.expected_vector_host_mask =
                replacement_plan.target_vector_mask &
                replacement_plan.fresh_host_mask;
            transaction.preserved_host_mask =
                replacement_plan.preserved_host_mask;
            transaction.source_assets = replacement_plan.source_assets;
            transaction.target_assets = replacement_plan.target_assets;
            transaction.source_resources =
                replacement_plan.source_resources;
            transaction.target_resources =
                replacement_plan.target_resources;

            // Debt from an earlier, already-committed bank may outlive its
            // display object for a renderer frame.  Wait before staging a new
            // overlap, but leave every current source Shape attached and
            // visible throughout that wait.
            if (!debts_zero) {
                transaction.stage =
                    InPlaceRgba8RebindStage::waiting_retirement;
                if (!destroy_tab_layers(layers)) {
                    transaction.failed = true;
                    return false;
                }
                log_line(
                    "rgba8Handoff transition=%llu waiting for prior renderer debt with source banks still visible",
                    static_cast<unsigned long long>(
                        transaction.transition_sequence));
                return true;
            }

            // No display object has been detached.  The target repaint will
            // attach every changed target into the inactive bank, above the
            // still-visible source, before source retirement begins.
            transaction.detach_completed_qpc =
                query_performance_ticks();
            transaction.retirement_completed_qpc =
                transaction.detach_completed_qpc;
            transaction.stage = InPlaceRgba8RebindStage::ready_to_repaint;
            log_line(
                "rgba8Handoff transition=%llu source preserved; atomic inactive-bank target repaint armed",
                static_cast<unsigned long long>(
                    transaction.transition_sequence));
        }

        const bool matching_ready_transaction = transaction.active &&
            !transaction.failed &&
            transaction.stage ==
                InPlaceRgba8RebindStage::ready_to_repaint &&
            transaction.top == top &&
            transaction.presentation_epoch ==
                g_runtime.presentation_epoch &&
            transaction.source_generation ==
                g_runtime.viewport.mapping_generation() &&
            transaction.target_generation == viewport.mapping_generation() &&
            transaction.source_window ==
                g_runtime.viewport.window_start() &&
            transaction.target_window == viewport.window_start() &&
            transaction.completed_clear_mask == 0;
        const TabHostMask injected = g_injected_icon_hosts.load(
            std::memory_order_acquire);
        const TabHostMask rgba = g_rgba8_icon_hosts.load(
            std::memory_order_acquire);
        bool source_assets_exact = true;
        bool staged_assets_empty = true;
        for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
            source_assets_exact = source_assets_exact &&
                g_rgba8_host_assets[bit].load(
                    std::memory_order_acquire) ==
                    transaction.source_assets[bit];
            if (transaction.source_assets[bit] != 0U) {
                source_assets_exact = source_assets_exact &&
                    retained_rgba8_resource(
                        transaction.source_assets[bit] - 1U) ==
                        transaction.source_resources[bit];
            }
            staged_assets_empty = staged_assets_empty &&
                g_staged_rgba8_host_assets[bit].load(
                    std::memory_order_acquire) == 0;
        }
        const bool debts_zero = all_rgba8_retirement_debts_zero();
        InjectedMovieFingerprint current_fingerprint{};
        const bool fingerprint_exact =
            sample_current_movie_fingerprint(top, current_fingerprint) &&
            same_movie_fingerprint(
                current_fingerprint,
                g_injected_movie_fingerprint);
        const bool refs_exact = debts_zero &&
            rgba8_references_match_tracked_hosts(
                "shape-replacement-ready");
        const bool publication_state_compatible =
            !g_rgba8_publication_disabled.load(
                std::memory_order_acquire) ||
            (transaction.source_rgba_mask |
                transaction.target_rgba_mask) == 0;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        const bool resource_bank_publishable =
            !g_runtime.pending.icon_resource_refresh ||
            staged_rgba8_bank_publishable();
#else
        constexpr bool resource_bank_publishable = true;
#endif
        const bool ready_ledger_exact = matching_ready_transaction &&
            injected == transaction.source_injected_mask &&
            rgba == transaction.source_rgba_mask && source_assets_exact &&
            g_staged_injected_icon_hosts.load(
                std::memory_order_acquire) == 0 &&
            g_staged_rgba8_icon_hosts.load(
                std::memory_order_acquire) == 0 && staged_assets_empty &&
            fingerprint_exact && refs_exact &&
            publication_state_compatible && resource_bank_publishable;
        TabHostMask property_state_mask{};
        if (ready_ledger_exact) {
            const auto preflight = [&property_state_mask, top](
                TabLayer layer,
                std::size_t physical,
                InPlaceRebindHostExpectation expectation,
                std::size_t expected_asset = 0) noexcept {
                const std::size_t bit = physical +
                    (layer == TabLayer::back
                        ? active_physical_tab_count()
                        : 0);
                const std::uint8_t active_bank =
                    active_icon_property_bank(bit);
                const bool exact = preflight_in_place_rgba8_rebind_host(
                        top,
                        layer,
                        physical,
                        expectation,
                        expected_asset,
                        active_bank) &&
                    preflight_in_place_rgba8_rebind_host(
                        top,
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::untracked,
                        0,
                        static_cast<std::uint8_t>(active_bank ^ 1U),
                        false);
                if (exact) {
                    property_state_mask |= TabHostMask{1} << bit;
                }
            };
            for (std::size_t bit = 0;
                 bit < active_tab_host_count();
                 ++bit) {
                const TabHostMask mask = TabHostMask{1} << bit;
                if ((transaction.audit_mask & mask) == 0) continue;
                const TabLayer layer = bit < active_physical_tab_count()
                    ? TabLayer::front
                    : TabLayer::back;
                const std::size_t physical =
                    bit % active_physical_tab_count();
                if ((transaction.source_rgba_mask & mask) != 0) {
                    preflight(
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::rgba8_shape,
                        transaction.source_assets[bit]);
                } else if ((transaction.source_injected_mask & mask) != 0) {
                    preflight(
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::vector_shape);
                } else {
                    preflight(
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::untracked);
                }
            }
        }
        if (!ready_ledger_exact ||
            property_state_mask != transaction.audit_mask) {
            transaction.failed = true;
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
            g_runtime.destructive_transition_failed = true;
            destroy_tab_layers(layers);
            log_failure(
                "atomic RGBA8 handoff preflight failed (ready:%u injected:0x%04X rgba:0x%04X sourceAssets:%u stagedEmpty:%u debtsZero:%u fingerprint:%u refs:%u resourceBank:%u properties:0x%04X/0x%04X)",
                matching_ready_transaction ? 1U : 0U,
                static_cast<unsigned>(injected),
                static_cast<unsigned>(rgba),
                source_assets_exact ? 1U : 0U,
                staged_assets_empty ? 1U : 0U,
                debts_zero ? 1U : 0U,
                fingerprint_exact ? 1U : 0U,
                refs_exact ? 1U : 0U,
                resource_bank_publishable ? 1U : 0U,
                static_cast<unsigned>(property_state_mask),
                static_cast<unsigned>(transaction.audit_mask));
            return false;
        }

        transaction.repaint_started_qpc = query_performance_ticks();
        transaction.stage = InPlaceRgba8RebindStage::repainting;
        if (!write_record_fields(top, desired)) {
            transaction.failed = true;
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
            g_runtime.destructive_transition_failed = true;
            destroy_tab_layers(layers);
            log_failure(
                "fresh-Shape RGBA8 replacement could not publish its target records after retirement; later remaps are frozen");
            return false;
        }

        const bool repainted = repaint_tab_layers(top, viewport, layers);
        // At this point every changed custom target is attached in the
        // inactive bank and is above the still-visible source Shape.  Prove
        // the complete staged set before removing a single source child.
        const TabHostMask expected_staged_rgba =
            transaction.target_rgba_mask & transaction.expected_host_mask;
        bool staged_assets_exact = true;
        for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
            const TabHostMask mask = TabHostMask{1} << bit;
            const std::size_t expected =
                (expected_staged_rgba & mask) != 0
                ? transaction.target_assets[bit]
                : 0;
            staged_assets_exact = staged_assets_exact &&
                g_staged_rgba8_host_assets[bit].load(
                    std::memory_order_acquire) == expected;
            #if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            if (expected != 0U && g_runtime.pending.icon_resource_refresh) {
                staged_assets_exact = staged_assets_exact &&
                    g_rgba8_staged_bank_ready &&
                    expected - 1U <
                        g_rgba8_staged_resources_scratch.size() &&
                    g_rgba8_staged_resources_scratch[expected - 1U]
                            .resource ==
                        transaction.target_resources[bit];
            }
            #endif
        }
        const bool staged_exact = repainted && !transaction.failed &&
            g_staged_injected_icon_hosts.load(
                std::memory_order_acquire) ==
                transaction.expected_host_mask &&
            g_staged_rgba8_icon_hosts.load(
                std::memory_order_acquire) == expected_staged_rgba &&
            staged_assets_exact &&
            transaction.completed_host_mask ==
                transaction.expected_host_mask &&
            transaction.completed_rgba_presentation_mask ==
                transaction.expected_rgba_presentation_mask &&
            transaction.completed_vector_host_mask ==
                transaction.expected_vector_host_mask;

        bool handoff_exact = staged_exact;
        for (std::size_t bit = 0;
             bit < active_tab_host_count() && handoff_exact;
             ++bit) {
            const TabHostMask mask = TabHostMask{1} << bit;
            if ((transaction.expected_vector_clear_mask & mask) == 0) {
                continue;
            }
            const TabLayer layer = bit < active_physical_tab_count()
                ? TabLayer::front
                : TabLayer::back;
            const bool target_is_injected =
                (transaction.target_injected_mask & mask) != 0;
            handoff_exact = detach_vector_host_for_replacement(
                top,
                layer,
                bit % active_physical_tab_count(),
                transaction,
                !target_is_injected);
        }
        for (std::size_t bit = 0;
             bit < active_tab_host_count() && handoff_exact;
             ++bit) {
            const TabHostMask mask = TabHostMask{1} << bit;
            if ((transaction.expected_raster_clear_mask & mask) == 0) {
                continue;
            }
            const TabLayer layer = bit < active_physical_tab_count()
                ? TabLayer::front
                : TabLayer::back;
            const bool target_is_injected =
                (transaction.target_injected_mask & mask) != 0;
            handoff_exact = detach_rgba8_host_for_replacement(
                top,
                layer,
                bit % active_physical_tab_count(),
                transaction,
                !target_is_injected);
        }

        // Publish inactive banks only after all old banks were removed.  The
        // bank bit and ownership ledgers change together inside this callback;
        // no renderer boundary can observe an authored-icon-only state.
        if (handoff_exact && transaction.completed_clear_mask ==
                transaction.expected_clear_mask) {
            const bool icon_bank_published =
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
                !g_runtime.pending.icon_resource_refresh ||
                publish_staged_rgba8_bank_after_handoff();
#else
                true;
#endif
            if (!icon_bank_published) {
                handoff_exact = false;
            }
        }
        if (handoff_exact && transaction.completed_clear_mask ==
                transaction.expected_clear_mask) {
            TabHostMask bank_b = g_icon_bank_b_hosts.load(
                std::memory_order_acquire);
            for (std::size_t bit = 0;
                 bit < active_tab_host_count();
                 ++bit) {
                const TabHostMask mask = TabHostMask{1} << bit;
                if ((transaction.expected_host_mask & mask) == 0) continue;
                const std::uint8_t old_bank = active_icon_property_bank(bit);
                if ((old_bank ^ 1U) == kIconPropertyBankB) {
                    bank_b |= mask;
                } else {
                    bank_b &= ~mask;
                }
                g_injected_icon_hosts.fetch_or(
                    mask, std::memory_order_acq_rel);
                if ((expected_staged_rgba & mask) != 0) {
                    g_rgba8_icon_hosts.fetch_or(
                        mask, std::memory_order_acq_rel);
                    g_rgba8_host_assets[bit].store(
                        transaction.target_assets[bit],
                        std::memory_order_release);
                }
                g_staged_injected_icon_hosts.fetch_and(
                    ~mask,
                    std::memory_order_acq_rel);
                g_staged_rgba8_icon_hosts.fetch_and(
                    ~mask,
                    std::memory_order_acq_rel);
                g_staged_rgba8_host_assets[bit].store(
                    0, std::memory_order_release);
            }
            g_icon_bank_b_hosts.store(bank_b, std::memory_order_release);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            if (g_runtime.pending.icon_resource_refresh) {
                finish_staged_rgba8_bank_publication();
            }
#endif
        } else {
            handoff_exact = false;
        }
        transaction.detach_completed_qpc = query_performance_ticks();

        TabHostMask postflight_property_mask{};
        if (handoff_exact) {
            const auto postflight = [&](
                TabLayer layer,
                std::size_t physical,
                InPlaceRebindHostExpectation expectation,
                std::size_t expected_asset = 0) noexcept {
                const std::size_t bit = physical +
                    (layer == TabLayer::back
                        ? active_physical_tab_count()
                        : 0);
                const std::uint8_t active_bank =
                    active_icon_property_bank(bit);
                const bool exact = preflight_in_place_rgba8_rebind_host(
                        top,
                        layer,
                        physical,
                        expectation,
                        expected_asset,
                        active_bank) &&
                    preflight_in_place_rgba8_rebind_host(
                        top,
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::untracked,
                        0,
                        static_cast<std::uint8_t>(active_bank ^ 1U),
                        false);
                if (exact) {
                    postflight_property_mask |= TabHostMask{1} << bit;
                }
            };
            for (std::size_t bit = 0;
                 bit < active_tab_host_count();
                 ++bit) {
                const TabHostMask mask = TabHostMask{1} << bit;
                if ((transaction.audit_mask & mask) == 0) continue;
                const TabLayer layer = bit < active_physical_tab_count()
                    ? TabLayer::front
                    : TabLayer::back;
                const std::size_t physical =
                    bit % active_physical_tab_count();
                if ((transaction.target_rgba_mask & mask) != 0) {
                    postflight(
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::rgba8_shape,
                        transaction.target_assets[bit]);
                } else if ((transaction.target_injected_mask & mask) != 0) {
                    postflight(
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::vector_shape);
                } else {
                    postflight(
                        layer,
                        physical,
                        InPlaceRebindHostExpectation::untracked);
                }
            }
        }
        const InPlaceRgba8RebindTransaction completed =
            g_in_place_rgba8_rebind_transaction;
        g_in_place_rgba8_rebind_transaction = {};
        const TabHostMask target_injected =
            g_injected_icon_hosts.load(std::memory_order_acquire);
        const TabHostMask target_rgba = g_rgba8_icon_hosts.load(
            std::memory_order_acquire);
        bool target_assets_exact = true;
        for (std::size_t bit = 0;
             bit < active_tab_host_count();
             ++bit) {
            const std::size_t expected = completed.target_assets[bit];
            target_assets_exact = target_assets_exact &&
                g_rgba8_host_assets[bit].load(std::memory_order_acquire) ==
                    expected;
            if (expected != 0U) {
                target_assets_exact = target_assets_exact &&
                    retained_rgba8_resource(expected - 1U) ==
                        completed.target_resources[bit];
            }
        }
        const bool final_refs_exact =
            rgba8_references_match_tracked_hosts(
                "shape-replacement-final");
        const bool transition_complete = handoff_exact && !completed.failed &&
            completed.transition_sequence ==
                g_runtime.completed_shape_replacement_transitions + 1U &&
            completed.completed_clear_mask ==
                completed.expected_clear_mask &&
            completed.completed_host_mask ==
                completed.expected_host_mask &&
            completed.completed_rgba_presentation_mask ==
                completed.expected_rgba_presentation_mask &&
            completed.completed_vector_host_mask ==
                completed.expected_vector_host_mask &&
            target_injected == completed.target_injected_mask &&
            target_rgba == completed.target_rgba_mask &&
            target_assets_exact && final_refs_exact &&
            postflight_property_mask ==
                completed.audit_mask;
        if (transition_complete) {
            g_runtime.completed_shape_replacement_transitions =
                completed.transition_sequence;
            if (completed.target_injected_mask == 0) {
                g_injected_movie_fingerprint = {};
            }
        } else {
            // A destructive detach or target repaint may already have changed
            // display-tree ownership. Preserve the target records/ledgers and
            // reject every later remap rather than attempting an unsafe undo.
            g_runtime.destructive_transition_failed = true;
        }
        const std::uint64_t transition_completed_qpc =
            query_performance_ticks();
        const bool timing_qpc_valid = completed.requested_qpc != 0 &&
            completed.phase_a_started_qpc >= completed.requested_qpc &&
            completed.retirement_completed_qpc >=
                completed.phase_a_started_qpc &&
            completed.repaint_started_qpc >=
                completed.retirement_completed_qpc &&
            completed.detach_completed_qpc >=
                completed.repaint_started_qpc &&
            transition_completed_qpc >= completed.detach_completed_qpc;
        const std::uint64_t transition_microseconds =
            elapsed_qpc_microseconds(
                completed.requested_qpc, transition_completed_qpc);
        const std::uint64_t queued_microseconds = elapsed_qpc_microseconds(
            completed.requested_qpc, completed.phase_a_started_qpc);
        const std::uint64_t phase_a_microseconds = elapsed_qpc_microseconds(
            completed.phase_a_started_qpc,
            completed.retirement_completed_qpc);
        const std::uint64_t retirement_microseconds =
            elapsed_qpc_microseconds(
                completed.detach_completed_qpc,
                transition_completed_qpc);
        const std::uint64_t ready_wait_microseconds =
            elapsed_qpc_microseconds(
                completed.retirement_completed_qpc,
                completed.repaint_started_qpc);
        const std::uint64_t repaint_microseconds = elapsed_qpc_microseconds(
            completed.repaint_started_qpc, completed.detach_completed_qpc);
        log_line(
            "STRESS transition sequence=%llu source={window:%zu generation:%llu injected:0x%04X rgba:0x%04X} target={window:%zu generation:%llu injected:0x%04X/0x%04X rgba:0x%04X/0x%04X} proof={detach:0x%04X/0x%04X fresh:0x%04X/0x%04X assets:%u refs:%u properties:0x%04X/0x%04X complete:%u frozen:%u} timingUs={valid:%u total:%llu queued:%llu phaseA:%llu retirement:%llu readyWait:%llu repaint:%llu frames:%llu}",
            static_cast<unsigned long long>(
                completed.transition_sequence),
            completed.source_window,
            static_cast<unsigned long long>(completed.source_generation),
            static_cast<unsigned>(completed.source_injected_mask),
            static_cast<unsigned>(completed.source_rgba_mask),
            completed.target_window,
            static_cast<unsigned long long>(completed.target_generation),
            static_cast<unsigned>(target_injected),
            static_cast<unsigned>(completed.target_injected_mask),
            static_cast<unsigned>(target_rgba),
            static_cast<unsigned>(completed.target_rgba_mask),
            static_cast<unsigned>(completed.completed_clear_mask),
            static_cast<unsigned>(completed.expected_clear_mask),
            static_cast<unsigned>(completed.completed_host_mask),
            static_cast<unsigned>(completed.expected_host_mask),
            target_assets_exact ? 1U : 0U,
            final_refs_exact ? 1U : 0U,
            static_cast<unsigned>(postflight_property_mask),
            static_cast<unsigned>(completed.audit_mask),
            transition_complete ? 1U : 0U,
            g_runtime.destructive_transition_failed ? 1U : 0U,
            timing_qpc_valid ? 1U : 0U,
            static_cast<unsigned long long>(transition_microseconds),
            static_cast<unsigned long long>(queued_microseconds),
            static_cast<unsigned long long>(phase_a_microseconds),
            static_cast<unsigned long long>(retirement_microseconds),
            static_cast<unsigned long long>(ready_wait_microseconds),
            static_cast<unsigned long long>(repaint_microseconds),
            static_cast<unsigned long long>(
                completed.retirement_wait_frames));
        if (!transition_complete) {
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
            log_failure(
                "fresh-Shape RGBA8 replacement did not satisfy its final transition gate; target mapping was retained, later viewport remaps are frozen, and later RGBA8 publication is disabled");
        } else {
            log_line(
                "rgba8Replace transition %llu completed; arbitrary forward, reverse, presentation-only, and wrap remaps remain armed",
                static_cast<unsigned long long>(
                    completed.transition_sequence));
        }
        return true;
    }

    // Script-created children survive native timeline repainting. Remove every
    // tracked child while its current parent mapping is still valid, then let
    // the desired repaint recreate only the custom icons it actually needs.
    const InjectedIconsClearResult initial_clear_result =
        clear_injected_icons(
            top, allow_transparent_initial_raster_placeholder);
    const bool initial_clear_accepted =
        allow_transparent_initial_raster_placeholder
        ? accept_vector_only_epoch_clear_result(
              top, initial_clear_result, "initial-mapping")
        : initial_clear_result == InjectedIconsClearResult::cleared;
    if (!initial_clear_accepted) {
        destroy_tab_layers(layers);
        log_failure(
            "slot remapping aborted because injected icons could not be cleared");
        return false;
    }

    if (!write_record_fields(top, desired)) {
        destroy_tab_layers(layers);
        if (!restore_previous_mapping(
                top,
                previous,
                allow_transparent_initial_raster_placeholder)) {
            // write_record_fields() is deliberately guarded per field and
            // can therefore fail after publishing a prefix. If the rollback
            // also fails, some live records may still borrow title pointers
            // from g_runtime.catalogue. Freeze mutation and retain that
            // storage rather than letting initialization zero it.
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
            g_runtime.destructive_transition_failed = true;
            log_failure(
                "record publication and rollback both failed; live tab-record storage is retained and later remaps are frozen");
        }
        return false;
    }
    const bool repainted = [&]() noexcept {
        if (!allow_transparent_initial_raster_placeholder) {
            return repaint_tab_layers(top, viewport, layers);
        }
        ScopedBooleanTrue initial_target_repaint{g_initial_target_repaint};
        return repaint_tab_layers(top, viewport, layers);
    }();
    if (!repainted) {
        // Some desired-pass cells may already have consumed their temporary
        // proxies. Re-resolve a complete pair of layers for rollback so native
        // records and visible cells agree.
        if (!restore_previous_mapping(
                top,
                previous,
                allow_transparent_initial_raster_placeholder)) {
            g_rgba8_publication_disabled.store(
                true, std::memory_order_release);
            g_runtime.destructive_transition_failed = true;
            log_failure(
                "tab repaint and rollback both failed; live tab-record storage is retained and later remaps are frozen");
        }
        return false;
    }
    return true;
}

bool read_selected_physical(void* top, std::uint32_t& selected) noexcept {
    std::int32_t value{-1};
    if (!top || !guarded_read(
            static_cast<const std::byte*>(top) + kSelectorOffset +
                kSelectorSelectedIndexOffset,
            value) ||
        value < 0 || value >=
            static_cast<std::int32_t>(active_physical_tab_count())) {
        return false;
    }
    selected = static_cast<std::uint32_t>(value);
    return true;
}

PanelSnapshot inspect_panel_slot(const void* slot) noexcept {
    PanelSnapshot snapshot{};
    snapshot.pointer_read = guarded_read(slot, snapshot.panel);
    if (!snapshot.pointer_read || !snapshot.panel) return snapshot;

    snapshot.vtable_read = guarded_read(snapshot.panel, snapshot.vtable);
    snapshot.option_setting = snapshot.vtable_read &&
        snapshot.vtable == g_game.base + kOptionSettingPanelVtableRva;
    // Do not walk deep offsets on an unknown panel type. The pointer and
    // vtable are enough to diagnose a foreign cached object safely.
    if (!snapshot.option_setting) return snapshot;

    snapshot.fields_read = guarded_read(
        static_cast<const std::byte*>(snapshot.panel) +
            kPanelVisualCapacityOffset,
        snapshot.visual_capacity) &&
        guarded_read(
            static_cast<const std::byte*>(snapshot.panel) +
                kPanelNativeRowCountOffset,
            snapshot.native_row_count);
    return snapshot;
}

PanelCarrierSnapshot inspect_panel_carrier(
    const PanelSnapshot& panel) noexcept {
    PanelCarrierSnapshot snapshot{};
    if (!panel.option_setting || !panel.panel || !g_game.base ||
        g_game.size == 0) {
        return snapshot;
    }

    auto* const proxy = static_cast<std::byte*>(panel.panel) + 0x1200;
    if (!guarded_read(proxy, snapshot.proxy_vtable) ||
        snapshot.proxy_vtable !=
            g_game.base + kSceneObjProxyVtableRva ||
        !guarded_read(
            static_cast<const std::byte*>(snapshot.proxy_vtable) +
                sizeof(void*),
            snapshot.getter) ||
        snapshot.getter != g_game.base + kSceneObjProxyGetValueRva) {
        return snapshot;
    }

    using CarrierGetterFn = void*(__fastcall*)(void*);
#if defined(_MSC_VER)
    __try {
#endif
        snapshot.wrapper = reinterpret_cast<CarrierGetterFn>(snapshot.getter)(
            proxy);
        if (snapshot.wrapper && guarded_read(
                static_cast<const std::byte*>(snapshot.wrapper) + 0x20,
                snapshot.wrapper_flags) &&
            guarded_read(
                static_cast<const std::byte*>(snapshot.wrapper) + 0x28,
                snapshot.payload) &&
            snapshot.payload) {
            snapshot.valid = true;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
    }
#endif
    return snapshot;
}

void log_research_carrier_selection(
    void* top,
    const char* stage,
    std::size_t logical,
    std::int32_t category) noexcept {
    if (!top || !stage || !g_runtime.active || g_runtime.top != top ||
        !is_research_custom_category(category)) {
        return;
    }
    const std::uint32_t event =
        g_carrier_selection_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event > kMaximumCarrierSelectionLogs) return;

    auto* const composite = static_cast<std::byte*>(top) +
        kCompositeOffset;
    const PanelSnapshot current = inspect_panel_slot(
        composite + kCompositeCurrentPanelOffset);
    const PanelSnapshot cache4 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 4 * sizeof(void*));
    const PanelSnapshot cache8 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 8 * sizeof(void*));
    const PanelCarrierSnapshot current_carrier =
        inspect_panel_carrier(current);
    const PanelCarrierSnapshot carrier4 = inspect_panel_carrier(cache4);
    const PanelCarrierSnapshot carrier8 = inspect_panel_carrier(cache8);
    const bool both_present = carrier4.valid && carrier8.valid;
    const bool distinct = both_present && cache4.panel != cache8.panel &&
        carrier4.wrapper != carrier8.wrapper &&
        carrier4.payload != carrier8.payload;
    const bool current_exact = current_carrier.valid && carrier4.valid &&
        current.panel == cache4.panel &&
        current_carrier.wrapper == carrier4.wrapper &&
        current_carrier.payload == carrier4.payload;

    log_line(
        "carrierSelection[%u] stage=%s logical=%zu category=%d current=%p/%p/%p custom4=%p/%p/%p{cap:%u rows:%llu} stockGraphics8=%p/%p/%p{cap:%u rows:%llu} bothPresent=%u distinct={panel:%u wrapper:%u payload:%u exact:%u} currentExact=%u",
        static_cast<unsigned>(event),
        stage,
        logical,
        category,
        current.panel,
        current_carrier.wrapper,
        current_carrier.payload,
        cache4.panel,
        carrier4.wrapper,
        carrier4.payload,
        static_cast<unsigned>(cache4.visual_capacity),
        static_cast<unsigned long long>(cache4.native_row_count),
        cache8.panel,
        carrier8.wrapper,
        carrier8.payload,
        static_cast<unsigned>(cache8.visual_capacity),
        static_cast<unsigned long long>(cache8.native_row_count),
        both_present ? 1U : 0U,
        both_present && cache4.panel != cache8.panel ? 1U : 0U,
        both_present && carrier4.wrapper != carrier8.wrapper ? 1U : 0U,
        both_present && carrier4.payload != carrier8.payload ? 1U : 0U,
        distinct ? 1U : 0U,
        current_exact ? 1U : 0U);
}

bool resolve_panel_child(
    void* panel,
    const char* path,
    ResolvedTabProxy& proxy) noexcept {
    if (!panel || !path || !*path || !g_resolve_path ||
        !g_value_exists || !g_destroy_scaleform_value) {
        return false;
    }

    bool resolved{};
#if defined(_MSC_VER)
    __try {
#endif
        void* const source = static_cast<std::byte*>(panel) + 0x1200;
        void* const result = g_resolve_path(
            source, proxy.storage.data(), path);
        proxy.constructed = result == proxy.storage.data();
        resolved = proxy.constructed &&
            g_value_exists(proxy.storage.data());
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        resolved = false;
    }
#endif
    return resolved;
}

bool read_display_num_children(
    void* carrier_wrapper,
    std::uint32_t& count) noexcept {
    count = 0;
    if (!carrier_wrapper || !g_get_scaleform_member ||
        !g_destroy_scaleform_value) {
        return false;
    }

    alignas(16) std::array<std::byte, 0x38> value{};
    bool constructed{};
    bool valid{};
#if defined(_MSC_VER)
    __try {
#endif
        const char* name = "numChildren";
        constructed = g_get_scaleform_member(
            carrier_wrapper, value.data(), &name) == value.data();
        std::uint32_t flags{};
        std::uint32_t encoded{};
        valid = constructed &&
            guarded_read(value.data() + 0x20, flags) &&
            (flags & 0x8Fu) == 0x03u &&
            guarded_read(value.data() + 0x28, encoded);
        if (valid) count = encoded;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        valid = false;
    }
#endif
    destroy_local_scaleform_value(value, constructed, "numChildren");
    return valid;
}

bool get_display_child_at(
    const DisplayValueView& carrier,
    std::uint32_t index,
    std::array<std::byte, 0x38>& wrapper,
    bool& constructed,
    DisplayValueView& child) noexcept {
    constructed = false;
    child = {};
    if (!exact_as3_display_bridge(carrier) ||
        !g_construct_scaleform_value || !g_destroy_scaleform_value) {
        return false;
    }

    bool invoked{};
#if defined(_MSC_VER)
    __try {
#endif
        constructed = g_construct_scaleform_value(wrapper.data()) ==
            wrapper.data();
        if (constructed) {
            const RawGfxValue argument = make_int_value(
                static_cast<std::int32_t>(index));
            invoked = reinterpret_cast<ValueInvokeFn>(carrier.invoke_slot)(
                carrier.object_interface,
                carrier.object_payload,
                wrapper.data() + 0x08,
                "getChildAt",
                &argument,
                1,
                true);
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        invoked = false;
    }
#endif
    return invoked && inspect_display_value(wrapper.data(), child) &&
        exact_as3_display_bridge(child);
}

void preserve_active_aliased_carrier_visibility(
    safetyhook::Context& context) noexcept {
    if (!g_runtime.active || !g_runtime.top || !g_game.base) return;

    auto* const composite = reinterpret_cast<std::byte*>(context.rsi);
    auto* const expected_composite =
        static_cast<std::byte*>(g_runtime.top) + kCompositeOffset;
    if (!composite || composite != expected_composite) return;

    // The native loop has just compared this candidate panel with the active
    // panel and placed the desired visibility in DL. Never alter an existing
    // native show decision, and never touch any pair other than the two exact
    // wrappers used by Game Options (cache 0) and the research tab (cache 4).
    const bool native_visible = (context.rdx & 0xFFu) != 0;
    if (native_visible) return;

    void* active{};
    if (!guarded_read(
            composite + kCompositeCurrentPanelOffset,
            active) ||
        !active) {
        return;
    }

    const PanelSnapshot cache0 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 0 * sizeof(void*));
    const PanelSnapshot cache4 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 4 * sizeof(void*));
    void* const candidate = reinterpret_cast<void*>(context.rcx);
    const void* const candidate_slot =
        reinterpret_cast<const void*>(context.rbx);
    const void* const cache0_slot =
        composite + kCompositePanelCacheOffset + 0 * sizeof(void*);
    const void* const cache4_slot =
        composite + kCompositePanelCacheOffset + 4 * sizeof(void*);
    const std::uintptr_t candidate_category = context.rdi;
    if (!cache0.option_setting || !cache4.option_setting ||
        !cache0.panel || !cache4.panel ||
        cache0.panel == cache4.panel || !candidate) {
        return;
    }

    const bool exact_alias_pair =
        (active == cache0.panel && candidate == cache4.panel &&
            candidate_slot == cache4_slot && candidate_category == 4) ||
        (active == cache4.panel && candidate == cache0.panel &&
            candidate_slot == cache0_slot && candidate_category == 0);
    if (!exact_alias_pair) return;

    // Different C++ panel wrappers are allowed to alias only when their
    // validated SceneObj proxies resolve to the exact same live Scaleform
    // payload. If any part of that proof changes, preserve native behavior.
    const PanelCarrierSnapshot cache0_carrier =
        inspect_panel_carrier(cache0);
    const PanelCarrierSnapshot cache4_carrier =
        inspect_panel_carrier(cache4);
    if (!cache0_carrier.valid || !cache4_carrier.valid ||
        (cache0_carrier.wrapper_flags & 0x8Fu) < 2u ||
        (cache4_carrier.wrapper_flags & 0x8Fu) < 2u ||
        cache0_carrier.payload != cache4_carrier.payload) {
        return;
    }

    context.rdx = (context.rdx & ~std::uintptr_t{0xFF}) |
        std::uintptr_t{1};

    const std::uint32_t event =
        g_alias_visibility_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumAliasVisibilityLogs) {
        log_line(
            "aliasVisibility[%u] active=%p candidate=%p activeCategory=%u candidateCategory=%u payload=%p native=0 final=1",
            static_cast<unsigned>(event),
            active,
            candidate,
            active == cache0.panel ? 0u : 4u,
            candidate == cache0.panel ? 0u : 4u,
            cache0_carrier.payload);
    }
}

const char* panel_state_isolation_reason_name(
    PanelStateIsolationReason reason) noexcept {
    switch (reason) {
    case PanelStateIsolationReason::provisional:
        return "provisional";
    case PanelStateIsolationReason::corrective:
        return "corrective";
    case PanelStateIsolationReason::none:
        break;
    }
    return "none";
}

bool begin_panel_state_isolation(
    void* top,
    std::int32_t target_category,
    PanelStateIsolationReason reason) noexcept {
    if (!top || reason == PanelStateIsolationReason::none ||
        target_category < 0 || target_category >= 10 ||
        !g_runtime.active || g_runtime.top != top ||
        g_state_isolation_expectation.active) {
        return false;
    }

    auto* const composite =
        static_cast<std::byte*>(top) + kCompositeOffset;
    void* source{};
    if (!guarded_read(
            composite + kCompositeCurrentPanelOffset,
            source) ||
        !source) {
        return false;
    }
    void* cached_target{};
    if (!guarded_read(composite + kCompositePanelCacheOffset +
            static_cast<std::size_t>(target_category) * sizeof(void*), cached_target) ||
        cached_target == source) {
        return false;
    }

    g_state_isolation_expectation = {
        true,
        false,
        composite,
        source,
        target_category,
        reason,
    };
    return true;
}

void end_panel_state_isolation(bool armed) noexcept {
    if (!armed) return;
    if (g_state_isolation_expectation.active &&
        !g_state_isolation_expectation.consumed) {
        log_failure(
            "expected %s panel-state isolation did not reach the validated native copy seam",
            panel_state_isolation_reason_name(
                g_state_isolation_expectation.reason));
    }
    g_state_isolation_expectation = {};
}

void isolate_panel_state_copy(safetyhook::Context& context) noexcept {
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) return;
    if (!g_runtime.active || !g_runtime.top || !g_game.base) return;

    auto* const composite = reinterpret_cast<std::byte*>(context.rsi);
    void* const source = reinterpret_cast<void*>(context.rbx);
    void* const target = reinterpret_cast<void*>(context.rax);
    auto* const expected_composite =
        static_cast<std::byte*>(g_runtime.top) + kCompositeOffset;
    if (!composite || composite != expected_composite || !source ||
        !target || source == target) {
        return;
    }

    void* current{};
    if (!guarded_read(
            composite + kCompositeCurrentPanelOffset,
            current) ||
        current != target) {
        return;
    }

    std::int32_t source_category{-1};
    std::int32_t target_category{-1};
    PanelSnapshot source_snapshot{};
    PanelSnapshot target_snapshot{};
    for (std::int32_t category = 0; category < 10; ++category) {
        const void* const slot = composite + kCompositePanelCacheOffset +
            static_cast<std::size_t>(category) * sizeof(void*);
        const PanelSnapshot snapshot = inspect_panel_slot(slot);
        if (snapshot.panel == source && source_category < 0) {
            source_category = category;
            source_snapshot = snapshot;
        }
        if (snapshot.panel == target && target_category < 0) {
            target_category = category;
            target_snapshot = snapshot;
        }
    }
    if (source_category < 0 || target_category < 0 ||
        !source_snapshot.option_setting || !source_snapshot.fields_read ||
        !target_snapshot.option_setting || !target_snapshot.fields_read) {
        return;
    }

    PanelStateIsolationExpectation& expectation =
        g_state_isolation_expectation;
    const bool expected_transition = expectation.active &&
        expectation.composite == composite &&
        expectation.source == source &&
        expectation.target_category == target_category;
    if (!expected_transition) return;

    expectation.consumed = true;
    context.rip = reinterpret_cast<std::uintptr_t>(g_game.base) +
        kSwitchPanelStateCopyResumeRva;

    const std::uint32_t event =
        g_state_isolation_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumStateIsolationLogs) {
        log_line(
            "panelStateIsolation[%u] reason=%s sourceCategory=%d targetCategory=%d source=%p target=%p skipped=eldenring.exe+0x%llX..+0x%llX",
            static_cast<unsigned>(event),
            panel_state_isolation_reason_name(expectation.reason),
            source_category,
            target_category,
            source,
            target,
            static_cast<unsigned long long>(kSwitchPanelStateCopyRva),
            static_cast<unsigned long long>(
                kSwitchPanelStateCopyResumeRva));
    }
}

void log_wrap_panel_state(
    void* top,
    const char* stage,
    std::size_t previous_logical,
    std::size_t desired_logical) noexcept {
    if (!top || !stage) return;
    const std::uint32_t line =
        g_wrap_diagnostic_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (line > kMaximumWrapDiagnosticLogs) return;

    const auto* const composite =
        static_cast<const std::byte*>(top) + kCompositeOffset;
    const PanelSnapshot current = inspect_panel_slot(
        composite + kCompositeCurrentPanelOffset);
    const PanelSnapshot cache0 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 0 * sizeof(void*));
    const PanelSnapshot cache4 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 4 * sizeof(void*));
    const PanelSnapshot cache5 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 5 * sizeof(void*));
    const PanelSnapshot cache8 = inspect_panel_slot(
        composite + kCompositePanelCacheOffset + 8 * sizeof(void*));
    const PanelCarrierSnapshot current_carrier =
        inspect_panel_carrier(current);
    const PanelCarrierSnapshot cache0_carrier =
        inspect_panel_carrier(cache0);
    const PanelCarrierSnapshot cache4_carrier =
        inspect_panel_carrier(cache4);

    std::uint32_t selected{};
    const bool selector_read = read_selected_physical(top, selected);
    std::int32_t selected_category{-1};
    const bool category_read = selector_read && guarded_read(
        record_at(top, selected) + kCategoryIdOffset,
        selected_category);

    log_line(
        "wrapPanel[%u] logical=%zu->%zu stage=%s selector=%s%u selectedCategory=%s%d current=%p eq={0:%u 4:%u 5:%u 8:%u} currentPanel={vtable:%p option:%u fields:%u cap:%u rows:%llu} cache0=%p{vtable:%p option:%u fields:%u cap:%u rows:%llu} cache4=%p{vtable:%p option:%u fields:%u cap:%u rows:%llu} cache5=%p{option:%u fields:%u rows:%llu} cache8=%p{option:%u fields:%u rows:%llu} carriers={current:%p>%p/0x%X/%u cache0:%p>%p/0x%X/%u cache4:%p>%p/0x%X/%u same04:%u}",
        static_cast<unsigned>(line),
        previous_logical,
        desired_logical,
        stage,
        selector_read ? "" : "?",
        static_cast<unsigned>(selected),
        category_read ? "" : "?",
        selected_category,
        current.panel,
        current.panel && current.panel == cache0.panel ? 1u : 0u,
        current.panel && current.panel == cache4.panel ? 1u : 0u,
        current.panel && current.panel == cache5.panel ? 1u : 0u,
        current.panel && current.panel == cache8.panel ? 1u : 0u,
        current.vtable,
        current.option_setting ? 1u : 0u,
        current.fields_read ? 1u : 0u,
        static_cast<unsigned>(current.visual_capacity),
        static_cast<unsigned long long>(current.native_row_count),
        cache0.panel,
        cache0.vtable,
        cache0.option_setting ? 1u : 0u,
        cache0.fields_read ? 1u : 0u,
        static_cast<unsigned>(cache0.visual_capacity),
        static_cast<unsigned long long>(cache0.native_row_count),
        cache4.panel,
        cache4.vtable,
        cache4.option_setting ? 1u : 0u,
        cache4.fields_read ? 1u : 0u,
        static_cast<unsigned>(cache4.visual_capacity),
        static_cast<unsigned long long>(cache4.native_row_count),
        cache5.panel,
        cache5.option_setting ? 1u : 0u,
        cache5.fields_read ? 1u : 0u,
        static_cast<unsigned long long>(cache5.native_row_count),
        cache8.panel,
        cache8.option_setting ? 1u : 0u,
        cache8.fields_read ? 1u : 0u,
        static_cast<unsigned long long>(cache8.native_row_count),
        current_carrier.wrapper,
        current_carrier.payload,
        static_cast<unsigned>(current_carrier.wrapper_flags),
        current_carrier.valid ? 1u : 0u,
        cache0_carrier.wrapper,
        cache0_carrier.payload,
        static_cast<unsigned>(cache0_carrier.wrapper_flags),
        cache0_carrier.valid ? 1u : 0u,
        cache4_carrier.wrapper,
        cache4_carrier.payload,
        static_cast<unsigned>(cache4_carrier.wrapper_flags),
        cache4_carrier.valid ? 1u : 0u,
        cache0_carrier.valid && cache4_carrier.valid &&
                cache0_carrier.payload == cache4_carrier.payload
            ? 1u
            : 0u);
}

bool restore_preserved_stock_records(void* top) noexcept {
    if (!top || !g_preserved_stock_catalogue_ready) return false;
    std::array<RecordFields, kPhysicalTabCapacity> stock{};
    for (std::size_t physical = 0;
         physical < kStockLogicalTabCount;
         ++physical) {
        const LogicalTab& logical =
            g_preserved_stock_catalogue[physical];
        if (logical.category != kExpectedStockCategories[physical] ||
            logical.title[0] == L'\0') {
            return false;
        }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        stock[physical].title = active_logical_title(logical);
#else
        stock[physical].title = logical.title.data();
#endif
        stock[physical].category = logical.category;
        stock[physical].presentation = logical.presentation;
    }
    const LogicalTab& graphics =
        g_preserved_stock_catalogue[kStockLogicalTabCount - 1U];
    for (std::size_t physical = kStockLogicalTabCount;
         physical < active_physical_tab_count();
         ++physical) {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        stock[physical].title = active_logical_title(graphics);
#else
        stock[physical].title = graphics.title.data();
#endif
        stock[physical].category = graphics.category;
        stock[physical].presentation = graphics.presentation;
    }
    return write_record_fields(top, stock);
}

bool freeze_active_two_phase_rgba8_transaction(
    const char* reason) noexcept {
    InPlaceRgba8RebindTransaction& transaction =
        g_in_place_rgba8_rebind_transaction;
    if (!transaction.active) return false;

    const InPlaceRgba8RebindStage stage = transaction.stage;
    const TabHostMask completed_clear =
        transaction.completed_clear_mask;
    transaction.failed = true;
    transaction.active = false;
    g_rgba8_publication_disabled.store(true, std::memory_order_release);
    g_runtime.destructive_transition_failed = true;
    log_failure(
        "two-phase RGBA8 transaction frozen after exact state was lost (stage=%u cleared=0x%04X reason=%s); ownership ledgers are retained and no rollback or manual release is attempted",
        static_cast<unsigned>(stage),
        static_cast<unsigned>(completed_clear),
        reason ? reason : "unspecified");
    return true;
}

void reset_runtime() noexcept {
    (void)freeze_active_two_phase_rgba8_transaction(
        "runtime reset while transaction was active");
    reset_pointer_dwell(nullptr);
    // A scheduled probe owns no AS value, but its menu coordinates are valid
    // only for the current Configuration epoch.
    g_deferred_bitmapdata_probe = {};
    // The BitmapData property itself is movie-owned and is intentionally left
    // alone. Only discard native verification coordinates when that movie
    // epoch ends; never dereference or clear the remembered payload here.
    g_bitmapdata_property_verification = {};
    g_in_place_rgba8_rebind_transaction = {};
    g_runtime = {};
    g_mutating_viewport = false;
    g_tab_selection_source = TabSelectionSource::unknown;
    g_pointer_cell_sample_trace = {};
    g_blocking_composite_input = false;
    g_state_isolation_expectation = {};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (g_native_tab_runtime &&
        g_injected_icon_hosts.load(std::memory_order_acquire) == 0U &&
        g_staged_injected_icon_hosts.load(std::memory_order_acquire) == 0U &&
        all_rgba8_retirement_debts_zero()) {
        g_native_tab_runtime->reset_epoch_tracking();
    }
#endif
}

bool acknowledge_exact_carrier_teardown() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const std::uint64_t carrier_teardown_generation =
        erui::native::native_tab_graphics_carrier_teardown_generation();
    if (carrier_teardown_generation ==
            g_research_carrier_teardown_generation) {
        return false;
    }
    const bool recovered = g_research_failed;
    const bool destructive_projection_boundary = recovered &&
        g_runtime.active && g_runtime.destructive_transition_failed &&
        g_runtime.frozen_projection_retained_target;
    g_research_carrier_teardown_generation = carrier_teardown_generation;
    g_research_failed = false;
    if (destructive_projection_boundary) {
        // The carrier counter proves that the failed category-8 page is gone,
        // but it does not prove that the surrounding tab-strip movie or its
        // borrowed record pointers were rebuilt. Keep the retained target and
        // its process-owned title/icon storage pinned. A later ensure_runtime
        // call must still validate a new movie/stock epoch, reconcile both
        // overlay banks, and restore pristine stock records before reset.
        log_line(
            "Frozen tab projection observed exact Graphics carrier teardown generation=%llu; retained target ownership remains pinned pending validated tab-strip reconciliation",
            static_cast<unsigned long long>(
                carrier_teardown_generation));
    }
    if (recovered) {
        log_line(
            "Graphics carrier fault cleared after exact native teardown generation=%llu; native tab recovery may resume",
            static_cast<unsigned long long>(
                carrier_teardown_generation));
    }
    return recovered;
#else
    return false;
#endif
}

bool initialize_runtime(
    void* top,
    bool use_preserved_stock = false,
    std::size_t requested_initial_logical = invalid_tab_index) noexcept {
    if (!validate_top(top)) return false;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!normalize_selector_active_count_for_projection(top)) return false;
#endif
    LiveTabGeometry live_geometry{};
    if (!read_live_tab_geometry(top, live_geometry)) {
        return false;
    }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_native_tab_runtime) return false;
    bool owns_provisional_physical_epoch{};
    const std::size_t epoch_count =
        g_native_tab_runtime->active_physical_slot_count();
    if (epoch_count == 0U) {
        if (!g_native_tab_runtime->begin_physical_epoch(
                live_geometry.active)) {
            return false;
        }
        owns_provisional_physical_epoch = true;
    } else if (epoch_count != live_geometry.active) {
        return false;
    }

    // Binding D0 to the consolidated runtime is provisional until RuntimeState
    // is published below. All returns before that boundary are observational or
    // operate only on process-owned catalogue resources, so this guard may end
    // the span exactly. Once published, reset_runtime() remains the sole owner
    // of epoch teardown because records and display objects may borrow it.
    struct ProvisionalPhysicalEpoch final {
        erui::detail::NativeTabRuntimeState* runtime{};
        bool owned{};
        bool published{};

        ~ProvisionalPhysicalEpoch() noexcept {
            if (runtime && owned && !published) {
                runtime->reset_epoch_tracking();
            }
        }
    } provisional_epoch{
        g_native_tab_runtime.get(),
        owns_provisional_physical_epoch,
        false,
    };
#endif
    if (live_geometry.active != active_physical_tab_count()) return false;
    log_line(
        "Configuration tab geometry validated top=%p active=%zu authored=%zu selected=%zu stockLogical=%zu nativeRecordCapacity=%zu",
        top,
        live_geometry.active,
        live_geometry.authored,
        live_geometry.selected,
        kStockLogicalTabCount,
        kPhysicalTabCapacity);
    (void)acknowledge_exact_carrier_teardown();
    g_pointer_dwell.policy = TabPointerDwell(g_pointer_dwell_timing);
    const std::uint64_t initialization_started_qpc =
        query_performance_ticks();
    if ((g_injected_icon_hosts.load(std::memory_order_acquire) |
            g_staged_injected_icon_hosts.load(
                std::memory_order_acquire)) != 0) {
        log_line(
            "Viewport initialization found a surviving injected-icon ledger; reconciling it before publishing a new catalogue");
        const InjectedIconsClearResult clear_result =
            clear_injected_icons(top, true);
        if (!accept_vector_only_epoch_clear_result(
                top, clear_result, "runtime-initialization")) {
            if (clear_result == InjectedIconsClearResult::failed) {
                log_failure(
                    "Viewport initialization refused to overwrite a surviving injected-icon ledger");
            }
            return false;
        }
    } else {
        g_injected_movie_fingerprint = {};
    }

    if (use_preserved_stock && !g_preserved_stock_catalogue_ready) {
        log_failure(
            "Viewport initialization requested a preserved stock catalogue before one was captured");
        return false;
    }

    std::array<RecordFields, kPhysicalTabCapacity> original{};
    if (!use_preserved_stock && !snapshot_record_fields(top, original)) {
        return false;
    }

    RuntimeState next{};
    next.top = top;
    next.presentation_epoch =
        g_presentation_epoch_counter.fetch_add(
            1, std::memory_order_relaxed) + 1;
    const auto initialization_tab = [&next](
        std::size_t logical) noexcept -> LogicalTab* {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        (void)next;
        return runtime_logical_tab(logical);
#else
        return logical < next.catalogue.size()
            ? &next.catalogue[logical]
            : nullptr;
#endif
    };
    if (active_logical_tab_count() < kStockLogicalTabCount ||
        active_logical_tab_count() < active_physical_tab_count()) {
        return false;
    }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (!g_logical_tab_catalogue ||
        g_logical_tab_catalogue->tabs.size() !=
            resource_logical_tab_count()) {
        log_failure("pinned logical tab catalogue is unavailable");
        return false;
    }
#endif
    for (std::size_t logical = 0;
         logical < kStockLogicalTabCount;
         ++logical) {
        LogicalTab* const destination = initialization_tab(logical);
        if (!destination) return false;
        if (use_preserved_stock) {
            const LogicalTab& stock = g_preserved_stock_catalogue[logical];
            if (stock.category != kExpectedStockCategories[logical] ||
                !copy_title(
                    stock.title.data(), destination->title)) {
                return false;
            }
            destination->category = stock.category;
            destination->presentation = stock.presentation;
        } else {
            if (original[logical].category !=
                    kExpectedStockCategories[logical] ||
                !copy_title(
                    original[logical].title,
                    destination->title)) {
                return false;
            }
            destination->category = original[logical].category;
            destination->presentation = original[logical].presentation;
        }
    }
    if (!use_preserved_stock) {
        for (std::size_t logical = 0;
             logical < kStockLogicalTabCount;
             ++logical) {
            const LogicalTab* const source = initialization_tab(logical);
            if (!source) return false;
            g_preserved_stock_catalogue[logical] = *source;
        }
        g_preserved_stock_catalogue_ready = true;
    }

    for (std::size_t icon = 0;
         icon < active_custom_tab_count();
         ++icon) {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        LogicalTab* const tab = runtime_stable_custom_logical_tab(icon);
#else
        LogicalTab* const tab = initialization_tab(
            kStockLogicalTabCount + icon);
#endif
        if (!tab) return false;
        if (!make_synthetic_icon_descriptor(
                tab->presentation, kRgba8IconIds[icon])) {
            log_failure(
                "could not construct RGBA8 icon descriptor %zu (id=%d)",
                icon,
                kRgba8IconIds[icon]);
            return false;
        }
    }
    if (!make_synthetic_icon_descriptor(
            next.overflow_presentation, kOverflowIconId)) {
        log_failure(
            "could not construct the overflow ellipsis icon descriptor");
        return false;
    }
    for (std::size_t logical = kStockLogicalTabCount;
         logical < active_logical_tab_count();
         ++logical) {
        LogicalTab* const destination = initialization_tab(logical);
        if (!destination) return false;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        const std::size_t stable =
            stable_custom_index_for_logical(logical);
        const wchar_t* const title = g_tab_projection_generation &&
                stable < g_tab_projection_generation->entries.size()
            ? g_tab_projection_generation->entries[stable].title.c_str()
            : nullptr;
        if (!copy_title(
                title, active_logical_title_storage(*destination))) {
            log_failure(
                "custom-tab title %zu is unavailable or exceeds %zu UTF-16 code units",
                stable,
                kTitleCapacity - 1U);
            return false;
        }
#else
        const int title_length = swprintf_s(
            destination->title.data(),
            destination->title.size(),
            L"[Probe] Client Page %02zu",
            logical - kStockLogicalTabCount + 1);
        if (title_length <= 0 ||
            static_cast<std::size_t>(title_length) >=
                destination->title.size()) {
            log_failure("could not generate a stable custom-tab title");
            return false;
        }
#endif
        // Every custom tab resolves through the category-8 Graphics carrier.
        destination->category = 8;
    }

    std::uint32_t selected{};
    if (!read_selected_physical(top, selected)) return false;
    const bool explicit_initial =
        requested_initial_logical != invalid_tab_index;
    const std::size_t initial_logical = explicit_initial
        ? requested_initial_logical
        : 0U;
    if (!next.viewport.initialize(
            active_logical_tab_count(),
            active_physical_tab_count(),
            g_navigation_margin,
            initial_logical)) {
        return false;
    }
    if (!explicit_initial) {
        const TabViewportChange initial_selection =
            next.viewport.select_physical(selected);
        if (!initial_selection.valid) return false;
    }

    // Publish the catalogue before build_mapping_fields() reads it. No hook
    // can interleave here because all menu callbacks execute on this UI thread.
    g_runtime = next;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    provisional_epoch.published = true;
#endif
    ResolvedTabLayers layers{};
    if (!resolve_tab_layers(top, layers)) {
        reset_runtime();
        return false;
    }
    if (!apply_mapping(top, g_runtime.viewport, layers, true)) {
        if (g_runtime.destructive_transition_failed) {
            // A partial initial write followed by an unprovable rollback can
            // leave native records borrowing this catalogue. Mark the epoch
            // active solely as a lifetime guard: ensure_runtime() will detect
            // the non-exact mapping, suspend mutation, and keep these strings
            // alive for as long as the Configuration instance can use them.
            g_runtime.active = true;
            g_runtime.pending = {};
            reset_pointer_dwell(
                "initial tab publication failed destructively");
            log_failure(
                "initial tab publication could not be rolled back exactly; catalogue storage is pinned and viewport mutation is suspended");
            return false;
        }
        reset_runtime();
        return false;
    }
    if (explicit_initial) {
        const std::size_t desired_physical =
            g_runtime.viewport.selected_physical();
        if (desired_physical >= active_physical_tab_count() ||
            !invoke_selector_select(
                top, static_cast<std::uint32_t>(desired_physical))) {
            reset_runtime();
            return false;
        }
    }
    g_runtime.active = true;

    // Raster resources are process-owned, while every Configuration movie
    // has a fresh display tree. RuntimeState remains inactive until the
    // mapping is complete, so initial publication always uses the proven
    // vector-Shape ownership route. When an exact raster survives from an
    // earlier epoch that Shape is transparent; otherwise it remains the
    // visible failure-safe cross. Schedule one ownership-checked presentation
    // pass for every new movie epoch to perform the normal raster handoff.
    const bool presentation_refresh_requested =
        !g_rgba8_publication_disabled.load(std::memory_order_acquire) &&
        g_runtime.catalogue_presentation_refresh.request_if_needed(
            g_runtime.viewport,
            kStockLogicalTabCount,
            g_runtime.presentation_epoch,
            g_composite_frame_sequence);
    log_line(
        "Configuration epoch deferred initial presentation refresh=%u epoch=%llu window=%zu generation=%llu catalogueReady=%u",
        presentation_refresh_requested ? 1U : 0U,
        static_cast<unsigned long long>(
            g_runtime.presentation_epoch),
        g_runtime.viewport.window_start(),
        static_cast<unsigned long long>(
            g_runtime.viewport.mapping_generation()),
        g_rgba8_catalogue_ready.load(std::memory_order_acquire)
            ? 1U
            : 0U);
    invoke_refresh_title(top);

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // Keep one lightweight, release-log proof per Configuration epoch. The
    // per-record and transition traces below remain diagnostics-only, but this
    // line is enough to establish which live selector geometry was accepted
    // when investigating a player report.
    erui::detail::logf(
        erui::LogLevel::info,
        "NativeTabs Configuration geometry accepted: active=%zu authored=%zu logical=%zu selected=%zu",
        live_geometry.active,
        live_geometry.authored,
        active_logical_tab_count(),
        live_geometry.selected);
#endif

    const std::uint64_t initialization_microseconds =
        elapsed_qpc_microseconds(
            initialization_started_qpc, query_performance_ticks());
    log_line(
        "STRESS viewport initialized epoch=%llu top=%p logical=%zu physical=%zu margin=%zu selectedLogical=%zu selectedPhysical=%zu window=%zu timingUs=%llu",
        static_cast<unsigned long long>(g_runtime.presentation_epoch),
        top,
        g_runtime.viewport.logical_count(),
        g_runtime.viewport.physical_capacity(),
        g_runtime.viewport.navigation_margin(),
        g_runtime.viewport.selected_logical(),
        g_runtime.viewport.selected_physical(),
        g_runtime.viewport.window_start(),
        static_cast<unsigned long long>(initialization_microseconds));
    for (std::size_t logical = 0;
         logical < active_logical_tab_count();
         ++logical) {
        const LogicalTab* const tab = runtime_logical_tab(logical);
        if (!tab) return false;
        std::uint32_t token{};
        std::int32_t resource_id{-1};
        std::memcpy(
            &token,
            tab->presentation.data(),
            sizeof(token));
        std::memcpy(
            &resource_id,
            tab->presentation.data() + 0x3C,
            sizeof(resource_id));
        log_line(
            "catalogue[%zu] category=%d iconToken=%u resourceId=%d title=%ls",
            logical,
            tab->category,
            static_cast<unsigned>(token),
            resource_id,
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            active_logical_title(*tab));
#else
            tab->title.data());
#endif
    }
    log_rgba8_lifecycle_snapshot("epoch-initialized", top);
    return true;
}

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
enum class TabProjectionRefreshResult : std::uint8_t {
    none,
    retiring,
    deferred_epoch,
    committed,
    failed,
};

struct StableTabSelection final {
    bool custom{};
    std::size_t index{invalid_tab_index};
};

[[nodiscard]] StableTabSelection selected_stable_tab() noexcept {
    StableTabSelection result{};
    if (!g_runtime.active || !g_runtime.viewport.initialized()) return result;
    const std::size_t logical = g_runtime.viewport.selected_logical();
    if (logical < kStockLogicalTabCount) {
        result.index = logical;
        return result;
    }
    const LogicalTab* const tab = runtime_logical_tab(logical);
    if (!tab || !tab->custom_runtime) return result;
    result.custom = true;
    result.index = tab->custom_runtime->index();
    return result;
}

[[nodiscard]] bool generation_has_icon_change(
    const erui::detail::TabRuntimeGeneration& generation) noexcept {
    if (!g_tab_projection_generation ||
        generation.entries.size() !=
            g_tab_projection_generation->entries.size()) {
        return true;
    }
    for (std::size_t index = 0; index < generation.entries.size(); ++index) {
        if (generation.entries[index].icon_revision !=
            g_tab_projection_generation->entries[index].icon_revision) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool validate_projection_generation(
    const erui::detail::TabRuntimeGeneration& generation) noexcept {
    if (!g_logical_tab_catalogue ||
        generation.entries.size() != active_custom_tab_count() ||
        g_logical_tab_catalogue->tabs.size() !=
            resource_logical_tab_count()) {
        return false;
    }
    for (std::size_t index = 0; index < generation.entries.size(); ++index) {
        const erui::detail::TabRuntimeEntry& entry =
            generation.entries[index];
        if (entry.stable_index >= generation.entries.size() ||
            entry.stable_index != index || entry.title.empty() ||
            entry.title.size() >= kTitleCapacity) {
            return false;
        }
    }
    for (std::size_t ordinal = 0;
         ordinal < generation.visible_order.size();
         ++ordinal) {
        const std::size_t stable = generation.visible_order[ordinal];
        if (stable >= generation.entries.size()) {
            return false;
        }
        for (std::size_t prior = 0; prior < ordinal; ++prior) {
            if (generation.visible_order[prior] == stable) return false;
        }
    }
    return true;
}

// All allocation and sorting already happened in TabRuntimeGeneration.
// Publishing the process-pinned title storage is a bounded no-throw copy.
[[nodiscard]] bool apply_projection_generation(
    const std::shared_ptr<const erui::detail::TabRuntimeGeneration>&
        generation) noexcept {
    if (!generation || !validate_projection_generation(*generation)) {
        return false;
    }
    for (const erui::detail::TabRuntimeEntry& entry : generation->entries) {
        LogicalTab* const tab =
            runtime_stable_custom_logical_tab(entry.stable_index);
        if (!tab) return false;
    }
    for (const erui::detail::TabRuntimeEntry& entry : generation->entries) {
        LogicalTab* const tab =
            runtime_stable_custom_logical_tab(entry.stable_index);
        if (!copy_title(
                entry.title.c_str(), active_logical_title_storage(*tab))) {
            return false;
        }
    }
    g_tab_projection_generation = generation;
    return true;
}

// Prepare a projection's title pointers without touching the buffers borrowed
// by the visible source records.  No active flag is changed until the target
// Shape bank has attached and the old bank has retired exactly.
[[nodiscard]] bool stage_projection_titles(
    const erui::detail::TabRuntimeGeneration& generation) noexcept {
    if (!validate_projection_generation(generation)) return false;
    for (const erui::detail::TabRuntimeEntry& entry : generation.entries) {
        if (!runtime_stable_custom_logical_tab(entry.stable_index)) {
            return false;
        }
    }
    for (const erui::detail::TabRuntimeEntry& entry : generation.entries) {
        LogicalTab* const tab =
            runtime_stable_custom_logical_tab(entry.stable_index);
        if (!copy_title(
                entry.title.c_str(), inactive_logical_title(*tab))) {
            return false;
        }
    }
    return true;
}

void commit_staged_projection_titles() noexcept {
    for (std::size_t stable = 0;
         stable < active_custom_tab_count();
         ++stable) {
        if (LogicalTab* const tab =
                runtime_stable_custom_logical_tab(stable)) {
            tab->alternate_title_active = !tab->alternate_title_active;
        }
    }
}

struct TabGenerationPublication final {
    bool accepted{};
    bool latest{};
};

[[nodiscard]] TabGenerationPublication publish_tab_generation(
    const std::shared_ptr<const erui::detail::TabRuntimeGeneration>&
        generation) noexcept {
    TabGenerationPublication result{};
    if (!generation) return result;
    result.latest =
        erui::detail::publish_applied_tab_generation(generation);
    const auto applied = erui::detail::acquire_applied_tab_generation();
    result.accepted = applied && applied.get() == generation.get() &&
        applied->revision == generation->revision;
    return result;
}

[[nodiscard]] bool stage_live_rgba8_generation(
    void* top,
    const std::shared_ptr<const erui::detail::TabRuntimeGeneration>&
        generation) noexcept {
    if (!generation || !top || !g_runtime.active ||
        g_runtime.top != top || !validate_top(top) ||
        !validate_projection_generation(*generation)) {
        return false;
    }
    if (g_rgba8_staged_bank_ready) {
        return g_rgba8_staged_tab_generation.get() == generation.get();
    }
    if (g_rgba8_staging_failed_epoch != 0U &&
        g_rgba8_staging_failed_epoch == g_runtime.presentation_epoch) {
        return false;
    }
    if (!all_rgba8_retirement_debts_zero()) return false;

    const std::size_t count = active_custom_tab_count();
    if (!g_tab_projection_generation ||
        generation->entries.size() != count ||
        g_rgba8_staged_resources_scratch.size() != count ||
        g_rgba8_staged_changed_scratch.size() != count) {
        return false;
    }
    std::fill(
        g_rgba8_staged_resources_scratch.begin(),
        g_rgba8_staged_resources_scratch.end(),
        RetainedRgba8Resource{});
    std::fill(
        g_rgba8_staged_changed_scratch.begin(),
        g_rgba8_staged_changed_scratch.end(),
        std::uint8_t{0});

    g_rgba8_staging_generation = generation.get();
    bool exact{true};
    for (std::size_t index = 0; index < count; ++index) {
        const bool changed = generation->entries[index].icon_revision !=
            g_tab_projection_generation->entries[index].icon_revision;
        g_rgba8_staged_changed_scratch[index] = changed ? 1U : 0U;
        if (!changed) {
            g_rgba8_staged_resources_scratch[index] =
                current_retained_rgba8(index);
            continue;
        }
        run_raw_image_lifecycle_probe_one(
            top, index, g_rgba8_staged_resources_scratch[index]);
        const RetainedRgba8Resource& staged =
            g_rgba8_staged_resources_scratch[index];
        exact = exact && staged.ready && staged.image && staged.resource &&
            staged.checksum == rgba8_probe_icon_checksum(index);
    }
    g_rgba8_staging_generation = nullptr;

    for (std::size_t index = 0; index < count && exact; ++index) {
        const RetainedRgba8Resource& entry =
            g_rgba8_staged_resources_scratch[index];
        exact = entry.ready && entry.image && entry.resource;
        for (std::size_t prior = 0; prior < index && exact; ++prior) {
            const RetainedRgba8Resource& other =
                g_rgba8_staged_resources_scratch[prior];
            exact = entry.image != other.image &&
                entry.resource != other.resource;
        }
    }
    if (!exact) {
        // Native image roots are intentionally process-retained. Do not
        // overwrite the uncertain scratch and allocate another complete bank
        // on every composite frame. A genuinely fresh movie epoch may retry
        // once with its own validated bridge.
        g_rgba8_staging_failed_epoch = g_runtime.presentation_epoch;
        log_failure(
            "live tab icon resource generation revision=%llu failed its immutable identity gate; source bank remains authoritative and this movie epoch will not retry",
            static_cast<unsigned long long>(generation->revision));
        return false;
    }
    g_rgba8_staged_tab_generation = generation;
    g_rgba8_staged_bank_ready = true;
    erui::detail::logf(
        erui::LogLevel::info,
        "NativeTabs live icon resource bank staged: revision=%llu stableCustom=%zu",
        static_cast<unsigned long long>(generation->revision),
        count);
    return true;
}

[[nodiscard]] bool staged_rgba8_bank_publishable() noexcept {
    if (!g_rgba8_staged_bank_ready ||
        !g_rgba8_staged_tab_generation ||
        !g_native_tab_runtime ||
        g_rgba8_staged_tab_generation->entries.size() !=
            active_custom_tab_count() ||
        g_rgba8_staged_resources_scratch.size() !=
            active_custom_tab_count() ||
        g_rgba8_staged_changed_scratch.size() !=
            active_custom_tab_count() ||
        g_rgba8_retired_resources_scratch.size() !=
            active_custom_tab_count() ||
        g_rgba8_retired_debts_scratch.size() !=
            active_custom_tab_count() ||
        g_rgba8_retired_bank_active) {
        return false;
    }

    for (std::size_t index = 0;
         index < active_custom_tab_count();
         ++index) {
        if (!runtime_custom_tab(index)) return false;
        const RetainedRgba8Resource current = current_retained_rgba8(index);
        const RetainedRgba8Resource& staged =
            g_rgba8_staged_resources_scratch[index];
        if (!current.ready || !current.resource || !current.image ||
            !staged.ready || !staged.resource || !staged.image) {
            return false;
        }
        const bool changed = g_rgba8_staged_changed_scratch[index] != 0U;
        if (changed == (current.resource == staged.resource) ||
            changed == (current.image == staged.image)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool publish_staged_rgba8_bank_after_handoff() noexcept {
    if (!staged_rgba8_bank_publishable()) return false;

    bool retired_pending{};
    for (std::size_t index = 0;
         index < active_custom_tab_count();
         ++index) {
        if (g_rgba8_staged_changed_scratch[index] == 0U) continue;
        g_rgba8_retired_resources_scratch[index] =
            current_retained_rgba8(index);
        g_rgba8_retired_debts_scratch[index] =
            rgba8_retirement_debt(index);
        retired_pending = retired_pending ||
            g_rgba8_retired_debts_scratch[index] != 0U;
    }
    // Make the old immutable roots discoverable before moving their debt out
    // of the current runtime entries.  The publishability preflight proves all
    // tab entries and sparse-worklist capacity before the irreversible source
    // Shape detach, so these UI-thread updates are allocation-free.
    g_rgba8_retired_bank_active = retired_pending;
    for (std::size_t index = 0;
         index < active_custom_tab_count();
         ++index) {
        if (g_rgba8_staged_changed_scratch[index] != 0U) {
            (void)set_rgba8_retirement_debt(index, 0U);
        }
    }

    // No callback is forwarded between publication and the host-ledger flip.
    // Close the ordinary lookup gate so an unrelated presenter cannot observe
    // a partially published table even if this code is later made reentrant.
    g_rgba8_catalogue_ready.store(false, std::memory_order_release);
    for (std::size_t index = 0;
         index < active_custom_tab_count();
         ++index) {
        if (g_rgba8_staged_changed_scratch[index] != 0U) {
            publish_retained_rgba8(
                index, g_rgba8_staged_resources_scratch[index]);
        }
        if (auto* const tab = runtime_custom_tab(index)) {
            tab->icon().raster_requested =
                erui::native::custom_tab_icon_requests_raster(
                    g_rgba8_staged_tab_generation->entries[index]
                        .icon.source);
        }
    }
    // Keep lookup closed until the caller flips every staged host ledger and
    // property-bank bit. finish_staged_rgba8_bank_publication() reopens it at
    // that exact commit boundary.
    return true;
}

void finish_staged_rgba8_bank_publication() noexcept {
    g_rgba8_staged_bank_ready = false;
    g_rgba8_staged_tab_generation.reset();
    std::fill(
        g_rgba8_staged_changed_scratch.begin(),
        g_rgba8_staged_changed_scratch.end(),
        std::uint8_t{0});
    g_rgba8_catalogue_ready.store(true, std::memory_order_release);
}

// A RawImage/ImageResource is immutable after native publication. An icon edit
// therefore builds a complete replacement bank, proves the old Shapes have
// detached, and publishes the resource/property ledgers at one commit boundary.
// The same transaction runs in an active movie or after fresh initialization.
// Old roots deliberately keep their process-lifetime reference and are never
// mutated or released.
[[nodiscard]] bool adopt_pending_tab_generation_for_fresh_epoch(
    void* top) noexcept {
    (void)top;
    if (!erui::detail::tab_updates_pending()) return true;
    const auto desired = erui::detail::acquire_desired_tab_generation();
    if (!desired || !validate_projection_generation(*desired)) return false;

    const bool icon_change = generation_has_icon_change(*desired);
    if (!icon_change) {
        const auto previous_generation = g_tab_projection_generation;
        if (!apply_projection_generation(desired)) return false;
        const TabGenerationPublication publication =
            publish_tab_generation(desired);
        if (!publication.accepted) {
            (void)apply_projection_generation(previous_generation);
            return false;
        }
        if (!publication.latest) {
            log_line(
                "fresh Configuration epoch adopted tab revision=%llu while a newer desired generation remained queued",
                static_cast<unsigned long long>(desired->revision));
        }
        return true;
    }

    // A fast reopen can arrive while the previous movie's renderer debt is
    // still retiring. Initialize this movie from the last applied generation
    // and leave the desired revision queued. The unified live transaction
    // retries automatically in this same movie once ownership is exact.
    erui::detail::logf(
        erui::LogLevel::info,
        "NativeTabs icon revision=%llu queued for same-movie live adoption after fresh initialization (active=0x%04X staged=0x%04X retired=%u runtimeRetiring=%u)",
        static_cast<unsigned long long>(desired->revision),
        static_cast<unsigned>(g_injected_icon_hosts.load(
            std::memory_order_acquire)),
        static_cast<unsigned>(g_staged_injected_icon_hosts.load(
            std::memory_order_acquire)),
        g_rgba8_retired_bank_active ? 1U : 0U,
        g_native_tab_runtime &&
                !g_native_tab_runtime->retiring_tab_indices().empty()
            ? 1U
            : 0U);
    return true;
}

[[nodiscard]] std::size_t projected_logical_for_selection(
    const StableTabSelection& selection,
    const erui::detail::TabRuntimeGeneration& generation) noexcept {
    if (selection.index == invalid_tab_index) return 0U;
    if (!selection.custom) {
        return selection.index < kStockLogicalTabCount
            ? selection.index
            : 0U;
    }
    const auto found = std::find(
        generation.visible_order.begin(),
        generation.visible_order.end(),
        selection.index);
    return found == generation.visible_order.end()
        ? 0U
        : kStockLogicalTabCount + static_cast<std::size_t>(
              found - generation.visible_order.begin());
}

TabProjectionRefreshResult apply_pending_tab_projection(
    void* top) noexcept {
    if (!erui::detail::tab_updates_pending()) {
        return TabProjectionRefreshResult::none;
    }
    auto desired = g_rgba8_staged_bank_ready
        ? g_rgba8_staged_tab_generation
        : erui::detail::acquire_desired_tab_generation();
    if (!desired || desired.get() == g_tab_projection_generation.get()) {
        return TabProjectionRefreshResult::none;
    }
    if (!top || !g_runtime.active || g_runtime.top != top ||
        g_runtime.pending.active || g_mutating_viewport ||
        g_in_place_rgba8_rebind_transaction.active ||
        g_runtime.destructive_transition_failed || g_research_failed ||
        !validate_projection_generation(*desired) ||
        !records_match_mapping(top, g_runtime.viewport)) {
        return TabProjectionRefreshResult::none;
    }

    InjectedMovieFingerprint current_fingerprint{};
    const bool fingerprint_known =
        g_injected_movie_fingerprint.object_interface &&
        g_injected_movie_fingerprint.owner_link &&
        g_injected_movie_fingerprint.movie_root;
    if (fingerprint_known &&
        (!sample_current_movie_fingerprint(top, current_fingerprint) ||
            !same_movie_fingerprint(
                current_fingerprint,
                g_injected_movie_fingerprint))) {
        // ensure_runtime() owns same-address movie reconciliation. Never
        // construct a target resource bank against a stale movie identity.
        return TabProjectionRefreshResult::none;
    }

    const bool icon_change = generation_has_icon_change(*desired);
    if (icon_change && !g_rgba8_staged_bank_ready) {
        if (!all_rgba8_retirement_debts_zero()) {
            if (g_rgba8_retirement_wait_logged_epoch !=
                    g_runtime.presentation_epoch ||
                g_rgba8_retirement_wait_logged_revision !=
                    desired->revision) {
                erui::detail::logf(
                    erui::LogLevel::info,
                    "NativeTabs live icon revision=%llu waiting in this movie for prior resource retirement (active=0x%04X staged=0x%04X retired=%u)",
                    static_cast<unsigned long long>(desired->revision),
                    static_cast<unsigned>(g_injected_icon_hosts.load(
                        std::memory_order_acquire)),
                    static_cast<unsigned>(g_staged_injected_icon_hosts.load(
                        std::memory_order_acquire)),
                    g_rgba8_retired_bank_active ? 1U : 0U);
                g_rgba8_retirement_wait_logged_epoch =
                    g_runtime.presentation_epoch;
                g_rgba8_retirement_wait_logged_revision = desired->revision;
            }
            return TabProjectionRefreshResult::retiring;
        }
        g_rgba8_retirement_wait_logged_epoch = 0U;
        g_rgba8_retirement_wait_logged_revision = 0U;
        if (!stage_live_rgba8_generation(top, desired)) {
            return TabProjectionRefreshResult::none;
        }
        desired = g_rgba8_staged_tab_generation;
    }

    if (g_tab_projection_generation &&
        desired->visible_order ==
            g_tab_projection_generation->visible_order &&
        !icon_change) {
        // Title edits, idempotent visibility edits, and preferred-order edits
        // whose effective ordering is unchanged need no selector or carrier
        // reconstruction.  Keeping the active page intact also preserves its
        // native row focus and continuation stack.
        const auto previous_generation = g_tab_projection_generation;
        if (!apply_projection_generation(desired)) {
            (void)apply_projection_generation(previous_generation);
            return TabProjectionRefreshResult::failed;
        }
        const TabGenerationPublication publication =
            publish_tab_generation(desired);
        if (!publication.accepted) {
            (void)apply_projection_generation(previous_generation);
            g_research_failed = true;
            log_failure(
                "NativeTabs refused a non-destructive generation whose applied-state acknowledgement was lost; tab mutation is frozen until native teardown");
            return TabProjectionRefreshResult::failed;
        }
        invoke_refresh_title(top);
        erui::detail::logf(
            erui::LogLevel::info,
            "NativeTabs non-destructive generation committed: revision=%llu visibleCustom=%zu latest=%u",
            static_cast<unsigned long long>(desired->revision),
            desired->visible_order.size(),
            publication.latest ? 1U : 0U);
        return TabProjectionRefreshResult::committed;
    }

    const StableTabSelection prior_selection = selected_stable_tab();
    const std::size_t physical_count = active_physical_tab_count();
    LiveTabGeometry live_geometry{};
    const std::size_t target_logical_count =
        kStockLogicalTabCount + desired->visible_order.size();
    if (prior_selection.index == invalid_tab_index ||
        !read_live_tab_geometry(top, live_geometry) ||
        live_geometry.active != physical_count ||
        live_geometry.selected != g_runtime.viewport.selected_physical()) {
        return TabProjectionRefreshResult::none;
    }

    // The native selector's D0/record geometry is immutable for the validated
    // movie epoch.  Most installations expose eight physical cells, so hiding
    // or reordering custom tabs changes only their logical projection.  If a
    // 9/10-cell movie would require a different physical count, defer the
    // complete generation to the next exact movie boundary instead of briefly
    // publishing a mismatched selector layout.
    if (target_logical_count < physical_count ||
        std::min(live_geometry.records, target_logical_count) !=
            physical_count) {
        return TabProjectionRefreshResult::deferred_epoch;
    }

    const std::size_t target_logical =
        projected_logical_for_selection(prior_selection, *desired);
    const bool selected_was_hidden = prior_selection.custom &&
        target_logical == 0U;
    TabViewport candidate{};
    if (!candidate.initialize(
            target_logical_count,
            physical_count,
            g_navigation_margin,
            target_logical) ||
        candidate.visible_slot_count() != physical_count ||
        candidate.selected_physical() >= physical_count ||
        !stage_projection_titles(*desired)) {
        return TabProjectionRefreshResult::failed;
    }

    const std::size_t source_logical =
        g_runtime.viewport.selected_logical();
    const LogicalTab* const source_tab =
        runtime_logical_tab(source_logical);
    const LogicalTab* target_tab{};
    InjectedHostLayout target_layout{};
    {
        ScopedTargetTabProjectionGeneration target_generation{desired.get()};
        target_tab = runtime_logical_tab(target_logical);
        if (!target_tab ||
            !derive_injected_host_layout(candidate, target_layout)) {
            return TabProjectionRefreshResult::failed;
        }
    }
    if (!source_tab) return TabProjectionRefreshResult::failed;

    PendingTransition refresh{};
    refresh.active = true;
    refresh.top = top;
    refresh.candidate = candidate;
    refresh.native_physical =
        static_cast<std::uint32_t>(live_geometry.selected);
    refresh.desired_physical = static_cast<std::uint32_t>(
        candidate.selected_physical());
    refresh.provisional_logical = source_logical;
    refresh.desired_logical = target_logical;
    refresh.previous_logical = source_logical;
    refresh.provisional_category = source_tab->category;
    refresh.desired_category = target_tab->category;
    refresh.requested_qpc = query_performance_ticks();
    refresh.projection_refresh = true;
    refresh.icon_resource_refresh = icon_change;
    refresh.projection_selected_hidden = selected_was_hidden;
    refresh.projection_generation = desired;
    g_runtime.pending = std::move(refresh);
    erui::detail::logf(
        erui::LogLevel::info,
        "NativeTabs atomic projection armed: revision=%llu visibleCustom=%zu sourceLogical=%zu targetLogical=%zu hiddenFallback=%u sourcePhysical=%zu targetPhysical=%zu",
        static_cast<unsigned long long>(desired->revision),
        desired->visible_order.size(),
        source_logical,
        target_logical,
        selected_was_hidden ? 1U : 0U,
        live_geometry.selected,
        candidate.selected_physical());
    return TabProjectionRefreshResult::none;
}
#endif

#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
[[nodiscard]] bool adopt_pending_tab_generation_for_fresh_epoch(
    void*) noexcept {
    return true;
}
#endif

bool ensure_runtime(void* top) noexcept {
    if (!validate_top(top)) {
        if (freeze_active_two_phase_rgba8_transaction(
                "Configuration top validation failed")) {
            g_runtime.pending = {};
            reset_pointer_dwell(
                "Configuration top validation failed during transaction");
        }
        return false;
    }
    if (g_runtime.active) {
        if (g_runtime.top == top) {
            if (records_match_mapping(top, g_runtime.viewport)) {
                const bool fingerprint_known =
                    g_injected_movie_fingerprint.object_interface &&
                    g_injected_movie_fingerprint.owner_link &&
                    g_injected_movie_fingerprint.movie_root;
                if (!fingerprint_known) {
                    // Without an exact movie identity there is no teardown
                    // proof that can release a failed target mapping.
                    return !g_runtime.destructive_transition_failed;
                }

                InjectedMovieFingerprint current_fingerprint{};
                if (!sample_current_movie_fingerprint(
                        top, current_fingerprint)) {
                    if (freeze_active_two_phase_rgba8_transaction(
                            "same-top movie fingerprint could not be sampled")) {
                        g_runtime.pending = {};
                        reset_pointer_dwell(
                            "movie fingerprint sampling failed during transaction");
                    }
                    log_failure(
                        "same-address Configuration movie fingerprint could not be sampled; viewport fast return refused");
                    return false;
                }
                if (same_movie_fingerprint(
                        g_injected_movie_fingerprint,
                        current_fingerprint)) {
                    return !g_runtime.destructive_transition_failed;
                }

                // Records can survive while Scaleform recreates its movie at
                // the same native Configuration address. Reconcile the old
                // ledger against the newly sampled parent before any lookup
                // or bind callback mutates that new presentation tree.
                log_line(
                    "same-address Configuration records retained across a movie change; reconciling old fingerprint={interface:%p owner:%p root:%p} current={interface:%p owner:%p root:%p}",
                    g_injected_movie_fingerprint.object_interface,
                    g_injected_movie_fingerprint.owner_link,
                    g_injected_movie_fingerprint.movie_root,
                    current_fingerprint.object_interface,
                    current_fingerprint.owner_link,
                    current_fingerprint.movie_root);
                (void)freeze_active_two_phase_rgba8_transaction(
                    "same-address movie fingerprint changed");
                g_runtime.pending = {};
                reset_pointer_dwell("same-address movie epoch changed");
                const InjectedIconsClearResult clear_result =
                    clear_injected_icons(top, true);
                if (!accept_vector_only_epoch_clear_result(
                        top,
                        clear_result,
                        "same-address-movie-change")) {
                    if (clear_result == InjectedIconsClearResult::failed) {
                        log_failure(
                            "same-address movie change could not reconcile the retained icon ledger; viewport remains suspended");
                    }
                    return false;
                }
                if (!restore_preserved_stock_records(top)) {
                    log_failure(
                        "same-address movie change could not restore the process-stable stock records before reinitialization");
                    return false;
                }
                if (g_runtime.frozen_projection_retained_target) {
                    log_line(
                        "Frozen tab projection recovered at a validated same-address movie boundary after overlay reconciliation and pristine stock restoration (top=%p)",
                        top);
                }
                log_line(
                    "same-address movie change restored all eight stock records from the preserved pristine catalogue before reinitialization");
                reset_runtime();
                if (!adopt_pending_tab_generation_for_fresh_epoch(top)) {
                    return false;
                }
                return initialize_runtime(top, true);
            }
            if (exact_stock_layout_republished(top)) {
                // Elden Ring can reuse the same Configuration allocation for
                // a later menu epoch. It first republishes all eight exact
                // stock records, including native-owned title pointers. Only
                // that complete fingerprint is accepted as a safe lifetime
                // boundary; arbitrary same-address mutations still fail
                // closed below.
                (void)freeze_active_two_phase_rgba8_transaction(
                    "stock layout was republished during the transaction");
                g_runtime.pending = {};
                reset_pointer_dwell("configuration epoch reset");
                const InjectedIconsClearResult clear_result =
                    clear_injected_icons(top, true);
                if (!accept_vector_only_epoch_clear_result(
                        top,
                        clear_result,
                        "same-address-stock-reset")) {
                    if (clear_result == InjectedIconsClearResult::failed) {
                        log_failure(
                            "same-address stock reset could not clear tracked custom icons; viewport remains suspended");
                    }
                    return false;
                }
                if (g_runtime.frozen_projection_retained_target) {
                    log_line(
                        "Frozen tab projection recovered after exact stock republication and overlay reconciliation (top=%p)",
                        top);
                }
                log_line(
                    "Exact stock tab layout republished at the same Configuration address; starting a new viewport epoch (top=%p)",
                    top);
                reset_runtime();
                if (!adopt_pending_tab_generation_for_fresh_epoch(top)) {
                    return false;
                }
                return initialize_runtime(top);
            }
            // Live records borrow title pointers from catalogue. Keep that
            // storage intact if another component rewrites this same menu;
            // zeroing it here would invalidate the strings still in use.
            (void)freeze_active_two_phase_rgba8_transaction(
                "same-top viewport records no longer match the source mapping");
            g_runtime.pending = {};
            reset_pointer_dwell("viewport mapping invalidated");
            log_failure(
                "same Configuration instance no longer matches the viewport mapping; mutation is suspended");
            return false;
        }
        log_line(
            "Configuration instance changed; rebuilding viewport state (oldTop=%p newTop=%p)",
            g_runtime.top,
            top);

        (void)freeze_active_two_phase_rgba8_transaction(
            "Configuration instance changed during the transaction");

        // A new Configuration wrapper does not by itself prove whether Elden
        // Ring retained or recreated the tab-strip display tree. Reacquire
        // every tracked parent through the current, already-validated top. If
        // our two reserved properties still contain the exact objects, remove
        // them. If both are exactly Undefined, the fresh movie already owns a
        // clean parent and this proven epoch boundary may discard the stale
        // bit. Every partial or unexpected state still fails closed.
        const InjectedIconsClearResult clear_result =
            clear_injected_icons(top, true);
        if (!accept_vector_only_epoch_clear_result(
                top, clear_result, "configuration-instance-change")) {
            if (clear_result == InjectedIconsClearResult::failed) {
                log_failure(
                    "Configuration instance changed but retained tab-strip icons could not be cleared through the new top; viewport remains suspended");
            }
            return false;
        }
        if (!restore_preserved_stock_records(top)) {
            log_failure(
                "Configuration instance changed but its records could not be restored from the process-stable stock catalogue");
            return false;
        }
        if (g_runtime.frozen_projection_retained_target) {
            log_line(
                "Frozen tab projection recovered at a validated Configuration-instance boundary after overlay reconciliation and pristine stock restoration (oldTop=%p newTop=%p)",
                g_runtime.top,
                top);
        }
        log_line(
            "Reconciled retained or already-absent tab-strip icons and restored pristine stock records through the new Configuration top before rebuilding viewport state");
        reset_runtime();
        if (!adopt_pending_tab_generation_for_fresh_epoch(top)) {
            return false;
        }
        return initialize_runtime(top, true);
    }
    if (!adopt_pending_tab_generation_for_fresh_epoch(top)) return false;
    return initialize_runtime(top);
}

const char* transition_kind(
    std::size_t old_physical,
    std::size_t new_physical) noexcept {
    const std::size_t count = active_physical_tab_count();
    if ((old_physical + 1) % count == new_physical) {
        return "next";
    }
    if ((old_physical + count - 1) % count ==
        new_physical) {
        return "previous";
    }
    return "direct";
}

const char* selection_source_name(TabSelectionSource source) noexcept {
    switch (source) {
    case TabSelectionSource::pointer:
        return "pointer";
    case TabSelectionSource::navigation_previous:
        return "navigation-previous";
    case TabSelectionSource::navigation_next:
        return "navigation-next";
    case TabSelectionSource::unknown:
        break;
    }
    return "unknown";
}

const char* pointer_dwell_phase_name(
    TabPointerDwellPhase phase) noexcept {
    switch (phase) {
    case TabPointerDwellPhase::idle:
        return "idle";
    case TabPointerDwellPhase::awaiting_commit:
        return "awaiting-commit";
    case TabPointerDwellPhase::cooldown:
        return "cooldown";
    case TabPointerDwellPhase::repeat_due:
        return "repeat-due";
    }
    return "unknown";
}

const char* pointer_dwell_edge_name(
    TabPointerDwellEdge edge) noexcept {
    switch (edge) {
    case TabPointerDwellEdge::previous:
        return "previous";
    case TabPointerDwellEdge::next:
        return "next";
    case TabPointerDwellEdge::none:
        break;
    }
    return "none";
}

void log_pointer_dwell(
    const char* event,
    const char* detail = "") noexcept {
    const std::uint32_t record =
        g_pointer_dwell_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (record > kMaximumPointerDwellLogs) return;
    log_line(
        "pointerDwell[%u] event=%s phase=%s edge=%s physical=%zu selectedLogical=%zu window=%zu delay=%llu deadline=%llu repeatCommits=%llu ticket=%llu %s",
        static_cast<unsigned>(record),
        event ? event : "unknown",
        pointer_dwell_phase_name(g_pointer_dwell.policy.phase()),
        pointer_dwell_edge_name(g_pointer_dwell.policy.edge()),
        g_pointer_dwell.policy.physical_slot(),
        g_runtime.viewport.initialized()
            ? g_runtime.viewport.selected_logical()
            : invalid_tab_index,
        g_runtime.viewport.initialized()
            ? g_runtime.viewport.window_start()
            : 0,
        static_cast<unsigned long long>(
            g_pointer_dwell.policy.current_delay_ms()),
        static_cast<unsigned long long>(
            g_pointer_dwell.policy.deadline_ms()),
        static_cast<unsigned long long>(
            g_pointer_dwell.policy.automatic_repeats_committed()),
        static_cast<unsigned long long>(
            g_pointer_dwell.transaction_ticket),
        detail ? detail : "");
}

void clear_pointer_dwell_intents() noexcept {
    g_pointer_dwell.natural_intent = {};
    g_pointer_dwell.timed_intent = {};
}

void reset_pointer_dwell(
    const char* reason = nullptr) noexcept {
    const bool was_active = g_pointer_dwell.policy.active() ||
        g_pointer_dwell.timed_intent.active;
    if (was_active && reason) log_pointer_dwell("cancel", reason);
    g_pointer_dwell.policy.reset();
    g_pointer_dwell.top = nullptr;
    g_pointer_dwell.direction = TabDirection::next;
    g_pointer_dwell.transaction_ticket = 0;
    clear_pointer_dwell_intents();
}

std::uint64_t next_pointer_dwell_ticket() noexcept {
    ++g_pointer_dwell.ticket_counter;
    if (g_pointer_dwell.ticket_counter == 0) {
        ++g_pointer_dwell.ticket_counter;
    }
    return g_pointer_dwell.ticket_counter;
}

TabPointerDwellEdge overflow_edge_for_physical(
    const TabViewport& viewport,
    std::size_t physical) noexcept {
    switch (tab_overflow_indicator(viewport, physical)) {
    case TabOverflowIndicator::previous:
        return TabPointerDwellEdge::previous;
    case TabOverflowIndicator::next:
        return TabPointerDwellEdge::next;
    case TabOverflowIndicator::none:
        break;
    }
    return TabPointerDwellEdge::none;
}

TabDirection direction_for_pointer_edge(
    TabPointerDwellEdge edge) noexcept {
    return edge == TabPointerDwellEdge::previous
        ? TabDirection::previous
        : TabDirection::next;
}

bool pointer_dwell_has_directional_target(
    const TabViewport& viewport,
    TabDirection direction,
    std::size_t physical,
    std::size_t* target_output = nullptr) noexcept {
    if (!viewport.initialized()) return false;
    std::size_t target{};
    if (!viewport.logical_for_physical(physical, target)) return false;
    const std::size_t selected = viewport.selected_logical();
    const bool valid = direction == TabDirection::next
        ? selected < viewport.logical_count() - 1 &&
            target == selected + 1
        : selected != 0 && target + 1 == selected;
    if (valid && target_output) *target_output = target;
    return valid;
}

bool pointer_dwell_adapter_valid() noexcept {
    return g_pointer_dwell.policy.active() &&
        g_pointer_dwell.top &&
        g_runtime.active &&
        g_runtime.top == g_pointer_dwell.top &&
        g_get_pointer_position &&
        g_grid_hit_test &&
        g_native_pointer_select;
}

void finish_pointer_dwell_transition(
    std::uint64_t ticket,
    const char* detail) noexcept {
    if (!ticket ||
        ticket != g_pointer_dwell.transaction_ticket ||
        !pointer_dwell_adapter_valid()) {
        if (g_pointer_dwell.policy.active()) {
            reset_pointer_dwell("transaction ticket mismatch");
        }
        return;
    }

    g_pointer_dwell.transaction_ticket = 0;
    const bool has_more = pointer_dwell_has_directional_target(
        g_runtime.viewport,
        g_pointer_dwell.direction,
        g_pointer_dwell.policy.physical_slot());
    g_pointer_dwell.policy.transition_committed(
        GetTickCount64(), has_more);
    if (g_pointer_dwell.policy.active()) {
        log_pointer_dwell("commit", detail);
    } else {
        log_pointer_dwell("endpoint", detail);
        g_pointer_dwell.top = nullptr;
        clear_pointer_dwell_intents();
    }
}

void fail_pointer_dwell_transition(
    std::uint64_t ticket,
    const char* reason) noexcept {
    if (ticket &&
        ticket == g_pointer_dwell.transaction_ticket) {
        g_pointer_dwell.policy.transition_failed();
        g_pointer_dwell.transaction_ticket = 0;
        log_pointer_dwell("failed", reason);
        g_pointer_dwell.top = nullptr;
        clear_pointer_dwell_intents();
    }
}

bool is_active_tab_selector(std::uintptr_t candidate) noexcept {
    if (!candidate || !g_runtime.active || !g_runtime.top) return false;
    return candidate == reinterpret_cast<std::uintptr_t>(
        static_cast<std::byte*>(g_runtime.top) + kSelectorOffset);
}

std::uint64_t elapsed_ms(
    std::uint64_t now,
    std::uint64_t then) noexcept {
    return now >= then ? now - then : 0;
}

float pointer_coordinate(std::uint32_t bits) noexcept {
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void log_pointer_poll_summary(
    const PointerCellSampleTrace& trace,
    std::uint64_t now,
    std::int32_t selected) noexcept {
    const std::uint32_t record =
        g_pointer_sample_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (record > kMaximumPointerSampleLogs) return;

    log_line(
        "pointerPoll[%u] event=summary hit=%d x=%.3f y=%.3f selected=%d frames=%u polls=%u natural=%u fallback=%u unavailable=%u moves=%u hitChanges=%u comparisons=%u coordMismatch=%u hitMismatch=%u faults=%u lastSeh=0x%08lX stationaryMs=%llu",
        static_cast<unsigned>(record),
        trace.last_hit,
        static_cast<double>(pointer_coordinate(trace.last_x_bits)),
        static_cast<double>(pointer_coordinate(trace.last_y_bits)),
        selected,
        static_cast<unsigned>(trace.dispatch_frames),
        static_cast<unsigned>(trace.successful_polls),
        static_cast<unsigned>(trace.natural_frames),
        static_cast<unsigned>(trace.fallback_frames),
        static_cast<unsigned>(trace.unavailable_frames),
        static_cast<unsigned>(trace.coordinate_changes),
        static_cast<unsigned>(trace.hit_changes),
        static_cast<unsigned>(trace.comparison_frames),
        static_cast<unsigned>(trace.coordinate_mismatches),
        static_cast<unsigned>(trace.hit_mismatches),
        static_cast<unsigned>(trace.poll_faults),
        trace.last_poll_seh,
        static_cast<unsigned long long>(
            trace.initialized
                ? elapsed_ms(now, trace.last_position_change_ms)
                : 0));
}

void reset_pointer_poll_interval(
    PointerCellSampleTrace& trace,
    std::uint64_t now) noexcept {
    trace.last_report_ms = now;
    trace.dispatch_frames = 0;
    trace.successful_polls = 0;
    trace.natural_frames = 0;
    trace.fallback_frames = 0;
    trace.unavailable_frames = 0;
    trace.coordinate_changes = 0;
    trace.hit_changes = 0;
    trace.comparison_frames = 0;
    trace.coordinate_mismatches = 0;
    trace.hit_mismatches = 0;
    trace.poll_faults = 0;
    trace.last_poll_seh = 0;
}

bool poll_native_pointer(
    void* selector,
    const std::uint8_t* input_gate,
    NativePointerSample& sample) noexcept {
    sample = {};
    if (!selector || !input_gate || !g_get_pointer_position ||
        !g_grid_hit_test) {
        return false;
    }

    std::uint8_t input_enabled{};
    if (!guarded_read(input_gate, input_enabled) || input_enabled == 0) {
        return false;
    }

    PointerPosition position{};
    PointerPosition returned_position{};
    bool completed{};
#if defined(_MSC_VER)
    __try {
#endif
        PointerPosition* const returned =
            g_get_pointer_position(input_gate, &position);
        if (returned == &position) {
            returned_position = position;
            sample.hit = g_grid_hit_test(selector, &position);
            completed = true;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        sample.seh_code = static_cast<unsigned long>(GetExceptionCode());
    }
#endif

    if (!completed) return false;
    std::memcpy(&sample.x_bits, &returned_position.x, sizeof(sample.x_bits));
    std::memcpy(&sample.y_bits, &returned_position.y, sizeof(sample.y_bits));
    return true;
}

void observe_pointer_cell_sample(
    safetyhook::Context& context) noexcept {
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) return;
    if (!is_active_tab_selector(context.rsi)) return;

    std::uint32_t x_bits{};
    std::uint32_t y_bits{};
    if (!guarded_read(
            reinterpret_cast<const void*>(context.rsp + 0x20),
            x_bits) ||
        !guarded_read(
            reinterpret_cast<const void*>(context.rsp + 0x24),
            y_bits)) {
        ++g_pointer_cell_sample_trace.poll_faults;
        return;
    }

    PointerCellSampleTrace& trace = g_pointer_cell_sample_trace;
    trace.seen_this_dispatch = true;
    trace.natural_hit = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(context.rax));
    trace.natural_x_bits = x_bits;
    trace.natural_y_bits = y_bits;

    const std::int32_t natural_hit = trace.natural_hit;
    const std::uint64_t now = GetTickCount64();
    if (g_pointer_dwell.policy.active()) {
        if (!pointer_dwell_adapter_valid() ||
            !pointer_dwell_has_directional_target(
                g_runtime.viewport,
                g_pointer_dwell.direction,
                g_pointer_dwell.policy.physical_slot())) {
            reset_pointer_dwell("edge no longer has a directional target");
            return;
        }

        const auto update = g_pointer_dwell.policy.observe_natural(
            natural_hit, x_bits, y_bits, now);
        if (update.cancelled) {
            g_pointer_dwell.top = nullptr;
            g_pointer_dwell.transaction_ticket = 0;
            clear_pointer_dwell_intents();
            log_pointer_dwell("leave", "natural hit left the armed edge");
            // Continue through the idle classification below. If the pointer
            // moved directly onto the opposite ellipsis, this same native
            // hover remains immediate and starts a fresh dwell transaction.
        } else {
            if (update.deadline_restarted) {
                log_pointer_dwell("motion", "natural sample restarted delay");
            }
            if (update.suppress_natural_selection) {
                // At the pointer-cell seam, EAX == -1 takes the native
                // no-hit branch and
                // returns false from pointer processing. That both prevents a
                // repaint-triggered selection and leaves L1/R1 free to run
                // later in this same input frame.
                context.rax =
                    (context.rax & ~std::uintptr_t{0xFFFFFFFFu}) |
                    std::uintptr_t{0xFFFFFFFFu};
            }
            return;
        }
    }

    g_pointer_dwell.natural_intent = {};
    if (g_mutating_viewport || g_runtime.pending.active ||
        !g_get_pointer_position || !g_grid_hit_test ||
        !g_native_pointer_select || natural_hit < 0) {
        if (g_runtime.pending.active && natural_hit >= 0) {
            context.rax =
                (context.rax & ~std::uintptr_t{0xFFFFFFFFu}) |
                std::uintptr_t{0xFFFFFFFFu};
        }
        return;
    }

    const std::size_t physical = static_cast<std::size_t>(natural_hit);
    const TabPointerDwellEdge edge = overflow_edge_for_physical(
        g_runtime.viewport, physical);
    if (edge == TabPointerDwellEdge::none) return;
    g_pointer_dwell.natural_intent = {
        true,
        g_runtime.top,
        edge,
        static_cast<std::uint32_t>(physical),
        x_bits,
        y_bits,
    };
}

void observe_native_pointer_poll(
    safetyhook::Context& context) noexcept {
    PointerCellSampleTrace& trace = g_pointer_cell_sample_trace;
    ++trace.dispatch_frames;
    if (trace.seen_this_dispatch) ++trace.natural_frames;

    const std::uint64_t now = GetTickCount64();

    NativePointerSample sample{};
    const bool runtime_safe = !g_mutating_viewport &&
        !g_runtime.pending.active;
    const bool available = runtime_safe && poll_native_pointer(
        reinterpret_cast<void*>(context.rdi),
        reinterpret_cast<const std::uint8_t*>(context.rsi),
        sample);
    if (!available) {
        ++trace.unavailable_frames;
        if (sample.seh_code != 0) {
            ++trace.poll_faults;
            trace.last_poll_seh = sample.seh_code;
        }
    } else {
        ++trace.successful_polls;
        if (trace.seen_this_dispatch) {
            ++trace.comparison_frames;
            if (trace.natural_x_bits != sample.x_bits ||
                trace.natural_y_bits != sample.y_bits) {
                ++trace.coordinate_mismatches;
            }
            if (trace.natural_hit != sample.hit) {
                ++trace.hit_mismatches;
            }
        } else {
            ++trace.fallback_frames;
        }

        if (!trace.initialized) {
            trace.initialized = true;
            trace.last_hit = sample.hit;
            trace.last_x_bits = sample.x_bits;
            trace.last_y_bits = sample.y_bits;
            trace.last_position_change_ms = now;
            trace.last_report_ms = now;
        } else {
            const bool coordinates_changed =
                trace.last_x_bits != sample.x_bits ||
                trace.last_y_bits != sample.y_bits;
            if (coordinates_changed) {
                ++trace.coordinate_changes;
                trace.last_position_change_ms = now;
            }
            if (trace.last_hit != sample.hit) ++trace.hit_changes;
            trace.last_hit = sample.hit;
            trace.last_x_bits = sample.x_bits;
            trace.last_y_bits = sample.y_bits;
        }
    }

    if (g_pointer_dwell.policy.active()) {
        if (!pointer_dwell_adapter_valid()) {
            reset_pointer_dwell("runtime identity changed");
        } else if (available) {
            if (!pointer_dwell_has_directional_target(
                    g_runtime.viewport,
                    g_pointer_dwell.direction,
                    g_pointer_dwell.policy.physical_slot())) {
                reset_pointer_dwell(
                    "edge no longer has a directional target");
            } else {
                const TabPointerDwellPhase previous_phase =
                    g_pointer_dwell.policy.phase();
                const auto update = g_pointer_dwell.policy.observe_poll(
                    sample.hit,
                    sample.x_bits,
                    sample.y_bits,
                    now);
                if (update.cancelled) {
                    g_pointer_dwell.top = nullptr;
                    g_pointer_dwell.transaction_ticket = 0;
                    clear_pointer_dwell_intents();
                    log_pointer_dwell(
                        "leave", "native poll left the armed edge");
                } else {
                    if (update.deadline_restarted) {
                        log_pointer_dwell(
                            "motion", "native poll restarted delay");
                    }
                    if (previous_phase != TabPointerDwellPhase::repeat_due &&
                        update.repeat_due) {
                        log_pointer_dwell(
                            "due", "stationary delay elapsed");
                    }
                }
            }
        } else if (!g_blocking_composite_input &&
            !g_runtime.pending.active && !g_mutating_viewport) {
            reset_pointer_dwell("native pointer poll became unavailable");
        }
    }

    if (trace.last_report_ms == 0) trace.last_report_ms = now;
    if (elapsed_ms(now, trace.last_report_ms) >= kPointerPollReportMs) {
        std::uint32_t selected{};
        const bool selected_read = guarded_read(
            reinterpret_cast<const void*>(
                context.rdi + kSelectorSelectedIndexOffset),
            selected);
        log_pointer_poll_summary(
            trace,
            now,
            selected_read ? static_cast<std::int32_t>(selected) : -1);
        reset_pointer_poll_interval(trace, now);
    }
}

void observe_input_dispatch_frame_begin(
    safetyhook::Context& context) noexcept {
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) return;
    if (is_active_tab_selector(context.rdi)) {
        // A timed request is published immediately before A394. The ordinary
        // native change path must consume it in this same dispatch. Reaching
        // another frame with the marker intact means no matching callback was
        // delivered; fail closed instead of suppressing the edge forever.
        if (g_pointer_dwell.timed_intent.active) {
            reset_pointer_dwell(
                "timed selection produced no callback before next dispatch");
        }
        g_tab_selection_source = TabSelectionSource::unknown;
        g_pointer_cell_sample_trace.seen_this_dispatch = false;
        g_pointer_dwell.natural_intent = {};
    }
}

void observe_pointer_dispatch_result(
    safetyhook::Context& context) noexcept {
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) return;
    if (!is_active_tab_selector(context.rdi)) return;

    // Reuse Elden Ring's own cursor conversion and GridControl hit test at a
    // stable post-dispatch seam. This remains observation-only: the returned
    // hit is compared with B880 when that natural path ran, otherwise it only
    // proves that stationary per-frame samples are available.
    observe_native_pointer_poll(context);

    // This seam runs immediately after the pointer helper. Do not overwrite a
    // stronger marker if another validated path was observed in this frame.
    // Otherwise a changed selection returned by the helper is the pointer
    // path. R12D is the selected index saved at this frame's entry seam.
    std::uint32_t selected{};
    const bool selection_changed =
        (context.rax & 0xFFu) != 0u &&
        guarded_read(
            reinterpret_cast<const void*>(
                context.rdi + kSelectorSelectedIndexOffset),
            selected) &&
        selected != static_cast<std::uint32_t>(context.r12);
    if (selection_changed &&
        g_tab_selection_source == TabSelectionSource::unknown) {
        g_tab_selection_source = TabSelectionSource::pointer;
    }
}

void observe_horizontal_navigation_commit(
    safetyhook::Context& context) noexcept {
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) return;
    if (!is_active_tab_selector(context.rdi)) return;

    const auto delta_x = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(context.rbx));
    const auto delta_y = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(context.rbx >> 32u));
    if (delta_y != 0 || (delta_x != -1 && delta_x != 1)) return;

    if (g_pointer_dwell.policy.active()) {
        log_pointer_dwell(
            "controller", "native horizontal navigation took priority");
        g_pointer_dwell.policy.controller_navigation();
        g_pointer_dwell.top = nullptr;
        g_pointer_dwell.transaction_ticket = 0;
        clear_pointer_dwell_intents();
    }

    g_tab_selection_source = delta_x < 0
        ? TabSelectionSource::navigation_previous
        : TabSelectionSource::navigation_next;
}

bool call_native_pointer_select(
    void* selector,
    std::uint32_t physical) noexcept {
    if (!selector || !g_native_pointer_select ||
        physical >= active_physical_tab_count()) {
        return false;
    }
    bool completed{};
#if defined(_MSC_VER)
    __try {
#endif
        static_cast<void>(g_native_pointer_select(selector, physical));
        completed = true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_failure(
            "timed pointer cell selection faulted (seh=0x%08lX)",
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
    return completed;
}

void dispatch_due_pointer_dwell(
    safetyhook::Context& context) noexcept {
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) return;
    if (!is_active_tab_selector(context.rdi) ||
        !g_pointer_dwell.policy.repeat_due()) {
        return;
    }

    if (g_tab_selection_source ==
            TabSelectionSource::navigation_previous ||
        g_tab_selection_source == TabSelectionSource::navigation_next) {
        reset_pointer_dwell("controller navigation reached dispatch");
        return;
    }
    if (g_tab_selection_source != TabSelectionSource::unknown ||
        !pointer_dwell_adapter_valid() || g_mutating_viewport ||
        g_runtime.pending.active ||
        !records_match_mapping(g_runtime.top, g_runtime.viewport)) {
        reset_pointer_dwell("timed dispatch safety gate failed");
        return;
    }

    std::uint32_t selected{};
    if (!guarded_read(
            reinterpret_cast<const void*>(
                context.rdi + kSelectorSelectedIndexOffset),
            selected) ||
        selected != static_cast<std::uint32_t>(context.r12)) {
        reset_pointer_dwell("another native input changed selection");
        return;
    }

    const std::size_t edge_physical =
        g_pointer_dwell.policy.physical_slot();
    std::size_t target_logical{invalid_tab_index};
    if (edge_physical >= active_physical_tab_count() ||
        !pointer_dwell_has_directional_target(
            g_runtime.viewport,
            g_pointer_dwell.direction,
            edge_physical,
            &target_logical)) {
        reset_pointer_dwell("timed edge reached catalogue endpoint");
        return;
    }

    if (!call_native_pointer_select(
            reinterpret_cast<void*>(context.rdi),
            static_cast<std::uint32_t>(edge_physical))) {
        reset_pointer_dwell("native timed selection call failed");
        return;
    }

    std::uint32_t updated{};
    if (!guarded_read(
            reinterpret_cast<const void*>(
                context.rdi + kSelectorSelectedIndexOffset),
            updated) ||
        updated != edge_physical || updated == selected) {
        reset_pointer_dwell("native timed selection was rejected");
        return;
    }

    g_pointer_dwell.policy.repeat_dispatched();
    g_pointer_dwell.timed_intent = {
        true,
        g_runtime.top,
        updated,
        target_logical,
    };
    g_tab_selection_source = TabSelectionSource::pointer;
    log_pointer_dwell(
        "dispatch", "native cell setter accepted timed repeat");
}

TabSelectionSource consume_tab_selection_source() noexcept {
    const TabSelectionSource source = g_tab_selection_source;
    g_tab_selection_source = TabSelectionSource::unknown;
    return source;
}

TabViewportChange calculate_transition(
    TabViewport& candidate,
    std::size_t old_physical,
    std::size_t new_physical,
    TabSelectionSource source) noexcept {
    static_cast<void>(old_physical);
    if (source == TabSelectionSource::navigation_next) {
        return candidate.move(
            TabDirection::next, TabBoundaryBehavior::wrap);
    }
    if (source == TabSelectionSource::navigation_previous) {
        return candidate.move(
            TabDirection::previous, TabBoundaryBehavior::wrap);
    }
    // Pointer input resolves the physical cell against the mapping that is
    // actually visible. Never infer circular next/previous from 0 <-> 7: a
    // direct mouse hover on the opposite edge is not controller wrap.
    return candidate.select_physical_aligned(new_physical);
}

bool arm_catalogue_presentation_refresh(void* top) noexcept {
    if (!g_runtime.catalogue_presentation_refresh.pending()) return false;

    InPlaceRgba8RebindTransaction& transaction =
        g_in_place_rgba8_rebind_transaction;
    if (!top || !g_runtime.active || g_runtime.top != top ||
        !g_rgba8_catalogue_ready.load(std::memory_order_acquire) ||
        g_rgba8_publication_disabled.load(std::memory_order_acquire) ||
        g_mutating_viewport || g_runtime.pending.active ||
        transaction.active || transaction.failed ||
        g_runtime.destructive_transition_failed ||
        !all_rgba8_retirement_debts_zero() ||
        !validate_top(top) ||
        !records_match_mapping(top, g_runtime.viewport)) {
        return false;
    }

    const DeferredTabPresentationRefreshPoll refresh_poll =
        g_runtime.catalogue_presentation_refresh.poll(
            g_runtime.presentation_epoch,
            g_runtime.viewport.mapping_generation(),
            g_composite_frame_sequence);
    if (refresh_poll != DeferredTabPresentationRefreshPoll::ready) {
        return false;
    }

    InjectedMovieFingerprint current_fingerprint{};
    const bool fingerprint_exact =
        sample_current_movie_fingerprint(top, current_fingerprint) &&
        same_movie_fingerprint(
            current_fingerprint, g_injected_movie_fingerprint) &&
        g_injected_movie_fingerprint.presentation_epoch ==
            g_runtime.presentation_epoch;
    if (!fingerprint_exact) return false;

    InjectedHostLayout source{};
    InjectedHostLayout target{};
    if (!snapshot_injected_host_layout(source) ||
        !derive_injected_host_layout(g_runtime.viewport, target)) {
        g_runtime.catalogue_presentation_refresh.consume();
        log_failure(
            "initial catalogue presentation refresh could not derive an exact source/target host layout; vector fallbacks remain visible");
        return false;
    }

    if (same_injected_host_layout(source, target)) {
        // Capacities eight and nine expose only stock tabs plus the overflow
        // marker on the initial window, so catalogue publication changes no
        // visible presentation and needs no display-tree mutation.
        g_runtime.catalogue_presentation_refresh.consume();
        log_line(
            "initial catalogue presentation refresh not required for the current viewport");
        return false;
    }

    if (!exact_initial_catalogue_upgrade(source, target)) {
        g_runtime.catalogue_presentation_refresh.consume();
        log_failure(
            "initial catalogue presentation refresh refused a non-vector-to-RGBA ownership topology (source={injected:0x%08X rgba:0x%08X vector:0x%08X} target={injected:0x%08X rgba:0x%08X vector:0x%08X}); vector fallbacks remain visible",
            static_cast<unsigned>(source.injected_mask),
            static_cast<unsigned>(source.rgba_mask),
            static_cast<unsigned>(source.vector_mask),
            static_cast<unsigned>(target.injected_mask),
            static_cast<unsigned>(target.rgba_mask),
            static_cast<unsigned>(target.vector_mask));
        return false;
    }

    std::uint32_t selected_physical{};
    const std::size_t selected_logical =
        g_runtime.viewport.selected_logical();
    const std::size_t expected_physical =
        g_runtime.viewport.selected_physical();
    const LogicalTab* const selected_tab =
        runtime_logical_tab(selected_logical);
    if (!selected_tab ||
        expected_physical >= active_physical_tab_count() ||
        !read_selected_physical(top, selected_physical) ||
        selected_physical != expected_physical) {
        return false;
    }

    PendingTransition refresh{};
    refresh.active = true;
    refresh.presentation_refresh = true;
    refresh.top = top;
    refresh.candidate = g_runtime.viewport;
    refresh.native_physical = selected_physical;
    refresh.desired_physical = selected_physical;
    refresh.provisional_logical = selected_logical;
    refresh.desired_logical = selected_logical;
    refresh.previous_logical = selected_logical;
    refresh.provisional_category = selected_tab->category;
    refresh.desired_category = selected_tab->category;
    refresh.requested_qpc = query_performance_ticks();
    g_runtime.pending = refresh;
    g_runtime.catalogue_presentation_refresh.consume();
    log_line(
        "initial catalogue presentation refresh armed epoch=%llu window=%zu generation=%llu sourceVector=0x%08X targetRgba=0x%08X",
        static_cast<unsigned long long>(g_runtime.presentation_epoch),
        g_runtime.viewport.window_start(),
        static_cast<unsigned long long>(
            g_runtime.viewport.mapping_generation()),
        static_cast<unsigned>(source.vector_mask),
        static_cast<unsigned>(target.rgba_mask));
    return true;
}

void abandon_pending_to_native_selection() noexcept {
    if (!g_runtime.pending.active) return;
    const std::uint32_t native_physical =
        g_runtime.pending.native_physical;
    const std::uint64_t pointer_dwell_ticket =
        g_runtime.pending.pointer_dwell_ticket;
    g_runtime.pending = {};
    fail_pointer_dwell_transition(
        pointer_dwell_ticket,
        "viewport remap was abandoned");
    const TabViewportChange fallback =
        g_runtime.viewport.select_physical(native_physical);
    if (!fallback.valid) {
        // Active native records may borrow catalogue strings. Even this
        // nominally unreachable corrupt fallback must preserve their owner.
        g_rgba8_publication_disabled.store(
            true, std::memory_order_release);
        g_runtime.destructive_transition_failed = true;
        log_failure(
            "pending-transition fallback index was invalid; runtime storage is retained and viewport mutation is suspended");
    }
}

PendingTransitionCommitResult commit_pending_transition(
    void* top) noexcept {
    PendingTransition& pending = g_runtime.pending;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    bool projection_post_attach_failure{};
    bool projection_selector_target_exact{true};
#endif
    const auto freeze_destructive_transaction =
        [&pending](const char* reason) noexcept {
            if (!g_in_place_rgba8_rebind_transaction.active) return false;
            const std::uint64_t ticket = pending.pointer_dwell_ticket;
            (void)freeze_active_two_phase_rgba8_transaction(reason);
            pending = {};
            fail_pointer_dwell_transition(ticket, reason);
            return true;
        };
    if (!pending.active || pending.top != top || !validate_top(top) ||
        !records_match_mapping(top, g_runtime.viewport)) {
        const bool frozen = freeze_destructive_transaction(
            "pending transition validation or source mapping failed");
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        if (!frozen && pending.projection_refresh) {
            // No target record or Shape was touched, so dropping only the
            // captured transaction is exact. The immutable desired generation
            // remains queued and may be re-armed after source validation.
            pending = {};
        }
#else
        (void)frozen;
#endif
        return PendingTransitionCommitResult::failed;
    }
    if (g_in_place_rgba8_rebind_transaction.active &&
        !g_in_place_rgba8_rebind_transaction.failed &&
        g_in_place_rgba8_rebind_transaction.stage ==
            InPlaceRgba8RebindStage::waiting_retirement) {
        return PendingTransitionCommitResult::waiting;
    }

    const bool repaint_required =
        pending.presentation_refresh ||
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        pending.projection_refresh ||
#endif
        pending.candidate.mapping_generation() !=
            g_runtime.viewport.mapping_generation() ||
        pending.candidate.window_start() !=
            g_runtime.viewport.window_start() ||
        tab_overflow_indicators_changed(
            g_runtime.viewport, pending.candidate);
    if (pending.diagnose_wrap) {
        log_wrap_panel_state(top, "commit-before-remap",
            pending.previous_logical, pending.desired_logical);
    }

    g_mutating_viewport = true;
    if (repaint_required) {
        ResolvedTabLayers layers{};
        if (!resolve_tab_layers(top, layers)) {
            g_mutating_viewport = false;
            log_failure(
                "viewport remap aborted before mutation: dual-layer cells unavailable");
            if (freeze_destructive_transaction(
                    "dual-layer cells unavailable after phase-A clear")) {
                return PendingTransitionCommitResult::failed;
            }
            abandon_pending_to_native_selection();
            return PendingTransitionCommitResult::failed;
        }
        const bool selector_already_relocated =
            g_in_place_rgba8_rebind_transaction.active &&
            (g_in_place_rgba8_rebind_transaction.stage ==
                    InPlaceRgba8RebindStage::ready_to_repaint ||
                g_in_place_rgba8_rebind_transaction.stage ==
                    InPlaceRgba8RebindStage::repainting);
        if (
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            !pending.projection_refresh &&
#endif
            !selector_already_relocated &&
            pending.desired_physical != pending.native_physical &&
            !invoke_selector_select(top, pending.desired_physical)) {
            g_mutating_viewport = false;
            destroy_tab_layers(layers);
            log_failure(
                "viewport remap aborted: selector refused physical=%u",
                static_cast<unsigned>(pending.desired_physical));
            if (freeze_destructive_transaction(
                    "selector relocation failed during transaction")) {
                return PendingTransitionCommitResult::failed;
            }
            abandon_pending_to_native_selection();
            return PendingTransitionCommitResult::failed;
        }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        const bool destructive_failure_before_mapping =
            g_runtime.destructive_transition_failed;
#endif
        const bool mapping_applied = [&]() noexcept {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            if (pending.projection_refresh) {
                if (!pending.projection_generation) return false;
                ScopedTargetTabProjectionGeneration target_generation{
                    pending.projection_generation.get()};
                return apply_mapping(top, pending.candidate, layers);
            }
#endif
            return apply_mapping(top, pending.candidate, layers);
        }();
        if (!mapping_applied) {
            if (freeze_destructive_transaction(
                    "two-phase Shape detach/target repaint failed")) {
                g_mutating_viewport = false;
                return PendingTransitionCommitResult::failed;
            }
            if (pending.desired_physical != pending.native_physical) {
                (void)invoke_selector_select(
                    top, pending.native_physical);
            }
            g_mutating_viewport = false;
            log_failure("viewport remap record rewrite/repaint failed");
            abandon_pending_to_native_selection();
            return PendingTransitionCommitResult::failed;
        }
        if (g_in_place_rgba8_rebind_transaction.active &&
            !g_in_place_rgba8_rebind_transaction.failed &&
            g_in_place_rgba8_rebind_transaction.stage ==
                InPlaceRgba8RebindStage::waiting_retirement) {
            g_mutating_viewport = false;
            return PendingTransitionCommitResult::waiting;
        }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        if (pending.projection_refresh &&
            !destructive_failure_before_mapping &&
            g_runtime.destructive_transition_failed) {
            // apply_mapping deliberately retains the target records/ownership
            // after a postflight failure because undoing a partially retired
            // Shape bank is unsafe. Treat that retained target as failed, not
            // as a successful projection generation.
            projection_post_attach_failure = true;
            g_research_failed = true;
            log_failure(
                "atomic tab projection retained its target mapping after an inexact final Shape handoff; target ownership will be published coherently, but mutation is frozen until teardown");
        }
        if (pending.projection_refresh) {
            const bool selector_call =
                pending.desired_physical == pending.native_physical ||
                invoke_selector_select(top, pending.desired_physical);
            std::uint32_t observed_physical{};
            const bool selector_read = read_selected_physical(
                top, observed_physical);
            projection_selector_target_exact = selector_call &&
                selector_read &&
                observed_physical == pending.desired_physical;
            if (!projection_selector_target_exact) {
                // The complete target bank is already attached and its source
                // bank retired. It is no longer safe to repaint the source
                // over it. Retain its ownership below, then freeze mutation.
                g_runtime.destructive_transition_failed = true;
                g_research_failed = true;
                projection_post_attach_failure = true;
                log_failure(
                    "atomic tab projection attached its target mapping, but selector relocation was not exact (call=%u read=%u observed=%u expected=%u); target ownership is retained and mutation is frozen until teardown",
                    selector_call ? 1U : 0U,
                    selector_read ? 1U : 0U,
                    static_cast<unsigned>(observed_physical),
                    static_cast<unsigned>(pending.desired_physical));
            }
        }
#endif
    } else {
        // A content-only route transaction must leave every record and icon
        // byte untouched. The native selector already points at its target.
        if (pending.candidate.mapping_generation() !=
                g_runtime.viewport.mapping_generation() ||
            pending.candidate.window_start() !=
                g_runtime.viewport.window_start() ||
            tab_overflow_indicators_changed(
                g_runtime.viewport, pending.candidate)) {
            g_mutating_viewport = false;
            const std::uint64_t ticket = pending.pointer_dwell_ticket;
            pending = {};
            fail_pointer_dwell_transition(
                ticket, "content-only transaction changed presentation");
            return PendingTransitionCommitResult::failed;
        }
        log_line(
            "graphicsCarrier contentOnly logical=%zu->%zu window=%zu generation=%llu iconRepaint=0 recordRewrite=0",
            pending.previous_logical, pending.desired_logical,
            g_runtime.viewport.window_start(),
            static_cast<unsigned long long>(
                g_runtime.viewport.mapping_generation()));
    }

    if (pending.diagnose_wrap) {
        log_wrap_panel_state(top, "commit-after-mapping",
            pending.previous_logical, pending.desired_logical);
    }

    const bool route_transition =
        pending.previous_logical != pending.desired_logical &&
        (logical_uses_graphics_carrier(pending.previous_logical) ||
            logical_uses_graphics_carrier(pending.desired_logical));
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const bool projection_route_transition = pending.projection_refresh &&
        pending.projection_selected_hidden &&
        projection_selector_target_exact;
    const bool ordinary_route_transition = !pending.projection_refresh &&
        (pending.provisional_category != pending.desired_category ||
            route_transition ||
            pending.previous_logical != pending.desired_logical);
#else
    const bool projection_route_transition = false;
    const bool ordinary_route_transition =
        pending.provisional_category != pending.desired_category ||
        route_transition ||
        pending.previous_logical != pending.desired_logical;
#endif
    if (projection_route_transition || ordinary_route_transition) {
        const bool custom_route_transition =
            is_custom_logical(pending.previous_logical) ||
            is_custom_logical(pending.desired_logical);
        invoke_switch_panel(top, pending.desired_category,
            custom_route_transition
                ? PanelStateIsolationReason::corrective
                : PanelStateIsolationReason::none);
        if (!g_research_switch_succeeded) {
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            if (pending.projection_refresh) {
                // As with a post-attach selector failure, the target Shape
                // bank and records are already authoritative and cannot be
                // rolled back safely. Retain/publish their ownership and stop
                // every later mutation until exact native teardown.
                projection_post_attach_failure = true;
                g_runtime.destructive_transition_failed = true;
                g_research_failed = true;
                log_failure(
                    "atomic tab projection attached its target mapping, but the stock fallback carrier switch failed; target ownership is retained and mutation is frozen until teardown");
            } else
#endif
            {
            const std::uint64_t ticket = pending.pointer_dwell_ticket;
            if (repaint_required) {
                // Records/icons/selector now describe the candidate. Publish
                // it before freezing further carrier transitions.
                g_runtime.viewport = pending.candidate;
                pending = {};
                g_mutating_viewport = false;
                fail_pointer_dwell_transition(
                    ticket, "carrier switch failed after viewport repaint");
                return PendingTransitionCommitResult::failed;
            }
            const std::uint32_t old_physical =
                static_cast<std::uint32_t>(
                    g_runtime.viewport.selected_physical());
            const bool restored =
                invoke_selector_select(top, old_physical);
            std::uint32_t observed{};
            const bool exact = restored &&
                read_selected_physical(top, observed) &&
                observed == old_physical;
            pending = {};
            g_mutating_viewport = false;
            fail_pointer_dwell_transition(
                ticket, "content-only carrier switch failed");
            log_failure(
                "Graphics carrier content-only failure selectorRestore={call:%u observed:%u exact:%u}; custom transitions frozen, native Back remains live",
                restored ? 1u : 0u, static_cast<unsigned>(observed),
                exact ? 1u : 0u);
            return PendingTransitionCommitResult::failed;
            }
        }
    }

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (pending.projection_refresh) {
        const auto committed_generation = pending.projection_generation;
        bool target_records_exact{};
        if (committed_generation) {
            ScopedTargetTabProjectionGeneration target_generation{
                committed_generation.get()};
            target_records_exact = records_match_mapping(
                top, pending.candidate);
        }
        if (!target_records_exact) {
            projection_post_attach_failure = true;
            g_runtime.destructive_transition_failed = true;
            g_research_failed = true;
            log_failure(
                "atomic tab projection target records did not match the staged desired generation after Shape handoff; target ownership is retained and mutation is frozen until teardown");
        }

        // From this point the native records and target Shape bank borrow the
        // staged title/resource mapping. Flip the process-owned title bank and
        // projection pointer before acknowledging the immutable generation;
        // no renderer callback is forwarded between these publication steps.
        commit_staged_projection_titles();
        g_tab_projection_generation = committed_generation;
        TabGenerationPublication publication{};
        if (!projection_post_attach_failure) {
            publication = publish_tab_generation(committed_generation);
            if (!publication.accepted) {
                projection_post_attach_failure = true;
                g_runtime.destructive_transition_failed = true;
                g_research_failed = true;
                log_failure(
                    "atomic tab projection lost its applied-generation acknowledgement after target Shape handoff; target ownership is retained, the desired generation remains pending, and mutation is frozen until teardown");
            }
        }

        const std::uint64_t committed_revision = committed_generation
            ? committed_generation->revision
            : 0U;
        const std::size_t committed_visible = committed_generation
            ? committed_generation->visible_order.size()
            : 0U;
        const bool selected_hidden = pending.projection_selected_hidden;
        if (projection_post_attach_failure) {
            // Distinguish this retained-target projection freeze from every
            // other destructive runtime fault. Only this exact state may be
            // released by the native Graphics-carrier teardown generation.
            g_runtime.frozen_projection_retained_target = true;
        }
        g_runtime.viewport = pending.candidate;
        pending = {};
        g_mutating_viewport = false;
        if (!projection_post_attach_failure && publication.accepted) {
            // Selector labels and tab main/continuation headings now resolve
            // through the same applied generation. Refresh only after that
            // acknowledgement so no callback can observe a split revision.
            invoke_refresh_title(top);
        }
        erui::detail::logf(
            projection_post_attach_failure
                ? erui::LogLevel::error
                : erui::LogLevel::info,
            "NativeTabs atomic projection %s: revision=%llu visibleCustom=%zu logical=%zu physical=%zu selectedLogical=%zu hiddenFallback=%u acknowledged=%u latest=%u",
            projection_post_attach_failure ? "frozen" : "committed",
            static_cast<unsigned long long>(committed_revision),
            committed_visible,
            active_logical_tab_count(),
            active_physical_tab_count(),
            g_runtime.viewport.selected_logical(),
            selected_hidden ? 1U : 0U,
            publication.accepted ? 1U : 0U,
            publication.latest ? 1U : 0U);
        return projection_post_attach_failure
            ? PendingTransitionCommitResult::failed
            : PendingTransitionCommitResult::committed;
    }
#endif

    if (!pending.presentation_refresh) {
        log_research_carrier_selection(top, "commit",
            pending.desired_logical, pending.desired_category);
    }
    if (pending.diagnose_wrap) {
        log_wrap_panel_state(top, "commit-after-switch",
            pending.previous_logical, pending.desired_logical);
    }
    if (!pending.presentation_refresh) invoke_refresh_title(top);

    const std::uint32_t event =
        g_transition_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumTransitionLogs) {
        log_line(
            "remap[%u] kind=%s nativePhysical=%u desiredPhysical=%u provisionalLogical=%zu desiredLogical=%zu category=%d->%d window=%zu->%zu generation=%llu",
            static_cast<unsigned>(event),
            pending.presentation_refresh
                ? "catalogue-refresh"
                : (repaint_required ? "presentation" : "content-only"),
            static_cast<unsigned>(pending.native_physical),
            static_cast<unsigned>(pending.desired_physical),
            pending.provisional_logical, pending.desired_logical,
            pending.provisional_category, pending.desired_category,
            g_runtime.viewport.window_start(),
            pending.candidate.window_start(),
            static_cast<unsigned long long>(
                pending.candidate.mapping_generation()));
    }
    const bool completed_catalogue_refresh =
        pending.presentation_refresh;
    const std::uint64_t ticket = pending.pointer_dwell_ticket;
    g_runtime.viewport = pending.candidate;
    pending = {};
    g_mutating_viewport = false;
    if (!completed_catalogue_refresh) {
        finish_pointer_dwell_transition(
            ticket, repaint_required
                ? "viewport remap committed"
                : "content-only carrier route committed");
    }
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (completed_catalogue_refresh) {
        erui::detail::logf(
            erui::LogLevel::info,
            "NativeTabs initial custom-icon presentation refresh completed: physical=%zu logical=%zu",
            active_physical_tab_count(),
            active_logical_tab_count());
    }
#endif
    return PendingTransitionCommitResult::committed;
}

void __fastcall selection_callback_detour(
    void* callable,
    const std::int32_t* physical_index) noexcept {
    SelectionCallbackFn const original =
        g_original_selection.load(std::memory_order_acquire);
    if (!original) return;
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) {
        original(callable, physical_index);
        return;
    }
    if (g_mutating_viewport) {
        g_tab_selection_source = TabSelectionSource::unknown;
        return;
    }

    std::int32_t next_physical{-1};
    void* top{};
    const bool physical_read = guarded_read(
        physical_index, next_physical);
    const bool top_read = callable && guarded_read(
        static_cast<const std::byte*>(callable) + sizeof(void*),
        top);
    const bool belongs_to_active_runtime = top_read &&
        g_runtime.active && g_runtime.top == top;
    if (!physical_read || !top_read ||
        next_physical < 0 ||
        next_physical >=
            static_cast<std::int32_t>(active_physical_tab_count()) ||
        !g_runtime.active || g_runtime.top != top) {
        if (belongs_to_active_runtime &&
            (g_pointer_dwell.policy.active() ||
                g_pointer_dwell.timed_intent.active)) {
            reset_pointer_dwell(
                "active tab selection callback failed validation");
            g_tab_selection_source = TabSelectionSource::unknown;
        }
        original(callable, physical_index);
        return;
    }

    const TabSelectionSource selection_source =
        consume_tab_selection_source();

    // Blocking the composite input byte cannot cancel a horizontal repeat
    // that Elden Ring latched earlier in the same dispatch. Such a callback
    // can therefore arrive during the one-renderer-frame RGBA8 retirement
    // wait. It must never replace the authoritative PendingTransition or move
    // the selector away from that transaction's source/target coordinates.
    if (g_runtime.pending.active && g_runtime.pending.top == top) {
        PendingTransition& pending = g_runtime.pending;
        const InPlaceRgba8RebindTransaction& transaction =
            g_in_place_rgba8_rebind_transaction;
        const bool target_relocated = transaction.active &&
            transaction.top == top
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
            && !pending.projection_refresh
#endif
            ;
        const std::uint32_t anchor = tab_transition_selector_anchor(
            target_relocated
                ? TabTransitionSelectorAnchor::target
                : TabTransitionSelectorAnchor::source,
            pending.native_physical,
            pending.desired_physical);

        bool restore_call{};
        {
            ScopedBooleanTrue mutating{g_mutating_viewport};
            restore_call = invoke_selector_select(top, anchor);
        }
        std::uint32_t restored_physical{};
        const bool restore_read = read_selected_physical(
            top, restored_physical);
        const bool restored = restore_read && restored_physical == anchor;

        g_pointer_dwell.natural_intent = {};
        g_pointer_dwell.timed_intent = {};
        if ((selection_source ==
                TabSelectionSource::navigation_previous ||
             selection_source == TabSelectionSource::navigation_next) &&
            g_pointer_dwell.policy.active()) {
            reset_pointer_dwell(
                "controller repeat overlapped a pending tab transition");
        }

        const std::uint32_t event =
            g_serialized_selection_log_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        if (event <= kMaximumTransitionLogs) {
            log_line(
                "heldOverlap[%u] source=%s callbackPhysical=%d phase=%s anchor=%u restore={call:%u read:%u observed:%u exact:%u} pending={native:%u desired:%u logical:%zu->%zu window:%zu->%zu generation:%llu}; callback serialized without replacing pending transition",
                static_cast<unsigned>(event),
                selection_source_name(selection_source),
                next_physical,
                target_relocated ? "target" : "source",
                static_cast<unsigned>(anchor),
                restore_call ? 1U : 0U,
                restore_read ? 1U : 0U,
                static_cast<unsigned>(restored_physical),
                restored ? 1U : 0U,
                static_cast<unsigned>(pending.native_physical),
                static_cast<unsigned>(pending.desired_physical),
                pending.previous_logical,
                pending.desired_logical,
                g_runtime.viewport.window_start(),
                pending.candidate.window_start(),
                static_cast<unsigned long long>(
                    pending.candidate.mapping_generation()));
        }
        if (!restored) {
            const std::uint64_t pointer_dwell_ticket =
                pending.pointer_dwell_ticket;
            if (transaction.active) {
                (void)freeze_active_two_phase_rgba8_transaction(
                    "overlapping selection could not restore the in-flight selector anchor");
            }
            pending = {};
            fail_pointer_dwell_transition(
                pointer_dwell_ticket,
                "overlapping selection could not restore its selector anchor");
            log_failure(
                "held-repeat serialization could not restore selector anchor=%u (call=%u read=%u observed=%u); the overlapping callback remained suppressed",
                static_cast<unsigned>(anchor),
                restore_call ? 1U : 0U,
                restore_read ? 1U : 0U,
                static_cast<unsigned>(restored_physical));
        }
        return;
    }

    if (!records_match_mapping(top, g_runtime.viewport)) {
        if (g_pointer_dwell.policy.active() ||
            g_pointer_dwell.timed_intent.active) {
            reset_pointer_dwell(
                "active tab selection callback failed source mapping validation");
        }
        original(callable, physical_index);
        return;
    }

    if (selection_source == TabSelectionSource::navigation_previous ||
        selection_source == TabSelectionSource::navigation_next) {
        if (g_pointer_dwell.policy.active()) {
            reset_pointer_dwell("controller selection callback");
        }
    }

    const std::size_t old_physical =
        g_runtime.viewport.selected_physical();
    if (old_physical >= active_physical_tab_count()) {
        if (g_pointer_dwell.policy.active() ||
            g_pointer_dwell.timed_intent.active) {
            reset_pointer_dwell(
                "active viewport had no selected physical tab");
        }
        original(callable, physical_index);
        return;
    }

    TabViewport candidate = g_runtime.viewport;
    const std::size_t new_physical =
        static_cast<std::size_t>(next_physical);
    const PointerDwellNaturalIntent natural_intent =
        g_pointer_dwell.natural_intent;
    const PointerDwellTimedIntent timed_intent =
        g_pointer_dwell.timed_intent;
    g_pointer_dwell.natural_intent = {};
    g_pointer_dwell.timed_intent = {};
    const bool natural_dwell_transition =
        g_mouse_dwell_enabled &&
        selection_source == TabSelectionSource::pointer &&
        natural_intent.active && natural_intent.top == top &&
        natural_intent.physical == new_physical &&
        !g_pointer_dwell.policy.active() &&
        overflow_edge_for_physical(
            g_runtime.viewport, new_physical) == natural_intent.edge;
    const bool timed_dwell_transition =
        selection_source == TabSelectionSource::pointer &&
        timed_intent.active && timed_intent.top == top &&
        timed_intent.physical == new_physical &&
        g_pointer_dwell.policy.phase() ==
            TabPointerDwellPhase::awaiting_commit &&
        g_pointer_dwell.top == top;
    std::size_t provisional_logical{invalid_tab_index};
    (void)g_runtime.viewport.logical_for_physical(
        new_physical, provisional_logical);
    const TabViewportChange change = calculate_transition(
        candidate,
        old_physical,
        new_physical,
        selection_source);
    // Selecting or leaving an edge can reveal/hide its presentation-only
    // overflow marker without moving the logical viewport window. It uses the
    // same fresh-Shape transaction as a mapping change.
    const bool presentation_changed = change.valid &&
        tab_overflow_indicators_changed(g_runtime.viewport, candidate);
    const bool frozen_carrier_change = change.valid &&
        change.logical_selection_changed && g_research_failed &&
        (logical_uses_graphics_carrier(change.previous_logical) ||
            logical_uses_graphics_carrier(change.selected_logical));
    const bool freeze_mapping_change = change.valid &&
        (((change.mapping_changed || presentation_changed) &&
             g_runtime.destructive_transition_failed) ||
            frozen_carrier_change);
    if (freeze_mapping_change) {
        g_mutating_viewport = true;
        const bool restore_call = invoke_selector_select(
            top, static_cast<std::uint32_t>(old_physical));
        std::uint32_t restore_readback{};
        const bool restore_read = read_selected_physical(
            top, restore_readback);
        g_mutating_viewport = false;
        g_runtime.pending = {};
        reset_pointer_dwell(
            "a destructive RGBA8 transition previously failed");
        const bool restored = restore_call && restore_read &&
            restore_readback == old_physical;
        log_line(
            "rgba8Rebind freeze refused remap after destructive failure completedTransitions=%llu logical=%zu->%zu window=%zu->%zu selectorRestore={call:%u read:%u observed:%u expected:%zu exact:%u} publicationDisabled=%u",
            static_cast<unsigned long long>(
                g_runtime.completed_shape_replacement_transitions),
            change.previous_logical,
            change.selected_logical,
            change.previous_window_start,
            change.window_start,
            restore_call ? 1U : 0U,
            restore_read ? 1U : 0U,
            static_cast<unsigned>(restore_readback),
            old_physical,
            restored ? 1U : 0U,
            g_rgba8_publication_disabled.load(
                std::memory_order_acquire) ? 1U : 0U);
        if (restored) return;

        // The validated selector setter is the only supported way to restore
        // the old cell. If it does not also read back exactly, keep the current
        // record window frozen and let the native callback finish on the
        // selector's observed cell. Selecting that physical cell without
        // alignment cannot remap the viewport, so the highlight, page, and
        // runtime logical selection converge without another GFX mutation.
        const std::size_t corrective_physical = restore_read
            ? static_cast<std::size_t>(restore_readback)
            : (restore_call ? old_physical : new_physical);
        TabViewport corrective = g_runtime.viewport;
        const TabViewportChange corrective_change =
            corrective.select_physical(corrective_physical);
        const bool corrective_exact = corrective_change.valid &&
            !corrective_change.mapping_changed &&
            corrective_change.selected_physical == corrective_physical;
        if (corrective_exact) {
            g_runtime.viewport = corrective;
        }
        const std::int32_t corrective_index = static_cast<std::int32_t>(
            corrective_exact ? corrective_physical : new_physical);
        log_failure(
            "rgba8Rebind selector restoration was not exact; applying fail-closed native callback correction without viewport remap (restoreCall=%u restoreRead=%u observed=%u callbackPhysical=%zu correctivePhysical=%d correctiveExact=%u restartRequired=1)",
            restore_call ? 1U : 0U,
            restore_read ? 1U : 0U,
            static_cast<unsigned>(restore_readback),
            new_physical,
            corrective_index,
            corrective_exact ? 1U : 0U);
        original(callable, &corrective_index);
        return;
    }
    std::uint64_t pointer_dwell_ticket{};
    if (natural_dwell_transition && change.valid &&
        change.logical_selection_changed) {
        g_pointer_dwell.policy.begin_transition(
            natural_intent.edge,
            new_physical,
            natural_intent.x_bits,
            natural_intent.y_bits);
        if (g_pointer_dwell.policy.phase() ==
            TabPointerDwellPhase::awaiting_commit) {
            g_pointer_dwell.top = top;
            g_pointer_dwell.direction = direction_for_pointer_edge(
                natural_intent.edge);
            pointer_dwell_ticket = next_pointer_dwell_ticket();
            g_pointer_dwell.transaction_ticket = pointer_dwell_ticket;
            log_pointer_dwell(
                "begin", "native ellipsis hover selected immediately");
        }
    } else if (timed_dwell_transition && change.valid &&
        change.logical_selection_changed &&
        change.selected_logical == timed_intent.logical) {
        pointer_dwell_ticket = next_pointer_dwell_ticket();
        g_pointer_dwell.transaction_ticket = pointer_dwell_ticket;
        log_pointer_dwell(
            "accepted", "timed native callback matched target");
    } else if (timed_intent.active) {
        reset_pointer_dwell("timed native callback did not match target");
    } else if (selection_source == TabSelectionSource::pointer &&
        g_pointer_dwell.policy.active()) {
        reset_pointer_dwell("different pointer selection superseded dwell");
    }
    const bool diagnose_wrap = change.valid &&
        ((change.previous_logical == active_logical_tab_count() - 1 &&
             change.selected_logical == 0) ||
            (change.previous_logical == 0 &&
             change.selected_logical ==
                 active_logical_tab_count() - 1));

    const bool reusable_content_changed = change.valid &&
        change.logical_selection_changed &&
        (logical_uses_graphics_carrier(change.previous_logical) ||
            logical_uses_graphics_carrier(change.selected_logical));
    if (change.valid &&
        (change.mapping_changed || presentation_changed ||
            reusable_content_changed) &&
        change.selected_logical < active_logical_tab_count() &&
        change.selected_physical < active_physical_tab_count() &&
        provisional_logical < active_logical_tab_count()) {
        const LogicalTab* const provisional_tab =
            runtime_logical_tab(provisional_logical);
        const LogicalTab* const selected_tab =
            runtime_logical_tab(change.selected_logical);
        if (!provisional_tab || !selected_tab) {
            g_runtime.pending = {};
            fail_pointer_dwell_transition(
                pointer_dwell_ticket,
                "selection callback could not resolve logical catalogue entries");
            original(callable, physical_index);
            return;
        }
        g_runtime.pending = {
            true,
            diagnose_wrap,
            false,
            top,
            candidate,
            static_cast<std::uint32_t>(new_physical),
            static_cast<std::uint32_t>(change.selected_physical),
            provisional_logical,
            change.selected_logical,
            change.previous_logical,
            provisional_tab->category,
            selected_tab->category,
            pointer_dwell_ticket,
            query_performance_ticks(),
        };
    } else if (change.valid) {
        g_runtime.pending = {};
        g_runtime.viewport = candidate;
        finish_pointer_dwell_transition(
            pointer_dwell_ticket,
            "selection completed without viewport remap");
    } else {
        fail_pointer_dwell_transition(
            pointer_dwell_ticket,
            "selection callback produced an invalid transition");
    }

    const std::uint32_t event =
        g_transition_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumTransitionLogs) {
        log_line(
            "selection[%u] source=%s inferred=%s physical=%zu->%zu logical=%zu->%zu window=%zu->%zu mappingChanged=%u presentationChanged=%u pending=%u",
            static_cast<unsigned>(event),
            selection_source_name(selection_source),
            transition_kind(old_physical, new_physical),
            old_physical,
            new_physical,
            change.previous_logical,
            change.selected_logical,
            change.previous_window_start,
            change.window_start,
            change.mapping_changed ? 1u : 0u,
            presentation_changed ? 1u : 0u,
            g_runtime.pending.active ? 1u : 0u);
    }

    // The native callback intentionally sees the old physical mapping. If a
    // viewport shift is pending, the composite-frame boundary corrects it
    // before the selected panel receives input.
    if (diagnose_wrap) {
        log_wrap_panel_state(
            top,
            "selection-before-native",
            change.previous_logical,
            change.selected_logical);
    }
    const bool valid_native_categories = change.valid &&
        change.previous_logical < active_logical_tab_count() &&
        provisional_logical < active_logical_tab_count();
    const LogicalTab* const previous_tab = valid_native_categories
        ? runtime_logical_tab(change.previous_logical)
        : nullptr;
    const LogicalTab* const provisional_tab = valid_native_categories
        ? runtime_logical_tab(provisional_logical)
        : nullptr;
    const std::int32_t previous_category = valid_native_categories
            && previous_tab
        ? previous_tab->category
        : -1;
    const std::int32_t provisional_category = valid_native_categories
            && provisional_tab
        ? provisional_tab->category
        : -1;
    const bool true_custom_route_involved = valid_native_categories &&
        previous_tab && provisional_tab &&
        (is_custom_logical(change.previous_logical) ||
            is_custom_logical(provisional_logical) ||
            (g_runtime.pending.active &&
                is_custom_logical(g_runtime.pending.desired_logical)));
    const bool crosses_carrier_boundary = true_custom_route_involved &&
        previous_category != provisional_category;
    const bool provisional_differs_from_desired =
        true_custom_route_involved && g_runtime.pending.active &&
        g_runtime.pending.top == top &&
        g_runtime.pending.provisional_category !=
            g_runtime.pending.desired_category;
    const bool provisional_isolation_armed =
        (crosses_carrier_boundary || provisional_differs_from_desired) &&
        begin_panel_state_isolation(
            top,
            provisional_category,
            PanelStateIsolationReason::provisional);
    original(callable, physical_index);
    end_panel_state_isolation(provisional_isolation_armed);
    log_research_carrier_selection(
        top,
        "native-selection",
        provisional_logical,
        provisional_category);
    if (diagnose_wrap) {
        log_wrap_panel_state(
            top,
            "selection-after-native",
            change.previous_logical,
            change.selected_logical);
    }
}

void progress_two_phase_rgba8_retirement(
    void* top,
    std::uint64_t frame_sequence) noexcept {
    InPlaceRgba8RebindTransaction& transaction =
        g_in_place_rgba8_rebind_transaction;
    if (!transaction.active || transaction.failed ||
        transaction.stage !=
            InPlaceRgba8RebindStage::waiting_retirement) {
        return;
    }

    std::uint32_t selected_physical{};
    const std::uint32_t expected_pending_physical =
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        g_runtime.pending.active && g_runtime.pending.projection_refresh
        ? g_runtime.pending.native_physical
        :
#endif
        g_runtime.pending.desired_physical;
    const bool selected_exact = read_selected_physical(
            top, selected_physical) &&
        g_runtime.pending.active &&
        selected_physical == expected_pending_physical;
    const bool coordinates_exact = top && transaction.top == top &&
        g_runtime.active && g_runtime.top == top &&
        transaction.presentation_epoch == g_runtime.presentation_epoch &&
        transaction.source_generation ==
            g_runtime.viewport.mapping_generation() &&
        transaction.source_window == g_runtime.viewport.window_start() &&
        records_match_mapping(top, g_runtime.viewport) &&
        g_runtime.pending.active && g_runtime.pending.top == top &&
        transaction.target_generation ==
            g_runtime.pending.candidate.mapping_generation() &&
        transaction.target_window ==
            g_runtime.pending.candidate.window_start() &&
        transaction.completed_clear_mask == 0 && selected_exact;
    InjectedMovieFingerprint current_fingerprint{};
    const bool fingerprint_exact = coordinates_exact &&
        sample_current_movie_fingerprint(top, current_fingerprint) &&
        same_movie_fingerprint(
            current_fingerprint,
            g_injected_movie_fingerprint);
    const bool reconciled = coordinates_exact && fingerprint_exact &&
        reconcile_rgba8_retirement_debt(
            "two-phase-wait-post-native");
    const TabHostMask injected = g_injected_icon_hosts.load(
        std::memory_order_acquire);
    const TabHostMask rgba = g_rgba8_icon_hosts.load(
        std::memory_order_acquire);
    bool source_assets_exact = true;
    bool staged_assets_empty = true;
    for (std::size_t bit = 0; bit < active_tab_host_count(); ++bit) {
        source_assets_exact = source_assets_exact &&
            g_rgba8_host_assets[bit].load(std::memory_order_acquire) ==
                transaction.source_assets[bit];
        staged_assets_empty = staged_assets_empty &&
            g_staged_rgba8_host_assets[bit].load(
                std::memory_order_acquire) == 0;
    }
    // Reconciliation is also the reference audit for this wait frame: it
    // samples every visible or retiring RawImage once, accepts only the bounded
    // [1 + active, 1 + active + debt] range, and updates debt from that same
    // observation. A second refcount read here could race the renderer's next
    // asynchronous decrement and falsely fail an otherwise valid retirement.
    const bool refs_exact = reconciled;
    const bool ledger_exact = refs_exact &&
        injected == transaction.source_injected_mask &&
        rgba == transaction.source_rgba_mask && source_assets_exact &&
        g_staged_injected_icon_hosts.load(std::memory_order_acquire) == 0 &&
        g_staged_rgba8_icon_hosts.load(std::memory_order_acquire) == 0 &&
        staged_assets_empty;
    const bool debts_zero = all_rgba8_retirement_debts_zero();
    const bool crossed_native_frame =
        frame_sequence > transaction.clear_frame;
    const bool timed_out = frame_sequence >= transaction.clear_frame &&
        frame_sequence - transaction.clear_frame >=
            kInPlaceRebindRetirementTimeoutFrames;

    if (coordinates_exact && fingerprint_exact && ledger_exact &&
        debts_zero && crossed_native_frame) {
        transaction.retirement_completed_qpc =
            query_performance_ticks();
        transaction.retirement_wait_frames =
            frame_sequence - transaction.clear_frame;
        transaction.stage =
            InPlaceRgba8RebindStage::ready_to_repaint;
        log_line(
            "rgba8Handoff transition=%llu prior renderer debt retired frame=%llu waitedFrames=%llu source banks stayed visible; atomic target repaint armed",
            static_cast<unsigned long long>(
                transaction.transition_sequence),
            static_cast<unsigned long long>(frame_sequence),
            static_cast<unsigned long long>(
                frame_sequence - transaction.clear_frame));
        return;
    }

    if (coordinates_exact && fingerprint_exact && ledger_exact &&
        !timed_out) {
        return;
    }

    const std::uint64_t pointer_dwell_ticket =
        g_runtime.pending.pointer_dwell_ticket;
    transaction.failed = true;
    transaction.active = false;
    g_rgba8_publication_disabled.store(
        true, std::memory_order_release);
    g_runtime.destructive_transition_failed = true;
    g_runtime.pending = {};
    fail_pointer_dwell_transition(
        pointer_dwell_ticket,
        timed_out
            ? "two-phase RGBA8 renderer retirement timed out"
            : "two-phase RGBA8 renderer retirement lost exact state");
    log_failure(
        "atomic RGBA8 handoff debt wait failed closed (frame=%llu clearFrame=%llu timeout=%u coordinates=%u selected=%u/%u fingerprint=%u reconciled=%u ledger=%u debtsZero=%u injected=0x%04X rgba=0x%04X sourceAssets=%u stagedEmpty=%u); source Shapes remained visible",
        static_cast<unsigned long long>(frame_sequence),
        static_cast<unsigned long long>(transaction.clear_frame),
        timed_out ? 1U : 0U,
        coordinates_exact ? 1U : 0U,
        static_cast<unsigned>(selected_physical),
        selected_exact ? 1U : 0U,
        fingerprint_exact ? 1U : 0U,
        reconciled ? 1U : 0U,
        ledger_exact ? 1U : 0U,
        debts_zero ? 1U : 0U,
        static_cast<unsigned>(injected),
        static_cast<unsigned>(rgba),
        source_assets_exact ? 1U : 0U,
        staged_assets_empty ? 1U : 0U);
}

void __fastcall composite_frame_detour(
    void* composite,
    float frame_value,
    const std::uint8_t* input_enabled) noexcept {
    CompositeFrameFn const original =
        g_original_composite_frame.load(std::memory_order_acquire);
    if (!original) return;
    if (!g_viewport_callbacks_armed.load(std::memory_order_acquire)) {
        original(composite, frame_value, input_enabled);
        return;
    }

    struct CompletedFrame {
        ~CompletedFrame() { ++g_research_completed_composite_frames; }
    } completed_frame;
    if (g_research_failed) {
        // Freeze only tab mutations. Never swallow native Back/System input.
        original(composite, frame_value, input_enabled);
        // Native teardown can run between frames or from work reached by the
        // original frame callback. Observe it after forwarding native input;
        // only its exact generation boundary may rearm a later menu epoch.
        (void)acknowledge_exact_carrier_teardown();
        return;
    }

    const std::uint64_t frame_sequence = ++g_composite_frame_sequence;

    void* const top = composite
        ? static_cast<std::byte*>(composite) - kCompositeOffset
        : nullptr;
    const bool transaction_active_at_entry =
        g_in_place_rgba8_rebind_transaction.active;
    const bool terminal_failure_at_entry =
        g_in_place_rgba8_rebind_transaction.failed &&
        g_in_place_rgba8_rebind_transaction.stage !=
            InPlaceRgba8RebindStage::none;
    bool force_blocked_input = transaction_active_at_entry;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    TabProjectionRefreshResult projection_refresh{
        TabProjectionRefreshResult::none};
    // Tab batches are isolated from ROW/SUBMENU batches by the host parser.
    // Do not hold the non-recursive row-publication mutex across native calls:
    // selector/panel callbacks can synchronously re-enter client code.
    projection_refresh = apply_pending_tab_projection(top);
    if (projection_refresh == TabProjectionRefreshResult::retiring ||
        projection_refresh == TabProjectionRefreshResult::committed ||
        projection_refresh == TabProjectionRefreshResult::failed) {
        std::uint8_t blocked_input{};
        {
            ScopedBooleanTrue blocking{g_blocking_composite_input};
            original(composite, frame_value, &blocked_input);
        }
        void* const post_top = composite
            ? static_cast<std::byte*>(composite) - kCompositeOffset
            : nullptr;
        (void)reconcile_rgba8_retirement_debt(
            "post-tab-generation-refresh");
        progress_two_phase_rgba8_retirement(
            post_top, frame_sequence);
        return;
    }
#endif
    if (!g_mutating_viewport && top) {
        if (!ensure_runtime(top)) {
            // Quietly retry on later frames while Configuration construction
            // is still publishing its eight stock category records, unless a
            // two-phase transaction had already entered a destructive stage.
        } else {
            static_cast<void>(true) /* Graphics is the rows-only carrier */;
            (void)arm_catalogue_presentation_refresh(top);
            if (g_runtime.pending.active &&
                g_runtime.pending.top == top) {
                const bool catalogue_presentation_refresh =
                    g_runtime.pending.presentation_refresh;
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
                const bool tab_projection_refresh =
                    g_runtime.pending.projection_refresh;
#else
                const bool tab_projection_refresh = false;
#endif
                const bool diagnose_wrap =
                    g_runtime.pending.diagnose_wrap;
                const std::size_t previous_logical =
                    g_runtime.pending.previous_logical;
                const std::size_t desired_logical =
                    g_runtime.pending.desired_logical;
                const PendingTransitionCommitResult commit_result =
                    commit_pending_transition(top);
                const TabTransitionInputAction input_action =
                    tab_transition_input_action(commit_result);
                if (input_action ==
                    TabTransitionInputAction::invoke_with_blocked_input) {
                    std::uint8_t blocked_input{};
                    {
                        ScopedBooleanTrue blocking{
                            g_blocking_composite_input};
                        original(composite, frame_value, &blocked_input);
                    }
                    void* const post_top = composite
                        ? static_cast<std::byte*>(composite) -
                            kCompositeOffset
                        : nullptr;
                    progress_two_phase_rgba8_retirement(
                        post_top, frame_sequence);
                    return;
                }
                if (input_action ==
                    TabTransitionInputAction::invoke_with_live_input) {
                    if (catalogue_presentation_refresh ||
                        tab_projection_refresh) {
                        // This internal repair was not initiated by native
                        // selection input. Keep its commit frame free of a
                        // simultaneous L1/R1 or pointer callback; ordinary
                        // live input resumes on the following frame.
                        std::uint8_t blocked_input{};
                        ScopedBooleanTrue blocking{
                            g_blocking_composite_input};
                        original(composite, frame_value, &blocked_input);
                        return;
                    }
                    // The logical mapping, selector, page, title, and icon
                    // presentation are coherent now. Preserve Elden Ring's
                    // real input gate on this frame so its native held-button
                    // repeat state is not interrupted at every viewport shift.
                    // Retirement frames above remain blocked because their
                    // source/target mapping is intentionally incomplete.
                    if (diagnose_wrap) {
                        log_wrap_panel_state(
                            top,
                            "frame-before-live-call",
                            previous_logical,
                            desired_logical);
                    }
                    if (g_pointer_dwell.policy.active()) {
                        std::uint8_t enabled{};
                        const bool input_gate_read =
                            guarded_read(input_enabled, enabled);
                        const bool cancel_dwell_for_gate =
                            should_cancel_pointer_dwell_for_composite_input(
                                input_action,
                                input_gate_read,
                                enabled != 0);
                        if (cancel_dwell_for_gate) {
                            reset_pointer_dwell(
                                "composite input was disabled");
                        } else if (!input_gate_read || enabled == 0) {
                            // A native pointer selection transiently
                            // publishes a zero gate on the exact frame where
                            // the atomic tab handoff commits. Preserve the
                            // dwell for this one transition-controlled frame.
                            // If the gate remains unavailable, the ordinary
                            // path on the next frame cancels it.
                            log_pointer_dwell(
                                "gate-deferred",
                                "coherent commit deferred the transient disabled input gate");
                        }
                    }
                    original(composite, frame_value, input_enabled);
                    return;
                }
            }
        }

        InPlaceRgba8RebindTransaction& transaction =
            g_in_place_rgba8_rebind_transaction;
        if (transaction.active) {
            // An active transaction must be consumed by the pending-transition
            // branch above. Never let live native input run through an orphaned
            // phase-A clear or ready-to-repaint state.
            (void)freeze_active_two_phase_rgba8_transaction(
                "composite frame could not route the active transaction through its pending commit");
            g_runtime.pending = {};
            reset_pointer_dwell(
                "orphaned two-phase transaction was frozen");
        }
        const bool terminal_failure_now = transaction.failed &&
            transaction.stage != InPlaceRgba8RebindStage::none;
        // Block only the frame that loses or freezes the transaction. The
        // persistent proof/publication gates refuse later remaps, while normal
        // Back/System input remains usable on following frames.
        force_blocked_input = force_blocked_input ||
            (!terminal_failure_at_entry && terminal_failure_now);
    }
    if (force_blocked_input) {
        std::uint8_t blocked_input{};
        ScopedBooleanTrue blocking{g_blocking_composite_input};
        original(composite, frame_value, &blocked_input);
        return;
    }
    if (g_pointer_dwell.policy.active()) {
        std::uint8_t enabled{};
        const bool input_gate_read =
            guarded_read(input_enabled, enabled);
        if (should_cancel_pointer_dwell_for_composite_input(
                TabTransitionInputAction::continue_normal_path,
                input_gate_read,
                enabled != 0)) {
            reset_pointer_dwell("composite input was disabled");
        }
    }
    original(composite, frame_value, input_enabled);

    // The pre-native-composite retained-object run still blackened the
    // renderer. Recompute the menu root after the native call, then let the
    // deferred helper reacquire and validate every AS/Movie object from that
    // post-call epoch. No pointer captured before original() crosses this
    // boundary.
    void* const post_top = composite
        ? static_cast<std::byte*>(composite) - kCompositeOffset
        : nullptr;
    if (!g_mutating_viewport && !g_runtime.pending.active) {
        (void)reconcile_rgba8_retirement_debt(
            "post-native-composite");
        const bool catalogue_ready_before =
            g_rgba8_catalogue_ready.load(std::memory_order_acquire);
        run_raw_image_lifecycle_probe(post_top);
        const bool catalogue_ready_after =
            g_rgba8_catalogue_ready.load(std::memory_order_acquire);
        if (!catalogue_ready_before && catalogue_ready_after && post_top &&
            g_runtime.active && g_runtime.top == post_top &&
            !g_runtime.destructive_transition_failed) {
            // Input may have remapped the viewport while the process-wide
            // catalogue was being constructed. Rebase the epoch request to
            // the mapping that is current at the readiness boundary; later
            // Configuration epochs schedule independently during initialize.
            const bool requested =
                !g_rgba8_publication_disabled.load(
                    std::memory_order_acquire) &&
                g_runtime.catalogue_presentation_refresh.request_if_needed(
                    g_runtime.viewport,
                    kStockLogicalTabCount,
                    g_runtime.presentation_epoch,
                    frame_sequence);
            log_line(
                "RGBA8 catalogue publication rebased epoch presentation refresh=%u epoch=%llu window=%zu generation=%llu",
                requested ? 1U : 0U,
                static_cast<unsigned long long>(
                    g_runtime.presentation_epoch),
                g_runtime.viewport.window_start(),
                static_cast<unsigned long long>(
                    g_runtime.viewport.mapping_generation()));
        }
        run_bitmapdata_property_survival_verification(
            post_top, frame_sequence);
        run_deferred_bitmapdata_constructor_probe(
            post_top, frame_sequence);
    }
}

template <typename Function>
bool prepare_hook(
    std::uintptr_t rva,
    void* detour,
    SafetyHookInline& hook,
    std::atomic<Function>& original,
    const char* name) noexcept {
    auto result = SafetyHookInline::create(
        g_game.base + rva,
        detour,
        SafetyHookInline::StartDisabled);
    if (!result) {
        log_line(
            "ERROR: could not prepare %s hook at RVA 0x%llX (SafetyHook error type %u)",
            name,
            static_cast<unsigned long long>(rva),
            static_cast<unsigned>(result.error().type));
        return false;
    }
    hook = std::move(*result);
    original.store(hook.original<Function>(), std::memory_order_release);
    return true;
}

bool enable_hook(SafetyHookInline& hook, const char* name) noexcept {
    auto result = hook.enable();
    if (!result) {
        log_line(
            "ERROR: could not enable %s hook (SafetyHook error type %u)",
            name,
            static_cast<unsigned>(result.error().type));
        return false;
    }
    return true;
}

bool enable_hook(SafetyHookMid& hook, const char* name) noexcept {
    auto result = hook.enable();
    if (!result) {
        log_line(
            "ERROR: could not enable %s hook (SafetyHook error type %u)",
            name,
            static_cast<unsigned>(result.error().type));
        return false;
    }
    return true;
}

void reset_hooks() noexcept {
    if (g_viewport_hooks_exposed.load(std::memory_order_acquire)) {
        // No quiescence protocol exists for game-thread callbacks. Once the
        // first enable has been attempted, even an apparently disabled hook
        // may have an in-flight trampoline. The pinned host retains the full
        // prepared set instead of invalidating executable storage.
        return;
    }
    g_viewport_callbacks_armed.store(false, std::memory_order_release);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    g_slot_growth_callbacks_armed.store(false, std::memory_order_release);
    g_slot_growth_early_hook.reset();
    g_slot_growth_late_hook.reset();
    g_slot_growth_post_constructor_hook.reset();
    g_category_append = nullptr;
    g_slot_growth_construction = {};
#endif
    g_research_switch_hook.reset();
    g_original_research_switch.store(nullptr, std::memory_order_release);
    g_composite_frame_hook.reset();
    g_selection_hook.reset();
    g_horizontal_navigation_source_hook.reset();
    g_selection_dispatch_hook.reset();
    g_pointer_cell_sample_hook.reset();
    g_pointer_dispatch_source_hook.reset();
    g_input_dispatch_frame_begin_hook.reset();
    g_panel_alias_visibility_hook.reset();
    g_panel_state_copy_hook.reset();
    g_texture_lookup_hook.reset();
    g_image_bind_hook.reset();
    g_original_composite_frame.store(nullptr, std::memory_order_release);
    g_original_selection.store(nullptr, std::memory_order_release);
    g_original_texture_lookup.store(nullptr, std::memory_order_release);
    g_original_image_bind.store(nullptr, std::memory_order_release);
    g_get_pointer_position = nullptr;
    g_grid_hit_test = nullptr;
    g_native_pointer_select = nullptr;
    g_mutating_viewport = false;
    g_blocking_composite_input = false;
    g_tab_selection_source = TabSelectionSource::unknown;
    g_pointer_cell_sample_trace = {};
    reset_pointer_dwell(nullptr);
    g_state_isolation_expectation = {};
}

bool install_hooks_and_interfaces(
    bool install_hooks = true,
    const erui::native::PreservedSharedMenuAddresses*
        preserved_shared = nullptr) noexcept {
    (void)preserved_shared;
    constexpr std::array<std::uint8_t, 16> kSelectionEntry{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0x12,
        0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x49, 0x08, 0x48};
    constexpr std::array<std::uint8_t, 16> kCompositeFrameEntry{
        0x48, 0x8B, 0x89, 0xB8, 0x00, 0x00, 0x00, 0x48,
        0x85, 0xC9, 0x74, 0x07, 0x48, 0x8B, 0x01, 0x48};
    constexpr std::array<std::uint8_t, 16> kSelectorSelectEntry{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48};
    constexpr std::array<std::uint8_t, 10>
        kInputDispatchFrameBeginBytes{
            0x44, 0x8B, 0xA7, 0xD4, 0x00, 0x00, 0x00,
            0x45, 0x33, 0xFF};
    constexpr std::array<std::uint8_t, 15>
        kPointerDispatchResultBytes{
            0x84, 0xC0,
            0x0F, 0x85, 0xC5, 0x01, 0x00, 0x00,
            0x4C, 0x8D, 0xB7, 0x60, 0x07, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 18>
        kHorizontalNavigationCommitBytes{
            0x8B, 0x97, 0xD4, 0x00, 0x00, 0x00,
            0x4C, 0x8D, 0x45, 0x40,
            0x48, 0x8B, 0xCF,
            0xE8, 0xA6, 0x1B, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 7>
        kSelectionDispatchBytes{
            0x44, 0x39, 0xA7, 0xD4, 0x00, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 17>
        kPointerCellSampleBytes{
            0x83, 0xF8, 0xFF,
            0x74, 0x59,
            0x8B, 0xD0,
            0x48, 0x8B, 0xCE,
            0xE8, 0xD1, 0x11, 0x00, 0x00,
            0xB0, 0x01};
    constexpr std::array<std::uint8_t, 21>
        kGetPointerPosition271Entry{
            0x48, 0x89, 0x5C, 0x24, 0x08,
            0x57,
            0x48, 0x83, 0xEC, 0x30,
            0x48, 0x8B, 0xDA,
            0x48, 0x8B, 0xF9,
            0xE8, 0x4B, 0x05, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 23>
        kGridHitTest271Entry{
            0x48, 0x8B, 0xC4,
            0x56,
            0x57,
            0x41, 0x56,
            0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00,
            0x48, 0xC7, 0x44, 0x24, 0x20,
            0xFE, 0xFF, 0xFF, 0xFF};
    constexpr std::array<std::uint8_t, 21>
        kNativePointerSelect271Entry{
            0x48, 0x89, 0x5C, 0x24, 0x18,
            0x89, 0x54, 0x24, 0x10,
            0x57,
            0x48, 0x83, 0xEC, 0x20,
            0x44, 0x8B, 0x99, 0xDC, 0x00, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 16> kResolvePathEntry{
        0x4C, 0x89, 0x44, 0x24, 0x18, 0x4C, 0x89, 0x4C,
        0x24, 0x20, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56};
    constexpr std::array<std::uint8_t, 16> kValueExistsEntry{
        0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x01, 0xFF,
        0x10, 0xF6, 0x40, 0x20, 0x8F, 0x0F, 0x95, 0xC0};
    constexpr std::array<std::uint8_t, 16> kPresentTabEntry{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x57,
        0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00, 0x48};
    constexpr std::array<std::uint8_t, 16> kMakeIconDescriptorEntry{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x85};
    constexpr std::array<std::uint8_t, 16> kTextureLookupEntry{
        0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
        0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24};
    // Both supported builds have this exact entry. Together with the stable
    // three-argument callsite below, it validates the bool-returning image
    // binding implementation before the research detour is installed.
    constexpr std::array<std::uint8_t, 32> kImageBindEntry{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x56,
        0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48,
        0x83, 0xEC, 0x60, 0x48, 0xC7, 0x40, 0x98, 0xFE,
        0xFF, 0xFF, 0xFF, 0x48, 0x89, 0x58, 0x08, 0x48};
    constexpr std::array<std::uint8_t, 12> kImageBindArguments{
        0x4C, 0x8B, 0xC0, 0x48, 0x8D, 0x54, 0x24, 0x28,
        0x48, 0x8D, 0x4D, 0x28};
    constexpr std::array<std::uint8_t, 2> kImageBindReturnsTrue{
        0xB0, 0x01};
    constexpr std::array<std::uint8_t, 2> kImageBindReturnsFalse{
        0x32, 0xC0};
    constexpr std::array<std::uint8_t, 16> kRefreshTitleEntry{
        0x40, 0x57, 0x48, 0x81, 0xEC, 0x90, 0x00, 0x00,
        0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF};
    constexpr std::array<std::uint8_t, 16> kSwitchPanelEntry{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48};
    constexpr std::array<std::uint8_t, 49>
        kSwitchPanelStateCopyBytes{
            0x48, 0x85, 0xC0, 0x74, 0x3A,
            0x48, 0x85, 0xDB, 0x74, 0x27,
            0x48, 0x8D, 0x93, 0x38, 0x1B, 0x00, 0x00,
            0x48, 0x8D, 0x88, 0x38, 0x1B, 0x00, 0x00,
            0xE8, 0x11, 0xF9, 0xFF, 0xFF,
            0x48, 0x8B, 0x8E, 0xB8, 0x00, 0x00, 0x00,
            0x0F, 0xB6, 0x83, 0xE3, 0x1C, 0x00, 0x00,
            0x88, 0x81, 0xE3, 0x1C, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 16>
        kSwitchPanelStateCopyResumeBytes{
            0x48, 0x8B, 0x8E, 0xB8, 0x00, 0x00, 0x00, 0x33,
            0xD2, 0xE8, 0x5F, 0xA3, 0x03, 0x00, 0x33, 0xFF};
    constexpr std::array<std::uint8_t, 41>
        kSwitchPanelVisibilityBytes{
            0x48, 0x8B, 0x0B,
            0x48, 0x85, 0xC9,
            0x74, 0x16,
            0x48, 0x39, 0x8E, 0xB8, 0x00, 0x00, 0x00,
            0x0F, 0x94, 0xC2,
            0x48, 0x81, 0xC1, 0x00, 0x12, 0x00, 0x00,
            0xE8, 0xFB, 0x77, 0xDF, 0xFF,
            0xFF, 0xC7,
            0x48, 0x83, 0xC3, 0x08,
            0x83, 0xFF, 0x0A,
            0x72, 0xD7};
    constexpr std::array<std::uint8_t, 17> kConstructor271Entry{
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x48, 0x83, 0xEC,
        0x18, 0x48, 0xC7, 0x04, 0x24, 0xFE, 0xFF, 0xFF,
        0xFF};
    constexpr std::array<std::uint8_t, 16> kGetMember271Entry{
        0x48, 0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83,
        0xEC, 0x40, 0x48, 0xC7, 0x44, 0x24, 0x38, 0xFE};
    constexpr std::array<std::uint8_t, 16> kSetPosition271Entry{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x55,
        0x48, 0x8D, 0x68, 0xC8, 0x48, 0x81, 0xEC, 0x30};
    constexpr std::array<std::uint8_t, 16> kSetVisible271Entry{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x7C,
        0x24, 0x18, 0x55, 0x48, 0x8D, 0x6C, 0x24, 0xF0};
    constexpr std::array<std::uint8_t, 16> kDestructor271Entry{
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x48, 0x83,
        0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    constexpr std::array<std::uint8_t, 16> kTopConstructorEntry{
        0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41,
        0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC};
    constexpr std::array<std::uint8_t, 16> kEarlySlotGrowth271{
        0x4C, 0x8D, 0x05, 0xB2, 0x8F, 0x17, 0x02, 0x48,
        0x8D, 0x55, 0x50, 0x48, 0x8B, 0x4C, 0x24, 0x50};
    constexpr std::array<std::uint8_t, 16> kEarlySlotGrowth272{
        0x4C, 0x8D, 0x05, 0xF2, 0xAB, 0x17, 0x02, 0x48,
        0x8D, 0x55, 0x50, 0x48, 0x8B, 0x4C, 0x24, 0x50};
    constexpr std::array<std::uint8_t, 16> kFirstCountPublication{
        0x49, 0x8B, 0x45, 0x00, 0x49, 0x8B, 0xCD, 0xFF,
        0x50, 0x08, 0x8B, 0xD0, 0x48, 0x8D, 0x4B, 0x18};
    constexpr std::array<std::uint8_t, 16> kSelectorGeometryDispatch{
        0x49, 0x8B, 0x06, 0x49, 0x8B, 0xCE, 0xFF, 0x50,
        0x18, 0x0F, 0xBF, 0x9D, 0x74, 0x01, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 16> kPostGeometryObservation{
        0x0F, 0xBF, 0x9D, 0x74, 0x01, 0x00, 0x00, 0x4C,
        0x89, 0x6C, 0x24, 0x40, 0x49, 0x8D, 0x8E, 0xE0};
    constexpr std::array<std::uint8_t, 17> kCountPropagationDispatch{
        0x45, 0x33, 0xC9, 0x44, 0x8B, 0xC3, 0x49, 0x8B,
        0xD5, 0x49, 0x8B, 0xCE, 0xE8, 0xD4, 0xF7, 0xFF,
        0xFF};
    constexpr std::array<std::uint8_t, 16> kPostConstructorObservation{
        0x48, 0x8B, 0xF8, 0xEB, 0x03, 0x48, 0x8B, 0xFB,
        0xF6, 0xC3, 0x01, 0x74, 0x11, 0x83, 0xE3, 0xFE};
    constexpr std::array<std::uint8_t, 16> kCategoryAppendEntry{
        0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
        0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48};
    constexpr std::array<std::uint8_t, 7> kCategoryCountGetterEntry{
        0x8B, 0x81, 0x60, 0x05, 0x00, 0x00, 0xC3};
    constexpr std::array<std::uint8_t, 16> kSelectorCountSetterEntry{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x85};
    constexpr std::array<std::uint8_t, 20> kGeometryEnumeratorEntry{
        0x48, 0x8B, 0xC4, 0x55, 0x56, 0x57, 0x41, 0x54,
        0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D,
        0xA8, 0xE8, 0xFE, 0xFF};
#endif

    if (!g_native_tabs_build_profile) {
        log_line(
            "ERROR: no exact native-tab build profile was selected; no hooks installed");
        return false;
    }
    const std::uintptr_t destructor_rva = kScaleformDestructor271Rva;
    const std::uintptr_t expected_texture_lookup_rva =
        kTextureLookup271Rva;
    const std::uintptr_t expected_image_bind_rva = kImageBind271Rva;
    std::uintptr_t texture_lookup_rva{};
    if (!resolve_relative_call_target(
            kTextureLookupCallRva,
            texture_lookup_rva) ||
        texture_lookup_rva != expected_texture_lookup_rva) {
        log_line(
            "ERROR: texture lookup call target at RVA 0x%llX was 0x%llX; expected 0x%llX",
            static_cast<unsigned long long>(kTextureLookupCallRva),
            static_cast<unsigned long long>(texture_lookup_rva),
            static_cast<unsigned long long>(
                expected_texture_lookup_rva));
        return false;
    }
    std::uintptr_t image_bind_rva{};
    if (!resolve_relative_call_target(kImageBindCallRva, image_bind_rva) ||
        image_bind_rva != expected_image_bind_rva) {
        log_line(
            "ERROR: image bind call target at RVA 0x%llX was 0x%llX; expected 0x%llX",
            static_cast<unsigned long long>(kImageBindCallRva),
            static_cast<unsigned long long>(image_bind_rva),
            static_cast<unsigned long long>(expected_image_bind_rva));
        return false;
    }
    const bool destructor_entry_matches =
        entry_matches(destructor_rva, kDestructor271Entry);
    const bool constructor_entry_matches =
        entry_matches(kScaleformConstructor271Rva, kConstructor271Entry);
    const bool shape_helper_entries_match =
        entry_matches(kScaleformGetMember271Rva, kGetMember271Entry) &&
            entry_matches(
                kScaleformSetPosition271Rva, kSetPosition271Entry) &&
            entry_matches(
                kScaleformSetVisible271Rva, kSetVisible271Entry);
    const bool native_pointer_poll_helpers_match =
        entry_matches(
            kGetPointerPositionRva,
            kGetPointerPosition271Entry) &&
        entry_matches(kGridHitTestRva, kGridHitTest271Entry) &&
        entry_matches(
            kNativePointerSelectRva,
            kNativePointerSelect271Entry);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    const bool early_slot_growth_matches =
        tabs_build().timestamp == kNativeTabsBuildProfiles[0].timestamp
        ? entry_matches(kEarlySlotGrowthRva, kEarlySlotGrowth271)
        : entry_matches(kEarlySlotGrowthRva, kEarlySlotGrowth272);
    const bool slot_growth_interfaces_match =
        entry_matches(kTopConstructorRva, kTopConstructorEntry) &&
        early_slot_growth_matches &&
        entry_matches(
            kFirstCountPublicationRva, kFirstCountPublication) &&
        entry_matches(
            kSelectorGeometryDispatchRva, kSelectorGeometryDispatch) &&
        entry_matches(
            kPostGeometryObservationRva, kPostGeometryObservation) &&
        entry_matches(
            kCountPropagationDispatchRva, kCountPropagationDispatch) &&
        entry_matches(
            kPostConstructorObservationRva,
            kPostConstructorObservation) &&
        entry_matches(kCategoryAppendRva, kCategoryAppendEntry) &&
        entry_matches(
            kCategoryCountGetterRva, kCategoryCountGetterEntry) &&
        entry_matches(
            kSelectorSetActiveCountRva, kSelectorCountSetterEntry) &&
        entry_matches(
            kSelectorGeometryEnumeratorRva, kGeometryEnumeratorEntry);
#endif

    const bool pristine_resolve_path_entry_matches =
        entry_matches(kResolvePathRva, kResolvePathEntry);
    bool host_owned_resolve_path_bridge_matches{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // Startup preflight validates the original entry before registration is
    // opened. Production installation happens after the ordinary menu hooks,
    // whose title bridge legitimately patches this exact same function. Only
    // accept that post-preflight mutation when the menu-hook owner confirms
    // its live bridge is installed at this precise address; an unknown patch
    // must still fail closed.
    host_owned_resolve_path_bridge_matches = install_hooks &&
        erui::native::native_scaleform_path_bridge_installed_at(
            reinterpret_cast<const void*>(
                g_game.base + kResolvePathRva));
#endif
    const bool resolve_path_interface_matches =
        pristine_resolve_path_entry_matches ||
        host_owned_resolve_path_bridge_matches;

    const bool pristine_selector_select_entry_matches =
        entry_matches(kSelectorSelectRva, kSelectorSelectEntry);
    bool approved_selector_select_entry_matches{};
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    if (install_hooks && preserved_shared) {
        erui::native::ModuleView live_game{};
        approved_selector_select_entry_matches =
            live_game.initialize(
                reinterpret_cast<HMODULE>(g_game.base)) &&
            erui::native::revalidate_preserved_shared_menu_entry(
                live_game,
                *preserved_shared,
                erui::native::PreservedSharedMenuEntry::selector_select);
    }
#endif
    const bool selector_select_interface_matches =
        pristine_selector_select_entry_matches ||
        approved_selector_select_entry_matches;

    if (!native_pointer_poll_helpers_match) {
        log_line(
            "ERROR: native pointer polling helpers differ from the validated Elden Ring %s executable; no hooks installed",
            tabs_build().version);
        return false;
    }

    if (!entry_matches(kSelectionCallbackRva, kSelectionEntry) ||
        !entry_matches(kCompositeFrameRva, kCompositeFrameEntry) ||
        !selector_select_interface_matches ||
        !entry_matches(
            kInputDispatchFrameBeginRva,
            kInputDispatchFrameBeginBytes) ||
        !entry_matches(
            kPointerDispatchResultRva,
            kPointerDispatchResultBytes) ||
        !entry_matches(
            kHorizontalNavigationCommitRva,
            kHorizontalNavigationCommitBytes) ||
        !entry_matches(
            kSelectionDispatchRva,
            kSelectionDispatchBytes) ||
        !entry_matches(
            kPointerCellSampleRva,
            kPointerCellSampleBytes) ||
        !entry_matches(kSwitchPanelRva, kSwitchPanelEntry) ||
        !entry_matches(
            tabs_build().switch_panel_state_copy_signature,
            kSwitchPanelStateCopyBytes) ||
        !entry_matches(
            kSwitchPanelStateCopyResumeRva,
            kSwitchPanelStateCopyResumeBytes) ||
        !entry_matches(
            tabs_build().switch_panel_visibility_signature,
            kSwitchPanelVisibilityBytes) ||
        !resolve_path_interface_matches ||
        !entry_matches(kValueExistsRva, kValueExistsEntry) ||
        !entry_matches(kPresentTabRva, kPresentTabEntry) ||
        !entry_matches(
            kMakeIconDescriptorRva,
            kMakeIconDescriptorEntry) ||
        !entry_matches(texture_lookup_rva, kTextureLookupEntry) ||
        !entry_matches(image_bind_rva, kImageBindEntry) ||
        !entry_matches(
            kImageBindCallRva - kImageBindArguments.size(),
            kImageBindArguments) ||
        !entry_matches(
            image_bind_rva + 0x282,
            kImageBindReturnsTrue) ||
        !entry_matches(
            image_bind_rva + 0x293,
            kImageBindReturnsFalse) ||
        !entry_matches(kRefreshTitleRva, kRefreshTitleEntry) ||
        !constructor_entry_matches ||
        !shape_helper_entries_match ||
        !destructor_entry_matches
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
        || !slot_growth_interfaces_match
#endif
        ) {
        log_line(
            "ERROR: one or more viewport interfaces differ from the validated executable; no hooks installed (resolvePath={pristine:%u hostOwned:%u accepted:%u} selectorSelect={pristine:%u solidApproved:%u accepted:%u})",
            pristine_resolve_path_entry_matches ? 1U : 0U,
            host_owned_resolve_path_bridge_matches ? 1U : 0U,
            resolve_path_interface_matches ? 1U : 0U,
            pristine_selector_select_entry_matches ? 1U : 0U,
            approved_selector_select_entry_matches ? 1U : 0U,
            selector_select_interface_matches ? 1U : 0U);
        return false;
    }

    g_selector_select = reinterpret_cast<SelectorSelectFn>(
        g_game.base + kSelectorSelectRva);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    g_selector_set_active_count =
        reinterpret_cast<SelectorSetActiveCountFn>(
            g_game.base + kSelectorSetActiveCountRva);
#endif
    g_get_pointer_position = native_pointer_poll_helpers_match
        ? reinterpret_cast<GetPointerPositionFn>(
              g_game.base + kGetPointerPositionRva)
        : nullptr;
    g_grid_hit_test = native_pointer_poll_helpers_match
        ? reinterpret_cast<GridHitTestFn>(g_game.base + kGridHitTestRva)
        : nullptr;
    g_native_pointer_select = native_pointer_poll_helpers_match
        ? reinterpret_cast<NativePointerSelectFn>(
              g_game.base + kNativePointerSelectRva)
        : nullptr;
    g_switch_panel = reinterpret_cast<SwitchPanelFn>(
        g_game.base + kSwitchPanelRva);
    g_refresh_title = reinterpret_cast<RefreshTitleFn>(
        g_game.base + kRefreshTitleRva);
    g_resolve_path = reinterpret_cast<ResolvePathFn>(
        g_game.base + kResolvePathRva);
    g_value_exists = reinterpret_cast<ValueExistsFn>(
        g_game.base + kValueExistsRva);
    g_present_tab = reinterpret_cast<PresentTabFn>(
        g_game.base + kPresentTabRva);
    g_make_icon_descriptor = reinterpret_cast<MakeIconDescriptorFn>(
        g_game.base + kMakeIconDescriptorRva);
    g_construct_scaleform_value = reinterpret_cast<ScaleformConstructorFn>(
        g_game.base + kScaleformConstructor271Rva);
    g_get_scaleform_member = reinterpret_cast<ScaleformGetMemberFn>(
        g_game.base + kScaleformGetMember271Rva);
    g_set_scaleform_position = reinterpret_cast<ScaleformSetPositionFn>(
        g_game.base + kScaleformSetPosition271Rva);
    g_set_scaleform_visible = reinterpret_cast<ScaleformSetVisibleFn>(
        g_game.base + kScaleformSetVisible271Rva);
    g_destroy_scaleform_value = reinterpret_cast<ScaleformDestructorFn>(
        g_game.base + destructor_rva);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    g_category_append = reinterpret_cast<CategoryAppendFn>(
        g_game.base + kCategoryAppendRva);
#endif

    if (!install_hooks) return true;

    auto state_copy_hook = SafetyHookMid::create(
        g_game.base + kSwitchPanelStateCopyRva,
        &isolate_panel_state_copy,
        SafetyHookMid::StartDisabled);
    if (!state_copy_hook) {
        log_line(
            "ERROR: could not prepare panel-state isolation hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(kSwitchPanelStateCopyRva),
            static_cast<unsigned>(state_copy_hook.error().type));
        return false;
    }
    g_panel_state_copy_hook = std::move(*state_copy_hook);

    auto frame_begin_source_hook = SafetyHookMid::create(
        g_game.base + kInputDispatchFrameBeginRva,
        &observe_input_dispatch_frame_begin,
        SafetyHookMid::StartDisabled);
    if (!frame_begin_source_hook) {
        log_line(
            "ERROR: could not prepare input-dispatch frame-begin hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(
                kInputDispatchFrameBeginRva),
            static_cast<unsigned>(frame_begin_source_hook.error().type));
        reset_hooks();
        return false;
    }
    g_input_dispatch_frame_begin_hook =
        std::move(*frame_begin_source_hook);

    auto pointer_source_hook = SafetyHookMid::create(
        g_game.base + kPointerDispatchResultRva,
        &observe_pointer_dispatch_result,
        SafetyHookMid::StartDisabled);
    if (!pointer_source_hook) {
        log_line(
            "ERROR: could not prepare pointer-source hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(
                kPointerDispatchResultRva),
            static_cast<unsigned>(pointer_source_hook.error().type));
        reset_hooks();
        return false;
    }
    g_pointer_dispatch_source_hook = std::move(*pointer_source_hook);

    auto horizontal_navigation_source_hook = SafetyHookMid::create(
        g_game.base + kHorizontalNavigationCommitRva,
        &observe_horizontal_navigation_commit,
        SafetyHookMid::StartDisabled);
    if (!horizontal_navigation_source_hook) {
        log_line(
            "ERROR: could not prepare horizontal-navigation source hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(
                kHorizontalNavigationCommitRva),
            static_cast<unsigned>(
                horizontal_navigation_source_hook.error().type));
        reset_hooks();
        return false;
    }
    g_horizontal_navigation_source_hook =
        std::move(*horizontal_navigation_source_hook);

    auto selection_dispatch_hook = SafetyHookMid::create(
        g_game.base + kSelectionDispatchRva,
        &dispatch_due_pointer_dwell,
        SafetyHookMid::StartDisabled);
    if (!selection_dispatch_hook) {
        log_line(
            "ERROR: could not prepare selection-dispatch hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(kSelectionDispatchRva),
            static_cast<unsigned>(selection_dispatch_hook.error().type));
        reset_hooks();
        return false;
    }
    g_selection_dispatch_hook = std::move(*selection_dispatch_hook);

    auto pointer_cell_sample_hook = SafetyHookMid::create(
        g_game.base + kPointerCellSampleRva,
        &observe_pointer_cell_sample,
        SafetyHookMid::StartDisabled);
    if (!pointer_cell_sample_hook) {
        log_line(
            "ERROR: could not prepare pointer-cell sample hook at RVA 0x%llX (SafetyHook error type %u)",
            static_cast<unsigned long long>(kPointerCellSampleRva),
            static_cast<unsigned>(
                pointer_cell_sample_hook.error().type));
        reset_hooks();
        return false;
    }
    g_pointer_cell_sample_hook =
        std::move(*pointer_cell_sample_hook);

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    auto slot_growth_early_hook = SafetyHookMid::create(
        g_game.base + kEarlySlotGrowthRva,
        &append_graphics_records_at_construction_seam,
        SafetyHookMid::StartDisabled);
    if (!slot_growth_early_hook) {
        log_line(
            "ERROR: could not prepare physical-tab growth hook (SafetyHook error type %u)",
            static_cast<unsigned>(slot_growth_early_hook.error().type));
        reset_hooks();
        return false;
    }
    auto slot_growth_late_hook = SafetyHookMid::create(
        g_game.base + kPostGeometryObservationRva,
        &observe_slot_growth_post_geometry,
        SafetyHookMid::StartDisabled);
    if (!slot_growth_late_hook) {
        log_line(
            "ERROR: could not prepare physical-tab post-geometry proof hook (SafetyHook error type %u)",
            static_cast<unsigned>(slot_growth_late_hook.error().type));
        reset_hooks();
        return false;
    }
    auto slot_growth_post_constructor_hook = SafetyHookMid::create(
        g_game.base + kPostConstructorObservationRva,
        &observe_slot_growth_post_constructor,
        SafetyHookMid::StartDisabled);
    if (!slot_growth_post_constructor_hook) {
        log_line(
            "ERROR: could not prepare physical-tab post-constructor proof hook (SafetyHook error type %u)",
            static_cast<unsigned>(
                slot_growth_post_constructor_hook.error().type));
        reset_hooks();
        return false;
    }
    g_slot_growth_early_hook = std::move(*slot_growth_early_hook);
    g_slot_growth_late_hook = std::move(*slot_growth_late_hook);
    g_slot_growth_post_constructor_hook =
        std::move(*slot_growth_post_constructor_hook);
#endif

    if (!prepare_hook(
            kSwitchPanelRva,
            reinterpret_cast<void*>(&research_switch_panel_detour),
            g_research_switch_hook,
            g_original_research_switch,
            "reusable carrier switch") ||
        !prepare_hook(
            texture_lookup_rva,
            reinterpret_cast<void*>(&texture_lookup_detour),
            g_texture_lookup_hook,
            g_original_texture_lookup,
            "Scaleform texture lookup") ||
        !prepare_hook(
            image_bind_rva,
            reinterpret_cast<void*>(&image_bind_detour),
            g_image_bind_hook,
            g_original_image_bind,
            "Scaleform image bind") ||
        !prepare_hook(
            kSelectionCallbackRva,
            reinterpret_cast<void*>(&selection_callback_detour),
            g_selection_hook,
            g_original_selection,
            "tab selection callback") ||
        !prepare_hook(
            kCompositeFrameRva,
            reinterpret_cast<void*>(&composite_frame_detour),
            g_composite_frame_hook,
            g_original_composite_frame,
            "composite panel frame")) {
        reset_hooks();
        return false;
    }
    // Arm panel-state isolation before a viewport callback can route category
    // 4, then arm the image observer before resource aliasing can publish a
    // marker.
    // From the first enable attempt onward the hook objects and catalogue are
    // process-lifetime. Until every target is enabled, callbacks are gated to
    // exact native pass-through/no-op behavior.
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // Hook preparation can allocate and suspend threads. Close that timing
    // window with one last exact selector-prefix and indirect-target check
    // while every prepared hook is still disabled and locally reversible.
    bool selector_still_approved =
        entry_matches(kSelectorSelectRva, kSelectorSelectEntry);
    if (preserved_shared && !selector_still_approved) {
        erui::native::ModuleView live_game{};
        selector_still_approved = live_game.initialize(
            reinterpret_cast<HMODULE>(g_game.base)) &&
            erui::native::revalidate_preserved_shared_menu_entry(
                live_game,
                *preserved_shared,
                erui::native::PreservedSharedMenuEntry::selector_select);
    }
    if (!selector_still_approved) {
        log_line(
            "ERROR: Selector::select changed after interface validation and before viewport hook exposure; no hooks installed");
        reset_hooks();
        return false;
    }
#endif
    g_viewport_callbacks_armed.store(false, std::memory_order_release);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    g_slot_growth_callbacks_armed.store(false, std::memory_order_release);
#endif
    g_viewport_hooks_exposed.store(true, std::memory_order_release);

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    // Publish the post-constructor observer first. Until all three hooks are
    // visible and the callback gate is armed, every callback is a native
    // no-op. This ordering ensures an attempted append can never escape
    // without the final ownership/count proof.
    if (!enable_hook(
            g_slot_growth_post_constructor_hook,
            "physical-tab post-constructor proof") ||
        !enable_hook(
            g_slot_growth_late_hook,
            "physical-tab post-geometry proof") ||
        !enable_hook(
            g_slot_growth_early_hook,
            "physical-tab growth")) {
        return false;
    }
#endif

    const auto state_copy_enabled = g_panel_state_copy_hook.enable();
    if (!state_copy_enabled) {
        log_line(
            "ERROR: could not enable panel-state isolation hook (SafetyHook error type %u)",
            static_cast<unsigned>(state_copy_enabled.error().type));
        return false;
    }
    if (!enable_hook(
            g_input_dispatch_frame_begin_hook,
            "tab input-dispatch frame-begin") ||
        !enable_hook(
            g_pointer_dispatch_source_hook,
            "tab pointer-source") ||
        !enable_hook(
            g_horizontal_navigation_source_hook,
            "tab horizontal-navigation-source") ||
        !enable_hook(
            g_selection_dispatch_hook,
            "tab selection-dispatch") ||
        !enable_hook(
            g_pointer_cell_sample_hook,
            "tab pointer-cell sample") ||
        !enable_hook(g_research_switch_hook, "reusable carrier switch") ||
        !enable_hook(g_image_bind_hook, "Scaleform image bind") ||
        !enable_hook(g_texture_lookup_hook, "Scaleform texture lookup") ||
        !enable_hook(g_selection_hook, "tab selection callback") ||
        !enable_hook(g_composite_frame_hook, "composite panel frame")) {
        return false;
    }

    // Make the complete viewport operational before native record growth can
    // run. A Configuration construction racing this publication may safely
    // retain the stock eight records, but it can never expose appended records
    // to an unarmed viewport.
    g_viewport_callbacks_armed.store(true, std::memory_order_release);
#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
    g_slot_growth_callbacks_armed.store(true, std::memory_order_release);
#endif

    log_line(
        "STRESS startup hooks installed: protocol=GraphicsCarrierV2 selection=eldenring.exe+0x%llX source={frame:+0x%llX pointer:+0x%llX horizontal:+0x%llX dispatch:+0x%llX cellSample:+0x%llX} nativePoll={status:%s position:+0x%llX hit:+0x%llX select:+0x%llX} compositeFrame=+0x%llX panelStateIsolation=+0x%llX->+0x%llX rowsCarrier=category8/selector11 parking=category5 textureLookup=+0x%llX imageBind=+0x%llX cross=%ls overflow=%ls alias=%ls; logical=%zu physical=%zu margin=%zu dwell={initial:%llums repeat:%llums decay:%llums minimum:%llums} imageProbe={nativeResource:dynamic-shape-rgba8 rawImageLifecycle:%s nativeResourceLifecycle:%s bitmapDataOwnership:%s pixelMutation:%s}",
        static_cast<unsigned long long>(kSelectionCallbackRva),
        static_cast<unsigned long long>(kInputDispatchFrameBeginRva),
        static_cast<unsigned long long>(kPointerDispatchResultRva),
        static_cast<unsigned long long>(
            kHorizontalNavigationCommitRva),
        static_cast<unsigned long long>(kSelectionDispatchRva),
        static_cast<unsigned long long>(kPointerCellSampleRva),
        native_pointer_poll_helpers_match ? "enabled" : "unavailable",
        static_cast<unsigned long long>(kGetPointerPositionRva),
        static_cast<unsigned long long>(kGridHitTestRva),
        static_cast<unsigned long long>(kNativePointerSelectRva),
        static_cast<unsigned long long>(kCompositeFrameRva),
        static_cast<unsigned long long>(kSwitchPanelStateCopyRva),
        static_cast<unsigned long long>(
            kSwitchPanelStateCopyResumeRva),
        static_cast<unsigned long long>(texture_lookup_rva),
        static_cast<unsigned long long>(image_bind_rva),
        kCrossIconResource,
        kOverflowIconResource,
        kAliasedNativeIconResource,
        active_logical_tab_count(),
        active_physical_tab_count(),
        g_navigation_margin,
        static_cast<unsigned long long>(
            g_pointer_dwell_timing.initial_delay_ms),
        static_cast<unsigned long long>(
            g_pointer_dwell_timing.repeat_delay_ms),
        static_cast<unsigned long long>(
            g_pointer_dwell_timing.repeat_decay_ms),
        static_cast<unsigned long long>(
            g_pointer_dwell_timing.minimum_repeat_delay_ms),
        kEnableRawImageLifecycleProbe ? "enabled" : "disabled",
        kEnableNativeImageResourceLifecycleProbe ? "enabled" : "disabled",
        kEnableBitmapDataOwnershipProbe ? "enabled" : "disabled",
        kEnableBitmapDataPixelMutationProbe ? "enabled" : "disabled");
    return true;
}

#if !defined(ER_NATIVE_UI_PRODUCTION_TABS)
DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    const std::uint64_t bootstrap_started_qpc = query_performance_ticks();
    log_line("STRESS ============================================================");
    log_line("STRESS startup configuration-tab viewport mutation probe");

    HMODULE pinned{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(g_module),
            &pinned)) {
        log_line("ERROR: probe DLL could not be pinned; no hooks installed");
        return 1;
    }

    if (!inspect_game_image(g_game)) {
        log_line("ERROR: could not inspect the main executable image");
        return 1;
    }
    log_line(
        "STRESS startup gameImage base=%p timestamp=0x%08X size=0x%zX",
        static_cast<void*>(g_game.base),
        static_cast<unsigned>(g_game.timestamp),
        g_game.size);
    g_native_tabs_build_profile = find_native_tabs_build_profile(
        g_game.timestamp, g_game.size);
    if (!g_native_tabs_build_profile) {
        log_line(
            "ERROR: unsupported Elden Ring executable identity; expected an exact 2.7.1.0 or 2.7.2.0 image");
        return 1;
    }
    if (!validate_official_host()) return 1;
    const bool installed = install_hooks_and_interfaces();
    log_line(
        "STRESS bootstrap complete=%u timingUs=%llu scope=game-validation-plus-hook-install",
        installed ? 1U : 0U,
        static_cast<unsigned long long>(elapsed_qpc_microseconds(
            bootstrap_started_qpc, query_performance_ticks())));
    return installed ? 0 : 1;
}
#endif

} // namespace

#if defined(ER_NATIVE_UI_PRODUCTION_TABS)
namespace erui::native {

NativeTabsPreflightStatus preflight_native_tabs() noexcept {
    g_native_tabs_build_profile = nullptr;
    GameImage image{};
    if (!inspect_game_image(image)) {
        return NativeTabsPreflightStatus::interface_mismatch;
    }
    const NativeTabsBuildProfile* const profile =
        find_native_tabs_build_profile(image.timestamp, image.size);
    if (!profile) {
        return NativeTabsPreflightStatus::unsupported_game;
    }

    g_native_tabs_build_profile = profile;
    g_game = image;
    GameAddresses carrier_addresses{};
    carrier_addresses.game_image_base = image.base;
    carrier_addresses.game_image_size = image.size;
    carrier_addresses.graphics_panel_materializer =
        reinterpret_cast<BuiltinPanelMaterializerFn>(
            image.base + profile->graphics_panel_materializer);
    if (!preflight_native_tab_carrier(carrier_addresses) ||
        !validate_official_host() ||
        !install_hooks_and_interfaces(false)) {
        return NativeTabsPreflightStatus::interface_mismatch;
    }
    return NativeTabsPreflightStatus::available;
}

bool install_native_tabs(
    const detail::NativeTabCatalogue& catalogue,
    const PreservedSharedMenuAddresses* preserved_shared) noexcept {
    if (g_native_tabs_installed.load(std::memory_order_acquire) ||
        catalogue.custom_count() == 0 ||
        catalogue.custom_count() > ERUI_CUSTOM_TAB_MAX_COUNT ||
        catalogue.entries().size() !=
            kStockLogicalTabCount + catalogue.custom_count()) {
        return false;
    }
    // The immutable-image preflight ran before provider registration opened.
    // At this point install_native_menu_hooks() has deliberately patched the
    // carrier entry points, so comparing them with the original executable
    // bytes again would reject our own healthy hooks. Validate the installed
    // carrier state instead; install_hooks_and_interfaces(true) below still
    // validates every viewport-specific executable seam before patching it.
    if (!native_tab_graphics_carrier_installed() ||
        !validate_official_host()) {
        detail::logf(
            LogLevel::error,
            "NativeTabs requires the already-installed validated Graphics carrier");
        return false;
    }

    detail::RuntimeState& host_runtime = detail::runtime_state();
    if (!host_runtime.menu) return false;
    const auto custom = catalogue.custom_entries();
    const std::shared_ptr<const detail::TabRuntimeGeneration>
        tab_generation = detail::acquire_applied_tab_generation();
    if (!tab_generation ||
        tab_generation->entries.size() != custom.size()) {
        detail::logf(
            LogLevel::error,
            "NativeTabs rejected an unavailable or mismatched runtime tab generation");
        return false;
    }
    const std::size_t configured_minimum =
        catalogue.navigation_settings().minimum_tab_slots;
    if (configured_minimum < kStockLogicalTabCount ||
        configured_minimum > kPhysicalTabCapacity) {
        detail::logf(
            LogLevel::error,
            "NativeTabs rejected MinimumTabSlots=%zu; native physical capacity is %zu..%zu",
            configured_minimum,
            kStockLogicalTabCount,
            kPhysicalTabCapacity);
        return false;
    }
    std::unique_ptr<detail::NativeTabRuntimeState> runtime_candidate =
        detail::NativeTabRuntimeState::create(custom.size());
    if (!runtime_candidate) {
        detail::logf(
            LogLevel::error,
            "NativeTabs could not allocate %zu exact-sized runtime entries",
            custom.size());
        return false;
    }
    std::unique_ptr<PinnedLogicalTabCatalogue> logical_candidate{};
    try {
        logical_candidate = std::make_unique<PinnedLogicalTabCatalogue>();
        logical_candidate->tabs.resize(catalogue.entries().size());
        g_rgba8_expected_checksums_scratch.assign(custom.size(), 0U);
        g_rgba8_staged_resources_scratch.assign(
            custom.size(), RetainedRgba8Resource{});
        g_rgba8_staged_changed_scratch.assign(custom.size(), 0U);
        g_rgba8_retired_resources_scratch.assign(
            custom.size(), RetainedRgba8Resource{});
        g_rgba8_retired_debts_scratch.assign(custom.size(), 0U);
    } catch (...) {
        detail::logf(
            LogLevel::error,
            "NativeTabs could not allocate the exact-sized logical catalogue and icon-generation scratch");
        return false;
    }
    const auto entries = catalogue.entries();
    for (std::size_t logical = 0; logical < entries.size(); ++logical) {
        logical_candidate->tabs[logical].source = &entries[logical];
    }
    for (std::size_t index = 0; index < custom.size(); ++index) {
        const detail::TabRuntimeEntry& runtime_source =
            tab_generation->entries[index];
        const wchar_t* const title = runtime_source.title.c_str();
        if (!title || title[0] == L'\0' ||
            wcsnlen_s(title, kTitleCapacity) >= kTitleCapacity) {
            detail::logf(
                LogLevel::error,
                "NativeTabs rejected custom tab %zu: title is unavailable or exceeds %zu UTF-16 code units",
                index,
                kTitleCapacity - 1U);
            return false;
        }
        LogicalTab& logical =
            logical_candidate->tabs[kStockLogicalTabCount + index];
        if (!copy_title(title, logical.title)) return false;
        logical.category = 8;
        logical.custom_runtime = runtime_candidate->tab(index);
        if (!logical.custom_runtime) return false;
        auto& icon_runtime = logical.custom_runtime->icon();
        icon_runtime.resource_id = kFirstRgba8IconId +
            static_cast<std::int32_t>(index);
        make_fixed_decimal_name(
            icon_runtime.resource_name,
            L"MENU_ItemIcon_",
            static_cast<std::uint32_t>(icon_runtime.resource_id),
            5U);
        make_fixed_decimal_name(
            icon_runtime.raw_resource_name,
            L"ERNativeUI.TabProbe.RawRGBA8.",
            static_cast<std::uint32_t>(index + 1U),
            4U);
        const auto& icon = runtime_source.icon;
        if (icon.source == TabIconSource::fallback) {
            if (!icon.rgba8_pixels.empty()) return false;
            icon_runtime.raster_requested =
                erui::native::custom_tab_icon_requests_raster(icon.source);
            continue;
        }
        if (icon.source != TabIconSource::rgba8 ||
            icon.rgba8_pixels.size() !=
                ERUI_TAB_ICON_RGBA8_BYTE_COUNT) {
            return false;
        }
        icon_runtime.raster_requested =
            erui::native::custom_tab_icon_requests_raster(icon.source);
    }
    g_native_tab_catalogue = &catalogue;
    g_native_tab_runtime = std::move(runtime_candidate);
    g_logical_tab_catalogue = std::move(logical_candidate);
    g_tab_projection_generation = tab_generation;
    // The configuration catalogue is immutable after registration freezes.
    // Constructor callbacks read only this process-lifetime scalar floor;
    // the exact live movie may safely raise the constructor-local target to a
    // larger compatible strip, up to the hard native capacity of ten.
    g_minimum_tab_slots = configured_minimum;
    g_navigation_margin = catalogue.navigation_settings().navigation_margin;
    g_mouse_dwell_enabled =
        catalogue.navigation_settings().mouse_dwell_enabled;
    g_pointer_dwell_timing =
        catalogue.navigation_settings().mouse_dwell_timing;
    if (!install_hooks_and_interfaces(true, preserved_shared)) {
        if (g_viewport_hooks_exposed.load(std::memory_order_acquire)) {
            // Once the first enable was attempted, an enabled/in-flight
            // trampoline may exist even when SafetyHook reported failure.
            // Publish the lifetime gate so fail_install() retains the complete
            // pinned runtime and the unarmed callbacks remain native no-ops.
            g_native_tabs_installed.store(true, std::memory_order_release);
            detail::logf(
                LogLevel::error,
                "NativeTabs hook installation failed after viewport-hook exposure; the partial unarmed pinned runtime is retained for process safety");
            return false;
        }
        // Hook creation/validation failed before any target was exposed, so
        // ordinary local rollback is still race-free.
        reset_hooks();
        reset_runtime();
        g_native_tab_catalogue = nullptr;
        g_native_tab_runtime.reset();
        g_logical_tab_catalogue.reset();
        g_tab_projection_generation.reset();
        g_rgba8_expected_checksums_scratch.clear();
        g_rgba8_staged_resources_scratch.clear();
        g_rgba8_staged_changed_scratch.clear();
        g_rgba8_retired_resources_scratch.clear();
        g_rgba8_retired_debts_scratch.clear();
        g_rgba8_staged_tab_generation.reset();
        g_rgba8_staged_bank_ready = false;
        g_rgba8_retired_bank_active = false;
        g_rgba8_staging_failed_epoch = 0U;
        g_rgba8_retirement_wait_logged_epoch = 0U;
        g_rgba8_retirement_wait_logged_revision = 0U;
        return false;
    }
    g_native_tabs_installed.store(true, std::memory_order_release);
    detail::logf(
        LogLevel::info,
        "NativeTabs installed: logical=%zu custom=%zu minimumPhysicalSlots=%zu hardPhysicalCapacity=%zu margin=%zu mouseDwell=%u",
        active_logical_tab_count(),
        active_custom_tab_count(),
        g_minimum_tab_slots,
        kPhysicalTabCapacity,
        g_navigation_margin,
        g_mouse_dwell_enabled ? 1U : 0U);
    return true;
}

bool remove_native_tabs() noexcept {
    if (!native_tab_graphics_carrier_exposed() &&
        !g_viewport_hooks_exposed.load(std::memory_order_acquire) &&
        !g_native_tabs_installed.load(std::memory_order_acquire)) {
        return true;
    }
    // There is deliberately no load/exchange teardown window here. A game
    // callback can enter immediately after the hooks are enabled and does not
    // take a host-side uninstall lock. Refuse the whole runtime uninstall so
    // catalogue storage, Shape resources, and hook trampolines remain valid.
    detail::logf(
        LogLevel::warning,
        "NativeTabs hot removal was refused after carrier/viewport hook exposure; the pinned API 1.3 runtime and catalogue remain process-lifetime");
    return false;
}

bool native_tabs_installed() noexcept {
    return g_native_tabs_installed.load(std::memory_order_acquire);
}

} // namespace erui::native
#else
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, &run_probe, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
#endif
