#include "module.hpp"

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
#include <limits>
#include <utility>

namespace {

// This mutation proof is deliberately isolated from ERNativeUI.dll. It exists
// only to prove that native MenuOptionCategory records can be copy-constructed
// up to the fixed vector's validated capacity while the stock top constructor
// is still populating that vector.
constexpr std::uint32_t kEldenRing271Timestamp = 0x6A96B418;
constexpr std::size_t kEldenRing271ImageSize = 0x5E0DA00;
constexpr std::uintptr_t kTopConstructorRva = 0x9672C0;
constexpr std::uintptr_t kEarlyAppendSeamRva = 0x96773F;
constexpr std::uintptr_t kFirstCountPublicationRva = 0x967757;
constexpr std::uintptr_t kSelectorGeometryDispatchRva = 0x967935;
constexpr std::uintptr_t kPostGeometryObservationSeamRva = 0x96793E;
constexpr std::uintptr_t kCountPropagationDispatchRva = 0x96795B;
constexpr std::uintptr_t kPostConstructorObserverRva = 0x8074A7;
constexpr std::uintptr_t kCategoryAppendRva = 0x968CF0;
constexpr std::uintptr_t kCategoryCountGetterRva = 0x968DA0;
constexpr std::uintptr_t kSelectorSetActiveCountRva = 0x739C10;
constexpr std::uintptr_t kSelectorGeometryEnumeratorRva = 0x738AB0;
constexpr std::uintptr_t kResolvePathRva = 0x74B140;
constexpr std::uintptr_t kValueExistsRva = 0x733FA0;
constexpr std::uintptr_t kScaleformDestructorRva = 0xD81600;
constexpr std::uintptr_t kTopVtableRva = 0x2B16B48;
constexpr std::uintptr_t kCategoryListVtableRva = 0x2B16B10;
constexpr std::uintptr_t kCategoryRecordVtableRva = 0x2AD6F20;
constexpr std::uintptr_t kSelectorVtableRva = 0x2A94438;
constexpr std::uintptr_t kSceneObjProxyVtableRva = 0x2A97AF0;
constexpr std::uintptr_t kSceneObjProxyValueVtableRva = 0x2A97AD8;
constexpr std::uintptr_t kSceneObjProxyGetValueRva = 0x74C930;
constexpr std::uintptr_t kAs3ValueInterfaceVtableRva = 0x2CBF988;
constexpr std::uintptr_t kAs3ValueGetMemberRva = 0x10DDD10;
constexpr std::uintptr_t kAs3ValueSetMemberRva = 0x10DE0D0;
constexpr std::uintptr_t kAs3ValueInvokeRva = 0x10DE510;
constexpr std::uintptr_t kAs3ValueGetMovieRva = 0x10E26D0;
constexpr std::uintptr_t kAs3MovieVtableRva = 0x2CBE5C0;
constexpr std::uintptr_t kAs3MovieCreateObjectRva = 0x10D4460;

constexpr std::size_t kSelectorOffset = 0x0A38;
constexpr std::size_t kMovieContextOffset = 0x0120;
constexpr std::size_t kSelectorActiveCountOffset = 0x00D0;
constexpr std::size_t kSelectorSelectedIndexOffset = 0x00D4;
constexpr std::size_t kSelectorAxis0CountOffset = 0x00D8;
constexpr std::size_t kSelectorAxis1CountOffset = 0x00DC;
constexpr std::size_t kCategoryListOffset = 0x1200;
constexpr std::size_t kCategoryStorageOffset = 0x1208;
constexpr std::size_t kCategoryCountOffset = 0x1760;
constexpr std::size_t kCompositeOffset = 0x1768;
constexpr std::size_t kTopControllerOffset = 0x1870;
constexpr std::size_t kCategoryRecordSize = 0x88;
constexpr std::size_t kCategoryTitleOffset = 0x08;
constexpr std::size_t kCategoryIdOffset = 0x40;
constexpr std::size_t kCategoryPresentationOffset = 0x48;
constexpr std::size_t kCategoryPresentationSize = 0x40;
constexpr std::size_t kSceneObjProxyValueOffset = 0x28;
constexpr std::size_t kNativeCategoryCapacity = 10;
#if !defined(ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET)
#error "The tab-slot append probe requires a research target"
#endif
constexpr std::uint32_t kRequiredMinimumTabSlots =
    ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET;
static_assert(
    kRequiredMinimumTabSlots == 9U || kRequiredMinimumTabSlots == 10U);
constexpr std::size_t kRequiredAppendCount =
    kRequiredMinimumTabSlots - 8U;
constexpr std::size_t kRequiredCellsPerStrip =
    kRequiredMinimumTabSlots - 7U;
constexpr std::size_t kRequiredLiveCellProofCount =
    kRequiredCellsPerStrip * 2U;
constexpr std::size_t kMaximumLiveCellProofCount = 6U;
static_assert(kRequiredLiveCellProofCount <= kMaximumLiveCellProofCount);
constexpr DWORD kHostWaitMilliseconds = 5000;
constexpr DWORD kHostPollMilliseconds = 25;
constexpr std::uint32_t kAllEarlyGateBits = 0xFFFF;
constexpr std::uint32_t kAllLateGateBits = 0x03FF;
#if defined(ER_NATIVE_UI_TAB_SLOT_APPEND_RESEARCH_MUTATION)
constexpr bool kMutationEnabled = true;
#else
constexpr bool kMutationEnabled = false;
#endif

using CategoryAppendFn = void(__fastcall*)(void* storage, const void* source_record);
using ResolvePathFn = void*(__fastcall*)(
    void* movie_context,
    void* destination,
    const char* path_format,
    ...);
using ValueExistsFn = bool(__fastcall*)(void* scene_obj_proxy);
using ScaleformDestructorFn = void(__fastcall*)(void* nested_value);

constexpr std::array<std::int32_t, 7> kExpectedPrefixCategories{
    0, 1, 2, 3, 5, 6, 7};
constexpr std::array<std::int32_t, kNativeCategoryCapacity>
    kExpectedExpandedCategories{0, 1, 2, 3, 5, 6, 7, 8, 8, 8};

struct RecordScalars {
    const wchar_t* title{};
    std::int32_t category{-1};
    std::array<std::byte, kCategoryPresentationSize> presentation{};
};

struct MovieFingerprint {
    void* proxy_owner{};
    void* object_interface{};
    std::uint32_t flags{};
    void* object_payload{};
    void* auxiliary{};
    void* owner_link{};
    void* movie_root{};
};

struct ConstructorContext {
    void* top{};
    void* scene_proxy{};
    MovieFingerprint movie{};
    bool active{};
    bool title_menu{};
    bool early_seam_seen{};
    bool late_seam_seen{};
    bool observation_only{};
    bool extra_append_attempted{};
    bool extra_append_committed{};
    bool cell_proofs_exact{};
    bool cell_cleanup_exact{};
    bool cell_payloads_distinct{};
    std::uint32_t append_checkpoints{};
    std::size_t cell_probe_count{};
    unsigned long cell_probe_seh{};
    std::array<std::uint8_t, kMaximumLiveCellProofCount> cell_probe_bits{};
    std::array<MovieFingerprint, kMaximumLiveCellProofCount> cell_movies{};
    std::uint32_t early_gate_bits{};
    std::uint32_t late_gate_bits{};
    std::uint64_t count_before{};
    std::int32_t active_before{-1};
    std::int32_t late_active{-1};
    std::int32_t late_axis0{};
    std::int32_t late_axis1{};
};

struct alignas(16) CellProbeStorage {
    std::array<std::byte, 0x60> proxy{};
    bool constructed{};
    bool poisoned{};
    std::array<std::byte, 14> alignment_padding{};
};

struct CellProbeResult {
    MovieFingerprint movie{};
    bool resolver_returned_destination{};
    bool existence_call_exact{};
    bool exists{};
    bool fingerprint_exact{};
    bool lineage_exact{};
    bool cleanup_exact{};
    unsigned long seh_code{};
};

static_assert(alignof(CellProbeStorage) == 16);
static_assert(sizeof(CellProbeStorage) == 0x70);

HMODULE g_module{};
HANDLE g_log_file{INVALID_HANDLE_VALUE};
SRWLOCK g_log_lock = SRWLOCK_INIT;
std::array<wchar_t, 32768> g_log_path{};
erui::native::ModuleView g_game{};
SafetyHookMid g_early_append_hook{};
SafetyHookMid g_post_geometry_observer_hook{};
SafetyHookMid g_post_constructor_hook{};
CategoryAppendFn g_category_append{};
ResolvePathFn g_resolve_path{};
ValueExistsFn g_value_exists{};
ScaleformDestructorFn g_destroy_scaleform_value{};
std::atomic_bool g_callbacks_armed{};
std::atomic_bool g_hooks_exposed{};
std::atomic_uint32_t g_successful_appends{};
std::atomic_uint32_t g_successful_observations{};
thread_local bool g_inside_seam_append{};
thread_local ConstructorContext g_constructor_context{};
thread_local std::array<
    CellProbeStorage, kMaximumLiveCellProofCount> g_cell_probe_storage{};

bool guarded_copy(void* destination, const void* source, std::size_t size) noexcept {
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

const std::byte* offset_address(
    const void* base,
    std::size_t offset) noexcept {
    if (!base) return nullptr;
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (offset > (std::numeric_limits<std::uintptr_t>::max)() - address) {
        return nullptr;
    }
    return reinterpret_cast<const std::byte*>(address + offset);
}

std::byte* offset_address(void* base, std::size_t offset) noexcept {
    return const_cast<std::byte*>(offset_address(
        static_cast<const void*>(base), offset));
}

const std::byte* record_address(
    const void* top,
    std::size_t index) noexcept {
    if (index >= kNativeCategoryCapacity) return nullptr;
    return offset_address(
        top, kCategoryStorageOffset + index * kCategoryRecordSize);
}

bool read_record_scalars(
    const void* record,
    RecordScalars& out) noexcept {
    return record &&
        guarded_read(offset_address(record, kCategoryTitleOffset), out.title) &&
        guarded_read(offset_address(record, kCategoryIdOffset), out.category) &&
        guarded_copy(
            out.presentation.data(),
            offset_address(record, kCategoryPresentationOffset),
            out.presentation.size());
}

bool record_has_identity(
    const void* top,
    std::size_t index,
    std::int32_t expected_category) noexcept {
    const std::byte* const record = record_address(top, index);
    void* vtable{};
    std::int32_t category{-1};
    return record && guarded_read(record, vtable) &&
        vtable == g_game.at_rva(kCategoryRecordVtableRva) &&
        guarded_read(offset_address(record, kCategoryIdOffset), category) &&
        category == expected_category;
}

bool records_match_scalars(
    const void* left,
    const void* right) noexcept {
    RecordScalars left_fields{};
    RecordScalars right_fields{};
    return read_record_scalars(left, left_fields) &&
        read_record_scalars(right, right_fields) &&
        left_fields.title == right_fields.title &&
        left_fields.category == right_fields.category &&
        left_fields.presentation == right_fields.presentation;
}

bool record_matches_snapshot(
    const void* record,
    const RecordScalars& expected) noexcept {
    RecordScalars actual{};
    return read_record_scalars(record, actual) &&
        actual.title == expected.title &&
        actual.category == expected.category &&
        actual.presentation == expected.presentation;
}

bool exact_prefix_records(const void* top) noexcept {
    for (std::size_t index = 0; index < kExpectedPrefixCategories.size();
         ++index) {
        if (!record_has_identity(
                top, index, kExpectedPrefixCategories[index])) {
            return false;
        }
    }
    return true;
}

bool exact_stock_records(const void* top) noexcept {
    return exact_prefix_records(top) && record_has_identity(top, 7, 8);
}

bool exact_records_through(
    const void* top,
    std::size_t count) noexcept {
    if (count < 8U || count > kRequiredMinimumTabSlots ||
        count > kExpectedExpandedCategories.size()) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (!record_has_identity(
                top, index, kExpectedExpandedCategories[index])) {
            return false;
        }
    }
    for (std::size_t index = 8U; index < count; ++index) {
        if (!records_match_scalars(
                record_address(top, 7), record_address(top, index))) {
            return false;
        }
    }
    return true;
}

bool exact_expanded_records(const void* top) noexcept {
    return exact_records_through(top, kRequiredMinimumTabSlots);
}

bool read_geometry(
    const void* top,
    std::int32_t& axis0,
    std::int32_t& axis1,
    std::int64_t& cell_count) noexcept {
    const std::byte* const selector = offset_address(top, kSelectorOffset);
    void* selector_vtable{};
    if (!selector || !guarded_read(selector, selector_vtable) ||
        selector_vtable != g_game.at_rva(kSelectorVtableRva) ||
        !guarded_read(
            offset_address(selector, kSelectorAxis0CountOffset), axis0) ||
        !guarded_read(
            offset_address(selector, kSelectorAxis1CountOffset), axis1) ||
        axis0 <= 0 || axis0 > 32 || axis1 <= 0 || axis1 > 64 ||
        (axis0 != 1 && axis1 != 1)) {
        return false;
    }
    cell_count = static_cast<std::int64_t>(axis0) * axis1;
    return cell_count ==
        static_cast<std::int64_t>(kRequiredMinimumTabSlots + 1U);
}

bool read_movie_fingerprint(
    const void* scene_proxy,
    MovieFingerprint& fingerprint) noexcept {
    fingerprint = {};
    if (!scene_proxy) return false;

    void* proxy_vtable{};
    void* proxy_getter{};
    void* proxy_self0{};
    void* proxy_self1{};
    void* proxy_self2{};
    void* proxy_owner{};
    void* value_vtable{};
    void* object_interface{};
    void* object_interface_vtable{};
    void* get_member{};
    void* set_member{};
    void* invoke{};
    void* get_movie{};
    void* object_payload{};
    void* auxiliary{};
    void* owner_link{};
    void* movie_root{};
    void* movie_vtable{};
    void* create_object{};
    std::uint32_t flags{};
    const std::byte* const proxy =
        static_cast<const std::byte*>(scene_proxy);
    const std::byte* const wrapper =
        proxy + kSceneObjProxyValueOffset;

    const bool exact = guarded_read(proxy, proxy_vtable) &&
        proxy_vtable == g_game.at_rva(kSceneObjProxyVtableRva) &&
        guarded_read(
            offset_address(proxy_vtable, sizeof(void*)), proxy_getter) &&
        proxy_getter == g_game.at_rva(kSceneObjProxyGetValueRva) &&
        guarded_read(proxy + 0x08, proxy_self0) &&
        proxy_self0 == scene_proxy &&
        guarded_read(proxy + 0x10, proxy_self1) &&
        proxy_self1 == scene_proxy &&
        guarded_read(proxy + 0x18, proxy_self2) &&
        proxy_self2 == scene_proxy &&
        guarded_read(proxy + 0x20, proxy_owner) &&
        proxy_owner != nullptr &&
        guarded_read(wrapper, value_vtable) &&
        value_vtable == g_game.at_rva(kSceneObjProxyValueVtableRva) &&
        guarded_read(wrapper + 0x18, object_interface) &&
        object_interface != nullptr &&
        guarded_read(object_interface, object_interface_vtable) &&
        object_interface_vtable ==
            g_game.at_rva(kAs3ValueInterfaceVtableRva) &&
        guarded_read(
            offset_address(object_interface_vtable, 0x20), get_member) &&
        get_member == g_game.at_rva(kAs3ValueGetMemberRva) &&
        guarded_read(
            offset_address(object_interface_vtable, 0x28), set_member) &&
        set_member == g_game.at_rva(kAs3ValueSetMemberRva) &&
        guarded_read(
            offset_address(object_interface_vtable, 0x30), invoke) &&
        invoke == g_game.at_rva(kAs3ValueInvokeRva) &&
        guarded_read(
            offset_address(object_interface_vtable, 0x178), get_movie) &&
        get_movie == g_game.at_rva(kAs3ValueGetMovieRva) &&
        guarded_read(wrapper + 0x20, flags) &&
        (flags & 0x8FU) == 0x0AU &&
        guarded_read(wrapper + 0x28, object_payload) &&
        object_payload != nullptr &&
        guarded_read(wrapper + 0x30, auxiliary) &&
        guarded_read(offset_address(object_interface, 0x08), owner_link) &&
        owner_link != nullptr &&
        guarded_read(offset_address(owner_link, 0x18), movie_root) &&
        movie_root != nullptr &&
        guarded_read(movie_root, movie_vtable) &&
        movie_vtable == g_game.at_rva(kAs3MovieVtableRva) &&
        guarded_read(
            offset_address(movie_vtable, 0x170), create_object) &&
        create_object == g_game.at_rva(kAs3MovieCreateObjectRva);
    if (!exact) return false;

    fingerprint.proxy_owner = proxy_owner;
    fingerprint.object_interface = object_interface;
    fingerprint.flags = flags;
    fingerprint.object_payload = object_payload;
    fingerprint.auxiliary = auxiliary;
    fingerprint.owner_link = owner_link;
    fingerprint.movie_root = movie_root;
    return true;
}

bool same_movie_fingerprint(
    const MovieFingerprint& left,
    const MovieFingerprint& right) noexcept {
    return left.proxy_owner != nullptr && left.object_interface != nullptr &&
        left.object_payload != nullptr && left.owner_link != nullptr &&
        left.movie_root != nullptr &&
        left.proxy_owner == right.proxy_owner &&
        left.object_interface == right.object_interface &&
        left.flags == right.flags &&
        left.object_payload == right.object_payload &&
        left.auxiliary == right.auxiliary &&
        left.owner_link == right.owner_link &&
        left.movie_root == right.movie_root;
}

bool same_movie_lineage(
    const MovieFingerprint& cell,
    const MovieFingerprint& movie) noexcept {
    // The payload identifies the resolved display object, so it must differ
    // between the root proxy and an individual tab cell. The temporary
    // SceneObjProxy owner, value flags, and auxiliary field are properties of
    // that child value rather than Movie identity. The interface -> owner ->
    // MovieRoot chain is the authoritative shared lineage.
    return cell.object_interface != nullptr && cell.owner_link != nullptr &&
        cell.movie_root != nullptr &&
        cell.object_interface == movie.object_interface &&
        cell.owner_link == movie.owner_link &&
        cell.movie_root == movie.movie_root;
}

void remember_first_seh(
    unsigned long candidate,
    unsigned long& destination) noexcept {
    if (destination == 0 && candidate != 0) destination = candidate;
}

bool call_resolve_path(
    void* movie_context,
    void* destination,
    const char* path,
    void*& result,
    unsigned long& seh_code) noexcept {
    result = nullptr;
    seh_code = 0;
    if (!g_resolve_path || !movie_context || !destination || !path) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        result = g_resolve_path(movie_context, destination, path);
        return true;
#if defined(_MSC_VER)
    } __except (seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool call_value_exists(
    void* scene_proxy,
    bool& exists,
    unsigned long& seh_code) noexcept {
    exists = false;
    seh_code = 0;
    if (!g_value_exists || !scene_proxy) return false;
#if defined(_MSC_VER)
    __try {
#endif
        exists = g_value_exists(scene_proxy);
        return true;
#if defined(_MSC_VER)
    } __except (seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool destroy_resolved_value(
    void* scene_proxy,
    unsigned long& seh_code) noexcept {
    seh_code = 0;
    if (!g_destroy_scaleform_value || !scene_proxy) return false;
    void* const nested_value = offset_address(
        scene_proxy, kSceneObjProxyValueOffset);
    if (!nested_value) return false;
#if defined(_MSC_VER)
    __try {
#endif
        g_destroy_scaleform_value(nested_value);
        return true;
#if defined(_MSC_VER)
    } __except (seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

CellProbeResult probe_live_cell(
    void* movie_context,
    const char* path,
    const MovieFingerprint& current_movie,
    CellProbeStorage& storage) noexcept {
    CellProbeResult proof{};
    if (storage.poisoned || storage.constructed || !movie_context || !path) {
        return proof;
    }

    storage.proxy.fill(std::byte{});
    void* result{};
    unsigned long seh_code{};
    const bool resolver_call_exact = call_resolve_path(
        movie_context, storage.proxy.data(), path, result, seh_code);
    remember_first_seh(seh_code, proof.seh_code);
    if (!resolver_call_exact || result != storage.proxy.data()) {
        // A return mismatch leaves construction state ambiguous. Retain this
        // process-lifetime TLS buffer and never try to reuse or destroy it.
        storage.poisoned = true;
        return proof;
    }

    storage.constructed = true;
    proof.resolver_returned_destination = true;
    proof.existence_call_exact = call_value_exists(
        storage.proxy.data(), proof.exists, seh_code);
    remember_first_seh(seh_code, proof.seh_code);
    proof.fingerprint_exact = read_movie_fingerprint(
        storage.proxy.data(), proof.movie);
    proof.lineage_exact = proof.fingerprint_exact &&
        same_movie_lineage(proof.movie, current_movie);

    // ResolvePath constructs an owning CSScaleformValue even when the final
    // member is Undefined. Destruction is therefore unconditional once the
    // resolver returned our destination.
    proof.cleanup_exact = destroy_resolved_value(
        storage.proxy.data(), seh_code);
    remember_first_seh(seh_code, proof.seh_code);
    if (proof.cleanup_exact) {
        storage.constructed = false;
        storage.proxy.fill(std::byte{});
    } else {
        // Never retry a possibly half-destroyed owning value.
        storage.poisoned = true;
    }
    return proof;
}

bool cell_proof_is_exact(const CellProbeResult& proof) noexcept {
    return proof.resolver_returned_destination &&
        proof.existence_call_exact && proof.exists &&
        proof.fingerprint_exact && proof.lineage_exact &&
        proof.cleanup_exact && proof.seh_code == 0;
}

std::uint8_t cell_proof_bits(const CellProbeResult& proof) noexcept {
    return static_cast<std::uint8_t>(
        (proof.resolver_returned_destination ? 1U << 0U : 0U) |
        (proof.existence_call_exact ? 1U << 1U : 0U) |
        (proof.exists ? 1U << 2U : 0U) |
        (proof.fingerprint_exact ? 1U << 3U : 0U) |
        (proof.lineage_exact ? 1U << 4U : 0U) |
        (proof.cleanup_exact ? 1U << 5U : 0U) |
        (proof.seh_code == 0 ? 1U << 6U : 0U));
}

bool distinct_child_payloads(
    const MovieFingerprint& root,
    const std::array<
        CellProbeResult, kMaximumLiveCellProofCount>& cells,
    std::size_t count) noexcept {
    if (count != kRequiredLiveCellProofCount || count > cells.size()) {
        return false;
    }
    std::array<void*, kMaximumLiveCellProofCount> payloads{};
    for (std::size_t index = 0; index < count; ++index) {
        if (!cells[index].fingerprint_exact ||
            !cells[index].lineage_exact ||
            !cells[index].movie.object_payload ||
            cells[index].movie.object_payload == root.object_payload) {
            return false;
        }
        payloads[index] = cells[index].movie.object_payload;
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (payloads[earlier] == payloads[index]) return false;
        }
    }
    return true;
}

void log_line(const char* format, ...) noexcept {
    if (g_log_file == INVALID_HANDLE_VALUE || !format) return;
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
        line.data(), line.size(),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [T%lu] [TabSlotAppendProbe] %s\r\n",
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
        static_cast<std::size_t>(line_length), line.size() - 1U));
    AcquireSRWLockExclusive(&g_log_lock);
    DWORD bytes_written{};
    (void)WriteFile(
        g_log_file, line.data(), bytes_to_write, &bytes_written, nullptr);
    (void)FlushFileBuffers(g_log_file);
    ReleaseSRWLockExclusive(&g_log_lock);
}

bool initialize_log() noexcept {
    std::array<wchar_t, 32768> module_path{};
    const DWORD length = GetModuleFileNameW(
        g_module, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size()) return false;
    wchar_t* const separator = wcsrchr(module_path.data(), L'\\');
    if (!separator) return false;
    *(separator + 1) = L'\0';
    constexpr wchar_t kLogName[] = L"ERNativeUITabSlotAppendProbe.log";
    if (wcslen(module_path.data()) + _countof(kLogName) >
            g_log_path.size()) {
        return false;
    }
    wcscpy_s(g_log_path.data(), g_log_path.size(), module_path.data());
    wcscat_s(g_log_path.data(), g_log_path.size(), kLogName);
    g_log_file = CreateFileW(
        g_log_path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return g_log_file != INVALID_HANDLE_VALUE;
}

bool read_host_configuration(
    std::array<wchar_t, 32768>& host_path,
    std::array<wchar_t, 32768>& ini_path) noexcept {
    HMODULE host{};
    for (DWORD elapsed = 0; elapsed <= kHostWaitMilliseconds;
         elapsed += kHostPollMilliseconds) {
        host = GetModuleHandleW(L"ERNativeUI.dll");
        if (host) break;
        Sleep(kHostPollMilliseconds);
    }
    if (!host) {
        log_line("ERROR: canonical ERNativeUI.dll was not loaded");
        return false;
    }
    if (GetModuleHandleW(L"NativeTweaks.dll")) {
        log_line(
            "ERROR: NativeTweaks.dll is loaded; this proof must run only with the canonical host");
        return false;
    }

    const DWORD length = GetModuleFileNameW(
        host, host_path.data(), static_cast<DWORD>(host_path.size()));
    if (length == 0 || length >= host_path.size()) {
        log_line("ERROR: canonical host path could not be read");
        return false;
    }
    ini_path = host_path;
    wchar_t* const separator = wcsrchr(ini_path.data(), L'\\');
    if (!separator) {
        log_line("ERROR: canonical host directory could not be resolved");
        return false;
    }
    *(separator + 1) = L'\0';
    constexpr wchar_t kIniName[] = L"ERNativeUI.ini";
    if (wcslen(ini_path.data()) + _countof(kIniName) > ini_path.size()) {
        return false;
    }
    wcscat_s(ini_path.data(), ini_path.size(), kIniName);

    const UINT configured = GetPrivateProfileIntW(
        L"Tabs", L"MinimumTabSlots", 8, ini_path.data());
    log_line(
        "hostPath=%ls iniPath=%ls MinimumTabSlots=%u",
        host_path.data(), ini_path.data(), static_cast<unsigned>(configured));
    // The isolated GFX-growth research host intentionally leaves the public
    // setting at eight while forcing only its Scaleform transform to the
    // compile-time research target.
    // Live selector geometry at the constructor-local seam is the authority;
    // the INI value is recorded for attribution but is not a mutation gate.
    return true;
}

bool exact_bytes(
    std::uintptr_t rva,
    const std::uint8_t* expected,
    std::size_t size) noexcept {
    std::array<std::uint8_t, 32> actual{};
    return expected && size <= actual.size() &&
        guarded_copy(actual.data(), g_game.at_rva(rva), size) &&
        std::memcmp(actual.data(), expected, size) == 0;
}

bool category_append_entry_is_pristine() noexcept {
    constexpr std::array<std::uint8_t, 16> kCategoryAppendEntry{
        0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
        0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48};
    return exact_bytes(
        kCategoryAppendRva,
        kCategoryAppendEntry.data(),
        kCategoryAppendEntry.size());
}

bool static_profile_is_pristine() noexcept {
    constexpr std::array<std::uint8_t, 16> kTopConstructorEntry{
        0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41,
        0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC};
    constexpr std::array<std::uint8_t, 16> kEarlyAppendSeam{
        0x4C, 0x8D, 0x05, 0xB2, 0x8F, 0x17, 0x02, 0x48,
        0x8D, 0x55, 0x50, 0x48, 0x8B, 0x4C, 0x24, 0x50};
    constexpr std::array<std::uint8_t, 16> kPostGeometryObservationSeam{
        0x0F, 0xBF, 0x9D, 0x74, 0x01, 0x00, 0x00, 0x4C,
        0x89, 0x6C, 0x24, 0x40, 0x49, 0x8D, 0x8E, 0xE0};
    constexpr std::array<std::uint8_t, 16> kSelectorGeometryDispatch{
        0x49, 0x8B, 0x06, 0x49, 0x8B, 0xCE, 0xFF, 0x50,
        0x18, 0x0F, 0xBF, 0x9D, 0x74, 0x01, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 16> kFirstCountPublication{
        0x49, 0x8B, 0x45, 0x00, 0x49, 0x8B, 0xCD, 0xFF,
        0x50, 0x08, 0x8B, 0xD0, 0x48, 0x8D, 0x4B, 0x18};
    constexpr std::array<std::uint8_t, 17> kCountPropagationDispatch{
        0x45, 0x33, 0xC9, 0x44, 0x8B, 0xC3, 0x49, 0x8B,
        0xD5, 0x49, 0x8B, 0xCE, 0xE8, 0xD4, 0xF7, 0xFF,
        0xFF};
    constexpr std::array<std::uint8_t, 16> kPostConstructorObserver{
        0x48, 0x8B, 0xF8, 0xEB, 0x03, 0x48, 0x8B, 0xFB,
        0xF6, 0xC3, 0x01, 0x74, 0x11, 0x83, 0xE3, 0xFE};
    constexpr std::array<std::uint8_t, 7> kCategoryCountGetterEntry{
        0x8B, 0x81, 0x60, 0x05, 0x00, 0x00, 0xC3};
    constexpr std::array<std::uint8_t, 16> kSelectorCountSetterEntry{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x85};
    constexpr std::array<std::uint8_t, 16> kValueExistsEntry{
        0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x01, 0xFF,
        0x10, 0xF6, 0x40, 0x20, 0x8F, 0x0F, 0x95, 0xC0};
    constexpr std::array<std::uint8_t, 16> kScaleformDestructorEntry{
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x48, 0x83,
        0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE};
    return exact_bytes(
               kTopConstructorRva,
               kTopConstructorEntry.data(),
               kTopConstructorEntry.size()) &&
        exact_bytes(
            kSelectorGeometryDispatchRva,
            kSelectorGeometryDispatch.data(),
            kSelectorGeometryDispatch.size()) &&
        exact_bytes(
            kEarlyAppendSeamRva,
            kEarlyAppendSeam.data(),
            kEarlyAppendSeam.size()) &&
        exact_bytes(
            kFirstCountPublicationRva,
            kFirstCountPublication.data(),
            kFirstCountPublication.size()) &&
        exact_bytes(
            kPostGeometryObservationSeamRva,
            kPostGeometryObservationSeam.data(),
            kPostGeometryObservationSeam.size()) &&
        exact_bytes(
            kCountPropagationDispatchRva,
            kCountPropagationDispatch.data(),
            kCountPropagationDispatch.size()) &&
        exact_bytes(
            kPostConstructorObserverRva,
            kPostConstructorObserver.data(),
            kPostConstructorObserver.size()) &&
        category_append_entry_is_pristine() &&
        exact_bytes(
            kCategoryCountGetterRva,
            kCategoryCountGetterEntry.data(),
            kCategoryCountGetterEntry.size()) &&
        exact_bytes(
            kSelectorSetActiveCountRva,
            kSelectorCountSetterEntry.data(),
            kSelectorCountSetterEntry.size()) &&
        exact_bytes(
            kValueExistsRva,
            kValueExistsEntry.data(),
            kValueExistsEntry.size()) &&
        exact_bytes(
            kScaleformDestructorRva,
            kScaleformDestructorEntry.data(),
            kScaleformDestructorEntry.size());
}

void append_owned_graphics_at_early_seam(
    safetyhook::Context& cpu) noexcept {
    if (!g_callbacks_armed.load(std::memory_order_acquire) ||
        g_inside_seam_append) {
        return;
    }

    ConstructorContext& construction = g_constructor_context;
    void* const top = reinterpret_cast<void*>(cpu.rdi);
    if (!top || construction.active) return;

    construction = {};
    construction.top = top;
    construction.active = true;
    construction.early_seam_seen = true;
    construction.title_menu = cpu.r15 != 0;
    construction.observation_only = true;

    void* scene_proxy{};
    MovieFingerprint movie{};
    MovieFingerprint top_movie{};
    void* frame_top{};
    void* top_vtable{};
    void* list_vtable{};
    void* source_vtable{};
    void* selector_vtable{};
    void* geometry_enumerator{};
    std::uint64_t count_before{};
    std::int32_t active_before{-1};
    const std::byte* const storage =
        offset_address(top, kCategoryStorageOffset);
    const std::byte* const list =
        offset_address(top, kCategoryListOffset);
    const std::byte* const selector =
        offset_address(top, kSelectorOffset);
    const std::byte* const composite =
        offset_address(top, kCompositeOffset);
    const std::byte* const controller =
        offset_address(top, kTopControllerOffset);
    const std::byte* const source = record_address(top, 7);
    const bool scene_proxy_read = guarded_read(
        reinterpret_cast<const void*>(cpu.rsp + 0x50), scene_proxy);
    const bool caller_movie_exact = scene_proxy_read &&
        read_movie_fingerprint(scene_proxy, movie);
    const bool top_movie_exact = read_movie_fingerprint(
        offset_address(top, kMovieContextOffset), top_movie);
    const bool movie_fingerprint_exact = caller_movie_exact &&
        top_movie_exact && same_movie_fingerprint(movie, top_movie);
    const bool frame_top_read = cpu.rbp >= 0x58 && guarded_read(
        reinterpret_cast<const void*>(cpu.rbp - 0x58), frame_top);
    const bool exact_register_frame =
        cpu.rbx == reinterpret_cast<std::uintptr_t>(storage) &&
        cpu.r13 == reinterpret_cast<std::uintptr_t>(list) &&
        cpu.r14 == reinterpret_cast<std::uintptr_t>(selector) &&
        cpu.r12 == reinterpret_cast<std::uintptr_t>(composite) &&
        cpu.rsi == reinterpret_cast<std::uintptr_t>(controller) &&
        cpu.r15 == 0 && frame_top_read && frame_top == top;
    const bool late_native_tweaks =
        GetModuleHandleW(L"NativeTweaks.dll") != nullptr;
    const bool top_identity = guarded_read(top, top_vtable) &&
        top_vtable == g_game.at_rva(kTopVtableRva);
    const bool list_identity = guarded_read(list, list_vtable) &&
        list_vtable == g_game.at_rva(kCategoryListVtableRva);
    const bool source_identity = guarded_read(source, source_vtable) &&
        source_vtable == g_game.at_rva(kCategoryRecordVtableRva);
    const bool selector_identity =
        guarded_read(selector, selector_vtable) &&
        selector_vtable == g_game.at_rva(kSelectorVtableRva) &&
        guarded_read(
            offset_address(selector_vtable, 3U * sizeof(void*)),
            geometry_enumerator) &&
        geometry_enumerator ==
            g_game.at_rva(kSelectorGeometryEnumeratorRva);
    const bool count_exact = guarded_read(
        offset_address(top, kCategoryCountOffset), count_before) &&
        count_before == 8;
    const bool prefix_exact = exact_prefix_records(top);
    const bool graphics_exact = record_has_identity(top, 7, 8);
    const bool active_readable = guarded_read(
        offset_address(selector, kSelectorActiveCountOffset), active_before);
    const bool append_pristine = category_append_entry_is_pristine();
    RecordScalars source_before{};
    const bool source_snapshot_exact =
        read_record_scalars(source, source_before);

    std::array<
        CellProbeResult, kMaximumLiveCellProofCount> cell_proofs{};
    std::size_t cell_probe_count{};
    if (movie_fingerprint_exact && g_resolve_path && g_value_exists &&
        g_destroy_scaleform_value) {
        void* const movie_context = offset_address(
            top, kMovieContextOffset);
        constexpr std::array<const char*, 2> kStripNames{
            "TabList", "BackTabList"};
        for (const char* strip : kStripNames) {
            for (std::uint32_t cell_index = 8U;
                 cell_index <= kRequiredMinimumTabSlots;
                 ++cell_index) {
                std::array<char, 64> path{};
                const int path_length = std::snprintf(
                    path.data(), path.size(),
                    "%s/Item_0_%u", strip,
                    static_cast<unsigned>(cell_index));
                if (path_length > 0 &&
                    cell_probe_count < cell_proofs.size()) {
                    cell_proofs[cell_probe_count] = probe_live_cell(
                        movie_context,
                        path.data(),
                        movie,
                        g_cell_probe_storage[cell_probe_count]);
                }
                ++cell_probe_count;
            }
        }
    }
    construction.cell_probe_count = cell_probe_count;
    construction.cell_proofs_exact =
        cell_probe_count == kRequiredLiveCellProofCount;
    construction.cell_cleanup_exact =
        cell_probe_count == kRequiredLiveCellProofCount;
    for (std::size_t index = 0;
         index < (std::min)(cell_probe_count, cell_proofs.size());
         ++index) {
        construction.cell_movies[index] = cell_proofs[index].movie;
        construction.cell_probe_bits[index] =
            cell_proof_bits(cell_proofs[index]);
        construction.cell_proofs_exact =
            construction.cell_proofs_exact &&
            cell_proof_is_exact(cell_proofs[index]);
        construction.cell_cleanup_exact =
            construction.cell_cleanup_exact &&
            cell_proofs[index].cleanup_exact;
        remember_first_seh(
            cell_proofs[index].seh_code, construction.cell_probe_seh);
    }
    construction.cell_payloads_distinct = distinct_child_payloads(
        movie, cell_proofs, cell_probe_count);
    const bool live_cells_exact =
        construction.cell_proofs_exact &&
        construction.cell_cleanup_exact &&
        construction.cell_payloads_distinct &&
        construction.cell_probe_seh == 0;

    const std::array<bool, 16> gates{
        !construction.title_menu,
        !late_native_tweaks,
        g_category_append != nullptr,
        exact_register_frame,
        movie_fingerprint_exact,
        top_identity,
        list_identity,
        source_identity,
        selector_identity,
        count_exact,
        prefix_exact,
        graphics_exact,
        active_readable,
        append_pristine,
        source_snapshot_exact,
        live_cells_exact};
    std::uint32_t gate_bits{};
    for (std::size_t index = 0; index < gates.size(); ++index) {
        if (gates[index]) gate_bits |= 1U << index;
    }

    construction.scene_proxy = scene_proxy;
    construction.movie = movie;
    construction.early_gate_bits = gate_bits;
    construction.count_before = count_before;
    construction.active_before = active_before;

    const bool may_mutate = kMutationEnabled &&
        gate_bits == kAllEarlyGateBits;
    if (!may_mutate) return;

    construction.observation_only = false;
    construction.extra_append_attempted = true;
    for (std::uint32_t expected_count = 9U;
         expected_count <= kRequiredMinimumTabSlots;
         ++expected_count) {
        g_inside_seam_append = true;
        // The helper increments the owned-vector count before invoking the
        // record copy constructor. A fault can therefore leave a partially
        // constructed counted record, which is not recoverable. Do not catch
        // SEH and resume with ambiguous ownership.
        g_category_append(const_cast<std::byte*>(storage), source);
        g_inside_seam_append = false;

        std::uint64_t count_after{};
        std::int32_t active_after{-1};
        const bool count_after_read = guarded_read(
            offset_address(top, kCategoryCountOffset), count_after);
        const bool active_after_read = guarded_read(
            offset_address(selector, kSelectorActiveCountOffset), active_after);
        bool snapshots_exact = record_matches_snapshot(
            record_address(top, 7), source_before);
        for (std::size_t index = 8U;
             index < expected_count && snapshots_exact;
             ++index) {
            snapshots_exact = record_matches_snapshot(
                record_address(top, index), source_before);
        }
        const bool checkpoint_exact = count_after_read &&
            count_after == expected_count &&
            count_after <= kNativeCategoryCapacity && active_after_read &&
            active_after == active_before &&
            exact_records_through(top, expected_count) && snapshots_exact;
        if (!checkpoint_exact) {
            // A committed or partially committed owned record has no proven
            // rollback. Never return from the constructor after an ambiguous
            // intermediate append.
            log_line(
                "FATAL: append checkpoint failed target=%u checkpoint=%u/%u reads={count:%u active:%u} values={count:%llu active:%d expectedActive:%d} records=%u snapshots=%u",
                static_cast<unsigned>(kRequiredMinimumTabSlots),
                static_cast<unsigned>(expected_count - 8U),
                static_cast<unsigned>(kRequiredAppendCount),
                count_after_read ? 1U : 0U,
                active_after_read ? 1U : 0U,
                static_cast<unsigned long long>(count_after),
                active_after,
                active_before,
                exact_records_through(top, expected_count) ? 1U : 0U,
                snapshots_exact ? 1U : 0U);
            RaiseFailFastException(nullptr, nullptr, 0);
        }
        ++construction.append_checkpoints;
    }
    construction.extra_append_committed =
        construction.append_checkpoints == kRequiredAppendCount &&
        exact_expanded_records(top);
    if (!construction.extra_append_committed) {
        log_line(
            "FATAL: final append checkpoint failed target=%u checkpoints=%u/%u records=%u",
            static_cast<unsigned>(kRequiredMinimumTabSlots),
            static_cast<unsigned>(construction.append_checkpoints),
            static_cast<unsigned>(kRequiredAppendCount),
            exact_expanded_records(top) ? 1U : 0U);
        RaiseFailFastException(nullptr, nullptr, 0);
    }
}

void observe_post_geometry_seam(safetyhook::Context& cpu) noexcept {
    if (!g_callbacks_armed.load(std::memory_order_acquire) ||
        g_inside_seam_append) {
        return;
    }

    ConstructorContext& construction = g_constructor_context;
    void* const top = reinterpret_cast<void*>(cpu.rdi);
    if (!construction.active || !construction.top ||
        top != construction.top || construction.late_seam_seen) {
        return;
    }
    construction.late_seam_seen = true;

    void* scene_proxy{};
    MovieFingerprint movie{};
    MovieFingerprint top_movie{};
    void* frame_top{};
    void* top_vtable{};
    void* list_vtable{};
    void* selector_vtable{};
    void* geometry_enumerator{};
    std::uint64_t count{};
    std::int32_t active{-1};
    std::int32_t axis0{};
    std::int32_t axis1{};
    std::int64_t authored_cells{};
    const std::byte* const list =
        offset_address(top, kCategoryListOffset);
    const std::byte* const selector =
        offset_address(top, kSelectorOffset);
    const std::byte* const composite =
        offset_address(top, kCompositeOffset);
    const std::byte* const controller =
        offset_address(top, kTopControllerOffset);
    const bool scene_proxy_read = guarded_read(
        reinterpret_cast<const void*>(cpu.rsp + 0x50), scene_proxy);
    const bool caller_movie_exact = scene_proxy_read &&
        scene_proxy == construction.scene_proxy &&
        read_movie_fingerprint(scene_proxy, movie);
    const bool top_movie_exact = read_movie_fingerprint(
        offset_address(top, kMovieContextOffset), top_movie);
    const bool frame_top_read = cpu.rbp >= 0x58 && guarded_read(
        reinterpret_cast<const void*>(cpu.rbp - 0x58), frame_top);
    const bool exact_register_frame =
        cpu.r13 == reinterpret_cast<std::uintptr_t>(list) &&
        cpu.r14 == reinterpret_cast<std::uintptr_t>(selector) &&
        cpu.r12 == reinterpret_cast<std::uintptr_t>(composite) &&
        cpu.rsi == reinterpret_cast<std::uintptr_t>(controller) &&
        cpu.r15 == 0 && frame_top_read && frame_top == top;
    const bool movie_matches = caller_movie_exact && top_movie_exact &&
        same_movie_fingerprint(movie, top_movie) &&
        same_movie_fingerprint(movie, construction.movie);
    const bool late_native_tweaks =
        GetModuleHandleW(L"NativeTweaks.dll") != nullptr;
    const bool top_identity = guarded_read(top, top_vtable) &&
        top_vtable == g_game.at_rva(kTopVtableRva);
    const bool list_identity = guarded_read(list, list_vtable) &&
        list_vtable == g_game.at_rva(kCategoryListVtableRva);
    const bool selector_identity =
        guarded_read(selector, selector_vtable) &&
        selector_vtable == g_game.at_rva(kSelectorVtableRva) &&
        guarded_read(
            offset_address(selector_vtable, 3U * sizeof(void*)),
            geometry_enumerator) &&
        geometry_enumerator ==
            g_game.at_rva(kSelectorGeometryEnumeratorRva);
    const std::uint64_t expected_count =
        construction.extra_append_committed ? kRequiredMinimumTabSlots : 8;
    const bool count_exact = guarded_read(
        offset_address(top, kCategoryCountOffset), count) &&
        count == expected_count;
    const bool records_exact = construction.extra_append_committed
        ? exact_expanded_records(top)
        : exact_stock_records(top);
    // This seam runs after the selector has enumerated the authored geometry,
    // but before the constructor publishes the native record count to D0.
    // D0 must therefore still contain the exact value observed at the early
    // pre-publication seam. The final constructor-return proof below checks
    // the naturally published count (8 in observation mode, target in
    // mutation mode).
    const bool active_exact = guarded_read(
        offset_address(selector, kSelectorActiveCountOffset), active) &&
        active == construction.active_before;
    const bool geometry_exact =
        read_geometry(top, axis0, axis1, authored_cells);

    const std::array<bool, 10> gates{
        construction.early_seam_seen && exact_register_frame,
        movie_matches,
        !late_native_tweaks,
        top_identity,
        list_identity,
        selector_identity,
        count_exact,
        records_exact,
        active_exact,
        geometry_exact};
    std::uint32_t gate_bits{};
    for (std::size_t index = 0; index < gates.size(); ++index) {
        if (gates[index]) gate_bits |= 1U << index;
    }
    construction.late_gate_bits = gate_bits;
    construction.late_active = active;
    construction.late_axis0 = axis0;
    construction.late_axis1 = axis1;
}

void validate_post_constructor(
    const ConstructorContext& context,
    void* result,
    void* allocation) noexcept {
    void* top_vtable{};
    void* list_vtable{};
    void* count_getter{};
    void* selector_vtable{};
    std::uint64_t count{};
    std::int32_t active{-1};
    std::int32_t selected{-2};
    std::int32_t axis0{};
    std::int32_t axis1{};
    std::int64_t authored_cells{};
    const std::byte* const selector =
        offset_address(context.top, kSelectorOffset);
    const bool late_native_tweaks =
        GetModuleHandleW(L"NativeTweaks.dll") != nullptr;
    const bool top_vtable_read = guarded_read(context.top, top_vtable);
    const bool list_vtable_read = guarded_read(
        offset_address(context.top, kCategoryListOffset), list_vtable);
    const bool count_getter_read = list_vtable != nullptr && guarded_read(
        offset_address(list_vtable, sizeof(void*)), count_getter);
    const bool selector_vtable_read = guarded_read(selector, selector_vtable);
    const bool count_read = guarded_read(
        offset_address(context.top, kCategoryCountOffset), count);
    const bool active_read = guarded_read(
        offset_address(selector, kSelectorActiveCountOffset), active);
    const bool selected_read = guarded_read(
        offset_address(selector, kSelectorSelectedIndexOffset), selected);
    const bool geometry_read =
        read_geometry(context.top, axis0, axis1, authored_cells);
    const bool stock_records_exact = exact_stock_records(context.top);
    const bool expanded_records_exact = exact_expanded_records(context.top);
    MovieFingerprint caller_movie{};
    MovieFingerprint top_movie{};
    const bool caller_movie_exact = read_movie_fingerprint(
        context.scene_proxy, caller_movie);
    const bool top_movie_exact = read_movie_fingerprint(
        offset_address(context.top, kMovieContextOffset), top_movie);
    const bool movie_fingerprint_exact = caller_movie_exact &&
        top_movie_exact && same_movie_fingerprint(caller_movie, top_movie) &&
        same_movie_fingerprint(caller_movie, context.movie);

    const bool mutation = context.extra_append_attempted;
    const std::uint64_t expected_count =
        mutation ? kRequiredMinimumTabSlots : 8;
    const bool mode_exact = mutation
        ? (!context.observation_only && context.extra_append_committed &&
            context.append_checkpoints == kRequiredAppendCount &&
            expanded_records_exact)
        : (context.observation_only && !context.extra_append_committed &&
            stock_records_exact);
    const bool proof = context.early_seam_seen && context.late_seam_seen &&
        context.early_gate_bits == kAllEarlyGateBits &&
        context.late_gate_bits == kAllLateGateBits &&
        result == context.top && allocation == context.top &&
        movie_fingerprint_exact && !context.title_menu &&
        !late_native_tweaks && top_vtable_read &&
        top_vtable == g_game.at_rva(kTopVtableRva) && list_vtable_read &&
        list_vtable == g_game.at_rva(kCategoryListVtableRva) &&
        count_getter_read &&
        count_getter == g_game.at_rva(kCategoryCountGetterRva) &&
        selector_vtable_read &&
        selector_vtable == g_game.at_rva(kSelectorVtableRva) &&
        count_read && count == expected_count && active_read &&
        active == static_cast<std::int32_t>(expected_count) &&
        selected_read && selected >= -1 && selected < active &&
        geometry_read &&
        static_cast<std::int64_t>(active) <= authored_cells && mode_exact;

    if (!proof) {
        log_line(
            "ERROR: same-construction proof failed target=%u top=%p proxy=%p movie={caller:%u top:%u exact:%u proxyOwner:%p interface:%p ownerLink:%p root:%p payload:%p aux:%p flags:0x%08X} result=%p allocation=%p early=%u late=%u observe=%u attempted=%u committed=%u checkpoints=%u/%u gates={early:0x%04X/0x%04X late:0x%04X/0x%04X} pre={count:%llu D0:%d} late={D0:%d geometry:%d*%d} titleMenu=%u nativeTweaks=%u reads={top:%u list:%u getter:%u selector:%u count:%u D0:%u D4:%u geometry:%u} identities={top:%p list:%p getter:%p selector:%p} post={count:%llu D0:%d D4:%d geometry:%d*%d stock:%u expanded:%u}",
            static_cast<unsigned>(kRequiredMinimumTabSlots),
            context.top,
            context.scene_proxy,
            caller_movie_exact ? 1U : 0U,
            top_movie_exact ? 1U : 0U,
            movie_fingerprint_exact ? 1U : 0U,
            context.movie.proxy_owner,
            context.movie.object_interface,
            context.movie.owner_link,
            context.movie.movie_root,
            context.movie.object_payload,
            context.movie.auxiliary,
            static_cast<unsigned>(context.movie.flags),
            result,
            allocation,
            context.early_seam_seen ? 1U : 0U,
            context.late_seam_seen ? 1U : 0U,
            context.observation_only ? 1U : 0U,
            context.extra_append_attempted ? 1U : 0U,
            context.extra_append_committed ? 1U : 0U,
            static_cast<unsigned>(context.append_checkpoints),
            static_cast<unsigned>(kRequiredAppendCount),
            static_cast<unsigned>(context.early_gate_bits),
            static_cast<unsigned>(kAllEarlyGateBits),
            static_cast<unsigned>(context.late_gate_bits),
            static_cast<unsigned>(kAllLateGateBits),
            static_cast<unsigned long long>(context.count_before),
            context.active_before,
            context.late_active,
            context.late_axis0,
            context.late_axis1,
            context.title_menu ? 1U : 0U,
            late_native_tweaks ? 1U : 0U,
            top_vtable_read ? 1U : 0U,
            list_vtable_read ? 1U : 0U,
            count_getter_read ? 1U : 0U,
            selector_vtable_read ? 1U : 0U,
            count_read ? 1U : 0U,
            active_read ? 1U : 0U,
            selected_read ? 1U : 0U,
            geometry_read ? 1U : 0U,
            top_vtable,
            list_vtable,
            count_getter,
            selector_vtable,
            static_cast<unsigned long long>(count),
            active,
            selected,
            axis0,
            axis1,
            stock_records_exact ? 1U : 0U,
            expanded_records_exact ? 1U : 0U);
        log_line(
            "ERROR: live-cell proof count=%u/%u exact=%u cleanup=%u distinct=%u seh=0x%08lX bits=%02X/%02X/%02X/%02X/%02X/%02X payloads=%p/%p/%p/%p/%p/%p",
            static_cast<unsigned>(context.cell_probe_count),
            static_cast<unsigned>(kRequiredLiveCellProofCount),
            context.cell_proofs_exact ? 1U : 0U,
            context.cell_cleanup_exact ? 1U : 0U,
            context.cell_payloads_distinct ? 1U : 0U,
            context.cell_probe_seh,
            static_cast<unsigned>(context.cell_probe_bits[0]),
            static_cast<unsigned>(context.cell_probe_bits[1]),
            static_cast<unsigned>(context.cell_probe_bits[2]),
            static_cast<unsigned>(context.cell_probe_bits[3]),
            static_cast<unsigned>(context.cell_probe_bits[4]),
            static_cast<unsigned>(context.cell_probe_bits[5]),
            context.cell_movies[0].object_payload,
            context.cell_movies[1].object_payload,
            context.cell_movies[2].object_payload,
            context.cell_movies[3].object_payload,
            context.cell_movies[4].object_payload,
            context.cell_movies[5].object_payload);
        if (context.extra_append_attempted) {
            // Once the native append commits there is no proven rollback for
            // the owned record. Never return an object whose later count,
            // selector, geometry, or ownership invariants are unexpected.
            RaiseFailFastException(nullptr, nullptr, 0);
        }
        return;
    }

    if (!mutation) {
        const std::uint32_t generation = g_successful_observations.fetch_add(
            1, std::memory_order_relaxed) + 1U;
        log_line(
            "OBSERVE SUCCESS generation=%u target=%u top=%p movie={proxyOwner:%p interface:%p ownerLink:%p root:%p payload:%p aux:%p flags:0x%08X} liveCells={count:%u exact:1 cleanup:1 distinct:1 bits:%02X/%02X/%02X/%02X/%02X/%02X payloads:%p/%p/%p/%p/%p/%p} native count=8 selectorD0=8 selectedD4=%d geometry=%d*%d authoredCells=%lld mutationEnabled=%u; all required cells were proven in this exact live construction and no native record was changed",
            static_cast<unsigned>(generation),
            static_cast<unsigned>(kRequiredMinimumTabSlots),
            context.top,
            context.movie.proxy_owner,
            context.movie.object_interface,
            context.movie.owner_link,
            context.movie.movie_root,
            context.movie.object_payload,
            context.movie.auxiliary,
            static_cast<unsigned>(context.movie.flags),
            static_cast<unsigned>(context.cell_probe_count),
            static_cast<unsigned>(context.cell_probe_bits[0]),
            static_cast<unsigned>(context.cell_probe_bits[1]),
            static_cast<unsigned>(context.cell_probe_bits[2]),
            static_cast<unsigned>(context.cell_probe_bits[3]),
            static_cast<unsigned>(context.cell_probe_bits[4]),
            static_cast<unsigned>(context.cell_probe_bits[5]),
            context.cell_movies[0].object_payload,
            context.cell_movies[1].object_payload,
            context.cell_movies[2].object_payload,
            context.cell_movies[3].object_payload,
            context.cell_movies[4].object_payload,
            context.cell_movies[5].object_payload,
            selected,
            axis0,
            axis1,
            static_cast<long long>(authored_cells),
            kMutationEnabled ? 1U : 0U);
        return;
    }

    const std::uint32_t generation = g_successful_appends.fetch_add(
        1, std::memory_order_relaxed) + 1U;
    log_line(
        "MUTATION SUCCESS generation=%u target=%u top=%p movie={proxyOwner:%p interface:%p ownerLink:%p root:%p payload:%p aux:%p flags:0x%08X} liveCells={count:%u exact:1 cleanup:1 distinct:1 bits:%02X/%02X/%02X/%02X/%02X/%02X payloads:%p/%p/%p/%p/%p/%p} native count=%u selectorD0=%u selectedD4=%d geometry=%d*%d authoredCells=%lld appendCheckpoints=%u; records were copy-constructed after same-construction live-cell proof and before Elden Ring's first count publication, and D0 was derived naturally",
        static_cast<unsigned>(generation),
        static_cast<unsigned>(kRequiredMinimumTabSlots),
        context.top,
        context.movie.proxy_owner,
        context.movie.object_interface,
        context.movie.owner_link,
        context.movie.movie_root,
        context.movie.object_payload,
        context.movie.auxiliary,
        static_cast<unsigned>(context.movie.flags),
        static_cast<unsigned>(context.cell_probe_count),
        static_cast<unsigned>(context.cell_probe_bits[0]),
        static_cast<unsigned>(context.cell_probe_bits[1]),
        static_cast<unsigned>(context.cell_probe_bits[2]),
        static_cast<unsigned>(context.cell_probe_bits[3]),
        static_cast<unsigned>(context.cell_probe_bits[4]),
        static_cast<unsigned>(context.cell_probe_bits[5]),
        context.cell_movies[0].object_payload,
        context.cell_movies[1].object_payload,
        context.cell_movies[2].object_payload,
        context.cell_movies[3].object_payload,
        context.cell_movies[4].object_payload,
        context.cell_movies[5].object_payload,
        static_cast<unsigned>(kRequiredMinimumTabSlots),
        static_cast<unsigned>(kRequiredMinimumTabSlots),
        selected,
        axis0,
        axis1,
        static_cast<long long>(authored_cells),
        static_cast<unsigned>(context.append_checkpoints));
}

void observe_post_constructor_return(safetyhook::Context& cpu) noexcept {
    if (!g_callbacks_armed.load(std::memory_order_acquire)) return;

    const ConstructorContext& pending = g_constructor_context;
    if (!pending.active || !pending.top) return;
    void* const result = reinterpret_cast<void*>(cpu.rax);
    void* const allocation = reinterpret_cast<void*>(cpu.rdi);
    const ConstructorContext completed = pending;
    g_constructor_context = {};
    validate_post_constructor(completed, result, allocation);
}

bool install_hooks() noexcept {
    if (!static_profile_is_pristine()) {
        log_line(
            "ERROR: a validated constructor seam, native helper, Scaleform existence helper, or Scaleform value destructor differs from the supported executable");
        return false;
    }

    auto early_hook = SafetyHookMid::create(
        g_game.at_rva(kEarlyAppendSeamRva),
        &append_owned_graphics_at_early_seam,
        SafetyHookMid::StartDisabled);
    if (!early_hook) {
        log_line(
            "ERROR: early constructor append seam preparation failed (SafetyHook type=%u)",
            static_cast<unsigned>(early_hook.error().type));
        return false;
    }
    auto late_hook = SafetyHookMid::create(
        g_game.at_rva(kPostGeometryObservationSeamRva),
        &observe_post_geometry_seam,
        SafetyHookMid::StartDisabled);
    if (!late_hook) {
        log_line(
            "ERROR: post-geometry observation seam preparation failed (SafetyHook type=%u)",
            static_cast<unsigned>(late_hook.error().type));
        return false;
    }
    auto post_constructor_hook = SafetyHookMid::create(
        g_game.at_rva(kPostConstructorObserverRva),
        &observe_post_constructor_return,
        SafetyHookMid::StartDisabled);
    if (!post_constructor_hook) {
        log_line(
            "ERROR: post-constructor observer preparation failed (SafetyHook type=%u)",
            static_cast<unsigned>(post_constructor_hook.error().type));
        return false;
    }

    g_early_append_hook = std::move(*early_hook);
    g_post_geometry_observer_hook = std::move(*late_hook);
    g_post_constructor_hook = std::move(*post_constructor_hook);
    g_category_append = reinterpret_cast<CategoryAppendFn>(
        g_game.at_rva(kCategoryAppendRva));
    // The canonical host may already own a pass-through SafetyHook bridge at
    // ResolvePath. Calling the published entry is intentional; the two paths
    // below are outside every host rewrite scope.
    g_resolve_path = reinterpret_cast<ResolvePathFn>(
        g_game.at_rva(kResolvePathRva));
    g_value_exists = reinterpret_cast<ValueExistsFn>(
        g_game.at_rva(kValueExistsRva));
    g_destroy_scaleform_value = reinterpret_cast<ScaleformDestructorFn>(
        g_game.at_rva(kScaleformDestructorRva));

    // Publish the observation-only return hook first. Until the mutation seam
    // is also enabled and callbacks are armed, it is a process-lifetime no-op.
    const auto observer_enabled = g_post_constructor_hook.enable();
    if (!observer_enabled) {
        log_line(
            "ERROR: post-constructor observer enable failed (SafetyHook type=%u)",
            static_cast<unsigned>(observer_enabled.error().type));
        g_category_append = nullptr;
        g_resolve_path = nullptr;
        g_value_exists = nullptr;
        g_destroy_scaleform_value = nullptr;
        g_early_append_hook.reset();
        g_post_geometry_observer_hook.reset();
        g_post_constructor_hook.reset();
        return false;
    }
    g_hooks_exposed.store(true, std::memory_order_release);

    const auto late_enabled = g_post_geometry_observer_hook.enable();
    if (!late_enabled) {
        log_line(
            "ERROR: post-geometry observation seam enable failed after the return observer became visible (SafetyHook type=%u); retained process-lifetime but unarmed",
            static_cast<unsigned>(late_enabled.error().type));
        return false;
    }
    const auto early_enabled = g_early_append_hook.enable();
    if (!early_enabled) {
        log_line(
            "ERROR: early constructor append seam enable failed after observers became visible (SafetyHook type=%u); retained process-lifetime but unarmed",
            static_cast<unsigned>(early_enabled.error().type));
        return false;
    }
    g_callbacks_armed.store(true, std::memory_order_release);
    return true;
}

DWORD WINAPI run_probe(void*) noexcept {
    if (!initialize_log()) return 1;
    log_line("============================================================");
    log_line(
        "Isolated native tab-slot append proof starting target=%u",
        static_cast<unsigned>(kRequiredMinimumTabSlots));

    HMODULE pinned{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(g_module),
            &pinned)) {
        log_line("ERROR: probe DLL could not be pinned");
        return 1;
    }

    std::array<wchar_t, 32768> host_path{};
    std::array<wchar_t, 32768> ini_path{};
    if (!read_host_configuration(host_path, ini_path)) return 1;
    if (!g_game.initialize(GetModuleHandleW(nullptr)) ||
        g_game.timestamp() != kEldenRing271Timestamp ||
        g_game.image_size() != kEldenRing271ImageSize) {
        log_line("ERROR: unsupported Elden Ring executable; expected 2.7.1.0");
        return 1;
    }
    if (!install_hooks()) return 1;
    log_line(
        "probe ready: mode=%s target=%u; every System construction resolves front/back Item_0_8 through Item_0_%u from that exact live movie, then destroys all %u temporary values before any possible append",
        kMutationEnabled ? "same-construction-mutation" : "observation-only",
        static_cast<unsigned>(kRequiredMinimumTabSlots),
        static_cast<unsigned>(kRequiredMinimumTabSlots),
        static_cast<unsigned>(kRequiredLiveCellProofCount));
    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, &run_probe, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
