#include "native_menu.hpp"

#include "native_dialog.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"
#include "root_button_text_override.hpp"
#include "submenu_runtime.hpp"
#include "native_text_input.hpp"
#include "color_picker.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace erui::native {
namespace {

constexpr std::uint32_t kVanillaFallbackTextId = 0x47054;
constexpr std::size_t kButtonDisplayTextSize = 0x60;
constexpr std::size_t kButtonTextReferencesSize = 0x140;
constexpr std::size_t kButtonHelpReferenceOffset = 0x38;
constexpr std::size_t kDisplayOwnerOffset = 0x08;
constexpr std::size_t kDisplayAllocationOffset = 0x10;
constexpr std::size_t kDisplayCapacityOffset = 0x28;
constexpr std::size_t kDisplayReleaseVtableSlot = 0x68 / sizeof(void*);
constexpr std::uint32_t kButtonDiagnosticLogLimit = 128;
// Visual row capacity loaded from 02_040_optionsetting.gfx. Controlled
// vanilla/patched runtime comparisons produced the expected capacities, and
// ERNativeUI row injection does not mutate this field.
constexpr std::size_t kRootVisualCapacityOffset = 0xB14;
constexpr std::size_t kRootMaterializedRowCountOffset = 0x1AF0;
constexpr std::size_t kChoiceListSize = 0x920;
constexpr std::size_t kChoiceListElementsOffset = 0x08;
constexpr std::size_t kChoiceListCountOffset = 0x910;
constexpr std::size_t kChoiceElementSize = 0x48;
[[maybe_unused]] constexpr std::uint32_t kButtonFaultLogLimit = 32;

struct SliderRange {
    std::int32_t minimum{};
    std::int32_t maximum{};
    std::int32_t step{};
    std::int32_t reserved0{};
    std::uint32_t metadata{0x22308};
    std::uint32_t reserved1{};
};
static_assert(sizeof(SliderRange) == 0x18);

std::uint32_t label_id(const erui::detail::CompiledRow& row) noexcept {
    return erui::detail::runtime_state().options.enable_custom_text
        ? row.label_id
        : kVanillaFallbackTextId;
}

std::uint32_t help_id(const erui::detail::CompiledRow& row) noexcept {
    return erui::detail::runtime_state().options.enable_custom_text
        ? row.help_id
        : kVanillaFallbackTextId;
}

void invoke_button_action(erui::Action action) noexcept {
    if (native_dialog_owns_menu_input()) return;
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    const std::uint64_t hit =
        runtime.button_action_hits.fetch_add(1, std::memory_order_relaxed) + 1;

    if (!action) {
        erui::detail::logf(
            erui::LogLevel::error,
            "native button callback[%llu] reached an empty Action",
            static_cast<unsigned long long>(hit));
        return;
    }

    if (runtime.options.enable_diagnostics) {
        const std::uint32_t diagnostic_index =
            runtime.diagnostic_button_action_logs.fetch_add(
                1,
                std::memory_order_relaxed);
        if (diagnostic_index < kButtonDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "diagnostic: button callback[%llu] begin callback=0x%llX userData=%p",
                static_cast<unsigned long long>(hit),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(action.callback)),
                action.user_data);
        } else if (diagnostic_index == kButtonDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "diagnostic: further button-callback traces are suppressed after %u entries",
                static_cast<unsigned>(kButtonDiagnosticLogLimit));
        }
    }

#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
        action.invoke();
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        const std::uint32_t fault =
            runtime.button_action_faults.fetch_add(1, std::memory_order_relaxed) + 1;
        if (fault <= kButtonFaultLogLimit) {
            erui::detail::logf(
                erui::LogLevel::error,
                "native button callback[%llu] raised SEH exception 0x%08lX (fault=%u callback=0x%llX userData=%p)",
                static_cast<unsigned long long>(hit),
                seh_exception_code,
                static_cast<unsigned>(fault),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(action.callback)),
                action.user_data);
        }
        return;
    }
#else
    action.invoke();
#endif

    if (runtime.options.enable_diagnostics && hit <= kButtonDiagnosticLogLimit) {
        erui::detail::logf(
            erui::LogLevel::trace,
            "diagnostic: button callback[%llu] completed",
            static_cast<unsigned long long>(hit));
    }
}

struct ActionButtonThunk {
    erui::Action action{};

    void operator()() const noexcept {
        invoke_button_action(action);
    }
};

