#include "native_popup_choice.hpp"

#include "menu_compiler.hpp"
#include "native_menu.hpp"
#include "runtime_log.hpp"

#include <Windows.h>
#include <safetyhook.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace erui::native {
namespace {

constexpr std::size_t kNativeListCapacity = 32;
constexpr std::size_t kNativeElementValueOffset = 0x08;
constexpr std::uint32_t kFaultLogLimit = 8;

SafetyHookInline g_list_provider_hook{};
SafetyHookInline g_list_template_hook{};
std::atomic<PopupChoiceListProviderFn> g_original_list_provider{};
std::atomic<PopupChoiceListTemplateFn> g_original_list_template{};
std::atomic<std::uint32_t> g_fault_count{};
GameAddresses g_popup_addresses{};
thread_local const erui::detail::CompiledRow* g_active_row{};

class ActiveRowScope {
public:
    explicit ActiveRowScope(
        const erui::detail::CompiledRow* row) noexcept
        : previous_(g_active_row) {
        g_active_row = row;
    }

    ~ActiveRowScope() { g_active_row = previous_; }

    ActiveRowScope(const ActiveRowScope&) = delete;
    ActiveRowScope& operator=(const ActiveRowScope&) = delete;

private:
    const erui::detail::CompiledRow* previous_{};
};

void log_bridge_fault(const char* message) noexcept {
    const std::uint32_t fault =
        g_fault_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (fault <= kFaultLogLimit) {
        erui::detail::logf(
            erui::LogLevel::error,
            "popup-choice list bridge: %s (fault=%u)",
            message,
            static_cast<unsigned>(fault));
    } else if (fault == kFaultLogLimit + 1) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "popup-choice list bridge: further faults are suppressed");
    }
}

