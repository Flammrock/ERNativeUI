#include "color_picker.hpp"

#include "addresses.hpp"
#include "native_dialog.hpp"
#include "runtime_log.hpp"

#include <Windows.h>
#include <safetyhook.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <malloc.h>
#include <memory>
#include <mutex>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

#if !defined(_MSC_VER) || !defined(_WIN64)
#error The native ColorPicker backend requires the Microsoft x64 C++ ABI.
#endif

namespace erui::native {
namespace {

// Elden Ring 2.7.0.0, SHA-256
// D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134.
// Timestamp/size are supplemented by long entry validation below. Every
// other executable build fails closed.
constexpr std::uint32_t kSupportedTimestamp = 0x69E9C9B9;
constexpr std::size_t kSupportedImageSize = 0x5E09600;
constexpr std::uintptr_t kBuildMenuWindowJobRva = 0x7AD980;
constexpr std::uintptr_t kSubmitConsumeRva = 0x7AA0D0;
constexpr std::uintptr_t kHeapProviderRva = 0x7A8120;
constexpr std::uintptr_t kGameAllocateRva = 0x1EBBCD0;
constexpr std::uintptr_t kColorPaletteConstructorRva = 0x77C620;
constexpr std::uintptr_t kColorPaletteDestructorRva = 0x77CA10;
constexpr std::uintptr_t kColorPalettePopulateRva = 0x77CF90;
constexpr std::uintptr_t kSceneProxyBridgeRva = 0x7460D0;
constexpr std::uintptr_t kSceneObjProxyDestructorRva = 0xD81590;
constexpr std::uintptr_t kColorControlConstructorRva = 0x8B6F90;
constexpr std::uintptr_t kColorPickerMovieNameRva = 0x2AB8DE8;
constexpr std::uintptr_t kScaleformPathResolverRva = 0x74B140;
constexpr std::uintptr_t kGameOptionsActionWidgetProducerRva = 0x86B940;
constexpr std::uintptr_t kScaleformVisibilitySetterRva = 0x734190;
constexpr std::uintptr_t kScaleformValueExistsRva = 0x733FA0;
constexpr std::uintptr_t kScaleformColorTransformSetterRva = 0xD85610;

constexpr std::size_t kColorPaletteSize = 0x980;
constexpr std::size_t kColorPalettePopulatedCountOffset = 0x930;
constexpr std::uint64_t kExpectedColorPalettePopulatedCount = 143;
constexpr std::size_t kColorControlDialogSize = 0x17A0;
constexpr std::size_t kSceneObjProxyStorageSize = 0x60;
constexpr std::size_t kSceneObjProxyValueOffset = 0x28;
constexpr std::size_t kPageJobQueueOffset = 0x10;
constexpr std::size_t kPageQueuedJobCountOffset = 0x40;
constexpr std::size_t kPageComponentStackOffset = 0x50;
constexpr std::size_t kPageComponentCountOffset = 0x98;
constexpr std::size_t kMaximumComponentCount = 8;
constexpr std::size_t kPageMovieContextOffset = 0x120;
constexpr std::size_t kPropertyCountOffset = 0x1AF0;
constexpr std::size_t kMaximumPropertyRows = 16;
constexpr std::uint32_t kColorPaletteParameterId = 0x44D;
constexpr std::uint32_t kPreviewFaultLogLimit = 4;

constexpr std::string_view kBuildMenuWindowJobPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 "
    "48 8D 6C 24 D9 48 81 EC 00 01 00 00 "
    "48 C7 44 24 40 FE FF FF FF";
constexpr std::string_view kSubmitConsumePattern =
    "48 89 54 24 10 57 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 48 89 5C 24 40";
constexpr std::string_view kHeapProviderPattern =
    "48 8B 05 ?? ?? ?? ?? C3";
constexpr std::string_view kGameAllocatePattern =
    "49 8B 00 4D 8B C8 4C 8B C2 48 8B D1 49 8B C9 48 FF 60 50";
constexpr std::string_view kColorPaletteConstructorPattern =
    "4C 8B DC 57 41 54 41 55 41 56 41 57 "
    "48 81 EC 90 00 00 00 "
    "49 C7 43 88 FE FF FF FF";
constexpr std::string_view kColorPaletteDestructorPattern =
    "48 89 4C 24 08 55 56 57 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 48 89 5C 24 60";
constexpr std::string_view kColorPalettePopulatePattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 "
    "48 C7 44 24 20 FE FF FF FF 48 89 9C 24 98 00 00 00 "
    "8B DA 4C 8B F9";
constexpr std::string_view kSceneProxyBridgePattern =
    "48 89 54 24 10 53 48 83 EC 30 "
    "48 C7 44 24 28 FE FF FF FF 48 8B DA";
constexpr std::string_view kSceneObjProxyDestructorPattern =
    "48 89 4C 24 08 53 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF "
    "48 8D 05 ?? ?? ?? ?? 48 89 01";
constexpr std::string_view kColorControlConstructorPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 "
    "48 8D AC 24 10 F5 FF FF 48 81 EC F0 0B 00 00";
constexpr std::string_view kScaleformPathResolverPattern =
    "4C 89 44 24 18 4C 89 4C 24 20 55 53 56 57 41 56 41 57 "
    "48 8D 6C 24 D1 48 81 EC C8 00 00 00 "
    "48 C7 45 8F FE FF FF FF";
constexpr std::string_view kGameOptionsActionWidgetProducerPattern =
    "48 8B C4 57 48 81 EC 20 01 00 00 "
    "48 C7 44 24 40 FE FF FF FF "
    "48 89 58 10 48 89 68 18 48 89 70 20 "
    "49 8B F1 49 8B E8 48 8B FA 48 8B D9";
constexpr std::string_view kScaleformVisibilitySetterPattern =
    "40 53 48 83 EC 20 48 8B 01 0F B6 DA FF 50 08 "
    "8B 48 20 81 E1 8F 00 00 00 83 F9 02";
constexpr std::string_view kScaleformValueExistsPattern =
    "48 83 EC 28 48 8B 01 FF 10 F6 40 20 8F "
    "0F 95 C0 48 83 C4 28 C3";
constexpr std::string_view kScaleformColorTransformSetterPattern =
    "48 89 5C 24 20 57 48 81 EC 80 00 00 00 "
    "48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 70";

constexpr char kNativeButtonWidgetPath[] = "Widgets/Button";
constexpr char kColorPickerWidgetPath[] = "Widgets/ColorPicker";
constexpr char kColorPickerFillPath[] = "Widgets/ColorPicker/Color";

const char* color_picker_widget_host_path(
    ColorPickerWidgetHost host) noexcept {
    switch (host) {
    case ColorPickerWidgetHost::subpage:
        return "GraphicOption";
    case ColorPickerWidgetHost::game_options:
        return "ControllSetting";
    case ColorPickerWidgetHost::camera_options:
        return "CameraSetting";
    case ColorPickerWidgetHost::display:
        return "EnvironmentSetting";
    case ColorPickerWidgetHost::sound:
        return "AudioSetting";
    case ColorPickerWidgetHost::network:
        return "NetworkSetting";
    case ColorPickerWidgetHost::keyboard_mouse:
        return "PCMouseKey";
    case ColorPickerWidgetHost::graphics:
        return "PCGraphic";
    }
    return nullptr;
}

constexpr wchar_t kColorPickerMovieName[] =
    L"04_031_ChrMake_ColorEditor";

struct OpaqueSceneProxy;

struct ColorPickerMovieDescriptor {
    std::uint32_t kind{8};
    std::uint8_t flag{1};
    std::uint8_t reserved[3]{};
    const wchar_t* movie_name{};
};
static_assert(sizeof(ColorPickerMovieDescriptor) == 0x10);

using NativeFactory = std::function<void*(OpaqueSceneProxy&)>;
using NativeLiveCompletion = std::function<void(std::uint32_t, bool)>;
static_assert(sizeof(NativeFactory) == 0x40,
    "Unexpected MSVC std::function factory ABI");
static_assert(sizeof(NativeLiveCompletion) == 0x40,
    "Unexpected MSVC std::function completion ABI");

using BuildMenuWindowJobFn = void**(__fastcall*)(
    void** result,
    void* component_stack,
    const ColorPickerMovieDescriptor* descriptor,
    const NativeFactory* factory);
using SubmitConsumeFn = void(__fastcall*)(void* queue, void** job);
using HeapProviderFn = void*(__fastcall*)();
using GameAllocateFn = void*(__fastcall*)(
    std::size_t size, std::size_t alignment, void* heap);
using ColorPaletteConstructorFn = void*(__fastcall*)(void* storage);
using ColorPaletteDestructorFn = void(__fastcall*)(void* palette);
using ColorPalettePopulateFn = void(__fastcall*)(
    void* palette, std::uint32_t parameter_id);
using SceneProxyBridgeFn = void*(__fastcall*)(
    OpaqueSceneProxy& source, void* destination);
using SceneObjProxyDestructorFn = void(__fastcall*)(void* value);
using ColorControlConstructorFn = void*(__fastcall*)(
    void* storage,
    void* scene_obj_proxy,
    void* outer_parent,
    std::uint8_t mode,
    void* palette,
    std::uint32_t initial_color,
    std::uint8_t special_palette_entry,
    const NativeLiveCompletion* completion);
using GameOptionsActionWidgetProducerFn = void*(__fastcall*)(
    void* destination,
    void* row_scene_proxy,
    std::uintptr_t argument3,
    std::uintptr_t argument4,
    std::uintptr_t argument5,
    std::uintptr_t argument6);
using ScaleformVisibilitySetterFn = void(__fastcall*)(
    void* scene_obj_proxy,
    std::uint8_t visible);
using ScaleformValueExistsFn = bool(__fastcall*)(void* scene_obj_proxy);
using ScaleformColorTransformSetterFn = void*(__fastcall*)(
    void* scaleform_value,
    std::uint32_t packed_color);

struct NativeInterfaces {
    BuildMenuWindowJobFn build_job{};
    SubmitConsumeFn submit_consume{};
    HeapProviderFn heap_provider{};
    GameAllocateFn allocate{};
    ColorPaletteConstructorFn palette_constructor{};
    ColorPaletteDestructorFn palette_destructor{};
    ColorPalettePopulateFn populate_palette{};
    SceneProxyBridgeFn bridge_scene_proxy{};
    SceneObjProxyDestructorFn destroy_scene_obj_proxy{};
    ColorControlConstructorFn color_control_constructor{};
    ScaleformPathResolverFn resolve_path{};
    ScaleformVisibilitySetterFn set_visible{};
    ScaleformValueExistsFn value_exists{};
    ScaleformColorTransformSetterFn set_color_transform{};
    const wchar_t* movie_name{};
};

struct PreviewLocation {
    void* page{};
    std::size_t row_index{};
    ColorPickerWidgetHost host{ColorPickerWidgetHost::subpage};