struct SubmenuButtonThunk {
    mutable void* parent_page{};
    erui::detail::PageRoute target{};

    void operator()() const noexcept {
        if (native_dialog_owns_menu_input()) return;
        request_navigate_page(
            &parent_page,
            target,
            NavigationRequestKind::submenu);
    }
};

struct PaginationButtonThunk {
    mutable void* parent_page{};
    erui::detail::PageRoute target{};
    NavigationRequestKind kind{NavigationRequestKind::pagination_next};

    void operator()() const noexcept {
        if (native_dialog_owns_menu_input()) return;
        request_navigate_page(&parent_page, target, kind);
    }
};

static_assert(std::is_trivially_copy_constructible_v<ActionButtonThunk>);
static_assert(std::is_trivially_copy_constructible_v<SubmenuButtonThunk>);
static_assert(std::is_trivially_copy_constructible_v<PaginationButtonThunk>);



#if defined(_MSC_VER) && defined(_WIN64)
// The supplied Solid Uncapper helper passes two MSVC std::function<void()>
// objects. Its target pointer is at +0x38, making the x64 object 0x40 bytes.
static_assert(sizeof(std::function<void()>) == 0x40,
    "Unexpected MSVC std::function<void()> ABI for Elden Ring addButton");
#endif

bool add_toggle(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool& faulted) noexcept {
    faulted = false;
#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
#endif
        alignas(16) std::byte menu_context[0x240]{};
        alignas(16) std::byte on_off_list[0x240]{};
        alignas(16) std::byte text_references[0x240]{};

        if (erui::detail::trace_logging()) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "toggle: menu_context(%p)",
                menu_context);
        }
        addresses.menu_context(menu_context);

        if (erui::detail::trace_logging()) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "toggle: on_off_list(%p)",
                on_off_list);
        }
        addresses.on_off_list(on_off_list);

        addresses.text_ref_label(text_references, label_id(row));
        addresses.text_ref_help(text_references + 0x38, help_id(row));

        if (erui::detail::trace_logging()) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "toggle: add_toggle(page=%p value=%p label=%u help=%u enabled=%d)",
                page,
                const_cast<std::uint8_t*>(row.byte_value),
                static_cast<unsigned>(label_id(row)),
                static_cast<unsigned>(help_id(row)),
                row.enabled ? 1 : 0);
        }

        addresses.add_toggle(
            page,
            text_references,
            const_cast<std::uint8_t*>(row.byte_value),
            on_off_list,
            menu_context + 0x0E,
            row.enabled);
        return true;
#if defined(_MSC_VER)
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        faulted = true;
        erui::detail::logf(
            erui::LogLevel::error,
            "toggle native call raised SEH exception 0x%08lX",
            seh_exception_code);
        return false;
    }
#endif
}

bool add_slider(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool& faulted) noexcept {
    faulted = false;
#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
#endif
        alignas(16) std::byte text_references[0x240]{};
        SliderRange range{};
        range.minimum = row.slider.minimum;
        range.maximum = row.slider.maximum;
        range.step = row.slider.step;

        addresses.text_ref_label(text_references, label_id(row));
        addresses.text_ref_help(text_references + 0x38, help_id(row));

        const std::uint8_t current = row.byte_value ? *row.byte_value : 0;
        if (erui::detail::trace_logging()) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "slider: add_slider(page=%p value=%p current=%u range=%d..%d step=%d label=%u help=%u enabled=%d)",
                page,
                const_cast<std::uint8_t*>(row.byte_value),
                static_cast<unsigned>(current),
                range.minimum,
                range.maximum,
                range.step,
                static_cast<unsigned>(label_id(row)),
                static_cast<unsigned>(help_id(row)),
                row.enabled ? 1 : 0);
        }

        addresses.add_slider(
            page,
            text_references,
            const_cast<std::uint8_t*>(row.byte_value),
            &range,
            current,
            row.enabled);
        return true;
#if defined(_MSC_VER)
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        faulted = true;
        erui::detail::logf(
            erui::LogLevel::error,
            "slider native call raised SEH exception 0x%08lX",
            seh_exception_code);
        return false;
    }
#endif
}

