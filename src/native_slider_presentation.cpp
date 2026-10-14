#include "native_slider_presentation.hpp"

#include "native_title_bridge.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"

#include <Windows.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

namespace erui::native {
namespace {

constexpr std::size_t kMovieContextOffset = 0xA38;
constexpr std::size_t kCurrentValueOffset = 0xD28;
constexpr std::size_t kDefaultValueOffset = 0xD2C;
constexpr std::size_t kMinimumValueOffset = 0xD30;
constexpr std::size_t kMaximumValueOffset = 0xD34;
constexpr std::size_t kStepValueOffset = 0xD38;
constexpr std::size_t kBoundValuePointerOffset = 0xD90;
constexpr std::uint32_t kFaultLogLimit = 8;

struct PresentationSlot {
    std::uintptr_t bound_value{};
    std::vector<std::wstring> values{};
};

std::vector<PresentationSlot> g_slots{};
GameAddresses g_addresses{};
SafetyHookInline g_slider_refresh_hook{};
std::atomic<SliderRefreshFn> g_original_slider_refresh{};
std::atomic<std::uint32_t> g_fault_logs{};
std::atomic_bool g_presentation_active{};
thread_local const PresentationSlot* g_constructing_slot{};
thread_local std::int32_t g_constructing_value{};

void log_fault(const char* operation, unsigned long exception_code) noexcept {
    const std::uint32_t fault =
        g_fault_logs.fetch_add(1, std::memory_order_relaxed) + 1;
    if (fault <= kFaultLogLimit) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Formatted-slider %s failed (fault=%u seh=0x%08lX)",
            operation,
            static_cast<unsigned>(fault),
            exception_code);
    } else if (fault == kFaultLogLimit + 1) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Formatted-slider bridge suppressed further fault logs");
    }
}