    [[nodiscard]] bool valid_for(void* candidate) const noexcept {
        return page && page == candidate && row_index < kMaximumPropertyRows;
    }
};

struct PreviewBinding {
    detail::ColorPickerState* state{};
    PreviewLocation location{};
};

struct ColorRowConstructionContext {
    void* page{};
    std::size_t row_index{};
    detail::ColorPickerState* state{};
    std::uint32_t packed_color{};
    ColorPickerWidgetHost host{ColorPickerWidgetHost::subpage};
    bool active{};
};

struct Session {
    detail::ColorPickerState* state{};
    detail::ColorAction changed_action{};
    void* job{};
    std::uint32_t initial_color{};
    std::atomic<std::uint32_t> current_color{};
    std::atomic_bool current_special_palette_entry{};
    std::atomic_bool submitted{};
    std::atomic<std::uint32_t> terminal_kind{};
    PreviewLocation preview{};
    detail::RgbColor accepted_color{};
    bool changed{};

    ~Session();
};

struct PaletteOwner {
    void* palette{};
    ColorPaletteDestructorFn destructor{};

    PaletteOwner(
        void* native_palette,
        ColorPaletteDestructorFn native_destructor) noexcept
        : palette(native_palette), destructor(native_destructor) {}
    PaletteOwner(const PaletteOwner&) = delete;
    PaletteOwner& operator=(const PaletteOwner&) = delete;
    ~PaletteOwner();
};

NativeInterfaces g_native{};
std::atomic_bool g_available{};
std::mutex g_mutex{};
std::weak_ptr<Session> g_active{};
std::shared_ptr<Session> g_completed{};
std::vector<PreviewBinding> g_preview_bindings{};
SafetyHookInline g_action_widget_producer_hook{};
std::atomic<GameOptionsActionWidgetProducerFn>
    g_original_action_widget_producer{};
std::atomic<std::uint32_t> g_preview_fault_logs{};
std::atomic_bool g_widget_path_bridge_available{};
thread_local ColorRowConstructionContext g_color_row_construction{};
thread_local void* g_color_widget_route_source{};

PreviewBinding* find_preview_binding_locked(
    const detail::ColorPickerState* state) noexcept {
    for (PreviewBinding& binding : g_preview_bindings) {
        if (binding.state == state) return &binding;
    }
    return nullptr;
}

PreviewLocation preview_location(
    const detail::ColorPickerState* state,
    void* expected_page = nullptr) noexcept {
    if (!state) return {};
    std::lock_guard lock(g_mutex);
    const PreviewBinding* const binding = find_preview_binding_locked(state);
    if (!binding || (expected_page &&
            !binding->location.valid_for(expected_page))) {
        return {};
    }
    return binding->location;
}

bool bind_preview(
    detail::ColorPickerState* state,
    PreviewLocation location) noexcept {
    if (!state || !location.valid_for(location.page)) return false;
    try {
        std::lock_guard lock(g_mutex);
        if (PreviewBinding* const existing =
                find_preview_binding_locked(state)) {
            existing->location = location;
            return true;
        }
        g_preview_bindings.push_back({state, location});
        return true;
    } catch (...) {
        return false;
    }
}

bool validate_movie_name(
    const ModuleView& game,
    const wchar_t* candidate) noexcept {
    constexpr std::size_t bytes = sizeof(kColorPickerMovieName);
    return candidate && game.contains(candidate, bytes) &&
        std::memcmp(candidate, kColorPickerMovieName, bytes) == 0;
}

void destroy_resolved_proxy(std::byte* storage) noexcept {
    if (!storage || !g_native.destroy_scene_obj_proxy) return;
#if defined(_MSC_VER)
    __try {
#endif
        g_native.destroy_scene_obj_proxy(
            storage + kSceneObjProxyValueOffset);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color preview Scaleform result cleanup faulted safely: seh=0x%08lX",
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
}

bool resolve_existing_proxy(
    void* source,
    const char* path,
    std::array<std::byte, kSceneObjProxyStorageSize>& destination,
    bool& constructed) noexcept {
    constructed = false;
    if (!source || !path || !*path || !g_native.resolve_path ||
        !g_native.value_exists) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        if (g_native.resolve_path(source, destination.data(), path) !=
                destination.data()) {
            return false;
        }
        constructed = true;
        return g_native.value_exists(destination.data());
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool apply_color_to_resolved_proxy(
    std::byte* proxy,
    std::uint32_t packed_color) noexcept {
    if (!proxy || !g_native.set_color_transform) return false;
#if defined(_MSC_VER)
    __try {
#endif
        // SceneObjProxy vslot +8 returns its embedded CSScaleformValue at
        // +0x28. Calling through the vtable mirrors the native character-row
        // renderer and deliberately avoids 0x74B020, whose wrapper swaps R/B.
        auto*** const object = reinterpret_cast<void***>(proxy);
        if (!object || !*object || !(*object)[1]) return false;
        using AccessorFn = void*(__fastcall*)(void*);
        void* const value =
            reinterpret_cast<AccessorFn>((*object)[1])(proxy);
        if (!value) return false;
        g_native.set_color_transform(value, packed_color);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool set_existing_widget_visibility(
    void* source,
    const char* path,
    bool visible) noexcept {
    alignas(16) std::array<std::byte, kSceneObjProxyStorageSize> proxy{};
    bool constructed{};
    const bool resolved = resolve_existing_proxy(
        source, path, proxy, constructed);
    bool applied{};
    if (resolved) {
#if defined(_MSC_VER)
        __try {
#endif
            g_native.set_visible(proxy.data(), visible ? 1 : 0);
            applied = true;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            applied = false;
        }
#endif
    }
    if (constructed) destroy_resolved_proxy(proxy.data());
    return applied;
}

bool apply_color_widget_value(
    void* row_scene_proxy,
    std::uint32_t packed_color) noexcept {
    alignas(16) std::array<std::byte, kSceneObjProxyStorageSize> fill{};
    bool constructed{};
    const bool resolved = resolve_existing_proxy(
        row_scene_proxy, kColorPickerFillPath, fill, constructed);
    const bool applied = resolved &&
        apply_color_to_resolved_proxy(fill.data(), packed_color);
    if (constructed) destroy_resolved_proxy(fill.data());
    return applied;
}

bool refresh_color_preview(
    const PreviewLocation& location,
    std::uint32_t packed_color) noexcept {
    if (!location.valid_for(location.page)) return false;

    char path[160]{};
    const char* const panel = color_picker_widget_host_path(location.host);
    const int written = panel
        ? _snprintf_s(
              path,
              std::size(path),
              _TRUNCATE,
              "%s/Item_%zu_0/Widgets/ColorPicker/Color",
              panel,
              location.row_index)
        : -1;
    if (written <= 0) return false;

    alignas(16) std::array<std::byte, kSceneObjProxyStorageSize> fill{};
    bool fill_constructed{};
    void* const movie_context =
        static_cast<std::byte*>(location.page) + kPageMovieContextOffset;
    const bool resolved = resolve_existing_proxy(
        movie_context, path, fill, fill_constructed);
    const bool applied = resolved && apply_color_to_resolved_proxy(
        fill.data(), packed_color);
    if (fill_constructed) destroy_resolved_proxy(fill.data());
    return applied;
}

void* __fastcall action_widget_producer_detour(
    void* destination,
    void* row_scene_proxy,
    std::uintptr_t argument3,
    std::uintptr_t argument4,
    std::uintptr_t argument5,
    std::uintptr_t argument6) noexcept {
    GameOptionsActionWidgetProducerFn original =
        g_original_action_widget_producer.load(std::memory_order_acquire);
    if (!original) return destination;

    if (!row_scene_proxy) {
        return original(
            destination,
            row_scene_proxy,
            argument3,
            argument4,
            argument5,
            argument6);
    }

    const ColorRowConstructionContext context = g_color_row_construction;
    const bool matched_color_row = context.active && context.state;
    // Preflight the complete standalone widget while it is still hidden.
    // Only hide Button and redirect the native producer after the Color child
    // accepted its initial value. A missing/partial GFX patch therefore keeps
    // the ordinary Button path intact as a real fallback.
    const bool color_widget_ready = matched_color_row &&
        g_widget_path_bridge_available.load(std::memory_order_acquire) &&
        set_existing_widget_visibility(
            row_scene_proxy, kColorPickerWidgetPath, false) &&
        apply_color_widget_value(row_scene_proxy, context.packed_color);
    const bool route_to_color_widget = color_widget_ready &&
        set_existing_widget_visibility(
            row_scene_proxy, kNativeButtonWidgetPath, false);

    g_color_widget_route_source = route_to_color_widget
        ? row_scene_proxy
        : nullptr;
    void* const result = original(
        destination,
        row_scene_proxy,
        argument3,
        argument4,
        argument5,
        argument6);
    g_color_widget_route_source = nullptr;

    if (!matched_color_row) return result;
    if (!route_to_color_widget) {
        const std::uint32_t fault =
            g_preview_fault_logs.fetch_add(1, std::memory_order_relaxed) + 1;
        if (fault <= kPreviewFaultLogLimit) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Standalone ColorPicker widget was unavailable; preserving ordinary button presentation (fault=%u)",
                static_cast<unsigned>(fault));
        }
        return result;
    }

    const bool bound = bind_preview(
        context.state,
        {
            .page = context.page,
            .row_index = context.row_index,
            .host = context.host,
        });
    erui::detail::logf(
        erui::LogLevel::trace,
        "Color preview bound: state=%p page=%p row=%zu host=%u packed=0x%08X bound=%d",
        context.state,
        context.page,
        context.row_index,
        static_cast<unsigned>(context.host),
        static_cast<unsigned>(context.packed_color),
        bound ? 1 : 0);
    return result;
}

bool read_page_preflight(void* page) noexcept {
    if (!page) return false;
#if defined(_MSC_VER)
    __try {
#endif
        const auto* const bytes = static_cast<const std::byte*>(page);
        const void* const active_job =
            *reinterpret_cast<void* const*>(bytes + kPageJobQueueOffset);
        const std::size_t queued_jobs =
            *reinterpret_cast<const std::size_t*>(
                bytes + kPageQueuedJobCountOffset);
        const std::size_t component_count =
            *reinterpret_cast<const std::size_t*>(
                bytes + kPageComponentCountOffset);
        return !active_job && queued_jobs == 0 &&
            component_count < kMaximumComponentCount;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

void destroy_palette_safely(
    void* palette,
    ColorPaletteDestructorFn destructor) noexcept {
    if (!palette) return;
    if (destructor) {
#if defined(_MSC_VER)
        __try {
#endif
            destructor(palette);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color Picker palette destructor faulted safely: palette=%p seh=0x%08lX",
                palette,
                static_cast<unsigned long>(GetExceptionCode()));
        }
#endif
    }
    _aligned_free(palette);
}

PaletteOwner::~PaletteOwner() {
    destroy_palette_safely(palette, destructor);
}

Session::~Session() {
    if (submitted.load(std::memory_order_acquire) &&
        terminal_kind.load(std::memory_order_acquire) == 0) {
        // Page/job teardown can release the native factory without a final
        // poll. Canonical state remains unchanged and row change callbacks
        // are deliberately not synthesized from that uncertain path.
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker native lifetime ended without an accept/cancel poll; canonical state was preserved");
    }
}

bool construct_palette(
    void* storage,
    bool& constructed,
    unsigned long& exception) noexcept {
    constructed = false;
    exception = 0;
    if (!storage || !g_native.palette_constructor ||
        !g_native.populate_palette) return false;
#if defined(_MSC_VER)
    __try {
#endif
        if (g_native.palette_constructor(storage) != storage) return false;
        constructed = true;
        g_native.populate_palette(storage, kColorPaletteParameterId);
        const auto* const bytes = static_cast<const std::byte*>(storage);
        std::uint64_t populated_count{};
        std::memcpy(
            &populated_count,
            bytes + kColorPalettePopulatedCountOffset,
            sizeof(populated_count));
        if (populated_count != kExpectedColorPalettePopulatedCount) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color Picker palette population validation failed: expected=%llu actual=%llu",
                static_cast<unsigned long long>(
                    kExpectedColorPalettePopulatedCount),
                static_cast<unsigned long long>(populated_count));
            return false;
        }
        return true;
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

std::shared_ptr<PaletteOwner> create_palette() {
    void* const storage = _aligned_malloc(kColorPaletteSize, 8);
    if (!storage) return {};

    unsigned long exception{};
    bool constructed{};
    if (!construct_palette(storage, constructed, exception)) {
        // Once the constructor returned, its non-deleting destructor is the
        // only correct way to release native container members even if the
        // subsequent palette population failed.
        if (constructed) {
            destroy_palette_safely(storage, g_native.palette_destructor);
        } else {
            // A constructor fault leaves unknowable partial state. Avoid
            // speculative native teardown and release only our storage.
            _aligned_free(storage);
        }
        if (exception) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color Picker palette construction faulted safely: seh=0x%08lX",
                exception);
        }
        return {};
    }

    std::unique_ptr<PaletteOwner> owner{new (std::nothrow) PaletteOwner(
        storage, g_native.palette_destructor)};
    if (!owner) {
        destroy_palette_safely(storage, g_native.palette_destructor);
        return {};
    }
    // If allocating the shared control block throws, owner still destroys the
    // constructed native palette before the exception leaves this function.
    return std::shared_ptr<PaletteOwner>(std::move(owner));
}

bool bridge_scene_proxy(
    OpaqueSceneProxy& source,
    void* destination,
    void*& result,
    unsigned long& exception) noexcept {
    result = nullptr;
    exception = 0;
#if defined(_MSC_VER)
    __try {
#endif
        result = g_native.bridge_scene_proxy(source, destination);
        return result != nullptr;
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

void destroy_scene_obj_proxy(void* storage) noexcept {
    if (!storage || !g_native.destroy_scene_obj_proxy) return;
#if defined(_MSC_VER)
    __try {
#endif
        g_native.destroy_scene_obj_proxy(
            static_cast<std::byte*>(storage) +
            kSceneObjProxyValueOffset);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker SceneObjProxy cleanup faulted safely: seh=0x%08lX",
            static_cast<unsigned long>(GetExceptionCode()));
    }
#endif
}

void* allocate_dialog(unsigned long& exception) noexcept {
    exception = 0;
#if defined(_MSC_VER)
    __try {
#endif
        void* const heap = g_native.heap_provider();
        return heap
            ? g_native.allocate(kColorControlDialogSize, 8, heap)
            : nullptr;
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
#endif
}

void* construct_dialog(
    void* storage,
    void* scene_obj_proxy,
    void* palette,
    std::uint32_t initial_color,
    const NativeLiveCompletion* completion,
    unsigned long& exception) noexcept {
    exception = 0;
#if defined(_MSC_VER)
    __try {
#endif
        return g_native.color_control_constructor(
            storage,
            scene_obj_proxy,
            nullptr,
            0,
            palette,
            initial_color,
            0,
            completion);
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
#endif
}

void* create_dialog_for_factory(
    const std::shared_ptr<Session>& session,
    const std::shared_ptr<PaletteOwner>& palette,
    OpaqueSceneProxy& source) noexcept {
    if (!session || !palette || !palette->palette) return nullptr;

    alignas(16) std::byte scene_obj_proxy[kSceneObjProxyStorageSize]{};
    void* converted{};
    unsigned long exception{};
    if (!bridge_scene_proxy(
            source, scene_obj_proxy, converted, exception)) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker SceneProxy bridge failed safely: seh=0x%08lX",
            exception);
        return nullptr;
    }

    NativeLiveCompletion live_completion;
    try {
        live_completion =
            [session](std::uint32_t color, bool special) noexcept {
                session->current_color.store(
                    color, std::memory_order_release);
                session->current_special_palette_entry.store(
                    special, std::memory_order_release);
            };
    } catch (const std::bad_alloc&) {
        exception = ERROR_OUTOFMEMORY;
    } catch (...) {
        exception = ERROR_UNHANDLED_EXCEPTION;
    }
    if (!live_completion) {
        destroy_scene_obj_proxy(scene_obj_proxy);
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker native completion construction failed: exception=0x%08lX",
            exception);
        return nullptr;
    }

    void* const dialog_storage = allocate_dialog(exception);
    if (!dialog_storage) {
        destroy_scene_obj_proxy(scene_obj_proxy);
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker native dialog allocation failed: seh=0x%08lX",
            exception);
        return nullptr;
    }

    void* dialog{};
    dialog = construct_dialog(
        dialog_storage,
        converted,
        palette->palette,
        session->initial_color,
        &live_completion,
        exception);

    destroy_scene_obj_proxy(scene_obj_proxy);
    if (!dialog) {
        // The game owns successful ColorControlDialog allocations. If its
        // constructor faults, construction progress is unknowable, so trying
        // either a deleting destructor or a raw free is less safe than this
        // single bounded allocation.
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker native dialog construction failed safely: storage=%p seh=0x%08lX",
            dialog_storage,
            exception);
    }
    return dialog;
}

bool build_native_job(
    void** job,
    void* component_stack,
    const ColorPickerMovieDescriptor* descriptor,
    const NativeFactory* factory,
    unsigned long& exception) noexcept {
    exception = 0;
#if defined(_MSC_VER)
    __try {
#endif
        return g_native.build_job(
            job, component_stack, descriptor, factory) == job;
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool submit_native_job(
    void* queue,
    void** job,
    unsigned long& exception) noexcept {
    exception = 0;
#if defined(_MSC_VER)
    __try {
#endif
        g_native.submit_consume(queue, job);
        return *job == nullptr;
#if defined(_MSC_VER)
    } __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

void clear_failed_session(const std::shared_ptr<Session>& session) noexcept {
    std::lock_guard lock(g_mutex);
    if (g_active.lock() == session) g_active.reset();
}

bool read_scheduler_kind(
    const void* scheduler_state,
    std::uint32_t& kind) noexcept {
    if (!scheduler_state) return false;
#if defined(_MSC_VER)
    __try {
#endif
        kind = *static_cast<const std::uint32_t*>(scheduler_state);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool read_property_row_index(
    const void* page,
    std::size_t& row_index) noexcept {
    if (!page) return false;
#if defined(_MSC_VER)
    __try {
#endif
        std::memcpy(
            &row_index,
            static_cast<const std::byte*>(page) + kPropertyCountOffset,
            sizeof(row_index));
        return row_index < kMaximumPropertyRows;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

} // namespace

const char* color_picker_widget_path_override(
    const char* observed_path,
    const void* source_proxy) noexcept {
    if (!observed_path || !source_proxy ||
        !g_widget_path_bridge_available.load(std::memory_order_acquire) ||
        !g_color_row_construction.active ||
        !g_color_row_construction.state ||
        g_color_widget_route_source != source_proxy ||
        std::memcmp(
            observed_path,
            kNativeButtonWidgetPath,
            sizeof(kNativeButtonWidgetPath)) != 0) {
        return nullptr;
    }
    return kColorPickerWidgetPath;
}

void set_color_picker_widget_path_bridge_available(bool available) noexcept {
    g_widget_path_bridge_available.store(available, std::memory_order_release);
    if (!available) g_color_widget_route_source = nullptr;
}

void* color_picker_widget_path_hook_target() noexcept {
    return reinterpret_cast<void*>(g_native.resolve_path);
}

void reset_color_picker_page_widgets(
    void* page,
    ColorPickerWidgetHost host) noexcept {
    if (!page) return;
    {
        std::lock_guard lock(g_mutex);
        for (PreviewBinding& binding : g_preview_bindings) {
            if (binding.location.page == page) binding.location = {};
        }
    }
    if (!g_available.load(std::memory_order_acquire) ||
        !g_widget_path_bridge_available.load(std::memory_order_acquire)) {
        return;
    }

    void* const movie_context =
        static_cast<std::byte*>(page) + kPageMovieContextOffset;
    const char* const panel = color_picker_widget_host_path(host);
    if (!panel) return;
    for (std::size_t row_index = 0;
         row_index < kMaximumPropertyRows;
         ++row_index) {
        char path[128]{};
        const int written = _snprintf_s(
            path,
            std::size(path),
            _TRUNCATE,
            "%s/Item_%zu_0/Widgets/ColorPicker",
            panel,
            row_index);
        if (written <= 0) continue;
        (void)set_existing_widget_visibility(movie_context, path, false);
    }
}

void synchronize_color_picker_previews(void* current_page) noexcept {
    if (!current_page) return;

    struct PendingPreview {
        detail::ColorPickerState* state{};
        PreviewLocation location{};
    };

    std::vector<PendingPreview> pending{};
    try {
        std::lock_guard lock(g_mutex);
        pending.reserve(g_preview_bindings.size());
        for (const PreviewBinding& binding : g_preview_bindings) {
            // A logical binding can outlive one physical page instance. Only
            // dereference the page that the game's frame pump has proven is
            // current; leave closed-page updates pending until that row is
            // materialized and rebound again.
            if (binding.state &&
                binding.location.valid_for(current_page) &&
                binding.state->consume_preview_refresh()) {
                pending.push_back({binding.state, binding.location});
            }
        }
    } catch (...) {
        return;
    }

    for (const PendingPreview& update : pending) {
        if (!update.location.page) continue;
        if (!refresh_color_preview(
                update.location, update.state->native_value())) {
            update.state->request_preview_refresh();
        }
    }
}

ColorPickerRowPresentationScope::ColorPickerRowPresentationScope(
    void* page,
    detail::ColorPickerState* state,
    ColorPickerWidgetHost host) noexcept {
    if (!page || !state || g_color_row_construction.active ||
        !g_available.load(std::memory_order_acquire)) {
        return;
    }

    std::size_t row_index{};
    if (!read_property_row_index(page, row_index)) return;

    g_color_row_construction = {
        .page = page,
        .row_index = row_index,
        .state = state,
        .packed_color = state->native_value(),
        .host = host,
        .active = true,
    };
    active_ = true;
}

ColorPickerRowPresentationScope::~ColorPickerRowPresentationScope() {
    if (active_) {
        g_color_widget_route_source = nullptr;
        g_color_row_construction = {};
    }
}

bool install_color_picker(const ModuleView& game) noexcept {
    g_available.store(false, std::memory_order_release);
    g_widget_path_bridge_available.store(false, std::memory_order_release);
    g_color_widget_route_source = nullptr;
    g_preview_fault_logs.store(0, std::memory_order_release);
    g_action_widget_producer_hook.reset();
    g_original_action_widget_producer.store(
        nullptr, std::memory_order_release);
    {
        std::lock_guard lock(g_mutex);
        if (!g_active.expired() || g_completed) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color Picker cannot be reinstalled while a session is outstanding");
            return false;
        }
        g_active.reset();
        for (PreviewBinding& binding : g_preview_bindings) {
            binding.location = {};
        }
    }
    g_native = {};

    if (game.timestamp() != kSupportedTimestamp ||
        game.image_size() != kSupportedImageSize) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker unavailable: unsupported PE identity timestamp=0x%08X size=0x%zX",
            static_cast<unsigned>(game.timestamp()), game.image_size());
        return false;
    }

    auto* const build_job = game.at_rva(kBuildMenuWindowJobRva);
    auto* const submit_consume = game.at_rva(kSubmitConsumeRva);
    auto* const heap_provider = game.at_rva(kHeapProviderRva);
    auto* const allocate = game.at_rva(kGameAllocateRva);
    auto* const palette_constructor =
        game.at_rva(kColorPaletteConstructorRva);
    auto* const palette_destructor =
        game.at_rva(kColorPaletteDestructorRva);
    auto* const populate_palette = game.at_rva(kColorPalettePopulateRva);
    auto* const bridge_scene_proxy = game.at_rva(kSceneProxyBridgeRva);
    auto* const destroy_scene_obj_proxy =
        game.at_rva(kSceneObjProxyDestructorRva);
    auto* const color_control_constructor =
        game.at_rva(kColorControlConstructorRva);
    auto* const action_widget_producer =
        game.at_rva(kGameOptionsActionWidgetProducerRva);
    auto* const resolve_path = game.at_rva(kScaleformPathResolverRva);
    auto* const set_visible =
        game.at_rva(kScaleformVisibilitySetterRva);
    auto* const value_exists =
        game.at_rva(kScaleformValueExistsRva);
    auto* const set_color_transform =
        game.at_rva(kScaleformColorTransformSetterRva);
    const auto* const movie_name = reinterpret_cast<const wchar_t*>(
        game.at_rva(kColorPickerMovieNameRva));

    if (!build_job || !submit_consume || !heap_provider || !allocate ||
        !palette_constructor || !palette_destructor || !populate_palette ||
        !bridge_scene_proxy ||
        !destroy_scene_obj_proxy || !color_control_constructor ||
        !action_widget_producer || !resolve_path || !set_visible ||
        !value_exists || !set_color_transform ||
        !game.matches(build_job, kBuildMenuWindowJobPattern) ||
        !game.matches(submit_consume, kSubmitConsumePattern) ||
        !game.matches(heap_provider, kHeapProviderPattern) ||
        !game.matches(allocate, kGameAllocatePattern) ||
        !game.matches(palette_constructor, kColorPaletteConstructorPattern) ||
        !game.matches(palette_destructor, kColorPaletteDestructorPattern) ||
        !game.matches(populate_palette, kColorPalettePopulatePattern) ||
        !game.matches(bridge_scene_proxy, kSceneProxyBridgePattern) ||
        !game.matches(
            destroy_scene_obj_proxy, kSceneObjProxyDestructorPattern) ||
        !game.matches(
            color_control_constructor, kColorControlConstructorPattern) ||
        !game.matches(
            action_widget_producer,
            kGameOptionsActionWidgetProducerPattern) ||
        !game.matches(resolve_path, kScaleformPathResolverPattern) ||
        !game.matches(set_visible, kScaleformVisibilitySetterPattern) ||
        !game.matches(value_exists, kScaleformValueExistsPattern) ||
        !game.matches(
            set_color_transform,
            kScaleformColorTransformSetterPattern) ||
        !validate_movie_name(game, movie_name)) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker unavailable: one or more exact RVA validations failed");
        return false;
    }

    g_native = {
        .build_job = reinterpret_cast<BuildMenuWindowJobFn>(build_job),
        .submit_consume = reinterpret_cast<SubmitConsumeFn>(submit_consume),
        .heap_provider = reinterpret_cast<HeapProviderFn>(heap_provider),
        .allocate = reinterpret_cast<GameAllocateFn>(allocate),
        .palette_constructor = reinterpret_cast<ColorPaletteConstructorFn>(
            palette_constructor),
        .palette_destructor = reinterpret_cast<ColorPaletteDestructorFn>(
            palette_destructor),
        .populate_palette = reinterpret_cast<ColorPalettePopulateFn>(
            populate_palette),
        .bridge_scene_proxy = reinterpret_cast<SceneProxyBridgeFn>(
            bridge_scene_proxy),
        .destroy_scene_obj_proxy =
            reinterpret_cast<SceneObjProxyDestructorFn>(
                destroy_scene_obj_proxy),
        .color_control_constructor =
            reinterpret_cast<ColorControlConstructorFn>(
                color_control_constructor),
        .resolve_path = reinterpret_cast<ScaleformPathResolverFn>(
            resolve_path),
        .set_visible = reinterpret_cast<ScaleformVisibilitySetterFn>(
            set_visible),
        .value_exists = reinterpret_cast<ScaleformValueExistsFn>(
            value_exists),
        .set_color_transform =
            reinterpret_cast<ScaleformColorTransformSetterFn>(
                set_color_transform),
        .movie_name = movie_name,
    };

    auto hook = SafetyHookInline::create(
        action_widget_producer,
        reinterpret_cast<void*>(&action_widget_producer_detour),
        SafetyHookInline::StartDisabled);
    if (!hook) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker preview producer hook could not be prepared (SafetyHook error type %u)",
            static_cast<unsigned>(hook.error().type));
        g_native = {};
        return false;
    }
    g_action_widget_producer_hook = std::move(*hook);
    g_original_action_widget_producer.store(
        g_action_widget_producer_hook.original<
            GameOptionsActionWidgetProducerFn>(),
        std::memory_order_release);
    if (auto enabled = g_action_widget_producer_hook.enable(); !enabled) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker preview producer hook could not be enabled (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        g_action_widget_producer_hook.reset();
        g_original_action_widget_producer.store(
            nullptr, std::memory_order_release);
        g_native = {};
        return false;
    }
    g_available.store(true, std::memory_order_release);
    erui::detail::logf(
        erui::LogLevel::info,
        "Color Picker installed: builder=%p submit=%p palette=(%p,%p,%p) dialog=%p rowProducer=%p movie='%ls'",
        build_job,
        submit_consume,
        palette_constructor,
        palette_destructor,
        populate_palette,
        color_control_constructor,
        action_widget_producer,
        movie_name);
    return true;
}

void remove_color_picker() noexcept {
    g_available.store(false, std::memory_order_release);
    g_widget_path_bridge_available.store(false, std::memory_order_release);
    g_color_widget_route_source = nullptr;
    g_action_widget_producer_hook.reset();
    g_original_action_widget_producer.store(
        nullptr, std::memory_order_release);
    std::lock_guard lock(g_mutex);
    if (!g_active.expired() || g_completed) {
        // The host is pinned for process lifetime. Keep the resolved entries
        // alive for callable targets already owned by the game, but reject all
        // new requests. Hot unload is deliberately unsupported.
        erui::detail::logf(
            erui::LogLevel::warning,
            "Color Picker stopped with an outstanding session; native interfaces retained until process exit");
        return;
    }
    g_active.reset();
    g_preview_bindings.clear();
    g_native = {};
}

bool color_picker_available() noexcept {
    return g_available.load(std::memory_order_acquire) &&
        native_dialog_available();
}

bool color_picker_session_active() noexcept {
    std::lock_guard lock(g_mutex);
    return !g_active.expired();
}

ERUI_Result request_color_picker(
    void* page,
    detail::ColorPickerState& state,
    detail::ColorAction changed_action) noexcept {
    if (!page) return ERUI_INVALID_ARGUMENT;
    if (!color_picker_available()) {
        return ERUI_NOT_SUPPORTED;
    }
    if (!read_page_preflight(page)) return ERUI_QUEUE_FULL;

    const std::uint32_t initial_packed_color = state.native_value();

    std::shared_ptr<Session> session;
    try {
        session = std::make_shared<Session>();
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
    session->state = &state;
    session->changed_action = changed_action;
    session->initial_color = initial_packed_color;
    session->current_color.store(
        initial_packed_color, std::memory_order_relaxed);
    session->preview = preview_location(&state, page);

    {
        std::lock_guard lock(g_mutex);
        if (!g_active.expired() || g_completed) return ERUI_QUEUE_FULL;
        g_active.reset();
        g_active = session;
    }

    try {
        std::shared_ptr<PaletteOwner> palette = create_palette();
        if (!palette) {
            clear_failed_session(session);
            return ERUI_OUT_OF_MEMORY;
        }

        NativeFactory factory{
            [session, palette](OpaqueSceneProxy& source) noexcept -> void* {
                return create_dialog_for_factory(session, palette, source);
            }};
        ColorPickerMovieDescriptor descriptor{};
        descriptor.movie_name = g_native.movie_name;

        void* job{};
        unsigned long exception{};
        if (!build_native_job(
                &job,
                static_cast<std::byte*>(page) +
                    kPageComponentStackOffset,
                &descriptor,
                &factory,
                exception) ||
            !job) {
            clear_failed_session(session);
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color Picker job construction failed safely: seh=0x%08lX",
                exception);
            return exception ? ERUI_INTERNAL_ERROR : ERUI_OUT_OF_MEMORY;
        }

        void* const submitted_job = job;
        {
            std::lock_guard lock(g_mutex);
            if (g_active.lock() != session) {
                // Do not release a native intrusive job here. No consuming
                // handoff has occurred and its exact partial ownership is now
                // ambiguous; this can only happen during unsupported teardown.
                return ERUI_INTERNAL_ERROR;
            }
            session->job = job;
        }

        if (!submit_native_job(
                static_cast<std::byte*>(page) + kPageJobQueueOffset,
                &job,
                exception)) {
            clear_failed_session(session);
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color Picker consuming submission failed safely: job=%p seh=0x%08lX",
                submitted_job,
                exception);
            return ERUI_INTERNAL_ERROR;
        }
        session->submitted.store(true, std::memory_order_release);

        erui::detail::logf(
            erui::LogLevel::info,
            "Color Picker scheduled: state=%p page=%p job=%p initial=0x%08X",
            &state,
            page,
            submitted_job,
            static_cast<unsigned>(initial_packed_color));
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        clear_failed_session(session);
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        clear_failed_session(session);
        return ERUI_INTERNAL_ERROR;
    }
}