bool add_popup_choice(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool& faulted) noexcept {
    faulted = false;
    if (!addresses.popup_choice_constructor ||
        !addresses.popup_choice_value_vtable ||
        !addresses.popup_choice_selection_vtable ||
        !addresses.popup_choice_presentation_vtable ||
        !addresses.text_ref_label || !addresses.text_ref_help ||
        !addresses.destroy_text_references) {
        return false;
    }
#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
#endif
        alignas(16) std::byte text_references[0x240]{};
        alignas(16) std::array<std::byte, 0x40> value_provider{};
        alignas(16) std::array<std::byte, 0x40> selection_action{};
        alignas(16) std::array<std::byte, 0x40> presentation_provider{};
        const auto initialize_erased = [](
            auto& object, void** vtable) noexcept {
            *reinterpret_cast<void***>(object.data()) = vtable;
            *reinterpret_cast<void**>(object.data() + 0x38) = object.data();
        };
        initialize_erased(
            value_provider, addresses.popup_choice_value_vtable);
        initialize_erased(
            selection_action, addresses.popup_choice_selection_vtable);
        initialize_erased(
            presentation_provider,
            addresses.popup_choice_presentation_vtable);
        *reinterpret_cast<void**>(value_provider.data() + 0x08) =
            const_cast<void*>(row.popup_choice_state.address());
        *reinterpret_cast<void**>(selection_action.data() + 0x08) =
            const_cast<void*>(row.popup_choice_state.address());
        *reinterpret_cast<void**>(selection_action.data() + 0x10) = page;
        *reinterpret_cast<void**>(presentation_provider.data() + 0x08) =
            const_cast<void*>(row.popup_choice_state.address());

        addresses.text_ref_label(text_references, label_id(row));
        addresses.text_ref_help(text_references + 0x38, help_id(row));
        void* const result = addresses.popup_choice_constructor(
            page, text_references, value_provider.data(),
            selection_action.data(), presentation_provider.data());
        addresses.destroy_text_references(text_references);
        if (erui::detail::trace_logging()) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "popup choice: native row constructed page=%p result=%p options=%zu selected=%u",
                page,
                result,
                row.choice_ids.size(),
                static_cast<unsigned>(
                    row.popup_choice_state.native_selection() - 1u));
        }
        return result != nullptr;
#if defined(_MSC_VER)
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        faulted = true;
        erui::detail::logf(erui::LogLevel::error,
            "popup choice native call raised SEH exception 0x%08lX",
            seh_exception_code);
        return false;
    }
#endif
}

bool add_inline_choice(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& source_row,
    bool& faulted) noexcept {
    faulted = false;
    if (!addresses.add_inline_choice ||
        !addresses.choice_context_constructor ||
        !addresses.choice_list_builder ||
        !addresses.destroy_text_references) {
        return false;
    }
#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
#endif
        alignas(16) std::byte context[0x240]{};
        alignas(16) std::byte list[kChoiceListSize]{};
        alignas(16) std::byte text_references[0x240]{};
        addresses.choice_context_constructor(context);
        addresses.choice_list_builder(list);
        auto* count_pointer = reinterpret_cast<std::uint64_t*>(
            list + kChoiceListCountOffset);
        const std::uint64_t template_count = *count_pointer;
        if (template_count == 0 || template_count > 32) {
            faulted = true;
            return false;
        }
        using ElementDestructorFn = void(__fastcall*)(void*, std::uint32_t);
        void* first_element = list + kChoiceListElementsOffset;
        auto** choice_vtable = *static_cast<void***>(first_element);
        if (!choice_vtable || !choice_vtable[0]) {
            faulted = true;
            return false;
        }
        for (std::uint64_t index = 0; index < template_count; ++index) {
            void* element = list + kChoiceListElementsOffset +
                index * kChoiceElementSize;
            auto** vtable = *static_cast<void***>(element);
            reinterpret_cast<ElementDestructorFn>(vtable[0])(element, 0);
        }
        std::memset(
            list + kChoiceListElementsOffset, 0,
            32 * kChoiceElementSize);
        if (source_row.choice_ids.empty() || source_row.choice_ids.size() > 32 ||
            !source_row.byte_value || *source_row.byte_value >= source_row.choice_ids.size()) {
            faulted = true;
            return false;
        }
        for (std::size_t index = 0;
             index < source_row.choice_ids.size(); ++index) {
            auto* element = list + kChoiceListElementsOffset +
                index * kChoiceElementSize;
            *reinterpret_cast<void***>(element) = choice_vtable;
            *reinterpret_cast<std::uint8_t*>(element + 0x08) =
                static_cast<std::uint8_t>(index);
            addresses.text_ref_label(
                element + 0x10, source_row.choice_ids[index]);
        }
        *count_pointer = source_row.choice_ids.size();
        addresses.text_ref_label(text_references, label_id(source_row));
        addresses.text_ref_help(text_references + 0x38, help_id(source_row));
        addresses.add_inline_choice(
            page, text_references,
            const_cast<std::uint8_t*>(source_row.byte_value), list,
            context);
        addresses.destroy_text_references(text_references);

        const std::uint64_t count = *reinterpret_cast<const std::uint64_t*>(
            list + kChoiceListCountOffset);
        if (count > 32) {
            erui::detail::logf(
                erui::LogLevel::error,
                "choice native list reported invalid count=%llu",
                static_cast<unsigned long long>(count));
            faulted = true;
            return false;
        }
        for (std::uint64_t index = 0; index < count; ++index) {
            void* element = list + kChoiceListElementsOffset +
                index * kChoiceElementSize;
            auto** vtable = *static_cast<void***>(element);
            if (vtable && vtable[0]) {
                reinterpret_cast<ElementDestructorFn>(vtable[0])(element, 0);
            }
        }
        erui::detail::logf(
            erui::LogLevel::info,
            "choice: native row constructed page=%p options=%llu selected=%u",
            page, static_cast<unsigned long long>(count),
            static_cast<unsigned>(*source_row.byte_value));
        return true;
#if defined(_MSC_VER)
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        faulted = true;
        erui::detail::logf(
            erui::LogLevel::error,
            "choice native call raised SEH exception 0x%08lX",
            seh_exception_code);
        return false;
    }