const void* provider_state(const void* provider) noexcept {
    if (!provider) return nullptr;
#if defined(_MSC_VER)
    __try {
#endif
        return *reinterpret_cast<void* const*>(
            static_cast<const std::byte*>(provider) + sizeof(void*));
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
#endif
}

bool executable_address(const void* address) noexcept {
    if (!address) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT) {
        return false;
    }
    const DWORD protection = memory.Protect & 0xFFu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

bool rebuild_list(
    void* list,
    const erui::detail::CompiledRow& row) noexcept {
    if (!list || !g_popup_addresses.text_ref_label ||
        row.choice_ids.empty() ||
        row.choice_ids.size() > kNativeListCapacity) {
        return false;
    }

#if defined(_MSC_VER)
    __try {
#endif
        auto* bytes = static_cast<std::byte*>(list);
        auto* count = reinterpret_cast<std::uint64_t*>(
            bytes + popup_choice_list_count_offset);
        const std::uint64_t original_count = *count;
        if (original_count == 0 || original_count > kNativeListCapacity) {
            return false;
        }

        void* first = bytes + popup_choice_element_offset;
        void** element_vtable = *static_cast<void***>(first);
        if (!element_vtable || !executable_address(element_vtable[0])) {
            return false;
        }

        // Validate the complete native template before releasing any element.
        // All observed entries use the same destructor contract, although the
        // vtable pointer itself is retained from the first element.
        for (std::uint64_t index = 0; index < original_count; ++index) {
            void* element = bytes + popup_choice_element_offset +
                index * popup_choice_element_stride;
            void** current_vtable = *static_cast<void***>(element);
            if (!current_vtable ||
                !executable_address(current_vtable[0])) {
                return false;
            }
        }

        using ElementDestructorFn = void(__fastcall*)(void*, std::uint32_t);
        // Retire entries from the end and publish the shrinking live range
        // before each destructor call. If a native destructor faults, later
        // cleanup can only observe entries that have not been released yet.
        for (std::uint64_t remaining = original_count;
             remaining != 0;
             --remaining) {
            const std::uint64_t index = remaining - 1;
            void* element = bytes + popup_choice_element_offset +
                index * popup_choice_element_stride;
            void** current_vtable = *static_cast<void***>(element);
            *count = index;
            reinterpret_cast<ElementDestructorFn>(current_vtable[0])(
                element, 0);
        }

        std::memset(
            bytes + popup_choice_element_offset,
            0,
            kNativeListCapacity * popup_choice_element_stride);
        for (std::size_t index = 0; index < row.choice_ids.size(); ++index) {
            auto* element = bytes + popup_choice_element_offset +
                index * popup_choice_element_stride;
            *reinterpret_cast<void***>(element) = element_vtable;
            *reinterpret_cast<std::uint8_t*>(
                element + kNativeElementValueOffset) =
                static_cast<std::uint8_t>(index + 1u);
            if (!g_popup_addresses.text_ref_label(
                    element + popup_choice_element_text_offset,
                    row.choice_ids[index])) {
                return false;
            }
            // Only fully constructed elements are part of the live range.
            *count = index + 1u;
        }
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

void* __fastcall list_provider_detour(
    void* provider,
    void* destination,
    std::uintptr_t argument3,
    std::uintptr_t argument4) noexcept {
    PopupChoiceListProviderFn original =
        g_original_list_provider.load(std::memory_order_acquire);
    if (!original) return nullptr;

    const auto* row = find_popup_choice_row_by_native_state(
        provider_state(provider));
    ActiveRowScope scope(row);
    return original(provider, destination, argument3, argument4);
}

void* __fastcall list_template_detour(
    void* list,
    std::uint8_t selected_index) noexcept {
    PopupChoiceListTemplateFn original =
        g_original_list_template.load(std::memory_order_acquire);
    if (!original) return nullptr;

    void* result = original(list, selected_index);
    const erui::detail::CompiledRow* row = g_active_row;
    if (row && !rebuild_list(list, *row)) {
        // Validation failures leave the vanilla list intact. Mutation-time
        // failures leave only successfully constructed entries published, so
        // native cleanup never walks released or partially initialized slots.
        log_bridge_fault("could not rebuild a registered option list");
    }
    return result;
}

void clear_bridge() noexcept {
    // Stop new outer calls before removing the nested template detour.
    g_list_provider_hook.reset();
    g_list_template_hook.reset();
    g_original_list_provider.store(nullptr, std::memory_order_release);
    g_original_list_template.store(nullptr, std::memory_order_release);
    g_popup_addresses = {};
    g_active_row = nullptr;
}

} // namespace

bool install_native_popup_choice_bridge(
    const GameAddresses& addresses) noexcept {
    clear_bridge();
    g_fault_count.store(0, std::memory_order_release);
    if (!addresses.popup_choice_list_provider ||
        !addresses.popup_choice_list_template ||
        !addresses.text_ref_label) {
        return false;
    }

    auto template_result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.popup_choice_list_template),
        reinterpret_cast<void*>(&list_template_detour),
        SafetyHookInline::StartDisabled);
    if (!template_result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Popup-choice template hook could not be prepared (SafetyHook error type %u)",
            static_cast<unsigned>(template_result.error().type));
        return false;
    }

    auto provider_result = SafetyHookInline::create(
        reinterpret_cast<void*>(addresses.popup_choice_list_provider),
        reinterpret_cast<void*>(&list_provider_detour),
        SafetyHookInline::StartDisabled);
    if (!provider_result) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Popup-choice provider hook could not be prepared (SafetyHook error type %u)",
            static_cast<unsigned>(provider_result.error().type));
        return false;
    }

    g_popup_addresses = addresses;
    g_list_template_hook = std::move(*template_result);
    g_list_provider_hook = std::move(*provider_result);
    g_original_list_template.store(
        g_list_template_hook.original<PopupChoiceListTemplateFn>(),
        std::memory_order_release);
    g_original_list_provider.store(
        g_list_provider_hook.original<PopupChoiceListProviderFn>(),
        std::memory_order_release);

    if (auto enabled = g_list_template_hook.enable(); !enabled) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Popup-choice template hook could not be enabled (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        clear_bridge();
        return false;
    }
    if (auto enabled = g_list_provider_hook.enable(); !enabled) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Popup-choice provider hook could not be enabled (SafetyHook error type %u)",
            static_cast<unsigned>(enabled.error().type));
        clear_bridge();
        return false;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Popup-choice list bridge installed (registered selectors only)");
    return true;
}

void remove_native_popup_choice_bridge() noexcept {
    clear_bridge();
    g_fault_count.store(0, std::memory_order_release);
}

} // namespace erui::native
