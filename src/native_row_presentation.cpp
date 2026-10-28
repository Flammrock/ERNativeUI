#include "native_row_presentation.hpp"

#include "addresses.hpp"
#include "game_build.hpp"
#include "game_build_profiles.hpp"
#include "module.hpp"
#include "native_slider_presentation.hpp"
#include "native_title_bridge.hpp"
#include "row_presentation_state.hpp"
#include "runtime_log.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace erui::native {
namespace {

constexpr std::size_t kPropertyCollectionOffset = 0x1268;
constexpr std::size_t kPropertyRowStride = 0x88;
constexpr std::size_t kPropertyRowHelpDirectOffset = 0x40;
constexpr std::size_t kPropertyRowOwnedHelpOffset = 0x48;
constexpr std::size_t kPropertyControllerOffset = 0x78;
constexpr std::size_t kPropertyEnabledOffset = 0x80;
constexpr std::size_t kPropertyCountOffset = 0x1AF0;
constexpr std::size_t kSelectedRowCountOffset = 0xB08;
constexpr std::size_t kSelectedRowIndexOffset = 0xB0C;
constexpr std::size_t kMaximumPropertyRows = 16;
constexpr std::size_t kControllerMovieContextOffset = 0x10;
constexpr std::size_t kControllerCaptionAccessorSlot = 6;
constexpr std::size_t kEmbeddedSliderOffset = 0xB0;
constexpr std::size_t kSliderMovieProxyOffset = 0xA38;
constexpr std::size_t kSliderCurrentValueOffset = 0xD28;
constexpr std::size_t kSliderBoundValueOffset = 0xD90;
constexpr std::size_t kSliderRefreshVtableSlot = 13;
constexpr std::size_t kSceneProxyFirstSelfOffset = 0x08;
constexpr std::size_t kSceneProxySecondSelfOffset = 0x10;
constexpr std::size_t kSceneProxyThirdSelfOffset = 0x18;
constexpr std::size_t kSceneProxyOwnerOffset = 0x20;
constexpr std::size_t kSceneProxyValueOffset = 0x28;
constexpr std::size_t kScaleformValueFlagsOffset = 0x20;
constexpr std::size_t kScaleformValuePayloadOffset = 0x28;
constexpr std::size_t kScaleformValueInterfaceOffset = 0x18;
constexpr std::uint32_t kScaleformValueKindMask = 0x8F;
constexpr std::uint32_t kScaleformValueKindStringW = 0x07;
constexpr std::uint32_t kScaleformValueKindDisplayObject = 0x0A;
constexpr std::uint32_t kScaleformConvertStringW = 0x87;
constexpr std::uint32_t kScaleformStringW = 0x47;
constexpr std::size_t kScaleformMemberResultSize = 0x38;
constexpr std::size_t kMaximumSyncedValueChars = 64;
constexpr std::size_t kMaximumTrackedPages = 64;
constexpr std::size_t kSliderTransformRegistryCapacity =
    kMaximumTrackedPages * kMaximumPropertyRows;
constexpr std::size_t kInvalidSliderTransformSlot =
    static_cast<std::size_t>(-1);
constexpr std::uint32_t kFaultLogLimit = 16;
constexpr char kOrdinaryRowLabelPath[] = "Caption/Text_0";
constexpr char kFamilyARowLabelPath[] = "Text_1/Text";
constexpr char kFamilyAControlCaptionPath[] = "Text_0/Text";
constexpr char kFamilyBRowLabelPath[] = "Text_0";
constexpr char kFamilyBControlCaptionPath[] = "Text_1/Text";
constexpr char kSliderValueTextPath[] = "Text_0";
constexpr char kToggleNormalItemPath[] = "Item_0_0";
constexpr char kToggleDisabledItemPath[] = "GrayoutItem";
constexpr char kToggleNormalTextPath[] = "Item_0_0/Text_0";
constexpr char kToggleDisabledTextPath[] = "GrayoutItem/Text_0";
constexpr char kTextMemberName[] = "text";

constexpr std::array<std::uint8_t, 35> kGridPredicateBytes{
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x49, 0x08, 0x8B,
    0x12, 0x48, 0x81, 0xC1, 0x60, 0x12, 0x00, 0x00, 0x48,
    0x8B, 0x01, 0xFF, 0x50, 0x28, 0x0F, 0xB6, 0x80, 0x80,
    0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3,
};

constexpr std::array<std::uint8_t, 27> kVisibilitySetterBytes{
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x01,
    0x0F, 0xB6, 0xDA, 0xFF, 0x50, 0x08, 0x8B, 0x48, 0x20,
    0x81, 0xE1, 0x8F, 0x00, 0x00, 0x00, 0x83, 0xF9, 0x02,
};

constexpr std::array<std::uint8_t, 32> kScaleformTextSetterBytes{
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
    0x09, 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0x01, 0xFF,
    0x50, 0x08, 0x8B, 0x48, 0x20, 0x81, 0xE1, 0x8F,
    0x00, 0x00, 0x00, 0x83, 0xF9, 0x02, 0x72, 0x1B,
};

constexpr std::array<std::uint8_t, 17> kScaleformValueConstructorBytes{
    0x48, 0x89, 0x4C, 0x24, 0x08, 0x48, 0x83, 0xEC, 0x18,
    0x48, 0xC7, 0x04, 0x24, 0xFE, 0xFF, 0xFF, 0xFF,
};

constexpr std::array<std::uint8_t, 16> kScaleformValueDestructorBytes{
    0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x48, 0x83,
    0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE,
};

constexpr std::array<std::uint8_t, 16> kAs3GetMemberBytes{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74,
    0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20, 0x55,
};

constexpr std::array<std::uint8_t, 24> kColorTransformGetterBytes{
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
    0x4A, 0x28, 0x49, 0x8B, 0xD8, 0x8B, 0x81, 0x90,
    0x00, 0x00, 0x00, 0x83, 0xE8, 0x18, 0x83, 0xF8,
};

constexpr std::array<std::uint8_t, 20> kColorTransformSetterBytes{
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x4A, 0x28,
    0x8B, 0x81, 0x90, 0x00, 0x00, 0x00, 0x83, 0xE8,
    0x18, 0x83, 0xF8, 0x0C,
};

using ScaleformVisibilitySetterFn = void(__fastcall*)(
    void* scene_obj_proxy,
    std::uint8_t visible);
using ScaleformValueConstructorFn = void*(__fastcall*)(void* destination);
using As3GetMemberFn = bool(__fastcall*)(
    void* value_interface,
    void* value_payload,
    const char* member_name,
    void* destination);
using ScaleformColorTransformGetterFn = bool(__fastcall*)(
    void* value_interface,
    void* value_payload,
    float* destination);
using ScaleformColorTransformSetterFn = bool(__fastcall*)(
    void* value_interface,
    void* value_payload,
    const float* source);

struct RowAvailabilityProfile {
    EldenRingBuild build{};
    std::size_t image_size{};
    std::uintptr_t grid_predicate_rva{};
    std::uintptr_t visibility_setter_rva{};
    std::uintptr_t text_setter_rva{};
    std::uintptr_t action_controller_vtable_rva{};
    std::uintptr_t toggle_controller_vtable_rva{};
    std::uintptr_t slider_controller_vtable_rva{};
    std::uintptr_t inline_choice_controller_vtable_rva{};
    std::uintptr_t text_input_controller_vtable_rva{};
    std::uintptr_t value_constructor_rva{};
    std::uintptr_t value_destructor_rva{};
    std::uintptr_t scene_proxy_vtable_rva{};
    std::uintptr_t scene_proxy_getter_rva{};
    std::uintptr_t scene_value_vtable_rva{};
    std::uintptr_t as3_interface_vtable_rva{};
    std::uintptr_t as3_get_member_rva{};
    std::uintptr_t color_transform_getter_rva{};
    std::uintptr_t color_transform_setter_rva{};
};

constexpr std::array kRowAvailabilityProfiles{
    RowAvailabilityProfile{
        EldenRingBuild::v2_7_1_0,
        0x5E0DA00,
        0x977680,
        0x734190,
        0x74AE50,
        0x2AD7430,
        0x2B14DA8,
        0x2B14D68,
        0x2B14DE8,
        0x2B1B128,
        0xD814A0,
        0xD81600,
        0x2A97AF0,
        0x74C930,
        0x2A97AD8,
        0x2CBF988,
        0x10DDD10,
        0x10E14D0,
        0x10E1530,
    },
    RowAvailabilityProfile{
        EldenRingBuild::v2_7_2_0,
        0x5DFD600,
        0x977B30,
        0x734640,
        0x74B300,
        0x2AD9520,
        0x2B16E98,
        0x2B16E58,
        0x2B16ED8,
        0x2B1D218,
        0xD82B90,
        0xD82CF0,
        0x2A99BE0,
        0x74CDE0,
        0x2A99BC8,
        0x2CC1C38,
        0x10DF400,
        0x10E2BC0,
        0x10E2C20,
    },
};

static_assert(
    kSceneProxyValueOffset + kScaleformValueFlagsOffset +
            sizeof(std::uint32_t) <=
        scaleform_path_result_size);
static_assert(
    kSceneProxyValueOffset + 0x28 + sizeof(void*) <=
        scaleform_path_result_size);
static_assert(kScaleformMemberResultSize >=
    kScaleformValuePayloadOffset + sizeof(void*));

enum class LabelTargetKind : std::uint8_t {
    none,
    ordinary_row,
    widget_unresolved,
    family_a_row,
    family_b_row,
};

using EffectiveAvailabilityAppearance = AvailabilityVisualState;

struct AvailabilityColorTransformState {
    AvailabilityColorTransform base{};
    AvailabilityColorTransform installed{};
    bool initialized{};
    bool owned{};
};

struct SliderValueBinding {
    void* slider{};
    void* slider_vtable{};
    void* movie_proxy_vtable{};
    void* movie_owner{};
    void* settled_slider{};
    void* settled_owner{};
    void* settled_payload{};
    std::size_t transform_slot{kInvalidSliderTransformSlot};
    std::uint64_t transform_generation{};
    std::uint8_t stable_observations{};
    std::uint8_t observation_failures{};
    std::uint8_t mutation_failures{};
    bool candidate{};
    bool identity_captured{};
    bool settled{};
    bool attached{};
    bool frame_ready{};
    bool disabled{};
};

// The live formatted-value repaint path deliberately does not share the
// availability color-transform lifecycle above. Row configuration remains a
// separately negotiated capability, so a missing or locally incompatible
// disabled-appearance backend must not make a label-table commit wait for
// player movement before its visible value changes.
struct FormattedSliderRefreshBinding {
    void* slider{};
    std::uint8_t refresh_failures{};
    std::uint64_t presentation_revision{};
    std::uint64_t failed_presentation_revision{};
    bool candidate{};
};

struct SliderTransformRegistryEntry {
    void* owner{};
    void* payload{};
    AvailabilityColorTransform base{};
    AvailabilityColorTransform installed{};
    std::uint64_t generation{};
    std::uint64_t use_serial{};
    std::uint32_t references{};
    bool initialized{};
    bool owned{};
    bool quarantined{};
};

struct PhysicalBinding {
    std::size_t presentation_slot{detail::invalid_compiled_index};
    std::size_t physical_index{};
    void* native_row{};
    void* row_vtable{};
    void* controller{};
    void* controller_vtable{};
    void* caption_accessor{};
    void* row_movie_proxy{};
    void* row_movie_proxy_vtable{};
    void* row_movie_owner{};
    const volatile std::uint8_t* byte_value{};
    LabelTargetKind label_target{LabelTargetKind::none};
    RowKind row_kind{RowKind::toggle};
    const wchar_t* original_help{};
    const wchar_t* installed_help{};
    std::uint64_t label_revision{};
    std::uint64_t help_revision{};
    std::uint64_t caption_revision{};
    std::uint64_t interaction_revision{};
    std::uint64_t appearance_revision{};
    std::uint64_t label_failed_revision{};
    std::uint64_t help_failed_revision{};
    std::uint64_t caption_failed_revision{};
    std::uint64_t interaction_failed_revision{};
    std::uint64_t appearance_failed_revision{};
    std::uint8_t label_failures{};
    std::uint8_t help_failures{};
    std::uint8_t caption_failures{};
    std::uint8_t interaction_failures{};
    std::uint8_t appearance_failures{};
    std::uint8_t construction_interactive{1};
    std::uint8_t installed_interactive{1};
    std::uint8_t applied_appearance{0xFF};
    std::uint8_t applied_value{0xFF};
    AvailabilityColorTransformState label_color_transform{};
    SliderValueBinding slider_value{};
    FormattedSliderRefreshBinding slider_refresh{};
    bool help_conflict{};
    bool interaction_conflict{};
    bool availability_supported{};
    bool appearance_disabled{};
};

struct PageBinding {
    void* page{};
    void* page_vtable{};
    std::uint64_t epoch{};
    std::uint64_t use_serial{};
    std::array<PhysicalBinding, kMaximumPropertyRows> rows{};
    std::size_t row_count{};
    SelectedHelpRefreshTicket pending_help{};
    bool active{};
};

std::mutex g_mutex{};
std::array<PageBinding, kMaximumTrackedPages> g_pages{};
std::uint64_t g_epoch{};
std::uint64_t g_use_serial{};
std::array<SliderTransformRegistryEntry, kSliderTransformRegistryCapacity>
    g_slider_transform_registry{};
std::uint64_t g_slider_transform_generation{};
std::uint64_t g_slider_transform_use_serial{};
std::atomic<std::uint32_t> g_fault_logs{};
thread_local bool g_selected_help_presenter_active{};

struct TextReadbackSlot {
    alignas(16) std::array<std::byte, kScaleformMemberResultSize> storage{};
    bool busy{};
    bool poisoned{};
};

// A fault may leave a managed Scaleform value partially linked. Its backing
// storage therefore has process lifetime and a poisoned slot is never reused
// or cleared speculatively.
TextReadbackSlot g_text_readback{};

void log_fault(
    const char* operation,
    void* page,
    std::size_t physical_index,
    unsigned long seh_code = 0) noexcept {
    const std::uint32_t fault =
        g_fault_logs.fetch_add(1, std::memory_order_relaxed) + 1u;
    if (fault <= kFaultLogLimit) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Runtime row presentation %s failed (fault=%u page=%p physical=%zu seh=0x%08lX)",
            operation,
            static_cast<unsigned>(fault),
            page,
            physical_index,
            seh_code);
    } else if (fault == kFaultLogLimit + 1u) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Runtime row presentation suppressed further fault logs");
    }
}

const RowAvailabilityProfile* availability_profile(
    EldenRingBuild build,
    std::size_t image_size) noexcept {
    const auto found = std::find_if(
        kRowAvailabilityProfiles.begin(),
        kRowAvailabilityProfiles.end(),
        [build, image_size](const RowAvailabilityProfile& profile) {
            return profile.build == build &&
                profile.image_size == image_size;
        });
    return found == kRowAvailabilityProfiles.end() ? nullptr : &*found;
}

const RowAvailabilityProfile* availability_profile(
    const GameAddresses& addresses) noexcept {
    const auto found = std::find_if(
        kRowAvailabilityProfiles.begin(),
        kRowAvailabilityProfiles.end(),
        [&](const RowAvailabilityProfile& profile) {
            return profile.image_size == addresses.game_image_size;
        });
    return found == kRowAvailabilityProfiles.end() ? nullptr : &*found;
}

bool rva_fits(
    std::size_t image_size,
    std::uintptr_t rva,
    std::size_t size) noexcept {
    return rva <= image_size && size <= image_size - rva;
}