#endif
}

bool prepare_button_text(
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    std::byte* display_text,
    std::byte* text_references,
    void*& display_text_reference,
    bool root_text_style,
    bool& faulted) noexcept {
    faulted = false;
#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
#endif
        TextReferenceFn display_constructor = root_text_style
            ? addresses.root_button_display_text
            : addresses.text_ref_label;
        if (!display_constructor) return false;
        display_text_reference = display_constructor(display_text, label_id(row));
        if (root_text_style) {
            if (!addresses.root_button_text_references) {
                return false;
            }
            root_button_text_override = {
                .label_id = label_id(row),
                .help_id = help_id(row),
                .active = true,
            };
            addresses.root_button_text_references(text_references);
            root_button_text_override = {};
        } else {
            addresses.text_ref_label(text_references, label_id(row));
            addresses.text_ref_help(
                text_references + kButtonHelpReferenceOffset, help_id(row));
        }
        return display_text_reference != nullptr;
#if defined(_MSC_VER)
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        root_button_text_override = {};
        faulted = true;
        erui::detail::logf(
            erui::LogLevel::error,
            "button text-reference construction raised SEH exception 0x%08lX",
            seh_exception_code);
        return false;
    }
#endif
}

void release_button_display_text(std::byte* display_text) noexcept {
    const std::uint64_t capacity =
        *reinterpret_cast<const std::uint64_t*>(
            display_text + kDisplayCapacityOffset);
    if (capacity <= 7) {
        return;
    }

    void* owner = *reinterpret_cast<void**>(display_text + kDisplayOwnerOffset);
    void* allocation =
        *reinterpret_cast<void**>(display_text + kDisplayAllocationOffset);
    if (!owner) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(owner);
    if (!vtable || !vtable[kDisplayReleaseVtableSlot]) {
        return;
    }

    using ReleaseFn = void(__fastcall*)(void* owner, void* allocation);
    auto release = reinterpret_cast<ReleaseFn>(
        vtable[kDisplayReleaseVtableSlot]);
    release(owner, allocation);
}

bool call_add_button(
    void* page,
    const GameAddresses& addresses,
    void* text_references,
    std::byte* display_text,
    void* display_text_reference,
    void* primary_action,
    void* secondary_action,
    bool& faulted) noexcept {
    faulted = false;
#if defined(_MSC_VER)
    unsigned long seh_exception_code = 0;
    __try {
#endif
        addresses.add_button(
            page,
            text_references,
            display_text_reference,
            primary_action,
            secondary_action);

        // Solid Uncapper cleans both native text-reference objects after the
        // row constructor has cloned all inputs.
        addresses.destroy_text_references(text_references);
        release_button_display_text(display_text);
        return true;
#if defined(_MSC_VER)
    } __except (seh_exception_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        faulted = true;
        erui::detail::logf(
            erui::LogLevel::error,
            "button native constructor/cleanup raised SEH exception 0x%08lX",
            seh_exception_code);
        return false;
    }
#endif
}

