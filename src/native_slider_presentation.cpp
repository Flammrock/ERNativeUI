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
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

using ValueTexts = std::vector<std::wstring>;

struct PresentationSlot {
    std::uintptr_t bound_value{};
    std::shared_ptr<const ValueTexts> values{};
};

using PresentationBank = std::vector<PresentationSlot>;

// One immutable bank is published for the complete API update generation.
// A refresh retains the bank while it borrows a table, so old strings are
// reclaimed only after every in-flight detour has released them.
std::shared_ptr<const PresentationBank> g_desired_bank{};
std::atomic<std::shared_ptr<const PresentationBank>> g_applied_bank{};
std::unordered_map<std::uintptr_t, ValueTexts>
    g_preconfiguration_updates{};
std::mutex g_value_lock{};
bool g_values_configured{};
bool g_values_pending{};
GameAddresses g_addresses{};
SafetyHookInline g_slider_refresh_hook{};
std::atomic<SliderRefreshFn> g_original_slider_refresh{};
std::atomic<std::uint32_t> g_fault_logs{};
std::atomic_bool g_presentation_active{};
thread_local std::uintptr_t g_constructing_bound_value{};
thread_local std::int32_t g_constructing_value{};

[[nodiscard]] bool valid_update(
    const SliderPresentationUpdate& update) noexcept {
    if (!update.bound_value ||
        update.values.size() < ERUI_SLIDER_MIN_VALUE_COUNT ||
        update.values.size() > ERUI_SLIDER_MAX_VALUE_COUNT) {
        return false;
    }
    return std::all_of(
        update.values.begin(), update.values.end(),
        [](const std::wstring& value) {
            return !value.empty();
        });
}

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

template <class Bank>
auto find_slot(Bank& bank, std::uintptr_t bound_value) noexcept {
    const auto found = std::lower_bound(
        bank.begin(),
        bank.end(),
        bound_value,
        [](const PresentationSlot& slot, std::uintptr_t value) {
            return slot.bound_value < value;
        });
    return found != bank.end() && found->bound_value == bound_value
        ? &*found : nullptr;
}

[[nodiscard]] bool apply_presentation_from_bank(
    void* slider,
    const PresentationBank& bank) noexcept {
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
        if (g_constructing_bound_value != 0) {
            slot = find_slot(bank, g_constructing_bound_value);
        }
        const ValueTexts* values = slot && slot->values
            ? slot->values.get() : nullptr;
        if (slot && values &&
            minimum == 0 && step == 1 && maximum >= 1 &&
            static_cast<std::size_t>(maximum) + 1 == values->size() &&
            current == g_constructing_value && current >= minimum &&
            current <= maximum && baseline >= minimum && baseline <= maximum) {
            // The constructor's first structurally valid refresh consumes the
            // TLS marker. Later refreshes use SliderCtrl::bound_value.
            g_constructing_bound_value = 0;
            g_constructing_value = 0;
        } else {
            slot = find_slot(bank, bound_value);
            values = slot && slot->values ? slot->values.get() : nullptr;
        }
        if (!slot || !values) return false;

        if (minimum != 0 || step != 1 || maximum < 1 ||
            static_cast<std::size_t>(maximum) + 1 != values->size() ||
            current < minimum || current > maximum ||
            baseline < minimum || baseline > maximum) {
            log_fault("native range validation", 0);
            return false;
        }

        const bool current_assigned = replace_value_text(
            slider,
            "Text_0",
            (*values)[static_cast<std::size_t>(current)].c_str());
        (void)replace_value_text(
            slider,
            "Text_1",
            (*values)[static_cast<std::size_t>(baseline)].c_str());
        return current_assigned;
#if defined(_MSC_VER)
    } __except (seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        log_fault("owned SliderCtrl inspection", seh_code);
        return false;
    }
#endif
}

void apply_presentation(void* slider) noexcept {
    if (!slider) return;
    const std::shared_ptr<const PresentationBank> bank =
        g_applied_bank.load(std::memory_order_acquire);
    if (bank) (void)apply_presentation_from_bank(slider, *bank);
}

void __fastcall slider_refresh_detour(void* slider) noexcept {
    const SliderRefreshFn original =
        g_original_slider_refresh.load(std::memory_order_acquire);
    if (original) original(slider);
    if (g_presentation_active.load(std::memory_order_acquire)) {
        apply_presentation(slider);
    }
}