bool availability_profile_exact(
    const std::uint8_t* image,
    std::size_t image_size,
    const RowAvailabilityProfile& profile,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!image || image_size != profile.image_size ||
        !rva_fits(image_size, profile.grid_predicate_rva,
            kGridPredicateBytes.size()) ||
        !rva_fits(image_size, profile.visibility_setter_rva,
            kVisibilitySetterBytes.size()) ||
        !rva_fits(image_size, profile.text_setter_rva,
            kScaleformTextSetterBytes.size()) ||
        !rva_fits(image_size, profile.action_controller_vtable_rva,
            (kControllerCaptionAccessorSlot + 1u) * sizeof(void*)) ||
        !rva_fits(image_size, profile.toggle_controller_vtable_rva,
            (kControllerCaptionAccessorSlot + 1u) * sizeof(void*)) ||
        !rva_fits(image_size, profile.slider_controller_vtable_rva,
            (kControllerCaptionAccessorSlot + 1u) * sizeof(void*)) ||
        !rva_fits(image_size, profile.inline_choice_controller_vtable_rva,
            (kControllerCaptionAccessorSlot + 1u) * sizeof(void*)) ||
        !rva_fits(image_size, profile.text_input_controller_vtable_rva,
            (kControllerCaptionAccessorSlot + 1u) * sizeof(void*)) ||
        !rva_fits(image_size, profile.value_constructor_rva,
            kScaleformValueConstructorBytes.size()) ||
        !rva_fits(image_size, profile.value_destructor_rva,
            kScaleformValueDestructorBytes.size()) ||
        !rva_fits(image_size, profile.scene_proxy_vtable_rva,
            2u * sizeof(void*)) ||
        !rva_fits(image_size, profile.scene_proxy_getter_rva, 1) ||
        !rva_fits(image_size, profile.scene_value_vtable_rva,
            sizeof(void*)) ||
        !rva_fits(image_size, profile.as3_interface_vtable_rva,
            0x120u) ||
        !rva_fits(image_size, profile.as3_get_member_rva,
            kAs3GetMemberBytes.size()) ||
        !rva_fits(image_size, profile.color_transform_getter_rva,
            kColorTransformGetterBytes.size()) ||
        !rva_fits(image_size, profile.color_transform_setter_rva,
            kColorTransformSetterBytes.size())) {
        return false;
    }

    bool exact{};
#if defined(_MSC_VER)
    __try {
#endif
        void* observed_proxy_getter{};
        void* observed_get_member{};
        void* observed_color_transform_getter{};
        void* observed_color_transform_setter{};
        std::memcpy(
            &observed_proxy_getter,
            image + profile.scene_proxy_vtable_rva + sizeof(void*),
            sizeof(observed_proxy_getter));
        std::memcpy(
            &observed_get_member,
            image + profile.as3_interface_vtable_rva + 4u * sizeof(void*),
            sizeof(observed_get_member));
        std::memcpy(
            &observed_color_transform_getter,
            image + profile.as3_interface_vtable_rva + 0x110u,
            sizeof(observed_color_transform_getter));
        std::memcpy(
            &observed_color_transform_setter,
            image + profile.as3_interface_vtable_rva + 0x118u,
            sizeof(observed_color_transform_setter));
        exact = std::memcmp(
                    image + profile.grid_predicate_rva,
                    kGridPredicateBytes.data(),
                    kGridPredicateBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.visibility_setter_rva,
                    kVisibilitySetterBytes.data(),
                    kVisibilitySetterBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.text_setter_rva,
                    kScaleformTextSetterBytes.data(),
                    kScaleformTextSetterBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.value_constructor_rva,
                    kScaleformValueConstructorBytes.data(),
                    kScaleformValueConstructorBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.value_destructor_rva,
                    kScaleformValueDestructorBytes.data(),
                    kScaleformValueDestructorBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.as3_get_member_rva,
                    kAs3GetMemberBytes.data(),
                    kAs3GetMemberBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.color_transform_getter_rva,
                    kColorTransformGetterBytes.data(),
                    kColorTransformGetterBytes.size()) == 0 &&
            std::memcmp(
                    image + profile.color_transform_setter_rva,
                    kColorTransformSetterBytes.data(),
                    kColorTransformSetterBytes.size()) == 0 &&
            observed_proxy_getter ==
                image + profile.scene_proxy_getter_rva &&
            observed_get_member == image + profile.as3_get_member_rva &&
            observed_color_transform_getter ==
                image + profile.color_transform_getter_rva &&
            observed_color_transform_setter ==
                image + profile.color_transform_setter_rva;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        exact = false;
    }
#endif
    return exact;
}

bool availability_runtime_exact(
    const GameAddresses& addresses,
    const RowAvailabilityProfile*& out_profile,
    unsigned long& out_seh) noexcept {
    out_profile = availability_profile(addresses);
    if (!out_profile || !addresses.game_image_base ||
        !addresses.title_bridge_complete() ||
        reinterpret_cast<void*>(addresses.scaleform_text_setter) !=
            addresses.game_image_base + out_profile->text_setter_rva ||
        reinterpret_cast<void*>(addresses.scaleform_result_destructor) !=
            addresses.game_image_base + out_profile->value_destructor_rva) {
        return false;
    }
    return availability_profile_exact(
        addresses.game_image_base,
        addresses.game_image_size,
        *out_profile,
        out_seh);
}

PageBinding* find_page(void* page) noexcept {
    const auto found = std::find_if(
        g_pages.begin(), g_pages.end(),
        [page](const PageBinding& candidate) {
            return candidate.active && candidate.page == page;
        });
    return found == g_pages.end() ? nullptr : &*found;
}