template <typename Callback>
bool add_button_with_callback(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    Callback callback,
    const char* callback_kind,
    bool root_text_style,
    bool& faulted) noexcept {
    faulted = false;
    if (!row.enabled || !addresses.add_button ||
        !addresses.destroy_text_references) {
        return false;
    }

    alignas(16) std::byte display_text[kButtonDisplayTextSize]{};
    alignas(16) std::byte text_references[kButtonTextReferencesSize]{};
    void* display_text_reference = nullptr;
    if (!prepare_button_text(
            addresses,
            row,
            display_text,
            text_references,
            display_text_reference,
            root_text_style,
            faulted)) {
        if (!faulted) {
            erui::detail::logf(
                erui::LogLevel::error,
                "button display-text constructor returned null");
        }
        return false;
    }

    try {
        // ABI-faithful to the supplied Solid Uncapper helper: a populated
        // MSVC std::function<void()> and an empty secondary function.
        std::function<void()> primary_action{std::move(callback)};
        std::function<void()> secondary_action{};

        if (erui::detail::trace_logging()) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "button: add_button(page=%p kind=%s label=%u help=%u displayRef=%p primary=%p secondary=%p action=0x%llX userData=%p targetPage=%zu)",
                page,
                callback_kind,
                static_cast<unsigned>(label_id(row)),
                static_cast<unsigned>(help_id(row)),
                display_text_reference,
                &primary_action,
                &secondary_action,
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(row.action.callback)),
                row.action.user_data,
                row.target_page_index);
        }

        return call_add_button(
            page,
            addresses,
            text_references,
            display_text,
            display_text_reference,
            &primary_action,
            &secondary_action,
            faulted);
    } catch (const std::exception& exception) {
        erui::detail::logf(
            erui::LogLevel::error,
            "button callback construction failed: %s",
            exception.what());
    } catch (...) {
        erui::detail::logf(
            erui::LogLevel::error,
            "button callback construction failed with an unknown C++ exception");
    }
    return false;
}

bool add_action_button(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool root_text_style,
    bool& faulted) noexcept {
    if (!row.action) {
        faulted = false;
        return false;
    }
    return add_button_with_callback(
        page,
        addresses,
        row,
        ActionButtonThunk{row.action},
        "action",
        root_text_style,
        faulted);
}

struct ColorPickerButtonThunk {
    void* page{};
    erui::detail::ColorPickerState* state{};
    erui::detail::ColorAction changed_action{};

    void operator()() const noexcept {
        if (!page || !state || native_dialog_owns_menu_input()) return;
        const ERUI_Result result = request_color_picker(
            page, *state, changed_action);
        if (result != ERUI_OK) {
            erui::detail::logf(
                result == ERUI_QUEUE_FULL
                    ? erui::LogLevel::trace
                    : erui::LogLevel::warning,
                "Color Picker activation rejected: state=%p result=%u",
                state,
                static_cast<unsigned>(result));
        }
    }
};

bool add_color_picker(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool root_text_style,
    ColorPickerWidgetHost widget_host,
    bool& faulted) noexcept {
    if (!row.color_picker_state) {
        faulted = false;
        return false;
    }
    ColorPickerRowPresentationScope presentation{
        page,
        row.color_picker_state,
        widget_host,
    };
    return add_button_with_callback(
        page,
        addresses,
        row,
        ColorPickerButtonThunk{
            page,
            row.color_picker_state,
            row.color_action,
        },
        "color-picker",
        root_text_style,
        faulted);
}

bool add_submenu_button(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool root_text_style,
    bool& faulted) noexcept {
    if (row.target_page_index == erui::detail::invalid_compiled_index) {
        faulted = false;
        return false;
    }
    return add_button_with_callback(
        page,
        addresses,
        row,
        SubmenuButtonThunk{
            .parent_page = page,
            .target = erui::detail::PageRoute::submenu(
                row.target_page_index,
                0),
        },
        "submenu",
        root_text_style,
        faulted);
}