[[nodiscard]] bool refresh_presentation_from_bank(
    void* slider,
    const volatile std::uint8_t* expected_bound_value,
    const PresentationBank& bank,
    SliderRefreshFn original) noexcept {
    unsigned long seh_code = 0;
#if defined(_MSC_VER)
    __try {
#endif
        auto* const bytes = static_cast<std::byte*>(slider);
        const std::uintptr_t bound_value =
            reinterpret_cast<std::uintptr_t>(
                *reinterpret_cast<void**>(
                    bytes + kBoundValuePointerOffset));
        const std::uintptr_t expected =
            reinterpret_cast<std::uintptr_t>(
                const_cast<std::uint8_t*>(expected_bound_value));
        const PresentationSlot* const slot =
            find_slot(bank, bound_value);
        if (bound_value != expected || !slot || !slot->values) {
            return false;
        }

        // Call the SafetyHook trampoline rather than the patched entry. This
        // executes the complete previously approved native/third-party chain
        // exactly once, then reapplies ERNativeUI's copied display table from
        // the same immutable bank retained by the caller.
        original(slider);
        const std::uintptr_t rebound_value =
            reinterpret_cast<std::uintptr_t>(
                *reinterpret_cast<void**>(
                    bytes + kBoundValuePointerOffset));
        const PresentationSlot* const rebound_slot =
            find_slot(bank, rebound_value);
        if (rebound_value != expected || rebound_slot != slot ||
            !rebound_slot || !rebound_slot->values) {
            return false;
        }
        return apply_presentation_from_bank(slider, bank);
#if defined(_MSC_VER)
    } __except (seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        log_fault("explicit UI-thread refresh", seh_code);
        return false;
    }
#endif
}

std::size_t collect_slots() {
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.menu) return 0;

    std::vector<std::pair<std::uintptr_t, ValueTexts>> collected{};
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
            const std::uintptr_t bound_value =
                reinterpret_cast<std::uintptr_t>(
                    const_cast<std::uint8_t*>(row.byte_value));
            collected.emplace_back(
                bound_value, row.slider_value_texts);
        }
    }
    std::sort(
        collected.begin(),
        collected.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
    std::lock_guard lock(g_value_lock);
    for (auto& [bound_value, values] : collected) {
        if (const auto pending =
                g_preconfiguration_updates.find(bound_value);
            pending != g_preconfiguration_updates.end()) {
            if (pending->second.size() != values.size()) {
                throw std::invalid_argument(
                    "formatted-slider preconfiguration cardinality");
            }
            values = pending->second;
        }
    }
    auto bank = std::make_shared<PresentationBank>();
    bank->reserve(collected.size());
    for (auto& [bound_value, values] : collected) {
        bank->push_back({
            bound_value,
            std::make_shared<const ValueTexts>(std::move(values)),
        });
    }
    g_desired_bank = bank;
    g_applied_bank.store(
        std::shared_ptr<const PresentationBank>(std::move(bank)),
        std::memory_order_release);
    g_values_configured = true;
    g_values_pending = false;
    return collected.size();
}

} // namespace

bool install_native_slider_presentation(
    const GameAddresses& addresses,
    const ModuleView* game,
    const PreservedSharedMenuAddresses* preserved_shared) noexcept {
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

    if (preserved_shared &&
        (!game || !revalidate_preserved_shared_menu_entry(
            *game,
            *preserved_shared,
            PreservedSharedMenuEntry::slider_refresh))) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Approved SliderCtrl refresh entry changed immediately before formatted-slider hook creation");
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
        g_constructing_bound_value = 0;
        g_constructing_value = 0;
        return;
    }

    // Before the first hook is created there cannot be an in-flight detour,
    // so failed preparation may release its temporary state normally.
    g_original_slider_refresh.store(nullptr, std::memory_order_release);
    {
        std::lock_guard lock(g_value_lock);
        g_desired_bank.reset();
        g_applied_bank.store(
            std::shared_ptr<const PresentationBank>{},
            std::memory_order_release);
        g_values_configured = false;
        g_values_pending = false;
        // Desired updates accepted after Registry freeze but before hook
        // installation intentionally survive this initial cleanup and are
        // reconciled by collect_slots().
    }
    g_addresses = {};
    g_fault_logs.store(0, std::memory_order_release);
    g_constructing_bound_value = 0;
    g_constructing_value = 0;
}

void begin_native_slider_presentation_row(
    volatile std::uint8_t* bound_value) noexcept {
    g_constructing_bound_value = reinterpret_cast<std::uintptr_t>(
        const_cast<std::uint8_t*>(bound_value));
    g_constructing_value = bound_value
        ? static_cast<std::int32_t>(*bound_value)
        : 0;
}

void end_native_slider_presentation_row() noexcept {
    g_constructing_bound_value = 0;
    g_constructing_value = 0;
}

struct PreparedSliderPresentationUpdates::Impl {
    explicit Impl(std::unique_lock<std::mutex> acquired) noexcept
        : lock(std::move(acquired)) {}

    std::unique_lock<std::mutex> lock{};
    bool configured{};
    std::shared_ptr<const PresentationBank> staged_bank{};
    std::unordered_map<std::uintptr_t, ValueTexts>
        staged_preconfiguration{};
};