void observe_color_picker_job_poll(
    void* job,
    void* scheduler_state) noexcept {
    if (!job || !scheduler_state) return;

    std::uint32_t kind{};
    if (!read_scheduler_kind(scheduler_state, kind)) return;
    if (kind != 2 && kind != 3) return;

    std::shared_ptr<Session> completed;
    {
        std::lock_guard lock(g_mutex);
        std::shared_ptr<Session> active = g_active.lock();
        if (!active) {
            // Discard a stale identity before the game's allocator can reuse
            // the same native job address for an unrelated menu operation.
            g_active.reset();
            return;
        }
        if (active->job != job || g_completed) return;
        std::uint32_t expected = 0;
        if (!active->terminal_kind.compare_exchange_strong(
                expected,
                kind,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            return;
        }
        completed = active;
    }

    const std::uint32_t completed_color =
        completed->current_color.load(std::memory_order_acquire);
    if (kind == 2 && completed->state) {
        completed->changed = completed->state->accept_native(
            completed_color, completed->accepted_color);
        const std::uint32_t canonical = completed->state->native_value();
        if (completed->preview.page &&
            !refresh_color_preview(completed->preview, canonical)) {
            completed->state->request_preview_refresh();
            erui::detail::logf(
                erui::LogLevel::warning,
                "Color preview could not be refreshed after acceptance; it will retry on page materialization");
        }
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Color Picker reached terminal native state: job=%p kind=%u color=0x%08X",
        job,
        static_cast<unsigned>(kind),
        static_cast<unsigned>(completed_color));

    {
        std::lock_guard lock(g_mutex);
        if (!g_completed) g_completed = completed;
        g_active.reset();
    }
}

void dispatch_color_picker_callbacks() noexcept {
    std::shared_ptr<Session> completed;
    {
        std::lock_guard lock(g_mutex);
        completed = std::move(g_completed);
    }
    if (!completed || !completed->state) return;

    const std::uint32_t kind =
        completed->terminal_kind.load(std::memory_order_acquire);
    if (kind == 2 && completed->changed && completed->changed_action) {
        completed->changed_action.invoke(completed->accepted_color);
    }
}

} // namespace erui::native