bool add_pagination_button(
    void* page,
    const GameAddresses& addresses,
    erui::detail::PageRoute target,
    NavigationRequestKind kind,
    bool root_text_style,
    bool& faulted) noexcept {
    const erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.menu) {
        faulted = false;
        return false;
    }

    erui::detail::CompiledRow row{};
    row.kind = erui::RowKind::button;
    row.enabled = true;
    if (kind == NavigationRequestKind::pagination_previous) {
        row.label_id = runtime.menu->previous_page_label_id;
        row.help_id = runtime.menu->previous_page_help_id;
    } else {
        row.label_id = runtime.menu->next_page_label_id;
        row.help_id = runtime.menu->next_page_help_id;
    }

    return add_button_with_callback(
        page,
        addresses,
        row,
        PaginationButtonThunk{
            .parent_page = page,
            .target = target,
            .kind = kind,
        },
        kind == NavigationRequestKind::pagination_previous
            ? "pagination-previous"
            : "pagination-next",
        root_text_style,
        faulted);
}

struct RootCapacityDetection {
    std::uint8_t visual_capacity{erui::detail::game_options_vanilla_capacity};
    std::uint8_t plan_capacity{};
    std::uint64_t materialized_row_count{};
    bool capacity_read{};
    bool capacity_valid{};
    bool count_read{};
};

RootCapacityDetection detect_root_capacity(void* page) noexcept {
    RootCapacityDetection result{};
    const std::uint8_t configured =
        erui::detail::runtime_state().options.game_options_visual_capacity;
    result.visual_capacity =
        configured >= erui::detail::game_options_vanilla_capacity &&
                configured <= erui::detail::game_options_max_visual_capacity
            ? configured
            : erui::detail::game_options_vanilla_capacity;

    if (!page) {
        return result;
    }
#if defined(_MSC_VER)
    __try {
#endif
        const auto* bytes = static_cast<const std::byte*>(page);
        const std::uint32_t runtime_capacity =
            *reinterpret_cast<const std::uint32_t*>(
                bytes + kRootVisualCapacityOffset);
        result.capacity_read = true;
        if (runtime_capacity >= erui::detail::game_options_vanilla_capacity &&
            runtime_capacity <= erui::detail::game_options_max_visual_capacity) {
            result.visual_capacity =
                static_cast<std::uint8_t>(runtime_capacity);
            result.capacity_valid = true;
        }
        result.materialized_row_count =
            *reinterpret_cast<const std::uint64_t*>(
                bytes + kRootMaterializedRowCountOffset);
        result.count_read = true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.materialized_row_count = 0;
        result.count_read = false;
    }
#endif
    if (result.capacity_valid && result.count_read) {
        result.plan_capacity = erui::detail::derive_root_plan_capacity(
            result.visual_capacity, result.materialized_row_count);
    }
    return result;
}

erui::detail::PageRoute route_with_slice(
    erui::detail::PageRoute route,
    std::size_t slice_index) noexcept {
    route.slice_index = slice_index;
    return route;
}

bool inject_logical_row(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool root_text_style,
    ColorPickerWidgetHost widget_host,
    RowInjectionOutcome& outcome) noexcept {
    bool faulted = false;
    bool constructor_succeeded = false;
    ++outcome.attempted;

    switch (row.kind) {
    case erui::RowKind::toggle:
        constructor_succeeded = row.byte_value &&
            add_toggle(page, addresses, row, faulted);
        break;
    case erui::RowKind::slider:
        constructor_succeeded = row.byte_value &&
            add_slider(page, addresses, row, faulted);
        break;
    case erui::RowKind::inline_choice:
        constructor_succeeded = row.byte_value &&
            add_inline_choice(page, addresses, row, faulted);
        break;
    case erui::RowKind::popup_choice:
        constructor_succeeded = row.byte_value &&
            add_popup_choice(page, addresses, row, faulted);
        break;
    case erui::RowKind::text_input:
        constructor_succeeded = row.text_input_state &&
            add_text_input(page, addresses, row, faulted);
        break;
    case erui::RowKind::color_picker:
        constructor_succeeded = row.color_picker_state &&
            add_color_picker(
                page,
                addresses,
                row,
                root_text_style,
                widget_host,
                faulted);
        break;
    case erui::RowKind::button:
        if (!row.enabled || !row.action) {
            ++outcome.skipped;
            return false;
        }
        constructor_succeeded = add_action_button(
            page, addresses, row, root_text_style, faulted);
        break;
    case erui::RowKind::submenu:
        if (!row.enabled ||
            row.target_page_index == erui::detail::invalid_compiled_index) {
            ++outcome.skipped;
            return false;
        }
        constructor_succeeded = add_submenu_button(
            page, addresses, row, root_text_style, faulted);
        break;
    }

    if (constructor_succeeded) {
        ++outcome.added;
    }
    if (faulted) {
        ++outcome.faulted;
    }
    return constructor_succeeded;
}

} // namespace