bool resolve_scaleform_path(
    void* movie_context,
    void* destination,
    const char* path,
    unsigned long& out_seh_code) noexcept {
    out_seh_code = 0;
#if defined(_MSC_VER)
    __try {
#endif
        return g_addresses.scaleform_path_resolver &&
            g_addresses.scaleform_path_resolver(
                movie_context, destination, path) == destination;
#if defined(_MSC_VER)
    } __except (out_seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool set_scaleform_text(
    void* value,
    const wchar_t* text,
    unsigned long& out_seh_code) noexcept {
    out_seh_code = 0;
#if defined(_MSC_VER)
    __try {
#endif
        if (!g_addresses.scaleform_text_setter || !text) return false;
        g_addresses.scaleform_text_setter(value, text);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool destroy_scaleform_result(
    void* nested_member,
    unsigned long& out_seh_code) noexcept {
    out_seh_code = 0;
#if defined(_MSC_VER)
    __try {
#endif
        if (!g_addresses.scaleform_result_destructor) return false;
        g_addresses.scaleform_result_destructor(nested_member);
        return true;
#if defined(_MSC_VER)
    } __except (out_seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool replace_value_text(
    void* slider,
    const char* path,
    const wchar_t* text) noexcept {
    alignas(16) std::array<std::byte, scaleform_path_result_size> result{};
    auto* const bytes = static_cast<std::byte*>(slider);
    unsigned long seh_code = 0;
    if (!resolve_scaleform_path(
            bytes + kMovieContextOffset,
            result.data(),
            path,
            seh_code)) {
        log_fault("path resolution", seh_code);
        return false;
    }

    const bool assigned = set_scaleform_text(
        result.data() + scaleform_text_value_offset,
        text,
        seh_code);
    if (!assigned) log_fault("text assignment", seh_code);

    unsigned long cleanup_exception = 0;
    if (!destroy_scaleform_result(
            result.data() + scaleform_destructor_member_offset,
            cleanup_exception)) {
        log_fault("temporary-result cleanup", cleanup_exception);
        return false;
    }
    return assigned;
}

const PresentationSlot* find_slot(std::uintptr_t bound_value) noexcept {
    const auto found = std::lower_bound(
        g_slots.begin(),
        g_slots.end(),
        bound_value,
        [](const PresentationSlot& slot, std::uintptr_t value) {
            return slot.bound_value < value;
        });
    return found != g_slots.end() && found->bound_value == bound_value
        ? &*found
        : nullptr;
}

void apply_presentation(void* slider) noexcept {
    if (!slider) return;
#if defined(_MSC_VER)
    unsigned long seh_code = 0;
    __try {
#endif
        auto* const bytes = static_cast<std::byte*>(slider);
        const std::uintptr_t bound_value = reinterpret_cast<std::uintptr_t>(
            *reinterpret_cast<void**>(bytes + kBoundValuePointerOffset));
        const std::int32_t current = *reinterpret_cast<const std::int32_t*>(
            bytes + kCurrentValueOffset);
        const std::int32_t baseline = *reinterpret_cast<const std::int32_t*>(
            bytes + kDefaultValueOffset);
        const std::int32_t minimum = *reinterpret_cast<const std::int32_t*>(
            bytes + kMinimumValueOffset);
        const std::int32_t maximum = *reinterpret_cast<const std::int32_t*>(
            bytes + kMaximumValueOffset);
        const std::int32_t step = *reinterpret_cast<const std::int32_t*>(
            bytes + kStepValueOffset);

        const PresentationSlot* slot = nullptr;
        if (const PresentationSlot* const constructing = g_constructing_slot;
            constructing && minimum == 0 && step == 1 &&
            maximum >= 1 &&
            static_cast<std::size_t>(maximum) + 1 ==
                constructing->values.size() &&
            current == g_constructing_value && current >= minimum &&
            current <= maximum && baseline >= minimum && baseline <= maximum) {
            // The constructor's first structurally valid refresh consumes the
            // TLS marker. Later refreshes use SliderCtrl::bound_value.
            slot = constructing;
            g_constructing_slot = nullptr;
            g_constructing_value = 0;
        } else {
            slot = find_slot(bound_value);
        }
        if (!slot) return;

        const std::vector<std::wstring>& values = slot->values;
        if (minimum != 0 || step != 1 || maximum < 1 ||
            static_cast<std::size_t>(maximum) + 1 != values.size() ||
            current < minimum || current > maximum ||
            baseline < minimum || baseline > maximum) {
            log_fault("native range validation", 0);
            return;
        }

        (void)replace_value_text(
            slider,
            "Text_0",
            values[static_cast<std::size_t>(current)].c_str());
        (void)replace_value_text(
            slider,
            "Text_1",
            values[static_cast<std::size_t>(baseline)].c_str());
#if defined(_MSC_VER)
    } __except (seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        log_fault("owned SliderCtrl inspection", seh_code);
    }
#endif
}

void __fastcall slider_refresh_detour(void* slider) noexcept {
    const SliderRefreshFn original =
        g_original_slider_refresh.load(std::memory_order_acquire);
    if (original) original(slider);
    if (g_presentation_active.load(std::memory_order_acquire)) {
        apply_presentation(slider);
    }
}

std::size_t collect_slots() {
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.menu) return 0;

    std::vector<PresentationSlot> collected{};
    const std::size_t expected =
        runtime.menu->modeled_numeric_slider_count +
        runtime.menu->modeled_custom_slider_count;
    collected.reserve(expected);
    for (const erui::detail::CompiledPage& page : runtime.menu->pages) {
        for (const erui::detail::CompiledRow& row : page.rows) {
            if ((row.kind != erui::RowKind::numeric_slider &&
                    row.kind != erui::RowKind::custom_slider) ||
                !row.byte_value ||
                row.slider_value_texts.size() <
                    ERUI_SLIDER_MIN_VALUE_COUNT ||
                row.slider_value_texts.size() >
                    ERUI_SLIDER_MAX_VALUE_COUNT) {
                continue;
            }
            collected.push_back({
                reinterpret_cast<std::uintptr_t>(
                    const_cast<std::uint8_t*>(row.byte_value)),
                row.slider_value_texts,
            });
        }
    }
    std::sort(
        collected.begin(),
        collected.end(),
        [](const PresentationSlot& left, const PresentationSlot& right) {
            return left.bound_value < right.bound_value;
        });
    g_slots = std::move(collected);
    return g_slots.size();
}

} // namespace

bool install_native_slider_presentation(
    const GameAddresses& addresses) noexcept {
    remove_native_slider_presentation();
    if (g_slider_refresh_hook) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted-slider presentation cannot be reinstalled after its process-lifetime hook was created");
        return false;
    }

    std::size_t row_count = 0;
    try {
        row_count = collect_slots();
    } catch (const std::bad_alloc&) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted-slider bridge ran out of memory while indexing rows");
        remove_native_slider_presentation();
        return false;
    } catch (...) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted-slider bridge could not index compiled rows");
        remove_native_slider_presentation();
        return false;
    }
    if (row_count == 0) return true;

    if (!addresses.slider_refresh || !addresses.title_bridge_complete()) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted sliders requested by %zu rows, but SliderCtrl refresh or Scaleform text interfaces are unavailable",
            row_count);
        remove_native_slider_presentation();
        return false;
    }

    g_addresses = addresses;
    auto result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.slider_refresh),
        reinterpret_cast<void*>(&slider_refresh_detour),
        SafetyHookInline::StartDisabled);
    if (!result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted-slider hook creation failed (SafetyHook error type %u)",
            static_cast<unsigned>(result.error().type));
        remove_native_slider_presentation();
        return false;
    }

    g_slider_refresh_hook = std::move(*result);
    g_original_slider_refresh.store(
        g_slider_refresh_hook.original<SliderRefreshFn>(),
        std::memory_order_release);
    if (auto enabled = g_slider_refresh_hook.enable(); !enabled) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Formatted-slider hook enable failed (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        remove_native_slider_presentation();
        return false;
    }
    g_presentation_active.store(true, std::memory_order_release);

    erui::detail::logf(
        erui::LogLevel::info,
        "Formatted-slider presentation installed: ownedRows=%zu target=%p",
        row_count,
        reinterpret_cast<void*>(addresses.slider_refresh));
    return true;
}