PreparedSliderPresentationUpdates::PreparedSliderPresentationUpdates()
    noexcept = default;
PreparedSliderPresentationUpdates::~PreparedSliderPresentationUpdates() =
    default;
PreparedSliderPresentationUpdates::PreparedSliderPresentationUpdates(
    PreparedSliderPresentationUpdates&&) noexcept = default;
PreparedSliderPresentationUpdates&
PreparedSliderPresentationUpdates::operator=(
    PreparedSliderPresentationUpdates&&) noexcept = default;

PreparedSliderPresentationUpdates::operator bool() const noexcept {
    return impl_ != nullptr;
}

ERUI_Result prepare_native_slider_presentation_updates(
    std::vector<SliderPresentationUpdate> updates,
    PreparedSliderPresentationUpdates& output) noexcept {
    if (output || updates.empty()) return ERUI_INVALID_ARGUMENT;
    for (const SliderPresentationUpdate& update : updates) {
        if (!valid_update(update)) return ERUI_INVALID_ARGUMENT;
    }

    try {
        std::unordered_set<std::uintptr_t> unique{};
        unique.reserve(updates.size());
        for (const SliderPresentationUpdate& update : updates) {
            const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(
                const_cast<std::uint8_t*>(update.bound_value));
            if (!unique.insert(key).second) return ERUI_INVALID_ARGUMENT;
        }

        auto prepared =
            std::make_unique<PreparedSliderPresentationUpdates::Impl>(
                std::unique_lock<std::mutex>(g_value_lock));
        prepared->configured = g_values_configured;
        prepared->staged_preconfiguration =
            g_preconfiguration_updates;
        if (!g_values_configured) {
            for (SliderPresentationUpdate& update : updates) {
                const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(
                    const_cast<std::uint8_t*>(update.bound_value));
                prepared->staged_preconfiguration[key] =
                    std::move(update.values);
            }
        } else {
            if (!g_desired_bank) return ERUI_INTERNAL_ERROR;
            auto staged_bank =
                std::make_shared<PresentationBank>(*g_desired_bank);
            for (SliderPresentationUpdate& update : updates) {
                const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(
                    const_cast<std::uint8_t*>(update.bound_value));
                PresentationSlot* const slot = find_slot(*staged_bank, key);
                if (!slot || !slot->values) {
                    return ERUI_INVALID_HANDLE;
                }
                if (slot->values->size() != update.values.size()) {
                    return ERUI_INVALID_ARGUMENT;
                }
                prepared->staged_preconfiguration[key] = update.values;
                slot->values = std::make_shared<const ValueTexts>(
                    std::move(update.values));
            }
            prepared->staged_bank = std::move(staged_bank);
        }
        output.impl_ = std::move(prepared);
        return ERUI_OK;
    } catch (const std::bad_alloc&) {
        return ERUI_OUT_OF_MEMORY;
    } catch (...) {
        return ERUI_INTERNAL_ERROR;
    }
}

void commit_prepared_native_slider_presentation_updates(
    PreparedSliderPresentationUpdates&& prepared) noexcept {
    if (!prepared.impl_) return;
    PreparedSliderPresentationUpdates::Impl& state = *prepared.impl_;
    if (!state.configured) {
        g_preconfiguration_updates.swap(
            state.staged_preconfiguration);
        prepared.impl_.reset();
        return;
    }
    if (!state.staged_bank) {
        prepared.impl_.reset();
        return;
    }
    g_preconfiguration_updates.swap(
        state.staged_preconfiguration);
    g_desired_bank = std::move(state.staged_bank);
    g_values_pending = true;
    prepared.impl_.reset();
}

void synchronize_pending_native_slider_presentations() noexcept {
    std::lock_guard lock(g_value_lock);
    if (!g_values_configured || !g_values_pending) return;
    g_applied_bank.store(g_desired_bank, std::memory_order_release);
    g_values_pending = false;
}

bool native_slider_presentation_updates_pending() noexcept {
    std::lock_guard lock(g_value_lock);
    return g_values_pending;
}

bool refresh_native_slider_presentation(
    void* slider,
    const volatile std::uint8_t* expected_bound_value) noexcept {
    if (!slider || !expected_bound_value ||
        !g_presentation_active.load(std::memory_order_acquire)) {
        return false;
    }

    const std::shared_ptr<const PresentationBank> bank =
        g_applied_bank.load(std::memory_order_acquire);
    const SliderRefreshFn original =
        g_original_slider_refresh.load(std::memory_order_acquire);
    if (!bank || !original) return false;
    return refresh_presentation_from_bank(
        slider, expected_bound_value, *bank, original);
}

} // namespace erui::native