namespace {

bool is_builtin_route(erui::detail::PageRouteKind kind) noexcept {
    return kind == erui::detail::PageRouteKind::builtin_main ||
        kind == erui::detail::PageRouteKind::builtin_continuation;
}

ColorPickerWidgetHost widget_host_for_route(
    const erui::detail::PageRoute& route) noexcept {
    if (route.kind == erui::detail::PageRouteKind::root_main) {
        return ColorPickerWidgetHost::game_options;
    }
    if (route.kind != erui::detail::PageRouteKind::builtin_main) {
        return ColorPickerWidgetHost::subpage;
    }
    switch (route.builtin_category) {
    case 1:
        return ColorPickerWidgetHost::camera_options;
    case 2:
        return ColorPickerWidgetHost::display;
    case 3:
        return ColorPickerWidgetHost::sound;
    case 5:
        return ColorPickerWidgetHost::network;
    case 7:
        return ColorPickerWidgetHost::keyboard_mouse;
    case 8:
        return ColorPickerWidgetHost::graphics;
    default:
        return ColorPickerWidgetHost::subpage;
    }
}

bool route_uses_game_options_text_style(
    const erui::detail::PageRoute& route) noexcept {
    return route.kind == erui::detail::PageRouteKind::root_main ||
        (route.kind == erui::detail::PageRouteKind::builtin_main &&
            route.builtin_category == 1);
}

} // namespace

bool resolve_page_route_slice(
    const erui::detail::CompiledMenu& menu,
    erui::detail::PageRoute route,
    erui::detail::PageSlice& output) noexcept {
    output = {};
    if (!route.valid() || route.logical_page_index >= menu.pages.size() ||
        !menu.page_reachable(route.logical_page_index)) {
        return false;
    }
    if (is_builtin_route(route.kind)) {
        return route.builtin_category !=
                erui::detail::invalid_builtin_category &&
            erui::detail::resolve_paginated_slice(
                route.logical_page_index,
                menu.pages[route.logical_page_index].rows.size(),
                route.root_capacity,
                erui::detail::native_subpage_capacity,
                route.slice_index,
                output);
    }
    const erui::detail::PageSlice* const resolved = menu.resolve_slice(route);
    if (!resolved) return false;
    output = *resolved;
    return true;
}

const erui::detail::CompiledRow* find_popup_choice_row_by_native_state(
    const void* state) noexcept {
    const erui::detail::RuntimeState& runtime =
        erui::detail::runtime_state();
    if (!runtime.menu || !state) return nullptr;
    for (const erui::detail::CompiledPage& page : runtime.menu->pages) {
        for (const erui::detail::CompiledRow& row : page.rows) {
            if (row.kind == erui::RowKind::popup_choice &&
                row.popup_choice_state.address() == state) {
                return &row;
            }
        }
    }
    return nullptr;
}

RowInjectionOutcome inject_registered_rows(
    void* page,
    const GameAddresses& addresses) noexcept {
    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.menu || !page) {
        return {};
    }

    const RootCapacityDetection capacity = detect_root_capacity(page);
    if (capacity.plan_capacity == 0) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Game Options row injection skipped: visualRows=%u capacityRead=%d capacityValid=%d materializedRowsBefore=%llu countRead=%d (no validated free slot)",
            static_cast<unsigned>(capacity.visual_capacity),
            capacity.capacity_read ? 1 : 0,
            capacity.capacity_valid ? 1 : 0,
            static_cast<unsigned long long>(
                capacity.materialized_row_count),
            capacity.count_read ? 1 : 0);
        return {};
    }
    const erui::detail::PageRoute route = erui::detail::PageRoute::root_main(
        runtime.menu->root_page_index,
        capacity.plan_capacity);

    if (runtime.options.enable_diagnostics) {
        const erui::detail::RootPagePlan& plan =
            runtime.menu->root_plan(capacity.plan_capacity);
        erui::detail::logf(
            erui::LogLevel::trace,
            "rootCapacity page=%p visualRows=%u capacityRead=%d capacityValid=%d fallbackRows=%u materializedRowsBefore=%llu countRead=%d effectivePlanRows=%u freeSlots=%zu logicalRows=%zu physicalPages=%zu",
            page,
            static_cast<unsigned>(capacity.visual_capacity),
            capacity.capacity_read ? 1 : 0,
            capacity.capacity_valid ? 1 : 0,
            static_cast<unsigned>(runtime.options.game_options_visual_capacity),
            static_cast<unsigned long long>(
                capacity.materialized_row_count),
            capacity.count_read ? 1 : 0,
            static_cast<unsigned>(capacity.plan_capacity),
            plan.custom_capacity,
            runtime.menu->root_page().rows.size(),
            plan.pages.slices.size());
    }

    RowInjectionOutcome outcome = inject_page_route(page, route, addresses);
    outcome.root_capacity = capacity.plan_capacity;
    return outcome;
}