void remove_native_slider_presentation() noexcept {
    g_presentation_active.store(false, std::memory_order_release);
    if (g_slider_refresh_hook) {
        if (auto disabled = g_slider_refresh_hook.disable(); !disabled) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Formatted-slider hook disable failed; retaining immutable presentation state for safety");
        } else {
            // The host DLL is pinned and hot unloading is unsupported. Keep
            // the disabled SafetyHook trampoline, copied strings, address
            // table, and original pointer alive until process exit: a thread
            // may already have branched into the destination detour before
            // SafetyHook restores the target, yet not have executed any
            // bookkeeping that could be waited on safely here.
            erui::detail::logf(
                erui::LogLevel::trace,
                "Formatted-slider hook disabled; process-lifetime presentation backing retained");
        }
        g_constructing_slot = nullptr;
        g_constructing_value = 0;
        return;
    }

    // Before the first hook is created there cannot be an in-flight detour,
    // so failed preparation may release its temporary state normally.
    g_original_slider_refresh.store(nullptr, std::memory_order_release);
    g_slots.clear();
    g_addresses = {};
    g_fault_logs.store(0, std::memory_order_release);
    g_constructing_slot = nullptr;
    g_constructing_value = 0;
}

void begin_native_slider_presentation_row(
    volatile std::uint8_t* bound_value) noexcept {
    g_constructing_slot = find_slot(reinterpret_cast<std::uintptr_t>(
        const_cast<std::uint8_t*>(bound_value)));
    g_constructing_value = g_constructing_slot && bound_value
        ? static_cast<std::int32_t>(*bound_value)
        : 0;
}

void end_native_slider_presentation_row() noexcept {
    g_constructing_slot = nullptr;
    g_constructing_value = 0;
}

} // namespace erui::native