bool read_page_identity(
    void* page,
    const GameAddresses& addresses,
    void*& out_vtable,
    std::size_t& out_count,
    unsigned long& out_seh) noexcept {
    out_vtable = nullptr;
    out_count = 0;
    out_seh = 0;
    if (!page || !addresses.option_setting_dialog_vtable) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(&out_vtable, page, sizeof(out_vtable));
        std::memcpy(
            &out_count,
            static_cast<const std::byte*>(page) + kPropertyCountOffset,
            sizeof(out_count));
        return out_vtable == addresses.option_setting_dialog_vtable &&
            out_count <= kMaximumPropertyRows;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool read_selected_row(
    void* page,
    std::uint32_t& out_count,
    std::uint32_t& out_index,
    unsigned long& out_seh) noexcept {
    out_count = 0;
    out_index = 0;
    out_seh = 0;
    if (!page) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(
            &out_count,
            static_cast<const std::byte*>(page) + kSelectedRowCountOffset,
            sizeof(out_count));
        std::memcpy(
            &out_index,
            static_cast<const std::byte*>(page) + kSelectedRowIndexOffset,
            sizeof(out_index));
        return out_count != 0 && out_count <= kMaximumPropertyRows &&
            out_index < out_count;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool direct_help_source_matches(
    const PhysicalBinding& binding,
    const wchar_t* expected,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.native_row || !binding.row_vtable || !expected) return false;
#if defined(_MSC_VER)
    __try {
#endif
        void* row_vtable{};
        const wchar_t* source{};
        std::memcpy(&row_vtable, binding.native_row, sizeof(row_vtable));
        std::memcpy(
            &source,
            static_cast<const std::byte*>(binding.native_row) +
                kPropertyRowHelpDirectOffset,
            sizeof(source));
        return row_vtable == binding.row_vtable && source == expected;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool call_selected_help_presenter(
    SelectedHelpPresenterFn presenter,
    void* page,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!presenter || !page) return false;
#if defined(_MSC_VER)
    __try {
#endif
        presenter(page);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

WidgetTextLayout widget_text_layout(LabelTargetKind target) noexcept {
    switch (target) {
    case LabelTargetKind::family_a_row:
        return WidgetTextLayout::family_a;
    case LabelTargetKind::family_b_row:
        return WidgetTextLayout::family_b;
    default:
        return WidgetTextLayout::none;
    }
}

const char* widget_text_target_path(WidgetTextTarget target) noexcept {
    switch (target) {
    case WidgetTextTarget::direct_text_0:
        return kFamilyBRowLabelPath;
    case WidgetTextTarget::nested_text_0:
        return kFamilyAControlCaptionPath;
    case WidgetTextTarget::nested_text_1:
        return kFamilyARowLabelPath;
    default:
        return nullptr;
    }
}

const char* label_target_path(LabelTargetKind target) noexcept {
    if (target == LabelTargetKind::ordinary_row) {
        return kOrdinaryRowLabelPath;
    }
    return widget_text_target_path(select_widget_text_target(
        widget_text_layout(target), WidgetTextRole::row_label));
}

const char* caption_target_path(LabelTargetKind target) noexcept {
    return widget_text_target_path(select_widget_text_target(
        widget_text_layout(target), WidgetTextRole::control_caption));
}

constexpr bool uses_widget_text_pair(RowKind kind) noexcept {
    return kind == RowKind::button || kind == RowKind::popup_choice ||
        kind == RowKind::color_picker || kind == RowKind::submenu;
}

constexpr bool uses_action_controller(RowKind kind) noexcept {
    return kind == RowKind::button || kind == RowKind::popup_choice ||
        kind == RowKind::color_picker || kind == RowKind::submenu;
}

constexpr bool supports_control_caption(RowKind kind) noexcept {
    return kind == RowKind::button || kind == RowKind::popup_choice ||
        kind == RowKind::submenu;
}

constexpr bool supports_runtime_availability(RowKind kind) noexcept {
    switch (kind) {
    case RowKind::button:
    case RowKind::toggle:
    case RowKind::slider:
    case RowKind::numeric_slider:
    case RowKind::custom_slider:
    case RowKind::inline_choice:
    case RowKind::popup_choice:
    case RowKind::text_input:
    case RowKind::color_picker:
        return true;
    case RowKind::submenu:
        return false;
    }
    return false;
}

constexpr bool is_slider_row(RowKind kind) noexcept {
    return kind == RowKind::slider || kind == RowKind::numeric_slider ||
        kind == RowKind::custom_slider;
}

constexpr bool is_formatted_slider_row(RowKind kind) noexcept {
    return kind == RowKind::numeric_slider ||
        kind == RowKind::custom_slider;
}

constexpr bool uses_authored_availability_twins(RowKind kind) noexcept {
    return kind == RowKind::toggle || kind == RowKind::inline_choice;
}

bool any_color_transform_initialized(
    const PhysicalBinding& binding) noexcept {
    return binding.label_color_transform.initialized;
}

bool any_color_transform_owned(const PhysicalBinding& binding) noexcept {
    return binding.label_color_transform.owned;
}

std::uintptr_t availability_controller_vtable_rva(
    const RowAvailabilityProfile& profile,
    RowKind kind) noexcept {
    switch (kind) {
    case RowKind::button:
    case RowKind::popup_choice:
    case RowKind::color_picker:
        return profile.action_controller_vtable_rva;
    case RowKind::toggle:
        return profile.toggle_controller_vtable_rva;
    case RowKind::slider:
    case RowKind::numeric_slider:
    case RowKind::custom_slider:
        return profile.slider_controller_vtable_rva;
    case RowKind::inline_choice:
        return profile.inline_choice_controller_vtable_rva;
    case RowKind::text_input:
        return profile.text_input_controller_vtable_rva;
    case RowKind::submenu:
        return 0;
    }
    return 0;
}

bool availability_controller_matches(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    const PhysicalBinding& binding) noexcept {
    const std::uintptr_t rva = availability_controller_vtable_rva(
        profile, binding.row_kind);
    return rva != 0 && binding.controller_vtable ==
        addresses.game_image_base + rva;
}

static_assert(uses_action_controller(RowKind::popup_choice));
static_assert(!uses_action_controller(RowKind::inline_choice));
static_assert(supports_control_caption(RowKind::button));
static_assert(supports_control_caption(RowKind::popup_choice));
static_assert(supports_control_caption(RowKind::submenu));
static_assert(!supports_control_caption(RowKind::color_picker));
static_assert(supports_runtime_availability(RowKind::button));
static_assert(supports_runtime_availability(RowKind::toggle));
static_assert(supports_runtime_availability(RowKind::slider));
static_assert(supports_runtime_availability(RowKind::numeric_slider));
static_assert(supports_runtime_availability(RowKind::custom_slider));
static_assert(supports_runtime_availability(RowKind::inline_choice));
static_assert(supports_runtime_availability(RowKind::popup_choice));
static_assert(supports_runtime_availability(RowKind::text_input));
static_assert(supports_runtime_availability(RowKind::color_picker));
static_assert(!supports_runtime_availability(RowKind::submenu));
static_assert(is_slider_row(RowKind::slider));
static_assert(is_slider_row(RowKind::numeric_slider));
static_assert(is_slider_row(RowKind::custom_slider));
static_assert(!is_slider_row(RowKind::inline_choice));
static_assert(is_formatted_slider_row(RowKind::numeric_slider));
static_assert(is_formatted_slider_row(RowKind::custom_slider));
static_assert(!is_formatted_slider_row(RowKind::slider));
static_assert(uses_authored_availability_twins(RowKind::toggle));
static_assert(uses_authored_availability_twins(RowKind::inline_choice));

bool destroy_label_result(
    const GameAddresses& addresses,
    std::byte* result,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
#if defined(_MSC_VER)
    __try {
#endif
        addresses.scaleform_result_destructor(
            result + scaleform_destructor_member_offset);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool resolve_validated_label_path(
    const GameAddresses& addresses,
    void* movie_context,
    const char* path,
    std::byte* result,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!movie_context || !path || !*path || !result ||
        !addresses.title_bridge_complete()) {
        return false;
    }

    // A resolver return equal to destination transfers one constructed native
    // result to this function. A valid return remains owned by the caller;
    // every invalid constructed result is destroyed here exactly once.
    bool constructed = false;
    bool display_object = false;
#if defined(_MSC_VER)
    __try {
#endif
        constructed = addresses.scaleform_path_resolver(
            movie_context, result, path) == result;
        if (constructed) {
            void* source_vtable{};
            void* source_owner{};
            void* result_vtable{};
            void* result_self0{};
            void* result_self1{};
            void* result_self2{};
            void* result_owner{};
            std::uint32_t value_flags{};
            void* value_payload{};
            std::memcpy(
                &source_vtable,
                movie_context,
                sizeof(source_vtable));
            std::memcpy(
                &source_owner,
                static_cast<const std::byte*>(movie_context) +
                    kSceneProxyOwnerOffset,
                sizeof(source_owner));
            std::memcpy(
                &result_vtable,
                result,
                sizeof(result_vtable));
            std::memcpy(
                &result_self0,
                result + kSceneProxyFirstSelfOffset,
                sizeof(result_self0));
            std::memcpy(
                &result_self1,
                result + kSceneProxySecondSelfOffset,
                sizeof(result_self1));
            std::memcpy(
                &result_self2,
                result + kSceneProxyThirdSelfOffset,
                sizeof(result_self2));
            std::memcpy(
                &result_owner,
                result + kSceneProxyOwnerOffset,
                sizeof(result_owner));
            std::memcpy(
                &value_flags,
                result + kSceneProxyValueOffset +
                    kScaleformValueFlagsOffset,
                sizeof(value_flags));
            std::memcpy(
                &value_payload,
                result + kSceneProxyValueOffset + 0x28,
                sizeof(value_payload));
            display_object = source_vtable && source_owner &&
                result_vtable == source_vtable &&
                result_self0 == result && result_self1 == result &&
                result_self2 == result && result_owner == source_owner &&
                value_payload &&
                (value_flags & kScaleformValueKindMask) ==
                    kScaleformValueKindDisplayObject;
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        display_object = false;
    }
#endif

    if (!constructed || display_object) return display_object;

    unsigned long destructor_seh{};
    if (!destroy_label_result(addresses, result, destructor_seh) &&
        out_seh == 0) {
        out_seh = destructor_seh;
    }
    return false;
}

bool probe_label_path(
    const GameAddresses& addresses,
    void* movie_context,
    const char* path,
    unsigned long& out_seh) noexcept {
    alignas(16) std::byte result[scaleform_path_result_size]{};
    if (!resolve_validated_label_path(
            addresses, movie_context, path, result, out_seh)) {
        return false;
    }

    unsigned long destructor_seh{};
    if (!destroy_label_result(addresses, result, destructor_seh)) {
        out_seh = destructor_seh;
        return false;
    }
    return true;
}

LabelTargetKind discover_widget_row_label_target(
    const GameAddresses& addresses,
    void* movie_context,
    unsigned long& out_seh) noexcept {
    out_seh = 0;

    // Every supported action-like widget has one nested Text_1/Text object.
    // Its semantic role differs between the two authored movie families, so
    // its presence alone can never identify the row label.
    unsigned long common_seh{};
    const bool common_nested_text = probe_label_path(
        addresses,
        movie_context,
        kFamilyBControlCaptionPath,
        common_seh);
    if (common_seh != 0) {
        out_seh = common_seh;
        return LabelTargetKind::none;
    }

    // Family A stores the right control caption in nested Text_0/Text and the
    // left row label in the common nested Text_1/Text object.
    unsigned long family_a_seh{};
    const bool family_a_caption = probe_label_path(
        addresses,
        movie_context,
        kFamilyAControlCaptionPath,
        family_a_seh);
    if (family_a_seh != 0) {
        out_seh = family_a_seh;
        return LabelTargetKind::none;
    }

    WidgetTextLayout layout = select_widget_text_layout(
        common_nested_text,
        family_a_caption,
        false);
    if (layout == WidgetTextLayout::family_a) {
        return LabelTargetKind::family_a_row;
    }

    // Family B has no nested Text_0/Text caption. Its left row label is the
    // direct Text_0 object and the common Text_1/Text object is the right
    // control caption/value. Require that complete pair before selecting it.
    unsigned long family_b_seh{};
    const bool family_b_label = probe_label_path(
        addresses,
        movie_context,
        kFamilyBRowLabelPath,
        family_b_seh);
    if (family_b_seh != 0) {
        out_seh = family_b_seh;
        return LabelTargetKind::none;
    }

    layout = select_widget_text_layout(
        common_nested_text,
        family_a_caption,
        family_b_label);
    if (layout == WidgetTextLayout::family_b) {
        return LabelTargetKind::family_b_row;
    }
    return LabelTargetKind::none;
}

bool resolve_text_target(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    WidgetTextRole role,
    std::byte* result,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
#if defined(_MSC_VER)
    __try {
#endif
        void* current_controller_vtable{};
        std::memcpy(
            &current_controller_vtable,
            binding.controller,
            sizeof(current_controller_vtable));
        if (current_controller_vtable != binding.controller_vtable) {
            return false;
        }
        void* current_caption_accessor{};
        std::memcpy(
            &current_caption_accessor,
            static_cast<const std::byte*>(current_controller_vtable) +
                kControllerCaptionAccessorSlot * sizeof(void*),
            sizeof(current_caption_accessor));
        auto* const movie_context = static_cast<std::byte*>(
            binding.row_movie_proxy);
        void* proxy_vtable{};
        void* proxy_self0{};
        void* proxy_self1{};
        void* proxy_self2{};
        void* proxy_owner{};
        std::uint32_t value_flags{};
        std::memcpy(&proxy_vtable, movie_context, sizeof(proxy_vtable));
        std::memcpy(&proxy_self0,
            movie_context + kSceneProxyFirstSelfOffset,
            sizeof(proxy_self0));
        std::memcpy(&proxy_self1,
            movie_context + kSceneProxySecondSelfOffset,
            sizeof(proxy_self1));
        std::memcpy(&proxy_self2,
            movie_context + kSceneProxyThirdSelfOffset,
            sizeof(proxy_self2));
        std::memcpy(&proxy_owner,
            movie_context + kSceneProxyOwnerOffset,
            sizeof(proxy_owner));
        std::memcpy(&value_flags,
            movie_context + kSceneProxyValueOffset +
                kScaleformValueFlagsOffset,
            sizeof(value_flags));
        if (current_caption_accessor != binding.caption_accessor ||
            proxy_vtable != binding.row_movie_proxy_vtable ||
            proxy_self0 != movie_context || proxy_self1 != movie_context ||
            proxy_self2 != movie_context ||
            proxy_owner != binding.row_movie_owner ||
            (value_flags & kScaleformValueKindMask) !=
                kScaleformValueKindDisplayObject) {
            return false;
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    if (binding.label_target == LabelTargetKind::widget_unresolved) {
        const LabelTargetKind discovered = discover_widget_row_label_target(
            addresses,
            binding.row_movie_proxy,
            out_seh);
        if (discovered == LabelTargetKind::none) return false;
        binding.label_target = discovered;
    }
    const char* const path = role == WidgetTextRole::row_label
        ? label_target_path(binding.label_target)
        : caption_target_path(binding.label_target);
    if (!path) return false;
    return resolve_validated_label_path(
        addresses,
        binding.row_movie_proxy,
        path,
        result,
        out_seh);
}

bool set_resolved_label(
    const GameAddresses& addresses,
    std::byte* result,
    const wchar_t* label,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
#if defined(_MSC_VER)
    __try {
#endif
        addresses.scaleform_text_setter(
            result + scaleform_text_value_offset,
            label);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool assign_label(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const wchar_t* label,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.controller || !binding.controller_vtable ||
        !binding.row_movie_proxy || !label || !*label ||
        !addresses.title_bridge_complete()) {
        return false;
    }

    alignas(16) std::byte result[scaleform_path_result_size]{};
    if (!resolve_text_target(
            addresses,
            binding,
            WidgetTextRole::row_label,
            result,
            out_seh)) {
        return false;
    }
    const bool assigned =
        set_resolved_label(addresses, result, label, out_seh);
    unsigned long destructor_seh{};
    const bool destroyed =
        destroy_label_result(addresses, result, destructor_seh);
    if (!destroyed) out_seh = destructor_seh;
    return assigned && destroyed;
}

bool assign_caption(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const wchar_t* caption,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!supports_control_caption(binding.row_kind) ||
        !binding.controller || !binding.controller_vtable ||
        !binding.row_movie_proxy || !caption || !*caption ||
        !addresses.title_bridge_complete()) {
        return false;
    }

    alignas(16) std::byte result[scaleform_path_result_size]{};
    if (!resolve_text_target(
            addresses,
            binding,
            WidgetTextRole::control_caption,
            result,
            out_seh)) {
        return false;
    }
    const bool assigned =
        set_resolved_label(addresses, result, caption, out_seh);
    unsigned long destructor_seh{};
    const bool destroyed =
        destroy_label_result(addresses, result, destructor_seh);
    if (!destroyed) out_seh = destructor_seh;
    return assigned && destroyed;
}

struct AvailabilityPathResult {
    alignas(16) std::array<std::byte, scaleform_path_result_size> storage{};
    bool constructed{};

    [[nodiscard]] void* proxy() noexcept { return storage.data(); }
    [[nodiscard]] void* text_value() noexcept {
        return storage.data() + scaleform_text_value_offset;
    }
};

bool resolve_availability_path(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const char* path,
    AvailabilityPathResult& result,
    unsigned long& out_seh) noexcept {
    result = {};
    result.constructed = resolve_validated_label_path(
        addresses,
        binding.row_movie_proxy,
        path,
        result.storage.data(),
        out_seh);
    return result.constructed;
}

bool destroy_availability_path(
    const GameAddresses& addresses,
    AvailabilityPathResult& result,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!result.constructed) return true;
    result.constructed = false;
    return destroy_label_result(addresses, result.storage.data(), out_seh);
}

bool color_transforms_equal(
    const AvailabilityColorTransform& left,
    const AvailabilityColorTransform& right) noexcept {
    return std::memcmp(
        left.data(), right.data(), sizeof(AvailabilityColorTransform)) == 0;
}

struct ScaleformTransformIdentity {
    void* proxy_vtable{};
    void* owner{};
};

ScaleformTransformIdentity row_transform_identity(
    const PhysicalBinding& binding) noexcept {
    return {
        .proxy_vtable = binding.row_movie_proxy_vtable,
        .owner = binding.row_movie_owner,
    };
}

bool resolve_color_transform_interfaces(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    ScaleformTransformIdentity identity,
    const void* scene_proxy,
    void*& out_interface,
    void*& out_payload,
    ScaleformColorTransformGetterFn& out_getter,
    ScaleformColorTransformSetterFn& out_setter,
    unsigned long& out_seh) noexcept {
    out_interface = nullptr;
    out_payload = nullptr;
    out_getter = nullptr;
    out_setter = nullptr;
    out_seh = 0;
    if (!scene_proxy) return false;

    void* proxy_vtable{};
    void* self0{};
    void* self1{};
    void* self2{};
    void* owner{};
    void* value_vtable{};
    void* interface_vtable{};
    void* getter{};
    void* setter{};
    std::uint32_t value_flags{};
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const proxy = static_cast<const std::byte*>(scene_proxy);
        const auto* const value = proxy + kSceneProxyValueOffset;
        std::memcpy(&proxy_vtable, proxy, sizeof(proxy_vtable));
        std::memcpy(&self0,
            proxy + kSceneProxyFirstSelfOffset, sizeof(self0));
        std::memcpy(&self1,
            proxy + kSceneProxySecondSelfOffset, sizeof(self1));
        std::memcpy(&self2,
            proxy + kSceneProxyThirdSelfOffset, sizeof(self2));
        std::memcpy(&owner,
            proxy + kSceneProxyOwnerOffset, sizeof(owner));
        std::memcpy(&value_vtable, value, sizeof(value_vtable));
        std::memcpy(&out_interface,
            value + kScaleformValueInterfaceOffset,
            sizeof(out_interface));
        std::memcpy(&value_flags,
            value + kScaleformValueFlagsOffset,
            sizeof(value_flags));
        std::memcpy(&out_payload,
            value + kScaleformValuePayloadOffset,
            sizeof(out_payload));
        if (out_interface) {
            std::memcpy(&interface_vtable,
                out_interface, sizeof(interface_vtable));
        }
        if (interface_vtable ==
            addresses.game_image_base + profile.as3_interface_vtable_rva) {
            std::memcpy(&getter,
                static_cast<const std::byte*>(interface_vtable) + 0x110u,
                sizeof(getter));
            std::memcpy(&setter,
                static_cast<const std::byte*>(interface_vtable) + 0x118u,
                sizeof(setter));
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif

    if (proxy_vtable != identity.proxy_vtable ||
        self0 != scene_proxy || self1 != scene_proxy ||
        self2 != scene_proxy || owner != identity.owner ||
        value_vtable !=
            addresses.game_image_base + profile.scene_value_vtable_rva ||
        !out_interface || !out_payload ||
        interface_vtable !=
            addresses.game_image_base + profile.as3_interface_vtable_rva ||
        getter != addresses.game_image_base +
            profile.color_transform_getter_rva ||
        setter != addresses.game_image_base +
            profile.color_transform_setter_rva ||
        (value_flags & kScaleformValueKindMask) !=
            kScaleformValueKindDisplayObject) {
        return false;
    }

    out_getter = reinterpret_cast<ScaleformColorTransformGetterFn>(getter);
    out_setter = reinterpret_cast<ScaleformColorTransformSetterFn>(setter);
    return true;
}

bool read_color_transform(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    ScaleformTransformIdentity identity,
    const void* scene_proxy,
    AvailabilityColorTransform& out_transform,
    unsigned long& out_seh) noexcept {
    out_transform = {};
    AvailabilityColorTransformTransfer transfer{};
    void* value_interface{};
    void* value_payload{};
    ScaleformColorTransformGetterFn getter{};
    ScaleformColorTransformSetterFn setter{};
    if (!resolve_color_transform_interfaces(
            addresses, profile, identity, scene_proxy,
            value_interface, value_payload, getter, setter, out_seh)) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        if (!getter(value_interface, value_payload, transfer.value.data())) {
            return false;
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    out_transform = transfer.value;
    return std::all_of(
        out_transform.begin(), out_transform.end(),
        [](float value) noexcept {
            return std::isfinite(value) && value >= -8192.0f &&
                value <= 8192.0f;
        });
}

bool write_color_transform(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    ScaleformTransformIdentity identity,
    void* scene_proxy,
    const AvailabilityColorTransform& transform,
    unsigned long& out_seh) noexcept {
    // Do not pass `transform.data()` directly. In particular, restoration can
    // be optimized into a reference to one of PhysicalBinding's captured base
    // transforms, whose std::array storage is not guaranteed to satisfy the
    // game's aligned SIMD load. The dimmed path happened to use an aligned
    // temporary, which made the bug appear only while returning to normal.
    AvailabilityColorTransformTransfer transfer{};
    transfer.value = transform;
    void* value_interface{};
    void* value_payload{};
    ScaleformColorTransformGetterFn getter{};
    ScaleformColorTransformSetterFn setter{};
    if (!resolve_color_transform_interfaces(
            addresses, profile, identity, scene_proxy,
            value_interface, value_payload, getter, setter, out_seh)) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        if (!setter(value_interface, value_payload, transfer.value.data())) {
            return false;
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    AvailabilityColorTransform observed{};
    return read_color_transform(
        addresses, profile, identity, scene_proxy, observed, out_seh) &&
        color_transforms_equal(observed, transform);
}

struct PreparedAvailabilityColorTransform {
    AvailabilityPathResult path{};
    AvailabilityColorTransformState* state{};
    AvailabilityColorTransform current{};
    AvailabilityColorTransform requested{};
    const char* target_operation{};
    const char* read_operation{};
    const char* ownership_operation{};
    const char* write_operation{};
    const char* destructor_operation{};
    bool needs_write{};
    bool write_attempted{};
};

bool apply_availability_color_transforms(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    PhysicalBinding& binding,
    EffectiveAvailabilityAppearance desired,
    const char*& out_operation,
    unsigned long& out_seh) noexcept {
    std::array<PreparedAvailabilityColorTransform, 1> targets{};
    constexpr std::size_t target_count{1};
    out_seh = 0;

    // Every validated row family exposes its left label through the same
    // semantic resolver used by label edits. Dim that exact object instead of
    // the row root: action-like movies do not reliably propagate a root color
    // transform to their nested label, and the Family A/B layouts place the
    // label at different paths.
    targets[0].state = &binding.label_color_transform;
    targets[0].target_operation =
        "availability label color-transform target";
    targets[0].read_operation =
        "availability label color-transform read";
    targets[0].ownership_operation =
        "availability label color-transform ownership";
    targets[0].write_operation =
        "availability label color-transform write";
    targets[0].destructor_operation =
        "availability label color-transform target destructor";
    out_operation = targets[0].target_operation;
    bool success = resolve_text_target(
        addresses,
        binding,
        WidgetTextRole::row_label,
        targets[0].path.storage.data(),
        out_seh);
    targets[0].path.constructed = success;

    const EffectiveAvailabilityAppearance construction =
        binding.construction_interactive != 0
        ? EffectiveAvailabilityAppearance::normal
        : EffectiveAvailabilityAppearance::disabled;

    // Read and validate ownership before mutating the resolved label.
    for (std::size_t index = 0; success && index < target_count; ++index) {
        PreparedAvailabilityColorTransform& target = targets[index];
        out_operation = target.read_operation;
        success = read_color_transform(
            addresses,
            profile,
            row_transform_identity(binding),
            target.path.proxy(),
            target.current,
            out_seh);
        if (!success) break;

        AvailabilityColorTransformState& state = *target.state;
        if (!state.initialized) {
            state.base = target.current;
            state.installed = target.current;
            state.initialized = true;
        }
        target.requested = compose_availability_color_transform(
            state.base, desired, construction);
        const bool current_is_base = color_transforms_equal(
            target.current, state.base);
        const bool current_is_installed = state.owned &&
            color_transforms_equal(target.current, state.installed);
        if (!current_is_base && !current_is_installed) {
            out_operation = target.ownership_operation;
            success = false;
            break;
        }
        target.needs_write = !color_transforms_equal(
            target.current, target.requested);
    }

    std::size_t attempted_count{};
    for (std::size_t index = 0; success && index < target_count; ++index) {
        PreparedAvailabilityColorTransform& target = targets[index];
        if (!target.needs_write) continue;
        out_operation = target.write_operation;
        target.write_attempted = true;
        attempted_count = index + 1;
        success = write_color_transform(
            addresses,
            profile,
            row_transform_identity(binding),
            target.path.proxy(),
            target.requested,
            out_seh);
    }

    if (!success && attempted_count != 0) {
        // A failed setter may still have changed its object before readback
        // failed. Restore the attempted label from its exact preflight value.
        for (std::size_t reverse = attempted_count; reverse != 0; --reverse) {
            PreparedAvailabilityColorTransform& target =
                targets[reverse - 1];
            if (!target.write_attempted) continue;
            unsigned long rollback_seh{};
            if (!write_color_transform(
                    addresses,
                    profile,
                    row_transform_identity(binding),
                    target.path.proxy(),
                    target.current,
                    rollback_seh)) {
                out_operation = "availability color-transform rollback";
                if (out_seh == 0) out_seh = rollback_seh;
            }
        }
    }

    if (success) {
        for (std::size_t index = 0; index < target_count; ++index) {
            PreparedAvailabilityColorTransform& target = targets[index];
            AvailabilityColorTransformState& state = *target.state;
            state.installed = target.requested;
            state.owned = !color_transforms_equal(
                target.requested, state.base);
        }
    }

    // Destroy every managed path result even when an earlier phase failed.
    // Preserve the first operational failure unless cleanup is the only one.
    for (std::size_t reverse = target_count; reverse != 0; --reverse) {
        PreparedAvailabilityColorTransform& target = targets[reverse - 1];
        unsigned long destructor_seh{};
        if (!destroy_availability_path(
                addresses, target.path, destructor_seh) && success) {
            success = false;
            out_operation = target.destructor_operation;
            out_seh = destructor_seh;
        }
    }
    return success;
}

bool synchronize_toggle_value_text(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    const AvailabilityPathResult& source,
    AvailabilityPathResult& destination,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!source.constructed || !destination.constructed ||
        g_text_readback.busy || g_text_readback.poisoned) {
        return false;
    }

    void* value_interface{};
    void* value_payload{};
    void* proxy_vtable{};
    void* value_vtable{};
    void* interface_vtable{};
    void* get_member{};
    std::uint32_t value_flags{};
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const value =
            source.storage.data() + kSceneProxyValueOffset;
        std::memcpy(&proxy_vtable, source.storage.data(),
            sizeof(proxy_vtable));
        std::memcpy(&value_vtable, value, sizeof(value_vtable));
        std::memcpy(&value_interface,
            value + kScaleformValueInterfaceOffset,
            sizeof(value_interface));
        std::memcpy(&value_flags,
            value + kScaleformValueFlagsOffset,
            sizeof(value_flags));
        std::memcpy(&value_payload,
            value + kScaleformValuePayloadOffset,
            sizeof(value_payload));
        if (value_interface) {
            std::memcpy(&interface_vtable, value_interface,
                sizeof(interface_vtable));
        }
        if (interface_vtable ==
            addresses.game_image_base + profile.as3_interface_vtable_rva) {
            std::memcpy(&get_member,
                static_cast<const std::byte*>(interface_vtable) +
                    4u * sizeof(void*),
                sizeof(get_member));
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif

    if (proxy_vtable !=
            addresses.game_image_base + profile.scene_proxy_vtable_rva ||
        value_vtable !=
            addresses.game_image_base + profile.scene_value_vtable_rva ||
        !value_interface || !value_payload ||
        interface_vtable !=
            addresses.game_image_base + profile.as3_interface_vtable_rva ||
        get_member != addresses.game_image_base + profile.as3_get_member_rva ||
        (value_flags & kScaleformValueKindMask) !=
            kScaleformValueKindDisplayObject) {
        return false;
    }

    g_text_readback.storage.fill(std::byte{});
    g_text_readback.busy = true;
    const auto poison_slot = []() noexcept {
        g_text_readback.poisoned = true;
        g_text_readback.busy = false;
    };
    const auto release_slot = []() noexcept {
        g_text_readback.storage.fill(std::byte{});
        g_text_readback.busy = false;
    };

    void* constructor_result{};
#if defined(_MSC_VER)
    __try {
#endif
        constructor_result =
            reinterpret_cast<ScaleformValueConstructorFn>(
                addresses.game_image_base + profile.value_constructor_rva)(
                    g_text_readback.storage.data());
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        poison_slot();
        return false;
    }
#endif
    if (constructor_result != g_text_readback.storage.data()) {
        poison_slot();
        return false;
    }

    std::memcpy(
        g_text_readback.storage.data() + kScaleformValueFlagsOffset,
        &kScaleformConvertStringW,
        sizeof(kScaleformConvertStringW));
    bool member_result{};
#if defined(_MSC_VER)
    __try {
#endif
        member_result = reinterpret_cast<As3GetMemberFn>(get_member)(
            value_interface,
            value_payload,
            kTextMemberName,
            g_text_readback.storage.data() + 0x08);
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        poison_slot();
        return false;
    }
#endif

    void* result_interface{};
    const wchar_t* result_text{};
    std::uint32_t result_flags{};
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(&result_interface,
            g_text_readback.storage.data() +
                kScaleformValueInterfaceOffset,
            sizeof(result_interface));
        std::memcpy(&result_flags,
            g_text_readback.storage.data() + kScaleformValueFlagsOffset,
            sizeof(result_flags));
        std::memcpy(&result_text,
            g_text_readback.storage.data() + kScaleformValuePayloadOffset,
            sizeof(result_text));
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        poison_slot();
        return false;
    }
#endif

    if (member_result &&
        (result_flags != kScaleformStringW ||
         (result_flags & kScaleformValueKindMask) !=
             kScaleformValueKindStringW ||
         result_interface != value_interface || !result_text ||
         reinterpret_cast<std::uintptr_t>(result_text) < 0x10000u ||
         reinterpret_cast<std::uintptr_t>(result_text) %
                 alignof(wchar_t) !=
             0u)) {
        poison_slot();
        return false;
    }

    std::array<wchar_t, kMaximumSyncedValueChars> copied{};
    bool terminated{};
    if (member_result) {
#if defined(_MSC_VER)
        __try {
#endif
            for (std::size_t index = 0; index < copied.size(); ++index) {
                copied[index] = result_text[index];
                if (copied[index] == L'\0') {
                    terminated = index != 0;
                    break;
                }
            }
#if defined(_MSC_VER)
        } __except (out_seh = GetExceptionCode(),
            EXCEPTION_EXECUTE_HANDLER) {
            poison_slot();
            return false;
        }
#endif
    }

#if defined(_MSC_VER)
    __try {
#endif
        // GetMember constructed the managed value at the base address. This
        // differs from path results, whose owned member begins at +0x28.
        addresses.scaleform_result_destructor(
            g_text_readback.storage.data());
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        poison_slot();
        return false;
    }
#endif
    release_slot();
    if (!member_result || !terminated) return false;

#if defined(_MSC_VER)
    __try {
#endif
        addresses.scaleform_text_setter(
            destination.text_value(), copied.data());
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool set_toggle_twin_visibility(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    AvailabilityPathResult& target,
    bool visible,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!target.constructed) return false;
#if defined(_MSC_VER)
    __try {
#endif
        reinterpret_cast<ScaleformVisibilitySetterFn>(
            addresses.game_image_base + profile.visibility_setter_rva)(
                target.proxy(), visible ? 1u : 0u);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool apply_toggle_visibility_pair(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    AvailabilityPathResult& normal,
    AvailabilityPathResult& disabled,
    EffectiveAvailabilityAppearance appearance,
    unsigned long& out_seh) noexcept {
    // Showing the destination first prevents a transient frame with neither
    // authored twin visible.
    if (appearance == EffectiveAvailabilityAppearance::disabled) {
        if (!set_toggle_twin_visibility(
                addresses, profile, disabled, true, out_seh)) {
            return false;
        }
        return set_toggle_twin_visibility(
            addresses, profile, normal, false, out_seh);
    }
    if (!set_toggle_twin_visibility(
            addresses, profile, normal, true, out_seh)) {
        return false;
    }
    return set_toggle_twin_visibility(
        addresses, profile, disabled, false, out_seh);
}

EffectiveAvailabilityAppearance effective_appearance(
    const detail::RowAvailabilityView& view) noexcept {
    return select_availability_visual_state(
        view.appearance, view.interactive);
}

bool apply_toggle_appearance(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    PhysicalBinding& binding,
    EffectiveAvailabilityAppearance desired,
    const char*& out_operation,
    unsigned long& out_seh) noexcept {
    AvailabilityPathResult normal{};
    AvailabilityPathResult disabled{};
    AvailabilityPathResult source_text{};
    AvailabilityPathResult destination_text{};
    out_operation = "availability normal path";
    out_seh = 0;

    bool success = resolve_availability_path(
        addresses, binding, kToggleNormalItemPath, normal, out_seh);
    if (success) {
        out_operation = "availability disabled path";
        success = resolve_availability_path(
            addresses, binding, kToggleDisabledItemPath, disabled, out_seh);
    }

    const EffectiveAvailabilityAppearance construction =
        binding.construction_interactive != 0
        ? EffectiveAvailabilityAppearance::normal
        : EffectiveAvailabilityAppearance::disabled;
    if (success && desired != construction) {
        const char* const source_path =
            construction == EffectiveAvailabilityAppearance::normal
            ? kToggleNormalTextPath
            : kToggleDisabledTextPath;
        const char* const destination_path =
            construction == EffectiveAvailabilityAppearance::normal
            ? kToggleDisabledTextPath
            : kToggleNormalTextPath;
        out_operation = "availability authoritative text path";
        success = resolve_availability_path(
            addresses, binding, source_path, source_text, out_seh);
        if (success) {
            out_operation = "availability destination text path";
            success = resolve_availability_path(
                addresses,
                binding,
                destination_path,
                destination_text,
                out_seh);
        }
        if (success) {
            out_operation = "availability localized value synchronization";
            success = synchronize_toggle_value_text(
                addresses,
                profile,
                source_text,
                destination_text,
                out_seh);
        }
    }

    if (success) {
        out_operation = "availability visibility pair";
        success = apply_toggle_visibility_pair(
            addresses, profile, normal, disabled, desired, out_seh);
        if (!success) {
            unsigned long rollback_seh{};
            const EffectiveAvailabilityAppearance previous =
                select_availability_rollback_visual_state(
                    binding.applied_appearance, construction);
            (void)apply_toggle_visibility_pair(
                addresses,
                profile,
                normal,
                disabled,
                previous,
                rollback_seh);
            if (out_seh == 0) out_seh = rollback_seh;
        }
    }

    unsigned long destructor_seh{};
    const auto destroy = [&](AvailabilityPathResult& result,
                             const char* operation) noexcept {
        if (!destroy_availability_path(
                addresses, result, destructor_seh) && success) {
            success = false;
            out_operation = operation;
            out_seh = destructor_seh;
        }
    };
    destroy(destination_text, "availability destination text destructor");
    destroy(source_text, "availability source text destructor");
    destroy(disabled, "availability disabled path destructor");
    destroy(normal, "availability normal path destructor");
    return success;
}

bool assign_help(
    PhysicalBinding& binding,
    const wchar_t* help,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.native_row || !help || binding.help_conflict) return false;
#if defined(_MSC_VER)
    __try {
#endif
        auto* const row = static_cast<std::byte*>(binding.native_row);
        void* current_vtable{};
        const wchar_t* current_help{};
        std::array<std::byte, 32> owned_before{};
        std::array<std::byte, 32> owned_after{};
        std::memcpy(&current_vtable, row, sizeof(current_vtable));
        std::memcpy(
            &current_help,
            row + kPropertyRowHelpDirectOffset,
            sizeof(current_help));
        if (current_vtable != binding.row_vtable ||
            (current_help != binding.original_help &&
             current_help != binding.installed_help)) {
            binding.help_conflict = true;
            return false;
        }
        std::memcpy(
            owned_before.data(),
            row + kPropertyRowOwnedHelpOffset,
            owned_before.size());
        std::memcpy(
            row + kPropertyRowHelpDirectOffset,
            &help,
            sizeof(help));
        const wchar_t* verified{};
        std::memcpy(
            &verified,
            row + kPropertyRowHelpDirectOffset,
            sizeof(verified));
        std::memcpy(
            owned_after.data(),
            row + kPropertyRowOwnedHelpOffset,
            owned_after.size());
        if (verified != help || owned_before != owned_after) return false;
        binding.installed_help = help;
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool restore_help_if_owned(
    PhysicalBinding& binding,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.native_row || !binding.installed_help) return true;
#if defined(_MSC_VER)
    __try {
#endif
        auto* const row = static_cast<std::byte*>(binding.native_row);
        void* current_vtable{};
        const wchar_t* current_help{};
        std::memcpy(&current_vtable, row, sizeof(current_vtable));
        std::memcpy(
            &current_help,
            row + kPropertyRowHelpDirectOffset,
            sizeof(current_help));
        if (current_vtable != binding.row_vtable ||
            current_help != binding.installed_help) {
            return true;
        }
        std::memcpy(
            row + kPropertyRowHelpDirectOffset,
            &binding.original_help,
            sizeof(binding.original_help));
        const wchar_t* verified{};
        std::memcpy(
            &verified,
            row + kPropertyRowHelpDirectOffset,
            sizeof(verified));
        return verified == binding.original_help;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool read_availability_identity(
    PhysicalBinding& binding,
    std::uint8_t& out_interactive,
    unsigned long& out_seh) noexcept {
    out_interactive = 0;
    out_seh = 0;
    if (!binding.availability_supported || !binding.native_row ||
        !binding.controller || !binding.controller_vtable) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        auto* const row = static_cast<std::byte*>(binding.native_row);
        void* row_vtable{};
        void* controller{};
        void* controller_vtable{};
        void* caption_accessor{};
        std::memcpy(&row_vtable, row, sizeof(row_vtable));
        std::memcpy(&controller,
            row + kPropertyControllerOffset,
            sizeof(controller));
        if (controller) {
            std::memcpy(&controller_vtable, controller,
                sizeof(controller_vtable));
        }
        if (controller_vtable) {
            std::memcpy(&caption_accessor,
                static_cast<const std::byte*>(controller_vtable) +
                    kControllerCaptionAccessorSlot * sizeof(void*),
                sizeof(caption_accessor));
        }
        std::memcpy(&out_interactive,
            row + kPropertyEnabledOffset,
            sizeof(out_interactive));
        return row_vtable == binding.row_vtable &&
            controller == binding.controller &&
            controller_vtable == binding.controller_vtable &&
            caption_accessor == binding.caption_accessor &&
            out_interactive <= 1u;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

struct SliderRuntimeIdentity {
    void* slider_vtable{};
    void* movie_proxy_vtable{};
    void* movie_owner{};
};

struct SliderBindingSnapshot {
    std::uintptr_t bound_value{};
    std::int32_t current_value{};
    std::uint8_t backing_value{};
};

enum class SliderObservationStatus : std::uint8_t {
    pending,
    settled,
    failed,
};

void reset_slider_settlement(SliderValueBinding& slider) noexcept {
    slider.settled_slider = nullptr;
    slider.settled_owner = nullptr;
    slider.settled_payload = nullptr;
    slider.stable_observations = 0;
    slider.settled = false;
    slider.frame_ready = false;
}

bool slider_identities_equal(
    const SliderValueBinding& captured,
    const SliderRuntimeIdentity& observed) noexcept {
    return captured.identity_captured &&
        captured.slider_vtable == observed.slider_vtable &&
        captured.movie_proxy_vtable == observed.movie_proxy_vtable &&
        captured.movie_owner == observed.movie_owner;
}

bool capture_slider_runtime_identity(
    void* slider,
    const GameAddresses& addresses,
    SliderRuntimeIdentity& out,
    unsigned long& out_seh) noexcept {
    out = {};
    out_seh = 0;
    if (!slider || !addresses.slider_refresh) return false;

    void* refresh{};
    void* self0{};
    void* self1{};
    void* self2{};
    std::uint32_t flags{};
#if defined(_MSC_VER)
    __try {
#endif
        auto* const bytes = static_cast<std::byte*>(slider);
        auto* const proxy = bytes + kSliderMovieProxyOffset;
        std::memcpy(&out.slider_vtable, slider,
            sizeof(out.slider_vtable));
        if (out.slider_vtable) {
            std::memcpy(&refresh,
                static_cast<const std::byte*>(out.slider_vtable) +
                    kSliderRefreshVtableSlot * sizeof(void*),
                sizeof(refresh));
        }
        std::memcpy(&out.movie_proxy_vtable, proxy,
            sizeof(out.movie_proxy_vtable));
        std::memcpy(&self0, proxy + kSceneProxyFirstSelfOffset,
            sizeof(self0));
        std::memcpy(&self1, proxy + kSceneProxySecondSelfOffset,
            sizeof(self1));
        std::memcpy(&self2, proxy + kSceneProxyThirdSelfOffset,
            sizeof(self2));
        std::memcpy(&out.movie_owner, proxy + kSceneProxyOwnerOffset,
            sizeof(out.movie_owner));
        std::memcpy(&flags,
            proxy + kSceneProxyValueOffset + kScaleformValueFlagsOffset,
            sizeof(flags));
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif

    auto* const proxy = static_cast<std::byte*>(slider) +
        kSliderMovieProxyOffset;
    return out.slider_vtable &&
        refresh == reinterpret_cast<void*>(addresses.slider_refresh) &&
        out.movie_proxy_vtable && self0 == proxy && self1 == proxy &&
        self2 == proxy && out.movie_owner &&
        (flags & kScaleformValueKindMask) ==
            kScaleformValueKindDisplayObject;
}

bool read_slider_binding_snapshot(
    const PhysicalBinding& binding,
    SliderBindingSnapshot& out,
    unsigned long& out_seh) noexcept {
    out = {};
    out_seh = 0;
    const SliderValueBinding& slider = binding.slider_value;
    if (!slider.slider || !binding.byte_value) return false;
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const bytes = static_cast<const std::byte*>(
            slider.slider);
        std::memcpy(&out.bound_value,
            bytes + kSliderBoundValueOffset,
            sizeof(out.bound_value));
        std::memcpy(&out.current_value,
            bytes + kSliderCurrentValueOffset,
            sizeof(out.current_value));
        out.backing_value = *binding.byte_value;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return out.bound_value == reinterpret_cast<std::uintptr_t>(
               const_cast<std::uint8_t*>(binding.byte_value)) &&
        out.current_value == static_cast<std::int32_t>(out.backing_value);
}

bool slider_runtime_identities_equal(
    const SliderRuntimeIdentity& left,
    const SliderRuntimeIdentity& right) noexcept {
    return left.slider_vtable == right.slider_vtable &&
        left.movie_proxy_vtable == right.movie_proxy_vtable &&
        left.movie_owner == right.movie_owner;
}

bool read_formatted_slider_refresh_snapshot(
    const PhysicalBinding& binding,
    SliderBindingSnapshot& out,
    unsigned long& out_seh) noexcept {
    out = {};
    out_seh = 0;
    const FormattedSliderRefreshBinding& refresh =
        binding.slider_refresh;
    if (!refresh.slider || !binding.byte_value) return false;
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const bytes = static_cast<const std::byte*>(
            refresh.slider);
        std::memcpy(&out.bound_value,
            bytes + kSliderBoundValueOffset,
            sizeof(out.bound_value));
        std::memcpy(&out.current_value,
            bytes + kSliderCurrentValueOffset,
            sizeof(out.current_value));
        out.backing_value = *binding.byte_value;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return out.bound_value == reinterpret_cast<std::uintptr_t>(
               const_cast<std::uint8_t*>(binding.byte_value)) &&
        out.current_value == static_cast<std::int32_t>(out.backing_value);
}

bool formatted_slider_row_identity_matches(
    const PhysicalBinding& binding,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.native_row || !binding.row_vtable ||
        !binding.controller || !binding.controller_vtable) {
        return false;
    }

    void* live_row_vtable{};
    void* live_controller{};
    void* live_controller_vtable{};
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const row = static_cast<const std::byte*>(
            binding.native_row);
        std::memcpy(&live_row_vtable, row,
            sizeof(live_row_vtable));
        std::memcpy(&live_controller,
            row + kPropertyControllerOffset,
            sizeof(live_controller));
        if (live_controller) {
            std::memcpy(&live_controller_vtable, live_controller,
                sizeof(live_controller_vtable));
        }
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return live_row_vtable == binding.row_vtable &&
        live_controller == binding.controller &&
        live_controller_vtable == binding.controller_vtable;
}

bool validate_formatted_slider_refresh_binding(
    const GameAddresses& addresses,
    const PhysicalBinding& binding,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    const FormattedSliderRefreshBinding& refresh =
        binding.slider_refresh;
    if (!refresh.candidate || !refresh.slider || !binding.controller ||
        !binding.byte_value || !is_formatted_slider_row(binding.row_kind) ||
        refresh.slider != static_cast<const std::byte*>(
                binding.controller) + kEmbeddedSliderOffset) {
        return false;
    }

    // Sample both the native runtime identity and its bound value on either
    // side of the validation window. The actual refresh bridge performs its
    // own pointer/bank checks again immediately before and after the native
    // trampoline; this first pass proves that the materializer-owned embedded
    // SliderCtrl is stable enough to enter that bridge.
    SliderRuntimeIdentity identity_before{};
    SliderRuntimeIdentity identity_after{};
    SliderBindingSnapshot value_before{};
    SliderBindingSnapshot value_after{};
    if (!formatted_slider_row_identity_matches(binding, out_seh) ||
        !capture_slider_runtime_identity(
            refresh.slider, addresses, identity_before, out_seh) ||
        !read_formatted_slider_refresh_snapshot(
            binding, value_before, out_seh) ||
        !capture_slider_runtime_identity(
            refresh.slider, addresses, identity_after, out_seh) ||
        !read_formatted_slider_refresh_snapshot(
            binding, value_after, out_seh) ||
        !formatted_slider_row_identity_matches(binding, out_seh)) {
        return false;
    }
    return slider_runtime_identities_equal(
            identity_before, identity_after) &&
        value_before.bound_value == value_after.bound_value &&
        value_before.current_value == value_after.current_value &&
        value_before.backing_value == value_after.backing_value;
}

bool read_availability_path_payload(
    const AvailabilityPathResult& result,
    void*& out_owner,
    void*& out_payload,
    unsigned long& out_seh) noexcept {
    out_owner = nullptr;
    out_payload = nullptr;
    out_seh = 0;
    if (!result.constructed) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(&out_owner,
            result.storage.data() + kSceneProxyOwnerOffset,
            sizeof(out_owner));
        std::memcpy(&out_payload,
            result.storage.data() + kSceneProxyValueOffset +
                kScaleformValuePayloadOffset,
            sizeof(out_payload));
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return out_owner && out_payload;
}

bool resolve_slider_text_0(
    const GameAddresses& addresses,
    const PhysicalBinding& binding,
    AvailabilityPathResult& result,
    void*& out_payload,
    unsigned long& out_seh) noexcept {
    result = {};
    out_payload = nullptr;
    out_seh = 0;
    const SliderValueBinding& slider = binding.slider_value;
    if (!slider.slider || !slider.identity_captured) return false;

    auto* const movie_proxy = static_cast<std::byte*>(slider.slider) +
        kSliderMovieProxyOffset;
    result.constructed = resolve_validated_label_path(
        addresses,
        movie_proxy,
        kSliderValueTextPath,
        result.storage.data(),
        out_seh);
    if (!result.constructed) return false;

    void* owner{};
    if (!read_availability_path_payload(
            result, owner, out_payload, out_seh) ||
        owner != slider.movie_owner) {
        out_payload = nullptr;
        return false;
    }
    return true;
}

SliderTransformRegistryEntry* slider_registry_entry(
    const SliderValueBinding& binding) noexcept {
    if (!binding.attached ||
        binding.transform_slot >= g_slider_transform_registry.size()) {
        return nullptr;
    }
    SliderTransformRegistryEntry& entry =
        g_slider_transform_registry[binding.transform_slot];
    return entry.initialized &&
            entry.generation == binding.transform_generation
        ? &entry
        : nullptr;
}

void clear_slider_registry_handle(SliderValueBinding& binding) noexcept {
    binding.transform_slot = kInvalidSliderTransformSlot;
    binding.transform_generation = 0;
    binding.attached = false;
}

bool read_bound_slider_transform(
    const GameAddresses& addresses,
    const RowAvailabilityProfile& profile,
    PhysicalBinding& binding,
    AvailabilityPathResult& path,
    void*& out_payload,
    AvailabilityColorTransform& out_transform,
    unsigned long& out_seh) noexcept {
    out_payload = nullptr;
    out_transform = {};
    out_seh = 0;
    SliderValueBinding& slider = binding.slider_value;
    if (!slider.slider || !slider.identity_captured) return false;

    SliderRuntimeIdentity observed{};
    if (!capture_slider_runtime_identity(
            slider.slider, addresses, observed, out_seh) ||
        !slider_identities_equal(slider, observed)) {
        return false;
    }

    SliderBindingSnapshot before{};
    if (!read_slider_binding_snapshot(binding, before, out_seh)) {
        return false;
    }
    if (!resolve_slider_text_0(
            addresses, binding, path, out_payload, out_seh)) {
        return false;
    }

    SliderBindingSnapshot after{};
    if (!read_slider_binding_snapshot(binding, after, out_seh) ||
        before.bound_value != after.bound_value ||
        before.current_value != after.current_value ||
        before.backing_value != after.backing_value) {
        return false;
    }

    const ScaleformTransformIdentity identity{
        .proxy_vtable = slider.movie_proxy_vtable,
        .owner = slider.movie_owner,
    };
    return read_color_transform(
        addresses,
        profile,
        identity,
        path.proxy(),
        out_transform,
        out_seh);
}

bool detach_slider_transform(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const char*& out_operation,
    unsigned long& out_seh) noexcept {
    out_operation = "slider value transform detach";
    out_seh = 0;
    SliderValueBinding& slider = binding.slider_value;
    if (!slider.attached) {
        clear_slider_registry_handle(slider);
        reset_slider_settlement(slider);
        return true;
    }

    SliderTransformRegistryEntry* const entry =
        slider_registry_entry(slider);
    if (!entry || entry->references == 0) {
        clear_slider_registry_handle(slider);
        reset_slider_settlement(slider);
        return false;
    }

    --entry->references;
    entry->use_serial = ++g_slider_transform_use_serial;
    clear_slider_registry_handle(slider);
    if (entry->references != 0) {
        reset_slider_settlement(slider);
        return true;
    }

    bool confirmed_base{};
    const RowAvailabilityProfile* profile{};
    if (!availability_runtime_exact(addresses, profile, out_seh) || !profile) {
        out_operation = "slider value transform detach profile";
    } else {
        AvailabilityPathResult path{};
        void* payload{};
        AvailabilityColorTransform current{};
        out_operation = "slider value transform detach target";
        bool success = read_bound_slider_transform(
            addresses,
            *profile,
            binding,
            path,
            payload,
            current,
            out_seh);
        if (success && (slider.movie_owner != entry->owner ||
                payload != entry->payload)) {
            success = false;
            out_operation = "slider value transform detach identity";
        }
        if (success) {
            const bool current_is_base = color_transforms_equal(
                current, entry->base);
            const bool current_is_installed = entry->owned &&
                color_transforms_equal(current, entry->installed);
            if (current_is_base) {
                confirmed_base = true;
            } else if (current_is_installed) {
                out_operation = "slider value transform restoration";
                const ScaleformTransformIdentity identity{
                    .proxy_vtable = slider.movie_proxy_vtable,
                    .owner = slider.movie_owner,
                };
                confirmed_base = write_color_transform(
                    addresses,
                    *profile,
                    identity,
                    path.proxy(),
                    entry->base,
                    out_seh);
            } else {
                out_operation = "slider value transform detach ownership";
            }
        }

        unsigned long destructor_seh{};
        if (!destroy_availability_path(
                addresses, path, destructor_seh)) {
            confirmed_base = false;
            out_operation = "slider value transform detach destructor";
            if (out_seh == 0) out_seh = destructor_seh;
        }
    }

    if (confirmed_base) {
        *entry = {};
    } else {
        // A zero-reference entry whose native base could not be proved must
        // retain its immutable baseline. Reusing the current transform as a
        // fresh base here is exactly the compounding-darkness failure seen by
        // the v2 probe during overlapping page epochs.
        entry->quarantined = true;
    }
    reset_slider_settlement(slider);
    return confirmed_base;
}

SliderObservationStatus observe_slider_binding(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    void*& out_payload,
    const char*& out_operation,
    unsigned long& out_seh) noexcept {
    out_payload = nullptr;
    out_operation = "slider value binding observation";
    out_seh = 0;
    SliderValueBinding& slider = binding.slider_value;
    if (!slider.candidate || !slider.slider || slider.disabled) {
        return SliderObservationStatus::failed;
    }

    std::uint8_t interactive{};
    if (!read_availability_identity(binding, interactive, out_seh)) {
        reset_slider_settlement(slider);
        return SliderObservationStatus::failed;
    }
    (void)interactive;

    SliderRuntimeIdentity observed{};
    if (!capture_slider_runtime_identity(
            slider.slider, addresses, observed, out_seh)) {
        reset_slider_settlement(slider);
        return SliderObservationStatus::failed;
    }
    if (!slider_identities_equal(slider, observed)) {
        if (slider.attached) {
            const char* detach_operation{};
            unsigned long detach_seh{};
            if (!detach_slider_transform(
                    addresses,
                    binding,
                    detach_operation,
                    detach_seh)) {
                out_operation = detach_operation;
                if (out_seh == 0) out_seh = detach_seh;
            }
        }
        slider.slider_vtable = observed.slider_vtable;
        slider.movie_proxy_vtable = observed.movie_proxy_vtable;
        slider.movie_owner = observed.movie_owner;
        slider.identity_captured = true;
        reset_slider_settlement(slider);
        return SliderObservationStatus::pending;
    }

    SliderBindingSnapshot before{};
    if (!read_slider_binding_snapshot(binding, before, out_seh)) {
        reset_slider_settlement(slider);
        return SliderObservationStatus::failed;
    }

    AvailabilityPathResult path{};
    bool success = resolve_slider_text_0(
        addresses, binding, path, out_payload, out_seh);
    SliderBindingSnapshot after{};
    if (success) {
        success = read_slider_binding_snapshot(
                binding, after, out_seh) &&
            before.bound_value == after.bound_value &&
            before.current_value == after.current_value &&
            before.backing_value == after.backing_value;
    }

    unsigned long destructor_seh{};
    if (!destroy_availability_path(
            addresses, path, destructor_seh)) {
        success = false;
        out_operation = "slider value binding target destructor";
        if (out_seh == 0) out_seh = destructor_seh;
    }
    if (!success) {
        out_payload = nullptr;
        reset_slider_settlement(slider);
        return SliderObservationStatus::failed;
    }

    const bool same_observation =
        slider.settled_slider == slider.slider &&
        slider.settled_owner == slider.movie_owner &&
        slider.settled_payload == out_payload;
    if (!same_observation) {
        if (slider.attached) {
            const char* detach_operation{};
            unsigned long detach_seh{};
            if (!detach_slider_transform(
                    addresses,
                    binding,
                    detach_operation,
                    detach_seh)) {
                out_operation = detach_operation;
                if (out_seh == 0) out_seh = detach_seh;
            }
        }
        slider.settled_slider = slider.slider;
        slider.settled_owner = slider.movie_owner;
        slider.settled_payload = out_payload;
        slider.stable_observations = 1;
        slider.settled = false;
        return SliderObservationStatus::pending;
    }

    if (slider.stable_observations < 2u) {
        ++slider.stable_observations;
    }
    slider.settled = slider.stable_observations >= 2u;
    return slider.settled
        ? SliderObservationStatus::settled
        : SliderObservationStatus::pending;
}

bool attach_slider_transform(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const char*& out_operation,
    unsigned long& out_seh) noexcept {
    out_operation = "slider value transform attach";
    out_seh = 0;
    SliderValueBinding& slider = binding.slider_value;
    if (!slider.settled || !slider.settled_owner ||
        !slider.settled_payload) {
        return false;
    }
    if (SliderTransformRegistryEntry* const attached =
            slider_registry_entry(slider)) {
        return attached->owner == slider.settled_owner &&
            attached->payload == slider.settled_payload;
    }
    clear_slider_registry_handle(slider);

    const RowAvailabilityProfile* profile{};
    if (!availability_runtime_exact(addresses, profile, out_seh) || !profile) {
        out_operation = "slider value transform attach profile";
        return false;
    }

    AvailabilityPathResult path{};
    void* payload{};
    AvailabilityColorTransform current{};
    bool success = read_bound_slider_transform(
        addresses,
        *profile,
        binding,
        path,
        payload,
        current,
        out_seh);
    if (success && (slider.movie_owner != slider.settled_owner ||
            payload != slider.settled_payload)) {
        success = false;
        out_operation = "slider value transform attach identity";
    }

    unsigned long destructor_seh{};
    if (!destroy_availability_path(addresses, path, destructor_seh)) {
        success = false;
        out_operation = "slider value transform attach destructor";
        if (out_seh == 0) out_seh = destructor_seh;
    }
    if (!success) return false;

    auto found = std::find_if(
        g_slider_transform_registry.begin(),
        g_slider_transform_registry.end(),
        [&](const SliderTransformRegistryEntry& candidate) {
            return candidate.initialized &&
                candidate.owner == slider.settled_owner &&
                candidate.payload == slider.settled_payload;
        });

    if (found == g_slider_transform_registry.end()) {
        found = std::find_if(
            g_slider_transform_registry.begin(),
            g_slider_transform_registry.end(),
            [](const SliderTransformRegistryEntry& candidate) {
                return !candidate.initialized;
            });
        if (found == g_slider_transform_registry.end()) {
            out_operation = "slider value transform registry capacity";
            return false;
        }
        std::uint64_t generation = ++g_slider_transform_generation;
        if (generation == 0) generation = ++g_slider_transform_generation;
        *found = {
            .owner = slider.settled_owner,
            .payload = slider.settled_payload,
            .base = current,
            .installed = current,
            .generation = generation,
            .use_serial = ++g_slider_transform_use_serial,
            .references = 0,
            .initialized = true,
            .owned = false,
            .quarantined = false,
        };
    } else {
        const bool current_is_base = color_transforms_equal(
            current, found->base);
        const bool current_is_installed = found->owned &&
            color_transforms_equal(current, found->installed);
        if (!current_is_base && !current_is_installed) {
            found->quarantined = true;
            found->use_serial = ++g_slider_transform_use_serial;
            out_operation = "slider value transform attach ownership";
            return false;
        }
        if (current_is_base) {
            // Native code may have republished the field while another page
            // epoch still held the registry key. Keep the original baseline
            // and simply relinquish the stale installed-state claim.
            found->installed = found->base;
            found->owned = false;
        }
        found->quarantined = false;
        found->use_serial = ++g_slider_transform_use_serial;
    }

    ++found->references;
    slider.transform_slot = static_cast<std::size_t>(
        found - g_slider_transform_registry.begin());
    slider.transform_generation = found->generation;
    slider.attached = true;
    return true;
}

void disable_slider_binding(
    void* page,
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const char* operation,
    unsigned long seh) noexcept {
    SliderValueBinding& slider = binding.slider_value;
    slider.disabled = true;
    slider.frame_ready = false;
    if (slider.attached) {
        const char* detach_operation{};
        unsigned long detach_seh{};
        if (!detach_slider_transform(
                addresses,
                binding,
                detach_operation,
                detach_seh)) {
            if (seh == 0) seh = detach_seh;
        }
    }
    log_fault(operation ? operation : "slider value presentation",
        page, binding.physical_index, seh);
}

void prepare_slider_bindings(
    void* page,
    const GameAddresses& addresses,
    PageBinding& page_binding,
    std::size_t physical_count) noexcept {
    std::array<PhysicalBinding*, kMaximumPropertyRows> active{};
    std::array<void*, kMaximumPropertyRows> payloads{};
    std::size_t active_count{};
    bool all_ready = true;

    for (std::size_t index = 0; index < page_binding.row_count; ++index) {
        PhysicalBinding& binding = page_binding.rows[index];
        SliderValueBinding& slider = binding.slider_value;
        if (!slider.candidate || slider.disabled ||
            binding.physical_index >= physical_count ||
            binding.native_row != static_cast<std::byte*>(page) +
                kPropertyCollectionOffset +
                binding.physical_index * kPropertyRowStride) {
            continue;
        }

        if (active_count >= active.size()) {
            disable_slider_binding(page, addresses, binding,
                "slider value page capacity", 0);
            continue;
        }
        active[active_count] = &binding;
        slider.frame_ready = false;

        const char* operation{};
        unsigned long seh{};
        const SliderObservationStatus status = observe_slider_binding(
            addresses,
            binding,
            payloads[active_count],
            operation,
            seh);
        if (status == SliderObservationStatus::failed) {
            all_ready = false;
            if (++slider.observation_failures >= 4u) {
                disable_slider_binding(
                    page, addresses, binding, operation, seh);
            }
        } else {
            slider.observation_failures = 0;
            if (status != SliderObservationStatus::settled) {
                all_ready = false;
            }
        }
        ++active_count;
    }

    for (std::size_t left = 0; left < active_count; ++left) {
        if (!payloads[left]) continue;
        for (std::size_t right = left + 1; right < active_count; ++right) {
            if (payloads[left] != payloads[right]) continue;
            all_ready = false;
            for (const std::size_t collision : {left, right}) {
                PhysicalBinding& binding = *active[collision];
                SliderValueBinding& slider = binding.slider_value;
                if (slider.attached) {
                    const char* detach_operation{};
                    unsigned long detach_seh{};
                    (void)detach_slider_transform(
                        addresses,
                        binding,
                        detach_operation,
                        detach_seh);
                }
                reset_slider_settlement(slider);
                if (++slider.observation_failures >= 4u) {
                    disable_slider_binding(page, addresses, binding,
                        "slider value in-page alias", 0);
                }
            }
        }
    }
    if (!all_ready) return;

    bool all_attached = true;
    for (std::size_t index = 0; index < active_count; ++index) {
        PhysicalBinding& binding = *active[index];
        const char* operation{};
        unsigned long seh{};
        if (!attach_slider_transform(
                addresses, binding, operation, seh)) {
            all_attached = false;
            disable_slider_binding(
                page, addresses, binding, operation, seh);
        }
    }
    if (!all_attached) return;
    for (std::size_t index = 0; index < active_count; ++index) {
        SliderValueBinding& slider = active[index]->slider_value;
        slider.frame_ready = !slider.disabled && slider.settled &&
            slider.attached;
    }
}

void apply_slider_value_appearance(
    void* page,
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const detail::RowAvailabilityView& view) noexcept {
    SliderValueBinding& slider = binding.slider_value;
    if (!view || !slider.candidate || slider.disabled ||
        !slider.frame_ready ||
        !slider.settled || !slider.attached) {
        return;
    }

    SliderTransformRegistryEntry* const entry =
        slider_registry_entry(slider);
    if (!entry || entry->owner != slider.settled_owner ||
        entry->payload != slider.settled_payload) {
        disable_slider_binding(page, addresses, binding,
            "slider value registry identity", 0);
        return;
    }

    EffectiveAvailabilityAppearance desired = effective_appearance(view);
    unsigned long seh{};
    if (binding.interaction_conflict &&
        view.appearance ==
            ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY) {
        std::uint8_t actual_interactive{};
        if (!read_availability_identity(
                binding, actual_interactive, seh)) {
            if (++slider.mutation_failures >= 4u) {
                disable_slider_binding(page, addresses, binding,
                    "slider value follow read", seh);
            }
            return;
        }
        desired = actual_interactive != 0
            ? EffectiveAvailabilityAppearance::normal
            : EffectiveAvailabilityAppearance::disabled;
    }

    const RowAvailabilityProfile* profile{};
    if (!availability_runtime_exact(addresses, profile, seh) || !profile ||
        !availability_controller_matches(addresses, *profile, binding)) {
        disable_slider_binding(page, addresses, binding,
            "slider value exact-build revalidation", seh);
        return;
    }

    AvailabilityPathResult path{};
    void* payload{};
    AvailabilityColorTransform current{};
    const char* operation = "slider value color-transform read";
    bool success = read_bound_slider_transform(
        addresses,
        *profile,
        binding,
        path,
        payload,
        current,
        seh);
    if (success && (slider.movie_owner != entry->owner ||
            payload != entry->payload)) {
        // This is a rematerialization boundary, not evidence that the new
        // Text_0 object is malformed. The next post-native frames must settle
        // it before any write is attempted.
        reset_slider_settlement(slider);
        success = false;
        operation = nullptr;
    }

    AvailabilityColorTransform requested{};
    AvailabilityColorTransform rollback = current;
    if (success) {
        const bool current_is_base = color_transforms_equal(
            current, entry->base);
        const bool current_is_installed = entry->owned &&
            color_transforms_equal(current, entry->installed);
        if (!current_is_base && !current_is_installed) {
            success = false;
            operation = "slider value color-transform ownership";
            entry->quarantined = true;
        } else {
            if (current_is_base && entry->owned) {
                entry->installed = entry->base;
                entry->owned = false;
            }
            // Unlike the left row label, SliderCtrl Text_0 remains authored
            // in its normal (white) transform even for a construction-disabled
            // row. Always compose its disabled presentation from that immutable
            // normal baseline; never invert based on +0x80 construction state.
            requested = compose_availability_color_transform(
                entry->base,
                desired,
                EffectiveAvailabilityAppearance::normal);
            if (!color_transforms_equal(current, requested)) {
                operation = "slider value color-transform write";
                const ScaleformTransformIdentity identity{
                    .proxy_vtable = slider.movie_proxy_vtable,
                    .owner = slider.movie_owner,
                };
                success = write_color_transform(
                    addresses,
                    *profile,
                    identity,
                    path.proxy(),
                    requested,
                    seh);
                if (!success) {
                    unsigned long rollback_seh{};
                    if (!write_color_transform(
                            addresses,
                            *profile,
                            identity,
                            path.proxy(),
                            rollback,
                            rollback_seh) &&
                        seh == 0) {
                        seh = rollback_seh;
                    }
                }
            }
        }
    }

    if (success) {
        entry->installed = requested;
        entry->owned = !color_transforms_equal(requested, entry->base);
        entry->quarantined = false;
        entry->use_serial = ++g_slider_transform_use_serial;
    }

    unsigned long destructor_seh{};
    if (!destroy_availability_path(
            addresses, path, destructor_seh)) {
        success = false;
        operation = "slider value color-transform target destructor";
        if (seh == 0) seh = destructor_seh;
    }

    if (success) {
        slider.mutation_failures = 0;
        return;
    }
    if (!operation) return;
    if (++slider.mutation_failures >= 4u) {
        disable_slider_binding(
            page, addresses, binding, operation, seh);
    } else {
        log_fault(operation, page, binding.physical_index, seh);
    }
}

bool install_interactivity(
    PhysicalBinding& binding,
    bool interactive,
    std::uint64_t revision,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (binding.interaction_conflict || !binding.availability_supported) {
        return false;
    }
    std::uint8_t current{};
    if (!read_availability_identity(binding, current, out_seh)) return false;
    if (current != binding.installed_interactive) {
        binding.interaction_conflict = true;
        return false;
    }

    const std::uint8_t desired = interactive ? 1u : 0u;
    if (current != desired) {
#if defined(_MSC_VER)
        __try {
#endif
            auto* const destination =
                static_cast<std::byte*>(binding.native_row) +
                kPropertyEnabledOffset;
            std::memcpy(destination, &desired, sizeof(desired));
            std::uint8_t verified{};
            std::memcpy(&verified, destination, sizeof(verified));
            if (verified != desired) return false;
#if defined(_MSC_VER)
        } __except (out_seh = GetExceptionCode(),
            EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#endif
        binding.installed_interactive = desired;
    }
    binding.interaction_revision = revision;
    return true;
}

bool restore_interactivity_if_owned(
    PhysicalBinding& binding,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.availability_supported || binding.interaction_conflict ||
        binding.installed_interactive == binding.construction_interactive) {
        return true;
    }
    std::uint8_t current{};
    if (!read_availability_identity(binding, current, out_seh)) return false;
    if (current != binding.installed_interactive) return true;
#if defined(_MSC_VER)
    __try {
#endif
        auto* const destination =
            static_cast<std::byte*>(binding.native_row) +
            kPropertyEnabledOffset;
        std::memcpy(destination,
            &binding.construction_interactive,
            sizeof(binding.construction_interactive));
        std::uint8_t verified{};
        std::memcpy(&verified, destination, sizeof(verified));
        return verified == binding.construction_interactive;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool restore_appearance_if_owned(
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!binding.availability_supported ||
        (!any_color_transform_owned(binding) &&
         binding.applied_appearance == static_cast<std::uint8_t>(
             binding.construction_interactive != 0
                 ? EffectiveAvailabilityAppearance::normal
                 : EffectiveAvailabilityAppearance::disabled))) {
        return true;
    }

    const RowAvailabilityProfile* profile{};
    if (!availability_runtime_exact(addresses, profile, out_seh) || !profile ||
        !availability_controller_matches(addresses, *profile, binding)) {
        return false;
    }
    const EffectiveAvailabilityAppearance construction =
        binding.construction_interactive != 0
        ? EffectiveAvailabilityAppearance::normal
        : EffectiveAvailabilityAppearance::disabled;
    const char* operation{};
    bool restored = true;
    if (any_color_transform_initialized(binding) &&
        !apply_availability_color_transforms(
            addresses,
            *profile,
            binding,
            construction,
            operation,
            out_seh)) {
        restored = false;
    }
    if (uses_authored_availability_twins(binding.row_kind) &&
        binding.applied_appearance !=
            static_cast<std::uint8_t>(construction) &&
        !apply_toggle_appearance(
            addresses,
            *profile,
            binding,
            construction,
            operation,
            out_seh)) {
        restored = false;
    }
    if (restored) {
        binding.applied_appearance = static_cast<std::uint8_t>(construction);
    }
    return restored;
}

void restore_page_owned_state(
    PageBinding& binding,
    const GameAddresses& addresses) noexcept {
    for (std::size_t index = 0; index < binding.row_count; ++index) {
        PhysicalBinding& row = binding.rows[index];
        unsigned long seh{};
        if (row.slider_value.attached) {
            const char* operation{};
            if (!detach_slider_transform(
                    addresses, row, operation, seh)) {
                log_fault(
                    operation ? operation
                              : "slider value presentation restoration",
                    binding.page,
                    row.physical_index,
                    seh);
            }
        }
        seh = 0;
        if (!restore_help_if_owned(row, seh)) {
            log_fault(
                "help-source restoration",
                binding.page,
                row.physical_index,
                seh);
        }
        seh = 0;
        if (!restore_interactivity_if_owned(row, seh)) {
            log_fault(
                "availability restoration",
                binding.page,
                row.physical_index,
                seh);
        }
        seh = 0;
        if (!restore_appearance_if_owned(addresses, row, seh)) {
            log_fault(
                "availability appearance restoration",
                binding.page,
                row.physical_index,
                seh);
        }
    }
}

void apply_binding_appearance(
    void* page,
    const GameAddresses& addresses,
    PhysicalBinding& binding,
    const detail::RowAvailabilityView& view) noexcept {
    if (!view || !binding.availability_supported ||
        binding.appearance_disabled) {
        return;
    }

    EffectiveAvailabilityAppearance desired = effective_appearance(view);
    unsigned long seh{};
    if (binding.interaction_conflict &&
        view.appearance ==
            ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY) {
        // Interaction and appearance have independent ownership. If another
        // mod owns +0x80, Follow must mirror the byte that is actually live
        // rather than the value ERNativeUI could not install. Explicit Normal
        // and Disabled remain valid visual-only overrides.
        std::uint8_t actual_interactive{};
        if (!read_availability_identity(
                binding, actual_interactive, seh)) {
            log_fault("availability follow read", page,
                binding.physical_index, seh);
            return;
        }
        desired = actual_interactive != 0
            ? EffectiveAvailabilityAppearance::normal
            : EffectiveAvailabilityAppearance::disabled;
    }
    std::uint8_t value{0xFF};
#if defined(_MSC_VER)
    __try {
#endif
        if (binding.byte_value) value = *binding.byte_value;
#if defined(_MSC_VER)
    } __except (seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        binding.appearance_disabled = true;
        log_fault("availability value read", page,
            binding.physical_index, seh);
        return;
    }
#endif

    const std::uint8_t desired_byte = static_cast<std::uint8_t>(desired);
    if (binding.appearance_revision == view.revision &&
        binding.applied_appearance == desired_byte &&
        binding.applied_value == value &&
        !any_color_transform_owned(binding)) {
        return;
    }

    const EffectiveAvailabilityAppearance construction =
        binding.construction_interactive != 0
        ? EffectiveAvailabilityAppearance::normal
        : EffectiveAvailabilityAppearance::disabled;
    if (binding.applied_appearance == desired_byte &&
        desired == construction && !any_color_transform_owned(binding)) {
        // The controller owns and refreshes its construction-selected twin.
        // No cross-twin text copy or visibility mutation is needed while that
        // authored twin remains selected, including the untouched baseline.
        binding.applied_value = value;
        binding.appearance_revision = view.revision;
        return;
    }

    if (binding.appearance_failed_revision != view.revision) {
        binding.appearance_failed_revision = view.revision;
        binding.appearance_failures = 0;
    } else if (binding.appearance_failures >= 4u) {
        return;
    }

    const RowAvailabilityProfile* profile{};
    if (!availability_runtime_exact(addresses, profile, seh) || !profile ||
        !availability_controller_matches(addresses, *profile, binding)) {
        binding.appearance_disabled = true;
        log_fault("availability exact-build revalidation", page,
            binding.physical_index, seh);
        return;
    }

    const EffectiveAvailabilityAppearance previous =
        select_availability_rollback_visual_state(
            binding.applied_appearance, construction);
    const bool authored_twins =
        uses_authored_availability_twins(binding.row_kind);
    const char* operation{};
    bool success = true;
    if (authored_twins) {
        success = apply_toggle_appearance(
            addresses,
            *profile,
            binding,
            desired,
            operation,
            seh);
    }
    if (success) {
        success = apply_availability_color_transforms(
            addresses,
            *profile,
            binding,
            desired,
            operation,
            seh);
    }
    if (!success) {
        // The transform setter can succeed before a later readback or path
        // destructor reports failure. Best-effort rollback therefore covers
        // both visual layers, not only the authored Toggle/Inline twins.
        if (any_color_transform_initialized(binding)) {
            const char* rollback_operation{};
            unsigned long rollback_seh{};
            (void)apply_availability_color_transforms(
                addresses,
                *profile,
                binding,
                previous,
                rollback_operation,
                rollback_seh);
            if (seh == 0) seh = rollback_seh;
        }
        if (authored_twins) {
            const char* rollback_operation{};
            unsigned long rollback_seh{};
            (void)apply_toggle_appearance(
                addresses,
                *profile,
                binding,
                previous,
                rollback_operation,
                rollback_seh);
            if (seh == 0) seh = rollback_seh;
        }
        ++binding.appearance_failures;
        // Missing or structurally modified GFX is a binding-local visual
        // incompatibility. Preserve independently proven +0x80 interaction.
        binding.appearance_disabled = true;
        log_fault(operation ? operation : "availability appearance",
            page, binding.physical_index, seh);
        return;
    }

    binding.applied_appearance = desired_byte;
    binding.applied_value = value;
    binding.appearance_revision = view.revision;
    binding.appearance_failures = 0;
}

bool apply_binding(
    void* page,
    const GameAddresses& addresses,
    PhysicalBinding& binding) noexcept {
    bool help_changed = false;
    const detail::RowPresentationView view =
        detail::row_presentation_view(binding.presentation_slot);
    if (view && view.help_revision != 0 &&
        binding.help_revision != view.help_revision &&
        !binding.help_conflict &&
        !(binding.help_failed_revision == view.help_revision &&
          binding.help_failures >= 4u)) {
        if (binding.help_failed_revision != view.help_revision) {
            binding.help_failed_revision = view.help_revision;
            binding.help_failures = 0;
        }
        unsigned long seh{};
        if (assign_help(binding, view.help, seh)) {
            binding.help_revision = view.help_revision;
            binding.help_failures = 0;
            help_changed = true;
        } else if (!binding.help_conflict) {
            ++binding.help_failures;
            log_fault("help-source publication", page,
                binding.physical_index, seh);
        } else {
            log_fault("help-source ownership conflict", page,
                binding.physical_index, seh);
        }
    }

    // Action-like widgets publish their registered text during every native
    // page frame. Reapply an edited row label after that pass even when the
    // client revision is unchanged. Ordinary rows retain the assigned label
    // and continue to use the cheaper revision-only path.
    const bool native_frame_republishes_label =
        binding.label_target == LabelTargetKind::family_a_row ||
        binding.label_target == LabelTargetKind::family_b_row;
    if (view && (binding.label_revision != view.revision ||
         native_frame_republishes_label) &&
        !(binding.label_failed_revision == view.revision &&
          binding.label_failures >= 4u)) {
        if (binding.label_failed_revision != view.revision) {
            binding.label_failed_revision = view.revision;
            binding.label_failures = 0;
        }
        unsigned long seh{};
        if (assign_label(addresses, binding, view.label, seh)) {
            binding.label_revision = view.revision;
            binding.label_failures = 0;
        } else {
            ++binding.label_failures;
            log_fault("label assignment", page,
                binding.physical_index, seh);
        }
    }

    // The right-hand text of Button, PopupChoice, and Submenu rows is also
    // native-owned and republished every frame. A non-null desired caption is
    // therefore an explicit persistent override, not a one-shot assignment.
    // PopupChoice continues to own its selection and popup list; only its
    // closed-row selected-value presentation is replaced here.
    if (view && view.caption && supports_control_caption(binding.row_kind) &&
        !(binding.caption_failed_revision == view.revision &&
          binding.caption_failures >= 4u)) {
        if (binding.caption_failed_revision != view.revision) {
            binding.caption_failed_revision = view.revision;
            binding.caption_failures = 0;
        }
        unsigned long seh{};
        if (assign_caption(addresses, binding, view.caption, seh)) {
            binding.caption_revision = view.revision;
            binding.caption_failures = 0;
        } else {
            ++binding.caption_failures;
            log_fault("caption assignment", page,
                binding.physical_index, seh);
        }
    }

    // Typed slider-table edits publish a dedicated per-row generation.
    // Revalidate the exact materializer-owned embedded SliderCtrl and backing
    // byte, then consume the generation after the first successful UI-thread
    // repaint. Failed assignments receive only the bounded retries below.
    // Authorization is independent of the optional disabled-appearance
    // transform backend.
    FormattedSliderRefreshBinding& slider = binding.slider_refresh;
    if (view && view.slider_value_revision != 0 &&
        slider.presentation_revision != view.slider_value_revision &&
        slider.candidate &&
        !(slider.failed_presentation_revision ==
                view.slider_value_revision &&
            slider.refresh_failures >= 4u)) {
        if (slider.failed_presentation_revision !=
                view.slider_value_revision) {
            slider.failed_presentation_revision =
                view.slider_value_revision;
            slider.refresh_failures = 0;
        }
        unsigned long seh{};
        const bool validated = validate_formatted_slider_refresh_binding(
            addresses, binding, seh);
        if (validated && refresh_native_slider_presentation(
                slider.slider, binding.byte_value)) {
            slider.presentation_revision =
                view.slider_value_revision;
            slider.refresh_failures = 0;
        } else {
            ++slider.refresh_failures;
            log_fault(validated
                    ? "formatted-slider immediate refresh"
                    : "formatted-slider refresh validation",
                page, binding.physical_index, seh);
        }
    }

    const detail::RowAvailabilityView availability =
        detail::row_availability_view(binding.presentation_slot);
    apply_binding_appearance(
        page,
        addresses,
        binding,
        availability);
    // SliderCtrl owns a second, direct Text_0 display object for its rendered
    // right-side value. Keep it independent from the row-label path so an
    // incompatible custom slider movie cannot disable proven interaction or
    // left-label availability behavior.
    apply_slider_value_appearance(
        page,
        addresses,
        binding,
        availability);
    return help_changed;
}

void initialize_formatted_slider_refresh_candidate(
    PhysicalBinding& binding) noexcept {
    if (!is_formatted_slider_row(binding.row_kind) ||
        !binding.controller || !binding.byte_value) {
        return;
    }
    FormattedSliderRefreshBinding& refresh =
        binding.slider_refresh;
    refresh.slider = static_cast<std::byte*>(binding.controller) +
        kEmbeddedSliderOffset;
    refresh.candidate = true;
}

void initialize_slider_value_candidate(
    const GameAddresses& addresses,
    PhysicalBinding& binding) noexcept {
    if (!binding.availability_supported ||
        !is_slider_row(binding.row_kind) || !binding.controller ||
        !binding.byte_value) {
        return;
    }

    SliderValueBinding& slider = binding.slider_value;
    slider.candidate = true;
    slider.slider = static_cast<std::byte*>(binding.controller) +
        kEmbeddedSliderOffset;

    std::uintptr_t bound_value{};
    unsigned long seh{};
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(&bound_value,
            static_cast<const std::byte*>(slider.slider) +
                kSliderBoundValueOffset,
            sizeof(bound_value));
#if defined(_MSC_VER)
    } __except (seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        (void)seh;
        return;
    }
#endif
    if (bound_value != reinterpret_cast<std::uintptr_t>(
            const_cast<std::uint8_t*>(binding.byte_value))) {
        return;
    }

    SliderRuntimeIdentity observed{};
    if (capture_slider_runtime_identity(
            slider.slider, addresses, observed, seh)) {
        slider.slider_vtable = observed.slider_vtable;
        slider.movie_proxy_vtable = observed.movie_proxy_vtable;
        slider.movie_owner = observed.movie_owner;
        slider.identity_captured = true;
    }
}

bool capture_physical_binding(
    void* native_row,
    const GameAddresses& addresses,
    const detail::CompiledRow& compiled_row,
    std::size_t presentation_slot,
    std::size_t physical_index,
    PhysicalBinding& captured,
    unsigned long& out_seh) noexcept {
    out_seh = 0;
    if (!native_row || !addresses.edit_property_vtable ||
        !addresses.text_input_controller_vtable ||
        !addresses.action_button_controller_vtable ||
        !addresses.title_bridge_complete()) {
        return false;
    }
    captured = {};
    captured.row_kind = compiled_row.kind;
    captured.byte_value = compiled_row.byte_value;
    captured.presentation_slot = presentation_slot;
    captured.physical_index = physical_index;
    captured.native_row = native_row;
#if defined(_MSC_VER)
    __try {
#endif
        auto* const row = static_cast<std::byte*>(native_row);
        std::memcpy(&captured.row_vtable, row,
            sizeof(captured.row_vtable));
        if (captured.row_vtable != addresses.edit_property_vtable) {
            return false;
        }
        std::memcpy(&captured.original_help,
            row + kPropertyRowHelpDirectOffset,
            sizeof(captured.original_help));
        std::memcpy(&captured.controller,
            row + kPropertyControllerOffset,
            sizeof(captured.controller));
        if (!captured.controller) return false;

        std::memcpy(&captured.controller_vtable,
            captured.controller,
            sizeof(captured.controller_vtable));
        if (!captured.controller_vtable) return false;
        void* expected_caption_accessor{};
        void* expected_action_accessor{};
        std::memcpy(&captured.caption_accessor,
            static_cast<const std::byte*>(captured.controller_vtable) +
                kControllerCaptionAccessorSlot * sizeof(void*),
            sizeof(captured.caption_accessor));
        std::memcpy(&expected_caption_accessor,
            static_cast<const std::byte*>(
                addresses.text_input_controller_vtable) +
                kControllerCaptionAccessorSlot * sizeof(void*),
            sizeof(expected_caption_accessor));
        std::memcpy(&expected_action_accessor,
            static_cast<const std::byte*>(
                addresses.action_button_controller_vtable) +
                kControllerCaptionAccessorSlot * sizeof(void*),
            sizeof(expected_action_accessor));
        if (!captured.caption_accessor) {
            return false;
        }
        const bool action_controller =
            captured.controller_vtable ==
                addresses.action_button_controller_vtable &&
            captured.caption_accessor == expected_action_accessor;
        const bool caption_controller =
            !action_controller &&
            captured.caption_accessor == expected_caption_accessor;
        const bool expected_action_controller =
            uses_action_controller(compiled_row.kind);
        if ((expected_action_controller && !action_controller) ||
            (!expected_action_controller && !caption_controller)) {
            return false;
        }
        captured.label_target = uses_widget_text_pair(compiled_row.kind)
            ? LabelTargetKind::widget_unresolved
            : LabelTargetKind::ordinary_row;

        auto* const movie_context =
            static_cast<std::byte*>(captured.controller) +
            kControllerMovieContextOffset;
        captured.row_movie_proxy = movie_context;
        void* self0{};
        void* self1{};
        void* self2{};
        std::uint32_t value_flags{};
        std::memcpy(&captured.row_movie_proxy_vtable,
            movie_context,
            sizeof(captured.row_movie_proxy_vtable));
        std::memcpy(&self0,
            movie_context + kSceneProxyFirstSelfOffset,
            sizeof(self0));
        std::memcpy(&self1,
            movie_context + kSceneProxySecondSelfOffset,
            sizeof(self1));
        std::memcpy(&self2,
            movie_context + kSceneProxyThirdSelfOffset,
            sizeof(self2));
        std::memcpy(&captured.row_movie_owner,
            movie_context + kSceneProxyOwnerOffset,
            sizeof(captured.row_movie_owner));
        std::memcpy(&value_flags,
            movie_context + kSceneProxyValueOffset +
                kScaleformValueFlagsOffset,
            sizeof(value_flags));
        const bool movie_context_valid =
            captured.row_movie_proxy_vtable &&
            self0 == movie_context && self1 == movie_context &&
            self2 == movie_context && captured.row_movie_owner &&
            (value_flags & kScaleformValueKindMask) ==
                kScaleformValueKindDisplayObject;
        if (!movie_context_valid) return false;

        const bool availability_candidate =
            supports_runtime_availability(compiled_row.kind);
        if (availability_candidate) {
            std::uint8_t constructed_interactive{};
            std::memcpy(&constructed_interactive,
                row + kPropertyEnabledOffset,
                sizeof(constructed_interactive));
            const RowAvailabilityProfile* profile{};
            unsigned long profile_seh{};
            const bool exact = availability_runtime_exact(
                addresses, profile, profile_seh);
            if (profile && exact &&
                (!uses_authored_availability_twins(compiled_row.kind) ||
                    compiled_row.byte_value) &&
                availability_controller_matches(
                    addresses, *profile, captured) &&
                constructed_interactive <= 1u &&
                constructed_interactive ==
                    (compiled_row.enabled ? 1u : 0u)) {
                captured.construction_interactive =
                    constructed_interactive;
                captured.installed_interactive = constructed_interactive;
                captured.availability_supported = true;
                captured.applied_appearance =
                    static_cast<std::uint8_t>(constructed_interactive != 0
                        ? EffectiveAvailabilityAppearance::normal
                        : EffectiveAvailabilityAppearance::disabled);
                captured.applied_value = compiled_row.byte_value
                    ? *compiled_row.byte_value
                    : 0xFF;
            }
        }
        initialize_formatted_slider_refresh_candidate(captured);
        initialize_slider_value_candidate(addresses, captured);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

} // namespace

bool native_row_presentation_build_supported(
    const ModuleView& game) noexcept {
    const EldenRingBuildIdentity* const identity = find_elden_ring_build(
        game.timestamp(), game.image_size());
    if (!identity) return false;
    const SelectedHelpPresenterAddressProfile* const profile =
        find_selected_help_presenter_address_profile(identity->build);
    return profile && profile->image_size == game.image_size();
}

bool native_row_availability_build_supported(
    const ModuleView& game) noexcept {
    const EldenRingBuildIdentity* const identity = find_elden_ring_build(
        game.timestamp(), game.image_size());
    if (!identity) return false;
    const RowAvailabilityProfile* const profile = availability_profile(
        identity->build, game.image_size());
    if (!profile || !game.base()) return false;

    unsigned long seh{};
    const bool exact = availability_profile_exact(
        game.base(), game.image_size(), *profile, seh);
    if (!exact) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Runtime row availability rejected exact native/Scaleform profile build=%s seh=0x%08lX",
            elden_ring_build_name(profile->build).data(),
            seh);
        return false;
    }
    return true;
}

void begin_native_row_presentation_page(
    void* page,
    const GameAddresses& addresses) noexcept {
    void* vtable{};
    std::size_t count{};
    unsigned long seh{};
    if (!read_page_identity(page, addresses, vtable, count, seh)) return;

    std::lock_guard lock(g_mutex);
    PageBinding* binding = find_page(page);
    if (!binding) {
        const auto empty = std::find_if(
            g_pages.begin(), g_pages.end(),
            [](const PageBinding& candidate) {
                return !candidate.active;
            });
        if (empty != g_pages.end()) {
            binding = &*empty;
        } else {
            const auto oldest = std::min_element(
                g_pages.begin(), g_pages.end(),
                [](const PageBinding& left, const PageBinding& right) {
                    return left.use_serial < right.use_serial;
                });
            binding = oldest == g_pages.end() ? nullptr : &*oldest;
        }
    }
    if (!binding) return;
    if (binding->active) {
        // A physical panel can be reconstructed in place, and the bounded
        // tracker can also evict an old epoch. Restore every pointer that is
        // still demonstrably ours before replacing its ownership record.
        restore_page_owned_state(*binding, addresses);
    }
    std::uint64_t epoch = ++g_epoch;
    if (epoch == 0) epoch = ++g_epoch;
    *binding = {
        .page = page,
        .page_vtable = vtable,
        .epoch = epoch,
        .use_serial = ++g_use_serial,
        .rows = {},
        .row_count = 0,
        .active = true,
    };
}

RowPresentationMaterializationTicket
begin_native_row_presentation_materialization(
    void* page,
    const GameAddresses& addresses,
    const detail::CompiledRow& row) noexcept {
    RowPresentationMaterializationTicket ticket{};
    if (row.presentation_slot == detail::invalid_compiled_index) return ticket;

    void* vtable{};
    std::size_t count{};
    unsigned long seh{};
    if (!read_page_identity(page, addresses, vtable, count, seh)) return ticket;

    std::lock_guard lock(g_mutex);
    PageBinding* binding = find_page(page);
    if (!binding || binding->page_vtable != vtable) return ticket;
    ticket.page = page;
    ticket.presentation_slot = row.presentation_slot;
    ticket.previous_count = count;
    ticket.page_epoch = binding->epoch;
    ticket.armed = true;
    return ticket;
}

void complete_native_row_presentation_materialization(
    void* page,
    const GameAddresses& addresses,
    const detail::CompiledRow& row,
    RowPresentationMaterializationTicket ticket,
    bool constructor_succeeded) noexcept {
    if (!ticket.armed || !constructor_succeeded || page != ticket.page ||
        row.presentation_slot != ticket.presentation_slot) {
        return;
    }

    void* page_vtable{};
    std::size_t count{};
    unsigned long seh{};
    if (!read_page_identity(page, addresses, page_vtable, count, seh) ||
        count != ticket.previous_count + 1u ||
        ticket.previous_count >= kMaximumPropertyRows) {
        log_fault("constructor count delta", page,
            ticket.previous_count, seh);
        return;
    }

    auto* const native_row = static_cast<std::byte*>(page) +
        kPropertyCollectionOffset +
        ticket.previous_count * kPropertyRowStride;
    PhysicalBinding captured{};
    if (!capture_physical_binding(
            native_row,
            addresses,
            row,
            row.presentation_slot,
            ticket.previous_count,
            captured,
            seh)) {
        log_fault("constructor capture", page,
            ticket.previous_count, seh);
        return;
    }

    const bool availability_candidate =
        supports_runtime_availability(row.kind);
    if (availability_candidate &&
        availability_profile(addresses) &&
        !captured.availability_supported) {
        log_fault("availability constructor capture", page,
            ticket.previous_count, seh);
    }

    std::lock_guard lock(g_mutex);
    PageBinding* binding = find_page(page);
    if (!binding || binding->epoch != ticket.page_epoch ||
        binding->page_vtable != page_vtable) {
        return;
    }
    const auto existing = std::find_if(
        binding->rows.begin(),
        binding->rows.begin() + binding->row_count,
        [&](const PhysicalBinding& candidate) {
            return candidate.physical_index == captured.physical_index;
        });
    if (existing == binding->rows.begin() + binding->row_count) {
        if (binding->row_count >= binding->rows.size()) {
            log_fault("binding capacity", page, captured.physical_index);
            return;
        }
        binding->rows[binding->row_count++] = captured;
    } else {
        // Materialization is authoritative for the embedded SliderCtrl. Never
        // discard a previous registry reference merely because the same
        // physical row index was reconstructed in place.
        if (existing->slider_value.attached) {
            const char* operation{};
            unsigned long detach_seh{};
            if (!detach_slider_transform(
                    addresses, *existing, operation, detach_seh)) {
                log_fault(
                    operation ? operation
                              : "slider value rematerialization detach",
                    page,
                    existing->physical_index,
                    detach_seh);
            }
        }
        if (binding->pending_help.armed &&
            binding->pending_help.physical_index ==
                captured.physical_index) {
            binding->pending_help = {};
        }
        *existing = captured;
    }
}

void apply_native_row_availability_pre_frame(
    void* page,
    const GameAddresses& addresses) noexcept {
    void* page_vtable{};
    std::size_t count{};
    unsigned long seh{};
    if (!read_page_identity(page, addresses, page_vtable, count, seh)) return;

    std::lock_guard lock(g_mutex);
    PageBinding* binding = find_page(page);
    if (!binding || binding->page_vtable != page_vtable) return;
    binding->use_serial = ++g_use_serial;
    for (std::size_t index = 0; index < binding->row_count; ++index) {
        PhysicalBinding& row = binding->rows[index];
        if (!row.availability_supported || row.physical_index >= count ||
            row.native_row != static_cast<std::byte*>(page) +
                kPropertyCollectionOffset +
                row.physical_index * kPropertyRowStride) {
            continue;
        }

        const detail::RowAvailabilityView view =
            detail::row_availability_view(row.presentation_slot);
        if (!view) continue;
        if (row.interaction_failed_revision != view.revision) {
            row.interaction_failed_revision = view.revision;
            row.interaction_failures = 0;
        } else if (row.interaction_failures >= 4u ||
            row.interaction_conflict) {
            continue;
        }

        seh = 0;
        if (install_interactivity(
                row, view.interactive, view.revision, seh)) {
            row.interaction_failures = 0;
        } else {
            ++row.interaction_failures;
            log_fault(row.interaction_conflict
                    ? "availability ownership conflict"
                    : "availability pre-frame publication",
                page,
                row.physical_index,
                seh);
        }
    }
}

SelectedHelpRefreshTicket apply_native_row_presentations(
    void* page,
    const GameAddresses& addresses) noexcept {
    void* page_vtable{};
    std::size_t count{};
    unsigned long seh{};
    if (!read_page_identity(page, addresses, page_vtable, count, seh)) {
        return {};
    }

    std::lock_guard lock(g_mutex);
    PageBinding* binding = find_page(page);
    if (!binding || binding->page_vtable != page_vtable) return {};
    binding->use_serial = ++g_use_serial;
    // Observe every materializer-authorized SliderCtrl after the native frame
    // and reject transient in-page Text_0 aliases before any value transform
    // is captured or written. Two identical observations are required.
    prepare_slider_bindings(page, addresses, *binding, count);
    std::uint32_t changed_help_rows{};
    for (std::size_t index = 0; index < binding->row_count; ++index) {
        PhysicalBinding& row = binding->rows[index];
        if (row.physical_index >= count ||
            row.native_row != static_cast<std::byte*>(page) +
                kPropertyCollectionOffset +
                row.physical_index * kPropertyRowStride) {
            continue;
        }
        if (apply_binding(page, addresses, row) &&
            row.physical_index < 32u) {
            changed_help_rows |= std::uint32_t{1} << row.physical_index;
        }
    }
    if (changed_help_rows != 0) {
        std::uint32_t selected_count{};
        std::uint32_t selected_index{};
        seh = 0;
        if (read_selected_row(
                page, selected_count, selected_index, seh) &&
            (changed_help_rows &
                (std::uint32_t{1} << selected_index)) != 0) {
            const auto selected = std::find_if(
                binding->rows.begin(),
                binding->rows.begin() + binding->row_count,
                [selected_index](const PhysicalBinding& row) {
                    return row.physical_index == selected_index;
                });
            if (selected != binding->rows.begin() + binding->row_count &&
                selected->installed_help && selected->help_revision != 0) {
                binding->pending_help = {
                    .page = page,
                    .native_row = selected->native_row,
                    .source = selected->installed_help,
                    .physical_index = selected->physical_index,
                    .presentation_slot = selected->presentation_slot,
                    .page_epoch = binding->epoch,
                    .help_revision = selected->help_revision,
                    .armed = true,
                };
            }
        }
    }
    return binding->pending_help;
}

void publish_native_selected_help(
    SelectedHelpRefreshTicket ticket,
    const GameAddresses& addresses) noexcept {
    if (!ticket.armed || !addresses.selected_help_presenter ||
        g_selected_help_presenter_active) {
        return;
    }

    void* page_vtable{};
    std::size_t physical_count{};
    unsigned long seh{};
    if (!read_page_identity(
            ticket.page,
            addresses,
            page_vtable,
            physical_count,
            seh)) {
        return;
    }
    std::uint32_t selected_count{};
    std::uint32_t selected_index{};
    const bool selected_valid = read_selected_row(
        ticket.page, selected_count, selected_index, seh);

    bool call_presenter = false;
    {
        std::lock_guard lock(g_mutex);
        PageBinding* const binding = find_page(ticket.page);
        if (!binding || binding->page_vtable != page_vtable ||
            !binding->pending_help.armed ||
            binding->pending_help.page_epoch != ticket.page_epoch ||
            binding->pending_help.help_revision != ticket.help_revision ||
            binding->pending_help.physical_index != ticket.physical_index ||
            binding->pending_help.presentation_slot !=
                ticket.presentation_slot ||
            binding->pending_help.native_row != ticket.native_row ||
            binding->pending_help.source != ticket.source) {
            return;
        }

        const SelectedHelpRefreshDecision decision =
            select_help_refresh_decision(
                ticket,
                false,
                binding->epoch,
                selected_valid ? selected_count : 0u,
                selected_valid ? selected_index : 0u,
                kMaximumPropertyRows);
        if (decision == SelectedHelpRefreshDecision::defer ||
            decision == SelectedHelpRefreshDecision::none) {
            return;
        }
        if (decision == SelectedHelpRefreshDecision::discard ||
            ticket.physical_index >= physical_count) {
            binding->pending_help = {};
            return;
        }

        const auto row = std::find_if(
            binding->rows.begin(),
            binding->rows.begin() + binding->row_count,
            [&](const PhysicalBinding& candidate) {
                return candidate.physical_index == ticket.physical_index &&
                    candidate.presentation_slot == ticket.presentation_slot &&
                    candidate.native_row == ticket.native_row &&
                    candidate.help_revision == ticket.help_revision &&
                    candidate.installed_help == ticket.source;
            });
        seh = 0;
        if (row == binding->rows.begin() + binding->row_count ||
            row->native_row != static_cast<std::byte*>(ticket.page) +
                kPropertyCollectionOffset +
                ticket.physical_index * kPropertyRowStride ||
            !direct_help_source_matches(*row, ticket.source, seh)) {
            binding->pending_help = {};
            return;
        }

        // Consume before entering native code. The presenter synchronously
        // clones the selected row source and may re-enter other UI helpers.
        binding->pending_help = {};
        call_presenter = true;
    }

    if (!call_presenter) return;
    g_selected_help_presenter_active = true;
    seh = 0;
    const bool called = call_selected_help_presenter(
        addresses.selected_help_presenter, ticket.page, seh);
    g_selected_help_presenter_active = false;
    if (!called) {
        log_fault(
            "selected-help presenter",
            ticket.page,
            ticket.physical_index,
            seh);
    }
}

void retire_native_row_presentation_page(
    void* page,
    const GameAddresses& addresses) noexcept {
    if (!page) return;
    std::lock_guard lock(g_mutex);
    for (PageBinding& binding : g_pages) {
        if (binding.active && binding.page == page) {
            restore_page_owned_state(binding, addresses);
            binding = {};
        }
    }
}

void reset_native_row_presentations(
    const GameAddresses& addresses) noexcept {
    std::lock_guard lock(g_mutex);
    for (PageBinding& binding : g_pages) {
        if (binding.active) restore_page_owned_state(binding, addresses);
    }
    g_pages.fill({});
    g_epoch = 0;
    g_use_serial = 0;
    g_fault_logs.store(0, std::memory_order_release);
}

} // namespace erui::native