RowInjectionOutcome inject_page_route(
    void* page,
    erui::detail::PageRoute route,
    const GameAddresses& addresses) noexcept {
    RowInjectionOutcome outcome{};
    outcome.route = route;
    outcome.root_capacity = route.root_capacity;
    if (!page) {
        return outcome;
    }

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.menu || route.logical_page_index >= runtime.menu->pages.size()) {
        return outcome;
    }

    erui::detail::PageSlice resolved_slice{};
    if (!resolve_page_route_slice(*runtime.menu, route, resolved_slice)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "physical page route could not be resolved kind=%u logicalPage=%zu slice=%zu capacity=%u",
            static_cast<unsigned>(route.kind),
            route.logical_page_index,
            route.slice_index,
            static_cast<unsigned>(route.root_capacity));
        return outcome;
    }
    const erui::detail::PageSlice* const slice = &resolved_slice;

    reset_color_picker_page_widgets(
        page,
        widget_host_for_route(route));

    outcome.slice_index = slice->slice_index;
    outcome.slice_count = slice->slice_count;
    outcome.first_row = slice->first_row;
    outcome.content_rows = slice->row_count;

    const erui::detail::CompiledPage& compiled_page =
        runtime.menu->pages[route.logical_page_index];
    const bool root_text_style = route_uses_game_options_text_style(route);
    const ColorPickerWidgetHost widget_host = widget_host_for_route(route);

    if (slice->has_previous) {
        erui::detail::PageRoute target{};
        if (route.kind == erui::detail::PageRouteKind::root_continuation &&
            slice->slice_index == 1) {
            target = erui::detail::PageRoute::root_main(
                route.logical_page_index,
                route.root_capacity);
        } else if (route.kind ==
                erui::detail::PageRouteKind::builtin_continuation &&
            slice->slice_index == 1) {
            target = erui::detail::PageRoute::builtin_main(
                route.logical_page_index,
                route.builtin_category,
                route.root_capacity);
        } else {
            target = route_with_slice(route, slice->slice_index - 1);
        }
        bool faulted = false;
        ++outcome.attempted;
        ++outcome.navigation;
        if (add_pagination_button(
                page,
                addresses,
                target,
                NavigationRequestKind::pagination_previous,
                root_text_style,
                faulted)) {
            ++outcome.added;
        }
        if (faulted) {
            ++outcome.faulted;
        }
    }

    const std::size_t end = slice->first_row + slice->row_count;
    for (std::size_t row_index = slice->first_row;
         row_index < end && row_index < compiled_page.rows.size();
         ++row_index) {
        inject_logical_row(
            page,
            addresses,
            compiled_page.rows[row_index],
            root_text_style,
            widget_host,
            outcome);
    }

    if (slice->has_next) {
        erui::detail::PageRoute target{};
        if (route.kind == erui::detail::PageRouteKind::root_main) {
            target = erui::detail::PageRoute::root_continuation(
                route.logical_page_index,
                route.root_capacity,
                slice->slice_index + 1);
        } else if (route.kind ==
                erui::detail::PageRouteKind::builtin_main) {
            target = erui::detail::PageRoute::builtin_continuation(
                route.logical_page_index,
                route.builtin_category,
                route.root_capacity,
                slice->slice_index + 1);
        } else {
            target = route_with_slice(route, slice->slice_index + 1);
        }
        bool faulted = false;
        ++outcome.attempted;
        ++outcome.navigation;
        if (add_pagination_button(
                page,
                addresses,
                target,
                NavigationRequestKind::pagination_next,
                root_text_style,
                faulted)) {
            ++outcome.added;
        }
        if (faulted) {
            ++outcome.faulted;
        }
    }

    return outcome;
}

} // namespace erui::native
